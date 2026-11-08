#include "../test.h"
#include <xrt/executor.h>
#include <xrt/thread.h>
#include <xrt/atomic.h>
typedef struct currentcase {
    xexecutor* Pool[3];
    xatomic32 Started[3],Gate,Done,Calls,Drops,Exits,ExitDropped,Refs;
} currentcase;
static xexecutor* currentPool(void)
{xexecutorconfig C={1,8,0};xexecutor* E=xrtExecutorCreate(&C);testRequire(E!=NULL,"current-context pool create");return E;}
static void currentUntil(const xatomic32* V,uint32 N)
{double End=xrtTimer()+10;while(xrtAtomic32Load(V,XMEMORY_ACQUIRE)<N){testRequire(xrtTimer()<End,"current-context bounded wait");xrtThreadYield();}}
static void currentCheck(currentcase* C,bool A,bool B,bool D)
{
    const xerror* Ambient=xrtGetError();testRequire(xrtExecutorIsCurrent(C->Pool[0])==A && xrtExecutorIsCurrent(C->Pool[1])==B && xrtExecutorIsCurrent(C->Pool[2])==D && !xrtExecutorIsCurrent(NULL) && xrtGetError()==Ambient,"exact current native executor scopes and error identity");
}
static void currentWarm(currentcase* C,bool Worker)
{
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget),"query allocation budget");xrtSetErrorInfo(XERR_IO,"executor.current.sentinel",7,"preserve");const xerror* Ambient=xrtGetError();testRequire(xrtAllocationBudgetEnter(Budget,&Scope),"enter current-query quota");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(true);xrtMemStatsGet(&Before);
#endif
    for(unsigned I=0;I<40000;++I){currentCheck(C,Worker,false,false);testRequire(xrtGetError()==Ambient,"warm predicate preserves ambient error");}
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);testRequire(Before.MallocCalls==After.MallocCalls && Before.CallocCalls==After.CallocCalls && Before.ReallocCalls==After.ReallocCalls && Before.BlockAllocCalls==After.BlockAllocCalls,"current-context query allocated");
#endif
    testRequire(xrtAllocationBudgetLeave(&Scope),"leave query quota");xrtClearError();xallocationbudgetinfo Info={.Size=sizeof(Info)};testRequire(xrtAllocationBudgetInfo(Budget,&Info) && !Info.LiveBytes && !Info.LiveCount,"query allocation ownership balance");xrtAllocationBudgetFree(Budget);
}
static void currentChild(ptr Raw)
{currentcase* C=Raw;currentCheck(C,true,false,false);xrtAtomic32FetchAdd(&C->Calls,1,XMEMORY_RELEASE);}
static void currentParent(ptr Raw)
{
    currentcase* C=Raw;currentWarm(C,true);testRequire(xrtExecutorSubmit(C->Pool[0],currentChild,C,NULL,NULL),"queue dependent child");
    /* A synchronous facade can now reject the dependency before waiting. */
    testRequire(xrtExecutorIsCurrent(C->Pool[0]) && !xrtAtomic32Load(&C->Calls,XMEMORY_ACQUIRE),"reject own-worker dependency before child can run");xrtAtomic32Store(&C->Done,1,XMEMORY_RELEASE);
}
static void currentDrop(ptr Raw,ptr Context)
{(void)Context;currentcase* C=Raw;currentCheck(C,true,false,false);xrtAtomic32FetchAdd(&C->Drops,1,XMEMORY_RELEASE);}
static void currentExit(ptr Raw)
{currentcase* C=Raw;currentCheck(C,true,false,false);xrtAtomic32FetchAdd(&C->Exits,1,XMEMORY_RELEASE);}
static void currentExitDrop(ptr Raw,ptr Context)
{(void)Context;currentcase* C=Raw;currentCheck(C,false,false,false);testRequire(xrtAtomic32FetchSub(&C->Refs,1,XMEMORY_ACQ_REL)==2,"owned-exit reference returned");xrtAtomic32Store(&C->ExitDropped,1,XMEMORY_RELEASE);}
static bool currentCount(const void* Raw,size_t* Count)
{const currentcase* C=Raw;*Count=xrtAtomic32Load(&C->Refs,XMEMORY_ACQUIRE);return *Count!=0;}
static bool currentTrace(const void* Raw,xrtownershipvisitor Visit,ptr Context)
{(void)Raw;(void)Context;return Visit!=NULL;}
static const xrtownershipops CurrentOps={currentCount,currentTrace};
static const xexecutorworkerexitownershipv1 CurrentExit={sizeof(CurrentExit),currentExit,currentExitDrop,&CurrentOps};
static void currentReadyB(ptr Raw)
{currentcase* C=Raw;currentCheck(C,false,true,false);xrtAtomic32Store(&C->Started[1],1,XMEMORY_RELEASE);}
static void currentReadyD(ptr Raw)
{currentcase* C=Raw;currentCheck(C,false,false,true);xrtAtomic32Store(&C->Started[2],1,XMEMORY_RELEASE);}
static void testCurrentWorker(void)
{
    currentcase C={0};C.Pool[1]=currentPool();C.Pool[2]=currentPool();xrtAtomic32Store(&C.Refs,2,XMEMORY_RELAXED);xexecutorconfig Config={1,8,0};C.Pool[0]=xrtExecutorCreateOwnedExitV1(&Config,&C,&CurrentExit);testRequire(C.Pool[0]!=NULL,"owned-exit current context");
    testRequire(xrtExecutorSubmit(C.Pool[1],currentReadyB,&C,NULL,NULL) && xrtExecutorSubmit(C.Pool[2],currentReadyD,&C,NULL,NULL),"warm all foreign worker contexts before global allocation snapshot");currentUntil(&C.Started[1],1);currentUntil(&C.Started[2],1);
    currentWarm(&C,false);testRequire(xrtExecutorSubmit(C.Pool[0],currentParent,&C,currentDrop,NULL),"current parent admitted");currentUntil(&C.Done,1);
    testRequire(xrtExecutorClose(C.Pool[0]) && xrtExecutorWaitFor(C.Pool[0],10000)==XWAIT_OK,"worker/cleanup/exit drain");
    currentCheck(&C,false,false,false);testRequire(xrtAtomic32Load(&C.Calls,XMEMORY_ACQUIRE)==1 && xrtAtomic32Load(&C.Drops,XMEMORY_ACQUIRE)==1 && xrtAtomic32Load(&C.Exits,XMEMORY_ACQUIRE)==1,"body/drop/exit current contexts all tested");
    testRequire(xrtExecutorDestroy(C.Pool[0]),"owned-exit executor destroyed");C.Pool[0]=NULL;testRequire(xrtAtomic32Load(&C.ExitDropped,XMEMORY_ACQUIRE)==1 && xrtAtomic32Load(&C.Refs,XMEMORY_ACQUIRE)==1,"post-join context is not a worker/queued-cleanup context");
    testRequire(xrtExecutorDestroy(C.Pool[1]),"other pool one destroyed");testRequire(xrtExecutorDestroy(C.Pool[2]),"other pool two destroyed");
}
static void currentBlockB(ptr Raw)
{currentcase* C=Raw;currentCheck(C,false,true,false);xrtAtomic32Store(&C->Started[1],1,XMEMORY_RELEASE);currentUntil(&C->Gate,1);}
static void currentBlockD(ptr Raw)
{currentcase* C=Raw;currentCheck(C,false,false,true);xrtAtomic32Store(&C->Started[2],1,XMEMORY_RELEASE);currentUntil(&C->Gate,1);}
static void currentUnreachable(ptr Raw)
{(void)Raw;testRequire(false,"cancelled queued body must not run");}
static void currentNestedDrop(ptr Raw,ptr Context)
{(void)Context;currentcase* C=Raw;currentCheck(C,true,true,true);xrtAtomic32FetchAdd(&C->Drops,1,XMEMORY_RELEASE);}
static void currentOuterDrop(ptr Raw,ptr Context)
{(void)Context;currentcase* C=Raw;currentCheck(C,true,true,false);testRequire(xrtExecutorCancel(C->Pool[2]),"nested native cancellation");currentCheck(C,true,true,false);xrtAtomic32FetchAdd(&C->Drops,1,XMEMORY_RELEASE);}
static void currentCancelB(ptr Raw)
{currentcase* C=Raw;currentCheck(C,true,false,false);testRequire(xrtExecutorCancel(C->Pool[1]),"cross-pool native cancellation");currentCheck(C,true,false,false);xrtAtomic32Store(&C->Done,1,XMEMORY_RELEASE);}
static void testCurrentNestedCancellation(void)
{
    currentcase C={0};for(unsigned I=0;I<3;++I)C.Pool[I]=currentPool();
    testRequire(xrtExecutorSubmit(C.Pool[1],currentBlockB,&C,NULL,NULL) && xrtExecutorSubmit(C.Pool[2],currentBlockD,&C,NULL,NULL),"occupy two workers");currentUntil(&C.Started[1],1);currentUntil(&C.Started[2],1);
    testRequire(xrtExecutorSubmit(C.Pool[1],currentUnreachable,&C,currentOuterDrop,NULL) && xrtExecutorSubmit(C.Pool[2],currentUnreachable,&C,currentNestedDrop,NULL),"queued nested cleanup records");
    testRequire(xrtExecutorSubmit(C.Pool[0],currentCancelB,&C,NULL,NULL),"cancel from other executor worker");currentUntil(&C.Done,1);currentCheck(&C,false,false,false);testRequire(xrtAtomic32Load(&C.Drops,XMEMORY_ACQUIRE)==2,"nested cancellation cleanup scopes");xrtAtomic32Store(&C.Gate,1,XMEMORY_RELEASE);
    for(unsigned I=0;I<3;++I)testRequire(xrtExecutorDestroy(C.Pool[I]),"nested pools close/join");
}
static void currentCallerDrop(ptr Raw,ptr Context)
{(void)Context;currentcase* C=Raw;currentCheck(C,false,true,false);xrtAtomic32FetchAdd(&C->Drops,1,XMEMORY_RELEASE);}
static void testCurrentCallerCancellation(void)
{
    currentcase C={0};for(unsigned I=0;I<3;++I)C.Pool[I]=currentPool();testRequire(xrtExecutorSubmit(C.Pool[1],currentBlockB,&C,NULL,NULL),"caller cancellation blocker");currentUntil(&C.Started[1],1);
    testRequire(xrtExecutorSubmit(C.Pool[1],currentUnreachable,&C,currentCallerDrop,NULL),"caller cleanup queued");currentCheck(&C,false,false,false);testRequire(xrtExecutorCancel(C.Pool[1]),"caller cancels queued item");currentCheck(&C,false,false,false);testRequire(xrtAtomic32Load(&C.Drops,XMEMORY_ACQUIRE)==1,"caller cleanup context removed after return");xrtAtomic32Store(&C.Gate,1,XMEMORY_RELEASE);
    for(unsigned I=0;I<3;++I)testRequire(xrtExecutorDestroy(C.Pool[I]),"caller cancellation pools destroyed");
}
int main(void)
{
    testCurrentWorker();testCurrentNestedCancellation();testCurrentCallerCancellation();
    puts("[PASS] executor current: sole-worker dependency guard/body/drop/exit/nested and external cancel/80000 zero-heap checks");return 0;
}
