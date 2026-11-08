#include <xcron.h>
#include <xrt/atomic.h>
#include <xrt/thread.h>
#include <stdio.h>
typedef struct selectedexample {xtime First,Last;xatomic32 Calls;} selectedexample;
static bool selectedExampleClock(xcronclock* Clock,ptr Raw)
{selectedexample* C=Raw;*Clock=(xcronclock){C->Last,1};return true;}
static xcronrunresult selectedExampleRun(const xcronrun* R,const xcronexecution* T,ptr Raw)
{
    selectedexample* C=Raw;const xcronexecutionlabel* L=xcronExecutionSelected(T);
    if(R->Data!=C || !L || L->Candidates!=2 || L->Local.Year!=2024 || L->Local.Month!=11 || L->Local.Day!=3 || L->Local.Hour!=1 || L->Local.Minute!=30 || L->Local.Second || L->Local.Offset || L->Local.IsDST!=-1 || (R->ScheduledAt!=C->First && R->ScheduledAt!=C->Last))return XCRON_RUN_FAILED;
    xrtAtomic32FetchAdd(&C->Calls,1,XMEMORY_RELEASE);return XCRON_RUN_SUCCEEDED;
}
int main(void)
{
    int Exit=1;selectedexample C={0};xexecutor* E=NULL;xcronservice* S=NULL;xcronzone* Z=NULL;xcronrequestid Pending=0;xcronjobid Job=0;
    if(!xrtDateTime(2024,11,3,5,30,0,0,&C.First))goto Done;
    C.Last=C.First+XRT_TIME_HOUR;xexecutorconfig PC={2,4,0};E=xrtExecutorCreate(&PC);if(!E)goto Done;
    xcronselectedserviceconfig SC={.Service={.Scheduler={1,2,32,0,65536},.Executor=E,.Proc=selectedExampleRun,.Context=&C,.Clock=selectedExampleClock,.ClockContext=&C,.CommandLimit=2,.CompletionLimit=2,.BatchLimit=2,.CommandWorkLimit=2,.ResampleMilliseconds=5,.RetryIntervalMilliseconds=5,.RetryAttempts=3,.AdmissionMilliseconds=100,.BytesLimit=262144,.AdapterBytesLimit=65536},.SubmissionBatchLimit=2};
    xcronjobconfig J={.Begin=C.First,.End=C.Last,.Overlap=XCRON_OVERLAP_ALLOW,.RunningLimit=2,.Data=&C};
    if(!xcronServiceCreateSelected(&SC,&S,NULL) || !xcronZoneCreatePosix(XRT_STR_LITERAL("EST5EDT,M3.2.0,M11.1.0"),XCRON_ZONE_TZIF3,&Z,NULL) || !xcronExpressionCompile(XRT_STR_LITERAL("0 30 1 * * *"),XCRON_SECONDS6,&J.Expression,NULL))goto Done;
    J.Zone=Z;if(!xcronServiceAddSelectedAsync(S,&J,(xcrondeliverypolicy){XCRON_FOLD_ALL,XCRON_MISSING_REJECT},&Pending,NULL))goto Done;
    xcronZoneRelease(Z);Z=NULL;xcronselectedreply R;if(xcronServiceCollectSelected(S,Pending,10000,&R,NULL)!=XWAIT_OK)goto Done;
    Pending=0;bool Admitted=R.Code==XCRON_SERVICE_OK && R.Job && !R.Data && !R.Error;Job=R.Job;xrtErrorFree(R.Error);if(!Admitted)goto Done;
    {unsigned Seen=0;double Deadline=xrtTimer()+10;
        while(Seen!=3){xcronselectedservicecompletion D;size_t N;if(!xcronServiceDrainSelected(S,&D,1,&N,NULL) || xrtTimer()>Deadline)goto Done;if(!N){xrtThreadYield();continue;}
            unsigned K=D.Completion.ScheduledAt==C.First?0:1;bool OK=D.Completion.Job==Job && D.Completion.ScheduledAt==(K?C.Last:C.First) && !(Seen&(1u<<K)) && D.Completion.Result==XCRON_RUN_SUCCEEDED && !D.Completion.Error && D.Candidates==2 && D.Local.Hour==1 && D.Local.Minute==30;xrtErrorFree(D.Completion.Error);if(!OK)goto Done;Seen|=1u<<K;
        }
    }
    if(xrtAtomic32Load(&C.Calls,XMEMORY_ACQUIRE)!=2)goto Done;
    Exit=0;
Done:
    if(S){if(!xcronServiceClose(S,true,NULL) || xcronServiceWaitFor(S,10000,NULL)!=XWAIT_OK)return 2;
        if(Pending){xcronselectedreply R;if(xcronServiceCollectSelected(S,Pending,10000,&R,NULL)!=XWAIT_OK)return 2;xrtErrorFree(R.Error);}
        for(;;){xcronselectedservicecompletion D;size_t N;if(!xcronServiceDrainSelected(S,&D,1,&N,NULL))return 2;if(!N)break;xrtErrorFree(D.Completion.Error);}
        bool Reaped=false;for(;;){xcronselectedretired R;bool Found;if(!xcronServiceReapStoppedSelected(S,&R,&Found,NULL))return 2;if(!Found)break;if(R.Data!=&C)return 2;if(R.Id==Job && R.Info.Succeeded==2 && R.Info.Policy.Fold==XCRON_FOLD_ALL)Reaped=true;}
        xcronselectedservicestats T;if(!xcronServiceInspectSelected(S,&T,NULL) || T.Service.ActiveNativeTimers || (!Exit && !Reaped) || !xcronServiceDestroy(S,NULL))return 2;
    }
    if(E && (!xrtExecutorClose(E) || xrtExecutorWaitFor(E,10000)!=XWAIT_OK || !xrtExecutorDestroy(E)))return 2;
    xcronZoneRelease(Z);if(!Exit)puts("Selected live service preserved both fold labels and original Data retirement.");return Exit;
}
