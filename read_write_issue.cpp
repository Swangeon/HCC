/*
 * /dev/config/tun network tunnel driver for BeOS
 * (c) 2003, mmu_man, revol@free.fr
 * licenced under MIT licence.
 */
#include <Drivers.h>
#include <KernelExport.h>
#include <OS.h>
#include <Select.h>
 
#include "BufferQueue.h"
#include <net_buffer.h>
 
#include <condition_variable.h>
#include <fcntl.h>
#include <fs/select_sync_pool.h>
#include <kernel.h>
#include <net/if_tun.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>
 
// #define TUN_DRIVER_NAME "tun0"
#define TAP_DRIVER_NAME "tap0"
#define NET_TUN_MODULE_NAME "network/devices/tun/v1"
#define BUFFER_QUEUE_MAX 30000
 
const char *device_names[] = {TAP_DRIVER_NAME, "misc/tun_interface", NULL};
 
int32 api_version = B_CUR_DRIVER_API_VERSION;
 
static net_buffer *create_filled_buffer(uint8* data, size_t bytes);
static status_t get_packet_data(void* data, size_t* numbytes, net_buffer* buffer);
static status_t retreive_packet(BufferQueue* queueToUse, void* data, size_t* numbytes);
 
status_t tun_open(const char* name, uint32 flags, void** cookie);
status_t tun_close(void* cookie);
status_t tun_free(void* cookie);
status_t tun_ioctl(void* cookie, uint32 op, void* data, size_t len);
status_t tun_read(void* cookie, off_t position, void* data, size_t * numbytes);
status_t tun_write(void* cookie, off_t position, const void* data, size_t*numbytes);
status_t tun_select(void* cookie, uint8 event, uint32 ref, selectsync* sync);
status_t tun_deselect(void* cookie, uint8 event, selectsync* sync);
 
device_hooks tun_hooks = {
    tun_open,
    tun_close,
    tun_free,
    tun_ioctl,
    tun_read,
    tun_write,
    tun_select,
    tun_deselect,
    NULL,
    NULL};
 
/**
 * @brief Represents the tun_struct data structure used for tun driver management.
 *
 * The `tun_struct` struct holds various semaphores and BufferQueue pointers
 * that are used for tun driver management and communication.
 *
 * @var name         Name to distingush whatever side has opened the driver.
 * @var sendQueue    A pointer to the BufferQueue used for sending data to the interface.
 * @var recvQueue    A pointer to the BufferQueue used for receiving data from the interface.
 * @var readLock     A pointer to a mutex lock that will be used for blocking tun_read.
 * @var readWait     A pointer to a ConditionVariable that will be used for blocking tun_read.
 */
typedef struct tun_struct {
    uint32_t                    name[3];
    unsigned long               flags;
    BufferQueue*                sendQueue;
    BufferQueue*                recvQueue;
    ConditionVariable*          readWait;
    ConditionVariable*          writeWait;
    mutex                       readLock;
    union {
        struct {
            select_sync_pool*   select_pool;
            bool                readable;
            bool                writable;
        }                       app;
    };
} tun_struct;
 
BufferQueue gAppQueue(BUFFER_QUEUE_MAX);
BufferQueue gIntQueue(BUFFER_QUEUE_MAX);
ConditionVariable gIntWait;
ConditionVariable gAppWait;
struct net_buffer_module_info *gBufferModule;
 
/**
 * @brief Creates a new net_buffer and fills it with the specified data.
 *
 * This function creates a new net_buffer using the gBufferModule and fills it with the
 * data provided. The net_buffer must be later freed by calling gBufferModule->free().
 *
 * @param data  Pointer to the byte stream data to be copied into the net_buffer.
 * @param bytes Number of bytes to copy from the data pointer.
 * @return      A pointer to the newly created net_buffer if successful, NULL otherwise.
 */
static net_buffer*
create_filled_buffer(uint8* data, size_t bytes)
{
    net_buffer* buffer = gBufferModule->create(256);
    if (buffer == NULL) {
        return NULL;
    }
 
    status_t status = gBufferModule->append(buffer, data, bytes);
    if (status != B_OK) {
        gBufferModule->free(buffer);
        return NULL;
    }
 
    return buffer;
}
 
/**
 * @brief Reads data from a net_buffer into a memory buffer.
 *
 * This function reads data from the specified net_buffer and stores it in the
 * provided memory buffer. The number of bytes to read is determined by the value
 * pointed to by numbytes.
 *
 * @param data      A pointer to the memory buffer where the data will be stored.
 * @param numbytes  A pointer to a size_t variable specifying the number of bytes
 *                  to read from the net_buffer. Upon return, this variable will
 *                  contain the actual number of bytes read.
 * @param buffer    A pointer to the net_buffer from which to read the data.
 * @return status_t The status of the operation. B_OK if successful, an error code
 *                  otherwise.
 */
