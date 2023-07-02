/*
 * /dev/config/tun network tunnel driver for BeOS
 * (c) 2003, mmu_man, revol@free.fr
 * licenced under MIT licence.
 */
#include <Drivers.h>
#include <KernelExport.h>
#include <OS.h>

#include "BufferQueue.h"
#include <net_buffer.h>

#include <fcntl.h>
// #include <fsproto.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>
#include <util/queue.h>


#define TUN_DRIVER_NAME "misc/tun_driver"

#define NET_TUN_MODULE_NAME "network/devices/tun/v1"

const char * device_names[] = {TUN_DRIVER_NAME, "misc/tun_interface", NULL};

int32 api_version = B_CUR_DRIVER_API_VERSION;


status_t tun_open(const char *name, uint32 flags, void **cookie);
status_t tun_close(void *cookie);
status_t tun_free(void *cookie);
status_t tun_ioctl(void *cookie, uint32 op, void *data, size_t len);
status_t tun_read(void *cookie, off_t position, void *data, size_t *numbytes);
status_t tun_write(void *cookie, off_t position, const void *data, size_t *numbytes);
status_t tun_readv(void *cookie, off_t position, const iovec *vec, size_t count, size_t *numBytes);
status_t tun_writev(void *cookie, off_t position, const iovec *vec, size_t count, size_t *numBytes);


device_hooks tun_hooks = {
        tun_open,
        tun_close,
        tun_free,
        tun_ioctl,
        tun_read,
        tun_write,
        NULL,
        NULL,
        tun_readv,
        tun_writev
};

BufferQueue appQ(3000); // 3000 for now
BufferQueue interfaceQ(3000); // 3000 for now
sem_id readSem;
sem_id writeSem;
struct net_buffer_module_info* netBufferModule;

status_t
init_hardware(void)
{
    /* No Hardware */
    return B_OK;
}


status_t
init_driver(void)
{
    /* Init driver */
    dprintf("tun:init_driver() at /dev/misc\n");
    status_t status = get_module(NET_BUFFER_MODULE_NAME, (module_info **)&netBufferModule);
    if (status != B_OK){
        dprintf("Getting BufferModule failed\n");
        return status;
    }
    return B_OK;
}


void
uninit_driver(void)
{
    dprintf("tun:uninit_driver()\n");
}


status_t
tun_open(const char *name, uint32 flags, void **cookie)
{
    /* Make interface here */
    dprintf("tun:open_driver with name %s and flags %u\n", name, flags);
    if (flags == 2) { // 2 is the flag that gets passed by a tun interface
        *cookie = (void*)"tun";
        dprintf("Marked as interface for driver\n");
    } else { 
        *cookie = (void*)"app";
        dprintf("Marked as application for driver\n");
    }
    if ((readSem = create_sem(1, "tun_notify_read")) < B_NO_ERROR) {
        put_module(NET_BUFFER_MODULE_NAME);
        return B_ERROR;
    }
    if ((writeSem = create_sem(1, "tun_notify_write")) < B_NO_ERROR) {
        put_module(NET_BUFFER_MODULE_NAME);
        return B_ERROR;
    }
    return B_OK;
}


status_t
tun_close(void *cookie)
{
    /* Close interface here */
    dprintf("tun:close_driver()\n");
    // (void)cookie;
    return B_OK;
}


status_t
tun_free(void *cookie)
{
    return B_OK;
}


status_t
tun_ioctl(void *cookie, uint32 op, void *data, size_t len)
{
// 	switch (op) {
// 	case B_SET_NONBLOCKING_IO:
// 		cookie->blocking_io = false;
// 		return B_OK;
// 	case B_SET_BLOCKING_IO:
// 		cookie->blocking_io = true;
// 		return B_OK;
// 	case TUNSETNOCSUM:
// 		return B_OK;//EOPNOTSUPP;
// 	case TUNSETDEBUG:
// 		return B_OK;//EOPNOTSUPP;
// 	case TUNSETIFF:
// 		if (data == NULL)
// 			return EINVAL;
// 		ifr = (ifreq_t *)data;

// 		iface = gIfaceModule->tun_reuse_or_create(ifr, cookie);
// 		if (iface != NULL) {
// 			dprintf("tun: new tunnel created: %s, flags: 0x%08lx\n", ifr->ifr_name, iface->flags);
// 			return B_OK;
// 		} else
// 			dprintf("tun: can't allocate a new tunnel!\n");
// 		break;

// 	case SIOCGIFHWADDR:
// 		if (data == NULL)
// 			return EINVAL;
// 		ifr = (ifreq_t *)data;
// 		if (iface == NULL)
// 			return EINVAL;
// 		if (strncmp(ifr->ifr_name, iface->ifn->if_name, IFNAMSIZ) != 0)
// 			return EINVAL;
// 		memcpy(ifr->ifr_hwaddr, iface->fakemac.octet, 6);
// 		return B_OK;
// 	case SIOCSIFHWADDR:
// 		if (data == NULL)
// 			return EINVAL;
// 		ifr = (ifreq_t *)data;
// 		if (iface == NULL)
// 			return EINVAL;
// 		if (strncmp(ifr->ifr_name, iface->ifn->if_name, IFNAMSIZ) != 0)
// 			return EINVAL;
// 		memcpy(iface->fakemac.octet, ifr->ifr_hwaddr, 6);
// 		return B_OK;

// 	}
// 	return B_ERROR;
    return B_OK;
}


