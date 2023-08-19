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
    if (tun->flags & B_SET_NONBLOCKING_IO) { // APP only
        dprintf("Notifying Select (Write)\n");
        tun_notify(tun);
    }
    return B_OK;
}
 
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
 
    if (tun->flags & B_SET_NONBLOCKING_IO) { // APP only
        dprintf("Notifying Select (Read)\n");
        tun_notify(tun);
    }
 
    mutex_unlock(&tun->readLock);
    return B_OK;
}
