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


#define TUN_MODULE_NAME "tun_driver"

#define NET_TUN_MODULE_NAME "network/devices/tun/v1"

const char * device_names[] = {"misc/" TUN_MODULE_NAME, NULL};

int32 api_version = B_CUR_DRIVER_API_VERSION;

struct net_buffer_module_info* gBufferModule;


status_t tun_open(const char *name, uint32 flags, void* cookie);
status_t tun_close(void *cookie);
status_t tun_free(void *cookie);
status_t tun_ioctl(void *cookie, uint32 op, void *data, size_t len);
status_t tun_read(void *cookie, off_t position, void *data, size_t *numbytes);
status_t tun_write(void *cookie, off_t position, const void *data, size_t *numbytes);
status_t tun_readv(void *cookie, off_t position, const iovec *vec, size_t count, size_t *numBytes);
status_t tun_writev(void *cookie, off_t position, const iovec *vec, size_t count, size_t *numBytes);


device_hooks tun_hooks = {
	(device_open_hook)tun_open,
	(device_close_hook) tun_close,
	(device_free_hook)tun_free,
	(device_control_hook)tun_ioctl,
	(device_read_hook)tun_read,
	(device_write_hook)tun_write,
	NULL,
	NULL,
	(device_readv_hook)tun_readv,
	(device_writev_hook)tun_writev
};


status_t
init_hardware(void)
{
	/* No Hardware */
	return B_OK;
}


status_t
init_driver(void)
{
	dprintf("tun:init_driver()\n");
	return B_OK;
}


void
uninit_driver(void)
{
	dprintf("tun:uninit_driver()\n");
}


status_t
tun_open(const char *name, uint32 flags, void *cookie)
{
	/* Make interface here */
	dprintf("tun:open_driver()\n");
	BufferQueue userQueue(32768);
	BufferQueue appQueue(32768);
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
tun_read(void *cookie, off_t position, void *data, size_t *numbytes)
{
	/* Read data from driver */
	dprintf("TUN: Reading %li bytes of data\n", *numbytes);
	return B_OK;
}


status_t
tun_write(void *cookie, off_t position, const void *data, size_t *numbytes)
{
	/* Write data to driver */
	dprintf("tun:write_driver(): writting %li bytes\n", *numbytes);
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
