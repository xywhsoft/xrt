#include <xcron.h>
#include <xrt/atomic.h>
#include <xrt/thread.h>
#include <stdio.h>
#include <string.h>
typedef struct batchexample {xtime First,Last;xatomic32 Read;} batchexample;
static xcronrunresult batchExampleProc(const xcronrun* R,const xcronexecution* T,ptr Raw)
{
    batchexample* C=Raw;const xcronexecutionlabel* L=xcronExecutionSelected(T);
    if(!L || (R->ScheduledAt!=C->First && R->ScheduledAt!=C->Last) || L->Local.Year!=2024 || L->Local.Month!=11 || L->Local.Day!=3 || L->Local.Hour!=1 || L->Local.Minute!=30 || L->Candidates!=2)return XCRON_RUN_FAILED;
    xrtAtomic32FetchAdd(&C->Read,1,XMEMORY_RELEASE);return XCRON_RUN_SUCCEEDED;
}
int main(void)
{
    xexecutor* E=NULL;xcronexecutor* B=NULL;xcronscheduler* S=NULL;xcronzone* Z=NULL;xcronrunid Active[2]={0};bool Transferred=false;int Exit=1;batchexample C={0};
    xcronjobconfig J={.Overlap=XCRON_OVERLAP_ALLOW,.RunningLimit=2};xcronschedulerconfig SC={1,2,8,0,65536};xcronjobid Id;
    if(!xcronZoneCreatePosix(XRT_STR_LITERAL("EST5EDT,M3.2.0,M11.1.0"),XCRON_ZONE_TZIF3,&Z,NULL) || !xcronExpressionCompile(XRT_STR_LITERAL("0 30 1 * * *"),XCRON_SECONDS6,&J.Expression,NULL) || !xrtDateTime(2024,11,3,5,30,0,0,&J.Begin))goto Done;
    J.Zone=Z;J.End=J.Begin+XRT_TIME_HOUR;C.First=J.Begin;C.Last=J.End;if(!xcronSchedulerCreateSelected(&SC,&S,NULL) || !xcronSchedulerAddSelected(S,&J,(xcrondeliverypolicy){XCRON_FOLD_ALL,XCRON_MISSING_REJECT},&Id,NULL))goto Done;
    xexecutorconfig EC={2,4,0};E=xrtExecutorCreate(&EC);xcronexecutorconfig BC={E,batchExampleProc,&C,NULL,NULL,2,0,65536};if(!E || !xcronExecutorCreateSelected(&BC,&B,NULL))goto Done;
    xcronselectedrun Runs[2];size_t Produced=0;
    for(unsigned T=0;T<128 && Produced<2;++T){xcronselectedpoll P;if(!xcronSchedulerPollSelected(S,(xcronclock){J.End,T},(xcronoccurrencebudget){1,1,1,1,1},Runs+Produced,2-Produced,&P,NULL))goto Done;
        for(size_t N=0;N<P.Poll.Count;++N)Active[Produced+N]=Runs[Produced+N].Run.Id;
        Produced+=P.Poll.Count;
    }
    if(Produced!=2 || Runs[0].Run.ScheduledAt!=C.First || Runs[1].Run.ScheduledAt!=C.Last || !xcronExecutorSubmitBatchSelected(B,Runs,2,NULL))goto Done;
    Transferred=true;memset(Runs,0xa5,sizeof(Runs));
    {size_t Received=0;double Deadline=xrtTimer()+10;
        while(Received<2){xcronselectedcompletion D;size_t N;if(!xcronExecutorDrainSelected(B,&D,1,&N,NULL) || xrtTimer()>Deadline)goto Done;if(!N){xrtThreadYield();continue;}
            unsigned K=0;while(K<2 && Active[K]!=D.Run.Run.Id)++K;bool Valid=K<2 && D.Result==XCRON_RUN_SUCCEEDED && D.Code==XCRON_EXECUTOR_OK && !D.Error && D.Run.Candidates==2;
            bool Completed=K<2 && xcronSchedulerComplete(S,D.Run.Run.Id,D.Result,NULL);if(Completed)Active[K]=0;xrtErrorFree(D.Error);if(!Valid || !Completed)goto Done;++Received;
        }
    }
    if(xrtAtomic32Load(&C.Read,XMEMORY_ACQUIRE)!=2)goto Done;
    Exit=0;puts("Selected worker batch preserved both nominal labels.");
Done:
    if(S)xcronSchedulerClose(S,NULL);
    if(B)xcronExecutorClose(B,true,NULL);
    if(E && (!xrtExecutorClose(E) || xrtExecutorWaitFor(E,10000)!=XWAIT_OK))return 2;
    if(B){for(;;){xcronselectedcompletion D;size_t N=0;if(!xcronExecutorDrainSelected(B,&D,1,&N,NULL))return 2;if(!N)break;bool Completed=S && xcronSchedulerComplete(S,D.Run.Run.Id,D.Result,NULL);for(unsigned K=0;K<2;++K){if(Completed && Active[K]==D.Run.Run.Id)Active[K]=0;}xrtErrorFree(D.Error);if(!Completed)return 2;}if(!xcronExecutorDestroy(B,NULL))return 2;}
    if(S){for(unsigned K=0;K<2;++K)if(Active[K]){if(Transferred || !xcronSchedulerComplete(S,Active[K],XCRON_RUN_CANCELLED,NULL))return 2;}
        xcronselectedretired R;bool Found;do{if(!xcronSchedulerReapSelected(S,&R,&Found,NULL))return 2;}while(Found);if(!xcronSchedulerDestroy(S,NULL))return 2;}
    if(E && !xrtExecutorDestroy(E))return 2;
    xcronZoneRelease(Z);return Exit;
}
