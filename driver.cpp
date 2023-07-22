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
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>

#define TUN_DRIVER_NAME "tun0"

#define NET_TUN_MODULE_NAME "network/devices/tun/v1"

const char *device_names[] = {TUN_DRIVER_NAME, "misc/tun_interface", NULL};

int32 api_version = B_CUR_DRIVER_API_VERSION;

static net_buffer *create_filled_buffer(uint8 *data, size_t bytes);
static status_t get_packet_data(void *data, size_t *numbytes, net_buffer *buffer);
static status_t retreive_packet(BufferQueue *queueToUse, void *data, size_t *numbytes);

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
    tun_writev};

/**
 * @brief Represents the tun_struct data structure used for tun driver management.
 *
 * The `tun_struct` struct holds various semaphores and BufferQueue pointers
 * that are used for tun driver management and communication.
 *
 * @var selfSendSem  The semaphore ID used for self-send operations.
 * @var selfRecvSem  The semaphore ID used for self-receive operations.
 * @var sendSem      A pointer to the semaphore ID used for sending data to the interface.
 * @var recvSem      A pointer to the semaphore ID used for receiving data from the interface.
 * @var sendQueue    A pointer to the BufferQueue used for sending data to the interface.
 * @var recvQueue    A pointer to the BufferQueue used for receiving data from the interface.
 */
typedef struct tun_struct
{
    sem_id selfSendSem;
    sem_id selfRecvSem;
    sem_id *sendSem;
    sem_id *recvSem;
    BufferQueue *sendQueue;
    BufferQueue *recvQueue;
} tun_struct;

BufferQueue gAppQueue(30000);
BufferQueue gIntQueue(30000);
sem_id gIntReadSem;
sem_id gAppReadSem;
sem_id gIntWriteSem;
sem_id gAppWriteSem;
struct net_buffer_module_info *gBufferModule;

/**
 * @brief Creates a new net_buffer and fills it with the specified data.
 *
 * This function creates a new net_buffer using the gBufferModule and fills it with the
 * data provided. The net_buffer must be later freed by calling gBufferModule->free().
 *
 * @param data  Pointer to the byte stream data to be copied into the net_buffer.
 * @param bytes Number of bytes to copy from the data pointer.
 * @return A pointer to the newly created net_buffer if successful, NULL otherwise.
 */