status_t
get_packet_data(void *data, size_t *numbytes, net_buffer* buffer)
{
    // struct net_buffer_module_info* netBufferModule;
    // status_t status;
    // status = get_module(NET_BUFFER_MODULE_NAME, (module_info **)&netBufferModule);
    // if (status != B_OK){
    //     dprintf("Getting BufferModule failed\n");
    //     return status;
    // }
	// if (buffer->size == *numbytes) {
    //     return B_ERROR;
    // }
    // // set data = to the uint8* byte stream?
	// status = netBufferModule->read(buffer, 0, data, *numbytes);
	// if (status != B_OK) {
	// 	dprintf("Failed reading data\n");
    //     netBufferModule->free(buffer);
    //     put_module(NET_BUFFER_MODULE_NAME);
    //     return status;
    // }
    // put_module(NET_BUFFER_MODULE_NAME);
    return B_OK;
}


status_t
tun_read(void *cookie, off_t position, void *data, size_t *numbytes)
{
    // Read data from driver 
    dprintf("%li bytes available in appQ\n", appQ.Used());
    dprintf("%li bytes available in interfaceQ\n", interfaceQ.Used());
    status_t status;
    net_buffer* buffer;
    if ((appQ.Used() == 0) || (interfaceQ.Used() == 0)){
        dprintf("Acquiring Semaphore\n");
        status = acquire_sem_etc(readSem, 1, B_CAN_INTERRUPT, 0);
        if (status < B_OK) {
            *numbytes = 0;
            return status;
        }
    }
    // dprintf("TUN: Reading %li bytes of data\n", *numbytes);
    if ((strcmp((char*)cookie, "app") == 0)) {
        dprintf("TUN: APP Reading %li bytes of data\n", *numbytes);
        status = appQ.Get(*numbytes, true, &buffer);
        if (status != B_OK) {
            dprintf("getting packet failed: %s\n", strerror(status));
            return status;
        }
        data = (void*)appQ.Used();
        *numbytes = sizeof(size_t);
    } else if ((strcmp((char*)cookie, "tun") == 0) && (interfaceQ.Used() != 0)) {
        dprintf("TUN: INTERFACE Reading %li bytes of data\n", *numbytes);
        status = interfaceQ.Get(*numbytes, true, &buffer);
        if (status != B_OK) {
            dprintf("getting packet failed: %s\n", strerror(status));
            return status;
        }
        data = (void*)interfaceQ.Used();
        *numbytes = sizeof(size_t);
    } else {
        dprintf("No Data\n");
        return B_ERROR;
    }
    return B_OK;
}


net_buffer*
create_filled_buffer(uint8* data, size_t bytes)
{
	net_buffer* buffer = netBufferModule->create(256);
	if (buffer == NULL) {
		dprintf("creating a buffer failed!\n");
		return NULL;
	}

	status_t status = netBufferModule->append(buffer, data, bytes);
	if (status != B_OK) {
		dprintf("appending %lu bytes to buffer %p failed: %s\n", bytes, buffer,
			strerror(status));
		netBufferModule->free(buffer);
		return NULL;
	}
	
	return buffer;
}


status_t
tun_write(void *cookie, off_t position, const void *data, size_t *numbytes)
{
    // Write data to driver 
    dprintf("tun:write_driver: writting %s with length of %li bytes\n", (char*)data, *numbytes);
    if (!cookie && interfaceQ.Used() != 3000) {
        dprintf("Appending to interfaceQ\n");
        interfaceQ.Add(create_filled_buffer((uint8 *)data, *numbytes));
    } else if (appQ.Used() != 3000) {
        dprintf("%s appending to appQ\n", (char*)cookie);
        appQ.Add(create_filled_buffer((uint8 *)data, *numbytes));
        // interfaceQ.Add(create_filled_buffer((uint8 *)data, *numbytes)); // For testing
        // dprintf("Data at %p of size %lu\n", data, *numbytes);
    } else { 
        return B_ERROR;
    }
    release_sem(readSem);
    return B_OK;
}


status_t
tun_readv(void *cookie, off_t position, const iovec *vec, size_t count, size_t *numBytes)
{
    return EOPNOTSUPP;
}


status_t
tun_writev(void *cookie, off_t position, const iovec *vec, size_t count, size_t *numBytes)
{
    return EOPNOTSUPP;
}


const char**
publish_devices()
{
    return device_names;
}


device_hooks*
find_device(const char *name)
{
    return &tun_hooks;
}

