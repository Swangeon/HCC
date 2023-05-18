/*
 * Copyright 2006-2007, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Axel Dörfler, axeld@pinc-software.de
 */

#include <net_tun.h>

#include <net_buffer.h>
#include <net_device.h>
#include <net_stack.h>

#include <KernelExport.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <new>
#include <stdlib.h>
#include <string.h>

#include <kernel/debug.h>

struct net_buffer_module_info* gBufferModule;
static struct net_stack_module_info* sStackModule;

//static mutex sListLock;
//static DoublyLinkedList<ethernet_device> sCheckList;


//	#pragma mark -


status_t
tun_init(const char* name, net_device** _device)
{
	tun_device* device;

	if (strncmp(name, "tun", 3)
		&& strncmp(name, "tap", 3)
		&& strncmp(name, "dns", 3))	/* iodine uses that */
		return B_BAD_VALUE;

	device = new (std::nothrow) tun_device;
	if (device == NULL) {
		return B_NO_MEMORY;
	}

	memset(device, 0, sizeof(tun_device));

	strcpy(device->name, name);
	device->flags = strncmp(name, "tap", 3) ? IFF_LOOPBACK | IFF_LINK : IFF_BROADCAST | IFF_ALLMULTI | IFF_LINK;
	device->type = strncmp(name, "tap", 3) ? IFT_TUN : IFT_ETHER;
	device->mtu = 1500; /* Most VPN MTU's are no more than 1500 bytes */
	device->media = IFM_ACTIVE;

	*_device = device;
	dprintf("Init TUN Interface {%s}\n", name);
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
tun_up(net_device* device)
{
	dprintf("TUN Up\n");
	return B_OK;
}


void
tun_down(net_device* device)
{
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
	dprintf("TUN Sending Data\n");
	return sStackModule->device_enqueue_buffer(device, buffer);
}

status_t
tun_receive_data(net_device* device, net_buffer** buffer)
{
	dprintf("TUN Receiving Data\n");
	// return sStackModule->device_enqueue_buffer(device, buffer);
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
	NULL, // Receive Data
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
