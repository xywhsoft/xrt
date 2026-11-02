#include <xrt.h>
#include <stdio.h>
int main(void)
{
    xevent Event;xeventwaiter Waiter;xeventwaiterinfo Info;
    if(!xrtEventInit(&Event,false,false) || !xrtEventWaiterInit(&Waiter,&Event,0) || !xrtEventWaiterGet(&Waiter,&Info))return 1;
    if(!xrtEventSet(&Event) || xrtEventWaiterWaitFor(&Waiter,0)!=XWAIT_OK)return 2;
    double Start=xrtTimer();if(xrtEventWaiterWaitFor(&Waiter,5)!=XWAIT_TIMEOUT || xrtTimer()-Start<0.005)return 3;
    if(!xrtEventWaiterUnit(&Waiter) || !xrtEventSet(&Event) || xrtEventWaitFor(&Event,0)!=XWAIT_OK || !xrtEventUnit(&Event))return 4;
    printf("[PASS] reusable deadline context; backend=%u native_timers=%u storage=%zu\n",(unsigned)Info.Backend,Info.NativeTimers,Info.StorageBytes);return 0;
}