static net_buffer *
create_filled_buffer(uint8 *data, size_t bytes)
{
    net_buffer *buffer = gBufferModule->create(256);
    if (buffer == NULL)
    {
        return NULL;
    }

    status_t status = gBufferModule->append(buffer, data, bytes);
    if (status != B_OK)
    {
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
 * @param data     A pointer to the memory buffer where the data will be stored.
 * @param numbytes A pointer to a size_t variable specifying the number of bytes
 *                 to read from the net_buffer. Upon return, this variable will
 *                 contain the actual number of bytes read.
 * @param buffer   A pointer to the net_buffer from which to read the data.
 * @return status_t The status of the operation. B_OK if successful, an error code
 *                  otherwise.
 */
static status_t
get_packet_data(void *data, size_t *numbytes, net_buffer *buffer)
{
    status_t status;
    status = gBufferModule->read(buffer, 0, data, *numbytes);
    if (status != B_OK)
    {
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
retreive_packet(BufferQueue *queueToUse, void *data, size_t *numbytes)
{
    net_buffer *buffer;
    status_t status = B_OK;

    if (queueToUse->Used())
    {
        status = queueToUse->Get(*numbytes, true, &buffer);
        if (status != B_OK)
        {
            return status;
        }
        *numbytes = buffer->size;
        status = get_packet_data(data, numbytes, buffer);
        if (status != B_OK)
        {
            dprintf("Couldn't get packet data\n");
            return B_ERROR;
        }
        status = B_OK;
    }
    return status;
}

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
    status_t status = get_module(NET_BUFFER_MODULE_NAME, (module_info **)&gBufferModule);
    if (status != B_OK)
    {
        dprintf("Getting BufferModule failed\n");
        return status;
    }
    return B_OK;
}

void uninit_driver(void)
{
    put_module(NET_BUFFER_MODULE_NAME);
    delete_sem(gIntReadSem);
    delete_sem(gAppReadSem);
    delete_sem(gIntWriteSem);
    delete_sem(gAppWriteSem);
    dprintf("tun:uninit_driver()\n");
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
 *              after the function call.
 *
 * @return       Returns B_OK on success, or an appropriate error code on failure.
 */
status_t
tun_open(const char *name, uint32 flags, void **cookie)
{
    /* Setup driver for interface here */
    tun_struct *tun = new tun_struct();
    if (strncmp(name, "misc/tun_interface", 18) == 0)
    {
        gIntReadSem = create_sem(0, "tun_notify_read");
        if (gIntReadSem < B_NO_ERROR)
        {
            return B_ERROR;
        }
        gIntWriteSem = create_sem(1, "tun_notify_write");
        if (gIntWriteSem < B_NO_ERROR)
        {
            return B_ERROR;
        }
        tun->selfSendSem = gIntWriteSem;
        tun->selfRecvSem = gIntReadSem;
        tun->sendSem = &gAppWriteSem;
        tun->recvSem = &gAppReadSem;
        tun->sendQueue = &gAppQueue;
        tun->recvQueue = &gIntQueue;
        *cookie = static_cast<void *>(tun);
    }
    else
    {
        gAppReadSem = create_sem(0, "app_notify_read");
        if (gAppReadSem < B_NO_ERROR)
        {
            return B_ERROR;
        }
        gAppWriteSem = create_sem(1, "app_notify_write");
        if (gAppWriteSem < B_NO_ERROR)
        {
            return B_ERROR;
        }
        tun->selfSendSem = gAppWriteSem;
        tun->selfRecvSem = gAppReadSem;
        tun->sendSem = &gIntWriteSem;
        tun->recvSem = &gIntReadSem;
        tun->sendQueue = &gIntQueue;
        tun->recvQueue = &gAppQueue;
        *cookie = static_cast<void *>(tun);
    }

    return B_OK;
}

status_t
tun_close(void *cookie)
{
    /* Close interface here */
    dprintf("tun:close_driver()\n");
    tun_struct *tun = static_cast<tun_struct *>(cookie);
    delete tun;
    tun = NULL;
    cookie = NULL;
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
    /* IOCTL for driver */
    return B_OK;
}

/**
 * @brief Reads data from the TUN device.
 *
 * This function reads data from the TUN device represented by the given `cookie` which
 * is casted to a tun_struct. It acquires the appropriate semaphores, retrieves the
 * packet from the receive queue, and releases the opposite send semaphore once the read
 * operation is complete.
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
tun_read(void *cookie, off_t position, void *data, size_t *numbytes)
{
    tun_struct *tun = static_cast<tun_struct *>(cookie);
    status_t status;
    status = acquire_sem_etc(tun->selfRecvSem, 1, B_CAN_INTERRUPT, 0);
    if (status < B_OK)
    {
        dprintf("Could not acquire receiving sem\n");
        *numbytes = 0;
        return status;
    }
    status = retreive_packet(tun->recvQueue, data, numbytes);
    if (status != B_OK)
    {
        return status;
    }
    release_sem(*(tun->sendSem));
    return B_OK;
}

/**
 * @brief Write data to the TUN device.
 *
 * This function writes data to the TUN device specified by the given cookie.
 *
 * @param cookie   Pointer to the TUN device data (tun_struct).
 * @param position The offset position for writing (not used in this implementation).
 * @param data     Pointer to the data to be written.
 * @param numbytes Pointer to the size of the data to be written. On output, it will
 *                 contain the actual number of bytes written.
 * @return status_t Returns B_OK if the write operation is successful,
 *                 or an appropriate error code if there is a failure.
 */
status_t
tun_write(void *cookie, off_t position, const void *data, size_t *numbytes)
{
    tun_struct *tun = static_cast<tun_struct *>(cookie);
    size_t used = tun->sendQueue->Used();
    status_t status;
    status = acquire_sem_etc(tun->selfSendSem, 1, B_CAN_INTERRUPT, 0);
    if (status < B_OK)
    {
        dprintf("Could not acquire sending sem\n");
        *numbytes = 0;
        return status;
    }
    if ((used + *numbytes) >= 30000)
    {
        dprintf("%li bytes used\n", used + *numbytes);
        return B_ERROR;
    }
    while (!*(tun->recvSem))
    {
        snooze(10000);
    }
    tun->sendQueue->Add(create_filled_buffer((uint8 *)data, *numbytes));
    release_sem(*(tun->recvSem));
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

const char **
publish_devices()
{
    /* Publish driver names to devfs */
    return device_names;
}

device_hooks *
find_device(const char *name)
{
    /* Find hooks for driver functions */
    return &tun_hooks;
}
