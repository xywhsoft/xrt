#include "test.h"
#include <xrt/atomic.h>
#include <xrt/executor.h>
#include <xrt/thread.h>
typedef struct execcontext {
    xcronexecutor* Adapter;
    xatomic32 Started,Gate,Notified,NotifyGate,NotifyEntered,Reentry;
    xatomic64 Calls;
    int Mode;
    bool Block,BlockNotify,Probe,ProbeNotify;
} execcontext;
static void execUntil(const xatomic32* Value,uint32 Minimum,cstr Message)
{double End=xrtTimer()+10;while(xrtAtomic32Load(Value,XMEMORY_ACQUIRE)<Minimum){testRequire(xrtTimer()<End,Message);xrtThreadYield();}}
static xexecutor* execPool(uint32 Threads,size_t Queue)
{xexecutorconfig C={Threads,Queue,0};xexecutor* E=xrtExecutorCreate(&C);testRequire(E!=NULL,"execution pool create");return E;}
static void execPoolEnd(xexecutor* E)
{testRequire(xrtExecutorClose(E) && xrtExecutorWaitFor(E,10000)==XWAIT_OK && xrtExecutorDestroy(E),"execution pool drain/join");}
static xcronrun execLease(uint64 Id,uint64 Job,ptr Data)
{return (xcronrun){.Id=Id,.Job=Job,.Data=Data,.ScheduledAt=0,.Through=0,.Observed={0,0}};}
static xcronrunresult execProc(const xcronrun* R,const xcronexecution* Token,ptr Raw)
{
    execcontext* C=Raw;testRequire(R->Id && Token,"worker borrowed lease/token");xrtAtomic64FetchAdd(&C->Calls,1,XMEMORY_RELAXED);
    xrtAtomic32FetchAdd(&C->Started,1,XMEMORY_RELEASE);
    if(C->Probe){xcronexecutorstats S;xcronexecutorissue I;testRequire(!xcronExecutorInspect(C->Adapter,&S,&I) && I.Code==XCRON_EXECUTOR_THREAD,"callback cannot enter owner APIs");xrtClearError();}
    if(C->Block)execUntil(&C->Gate,1,"release worker body");
    if(C->Mode==1){xrtSetErrorInfo(XERR_IO,"test.cron.worker",17,"specific worker failure");return XCRON_RUN_FAILED;}
    if(C->Mode==2)return (xcronrunresult)99;
    if(C->Mode==3)return xcronExecutionCancelled(Token)?XCRON_RUN_CANCELLED:XCRON_RUN_SUCCEEDED;
    return XCRON_RUN_SUCCEEDED;
}
static void execNotify(ptr Raw)
{
    execcontext* C=Raw;xrtAtomic32FetchAdd(&C->Notified,1,XMEMORY_RELEASE);xrtAtomic32Store(&C->NotifyEntered,1,XMEMORY_RELEASE);
    if(C->ProbeNotify){xcronexecutorstats S;xcronexecutorissue I;
        testRequire(!xcronExecutorInspect(C->Adapter,&S,&I) && I.Code==XCRON_EXECUTOR_REENTRY,"owner cancellation Notify reentry rejected");xrtClearError();xcronrun R=execLease(UINT64_MAX,1,NULL);
        testRequire(!xcronExecutorSubmitBatch(C->Adapter,&R,1,&I) && I.Code==XCRON_EXECUTOR_REENTRY,"batch owner Notify reentry rejected");xrtAtomic32FetchAdd(&C->Reentry,1,XMEMORY_RELEASE);xrtClearError();}
    if(C->BlockNotify)execUntil(&C->NotifyGate,1,"release notification tail");
}
static xcronexecutor* execAdapter(xexecutor* E,uint32 Limit,execcontext* C,bool Notify)
{xcronexecutor* B=NULL;xcronexecutorconfig Config={E,execProc,C,Notify?execNotify:NULL,C,Limit,0,SIZE_MAX};testRequire(xcronExecutorCreate(&Config,&B,NULL),"execution adapter create");C->Adapter=B;return B;}
static xcroncompletion execReceipt(xcronexecutor* B)
{xcroncompletion C;size_t N;double End=xrtTimer()+10;do{testRequire(xcronExecutorDrain(B,&C,1,&N,NULL),"execution drain");if(!N){testRequire(xrtTimer()<End,"bounded receipt wait");xrtThreadYield();}}while(!N);return C;}
static void execAdapterEnd(xcronexecutor* B)
{xcronexecutorstats S;double End=xrtTimer()+10;do{testRequire(xcronExecutorInspect(B,&S,NULL) && !S.Active,"adapter has no undrained receipts");if(S.Producers){testRequire(xrtTimer()<End,"bounded publisher tail wait");xrtThreadYield();}}while(S.Producers);testRequire(xcronExecutorDestroy(B,NULL),"execution adapter destroy");}
static void execError(bool Value,xcronexecutorissue* I,xcronexecutorerror Code,cstr Message)
{testRequire(!Value && I->Code==Code && xrtErrorCode(xrtGetError())==(int32)Code && !strcmp(xrtErrorDomain(xrtGetError()),"xrt.cron.executor"),Message);xrtClearError();}
static void testExecResults(void)
{
    xexecutor* E=execPool(1,4);execcontext C={.Probe=true};xcronexecutor* B=execAdapter(E,2,&C,false);xcronexecutorissue I;
    for(unsigned Mode=0;Mode<3;++Mode){C.Mode=(int)Mode;xcronrun R=execLease(Mode+1,1,&C);xrtSetErrorInfo(XERR_STATE,"owner.sentinel",7,"retain owner");const xerror* Ambient=xrtGetError();
        testRequire(xcronExecutorSubmit(B,&R,NULL),"execute callback");xcroncompletion Done=execReceipt(B);testRequire(Done.Run.Id==R.Id && Done.Run.Data==&C && xrtGetError()==Ambient,"receipt identity and owner error isolation");
        if(Mode==0)testRequire(Done.Result==XCRON_RUN_SUCCEEDED && Done.Code==XCRON_EXECUTOR_OK && !Done.Error,"success receipt");
        if(Mode==1)testRequire(Done.Result==XCRON_RUN_FAILED && Done.Code==XCRON_EXECUTOR_OK && xrtErrorCode(Done.Error)==17 && !strcmp(xrtErrorDomain(Done.Error),"test.cron.worker"),"owned callback error crosses worker boundary");
        if(Mode==2)testRequire(Done.Result==XCRON_RUN_FAILED && Done.Code==XCRON_EXECUTOR_RESULT && !Done.Error,"invalid result does not borrow a previous worker error");
        xrtErrorFree(Done.Error);xrtClearError();
    }
    xcronrun R=execLease(4,1,NULL);C.Mode=0;C.Block=true;testRequire(xcronExecutorSubmit(B,&R,NULL),"active close fixture");execUntil(&C.Started,4,"active callback started");
    testRequire(xcronExecutorClose(B,true,NULL) && xcronExecutorClose(B,true,NULL),"close cancellation idempotence");xcronexecutorstats S;testRequire(xcronExecutorInspect(B,&S,NULL) && S.Active==1 && S.CancelRequests==1,"closed adapter retains active execution");
    execError(xcronExecutorDestroy(B,&I),&I,XCRON_EXECUTOR_BUSY,"busy destroy retains callback context");execError(xcronExecutorSubmit(B,&R,&I),&I,XCRON_EXECUTOR_CLOSED,"close rejects new submission");
    xrtAtomic32Store(&C.Gate,1,XMEMORY_RELEASE);xcroncompletion Done=execReceipt(B);testRequire(Done.Result==XCRON_RUN_SUCCEEDED,"late cancellation cannot rewrite successful physical work");
    execError(xcronExecutorCancel(B,R.Id,&I),&I,XCRON_EXECUTOR_STALE,"drained execution ID stale");execAdapterEnd(B);xexecutorstats Pool;testRequire(xrtExecutorGet(E,&Pool) && !Pool.Closed,"adapter close does not close borrowed executor");execPoolEnd(E);
}
static void execBlock(ptr Raw)
{execcontext* C=Raw;xrtAtomic32Store(&C->Started,1,XMEMORY_RELEASE);execUntil(&C->Gate,1,"external blocker released");}
static void testExecQueueAndCancel(void)
{
    xexecutor* E=execPool(1,1);execcontext Block={0},C={.ProbeNotify=true};testRequire(xrtExecutorSubmit(E,execBlock,&Block,NULL,NULL),"external shared-executor work");execUntil(&Block.Started,1,"shared blocker started");xcronexecutor* B=execAdapter(E,3,&C,true);
    xcronrun R=execLease(1,7,&C),Other=execLease(2,8,NULL);testRequire(xcronExecutorSubmit(B,&R,NULL),"queued bridge work admitted");xcronexecutorissue I;
    execError(xcronExecutorSubmit(B,&R,&I),&I,XCRON_EXECUTOR_DUPLICATE,"active duplicate rejected");
    execError(xcronExecutorSubmit(B,&Other,&I),&I,XCRON_EXECUTOR_REJECTED,"shared queue rejection transfers nothing");testRequire(I.Cause==XERR_AGAIN,"preserved rejection category");
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"queue-rejection diagnostic quota");
    testRequire(!xcronExecutorSubmit(B,&Other,&I) && I.Code==XCRON_EXECUTOR_REJECTED && I.Cause==XERR_AGAIN && xrtErrorKind(xrtGetError())==XERR_MEMORY,"typed rejection and ownership rollback survive diagnostic OOM");
    testRequire(xrtAllocationBudgetLeave(&Scope),"leave rejection quota");xrtClearError();xallocationbudgetinfo BudgetInfo={.Size=sizeof(BudgetInfo)};testRequire(xrtAllocationBudgetInfo(Budget,&BudgetInfo) && !BudgetInfo.LiveBytes && !BudgetInfo.LiveCount,"rejected allocation ownership balance");xrtAllocationBudgetFree(Budget);
    testRequire(xcronExecutorCancel(B,R.Id,NULL) && xcronExecutorCancel(B,R.Id,NULL),"queued cancellation idempotence");uint32 Requested=99;testRequire(xcronExecutorCancelJob(B,7,&Requested,NULL) && !Requested,"per-job cancel counts only new requests");
    /* Native cancellation executes queued-item Drop/Notify on this owner. */
    testRequire(xrtExecutorCancel(E),"borrowed executor cancels queued work");testRequire(xrtAtomic32Load(&C.Reentry,XMEMORY_ACQUIRE)==1 && !xrtAtomic64Load(&C.Calls,XMEMORY_RELAXED),"drop path has no user procedure and rejects owner reentry");
    xcroncompletion Done=execReceipt(B);testRequire(Done.Run.Id==R.Id && Done.Result==XCRON_RUN_CANCELLED && Done.Code==XCRON_EXECUTOR_OK && !Done.Error,"queued native cancellation has one receipt");
    xrtAtomic32Store(&Block.Gate,1,XMEMORY_RELEASE);execAdapterEnd(B);execPoolEnd(E);
    E=execPool(1,4);Block=(execcontext){0};C=(execcontext){0};testRequire(xrtExecutorSubmit(E,execBlock,&Block,NULL,NULL),"queued token blocker");execUntil(&Block.Started,1,"queued token blocker started");B=execAdapter(E,3,&C,false);
    for(unsigned J=0;J<3;++J){R=execLease(J+1,J<2?10:11,NULL);testRequire(xcronExecutorSubmit(B,&R,NULL),"queued token admission");}
    testRequire(xcronExecutorCancelJob(B,10,&Requested,NULL) && Requested==2,"job-wide token requests");testRequire(xcronExecutorClose(B,true,NULL),"close requests remaining token");xrtAtomic32Store(&Block.Gate,1,XMEMORY_RELEASE);
    for(unsigned J=0;J<3;++J){Done=execReceipt(B);testRequire(Done.Result==XCRON_RUN_CANCELLED && !Done.Error,"queued token suppresses callback");}
    testRequire(!xrtAtomic64Load(&C.Calls,XMEMORY_RELAXED),"queued cancelled callbacks never invoked");execAdapterEnd(B);execPoolEnd(E);
}
static void testExecNotifyTail(void)
{
    xexecutor* E=execPool(1,4);execcontext C={.BlockNotify=true};xcronexecutor* B=execAdapter(E,1,&C,true);xcronrun R=execLease(1,1,&C);testRequire(xcronExecutorSubmit(B,&R,NULL),"notification-tail fixture");execUntil(&C.NotifyEntered,1,"notification tail entered");
    xcroncompletion Done=execReceipt(B);testRequire(Done.Result==XCRON_RUN_SUCCEEDED,"physical user data is done before notification exits");xcronexecutorstats S;testRequire(xcronExecutorInspect(B,&S,NULL) && !S.Active && S.Producers==1,"publisher lifetime independent of receipt capacity");xcronexecutorissue I;execError(xcronExecutorDestroy(B,&I),&I,XCRON_EXECUTOR_BUSY,"notification context cannot be destroyed early");
    /* Reuse the only slot while its old Drop still uses adapter, not slot. */
    R.Id=2;testRequire(xcronExecutorSubmit(B,&R,NULL),"capacity released before notification tail ends");xrtAtomic32Store(&C.NotifyGate,1,XMEMORY_RELEASE);Done=execReceipt(B);testRequire(Done.Run.Id==2,"reused slot has independent completion");execAdapterEnd(B);execPoolEnd(E);
}
typedef struct execownerfixture {xexecutor* Pool;execcontext Callback;xatomic32 Finished;} execownerfixture;
static void execOwnerCreate(ptr Raw)
{execownerfixture* F=Raw;xcronexecutor* B=execAdapter(F->Pool,1,&F->Callback,true);xcronrun R=execLease(1,1,NULL);testRequire(xcronExecutorSubmit(B,&R,NULL),"worker-owned adapter admission");}
static void execOwnerFinish(ptr Raw)
{execownerfixture* F=Raw;xcroncompletion C=execReceipt(F->Callback.Adapter);testRequire(C.Result==XCRON_RUN_FAILED && C.Code==XCRON_EXECUTOR_OWNER_CONFLICT && !C.Error && !xrtAtomic64Load(&F->Callback.Calls,XMEMORY_RELAXED),"executor selection on owner refuses user callback");execAdapterEnd(F->Callback.Adapter);xrtAtomic32Store(&F->Finished,1,XMEMORY_RELEASE);}
static void testExecOwnerConflict(void)
{
    execownerfixture F={.Pool=execPool(1,4)};testRequire(xrtExecutorSubmit(F.Pool,execOwnerCreate,&F,NULL,NULL),"create adapter on real worker");execUntil(&F.Callback.Notified,1,"owner-conflict receipt published");
    xcronexecutorstats S;xcronexecutorissue I;execError(xcronExecutorInspect(F.Callback.Adapter,&S,&I),&I,XCRON_EXECUTOR_THREAD,"foreign controller cannot enter worker-owned adapter");
    testRequire(xrtExecutorSubmit(F.Pool,execOwnerFinish,&F,NULL,NULL),"return receipt to its real owner");execUntil(&F.Finished,1,"worker-owned adapter physically retired");execPoolEnd(F.Pool);
}
static uint64 ExecRandom=UINT64_C(0xaf924e7db168e53b);
static uint64 execRandom(void)
{ExecRandom^=ExecRandom<<13;ExecRandom^=ExecRandom>>7;ExecRandom^=ExecRandom<<17;return ExecRandom;}
static void testExecStress(void)
{
    xexecutor* E=execPool(4,128);execcontext C={.Mode=3};xcronexecutor* B=execAdapter(E,63,&C,false);uint64 Total=0;
    for(unsigned Round=0;Round<512;++Round){xcronrun Runs[63];bool Seen[63]={false};
        for(unsigned J=0;J<63;++J){uint64 Id=execRandom();if(!Id)Id=1;for(unsigned K=0;K<J;++K)testRequire(Id!=Runs[K].Id,"unique randomized lease fixture");Runs[J]=execLease(Id,1+J%7,&Runs[J]);testRequire(xcronExecutorSubmit(B,&Runs[J],NULL),"bounded many-worker admission");}
        for(unsigned J=0;J<63;J+=3)testRequire(xcronExecutorCancel(B,Runs[J].Id,NULL),"random cluster hash lookup after worker completion");
        unsigned Received=0;double End=xrtTimer()+10;
        while(Received<63){xcroncompletion D[7];size_t N;testRequire(xcronExecutorDrain(B,D,1+Round%7,&N,NULL),"bounded many-worker drain");if(!N){testRequire(xrtTimer()<End,"stress completion deadline");xrtThreadYield();}
            for(size_t I=0;I<N;++I){unsigned K;for(K=0;K<63 && D[I].Run.Id!=Runs[K].Id;++K){}testRequire(K<63 && !Seen[K] && D[I].Run.Data==&Runs[K] && D[I].Code==XCRON_EXECUTOR_OK && !D[I].Error && (D[I].Result==XCRON_RUN_SUCCEEDED || D[I].Result==XCRON_RUN_CANCELLED),"concurrent exactly-once ID/data/result receipt");Seen[K]=true;++Received;++Total;}
        }
    }
    xcronexecutorstats S;testRequire(Total==32256 && xcronExecutorInspect(B,&S,NULL) && S.Submitted==Total && S.Drained==Total && !S.Active && S.Peak<=63,"concurrent accounting and hash reuse");execAdapterEnd(B);execPoolEnd(E);
}
static void testExecKernel(void)
{
    xexecutor* E=execPool(2,8);execcontext C={.Block=true,.Mode=3};xcronexecutor* B=execAdapter(E,2,&C,false);xcronzone* Z=NULL;xcronexpression Expression;xcronscheduler* S=NULL;xcronschedulerconfig SC={1,2,32,0,65536};
    testRequire(xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_TZIF3,&Z,NULL) && xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&Expression,NULL) && xcronSchedulerCreate(&SC,&S,NULL),"kernel adapter integration fixture");
    int* Data=xrtMalloc(sizeof(*Data));testRequire(Data!=NULL,"owned job data");*Data=42;xcronjobconfig JC={.Expression=Expression,.Zone=Z,.Begin=0,.End=10000,.Overlap=XCRON_OVERLAP_ALLOW,.RunningLimit=2,.Data=Data};xcronjobid Id;testRequire(xcronSchedulerAdd(S,&JC,&Id,NULL),"real job admission");xcronZoneRelease(Z);
    xcronrun Runs[2];xcronpoll P;testRequire(xcronSchedulerPoll(S,(xcronclock){1000,0},Runs,2,&P,NULL) && P.Count==2,"real scheduler leases");for(unsigned J=0;J<2;++J)testRequire(xcronExecutorSubmit(B,&Runs[J],NULL),"real lease dispatch");execUntil(&C.Started,2,"both real task bodies running");
    testRequire(xcronSchedulerClose(S,NULL),"kernel cancellation with live executions");uint32 Requested;testRequire(xcronExecutorCancelJob(B,Id,&Requested,NULL) && Requested==2,"cooperative cancellation follows job close");xcronretired Retired;bool Found=true;testRequire(xcronSchedulerReap(S,&Retired,&Found,NULL) && !Found && *Data==42,"job data lives until physical return");
    xrtAtomic32Store(&C.Gate,1,XMEMORY_RELEASE);for(unsigned J=0;J<2;++J){xcroncompletion D=execReceipt(B);testRequire(D.Result==XCRON_RUN_CANCELLED && !D.Error && xcronSchedulerComplete(S,D.Run.Id,D.Result,NULL),"physical completion returned to kernel owner");}
    testRequire(xcronSchedulerReap(S,&Retired,&Found,NULL) && Found && Retired.Data==Data && Retired.Info.Cancelled==2 && Retired.Info.State==XCRON_JOB_CANCELLED,"kernel retirement after both bodies");xrtFree(Data);testRequire(xcronSchedulerDestroy(S,NULL),"integrated kernel destroy");execAdapterEnd(B);execPoolEnd(E);
}
static void testExecOOMAndWarm(void)
{
    xexecutor* E=execPool(1,4);execcontext C={0};xcronexecutor* B=execAdapter(E,1,&C,false);xcronexecutorstats Stats;testRequire(xcronExecutorInspect(B,&Stats,NULL),"exact adapter bytes");xcronexecutorconfig Config={E,execProc,&C,NULL,NULL,1,0,Stats.Bytes-1};xcronexecutor* Out=B;xcronexecutorissue I;
    execError(xcronExecutorCreate(&Config,&Out,&I),&I,XCRON_EXECUTOR_LIMIT,"hard adapter bytes");testRequire(Out==B,"factory output preserved");Config.BytesLimit=SIZE_MAX;
    memset(&I,0x5a,sizeof(I));xcronexecutorissue Old=I;testRequire(!xcronExecutorCreate(&Config,(xcronexecutor**)&Config,&I) && !memcmp(&I,&Old,sizeof(I)),"factory input/output alias atomicity");xrtClearError();
    xcroncompletion Done;size_t Count=77;testRequire(!xcronExecutorDrain(B,&Done,1,(size_t*)&Done,&I) && !memcmp(&I,&Old,sizeof(I)),"drain output alias rejection");xrtClearError();testRequire(!xcronExecutorDrain(B,(xcroncompletion*)B,1,&Count,&I) && Count==77,"owned adapter output rejected");xrtClearError();
    testRequire(!xcronExecutorDrain(B,&Done,SIZE_MAX,&Count,&I) && Count==77,"drain size overflow rejected");xrtClearError();
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"adapter OOM quota");Out=NULL;testRequire(!xcronExecutorCreate(&Config,&Out,&I) && !Out && I.Code==XCRON_EXECUTOR_MEMORY && xrtErrorKind(xrtGetError())==XERR_MEMORY,"typed adapter factory OOM");testRequire(xrtAllocationBudgetLeave(&Scope),"leave adapter OOM quota");xrtClearError();xrtAllocationBudgetFree(Budget);
    /* Warm worker/runtime caches before measuring globally counted allocation. */
    xcronrun R=execLease(1,1,NULL);testRequire(xcronExecutorSubmit(B,&R,NULL),"warm runtime submission");Done=execReceipt(B);
    testRequire(xrtAllocationBudgetCreate(1,&Budget),"warm owner quota");xrtSetErrorInfo(XERR_STATE,"owner.sentinel",9,"warm owner error");const xerror* Ambient=xrtGetError();testRequire(xrtAllocationBudgetEnter(Budget,&Scope),"enter warm owner scope");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(true);xrtMemStatsGet(&Before);
