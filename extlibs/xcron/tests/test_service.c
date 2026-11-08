#include "test.h"
#include <xrt/atomic.h>
#include <xrt/thread.h>
#include <xrt/memory.h>
#include <limits.h>
#include "tzif_fixture.h"
typedef struct servicefixture {
    xcronservice* Service;
    xatomic64 Wall,Mono,Calls;
    xatomic32 HoldClock,ClockEntered,FailClock,ExecuteGate,Started,Probes,ProbeDone;
    xcronrequestid ProbeRequest;
    int Mode;
} servicefixture;
static void serviceUntil(const xatomic32* A,uint32 V,cstr Text)
{double End=xrtTimer()+10;while(xrtAtomic32Load(A,XMEMORY_ACQUIRE)!=V){testRequire(xrtTimer()<End,Text);xrtThreadYield();}}
static bool serviceClock(xcronclock* C,ptr Raw)
{
    servicefixture* F=Raw;xrtAtomic32Store(&F->ClockEntered,1,XMEMORY_RELEASE);serviceUntil(&F->HoldClock,0,"release explicit clock");
    if(xrtAtomic32Load(&F->FailClock,XMEMORY_ACQUIRE)){xrtSetErrorInfo(XERR_IO,"test.clock",71,"clock source failed");return false;}
    *C=(xcronclock){(xtime)xrtAtomic64Load(&F->Wall,XMEMORY_ACQUIRE),xrtAtomic64Load(&F->Mono,XMEMORY_ACQUIRE)};
    if(xrtAtomic32Exchange(&F->Probes,0,XMEMORY_ACQ_REL)){
        xcronserviceissue I;xcronreply R;testRequire(xcronServiceReapAsync(F->Service,&F->ProbeRequest,NULL),"owner admits asynchronous command");
        testRequire(xcronServiceCollect(F->Service,F->ProbeRequest,10,&R,&I)==XWAIT_ERROR && I.Code==XCRON_SERVICE_THREAD,"owner pending collect cannot self-wait");xrtClearError();
        testRequire(xcronServiceCollect(F->Service,F->ProbeRequest,0,&R,NULL)==XWAIT_TIMEOUT,"owner nonblocking collect allowed");
        testRequire(xcronServiceWaitFor(F->Service,10,&I)==XWAIT_ERROR && I.Code==XCRON_SERVICE_THREAD,"owner join rejected");xrtClearError();xrtAtomic32Store(&F->ProbeDone,1,XMEMORY_RELEASE);
    }
    return true;
}
static xcronrunresult serviceProc(const xcronrun* R,const xcronexecution* Token,ptr Raw)
{
    servicefixture* F=Raw;testRequire(R->Data==F,"business data identity");xrtAtomic64FetchAdd(&F->Calls,1,XMEMORY_RELAXED);xrtAtomic32Store(&F->Started,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&F->ExecuteGate,XMEMORY_ACQUIRE)){if(xcronExecutionCancelled(Token))return XCRON_RUN_CANCELLED;xrtThreadYield();}
    if(F->Mode==1){xrtSetErrorInfo(XERR_IO,"test.service.body",19,"business failure");return XCRON_RUN_FAILED;}
    return F->Mode==2?(xcronrunresult)99:XCRON_RUN_SUCCEEDED;
}
static servicefixture serviceFixture(void)
{servicefixture F={0};xrtAtomic64Init(&F.Wall,(uint64)XRT_TIME_UNIX_EPOCH);xrtAtomic64Init(&F.Mono,100);xrtAtomic32Init(&F.ExecuteGate,1);return F;}
static xexecutor* servicePool(uint32 Workers,uint32 Queue)
{xexecutorconfig C={Workers,Queue,0};xexecutor* E=xrtExecutorCreate(&C);testRequire(E!=NULL,"service borrowed pool");return E;}
static void servicePoolEnd(xexecutor* E)
{testRequire(xrtExecutorClose(E) && xrtExecutorWaitFor(E,10000)==XWAIT_OK && xrtExecutorDestroy(E),"host joins borrowed pool");}
static xcronserviceconfig serviceConfig(xexecutor* E,servicefixture* F)
{return (xcronserviceconfig){.Scheduler={8,4,32,0,65536},.Executor=E,.Proc=serviceProc,.Context=F,.Clock=serviceClock,.ClockContext=F,.CommandLimit=8,.CompletionLimit=4,.BatchLimit=4,.CommandWorkLimit=8,.ResampleMilliseconds=5,.RetryIntervalMilliseconds=10,.RetryAttempts=3,.AdmissionMilliseconds=100,.BytesLimit=262144,.AdapterBytesLimit=65536};}
static void serviceStart(servicefixture* F,const xcronserviceconfig* C)
{testRequire(xcronServiceCreate(C,&F->Service,NULL),"live service create");serviceUntil(&F->ClockEntered,1,"owner samples clock");}
static xcronzone* serviceZone(void)
{xcronzone* Z=NULL;testRequire(xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_TZIF3,&Z,NULL),"service UTC zone");return Z;}
static xcronjobconfig serviceJob(xcronzone* Z,servicefixture* F,xtime Begin,xtime End)
{xcronjobconfig C={.Zone=Z,.Begin=Begin,.End=End,.RunningLimit=1,.Data=F};testRequire(xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&C.Expression,NULL),"service expression");return C;}
static xcronreply serviceReply(xcronservice* S,xcronrequestid Id)
{xcronreply R;testRequire(xcronServiceCollect(S,Id,10000,&R,NULL)==XWAIT_OK,"collect accepted command");return R;}
static xcronjobid serviceAdd(xcronservice* S,const xcronjobconfig* C)
{xcronrequestid Q;testRequire(xcronServiceAddAsync(S,C,&Q,NULL),"admit Add");xcronreply R=serviceReply(S,Q);testRequire(R.Code==XCRON_SERVICE_OK && R.Job && !R.Error && !R.Data,"successful Add reply");return R.Job;}
static xcronservicestats serviceStats(xcronservice* S)
{xcronservicestats T;testRequire(xcronServiceInspect(S,&T,NULL),"service inspect");return T;}
static xcronservicestats serviceReady(xcronservice* S,uint32 Ready,uint32 Unsent)
{double End=xrtTimer()+10;for(;;){xcronservicestats T=serviceStats(S);if(T.ReadyCompletions==Ready && T.Unsent==Unsent)return T;testRequire(T.State!=XCRON_SERVICE_FAILED && xrtTimer()<End,"bounded output/retry progress");xrtThreadYield();}}
static xcronservicecompletion serviceDone(xcronservice* S)
{double End=xrtTimer()+10;for(;;){xcronservicecompletion D;size_t N;testRequire(xcronServiceDrain(S,&D,1,&N,NULL),"service metadata drain");if(N)return D;testRequire(xrtTimer()<End,"physical service receipt");xrtThreadYield();}}
static void serviceStop(servicefixture* F)
{testRequire(xcronServiceClose(F->Service,true,NULL) && xcronServiceWaitFor(F->Service,10000,NULL)==XWAIT_OK,"stop and physically join owner");}
static void serviceClear(servicefixture* F)
{
    xcronservicecompletion D;size_t N;do{testRequire(xcronServiceDrain(F->Service,&D,1,&N,NULL),"drain final metadata");if(N)xrtErrorFree(D.Error);}while(N);
    xcronretired R;bool Found;do{testRequire(xcronServiceReapStopped(F->Service,&R,&Found,NULL),"post-stop retirement");if(Found)testRequire(R.Data==F,"returned original data");}while(Found);
    testRequire(xcronServiceDestroy(F->Service,NULL),"destroy duty-free service");F->Service=NULL;
}
#include "service_budget_cases.h"
static void testServiceResourceBytes(void)
{
    servicefixture F=serviceFixture();xrtAtomic32Store(&F.HoldClock,1,XMEMORY_RELEASE);
    xexecutor* E=servicePool(1,8);xcronserviceconfig C=serviceConfig(E,&F);
    xallocationbudget* B=NULL;testRequire(xrtAllocationBudgetCreate(SIZE_MAX,&B),"resource snapshot allocation budget");C.InitializationBudget=B;serviceStart(&F,&C);
    serviceUntil(&F.ClockEntered,1,"owner held before first clock sample");
    xcronservicestats T=serviceStats(F.Service);xallocationbudgetinfo Before={.Size=sizeof(Before)},After={.Size=sizeof(After)};
    testRequire(xrtAllocationBudgetInfo(B,&Before),"physical startup charge");
    printf("[resource] before-clock bytes=%zu arena=%zu kernel=%zu adapter=%zu charged=%zu\n",T.Bytes,T.ArenaBytes,T.KernelBytes,T.AdapterBytes,Before.LiveBytes);
    testRequire(T.KernelBytes && T.AdapterBytes && T.Executor.Bytes==T.AdapterBytes && T.Bytes==T.ArenaBytes+T.KernelBytes+T.AdapterBytes && Before.LiveBytes>=T.Bytes,"all allocated components visible before the first clock callback returns");
    size_t Adapter=T.AdapterBytes,Arena=T.ArenaBytes,Kernel=T.KernelBytes;
    xrtAtomic32Store(&F.HoldClock,0,XMEMORY_RELEASE);serviceStop(&F);T=serviceStats(F.Service);
    testRequire(xrtAllocationBudgetInfo(B,&After),"physical stopped charge");
    printf("[resource] stopped bytes=%zu adapter=%zu charged=%zu dropped=%zu\n",T.Bytes,T.AdapterBytes,After.LiveBytes,Before.LiveBytes-After.LiveBytes);
    testRequire(!T.AdapterBytes && !T.Executor.Bytes && T.Bytes==Arena+Kernel && !T.Executor.Active && !T.Executor.Producers && T.Executor.Closed,"stopped snapshot excludes physically freed adapter and retains closed history");
    testRequire(Before.LiveBytes>=After.LiveBytes && Before.LiveBytes-After.LiveBytes>=Adapter && After.LiveBytes>=T.Bytes,"public byte counters agree with physical allocation-budget release");
    serviceClear(&F);testRequire(xrtAllocationBudgetInfo(B,&After) && !After.LiveBytes && !After.LiveCount,"destroy returns arena, kernel and native thread charges");xrtAllocationBudgetFree(B);servicePoolEnd(E);
}
static void testServiceCredits(void)
{
    servicefixture F=serviceFixture();xexecutor* E=servicePool(1,8);xcronserviceconfig C=serviceConfig(E,&F);C.CompletionLimit=C.BatchLimit=1;serviceStart(&F,&C);
    xcronservicestats Backend=serviceStats(F.Service);
    testRequire(Backend.Waiter.StorageBytes==sizeof(xeventwaiter) && Backend.ActiveNativeTimers==Backend.Waiter.NativeTimers,"owner-bound waiter backend and inline storage reported");
    xcronzone* Z=serviceZone();xcronjobconfig J=serviceJob(Z,&F,XRT_TIME_UNIX_EPOCH,XRT_TIME_UNIX_EPOCH+3000);xrtAtomic64Store(&F.Wall,(uint64)J.End,XMEMORY_RELEASE);xcronjobid Job=serviceAdd(F.Service,&J);xcronZoneRelease(Z);
    serviceReady(F.Service,1,0);xrtSleep(15);xcronservicestats T=serviceStats(F.Service);testRequire(T.ReservedCompletions==1 && T.Kernel.Dispatched==1 && xrtAtomic64Load(&F.Calls,XMEMORY_RELAXED)==1,"full completion output stops further dispatch");
    xcronserviceissue I;testRequire(!xcronServiceDestroy(F.Service,&I) && I.Code==XCRON_SERVICE_BUSY,"busy destroy preserves duties");xrtClearError();
    serviceStop(&F);T=serviceStats(F.Service);testRequire(T.State==XCRON_SERVICE_STOPPED && T.Kernel.Completed==1 && T.Kernel.Jobs==1 && !T.ActiveNativeTimers,"close joins before consuming results or retirement");
    xcronretired R;bool Found=false;testRequire(xcronServiceReapStopped(F.Service,&R,&Found,NULL) && Found && R.Id==Job && R.Data==&F && R.Info.Succeeded==1,"retirement receipt precedes metadata consumption");
    xcronservicecompletion D=serviceDone(F.Service);testRequire(D.Job==Job && D.Result==XCRON_RUN_SUCCEEDED && !D.Error && D.Attempts==1,"metadata remains valid after retirement");
    testRequire(xcronServiceDestroy(F.Service,NULL),"all reserved duties returned");servicePoolEnd(E);
}
static void testServiceCommands(void)
{
    servicefixture F=serviceFixture();xrtAtomic32Store(&F.HoldClock,1,XMEMORY_RELEASE);xexecutor* E=servicePool(1,8);xcronserviceconfig C=serviceConfig(E,&F);C.CommandLimit=2;serviceStart(&F,&C);
    xcronzone* Z=serviceZone();xcronjobconfig J=serviceJob(Z,&F,XRT_TIME_UNIX_EPOCH,XRT_TIME_UNIX_EPOCH);xcronrequestid A,B,Sentinel=77;
    testRequire(xcronServiceAddAsync(F.Service,&J,&A,NULL) && xcronServiceAddAsync(F.Service,&J,&B,NULL),"queued config copies and zone retains");xcronZoneRelease(Z);
    xcronserviceissue I;testRequire(!xcronServiceReapAsync(F.Service,&Sentinel,&I) && Sentinel==77 && I.Code==XCRON_SERVICE_LIMIT,"uncollected bounded tickets");xrtClearError();
    xcronreply R,Before;memset(&R,0xa5,sizeof(R));Before=R;xrtSetErrorInfo(XERR_STATE,"caller.ambient",3,"timeout preserves");const xerror* Ambient=xrtGetError();double Start=xrtTimer();
    testRequire(xcronServiceCollect(F.Service,A,20,&R,NULL)==XWAIT_TIMEOUT && !memcmp(&R,&Before,sizeof(R)) && xrtGetError()==Ambient && xrtTimer()-Start<1,"one timeout budget preserves output/data duty/ambient");xrtClearError();
    testRequire(xcronServiceClose(F.Service,false,NULL),"close with queued Adds");xrtAtomic32Store(&F.HoldClock,0,XMEMORY_RELEASE);testRequire(xcronServiceWaitFor(F.Service,10000,NULL)==XWAIT_OK,"join with outstanding replies");
    R=serviceReply(F.Service,A);testRequire(R.Code==XCRON_SERVICE_CLOSED && R.Data==&F && R.Error,"closed Add returns data duty");xrtErrorFree(R.Error);R=serviceReply(F.Service,B);testRequire(R.Code==XCRON_SERVICE_CLOSED && R.Data==&F,"second closed Add returns data duty");xrtErrorFree(R.Error);
    R=Before;testRequire(xcronServiceCollect(F.Service,A,0,&R,&I)==XWAIT_ERROR && I.Code==XCRON_SERVICE_STALE && !memcmp(&R,&Before,sizeof(R)),"single-consumer generation ticket");xrtClearError();serviceClear(&F);servicePoolEnd(E);
    F=serviceFixture();E=servicePool(1,8);C=serviceConfig(E,&F);C.CommandLimit=1;serviceStart(&F,&C);Z=serviceZone();J=serviceJob(Z,&F,0,1);memset(&J.Expression,0,sizeof(J.Expression));testRequire(xcronServiceAddAsync(F.Service,&J,&A,NULL),"expression calculation error is an asynchronous reply");R=serviceReply(F.Service,A);
    testRequire(R.Code==XCRON_SERVICE_KERNEL && R.Scheduler.Code==XCRON_SCHEDULER_CALCULATION && R.Data==&F && !R.Job && R.Error,"failed Add returns typed issue and borrowed data after owner finished");xrtErrorFree(R.Error);
    testRequire(xcronServiceReapAsync(F.Service,&B,NULL) && A!=B,"ticket generation changes at slot reuse");R=serviceReply(F.Service,B);testRequire(!R.Found && R.Code==XCRON_SERVICE_OK,"empty asynchronous retirement");xcronZoneRelease(Z);serviceStop(&F);serviceClear(&F);servicePoolEnd(E);
}
static void serviceBlock(ptr Raw)
{servicefixture* F=Raw;xrtAtomic32Store(&F->Started,1,XMEMORY_RELEASE);serviceUntil(&F->ExecuteGate,1,"release unrelated borrowed work");}
static void serviceNoop(ptr Raw)
{servicefixture* F=Raw;xrtAtomic32Store(&F->ProbeDone,1,XMEMORY_RELEASE);}
static void testServiceRetries(void)
{
    for(unsigned Mode=0;Mode<4;++Mode){servicefixture F=serviceFixture(),Block=serviceFixture();xrtAtomic32Store(&Block.ExecuteGate,0,XMEMORY_RELAXED);xexecutor* E=servicePool(1,1);
        testRequire(xrtExecutorSubmit(E,serviceBlock,&Block,NULL,NULL),"shared worker occupied");serviceUntil(&Block.Started,1,"unrelated work running");testRequire(xrtExecutorSubmit(E,serviceNoop,&Block,NULL,NULL),"shared native queue full");
        xcronserviceconfig C=serviceConfig(E,&F);serviceStart(&F,&C);xcronzone* Z=serviceZone();xcronjobconfig J=serviceJob(Z,&F,XRT_TIME_UNIX_EPOCH,XRT_TIME_UNIX_EPOCH);serviceAdd(F.Service,&J);xcronZoneRelease(Z);serviceReady(F.Service,0,1);
        if(Mode==0){xrtAtomic64Store(&F.Mono,110,XMEMORY_RELEASE);xcronServiceWake(F.Service,NULL);double End=xrtTimer()+10;while(serviceStats(F.Service).SubmitAttempts<2){testRequire(xrtTimer()<End,"retry attempt two");xrtThreadYield();}xrtAtomic64Store(&F.Mono,120,XMEMORY_RELEASE);xcronServiceWake(F.Service,NULL);}
        if(Mode==1){xrtAtomic64Store(&F.Mono,200,XMEMORY_RELEASE);xcronServiceWake(F.Service,NULL);}
        if(Mode==2){xrtAtomic32Store(&Block.ExecuteGate,1,XMEMORY_RELEASE);serviceUntil(&Block.ProbeDone,1,"queue frees before retry");xrtAtomic64Store(&F.Mono,110,XMEMORY_RELEASE);xcronServiceWake(F.Service,NULL);}
        if(Mode==3)serviceStop(&F);
        xcronservicecompletion D=serviceDone(F.Service);xcronservicestats T=serviceStats(F.Service);
        if(Mode<2){testRequire(D.Code==XCRON_COMPLETION_EXPIRED && D.Result==XCRON_RUN_FAILED && D.Error && D.Attempts==(Mode?1u:3u),"attempt/elapsed admission expiry is explicit and no-effect");testRequire(!xrtAtomic64Load(&F.Calls,XMEMORY_RELAXED),"overloaded lease never ran");}
        if(Mode==2)testRequire(D.Code==XCRON_COMPLETION_NORMAL && D.Result==XCRON_RUN_SUCCEEDED && D.Attempts==2 && !D.Error && xrtAtomic64Load(&F.Calls,XMEMORY_RELAXED)==1,"only no-effect overload retries once capacity returns");
        if(Mode==3)testRequire(D.Code==XCRON_COMPLETION_CANCELLED && D.Result==XCRON_RUN_CANCELLED && D.Attempts==1 && !D.Error,"close returns unsent lease without user body");
        testRequire(T.ReservedCompletions<=C.CompletionLimit,"retry respects output credit");xrtErrorFree(D.Error);if(Mode!=3)serviceStop(&F);serviceClear(&F);xrtAtomic32Store(&Block.ExecuteGate,1,XMEMORY_RELEASE);servicePoolEnd(E);
    }
}
static void serviceUnrelatedProbe(ptr Raw)
{
    servicefixture* F=Raw;xcronserviceissue I;xcronreply R;testRequire(xcronServiceReapAsync(F->Service,&F->ProbeRequest,NULL),"unrelated pool callback can queue");
    testRequire(xcronServiceCollect(F->Service,F->ProbeRequest,100,&R,&I)==XWAIT_ERROR && I.Code==XCRON_SERVICE_THREAD,"ANY borrowed-executor callback guarded");xrtClearError();
    testRequire(xcronServiceWaitFor(F->Service,100,&I)==XWAIT_ERROR && I.Code==XCRON_SERVICE_THREAD,"unrelated pool callback cannot join service");xrtClearError();
    testRequire(xcronServiceCollect(F->Service,F->ProbeRequest,0,&R,NULL)==XWAIT_TIMEOUT,"pool nonblocking probe allowed");xrtAtomic32Store(&F->ProbeDone,1,XMEMORY_RELEASE);
}
static void testServiceReentryAndClock(void)
{
    servicefixture F=serviceFixture();xrtAtomic32Store(&F.Probes,1,XMEMORY_RELAXED);xexecutor* E=servicePool(1,8);xcronserviceconfig C=serviceConfig(E,&F);serviceStart(&F,&C);serviceUntil(&F.ProbeDone,1,"clock owner reentry probes");xcronreply R=serviceReply(F.Service,F.ProbeRequest);testRequire(!R.Error && !R.Found,"owner deferred command eventually returned");serviceStop(&F);serviceClear(&F);servicePoolEnd(E);
    F=serviceFixture();xrtAtomic32Store(&F.HoldClock,1,XMEMORY_RELAXED);E=servicePool(1,8);C=serviceConfig(E,&F);serviceStart(&F,&C);testRequire(xrtExecutorSubmit(E,serviceUnrelatedProbe,&F,NULL,NULL),"unrelated executor self-wait probe");serviceUntil(&F.ProbeDone,1,"unrelated callback returned without deadlock");xrtAtomic32Store(&F.HoldClock,0,XMEMORY_RELEASE);R=serviceReply(F.Service,F.ProbeRequest);testRequire(!R.Error,"nonblocking accepted request retained");serviceStop(&F);serviceClear(&F);servicePoolEnd(E);
    for(unsigned Mode=0;Mode<2;++Mode){F=serviceFixture();E=servicePool(1,8);C=serviceConfig(E,&F);serviceStart(&F,&C);double End=xrtTimer()+10;while(!serviceStats(F.Service).LastClock.MonotonicMilliseconds){testRequire(xrtTimer()<End,"first valid sample published");xrtThreadYield();}
        if(Mode)xrtAtomic32Store(&F.FailClock,1,XMEMORY_RELEASE);else xrtAtomic64Store(&F.Mono,99,XMEMORY_RELEASE);xcronServiceWake(F.Service,NULL);
        testRequire(xcronServiceWaitFor(F.Service,10000,NULL)==XWAIT_OK,"clock failure controlled physical shutdown");xcronservicestats T=serviceStats(F.Service);testRequire(T.State==XCRON_SERVICE_FAILED && T.Failure.Code==XCRON_SERVICE_CLOCK,"typed sticky clock failure");xerror* Error=NULL;testRequire(xcronServiceFailure(F.Service,&Error,NULL) && Error && xrtErrorFind(Error,"xrt.cron.service",XCRON_SERVICE_CLOCK),"owned fatal diagnostic");if(Mode)testRequire(xrtErrorFind(Error,"test.clock",71)!=NULL,"clock source cause retained");xrtErrorFree(Error);serviceClear(&F);servicePoolEnd(E);
    }
}
static void testServicePhysicalResults(void)
{
    for(unsigned Mode=0;Mode<3;++Mode){servicefixture F=serviceFixture();F.Mode=(int)Mode;xexecutor* E=servicePool(1,8);xcronserviceconfig C=serviceConfig(E,&F);serviceStart(&F,&C);xcronzone* Z=serviceZone();xcronjobconfig J=serviceJob(Z,&F,XRT_TIME_UNIX_EPOCH,XRT_TIME_UNIX_EPOCH);serviceAdd(F.Service,&J);xcronZoneRelease(Z);xrtSetErrorInfo(XERR_STATE,"owner.sentinel",2,"owner isolation");const xerror* Ambient=xrtGetError();xcronservicecompletion D=serviceDone(F.Service);testRequire(xrtGetError()==Ambient,"owner does not borrow worker error");
        if(!Mode)testRequire(D.Result==XCRON_RUN_SUCCEEDED && !D.Error,"normal physical success");
        if(Mode==1)testRequire(D.Result==XCRON_RUN_FAILED && xrtErrorFind(D.Error,"test.service.body",19),"worker error transferred");
        if(Mode==2)testRequire(D.Result==XCRON_RUN_FAILED && D.Executor==XCRON_EXECUTOR_RESULT && !D.Error,"invalid callback result typed");
        xrtErrorFree(D.Error);xrtClearError();serviceStop(&F);serviceClear(&F);servicePoolEnd(E);
    }
    servicefixture F=serviceFixture();xrtAtomic32Store(&F.ExecuteGate,0,XMEMORY_RELAXED);xexecutor* E=servicePool(1,8);xcronserviceconfig C=serviceConfig(E,&F);serviceStart(&F,&C);xcronzone* Z=serviceZone();xcronjobconfig J=serviceJob(Z,&F,XRT_TIME_UNIX_EPOCH,XRT_TIME_UNIX_EPOCH);xcronjobid Job=serviceAdd(F.Service,&J);xcronZoneRelease(Z);serviceUntil(&F.Started,1,"cooperative callback started");xcronrequestid Q;testRequire(xcronServiceCancelAsync(F.Service,Job,true,&Q,NULL),"request future and running cancellation");xcronreply R=serviceReply(F.Service,Q);testRequire(!R.Code && !R.Error,"owner applied cancellation");xcronservicecompletion D=serviceDone(F.Service);testRequire(D.Result==XCRON_RUN_CANCELLED,"physical cooperative tail ended");serviceStop(&F);serviceClear(&F);servicePoolEnd(E);
}
typedef struct servicereader {xcronservice* S;xcronjobid Job;xcronrequestid Ticket;xwaitresult Result;xcronserviceerror Code;} servicereader;
static int32 serviceCollectRace(ptr Raw)
{servicereader* A=Raw;xcronreply R;xcronserviceissue I={0};A->Result=xcronServiceCollect(A->S,A->Ticket,10000,&R,&I);A->Code=I.Code;if(A->Result==XWAIT_OK)xrtErrorFree(R.Error);return 0;}
static int32 serviceReadMany(ptr Raw)
{servicereader* A=Raw;for(unsigned N=0;N<128;++N){xcronrequestid Q;testRequire(xcronServiceInfoAsync(A->S,A->Job,&Q,NULL),"concurrent bounded command");xcronreply R=serviceReply(A->S,Q);testRequire(!R.Code && R.Job==A->Job && R.Info.State==XCRON_JOB_SCHEDULED && !R.Error,"serialized owner snapshot under command concurrency");}return 0;}
static void testServiceConcurrency(void)
{
    servicefixture F=serviceFixture();xrtAtomic32Store(&F.HoldClock,1,XMEMORY_RELAXED);xexecutor* E=servicePool(4,16);xcronserviceconfig C=serviceConfig(E,&F);serviceStart(&F,&C);xcronrequestid Q;testRequire(xcronServiceInfoAsync(F.Service,999,&Q,NULL),"racing collect ticket");servicereader A[4]={{.S=F.Service,.Ticket=Q},{.S=F.Service,.Ticket=Q}};xthread* Threads[4];Threads[0]=xrtThreadCreate(serviceCollectRace,&A[0],0);Threads[1]=xrtThreadCreate(serviceCollectRace,&A[1],0);testRequire(Threads[0] && Threads[1],"collect contenders");xrtAtomic32Store(&F.HoldClock,0,XMEMORY_RELEASE);
    for(unsigned N=0;N<2;++N){testRequire(xrtThreadWaitFor(Threads[N],10000)==XWAIT_OK,"collect contender returns");xrtThreadDestroy(Threads[N]);}testRequire((A[0].Result==XWAIT_OK && A[1].Code==XCRON_SERVICE_STALE) || (A[1].Result==XWAIT_OK && A[0].Code==XCRON_SERVICE_STALE),"exactly one collector transfers reply");
    xcronzone* Z=serviceZone();xcronjobconfig J=serviceJob(Z,&F,XRT_TIME_UNIX_EPOCH+100000,XRT_TIME_UNIX_EPOCH+100000);xcronjobid Job=serviceAdd(F.Service,&J);xcronZoneRelease(Z);
    for(unsigned N=0;N<4;++N){A[N]=(servicereader){.S=F.Service,.Job=Job};Threads[N]=xrtThreadCreate(serviceReadMany,&A[N],0);testRequire(Threads[N]!=NULL,"parallel command reader");}
    for(unsigned N=0;N<4;++N){testRequire(xrtThreadWaitFor(Threads[N],10000)==XWAIT_OK,"parallel reader joined");xrtThreadDestroy(Threads[N]);}serviceStop(&F);serviceClear(&F);servicePoolEnd(E);
}
static void testServiceBudgetAndWarm(void)
{
    servicefixture F=serviceFixture();xexecutor* E=servicePool(1,8);xcronserviceconfig C=serviceConfig(E,&F);xallocationbudget* B=NULL;testRequire(xrtAllocationBudgetCreate(SIZE_MAX,&B),"startup probe quota");C.InitializationBudget=B;serviceStart(&F,&C);xcronservicestats T=serviceStats(F.Service);double End=xrtTimer()+10;while(!T.KernelBytes){testRequire(xrtTimer()<End,"startup byte snapshot");T=serviceStats(F.Service);xrtThreadYield();}
    xallocationbudgetinfo BI={.Size=sizeof(BI)};testRequire(xrtAllocationBudgetInfo(B,&BI),"startup quota accounting");size_t Total=BI.LiveBytes,Arena=T.ArenaBytes,Kernel=T.KernelBytes,Adapter=T.AdapterBytes;
    testRequire(T.Bytes==Arena+Kernel+Adapter && T.Bytes<=C.BytesLimit && Total>T.Bytes,"local arena plus independent component caps and thread charge");serviceStop(&F);serviceClear(&F);testRequire(xrtAllocationBudgetInfo(B,&BI) && !BI.LiveBytes && !BI.LiveCount,"startup quota released across owner cleanup");xrtAllocationBudgetFree(B);
    size_t Thread=Total-Arena-Kernel-Adapter;size_t Limits[]={1,Arena-1,Arena,Arena+Thread-1,Arena+Thread,Arena+Thread+Kernel-1,Arena+Thread+Kernel,Total-1,Total};
    for(size_t N=0;N<sizeof(Limits)/sizeof(*Limits);++N){F=serviceFixture();testRequire(xrtAllocationBudgetCreate(Limits[N],&B),"prefix allocation quota");C=serviceConfig(E,&F);C.InitializationBudget=B;xcronservice* Sentinel=(xcronservice*)(uintptr_t)1;xcronserviceissue I;bool OK=xcronServiceCreate(&C,&Sentinel,&I);
        if(OK){F.Service=Sentinel;serviceStop(&F);serviceClear(&F);}else testRequire(Sentinel==(xcronservice*)(uintptr_t)1 && (I.Code==XCRON_SERVICE_MEMORY || I.Code==XCRON_SERVICE_SYSTEM),"cold prefix failure preserves output and typed cause");xrtClearError();testRequire(xrtAllocationBudgetInfo(B,&BI) && !BI.LiveBytes && !BI.LiveCount,"every observed cold allocation prefix balances");xrtAllocationBudgetFree(B);
    }
    F=serviceFixture();C=serviceConfig(E,&F);serviceStart(&F,&C);xcronzone* Z=serviceZone();const unsigned Rounds=2048;xcronjobconfig J=serviceJob(Z,&F,XRT_TIME_UNIX_EPOCH,XRT_TIME_UNIX_EPOCH+(xtime)Rounds*1000);serviceAdd(F.Service,&J);xcronZoneRelease(Z);xcronservicecompletion D=serviceDone(F.Service);testRequire(!D.Error,"warm first physical dispatch");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(true);xrtMemStatsGet(&Before);
#endif
    for(unsigned N=1;N<=Rounds;++N){xrtAtomic64Store(&F.Wall,(uint64)(XRT_TIME_UNIX_EPOCH+(xtime)N*1000),XMEMORY_RELEASE);xrtAtomic64Store(&F.Mono,100+(uint64)N*1000,XMEMORY_RELEASE);xcronServiceWake(F.Service,NULL);D=serviceDone(F.Service);testRequire(D.ScheduledAt==XRT_TIME_UNIX_EPOCH+(xtime)N*1000 && D.Result==XCRON_RUN_SUCCEEDED && !D.Error,"warm ordered run/slot generation reuse");}
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);testRequire(Before.MallocCalls==After.MallocCalls && Before.CallocCalls==After.CallocCalls && Before.ReallocCalls==After.ReallocCalls && Before.BlockAllocCalls==After.BlockAllocCalls,"warmed live owner/worker/requested-heap allocations are zero");
#endif
    serviceStop(&F);serviceClear(&F);servicePoolEnd(E);
}
static void testServiceRealClock(void)
{
    servicefixture F=serviceFixture();xexecutor* E=servicePool(1,8);xcronserviceconfig C=serviceConfig(E,&F);C.Clock=NULL;testRequire(xcronServiceCreate(&C,&F.Service,NULL),"system wall/monotonic live service");xcronzone* Z=serviceZone();xtime Now=xrtNow(),Next=Now-Now%1000+1000;xcronjobconfig J=serviceJob(Z,&F,Next,Next);serviceAdd(F.Service,&J);xcronZoneRelease(Z);xcronservicecompletion D=serviceDone(F.Service);testRequire(D.ScheduledAt==Next && D.Observed.Wall>=Next && D.Result==XCRON_RUN_SUCCEEDED && !D.Error,"real wall deadline fires exactly once");serviceStop(&F);serviceClear(&F);servicePoolEnd(E);
}
static void testServiceWallJumps(void)
{
    /* Forward jumps pass the exact kernel misfire policy; backward wall time
     * never rewinds a consumed actual occurrence. Monotonic time stays fixed. */
    for(unsigned Mode=0;Mode<3;++Mode){servicefixture F=serviceFixture();xexecutor* E=servicePool(2,8);xcronserviceconfig C=serviceConfig(E,&F);serviceStart(&F,&C);xcronzone* Z=serviceZone();
        xcronjobconfig J=serviceJob(Z,&F,XRT_TIME_UNIX_EPOCH,XRT_TIME_UNIX_EPOCH+3000);J.Misfire=(xcronmisfire)Mode;xcronjobid Job=serviceAdd(F.Service,&J);xcronZoneRelease(Z);xcronservicecompletion D=serviceDone(F.Service);testRequire(D.ScheduledAt==J.Begin,"first actual occurrence before jump");
        xrtAtomic64Store(&F.Wall,(uint64)J.End,XMEMORY_RELEASE);xcronServiceWake(F.Service,NULL);unsigned Count=Mode==0?3:1;
        for(unsigned N=0;N<Count;++N){D=serviceDone(F.Service);xtime Expected=Mode==0?J.Begin+(N+1)*1000:Mode==1?J.End:J.Begin+1000;
            testRequire(D.ScheduledAt==Expected && D.Result==XCRON_RUN_SUCCEEDED,"forward wall jump preserves catch-up/skip/coalesce selection");if(Mode==2)testRequire(D.Through==J.End && (D.Flags&XCRON_RUN_COALESCED),"coalescing range survives driver metadata");}
        xrtAtomic64Store(&F.Wall,(uint64)J.Begin,XMEMORY_RELEASE);xcronServiceWake(F.Service,NULL);xcronrequestid Q;testRequire(xcronServiceInfoAsync(F.Service,Job,&Q,NULL),"post-rollback info barrier");xcronreply R=serviceReply(F.Service,Q);testRequire(R.Info.State==XCRON_JOB_EXHAUSTED && R.Info.Dispatched==1+Count,"wall rollback never duplicates consumed work");serviceStop(&F);serviceClear(&F);servicePoolEnd(E);
    }
}
static void testServiceArguments(void)
{
    servicefixture F=serviceFixture();xexecutor* E=servicePool(1,8);xcronserviceconfig C=serviceConfig(E,&F),Bad=C;xcronserviceissue I={XCRON_SERVICE_STALE,XERR_IO};xcronservice* Out=(xcronservice*)(uintptr_t)1;
    Bad.Reserved=1;testRequire(!xcronServiceCreate(&Bad,&Out,&I) && Out==(xcronservice*)(uintptr_t)1 && I.Code==XCRON_SERVICE_STALE,"invalid values preserve outputs");xrtClearError();
    Bad=C;Bad.BytesLimit=1;testRequire(!xcronServiceCreate(&Bad,&Out,&I) && Out==(xcronservice*)(uintptr_t)1 && I.Code==XCRON_SERVICE_LIMIT,"combined hard byte reservation rejected before allocation");xrtClearError();
    Bad=C;Bad.Scheduler.BytesLimit=SIZE_MAX;testRequire(!xcronServiceCreate(&Bad,&Out,&I) && I.Code==XCRON_SERVICE_LIMIT,"component byte addition cannot wrap");xrtClearError();
    serviceStart(&F,&C);xcronservicestats S,Before;memset(&S,0xa5,sizeof(S));Before=S;
    testRequire(!xcronServiceInspect(F.Service,&S,(xcronserviceissue*)&S) && !memcmp(&S,&Before,sizeof(S)),"mutually aliased outputs preserved");xrtClearError();
    testRequire(!xcronServiceInspect(F.Service,(xcronservicestats*)F.Service,NULL),"private service arena cannot be overwritten");xrtClearError();
    xcronservicecompletion D;size_t N=71;testRequire(!xcronServiceDrain(F.Service,&D,SIZE_MAX,&N,NULL) && N==71,"output multiplication cannot wrap");xrtClearError();
    xcronretired R;bool Found=true;testRequire(!xcronServiceReapStopped(F.Service,&R,&Found,&I) && I.Code==XCRON_SERVICE_BUSY && Found,"running owner does not hand off its kernel");xrtClearError();
    testRequire(xcronServiceWaitFor(F.Service,0,NULL)==XWAIT_TIMEOUT,"nonblocking host join");serviceStop(&F);xcronrequestid Q=91;testRequire(!xcronServiceReapAsync(F.Service,&Q,&I) && I.Code==XCRON_SERVICE_CLOSED && Q==91,"closed queue rejects all new controls without duties");xrtClearError();serviceClear(&F);servicePoolEnd(E);
}
typedef struct servicecloserace {servicefixture* Fixture;xcronjobconfig Job;xatomic32 Accepted,Admitted,Finished;} servicecloserace;
static int32 serviceAddRace(ptr Raw)
{
    servicecloserace* Race=Raw;
    for(unsigned N=0;N<32;++N){xcronrequestid Q;xcronserviceissue I;
        if(!xcronServiceAddAsync(Race->Fixture->Service,&Race->Job,&Q,&I)){testRequire(I.Code==XCRON_SERVICE_CLOSED,"racing queue rejection consumes no data duty");xrtClearError();break;}
        xrtAtomic32FetchAdd(&Race->Accepted,1,XMEMORY_RELEASE);xcronreply R=serviceReply(Race->Fixture->Service,Q);
        if(!R.Code){testRequire(R.Job && !R.Data && !R.Error,"racing successful admission retains job duty");xrtAtomic32FetchAdd(&Race->Admitted,1,XMEMORY_RELEASE);}
        else {testRequire(R.Data==Race->Fixture && !R.Job && (R.Code==XCRON_SERVICE_CLOSED || (R.Code==XCRON_SERVICE_KERNEL && R.Scheduler.Code==XCRON_SCHEDULER_LIMIT)),"racing failed admission returns data exactly in reply");xrtErrorFree(R.Error);}
    }
    xrtAtomic32FetchAdd(&Race->Finished,1,XMEMORY_RELEASE);return 0;
}
static void testServiceCloseRace(void)
{
    for(unsigned Round=0;Round<8;++Round){servicefixture F=serviceFixture();xexecutor* E=servicePool(1,8);xcronserviceconfig C=serviceConfig(E,&F);serviceStart(&F,&C);xcronzone* Z=serviceZone();servicecloserace Race={.Fixture=&F,.Job=serviceJob(Z,&F,XRT_TIME_UNIX_EPOCH+100000,XRT_TIME_UNIX_EPOCH+100000)};xthread* T[4];
        for(unsigned N=0;N<4;++N){T[N]=xrtThreadCreate(serviceAddRace,&Race,0);testRequire(T[N]!=NULL,"racing command producer");}
        double Limit=xrtTimer()+10;while(xrtAtomic32Load(&Race.Accepted,XMEMORY_ACQUIRE)<4 && xrtAtomic32Load(&Race.Finished,XMEMORY_ACQUIRE)<4){testRequire(xrtTimer()<Limit,"racing command acceptance");xrtThreadYield();}
        serviceStop(&F);for(unsigned N=0;N<4;++N){testRequire(xrtThreadWaitFor(T[N],10000)==XWAIT_OK,"racing accepted commands remain collectible after stop");xrtThreadDestroy(T[N]);}xcronZoneRelease(Z);
        uint32 Retired=0;for(;;){xcronretired R;bool Found;testRequire(xcronServiceReapStopped(F.Service,&R,&Found,NULL),"racing job retirement");if(!Found)break;testRequire(R.Data==&F && !R.Info.Running,"racing retirement is physical data return");++Retired;}
        testRequire(Retired==xrtAtomic32Load(&Race.Admitted,XMEMORY_ACQUIRE) && !serviceStats(F.Service).ReadyReplies,"every successful racing Add has one matching retirement");serviceClear(&F);servicePoolEnd(E);
    }
}
static void testServiceFold(void)
{
    servicefixture F=serviceFixture();xexecutor* E=servicePool(2,8);xcronzone* Z=NULL;testRequire(xcronZoneCreatePosix(XRT_STR_LITERAL("EST5EDT,M3.2.0,M11.1.0"),XCRON_ZONE_TZIF3,&Z,NULL),"live fold zone");xdatetime Civil={.Year=2024,.Month=11,.Day=3,.Hour=1,.Minute=30};xtime At[2];size_t Count;
    testRequire(xcronZoneResolve(Z,&Civil,At,2,&Count,NULL)==XCRON_ZONE_FOUND && Count==2,"two actual fold occurrences");xrtAtomic64Store(&F.Wall,(uint64)At[1],XMEMORY_RELAXED);xcronserviceconfig C=serviceConfig(E,&F);serviceStart(&F,&C);xcronjobconfig J=serviceJob(Z,&F,At[0],At[1]);testRequire(xcronExpressionCompile(XRT_STR_LITERAL("30 1 * * *"),XCRON_UNIX5,&J.Expression,NULL),"fold expression");serviceAdd(F.Service,&J);xcronZoneRelease(Z);
    xcronservicecompletion A=serviceDone(F.Service),B=serviceDone(F.Service);testRequire(A.ScheduledAt==At[0] && B.ScheduledAt==At[1] && A.Result==XCRON_RUN_SUCCEEDED && B.Result==XCRON_RUN_SUCCEEDED,"live owner delivers every actual fold instance once");serviceStop(&F);serviceClear(&F);servicePoolEnd(E);
}
int main(void)
{
    testServiceBudgetedTurns();testServiceDeferredFailure();
    testServiceResourceBytes();testServiceCredits();testServiceCommands();testServiceRetries();testServiceReentryAndClock();testServicePhysicalResults();testServiceConcurrency();testServiceBudgetAndWarm();testServiceRealClock();testServiceWallJumps();testServiceArguments();testServiceCloseRace();testServiceFold();
    puts("[PASS] live cron service: reserved outputs/commands/data duties/retry bounds/self-wait/clock failure/physical cancellation/512 concurrent commands/OOM/2048 zero-heap cycles/real clock");return 0;
}
