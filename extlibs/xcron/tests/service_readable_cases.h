#ifndef XCRON_TEST_SERVICE_READABLE_CASES_H
#define XCRON_TEST_SERVICE_READABLE_CASES_H
#include <xrt/memory_stats.h>
typedef struct rwfixture {
    xcronservice* Service;xatomic64 Wall,Mono,Calls;xatomic32 Gate,Started,Probe,Probed,FailClock;
    bool Selected,FailBody;
} rwfixture;
static bool rwClock(xcronclock* C,ptr Raw)
{
    rwfixture* F=Raw;if(xrtAtomic32Load(&F->FailClock,XMEMORY_ACQUIRE)){xrtSetErrorInfo(XERR_IO,"readable.clock",17,"clock failed");return false;}
    *C=(xcronclock){(xtime)xrtAtomic64Load(&F->Wall,XMEMORY_ACQUIRE),xrtAtomic64Load(&F->Mono,XMEMORY_ACQUIRE)};
    uint32 Probe=xrtAtomic32Exchange(&F->Probe,0,XMEMORY_ACQ_REL);
    if(Probe){xcronserviceissue I;if(Probe==1){testRequire(xcronServiceWaitReadable(F->Service,0,&I)==XWAIT_TIMEOUT && !I.Code,"owner can poll empty completion channel");testRequire(xcronServiceWaitReadable(F->Service,10,&I)==XWAIT_ERROR && I.Code==XCRON_SERVICE_THREAD,"owner cannot wait for its own metadata publication");xrtClearError();}else testRequire(xcronServiceWaitReadable(F->Service,0,NULL)==XWAIT_OK && xcronServiceWaitReadable(F->Service,10,NULL)==XWAIT_OK,"ready wins over owner self-wait rejection without blocking");xrtAtomic32Store(&F->Probed,1,XMEMORY_RELEASE);}return true;
}
static xcronrunresult rwProc(const xcronrun* R,const xcronexecution* T,ptr Raw)
{
    rwfixture* F=Raw;testRequire(R->Data==F,"readability preserves original Data");
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
    if(F->Selected){const xcronexecutionlabel* L=xcronExecutionSelected(T);xdatetime Expected;xrtTimeSplit(R->ScheduledAt,&Expected);Expected.Offset=0;Expected.IsDST=-1;testRequire(L && L->Candidates==1 && !memcmp(&Expected,&L->Local,sizeof(Expected)),"readability preserves complete selected worker view");}
#endif
    xrtAtomic64FetchAdd(&F->Calls,1,XMEMORY_RELAXED);xrtAtomic32FetchAdd(&F->Started,1,XMEMORY_RELEASE);double End=xrtTimer()+10;
    while(!xrtAtomic32Load(&F->Gate,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"readable body physical release");xrtThreadYield();}
    if(F->FailBody){xrtSetErrorInfo(XERR_IO,"readable.body",23,"owned worker error");return XCRON_RUN_FAILED;}
    return xcronExecutionCancelled(T)?XCRON_RUN_CANCELLED:XCRON_RUN_SUCCEEDED;
}
static rwfixture rwFixture(bool Selected)
{rwfixture F={.Selected=Selected};xrtAtomic64Init(&F.Wall,(uint64)XRT_TIME_UNIX_EPOCH);xrtAtomic64Init(&F.Mono,100);xrtAtomic32Init(&F.Gate,1);return F;}
static xexecutor* rwStart(rwfixture* F)
{
    xexecutorconfig P={2,16,0};xexecutor* E=xrtExecutorCreate(&P);testRequire(E!=NULL,"readable borrowed pool");xcronserviceconfig C={.Scheduler={8,4,32,0,65536},.Executor=E,.Proc=rwProc,.Context=F,.Clock=rwClock,.ClockContext=F,.CommandLimit=8,.CompletionLimit=4,.BatchLimit=4,.CommandWorkLimit=8,.ResampleMilliseconds=5,.RetryIntervalMilliseconds=5,.RetryAttempts=3,.AdmissionMilliseconds=100,.BytesLimit=262144,.AdapterBytesLimit=65536};bool OK;
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
    if(F->Selected){xcronselectedserviceconfig S={.Service=C,.SubmissionBatchLimit=4};OK=xcronServiceCreateSelected(&S,&F->Service,NULL);}else
#endif
    OK=xcronServiceCreate(&C,&F->Service,NULL);
    testRequire(OK,"readable service factory");return E;
}
static xcronjobid rwAdd(rwfixture* F,xtime Begin,xtime End)
{
    xcronzone* Z=NULL;testRequire(xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_TZIF3,&Z,NULL),"readable UTC fixture");xcronjobconfig J={.Zone=Z,.Begin=Begin,.End=End,.RunningLimit=1,.Data=F};testRequire(xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&J.Expression,NULL),"readable expression");xcronrequestid Q;xcronjobid Id;
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
    if(F->Selected){testRequire(xcronServiceAddSelectedAsync(F->Service,&J,(xcrondeliverypolicy){0,1},&Q,NULL),"readable selected Add");xcronselectedreply R;testRequire(xcronServiceCollectSelected(F->Service,Q,10000,&R,NULL)==XWAIT_OK && !R.Code && R.Job && !R.Error,"readable selected admission");Id=R.Job;}else
