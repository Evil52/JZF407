#include "net_ready.h"

osSemaphoreId_t netReadySem;

void net_ready_init(void)
{
    /* Counting semaphore, max=1, initial=0; consumers acquire and immediately release */
    netReadySem = osSemaphoreNew(1, 0, NULL);
}

void net_ready_signal(void)
{
    if (netReadySem) osSemaphoreRelease(netReadySem);
}

void net_ready_wait(void)
{
    if (!netReadySem) return;
    osSemaphoreAcquire(netReadySem, osWaitForever);
    osSemaphoreRelease(netReadySem); /* re-arm so others can pass too */
}
