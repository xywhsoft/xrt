#ifdef TASK_POOL_WORKER_EXIT_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#else
#include "../../src/internal/xrt_task.h"
#endif
#include <xrt/detail/wait.h>
#include "../test.h"
#include <assert.h>

typedef struct ExitCase {
    size_t refs;
    xtaskpool* pool;
    xthreadkey* key;
    xatomic32 exits,drops,gate,jobs,late,lateGate,lateDone;
    unsigned counts,traces;
    bool probe;
} ExitCase;
typedef struct Edges {xrtownershipref edge;unsigned count;} Edges;
static const xtaskpoolworkerexitownershipv1 policy;
static unsigned owned_cases,resource_cases,late_cases,create_prefixes;
static uint32 signal_value(const xatomic32* signal)
{return xrtAtomic32Load(signal,XMEMORY_ACQUIRE);}
static void freeze_begin(xrtownershipscope* freeze)
{
    double until=__xrtWaitAfter(5000);
    while(!xrtOwnershipFreezeTryBegin(freeze)){assert(!__xrtWaitExpired(until));xrtThreadYield();}
}
static void signal_wait(const xatomic32* signal,uint32 value)
{
    double until=__xrtWaitAfter(5000);
    while(xrtAtomic32Load(signal,XMEMORY_ACQUIRE)<value){assert(!__xrtWaitExpired(until));xrtThreadYield();}
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount&&after.LiveBytes==before->LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        after.InvalidFreeCount==before->InvalidFreeCount&&after.DoubleFreeCount==before->DoubleFreeCount&&
        after.UseAfterFreeCount==before->UseAfterFreeCount);
}
static bool count_context(const void* raw,size_t* count)
{ExitCase* test=(ExitCase*)raw;++test->counts;*count=test->refs;return *count!=0;}
static bool trace_context(const void* raw,xrtownershipvisitor visit,ptr context)
{ExitCase* test=(ExitCase*)raw;++test->traces;(void)context;return visit!=NULL;}
static const xrtownershipops context_ops={count_context,trace_context};
static const xrtownershipadapterv1* admitted(xrtownershipref ref,const xrtownershippreparationv1** prepare)
{const xtaskpoolworkerexitownershipv1* known[]={&policy};return xrtTaskPoolOwnershipAdapterV2(ref,known,1,prepare);}
static bool edge(xrtownershipref ref,ptr raw)
{Edges* edges=raw;edges->edge=ref;++edges->count;return true;}
static void busy_probe(ExitCase* test)
{
    xrtownershipscope freeze={0};freeze_begin(&freeze);xrtownershipref ref=xrtTaskPoolOwnership(test->pool);size_t count;
    assert(!ref.Ops->Count(ref.Data,&count)&&xrtTaskPoolOwnershipQuiescenceV1Get()->Busy(ref.Data));
    const xrtownershippreparationv1* sentinel=(const void*)test;
    assert(!admitted(ref,&sentinel)&&sentinel==(const void*)test);
    assert(xrtOwnershipScopeEnd(&freeze));
}
static void exit_worker(ptr raw)
{
    ExitCase* test=raw;
    if(test->probe){
        busy_probe(test); /* This succeeds only outside mutation/locks. */
        assert(xrtTaskPoolTryJoinV1(test->pool)==XWAIT_ERROR);
        assert(!xrtTaskPoolDestroy(test->pool));xrtClearError();
    }
    xrtAtomic32FetchAdd(&test->exits,1,XMEMORY_RELEASE);
    signal_wait(&test->gate,1);
}
static void drop_context(ptr raw,ptr context)
{
    ExitCase* test=raw;assert(!context&&test->refs==2);
    xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));--test->refs;
    assert(xrtOwnershipScopeEnd(&mutation));xrtAtomic32FetchAdd(&test->drops,1,XMEMORY_RELEASE);
}
static const xtaskpoolworkerexitownershipv1 policy={sizeof(policy),exit_worker,drop_context,&context_ops};
static const xtaskpoolworkerexitownershipv1 policy_copy={sizeof(policy_copy),exit_worker,drop_context,&context_ops};
static xtaskoutcome job(xcancel* cancel,ptr raw,xtaskvalue* result)
{
    (void)cancel;(void)result;ExitCase* test=raw;
    if(test->key){
        ptr previous=xrtThreadKeyGet(test->key);assert(!previous||previous==test);
        if(!previous)assert(xrtThreadKeySet(test->key,test));
    }
    xrtAtomic32FetchAdd(&test->jobs,1,XMEMORY_RELEASE);return XTASK_SUCCESS;
}
static void join(xtaskpool* pool)
{
    double until=__xrtWaitAfter(5000);
    for(;;){xwaitresult result=xrtTaskPoolTryJoinV1(pool);if(result==XWAIT_OK)return;
        assert(result==XWAIT_TIMEOUT&&!__xrtWaitExpired(until));xrtThreadYield();}
}
static void owned(unsigned threads,bool restore)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);ExitCase test={.refs=2,.probe=true};
    xtaskpoolconfig config={threads,8,0};test.pool=xrtTaskPoolCreateOwnedExitV1(&config,&test,&policy);assert(test.pool);
    double unclosed=__xrtWaitAfter(5000);
    for(;;){xwaitresult result=xrtTaskPoolTryJoinV1(test.pool);if(result==XWAIT_ERROR)break;
        assert(result==XWAIT_TIMEOUT&&!__xrtWaitExpired(unclosed));xrtThreadYield();}
    xrtClearError();
    for(unsigned i=0;i<4;++i){
        xfuture* future=xrtTaskSubmit(test.pool,job,&test,NULL);assert(future&&xrtFutureWaitFor(future,5000)==XWAIT_OK);
        xrtFutureDestroy(future);assert(xrtAtomic32Load(&test.exits,XMEMORY_ACQUIRE)==0);
    }
    assert(xrtTaskPoolClose(test.pool)&&xrtTaskPoolWaitFor(test.pool,5000)==XWAIT_OK);
    assert(!xrtAtomic32Load(&test.exits,XMEMORY_ACQUIRE)); /* Close is NOT stop. */
    xrtownershipscope freeze={0};const xrtownershippreparationv1* prepare=NULL;const xrtownershipadapterv1* adapter=NULL;
    double until=__xrtWaitAfter(5000);xrtownershipref ref=xrtTaskPoolOwnership(test.pool);
    for(;;){freeze_begin(&freeze);size_t count;if(ref.Ops->Count(ref.Data,&count))break;
        assert(xrtOwnershipScopeEnd(&freeze)&&!__xrtWaitExpired(until));xrtThreadYield();}
    const xrtownershippreparationv1* sentinel=(const void*)&test;prepare=sentinel;
    assert(!xrtTaskPoolOwnershipAdapterV1(ref,&prepare)&&prepare==sentinel);
    const xtaskpoolworkerexitownershipv1* copied[]={&policy_copy};
    assert(!xrtTaskPoolOwnershipAdapterV2(ref,copied,1,&prepare)&&prepare==sentinel);
    assert(!xrtTaskPoolOwnershipAdapterV2(ref,NULL,1,&prepare)&&prepare==sentinel);
    assert(!test.counts&&!test.traces);
    adapter=admitted(ref,&prepare);assert(adapter&&prepare);
    Edges edges={0};assert(ref.Ops->Trace(ref.Data,edge,&edges));
    assert(edges.count==1&&edges.edge.Data==&test&&edges.edge.Ops==&context_ops);
    size_t count;assert(edges.edge.Ops->Count(edges.edge.Data,&count)&&count==2);
    assert(adapter->Hold(ref.Data)&&adapter->Claim(ref.Data,&test));assert(xrtOwnershipScopeEnd(&freeze));
    until=__xrtWaitAfter(5000);
    while(!xrtAtomic32Load(&test.exits,XMEMORY_ACQUIRE)){
        assert(prepare->Prepare(ref.Data,&test)==XRT_OWNERSHIP_PREPARE_BUSY);
        assert(!__xrtWaitExpired(until));xrtThreadYield();
    }
    signal_wait(&test.exits,threads);busy_probe(&test);assert(!signal_value(&test.drops));
    if(restore){
        freeze_begin(&freeze);adapter->Restore(ref.Data,&test);assert(xrtOwnershipScopeEnd(&freeze));
        assert(xrtTaskPoolTryJoinV1(test.pool)==XWAIT_TIMEOUT); /* Tail still running. */
    }
    xrtAtomic32Store(&test.gate,1,XMEMORY_RELEASE);
    if(restore){
        join(test.pool);assert(xrtTaskPoolTryJoinV1(test.pool)==XWAIT_OK);
        xtaskpoolstats stats={0};assert(xrtTaskPoolGet(test.pool,&stats)&&stats.Submitted==4&&stats.Completed==4&&stats.Closed);
    }else{
        until=__xrtWaitAfter(5000);
        for(;;){xrtownershipprepareresult result=prepare->Prepare(ref.Data,&test);
            if(result==XRT_OWNERSHIP_PREPARE_READY)break;
            assert(result==XRT_OWNERSHIP_PREPARE_BUSY&&!__xrtWaitExpired(until));xrtThreadYield();}
        freeze_begin(&freeze);adapter->Clear(ref.Data,&test);assert(xrtOwnershipScopeEnd(&freeze));
        assert(adapter->Finish(ref.Data,&test)&&test.refs==1&&signal_value(&test.drops)==1);
    }
    assert(xrtTaskPoolDestroy(test.pool));assert(test.refs==1&&signal_value(&test.drops)==1);
    adapter->Drop(ref.Data);balanced(&before);++owned_cases;
}
static void resource_credit(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);ExitCase test={.refs=2};xtaskpoolconfig config={1,2,0};
    test.pool=xrtTaskPoolCreateOwnedExitV1(&config,&test,&policy);assert(test.pool&&__xrtTaskPoolAcquireResource(test.pool));
    assert(xrtTaskPoolClose(test.pool)&&xrtTaskPoolWaitFor(test.pool,0)==XWAIT_OK);
    assert(xrtTaskPoolTryJoinV1(test.pool)==XWAIT_TIMEOUT&&!signal_value(&test.exits)&&!xrtGetError());
    assert(__xrtTaskPoolAcquireResource(test.pool)); /* Accepted resource tail still legal. */
    __xrtTaskPoolReleaseResource(test.pool);__xrtTaskPoolReleaseResource(test.pool);
    double until=__xrtWaitAfter(5000);
    while(!xrtAtomic32Load(&test.exits,XMEMORY_ACQUIRE)){
        assert(xrtTaskPoolTryJoinV1(test.pool)==XWAIT_TIMEOUT&&!__xrtWaitExpired(until));xrtThreadYield();}
    assert(!__xrtTaskPoolAcquireResource(test.pool));xrtClearError();
    xrtAtomic32Store(&test.gate,1,XMEMORY_RELEASE);
    assert(xrtMemDebugFailAfter(0));join(test.pool);assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    assert(xrtTaskPoolDestroy(test.pool)&&test.refs==1&&signal_value(&test.drops)==1);balanced(&before);++resource_cases;
}
static void late_cleanup(ptr raw)
{
    ExitCase* test=raw;
    assert(xrtAtomic32Load(&test->exits,XMEMORY_ACQUIRE)==1);
    xrtAtomic32Store(&test->late,1,XMEMORY_RELEASE);signal_wait(&test->lateGate,1);
    xrtAtomic32Store(&test->lateDone,1,XMEMORY_RELEASE);
}
static void thread_tail(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);ExitCase test={.refs=2};xtaskpoolconfig config={1,2,0};
    test.key=xrtThreadKeyCreate(late_cleanup);assert(test.key);xrtAtomic32Store(&test.gate,1,XMEMORY_RELEASE);
    test.pool=xrtTaskPoolCreateOwnedExitV1(&config,&test,&policy);assert(test.pool);
    for(unsigned i=0;i<3;++i){xfuture* future=xrtTaskSubmit(test.pool,job,&test,NULL);
        assert(future&&xrtFutureWaitFor(future,5000)==XWAIT_OK);xrtFutureDestroy(future);assert(!signal_value(&test.late)&&!signal_value(&test.exits));}
    assert(xrtTaskPoolClose(test.pool)&&xrtTaskPoolWaitFor(test.pool,5000)==XWAIT_OK);
    double until=__xrtWaitAfter(5000);
    while(!xrtAtomic32Load(&test.late,XMEMORY_ACQUIRE)){
        assert(xrtTaskPoolTryJoinV1(test.pool)==XWAIT_TIMEOUT&&!__xrtWaitExpired(until));xrtThreadYield();}
    /* Worker-body Exited is already true; native thread wrapper cleanup is
     * still blocked. Neither poll nor ownership prepare may fake completion. */
    assert(xrtTaskPoolTryJoinV1(test.pool)==XWAIT_TIMEOUT);busy_probe(&test);assert(!signal_value(&test.lateDone)&&!signal_value(&test.drops));
    xrtAtomic32Store(&test.lateGate,1,XMEMORY_RELEASE);join(test.pool);
    assert(signal_value(&test.lateDone)&&xrtTaskPoolDestroy(test.pool)&&signal_value(&test.drops)==1);
    assert(xrtThreadKeyDestroy(test.key));balanced(&before);++late_cases;
}
static void startup(void)
{
    bool complete=false;
    for(unsigned budget=0;budget<80;++budget){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);ExitCase test={.refs=2};xtaskpoolconfig config={2,2,0};
        xrtAtomic32Store(&test.gate,1,XMEMORY_RELEASE);assert(xrtMemDebugFailAfter(budget));
        test.pool=xrtTaskPoolCreateOwnedExitV1(&config,&test,&policy);bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(test.pool){assert(xrtTaskPoolDestroy(test.pool));assert(test.refs==1&&signal_value(&test.drops)==1&&signal_value(&test.exits)==2);}
        else assert(test.refs==2&&!signal_value(&test.drops)&&!signal_value(&test.exits));
        xrtClearError();balanced(&before);++create_prefixes;if(test.pool&&!triggered){complete=true;break;}
    }
    assert(complete);ExitCase test={.refs=2};xtaskpoolconfig config={1,2,0};xtaskpoolworkerexitownershipv1 invalid=policy;invalid.size=0;
    assert(!xrtTaskPoolCreateOwnedExitV1(&config,&test,&invalid));assert(!xrtTaskPoolCreateOwnedExitV1(&config,NULL,&policy));
    assert(!xrtTaskPoolCreateOwnedExitV1(&config,&test,NULL));assert(xrtTaskPoolTryJoinV1(NULL)==XWAIT_ERROR);xrtClearError();
}
int main(void)
{
    testRequire(xrtMemDebugEnable(true),"enable real worker-exit accounting");
    xrtSetErrorKind(XERR_STATE);xrtClearError();assert(xrtTemp(8)&&xrtTempClear());
    for(unsigned i=0;i<16;++i){owned(i%2?3:1,(i&2)!=0);resource_credit();thread_tail();}
    startup();
    printf("[PASS] task pool owned exit: owned=%u resources=%u actual-thread-tails=%u startup-prefixes=%u; exact policy/edge/pin/restore/finish, no per-job cleanup, balanced\n",owned_cases,resource_cases,late_cases,create_prefixes);
    return 0;
}