static status_t
get_packet_data(void* data, size_t* numbytes, net_buffer* buffer)
{
    status_t status;
    status = gBufferModule->read(buffer, 0, data, *numbytes);
    if (status != B_OK) {
        dprintf("Failed reading data\n");
        gBufferModule->free(buffer);
        return status;
    }
    return B_OK;
}
 
/**
 * @brief Retrieve a packet from the specified BufferQueue.
 *
 * This function retrieves a packet from the specified BufferQueue and populates the data
 * buffer with the packet's content. The numbytes parameter will be updated with the size
 * of the retrieved packet data.
 *
 * @param queueToUse Pointer to the BufferQueue to retrieve the packet from.
 * @param data       Pointer to the buffer that will hold the retrieved packet data.
 * @param numbytes   Pointer to the size of the data buffer. Will be updated with the actual
 *                   size of the retrieved packet data.
 *
 * @return status_t Returns B_OK if the packet was successfully retrieved and the data
 *                  populated. Returns an appropriate error code otherwise.
 */
static status_t
retreive_packet(BufferQueue* queueToUse, void* data, size_t* numbytes)
{
    net_buffer* buffer;
    status_t status = B_OK;
 
    if (queueToUse->Used() > 0) {
        status = queueToUse->Get(*numbytes, true, &buffer);
        if (status != B_OK) {
            return status;
        }
        *numbytes = buffer->size;
        status = get_packet_data(data, numbytes, buffer);
        if (status != B_OK) {
            dprintf("Couldn't get packet data\n");
            return B_ERROR;
        }
        status = B_OK;
    } else {
        *numbytes = 0;
    }
    return status;
}
 
 
static inline void
tun_notify(tun_struct* tun)
{
    dprintf("Check Read\n");
    bool readable = tun->recvQueue->Used() != 0;
	if (!tun->app.readable && readable)
		notify_select_event_pool(tun->app.select_pool, B_SELECT_READ);
	tun->app.readable = readable;
 
    dprintf("Check Write\n");
	bool writable = tun->sendQueue->Used() != BUFFER_QUEUE_MAX;
	if (!tun->app.writable && writable)
		notify_select_event_pool(tun->app.select_pool, B_SELECT_WRITE);
	tun->app.writable = writable;
}
 
 
status_t
init_hardware(void)
{
    /* No Hardware */
    dprintf("tun:init_hardware()\n");
    return B_OK; // Need to change to B_ERROR
}
 
status_t
init_driver(void)
{
    /* Init driver */
    dprintf("tun:init_driver()\n");
    status_t status = get_module(NET_BUFFER_MODULE_NAME, (module_info**)&gBufferModule);
    if (status != B_OK) {
        dprintf("Getting BufferModule failed\n");
        return status;
    }
    return B_OK;
}
 
void uninit_driver(void)
{
    dprintf("tun:uninit_driver()\n");
    put_module(NET_BUFFER_MODULE_NAME);
}
 
/**
 * @brief Opens the tun driver interface and sets up the driver for the interface.
 *
 * This function opens the tun driver interface, initializes necessary resources,
 * and sets up the driver for the interface operation.
 *
 * @param name   The name of the tun driver interface.
 * @param flags  Flags for the driver interface (if applicable).
 * @param cookie A pointer to a void pointer that will hold the tun_struct data
 *               after the function call.
 *
 * @return       Returns B_OK on success, or an appropriate error code on failure.
 */
status_t
tun_open(const char* name, uint32 flags, void** cookie)
{
    /* Setup driver for interface here */
    tun_struct* tun = new tun_struct();
    memcpy(tun->name, "app", sizeof(tun->name));
    tun->sendQueue = &gIntQueue;
    tun->recvQueue = &gAppQueue;
    tun->readWait = &gAppWait;
    tun->writeWait = &gIntWait;
    tun->flags = 0;
    mutex_init(&tun->readLock, "read_avail");
    tun->app.readable = tun->recvQueue->Used() != 0;
    tun->app.writable = tun->sendQueue->Used() != BUFFER_QUEUE_MAX;
    tun->app.select_pool = NULL;
    *cookie = static_cast<void*>(tun);
    return B_OK;
}
 
status_t
tun_close(void* cookie)
{
    /* Close interface here */
    tun_struct* tun = static_cast<tun_struct*>(cookie);
    tun->readWait->NotifyAll(B_ERROR);
    snooze(10); // Due to a lock timing issue, we sleep for 10ms 
    delete tun;
    tun = NULL;
    return B_OK;
}
 
status_t
tun_free(void* cookie)
{
    return B_OK;
}
 
status_t
tun_ioctl(void* cookie, uint32 op, void* data, size_t len)
{
    /* IOCTL for driver */
    tun_struct *tun = static_cast<tun_struct*>(cookie);
	// dprintf("%n: tun_ioctl | op = %" B_PRIu32 "\n", tun->name, op);
	switch (op) {
	case TUNSETIFF: 
        memcpy(tun->name, "int", sizeof(tun->name));
        tun->sendQueue = &gAppQueue;
        tun->recvQueue = &gIntQueue;
        tun->readWait = &gIntWait;
        tun->writeWait = &gAppWait;
		return B_OK;
	case B_SET_NONBLOCKING_IO:
        tun->flags |= B_SET_NONBLOCKING_IO;
        return B_OK;
    default:
		return B_DEV_INVALID_IOCTL;
	};
 
    return B_OK;
}
 
