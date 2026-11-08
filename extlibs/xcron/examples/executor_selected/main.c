#include <xcron.h>
#include <xrt/atomic.h>
#include <xrt/thread.h>
#include <stdio.h>
typedef struct selectedexample {xtime Expected;xatomic32 Read;} selectedexample;
static xcronrunresult selectedExampleProc(const xcronrun* R,const xcronexecution* T,ptr Raw)
{
    selectedexample* C=Raw;const xcronexecutionlabel* L=xcronExecutionSelected(T);
    if(!L || R->ScheduledAt!=C->Expected || L->Local.Hour!=1 || L->Local.Minute!=30 || L->Candidates!=2)return XCRON_RUN_FAILED;
    xrtAtomic32Store(&C->Read,1,XMEMORY_RELEASE);return XCRON_RUN_SUCCEEDED;
}
int main(void)
{
    xexecutor* E=NULL;xcronexecutor* B=NULL;xcronscheduler* S=NULL;xcronzone* Z=NULL;xcronrunid Active=0;bool Transferred=false;int Exit=1;selectedexample C={0};
    xcronjobconfig J={.RunningLimit=1};xcronschedulerconfig SC={1,1,8,0,65536};xcronjobid Id;
    if(!xcronZoneCreatePosix(XRT_STR_LITERAL("EST5EDT,M3.2.0,M11.1.0"),XCRON_ZONE_TZIF3,&Z,NULL) || !xcronExpressionCompile(XRT_STR_LITERAL("0 30 1 * * *"),XCRON_SECONDS6,&J.Expression,NULL) || !xrtDateTime(2024,11,3,5,30,0,0,&J.Begin))goto Done;
    J.Zone=Z;J.End=J.Begin+XRT_TIME_HOUR;C.Expected=J.End;if(!xcronSchedulerCreateSelected(&SC,&S,NULL) || !xcronSchedulerAddSelected(S,&J,(xcrondeliverypolicy){2,1},&Id,NULL))goto Done;
    xexecutorconfig EC={1,4,0};E=xrtExecutorCreate(&EC);xcronexecutorconfig BC={E,selectedExampleProc,&C,NULL,NULL,1,0,65536};if(!E || !xcronExecutorCreateSelected(&BC,&B,NULL))goto Done;
    for(unsigned T=0;T<128;++T){xcronselectedrun R;xcronselectedpoll P;if(!xcronSchedulerPollSelected(S,(xcronclock){J.End,T},(xcronoccurrencebudget){1,1,1,1,1},&R,1,&P,NULL))goto Done;
        if(P.Poll.Count){Active=R.Run.Id;if(!xcronExecutorSubmitSelected(B,&R,NULL))goto Done;Transferred=true;break;}}
    if(!Transferred)goto Done;
    {size_t N=0;xcronselectedcompletion D;double Deadline=xrtTimer()+10;
        while(!N){if(!xcronExecutorDrainSelected(B,&D,1,&N,NULL) || xrtTimer()>Deadline)goto Done;if(!N)xrtThreadYield();}
        Transferred=false;bool Valid=D.Result==XCRON_RUN_SUCCEEDED && D.Code==XCRON_EXECUTOR_OK && !D.Error && D.Run.Run.ScheduledAt==C.Expected && D.Run.Candidates==2 && xrtAtomic32Load(&C.Read,XMEMORY_ACQUIRE)==1;
        bool Completed=xcronSchedulerComplete(S,D.Run.Run.Id,D.Result,NULL);if(Completed)Active=0;xrtErrorFree(D.Error);if(!Valid || !Completed)goto Done;
    }
    Exit=0;puts("Selected worker read immutable nominal metadata.");
Done:
    if(S)xcronSchedulerClose(S,NULL);
    if(B)xcronExecutorClose(B,true,NULL);
    if(E && (!xrtExecutorClose(E) || xrtExecutorWaitFor(E,10000)!=XWAIT_OK))return 2;
    if(B){for(;;){xcronselectedcompletion D;size_t N=0;if(!xcronExecutorDrainSelected(B,&D,1,&N,NULL))return 2;if(!N)break;if(S)xcronSchedulerComplete(S,D.Run.Run.Id,D.Result,NULL);if(D.Run.Run.Id==Active)Active=0;xrtErrorFree(D.Error);}if(!xcronExecutorDestroy(B,NULL))return 2;}
    if(S){if(Active && !Transferred)xcronSchedulerComplete(S,Active,XCRON_RUN_CANCELLED,NULL);xcronselectedretired R;bool Found;do{Found=false;xcronSchedulerReapSelected(S,&R,&Found,NULL);}while(Found);if(!xcronSchedulerDestroy(S,NULL))return 2;}
    if(E && !xrtExecutorDestroy(E))return 2;
    xcronZoneRelease(Z);return Exit;
}