#endif
    {testRequire(xcronServiceAddAsync(F->Service,&J,&Q,NULL),"readable actual Add");xcronreply R;testRequire(xcronServiceCollect(F->Service,Q,10000,&R,NULL)==XWAIT_OK && !R.Code && R.Job && !R.Error,"readable actual admission");Id=R.Job;}
    xcronZoneRelease(Z);return Id;
}
static xcronservicestats rwStats(rwfixture* F)
{
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
    if(F->Selected){xcronselectedservicestats T;testRequire(xcronServiceInspectSelected(F->Service,&T,NULL),"readable selected stats");return T.Service;}
#endif
    xcronservicestats T;testRequire(xcronServiceInspect(F->Service,&T,NULL),"readable stats");return T;
}
static size_t rwDrain(rwfixture* F,xcronservicecompletion* D)
{
    size_t N;
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
    if(F->Selected){xcronselectedservicecompletion R;testRequire(xcronServiceDrainSelected(F->Service,&R,1,&N,NULL),"readable selected drain");if(N){xdatetime L;xrtTimeSplit(R.Completion.ScheduledAt,&L);L.Offset=0;L.IsDST=-1;testRequire(R.Candidates==1 && !memcmp(&L,&R.Local,sizeof(L)),"ready wait cannot truncate selected nominal record");*D=R.Completion;}return N;}
#endif
    testRequire(xcronServiceDrain(F->Service,D,1,&N,NULL),"readable actual drain");return N;
}
static void rwClear(rwfixture* F,xexecutor* E)
{
    testRequire(xcronServiceClose(F->Service,true,NULL) && xcronServiceWaitFor(F->Service,10000,NULL)==XWAIT_OK,"readable owner physical join");for(;;){xcronservicecompletion D;if(!rwDrain(F,&D))break;xrtErrorFree(D.Error);}
    for(;;){bool Found;ptr Data;
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
        if(F->Selected){xcronselectedretired R;testRequire(xcronServiceReapStoppedSelected(F->Service,&R,&Found,NULL),"readable selected retirement");Data=Found?R.Data:NULL;}else
#endif
        {xcronretired R;testRequire(xcronServiceReapStopped(F->Service,&R,&Found,NULL),"readable actual retirement");Data=Found?R.Data:NULL;}if(!Found)break;testRequire(Data==F,"readable wait never consumes Data duty");}
    testRequire(xcronServiceDestroy(F->Service,NULL) && xrtExecutorClose(E) && xrtExecutorWaitFor(E,10000)==XWAIT_OK && xrtExecutorDestroy(E),"readable final caller/pool lifetime");
}
static void rwUntil(const xatomic32* Value,uint32 N,cstr Text)
{double End=xrtTimer()+10;while(xrtAtomic32Load(Value,XMEMORY_ACQUIRE)<N){testRequire(xrtTimer()<End,Text);xrtThreadYield();}}
typedef struct rwwaiter {rwfixture* F;int64 Timeout;xatomic32 Entered,Done;xwaitresult Result;xcronserviceissue Issue;double Elapsed;bool Ambient,StickyIdentity;} rwwaiter;
static int32 rwWaitThread(ptr Raw)
{
    rwwaiter* W=Raw;xrtSetErrorInfo(XERR_IO,"readable.caller",39,"caller identity");xerror* Before=xrtErrorRef(xrtGetError());xrtAtomic32Store(&W->Entered,1,XMEMORY_RELEASE);double Start=xrtTimer();W->Result=xcronServiceWaitReadable(W->F->Service,W->Timeout,&W->Issue);W->Elapsed=(xrtTimer()-Start)*1000.0;W->Ambient=xrtGetError()==Before;if(W->Result==XWAIT_ERROR && W->Issue.Code==XCRON_SERVICE_CLOCK){xerror* Sticky=NULL;testRequire(xcronServiceFailure(W->F->Service,&Sticky,NULL),"read original sticky service error");W->StickyIdentity=Sticky && Sticky==xrtGetError();xrtErrorFree(Sticky);}xrtErrorFree(Before);xrtClearError();xrtAtomic32Store(&W->Done,1,XMEMORY_RELEASE);return 0;
}
static xthread* rwSpawn(rwwaiter* W)
{xthread* T=xrtThreadCreate(rwWaitThread,W,0);testRequire(T!=NULL,"readable consumer thread");rwUntil(&W->Entered,1,"readable consumer starts");return T;}
static void rwJoin(xthread* T)
{testRequire(xrtThreadWaitFor(T,10000)==XWAIT_OK,"readable consumer joins");xrtThreadDestroy(T);}
static void rwForeign(ptr Raw)
{rwwaiter* W=Raw;W->Result=xcronServiceWaitReadable(W->F->Service,10,&W->Issue);testRequire(W->Result==XWAIT_ERROR && W->Issue.Code==XCRON_SERVICE_THREAD,"unrelated borrowed-pool callback readiness self-wait rejected");xrtClearError();testRequire(xcronServiceWaitReadable(W->F->Service,0,NULL)==XWAIT_TIMEOUT,"foreign callback can poll pending results");xrtAtomic32Store(&W->Done,1,XMEMORY_RELEASE);}
static void rwForeignReady(ptr Raw)
{rwwaiter* W=Raw;testRequire(xcronServiceWaitReadable(W->F->Service,10,NULL)==XWAIT_OK,"ready result can be observed from unrelated borrowed-pool callback");xrtAtomic32Store(&W->Done,1,XMEMORY_RELEASE);}
static void testRWReadiness(bool Selected)
{
    rwfixture F=rwFixture(Selected);xexecutor* E=rwStart(&F);xcronserviceissue I={XCRON_SERVICE_STALE,XERR_IO},Before=I;testRequire(xcronServiceWaitReadable(NULL,1,&I)==XWAIT_ERROR && !memcmp(&I,&Before,sizeof(I)) && xcronServiceWaitReadable(F.Service,-1,&I)==XWAIT_ERROR && !memcmp(&I,&Before,sizeof(I)),"readable invalid arguments preserve Issue");xrtClearError();testRequire(xcronServiceWaitReadable(F.Service,0,(xcronserviceissue*)F.Service)==XWAIT_ERROR,"readable Issue cannot overlap service arena");xrtClearError();testRequire(xcronServiceWaitReadable(F.Service,0,NULL)==XWAIT_TIMEOUT,"new empty service is not readable");
    xrtAtomic32Store(&F.Probe,1,XMEMORY_RELEASE);xcronServiceWake(F.Service,NULL);rwUntil(&F.Probed,1,"owner readiness self-wait probes");rwwaiter Foreign={.F=&F};testRequire(xrtExecutorSubmit(E,rwForeign,&Foreign,NULL,NULL),"foreign pool readiness probe");rwUntil(&Foreign.Done,1,"foreign readiness probe returned");
    xrtAtomic32Store(&F.Gate,0,XMEMORY_RELEASE);rwAdd(&F,XRT_TIME_UNIX_EPOCH,XRT_TIME_UNIX_EPOCH);rwUntil(&F.Started,1,"readable body holds physical result");rwwaiter W[3]={{.F=&F,.Timeout=10000},{.F=&F,.Timeout=10000},{.F=&F,.Timeout=10000}};xthread* T[3];for(unsigned N=0;N<3;++N)T[N]=rwSpawn(&W[N]);xrtAtomic32Store(&F.Gate,1,XMEMORY_RELEASE);
    for(unsigned N=0;N<3;++N){rwJoin(T[N]);testRequire(W[N].Result==XWAIT_OK && !W[N].Issue.Code && W[N].Ambient,"all readable waiters wake without consuming metadata/ambient error");}
    xrtAtomic32Store(&F.Probed,0,XMEMORY_RELEASE);xrtAtomic32Store(&F.Probe,2,XMEMORY_RELEASE);xcronServiceWake(F.Service,NULL);rwUntil(&F.Probed,1,"owner observes ready result");Foreign=(rwwaiter){.F=&F};testRequire(xrtExecutorSubmit(E,rwForeignReady,&Foreign,NULL,NULL),"foreign ready callback");rwUntil(&Foreign.Done,1,"foreign ready callback returned");
    xcronservicestats S=rwStats(&F);testRequire(S.ReadyCompletions==1 && S.ReservedCompletions==1 && !S.Collected,"readiness leaves every completion credit and receipt intact");testRequire(xcronServiceClose(F.Service,false,NULL) && xcronServiceWaitFor(F.Service,10000,NULL)==XWAIT_OK && xcronServiceWaitReadable(F.Service,0,NULL)==XWAIT_OK && xcronServiceWaitReadable(F.Service,100,NULL)==XWAIT_OK,"ready metadata wins even after terminal stop");xcronservicecompletion D;testRequire(rwDrain(&F,&D)==1 && !D.Error && D.Result==XCRON_RUN_SUCCEEDED,"readability transfers no hidden receipt ownership");testRequire(xcronServiceWaitReadable(F.Service,0,&I)==XWAIT_ERROR && I.Code==XCRON_SERVICE_CLOSED,"stopped empty is explicit CLOSED");xrtClearError();rwClear(&F,E);
}
static void testRWCloseAndErrors(bool Selected)
{
    rwfixture F=rwFixture(Selected);xexecutor* E=rwStart(&F);xrtAtomic32Store(&F.Gate,0,XMEMORY_RELEASE);rwAdd(&F,XRT_TIME_UNIX_EPOCH,XRT_TIME_UNIX_EPOCH);rwUntil(&F.Started,1,"readable cancelled body retains physical tail");testRequire(xcronServiceClose(F.Service,true,NULL) && xcronServiceWaitReadable(F.Service,0,NULL)==XWAIT_TIMEOUT,"closing without physical result is not EOF");rwwaiter W={.F=&F,.Timeout=10000};xthread* T=rwSpawn(&W);testRequire(!xrtAtomic32Load(&W.Done,XMEMORY_ACQUIRE),"readiness does not infer completion from cancellation");xrtAtomic32Store(&F.Gate,1,XMEMORY_RELEASE);rwJoin(T);testRequire(W.Result==XWAIT_OK,"cancelled physical result wakes reader");xcronservicecompletion D;testRequire(rwDrain(&F,&D)==1 && D.Result==XCRON_RUN_CANCELLED && !D.Error,"cancelled receipt remains owned by Drain");rwClear(&F,E);
    F=rwFixture(Selected);F.FailBody=true;E=rwStart(&F);rwAdd(&F,XRT_TIME_UNIX_EPOCH,XRT_TIME_UNIX_EPOCH);testRequire(xcronServiceWaitReadable(F.Service,10000,NULL)==XWAIT_OK && xcronServiceWaitReadable(F.Service,0,NULL)==XWAIT_OK,"worker error is readable without taking Error");testRequire(rwDrain(&F,&D)==1 && D.Error && xrtErrorCode(D.Error)==23,"worker Error transferred exactly once by Drain");xrtErrorFree(D.Error);rwClear(&F,E);
    F=rwFixture(Selected);E=rwStart(&F);W=(rwwaiter){.F=&F,.Timeout=10000};T=rwSpawn(&W);xrtAtomic32Store(&F.FailClock,1,XMEMORY_RELEASE);xcronServiceWake(F.Service,NULL);rwJoin(T);testRequire(W.Result==XWAIT_ERROR && W.Issue.Code==XCRON_SERVICE_CLOCK && W.StickyIdentity,"empty failed owner returns original sticky error, never caller ambient cause");rwClear(&F,E);
    F=rwFixture(Selected);E=rwStart(&F);W=(rwwaiter){.F=&F,.Timeout=10000};T=rwSpawn(&W);testRequire(xcronServiceClose(F.Service,false,NULL),"close idle readable service");rwJoin(T);testRequire(W.Result==XWAIT_ERROR && W.Issue.Code==XCRON_SERVICE_CLOSED,"normal stopped-empty wakes all result readers");rwClear(&F,E);
}
static void testRWTimeoutAndWarm(bool Selected)
{
    rwfixture F=rwFixture(Selected);xexecutor* E=rwStart(&F);rwwaiter W={.F=&F,.Timeout=60};xthread* T=rwSpawn(&W);double End=xrtTimer()+2;
    while(!xrtAtomic32Load(&W.Done,XMEMORY_ACQUIRE)){xcronrequestid Q;testRequire(xcronServiceReapAsync(F.Service,&Q,NULL),"unrelated reply broadcast during readable timeout");
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
        if(Selected){xcronselectedreply R;testRequire(xcronServiceCollectSelected(F.Service,Q,10000,&R,NULL)==XWAIT_OK && !R.Code && !R.Error && !R.Found,"selected unrelated ready reply");}else
#endif
        {xcronreply R;testRequire(xcronServiceCollect(F.Service,Q,10000,&R,NULL)==XWAIT_OK && !R.Code && !R.Error && !R.Found,"actual unrelated ready reply");}testRequire(xrtTimer()<End,"unrelated wakes cannot extend total readable timeout");}
    rwJoin(T);testRequire(W.Result==XWAIT_TIMEOUT && !W.Issue.Code && W.Ambient && W.Elapsed>=40 && W.Elapsed<1000,"readable timeout has one budget and preserves ambient error");rwClear(&F,E);
    F=rwFixture(Selected);E=rwStart(&F);const uint32 Rounds=512;rwAdd(&F,XRT_TIME_UNIX_EPOCH,XRT_TIME_UNIX_EPOCH+(xtime)Rounds*1000);testRequire(xcronServiceWaitReadable(F.Service,10000,NULL)==XWAIT_OK,"warm initial readable result");xcronservicecompletion D;testRequire(rwDrain(&F,&D)==1 && !D.Error,"warm initial result drained");xrtSetErrorInfo(XERR_STATE,"readable.warm",41,"ambient identity");xerror* Ambient=xrtErrorRef(xrtGetError());
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats A,B;xrtMemStatsEnable(true);xrtMemStatsGet(&A);
#endif
    for(uint32 N=1;N<=Rounds;++N){xtime When=XRT_TIME_UNIX_EPOCH+(xtime)N*1000;xrtAtomic64Store(&F.Wall,(uint64)When,XMEMORY_RELEASE);xrtAtomic64Store(&F.Mono,100+(uint64)N*1000,XMEMORY_RELEASE);testRequire(xcronServiceWake(F.Service,NULL) && xcronServiceWaitReadable(F.Service,10000,NULL)==XWAIT_OK && rwDrain(&F,&D)==1 && D.ScheduledAt==When && !D.Error && xrtGetError()==Ambient,"warmed readiness/slot reuse preserves result and ambient identity");}
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&B);xrtMemStatsEnable(false);testRequire(A.MallocCalls==B.MallocCalls && A.CallocCalls==B.CallocCalls && A.ReallocCalls==B.ReallocCalls && A.BlockAllocCalls==B.BlockAllocCalls,"warmed readable waits request no heap");
#endif
    xrtErrorFree(Ambient);xrtClearError();rwClear(&F,E);
}
typedef struct rwconsumer {rwfixture* F;xcronjobid Job;xatomic32* Seen;xatomic32* Received;uint32 Count;} rwconsumer;
static int32 rwConsumeThread(ptr Raw)
{
    rwconsumer* C=Raw;double End=xrtTimer()+10;
    for(;;){xcronserviceissue I;xwaitresult R=xcronServiceWaitReadable(C->F->Service,1000,&I);if(R==XWAIT_ERROR){testRequire(I.Code==XCRON_SERVICE_CLOSED,"competing readable consumers terminate at EOF");xrtClearError();break;}testRequire(xrtTimer()<End,"competing readable consumer deadline");if(R==XWAIT_TIMEOUT)continue;xcronservicecompletion D;if(!rwDrain(C->F,&D))continue;
        testRequire(D.Job==C->Job && D.ScheduledAt>=XRT_TIME_UNIX_EPOCH && (D.ScheduledAt-XRT_TIME_UNIX_EPOCH)%1000==0 && D.Result==XCRON_RUN_SUCCEEDED && !D.Error,"competing consumer receives valid physical metadata");uint64 N=(uint64)(D.ScheduledAt-XRT_TIME_UNIX_EPOCH)/1000;testRequire(N<C->Count,"competing consumer expected ordinal");uint32 Empty=0;testRequire(xrtAtomic32CompareExchange(&C->Seen[N],&Empty,1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE),"readable indication never duplicates a drained result");xrtAtomic32FetchAdd(C->Received,1,XMEMORY_RELEASE);
    }return 0;
}
static void testRWCompetingConsumers(bool Selected)
{
    const uint32 Count=512;rwfixture F=rwFixture(Selected);xrtAtomic64Store(&F.Wall,(uint64)(XRT_TIME_UNIX_EPOCH+(Count-1)*1000),XMEMORY_RELEASE);xexecutor* E=rwStart(&F);xcronjobid Job=rwAdd(&F,XRT_TIME_UNIX_EPOCH,XRT_TIME_UNIX_EPOCH+(Count-1)*1000);xatomic32 Seen[512],Received={0};for(uint32 N=0;N<Count;++N)xrtAtomic32Init(&Seen[N],0);rwconsumer C={&F,Job,Seen,&Received,Count};xthread* T[4];for(unsigned N=0;N<4;++N){T[N]=xrtThreadCreate(rwConsumeThread,&C,0);testRequire(T[N]!=NULL,"competing readable consumer");}rwUntil(&Received,Count,"every competing-consumer receipt returned");testRequire(xcronServiceClose(F.Service,false,NULL),"close after every competing consumer result");for(unsigned N=0;N<4;++N)rwJoin(T[N]);for(uint32 N=0;N<Count;++N)testRequire(xrtAtomic32Load(&Seen[N],XMEMORY_ACQUIRE)==1,"every expected event drained exactly once under competing waits");rwClear(&F,E);
}
typedef struct rwhold {xatomic32 Gate,Started;} rwhold;
static void rwHoldPool(ptr Raw)
{rwhold* B=Raw;xrtAtomic32FetchAdd(&B->Started,1,XMEMORY_RELEASE);double End=xrtTimer()+10;while(!xrtAtomic32Load(&B->Gate,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"readable foreign native queue release");xrtThreadYield();}}
static void testRWUnsent(bool Selected)
{
    rwfixture F=rwFixture(Selected);xexecutor* E=rwStart(&F);rwhold B={0};for(unsigned N=0;N<2;++N)testRequire(xrtExecutorSubmit(E,rwHoldPool,&B,NULL,NULL),"occupy borrowed workers");rwUntil(&B.Started,2,"both borrowed workers occupied");for(unsigned N=0;N<32;++N)testRequire(xrtExecutorSubmit(E,rwHoldPool,&B,NULL,NULL),"fill both borrowed native queues");rwAdd(&F,XRT_TIME_UNIX_EPOCH,XRT_TIME_UNIX_EPOCH);double End=xrtTimer()+10;while(rwStats(&F).Unsent!=1){testRequire(xrtTimer()<End,"readable no-effect rejection enters retry heap");xrtThreadYield();}rwwaiter W={.F=&F,.Timeout=10000};xthread* T=rwSpawn(&W);testRequire(xcronServiceClose(F.Service,true,NULL),"cancel unsent readable lease");rwJoin(T);testRequire(W.Result==XWAIT_OK && !xrtAtomic64Load(&F.Calls,XMEMORY_ACQUIRE),"unsent cancellation is readable without business callback");xcronservicecompletion D;testRequire(rwDrain(&F,&D)==1 && D.Result==XCRON_RUN_CANCELLED && D.Code==XCRON_COMPLETION_CANCELLED && D.Attempts==1 && !D.Error,"readable unsent receipt retains its original attempt duty");xrtAtomic32Store(&B.Gate,1,XMEMORY_RELEASE);rwClear(&F,E);
}
static void testServiceReadable(bool Selected)
{testRWReadiness(Selected);testRWCloseAndErrors(Selected);testRWTimeoutAndWarm(Selected);testRWCompetingConsumers(Selected);testRWUnsent(Selected);puts("[PASS] completion readability: nonconsuming broadcast, physical/unsent tails/errors/EOF, self-waits, total timeout,512 competing receipts and512 no-heap cycles");}
#endif
