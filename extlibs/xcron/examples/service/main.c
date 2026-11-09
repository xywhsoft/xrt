#include <xcron.h>
#include <xrt/atomic.h>
#include <xrt/thread.h>
#include <stdio.h>
/* The host owns the shared pool and business data through the retirement receipt. */
static xcronrunresult exampleRun(const xcronrun* Run,const xcronexecution* Token,ptr Context)
{(void)Token;if(Run->Data!=Context)return XCRON_RUN_FAILED;xrtAtomic32Store((xatomic32*)Context,1,XMEMORY_RELEASE);return XCRON_RUN_SUCCEEDED;}
int main(void)
{
    xatomic32 Calls={0};xexecutorconfig P={2,16,0};xexecutor* Pool=xrtExecutorCreate(&P);if(!Pool)return 1;
    xcronserviceconfig C={.Scheduler={8,4,32,0,65536},.Executor=Pool,.Proc=exampleRun,.Context=&Calls,.CommandLimit=8,.CompletionLimit=8,.BatchLimit=4,.CommandWorkLimit=8,.ResampleMilliseconds=10,.RetryIntervalMilliseconds=5,.RetryAttempts=3,.AdmissionMilliseconds=100,.BytesLimit=262144,.AdapterBytesLimit=65536};
    xcronservice* S=NULL;xcronzone* Z=NULL;xcronexpression Expression;
    if(!xcronServiceCreate(&C,&S,NULL) || !xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_TZIF3,&Z,NULL) || !xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&Expression,NULL))return 2;
    xtime Now=xrtNow(),Next=Now-Now%1000+1000;xcronjobconfig J={.Expression=Expression,.Zone=Z,.Begin=Next,.End=Next,.RunningLimit=1,.Data=&Calls};xcronrequestid Q;
    if(!xcronServiceAddAsync(S,&J,&Q,NULL))return 3;
    xcronZoneRelease(Z);xcronreply Reply;
    if(xcronServiceCollect(S,Q,10000,&Reply,NULL)!=XWAIT_OK || Reply.Code || Reply.Error)return 4;
    if(xcronServiceWaitReadable(S,10000,NULL)!=XWAIT_OK || !xrtAtomic32Load(&Calls,XMEMORY_ACQUIRE))return 5;
    /* Join does not require consuming reserved outputs. No user Data is freed here. */
    if(!xcronServiceClose(S,false,NULL) || xcronServiceWaitFor(S,10000,NULL)!=XWAIT_OK)return 6;
    xcronservicestats Stats;if(!xcronServiceInspect(S,&Stats,NULL) || Stats.ActiveNativeTimers)return 10;
    xcronretired Retired;bool Found=false;
    if(!xcronServiceReapStopped(S,&Retired,&Found,NULL) || !Found || Retired.Id!=Reply.Job || Retired.Data!=&Calls || Retired.Info.Succeeded!=1)return 7;
    xcronservicecompletion Done;size_t Count=0;
    if(!xcronServiceDrain(S,&Done,1,&Count,NULL) || Count!=1 || Done.Result!=XCRON_RUN_SUCCEEDED || Done.Error || Done.ScheduledAt!=Next)return 8;
    if(!xcronServiceDestroy(S,NULL) || !xrtExecutorClose(Pool) || xrtExecutorWaitFor(Pool,10000)!=XWAIT_OK || !xrtExecutorDestroy(Pool))return 9;
    printf("[waiter] backend=%d storage=%zu bytes timers-after-join=%u\n",(int)Stats.Waiter.Backend,Stats.Waiter.StorageBytes,Stats.ActiveNativeTimers);
    puts("[PASS] real UTC deadline, borrowed executor, reserved metadata and original-data retirement");return 0;
}
