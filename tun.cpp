 /* Authors:
 *		Axel Dörfler, axeld@pinc-software.de
 */

#include <net_tun.h>
#include <net_buffer.h>
#include <net_device.h>
#include <net_stack.h>

#include <lock.h>
#include <util/AutoLock.h>
#include <util/DoublyLinkedList.h>
#include <KernelExport.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <new>
#include <stdlib.h>
#include <string.h>
// To get rid of after testing
#include <debug.h>
#include <errno.h>
#include <stdio.h>
#include <sys/sockio.h>
#include <unistd.h>


struct tun_device : net_device, DoublyLinkedListLinkImpl<tun_device> {
	~tun_device()
	{
		free(read_buffer);
		free(write_buffer);
	}
	int		fd;
	uint32	frame_size;
	void* read_buffer, *write_buffer;
	mutex read_buffer_lock, write_buffer_lock;
};


struct net_buffer_module_info* gBufferModule;
static struct net_stack_module_info* sStackModule;

//static mutex sListLock;
//static DoublyLinkedList<ethernet_device> sCheckList;

//	#pragma mark -


status_t
tun_init(const char* name, net_device** _device)
{
	if (strncmp(name, "tun", 3)
		&& strncmp(name, "tap", 3)
		&& strncmp(name, "dns", 3))	/* iodine uses that */
		return B_BAD_VALUE;

	status_t status = get_module(NET_BUFFER_MODULE_NAME, (module_info **)&gBufferModule);
	if (status < B_OK) {
		dprintf("Get Mod Failed\n");
		return status;
	}

	tun_device *device = new (std::nothrow) tun_device;
	if (device == NULL) {
		put_module(NET_BUFFER_MODULE_NAME);
		return B_NO_MEMORY;
	}

	memset(device, 0, sizeof(tun_device));
	strcpy(device->name, name);

	if (strncmp(name, "tun", 3) == 0) {
		// fprintf(stdout, "TUN\n");
		device->flags = IFF_LOOPBACK | IFF_LINK;
		device->type = IFT_TUN;
	} else if (strncmp(name, "tap", 3) == 0) {
		// fprintf(stdout, "TAP\n");
		device->flags = IFF_BROADCAST | IFF_ALLMULTI | IFF_LINK;
		device->type = IFT_ETHER;
	} else {
		// fprintf(stdout, "Bad\n");
		return B_BAD_VALUE;
	}

	device->mtu = 1500;
	device->media = IFM_ACTIVE | IFM_ETHER;
	device->header_length = 20;
	device->fd = -1;
	device->read_buffer = device->write_buffer = NULL;
	device->read_buffer_lock = MUTEX_INITIALIZER("tun read_buffer"),
	device->write_buffer_lock = MUTEX_INITIALIZER("tun write_buffer");
	*_device = device;

	dprintf("TUN DEVICE CREATED\n");
	return B_OK;
}


status_t
tun_uninit(net_device* _device)
{
	tun_device* device = (tun_device*)_device;
	put_module(NET_STACK_MODULE_NAME);
	put_module(NET_BUFFER_MODULE_NAME);
	delete device;
	return B_OK;
}


status_t
tun_up(net_device *_device)
{
	tun_device *device = (tun_device *)_device;
	device->fd = open("/dev/misc/tun_driver", O_RDWR);
	if (device->fd < 0) {
		dprintf("Module Name %s failed in opening driver\n", device->name);
		return errno;
	}
	write(device->fd, "tun", 4);
	dprintf("Marked as interface for driver\n");
	return B_OK;
}


void
tun_down(net_device *_device)
{
	tun_device *device = (tun_device *)_device;
	close(device->fd);
	device->fd = -1;
}


status_t
tun_control(net_device* device, int32 op, void* argument,
	size_t length)
{
	return B_BAD_VALUE;
}


status_t
tun_send_data(net_device* device, net_buffer* buffer)
{
	// dprintf("Sending Packet:  Start: %u | End: %u Src: %s | Dst: %s\n", buffer->fragment.start, buffer->fragment.end, buffer->source->sa_data, buffer->destination->sa_data);
	// dprintf("Flags: %u | Size: %u | Protocol: %u\n", buffer->flags, buffer->size, buffer->protocol);
	// dprintf("Seq: %u | Offset: %u | Idx: %u | Type: %i\n", buffer->sequence, buffer->offset, buffer->index, buffer->type);
	void* data = malloc(buffer->size);
	gBufferModule->read(buffer, buffer->offset, data, buffer->size);
	dprintf("Reading Data from %p: ", data);
	uint8_t* bytePtr = NULL; // = static_cast<uint8_t*>(data);
	memcpy(bytePtr, data, buffer->size);
	// for (size_t i = 0; i < buffer->size; i++) {
    // 	uint8_t byte = *(bytePtr + i);
	// 	dprintf("%02x", byte);
	// }
	dprintf("%02x", *bytePtr);
	return sStackModule->device_enqueue_buffer(device, buffer);
}


status_t
tun_receive_data(net_device *_device, net_buffer **_buffer)
{
	return B_OK;
}


status_t
tun_set_mtu(net_device* device, size_t mtu)
{
	if (mtu > 65536 || mtu < 16)
		return B_BAD_VALUE;
	device->mtu = mtu;
	return B_OK;
}


status_t
tun_set_promiscuous(net_device* device, bool promiscuous)
{
	return EOPNOTSUPP;
}


status_t
tun_set_media(net_device* device, uint32 media)
{
	return EOPNOTSUPP;
}


status_t
tun_add_multicast(net_device* device, const sockaddr* address)
{
	return B_OK;
}


status_t
tun_remove_multicast(net_device* device, const sockaddr* address)
{
	return B_OK;
}


static status_t
tun_std_ops(int32 op, ...)
{
	switch (op) {
		case B_MODULE_INIT:
		{
			status_t status = get_module(NET_STACK_MODULE_NAME,
				(module_info**)&sStackModule);
			if (status < B_OK)
				return status;
			status = get_module(NET_BUFFER_MODULE_NAME,
				(module_info**)&gBufferModule);
			if (status < B_OK) {
				put_module(NET_STACK_MODULE_NAME);
				return status;
			}
			return B_OK;
		}
		case B_MODULE_UNINIT:
			put_module(NET_BUFFER_MODULE_NAME);
			put_module(NET_STACK_MODULE_NAME);
			return B_OK;
		default:
			return B_ERROR;
	}
}


net_device_module_info sTunModule = {
	{
		"network/devices/tun/v1",
		0,
		tun_std_ops
	},
	tun_init,
	tun_uninit,
	tun_up,
	tun_down,
	tun_control,
	tun_send_data,
	NULL, // receive_data
	tun_set_mtu,
	tun_set_promiscuous,
	tun_set_media,
	tun_add_multicast,
	tun_remove_multicast,
};


module_info* modules[] = {
	(module_info*)&sTunModule,
	NULL
};
