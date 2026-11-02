/* Offline actual worker execution, owner completion and full retirement.
 * python tools/build.py --compiler gcc --manifest extlibs/xcron/config/modules.json --suite xcron_executor --no-single --cflag=-O2
 * Expect exactly one worker call, one owner completion and one job receipt.
 * No service/account/environment is required. Clock sample is deterministic. */
#include <xcron.h>
#include <xrt/atomic.h>
#include <xrt/thread.h>
#include <stdio.h>
static xcronrunresult execute(const xcronrun* Run,const xcronexecution* Token,ptr Context)
{(void)Context;if(xcronExecutionCancelled(Token))return XCRON_RUN_CANCELLED;xrtAtomic32FetchAdd(Run->Data,1,XMEMORY_RELAXED);return XCRON_RUN_SUCCEEDED;}
int main(void)
{
    int Exit=1;xexecutor* Pool=NULL;xcronexecutor* Adapter=NULL;xcronscheduler* Scheduler=NULL;xcronzone* Zone=NULL;xatomic32 Calls=XRT_ATOMIC32_INIT(0);xcronrunid Lease=0;bool Submitted=false;
    xexecutorconfig PC={2,8,0};xcronexecutorconfig EC={0};xcronschedulerconfig SC={1,1,16,0,65536};xcronjobconfig Job={.Begin=0,.End=0,.RunningLimit=1,.Data=&Calls};xcronjobid Id;
    Pool=xrtExecutorCreate(&PC);if(!Pool)goto Done;EC.Executor=Pool;EC.Proc=execute;EC.RunLimit=1;EC.BytesLimit=65536;
    if(!xcronExecutorCreate(&EC,&Adapter,NULL) || !xcronSchedulerCreate(&SC,&Scheduler,NULL) || !xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_TZIF3,&Zone,NULL) || !xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&Job.Expression,NULL))goto Done;
    Job.Zone=Zone;if(!xcronSchedulerAdd(Scheduler,&Job,&Id,NULL))goto Done;xcronZoneRelease(Zone);Zone=NULL;
    xcronrun Run;xcronpoll Poll;if(!xcronSchedulerPoll(Scheduler,(xcronclock){0,0},&Run,1,&Poll,NULL) || Poll.Count!=1)goto Done;Lease=Run.Id;
    if(!xcronExecutorSubmit(Adapter,&Run,NULL))goto Done;
    Submitted=true;
    xcroncompletion Completion;size_t Count;double Limit=xrtTimer()+10;
    do{if(!xcronExecutorDrain(Adapter,&Completion,1,&Count,NULL) || xrtTimer()>Limit)goto Done;if(!Count)xrtThreadYield();}while(!Count);
    Submitted=false;if(!xcronSchedulerComplete(Scheduler,Completion.Run.Id,Completion.Result,NULL)){xrtErrorFree(Completion.Error);goto Done;}Lease=0;
    bool OK=Completion.Result==XCRON_RUN_SUCCEEDED && !Completion.Error && xrtAtomic32Load(&Calls,XMEMORY_RELAXED)==1;xrtErrorFree(Completion.Error);if(!OK)goto Done;
    xcronretired Receipt;bool Found=false;if(!xcronSchedulerReap(Scheduler,&Receipt,&Found,NULL) || !Found || Receipt.Id!=Id || Receipt.Info.Succeeded!=1)goto Done;
    puts("one worker call; one owner completion; one retired job");Exit=0;
Done:
    if(Adapter)xcronExecutorClose(Adapter,true,NULL);
    if(Pool){if(Exit)xrtExecutorCancel(Pool);else xrtExecutorClose(Pool);if(xrtExecutorWaitFor(Pool,10000)!=XWAIT_OK)Exit=1;}
    if(Adapter){xcroncompletion C;size_t N;do{N=0;if(!xcronExecutorDrain(Adapter,&C,1,&N,NULL)){Exit=1;break;}if(N){if(Scheduler)xcronSchedulerComplete(Scheduler,C.Run.Id,C.Result,NULL);if(C.Run.Id==Lease)Lease=0;xrtErrorFree(C.Error);}}while(N);
        if(!xcronExecutorDestroy(Adapter,NULL))Exit=1;}
    if(Scheduler){if(Lease && !Submitted)xcronSchedulerComplete(Scheduler,Lease,XCRON_RUN_CANCELLED,NULL);xcronSchedulerClose(Scheduler,NULL);xcronretired R;bool Found;do{Found=false;xcronSchedulerReap(Scheduler,&R,&Found,NULL);}while(Found);if(!xcronSchedulerDestroy(Scheduler,NULL))Exit=1;}
    xcronZoneRelease(Zone);if(Pool && !xrtExecutorDestroy(Pool))Exit=1;xrtClearError();return Exit;
}