/**
 * @brief Reads data from the TUN device.
 *
 * This function reads data from the TUN device represented by the given `cookie` which
 * is casted to a tun_struct. The mutex within the tun variable will get locked, passed onto
 * the condition variable to wait if is anything in the recieve queue, retrieves the packet 
 * from the receive queue, and unlocks the mutex when the operation is complete.
 *
 * @param cookie   A pointer to a `tun_struct` representing the TUN interface/application.
 * @param position The position from where to read data (not used in this function).
 * @param data     A pointer to a buffer where the read data will be stored.
 * @param numbytes On input, the number of bytes to read; on output, the number of bytes read
 *                 (see retreive_packet).
 *
 * @return A status code indicating the success or failure of the read operation.
 *         B_OK if successful, an error code otherwise.
 */
status_t
tun_read(void* cookie, off_t position, void* data, size_t* numbytes)
{
    tun_struct* tun = static_cast<tun_struct*>(cookie);
    status_t status;
    mutex_lock(&tun->readLock);
    while (tun->recvQueue->Used() == 0) {
        status = tun->readWait->Wait(&tun->readLock, B_CAN_INTERRUPT);
        if (status != B_OK) {
            mutex_unlock(&tun->readLock);
            return status;
        }
    }
    status = retreive_packet(tun->recvQueue, data, numbytes);
    if (status != B_OK)
        return status;
 
    if (tun->flags & B_SET_NONBLOCKING_IO) {
        dprintf("Notifying Select (Read)\n");
        tun_notify(tun);
    }
 
    mutex_unlock(&tun->readLock);
    return B_OK;
}
 
/**
 * @brief Write data to the TUN device.
 *
 * This function writes data to the TUN device BufferQueue specified by the given tun_struct
 * 'cookie' and notifies the condition variable for read to continue. 
 *
 * @param cookie    Pointer to the TUN device data (tun_struct).
 * @param position  The offset position for writing (not used in this implementation).
 * @param data      Pointer to the data to be written.
 * @param numbytes  Pointer to the size of the data to be written. On output, it will
 *                  contain the actual number of bytes written.
 * @return status_t Returns B_OK if the write operation is successful,
 *                  or an appropriate error code if there is a failure.
 */
status_t
tun_write(void* cookie, off_t position, const void* data, size_t* numbytes)
{
    tun_struct* tun = static_cast<tun_struct*>(cookie);
    size_t used = tun->sendQueue->Used();
    // Buffer is full or will be so we have to drop the packet
    if ((used + *numbytes) >= BUFFER_QUEUE_MAX) {
        return B_WOULD_BLOCK;
    }
    tun->sendQueue->Add(create_filled_buffer((uint8 *)data, *numbytes));
    tun->writeWait->NotifyOne();
    if (tun->flags & B_SET_NONBLOCKING_IO) {
        dprintf("Notifying Select (Write)\n");
        tun_notify(tun);
    }
    return B_OK;
}
 
status_t
tun_select(void* cookie, uint8 event, uint32 ref, selectsync* sync)
{
    tun_struct* tun = static_cast<tun_struct*>(cookie);
	size_t readUsed = tun->recvQueue->Used();
	size_t writeUsed = tun->sendQueue->Used();
	bool wasReadable = readUsed != 0;
	bool wasWritable = writeUsed != BUFFER_QUEUE_MAX;
 
	if (event != B_SELECT_READ && event != B_SELECT_WRITE) {
		return B_BAD_VALUE;
	}
	status_t status = add_select_sync_pool_entry(&tun->app.select_pool, sync, event);
	if (status != B_OK) {
        return B_BAD_VALUE;
    }
 
	if (event == B_SELECT_READ && wasReadable) {
        dprintf("Notify Read\n");
		notify_select_event(sync, B_SELECT_READ);
	} 
 
	if (event == B_SELECT_WRITE && wasWritable) {
        dprintf("Notify Write\n");
		notify_select_event(sync, B_SELECT_WRITE);
        return B_OK;
	}
	return B_OK;
}
 
status_t
tun_deselect(void* cookie, uint8 event, selectsync* sync)
{
    tun_struct* tun = static_cast<tun_struct*>(cookie);
    if (event != B_SELECT_READ)
		return B_BAD_VALUE;
 
	return remove_select_sync_pool_entry(&tun->app.select_pool, sync, event);
}
 
const char**
publish_devices()
{
    /* Publish driver names to devfs */
    return device_names;
}
 
device_hooks*
find_device(const char* name)
{
    /* Find hooks for driver functions */
    return &tun_hooks;
}