#endif
    for(unsigned J=0;J<10000;++J){R.Id=J+2;testRequire(xcronExecutorSubmit(B,&R,NULL),"allocation-free owner submission");Done=execReceipt(B);testRequire(Done.Run.Id==R.Id && Done.Result==XCRON_RUN_SUCCEEDED && !Done.Error && xrtGetError()==Ambient,"warm slot reuse/result/ambient identity");}
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);testRequire(Before.MallocCalls==After.MallocCalls && Before.CallocCalls==After.CallocCalls && Before.ReallocCalls==After.ReallocCalls && Before.BlockAllocCalls==After.BlockAllocCalls,"adapter and warmed executor allocated on steady path");
#endif
    testRequire(xrtAllocationBudgetLeave(&Scope),"leave warm owner quota");xrtClearError();xrtAllocationBudgetFree(Budget);execAdapterEnd(B);execPoolEnd(E);
}
#include "executor_batch_cases.h"
int main(void)
{
    testExecResults();testExecQueueAndCancel();testExecNotifyTail();testExecOwnerConflict();testExecStress();testExecKernel();testExecOOMAndWarm();testExecBatches();
    puts("[PASS] cron executor: results/errors/cancel/rejection/notify tails/32256 concurrent receipts/kernel/OOM/10000 zero-heap cycles");return 0;
}
