#include <Select.h>
#include <fs/select_sync_pool.h>


typedef struct tun_struct {
	uint32_t                    name[3];
	unsigned long               flags;
	BufferQueue*                sendQueue;
	BufferQueue*                recvQueue;
	ConditionVariable*          readWait;
	mutex                       readLock;
	union {
		struct {
			select_sync_pool*   select_pool;
			mutex               selectLock;
			bool                readable;
			bool                writable;
		}                       app;
	};
} tun_struct;


static void
tun_notify(tun_struct* tun)
{
	// bool select_pool_check = tun->app.select_pool != NULL;
	bool readable = tun->recvQueue->Used() != 0;
	if (!tun->app.readable && readable) {
		dprintf("Check Read\n");
		notify_select_event_pool(tun->app.select_pool, B_SELECT_READ);
	}
	tun->app.readable = readable;

	bool writable = tun->sendQueue->Used() != BUFFER_QUEUE_MAX;
	if (!tun->app.writable && writable) {
		dprintf("Check Write\n");
		notify_select_event_pool(tun->app.select_pool, B_SELECT_WRITE);
	}
	tun->app.writable = writable;
}


status_t
tun_write(void* cookie, off_t position, const void* data, size_t* numbytes)
{
	tun_struct* tun = static_cast<tun_struct*>(cookie);
	size_t used = tun->sendQueue->Used();
	net_buffer* packet = NULL;
	// Buffer is full or will be so we have to drop the packet
	if ((used + *numbytes) >= BUFFER_QUEUE_MAX)
		return B_WOULD_BLOCK;
	packet = create_filled_buffer((uint8*)data, *numbytes);
	if (packet == NULL)
		return B_ERROR;
	tun->sendQueue->Add(packet);
	if (tun->flags & B_SET_NONBLOCKING_IO) { // Are you the APP?
		tun->readWait->NotifyAll();
		tun_notify(tun);
	}
	return B_OK;
}


status_t
tun_read(void* cookie, off_t position, void* data, size_t* numbytes)
{
	tun_struct* tun = static_cast<tun_struct*>(cookie);
	status_t status;
	if (tun->flags & B_SET_NONBLOCKING_IO) { // Are you the APP?
		status = retreive_packet(tun->recvQueue, data, numbytes);
		if (status != B_OK)
			return -EAGAIN;
		tun_notify(tun);
	} else {
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
		mutex_unlock(&tun->readLock);
	}
	return B_OK;
}


status_t
tun_select(void* cookie, uint8 event, uint32 ref, selectsync* sync)
{
	tun_struct* tun = static_cast<tun_struct*>(cookie);
	status_t status;
	mutex_lock(&tun->app.selectLock);
	size_t readUsed = tun->recvQueue->Used();
	size_t writeUsed = tun->sendQueue->Used();
	bool wasReadable = readUsed != 0;
	bool wasWritable = writeUsed != BUFFER_QUEUE_MAX;

	if (event != B_SELECT_READ && event != B_SELECT_WRITE) {
		mutex_unlock(&tun->app.selectLock);
		return B_BAD_VALUE;
	}
	status = add_select_sync_pool_entry(&tun->app.select_pool, sync, event);
	if (status != B_OK) {
		mutex_unlock(&tun->app.selectLock);
		return B_BAD_VALUE;
	}

	if (event == B_SELECT_READ && wasReadable)
		notify_select_event(sync, event);
	if (event == B_SELECT_WRITE && wasWritable)
		notify_select_event(sync, event);
	mutex_unlock(&tun->app.selectLock);
	return B_OK;
}


status_t
tun_deselect(void* cookie, uint8 event, selectsync* sync)
{
	tun_struct* tun = static_cast<tun_struct*>(cookie);
	mutex_lock(&tun->app.selectLock);
	if (event != B_SELECT_READ && event != B_SELECT_WRITE) {
		mutex_unlock(&tun->app.selectLock);
		return B_BAD_VALUE;
	}
	status_t status = remove_select_sync_pool_entry(&tun->app.select_pool, sync, event);
	mutex_unlock(&tun->app.selectLock);
	return status;
}
