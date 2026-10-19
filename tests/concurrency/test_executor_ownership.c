#ifdef EXECUTOR_OWNERSHIP_SINGLE
#ifndef XRT_MODULE_EXECUTOR
#define XRT_MODULE_EXECUTOR
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#else
#include "../../src/internal/xrt_executor.h"
#endif
#include <xrt/detail/wait.h>
#include <assert.h>
#include <stdio.h>

typedef struct ExecutorCase {
    xexecutor* pool;
    const xrtownershipadapterv1* adapter;
    xatomic32 runs, drops, started, gate, drop_started, drop_gate;
    unsigned counts, traces;
    bool block_drop, probe, reject_jobs;
} ExecutorCase;
typedef struct ExecutorData { size_t refs; ExecutorCase* test; xexecutor* held; } ExecutorData;
typedef struct Edges { xrtownershipref items[8]; size_t count; } Edges;
static unsigned idle_cases, queued_cases, opaque_cases, reuse_cases, tail_cases, create_budgets, parallel_cases;
static unsigned snapshot_budgets, snapshot_failures;

static void freeze_begin(xrtownershipscope* scope)
{
    double end=__xrtWaitAfter(5000);
    while(!xrtOwnershipFreezeTryBegin(scope)){assert(!__xrtWaitExpired(end));xrtThreadYield();}
}
static void idle_freeze(xexecutor* pool,xrtownershipscope* scope)
{
    double end=__xrtWaitAfter(5000);size_t count;xrtownershipref ref=xrtExecutorOwnership(pool);
    for(;;){freeze_begin(scope);if(ref.Ops->Count(ref.Data,&count))return;
        assert(xrtOwnershipScopeEnd(scope));assert(!__xrtWaitExpired(end));xrtThreadYield();}
}
static void signal_wait(xatomic32* signal)
{
    double end=__xrtWaitAfter(5000);
    while(!xrtAtomic32Load(signal,XMEMORY_ACQUIRE)){assert(!__xrtWaitExpired(end));xrtThreadYield();}
}
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount&&after.LiveBytes==before->LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        after.InvalidFreeCount==before->InvalidFreeCount&&after.DoubleFreeCount==before->DoubleFreeCount&&
        after.UseAfterFreeCount==before->UseAfterFreeCount);
}
static bool edge(xrtownershipref ref,ptr data)
{Edges* edges=data;assert(edges->count<8);edges->items[edges->count++]=ref;return true;}
static bool count_data(const void* data,size_t* count)
{const ExecutorData* item=data;++item->test->counts;assert(item->refs);*count=item->refs;return true;}
static bool trace_data(const void* data,xrtownershipvisitor visit,ptr context)
{const ExecutorData* item=data;++item->test->traces;return !item->held||visit(xrtExecutorOwnership(item->held),context);}
static const xrtownershipops data_ops={count_data,trace_data};
static void active_probe(ExecutorCase* test)
{
    if(!test->probe)return;
    xrtownershipscope freeze={0};freeze_begin(&freeze);size_t count;
    xrtownershipref ref=xrtExecutorOwnership(test->pool);
    assert(!ref.Ops->Count(ref.Data,&count));assert(xrtExecutorOwnershipQuiescenceV1Get()->Busy(ref.Data));
    assert(xrtOwnershipScopeEnd(&freeze));
}
static void run_data(ptr data)
{
    ExecutorData* item=data;ExecutorCase* test=item->test;assert(item->refs==1);active_probe(test);
    if(test->probe){assert(xrtExecutorWaitFor(test->pool,0)==XWAIT_ERROR);
        assert(!xrtExecutorDestroy(test->pool));xrtClearError();}
    (void)xrtAtomic32FetchAdd(&test->runs,1,XMEMORY_RELEASE);
}
static void drop_data(ptr data,ptr context)
{
    ExecutorData* item=data;ExecutorCase* test=item->test;assert(!context&&item->refs==1);active_probe(test);
    if(test->block_drop){
        assert(xrtExecutorWaitFor(test->pool,0)==XWAIT_ERROR);assert(!xrtExecutorDestroy(test->pool));
        assert(xrtExecutorCancel(test->pool));assert(xrtExecutorWaitFor(test->pool,0)==XWAIT_ERROR);xrtClearError();
        xrtAtomic32Store(&test->drop_started,1,XMEMORY_RELEASE);signal_wait(&test->drop_gate);
    }
    xexecutor* held=item->held;xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));
    item->refs=0;item->held=NULL;xrtFree(item);assert(xrtOwnershipScopeEnd(&mutation));
    if(held)test->adapter->Drop(held);
    (void)xrtAtomic32FetchAdd(&test->drops,1,XMEMORY_RELEASE);
}
static const xexecutordataownershipv1 policy={sizeof(policy),run_data,drop_data,&data_ops};
static const xexecutordataownershipv1 wrong_policy={sizeof(wrong_policy),run_data,drop_data,&data_ops};
static ExecutorData* new_data(ExecutorCase* test)
{ExecutorData* item=xrtCalloc(1,sizeof(*item));assert(item);item->refs=1;item->test=test;return item;}
static const xrtownershipadapterv1* job_adapter(xrtownershipref job,const xrtownershippreparationv1** preparation)
{const xexecutordataownershipv1* policies[]={&policy};return xrtExecutorJobOwnershipAdapterV1(job,policies,1,preparation);}
static bool admit(xrtownershipref ref,ptr context)
{
    ExecutorCase* test=context;const xrtownershippreparationv1* preparation=NULL;
    if(ref.Ops==&data_ops)return true;
    if(xrtExecutorOwnershipAdapterV1(ref,&preparation))return true;
    return !test->reject_jobs&&job_adapter(ref,&preparation)!=NULL;
}
static xexecutor* create(ExecutorCase* test,unsigned threads,size_t queue)
{
    memset(test,0,sizeof(*test));xexecutorconfig config={threads,queue,0};
    test->pool=xrtExecutorCreate(&config);assert(test->pool);return test->pool;
}
static void prepare(ExecutorCase* test,const xrtownershippreparationv1* preparation)
{
    double end=__xrtWaitAfter(5000);
    for(;;){xrtownershipprepareresult result=preparation->Prepare(test->pool,test);
        if(result==XRT_OWNERSHIP_PREPARE_READY)return;
        assert(result==XRT_OWNERSHIP_PREPARE_BUSY&&!__xrtWaitExpired(end));xrtThreadYield();}
}
static void idle(unsigned threads,bool destroy_first)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);ExecutorCase test;create(&test,threads,2);
    xrtownershipscope freeze={0};idle_freeze(test.pool,&freeze);size_t refs;
    xrtownershipref ref=xrtExecutorOwnership(test.pool);const xrtownershippreparationv1* preparation=NULL;
    test.adapter=xrtExecutorOwnershipAdapterV1(ref,&preparation);assert(test.adapter&&preparation);
    assert(ref.Ops->Count(ref.Data,&refs)&&refs==1&&!xrtExecutorOwnershipQuiescenceV1Get()->Busy(ref.Data));
    const xrtownershippreparationv1* sentinel=(const xrtownershippreparationv1*)&test;
    assert(!xrtExecutorOwnershipAdapterV1((xrtownershipref){&test,&data_ops},&sentinel)&&sentinel==(const xrtownershippreparationv1*)&test);
    assert(!xrtExecutorOwnershipAdapterV1((xrtownershipref){0},&sentinel)&&!xrtExecutorOwnershipAdapterV1(ref,NULL));
    xrtownershipresult graph={0};assert(xrtOwnershipInspect(&ref,1,NULL,0,&graph)&&
        graph.NodeCount==1&&graph.EdgeCount==0&&graph.ExternalRootCount==1);
    assert(xrtOwnershipInspect(&ref,1,&ref,1,&graph)&&graph.ExternalRootCount==0);
    assert(test.adapter->Hold(ref.Data));assert(xrtOwnershipScopeEnd(&freeze));
    if(destroy_first)assert(xrtExecutorDestroy(test.pool));
    idle_freeze(test.pool,&freeze);assert(ref.Ops->Count(ref.Data,&refs)&&refs==(destroy_first?1u:2u));
    assert(test.adapter->Claim(ref.Data,&test)&&!test.adapter->Claim(ref.Data,&before));
    test.adapter->Restore(ref.Data,&test);assert(test.adapter->Claim(ref.Data,&test));
    assert(xrtOwnershipScopeEnd(&freeze));prepare(&test,preparation);
    idle_freeze(test.pool,&freeze);assert(preparation->Ready(ref.Data));test.adapter->Clear(ref.Data,&test);
    assert(!test.adapter->Hold(ref.Data));assert(xrtOwnershipScopeEnd(&freeze));
    assert(test.adapter->Finish(ref.Data,&test)&&test.adapter->Finish(ref.Data,&test));
    if(!destroy_first)assert(xrtExecutorDestroy(test.pool));
    test.adapter->Drop(ref.Data);balance(&before);++idle_cases;
}
static void queued(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);ExecutorCase test;create(&test,1,2);test.probe=true;
    ExecutorData* a=new_data(&test);ExecutorData* b=new_data(&test);
    xrtownershipscope freeze={0};idle_freeze(test.pool,&freeze);xrtownershipref ref=xrtExecutorOwnership(test.pool);
    const xrtownershippreparationv1* preparation=NULL;test.adapter=xrtExecutorOwnershipAdapterV1(ref,&preparation);
    assert(test.adapter&&test.adapter->Hold(ref.Data)&&test.adapter->Hold(ref.Data));a->held=test.pool;
    xexecutorowneditemv1 items[]={{a,&policy},{b,&policy}};assert(xrtMemDebugFailAfter(0));
    assert(xrtExecutorSubmitOwnedBatchV1(test.pool,items,2)&&!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    assert(xrtAtomic32Load(&test.runs,XMEMORY_ACQUIRE)==0);size_t refs;
    assert(ref.Ops->Count(ref.Data,&refs)&&refs==5);Edges jobs={0};assert(ref.Ops->Trace(ref.Data,edge,&jobs)&&jobs.count==2);
    const xrtownershipadapterv1* adapters[2];const xrtownershippreparationv1* preparations[2];
    for(size_t i=0;i<2;++i){
        xrtownershipref job=jobs.items[i];unsigned counts=test.counts,traces=test.traces;
        const xrtownershippreparationv1* sentinel=(const xrtownershippreparationv1*)&test;
        const xexecutordataownershipv1* wrong[]={&wrong_policy};
        assert(!xrtExecutorJobOwnershipAdapterV1(job,wrong,1,&sentinel)&&sentinel==(const xrtownershippreparationv1*)&test);
        xrt_executor_job* slot=(xrt_executor_job*)job.Data;const xexecutordataownershipv1* saved=slot->DataPolicy;
        slot->DataPolicy=(const xexecutordataownershipv1*)(uintptr_t)1;
        assert(!job_adapter(job,&sentinel));slot->DataPolicy=saved;
        assert(test.counts==counts&&test.traces==traces);
        adapters[i]=job_adapter(job,&preparations[i]);assert(adapters[i]&&adapters[i]->Hold(job.Data));
        assert(job.Ops->Count(job.Data,&refs)&&refs==2&&adapters[i]->Claim(job.Data,&test));
        assert(preparations[i]->Prepare(job.Data,&test)==XRT_OWNERSHIP_PREPARE_BUSY);
    }
    xrtownershipresult graph={0};assert(xrtOwnershipInspect(&ref,1,NULL,0,&graph)&&graph.NodeCount==5&&graph.EdgeCount==7);
    unsigned calls=test.counts+test.traces;xrtownershipsnapshot* snapshot=NULL;test.reject_jobs=true;
    assert(!xrtOwnershipSnapshotCreate(&ref,1,NULL,0,admit,&test,&snapshot)&&!snapshot&&calls==test.counts+test.traces);
    xrtClearError();test.reject_jobs=false;
    if(queued_cases==0)for(unsigned budget=0;budget<32;++budget){
        assert(xrtMemDebugFailAfter(budget));++snapshot_budgets;
        bool ok=xrtOwnershipSnapshotCreate(&ref,1,NULL,0,admit,&test,&snapshot);
        bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(ok){assert(snapshot&&!triggered&&xrtOwnershipSnapshotNodeCount(snapshot)==5);xrtOwnershipSnapshotDestroy(snapshot);snapshot=NULL;}
        else{assert(triggered&&!snapshot);++snapshot_failures;xrtClearError();}
    }
    assert(xrtOwnershipSnapshotCreate(&ref,1,NULL,0,admit,&test,&snapshot)&&xrtOwnershipSnapshotNodeCount(snapshot)==5);
    xrtOwnershipSnapshotDestroy(snapshot);
    assert(test.adapter->Claim(ref.Data,&test));assert(preparation->Prepare(ref.Data,&test)==XRT_OWNERSHIP_PREPARE_BUSY);
    assert(xrtOwnershipScopeEnd(&freeze));prepare(&test,preparation);idle_freeze(test.pool,&freeze);
    assert(xrtAtomic32Load(&test.runs,XMEMORY_ACQUIRE)==2&&xrtAtomic32Load(&test.drops,XMEMORY_ACQUIRE)==2);
    assert(ref.Ops->Count(ref.Data,&refs)&&refs==4);
    for(size_t i=0;i<2;++i){xrtownershipref job=jobs.items[i];Edges edges={0};
        assert(job.Ops->Count(job.Data,&refs)&&refs==1&&job.Ops->Trace(job.Data,edge,&edges)&&
            edges.count==1&&edges.items[0].Data==ref.Data);
        assert(preparations[i]->Ready(job.Data));adapters[i]->Clear(job.Data,&test);assert(!adapters[i]->Hold(job.Data));}
    test.adapter->Clear(ref.Data,&test);assert(xrtOwnershipScopeEnd(&freeze));
    assert(test.adapter->Finish(ref.Data,&test)&&xrtExecutorDestroy(test.pool));
    for(size_t i=0;i<2;++i){assert(adapters[i]->Finish(jobs.items[i].Data,&test));adapters[i]->Drop(jobs.items[i].Data);}
    test.adapter->Drop(ref.Data);balance(&before);++queued_cases;
}
static void reuse(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);ExecutorCase test;create(&test,1,1);
    ExecutorData* items[3]={new_data(&test),new_data(&test),new_data(&test)};
    xrtownershipref jobs[2];const xrtownershipadapterv1* adapters[2];
    for(size_t i=0;i<2;++i){xrtownershipscope freeze={0};idle_freeze(test.pool,&freeze);
        assert(xrtExecutorSubmitOwnedV1(test.pool,items[i],&policy));Edges edges={0};xrtownershipref ref=xrtExecutorOwnership(test.pool);
        assert(ref.Ops->Trace(ref.Data,edge,&edges)&&edges.count==1);jobs[i]=edges.items[0];
        const xrtownershippreparationv1* preparation=NULL;adapters[i]=job_adapter(jobs[i],&preparation);
        assert(adapters[i]&&adapters[i]->Hold(jobs[i].Data));assert(xrtOwnershipScopeEnd(&freeze));
        double end=__xrtWaitAfter(5000);while(xrtAtomic32Load(&test.drops,XMEMORY_ACQUIRE)<i+1){assert(!__xrtWaitExpired(end));xrtThreadYield();}
    }
    xrtownershipscope freeze={0};idle_freeze(test.pool,&freeze);assert(jobs[0].Data!=jobs[1].Data);
    assert(!xrtExecutorSubmitOwnedV1(test.pool,items[2],&policy)&&items[2]->refs==1);xrtClearError();
    adapters[0]->Drop(jobs[0].Data);assert(xrtExecutorSubmitOwnedV1(test.pool,items[2],&policy));
    Edges edges={0};xrtownershipref ref=xrtExecutorOwnership(test.pool);
    assert(ref.Ops->Trace(ref.Data,edge,&edges)&&edges.count==1&&edges.items[0].Data==jobs[0].Data);
    assert(xrtOwnershipScopeEnd(&freeze));assert(xrtExecutorDestroy(test.pool));
    assert(xrtAtomic32Load(&test.runs,XMEMORY_ACQUIRE)==3&&xrtAtomic32Load(&test.drops,XMEMORY_ACQUIRE)==3);
    adapters[1]->Drop(jobs[1].Data);balance(&before);++reuse_cases;
}
static void opaque(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);ExecutorCase test;create(&test,1,1);ExecutorData* item=new_data(&test);
    xrtownershipscope freeze={0};idle_freeze(test.pool,&freeze);
    assert(xrtExecutorSubmit(test.pool,run_data,item,drop_data,NULL));xrtownershipref ref=xrtExecutorOwnership(test.pool);
    Edges edges={0};assert(ref.Ops->Trace(ref.Data,edge,&edges)&&edges.count==1);size_t count;
    const xrtownershippreparationv1* preparation=NULL;assert(!job_adapter(edges.items[0],&preparation));
    assert(!edges.items[0].Ops->Count(edges.items[0].Data,&count));xrtownershipresult graph={0};
    assert(!xrtOwnershipInspect(&ref,1,NULL,0,&graph));assert(test.counts==0&&test.traces==0);
    assert(xrtOwnershipScopeEnd(&freeze));assert(xrtExecutorDestroy(test.pool));balance(&before);++opaque_cases;
}
static void block(ptr data)
{ExecutorCase* test=data;xrtAtomic32Store(&test->started,1,XMEMORY_RELEASE);signal_wait(&test->gate);}
static int32 cancel_thread(ptr data)
{ExecutorCase* test=data;assert(xrtExecutorCancel(test->pool));return 0;}
static void cleanup_tail(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);ExecutorCase test;create(&test,1,1);test.probe=true;test.block_drop=true;
    assert(xrtExecutorSubmit(test.pool,block,&test,NULL,NULL));signal_wait(&test.started);
    ExecutorData* item=new_data(&test);assert(xrtExecutorSubmitOwnedV1(test.pool,item,&policy));
    xthread* thread=xrtThreadCreate(cancel_thread,&test,0);assert(thread);signal_wait(&test.drop_started);
    xrtAtomic32Store(&test.gate,1,XMEMORY_RELEASE);double end=__xrtWaitAfter(5000);xexecutorstats stats;
    /* Stats fields are independent atomic observations, not a transaction.
     * Observe the worker's real completion tail before checking the one still
     * blocked cancellation cleanup; do not infer idle from one early field. */
    do{assert(!__xrtWaitExpired(end));assert(xrtExecutorGet(test.pool,&stats));xrtThreadYield();}
    while(stats.Completed!=1||stats.Running!=1);
    assert(stats.Queued==0&&stats.Running==1&&stats.Cancelled==0);
    assert(xrtExecutorWaitFor(test.pool,0)==XWAIT_TIMEOUT);assert(!xrtExecutorDestroy(test.pool));xrtClearError();
    active_probe(&test);xrtAtomic32Store(&test.drop_gate,1,XMEMORY_RELEASE);
    assert(xrtThreadWaitFor(thread,5000)==XWAIT_OK);xrtThreadDestroy(thread);
    assert(xrtExecutorWaitFor(test.pool,5000)==XWAIT_OK&&xrtExecutorGet(test.pool,&stats));
    assert(stats.Completed==2&&stats.Executed==1&&stats.Cancelled==1&&stats.Running==0&&stats.Queued==0);
    assert(xrtAtomic32Load(&test.runs,XMEMORY_ACQUIRE)==0&&xrtAtomic32Load(&test.drops,XMEMORY_ACQUIRE)==1);
    assert(xrtExecutorDestroy(test.pool));balance(&before);++tail_cases;
}
static void rejected(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);ExecutorCase test;create(&test,1,1);
    ExecutorData* a=new_data(&test);ExecutorData* b=new_data(&test);xexecutorowneditemv1 items[]={{a,&policy},{b,&policy}};
    assert(!xrtExecutorSubmitOwnedBatchV1(test.pool,items,2)&&a->refs==1&&b->refs==1);xrtClearError();
    assert(!xrtExecutorSubmitOwnedBatchV1(test.pool,NULL,1));assert(!xrtExecutorSubmitOwnedBatchV1(test.pool,items,0));
    xexecutordataownershipv1 invalid=policy;invalid.size=0;assert(!xrtExecutorSubmitOwnedV1(test.pool,a,&invalid));
    items[1].Policy=NULL;assert(!xrtExecutorSubmitOwnedBatchV1(test.pool,items,2));
    assert(xrtExecutorClose(test.pool));assert(!xrtExecutorSubmitOwnedV1(test.pool,a,&policy));xrtClearError();
    assert(xrtAtomic32Load(&test.runs,XMEMORY_ACQUIRE)==0&&xrtAtomic32Load(&test.drops,XMEMORY_ACQUIRE)==0);
    drop_data(a,NULL);drop_data(b,NULL);assert(xrtExecutorDestroy(test.pool));balance(&before);
}
static void parallel(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);ExecutorCase test;create(&test,4,32);test.probe=true;
    xexecutorowneditemv1 items[32];for(size_t i=0;i<32;++i)items[i]=(xexecutorowneditemv1){new_data(&test),&policy};
    assert(xrtExecutorSubmitOwnedBatchV1(test.pool,items,32)&&xrtExecutorClose(test.pool));
    assert(xrtExecutorWaitFor(test.pool,5000)==XWAIT_OK);xexecutorstats stats;assert(xrtExecutorGet(test.pool,&stats));
    assert(stats.Submitted==32&&stats.Completed==32&&stats.Executed==32&&stats.Cancelled==0&&stats.Queued==0&&stats.Running==0);
    assert(xrtExecutorDestroy(test.pool));assert(xrtAtomic32Load(&test.runs,XMEMORY_ACQUIRE)==32&&
        xrtAtomic32Load(&test.drops,XMEMORY_ACQUIRE)==32);balance(&before);++parallel_cases;
}
static void create_prefixes(void)
{
    bool complete=false;
    for(unsigned budget=0;budget<64;++budget){xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        assert(xrtMemDebugFailAfter(budget));xexecutorconfig config={2,2,0};xexecutor* pool=xrtExecutorCreate(&config);
        bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(pool)assert(xrtExecutorDestroy(pool));
        xrtClearError();balance(&before);++create_budgets;
        if(pool&&!triggered){complete=true;break;}
    }
    assert(complete);
}
int main(void)
{
    /* Warm the calling thread's lazy contexts before exact allocation ledgers. */
    assert(xrtMemDebugEnable(true));xrtClearError();assert(xrtTemp(8));assert(xrtTempClear());
    for(unsigned i=0;i<16;++i){idle(1,(i&1)!=0);idle(3,(i&1)==0);queued();reuse();opaque();cleanup_tail();rejected();parallel();}
    idle(8,false);create_prefixes();
    assert(snapshot_budgets==32&&snapshot_failures&&snapshot_failures<snapshot_budgets);
    printf("[PASS] executor ownership: idle=%u queued=%u opaque=%u pinned-reuse=%u cleanup-tail=%u parallel=%u create-prefixes=%u snapshots=%u/%u failed; exact live count/bytes and allocation/free balance\n",
        idle_cases,queued_cases,opaque_cases,reuse_cases,tail_cases,parallel_cases,create_budgets,snapshot_budgets,snapshot_failures);
    return 0;
}
