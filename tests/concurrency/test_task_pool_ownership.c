#ifdef TASK_POOL_OWNERSHIP_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"
#ifndef TASK_POOL_OWNERSHIP_SINGLE
#include "../../src/internal/xrt_task.h"
#endif
#include <assert.h>

typedef struct PoolCase {
    xtaskpool* pool;
    const xrtownershipadapterv1* adapter;
    unsigned mode, runs, drops, values, probes, counts, traces;
    bool reject_jobs;
    xcancel *started, *gate;
} PoolCase;
typedef struct PoolData {
    size_t refs;
    PoolCase* test;
    xfuture* held;
    xtaskpool* held_pool;
} PoolData;
static unsigned idle_cases, queued_cases, opaque_cases, entry_cases, finalizer_cases, snapshot_failures, snapshot_budgets;
static unsigned create_failures, create_successes;
static unsigned resource_cases;
static xtaskoutcome pool_run(xcancel*, ptr, xtaskvalue*);
static void pool_drop(ptr, ptr);
static void freeze_begin(xrtownershipscope* scope)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);
    while(!xrtOwnershipFreezeTryBegin(scope)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
}
static void idle_freeze(xtaskpool* pool,xrtownershipscope* freeze)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);xrtownershipref ref=xrtTaskPoolOwnership(pool);size_t refs;
    for(;;){freeze_begin(freeze);if(ref.Ops->Count(ref.Data,&refs))return;
        assert(xrtOwnershipScopeEnd(freeze));assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
}
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static bool count_data(const void* data,size_t* count)
{const PoolData* item=data;++item->test->counts;if(!item->refs)return false;*count=item->refs;return true;}
static bool trace_data(const void* data,xrtownershipvisitor visit,ptr context)
{
    const PoolData* item=data;++item->test->traces;
    return visit(xrtFutureOwnership(item->held),context)&&visit(xrtTaskPoolOwnership(item->held_pool),context);
}
static const xrtownershipops data_ops={count_data,trace_data};
static const xtaskdataownershipv1 data_policy={sizeof(data_policy),pool_run,pool_drop,&data_ops};
static const xtaskdataownershipv1 wrong_policy={sizeof(wrong_policy),pool_run,pool_drop,&data_ops};
static void drop_value(ptr value,ptr data)
{assert(value&&!data&&*(int*)value==71);xrtFree(value);}
static bool trace_value(const void* value,const void* data,xrtownershipvisitor visit,ptr context)
{(void)value;(void)context;return !data&&visit;}
static const xfuturepayloadownershipv1 result_policy={sizeof(result_policy),drop_value,trace_value};
static bool admit(xrtownershipref ref,ptr data)
{
    PoolCase* test=data;const xrtownershippreparationv1* preparation=NULL;
    const xtaskdataownershipv1* const policies[]={test->reject_jobs?&wrong_policy:&data_policy};
    const xfuturepayloadownershipv1* const results[]={&result_policy};
    const xfutureproducerownershipv1* const producers[]={xrtTaskProducerPolicyV1Get()};
    return xrtTaskPoolOwnershipAdapterV1(ref,&preparation)||ref.Ops==&data_ops||
        xrtTaskOwnershipAdapterV1(ref,policies,1,&preparation)||
        xrtFutureOwnershipAdapterV2(ref,results,1,producers,1)||xrtCancelOwnershipAdapterV1(ref);
}
static int32 refuse_active(ptr data)
{
    PoolCase* test=data;xrtownershipscope freeze={0};freeze_begin(&freeze);
    xrtownershipref ref=xrtTaskPoolOwnership(test->pool);size_t refs=91;
    const xrtownershippreparationv1* preparation=(const xrtownershippreparationv1*)test;
    assert(!ref.Ops->Count(ref.Data,&refs)&&refs==91);
    assert(!xrtTaskPoolOwnershipAdapterV1(ref,&preparation)&&preparation==(const xrtownershippreparationv1*)test);
    assert(xrtOwnershipScopeEnd(&freeze));return 0;
}
static void active_probe(PoolCase* test)
{
    /* A separate thread must acquire freeze while run/drop/native bodies are
     * active. Success proves no shared mutation was held around user code. */
    xthread* thread=xrtThreadCreate(refuse_active,test,0);assert(thread);
    assert(xrtThreadWaitFor(thread,5000000)==XWAIT_OK);xrtThreadDestroy(thread);++test->probes;
    assert(!xrtTaskPoolDestroy(test->pool));xrtClearError();
}
static xtaskoutcome pool_run(xcancel* cancel,ptr data,xtaskvalue* result)
{
    PoolData* item=data;PoolCase* test=item->test;(void)cancel;
    assert(item->refs==1);++test->runs;active_probe(test);
    if(test->mode==1){xrtSetErrorKind(XERR_STATE);return XTASK_FAILED;}
    assert(test->mode!=2);int* value=xrtMalloc(sizeof(*value));assert(value);*value=71;
    result->Value=value;result->Destroy=drop_value;return XTASK_SUCCESS;
}
static void pool_drop(ptr data,ptr unused)
{
    PoolData* item=data;PoolCase* test=item->test;xrtownershipscope mutation={0};
    assert(!unused&&item->refs==1);active_probe(test);++test->drops;
    assert(xrtOwnershipMutationBegin(&mutation));
    xfuture* future=item->held;xtaskpool* pool=item->held_pool;
    item->held=NULL;item->held_pool=NULL;item->refs=0;xrtFree(item);
    assert(xrtOwnershipScopeEnd(&mutation));
    xrtFutureDestroy(future);if(pool)test->adapter->Drop(pool);
}
static bool find_job(xrtownershipref ref,ptr data)
{
    const xtaskdataownershipv1* const policies[]={&data_policy};
    const xrtownershippreparationv1* preparation=NULL;
    if(xrtTaskOwnershipAdapterV1(ref,policies,1,&preparation)){
        xrtownershipref* output=data;assert(!output->Data);*output=ref;
    }
    return true;
}
typedef struct EdgeSet {xrtownershipref* jobs;unsigned count,seen;} EdgeSet;
static bool exact_job_edge(xrtownershipref ref,ptr data)
{
    EdgeSet* edges=data;assert(edges->seen<edges->count);
    assert(ref.Data==edges->jobs[edges->seen].Data&&ref.Ops==edges->jobs[edges->seen].Ops);
    ++edges->seen;return true;
}
static void prepare_pool(PoolCase* test,const xrtownershippreparationv1* preparation)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);
    for(;;){xrtownershipprepareresult result=preparation->Prepare(test->pool,test);
        if(result==XRT_OWNERSHIP_PREPARE_READY)return;
        assert(result==XRT_OWNERSHIP_PREPARE_BUSY&&!xrtDeadlineExpired(deadline));xrtThreadYield();}
}
static void idle(unsigned threads,bool destroy_first)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    PoolCase test={0};xtaskpoolconfig config={threads,4,0};test.pool=xrtTaskPoolCreate(&config);assert(test.pool);
    xrtownershipref ref=xrtTaskPoolOwnership(test.pool);xrtownershipscope freeze={0};idle_freeze(test.pool,&freeze);
    const xrtownershippreparationv1* preparation=NULL;size_t refs;
    test.adapter=xrtTaskPoolOwnershipAdapterV1(ref,&preparation);assert(test.adapter&&preparation);
    const xrtownershippreparationv1* sentinel=(const xrtownershippreparationv1*)&test;
    assert(!xrtTaskPoolOwnershipAdapterV1((xrtownershipref){&test,&data_ops},&sentinel)&&sentinel==(const xrtownershippreparationv1*)&test);
    assert(!xrtTaskPoolOwnershipAdapterV1((xrtownershipref){0},&sentinel));
    assert(!xrtTaskPoolOwnershipAdapterV1(ref,NULL));
    xrtownershipresult graph={0};assert(xrtOwnershipInspect(&ref,1,NULL,0,&graph)&&graph.NodeCount==1&&graph.EdgeCount==0&&graph.ExternalRootCount==1);
    assert(xrtOwnershipInspect(&ref,1,&ref,1,&graph)&&graph.ExternalRootCount==0);
    assert(ref.Ops->Count(ref.Data,&refs)&&refs==1&&test.adapter->Hold(ref.Data));
    assert(xrtOwnershipScopeEnd(&freeze));
    if(destroy_first)assert(xrtTaskPoolDestroy(test.pool));
    idle_freeze(test.pool,&freeze);assert(ref.Ops->Count(ref.Data,&refs)&&refs==(destroy_first?1u:2u));
    assert(test.adapter->Claim(ref.Data,&test));assert(!test.adapter->Claim(ref.Data,&config));
    test.adapter->Restore(ref.Data,&test);assert(test.adapter->Claim(ref.Data,&test));
    assert(xrtOwnershipScopeEnd(&freeze));prepare_pool(&test,preparation);
    idle_freeze(test.pool,&freeze);assert(preparation->Ready(ref.Data));test.adapter->Clear(ref.Data,&test);
    assert(!test.adapter->Hold(ref.Data));assert(xrtOwnershipScopeEnd(&freeze));
    assert(test.adapter->Finish(ref.Data,&test)&&test.adapter->Finish(ref.Data,&test));
    if(!destroy_first)assert(xrtTaskPoolDestroy(test.pool));
    test.adapter->Drop(ref.Data);balance(&before);++idle_cases;
}
static void queued(unsigned count,unsigned mode,bool oom)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    PoolCase test={0};test.mode=mode;xtaskpoolconfig config={1,4,0};test.pool=xrtTaskPoolCreate(&config);assert(test.pool);
    xrtownershipscope freeze={0};idle_freeze(test.pool,&freeze);xrtownershipref pool=xrtTaskPoolOwnership(test.pool);
    const xrtownershippreparationv1 *preparation=NULL,*job_preparations[3]={0};
    const xrtownershipadapterv1* jobs_adapters[3]={0};xfuture* futures[3]={0};xrtownershipref jobs[3]={{0}},slots[4]={pool};
    const xtaskdataownershipv1* const policies[]={&data_policy};
    test.adapter=xrtTaskPoolOwnershipAdapterV1(pool,&preparation);assert(test.adapter);
    for(unsigned i=0;i<count;++i){
        PoolData* item=xrtCalloc(1,sizeof(*item));assert(item);item->refs=1;item->test=&test;
        assert(test.adapter->Hold(test.pool));item->held_pool=test.pool;
        futures[i]=xrtTaskSubmitOwnedJobV1(test.pool,item,NULL,&data_policy,&result_policy);assert(futures[i]);
        item->held=xrtFutureRef(futures[i]);assert(item->held);slots[i+1]=xrtFutureOwnership(futures[i]);
        if(mode==2)assert(xrtFutureCancel(futures[i]));
        assert(slots[i+1].Ops->Trace(slots[i+1].Data,find_job,&jobs[i])&&jobs[i].Data);
        jobs_adapters[i]=xrtTaskOwnershipAdapterV1(jobs[i],policies,1,&job_preparations[i]);assert(jobs_adapters[i]);
    }
    assert(!test.runs&&!test.drops);EdgeSet edges={jobs,count,0};
    assert(pool.Ops->Trace(pool.Data,exact_job_edge,&edges)&&edges.seen==count);
    xrtownershipresult graph={0};
    assert(xrtOwnershipInspect(&pool,1,NULL,0,&graph)&&graph.NodeCount==1+4*count&&graph.EdgeCount==8*count&&graph.ExternalRootCount==1+count);
    assert(xrtOwnershipInspect(&pool,1,slots,count+1,&graph)&&graph.ExternalRootCount==0);
    assert(xrtOwnershipInspect(&pool,1,slots,count,&graph)&&graph.ExternalRootCount==1);
    unsigned calls=test.counts+test.traces;xrtownershipsnapshot* snapshot=NULL;test.reject_jobs=true;
    assert(!xrtOwnershipSnapshotCreate(&pool,1,slots,count+1,admit,&test,&snapshot)&&!snapshot&&calls==test.counts+test.traces);
    xrtClearError();test.reject_jobs=false;
    if(oom)for(unsigned budget=0;budget<32;++budget){
        assert(xrtMemDebugFailAfter(budget));++snapshot_budgets;
        bool ok=xrtOwnershipSnapshotCreate(&pool,1,slots,count+1,admit,&test,&snapshot);
        bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(!ok){assert(failed&&!snapshot);++snapshot_failures;xrtClearError();}
        else{assert(snapshot&&!failed);xrtOwnershipSnapshotDestroy(snapshot);snapshot=NULL;}
    }
    assert(xrtOwnershipSnapshotCreate(&pool,1,slots,count+1,admit,&test,&snapshot));
    assert(xrtOwnershipSnapshotNodeCount(snapshot)==1+4*count);xrtOwnershipSnapshotDestroy(snapshot);
    assert(test.adapter->Hold(pool.Data)&&test.adapter->Claim(pool.Data,&test));
    for(unsigned i=0;i<count;++i)assert(jobs_adapters[i]->Hold(jobs[i].Data)&&jobs_adapters[i]->Claim(jobs[i].Data,&test));
    assert(xrtOwnershipScopeEnd(&freeze));prepare_pool(&test,preparation);
    assert(test.drops==count&&test.runs==(mode==2?0:count)&&test.probes==test.runs+test.drops);
    for(unsigned i=0;i<count;++i){
        xfutureresult result={0};assert(xrtFutureResult(futures[i],&result));
        assert(result.State==(mode==2?XFUTURE_CANCELLED:mode==1?XFUTURE_FAILED:XFUTURE_RESOLVED));
        if(mode==0)assert(result.Value&&*(int*)result.Value==71);
        if(mode==1)assert(result.Error&&xrtErrorKind(result.Error)==XERR_STATE);
        assert(job_preparations[i]->Prepare(jobs[i].Data,&test)==XRT_OWNERSHIP_PREPARE_READY);
    }
    idle_freeze(test.pool,&freeze);size_t refs;
    assert(pool.Ops->Count(pool.Data,&refs)&&refs==2&&preparation->Ready(pool.Data));
    assert(xrtOwnershipInspect(&pool,1,NULL,0,&graph)&&graph.NodeCount==1&&graph.EdgeCount==0);
    for(unsigned i=0;i<count;++i){assert(jobs[i].Ops->Count(jobs[i].Data,&refs)&&refs==1);jobs_adapters[i]->Clear(jobs[i].Data,&test);}
    test.adapter->Clear(pool.Data,&test);assert(xrtOwnershipScopeEnd(&freeze));
    for(unsigned i=0;i<count;++i){assert(jobs_adapters[i]->Finish(jobs[i].Data,&test));jobs_adapters[i]->Drop(jobs[i].Data);}
    assert(test.adapter->Finish(pool.Data,&test)&&test.adapter->Finish(pool.Data,&test));
    assert(xrtTaskPoolDestroy(test.pool));test.adapter->Drop(pool.Data);
    for(unsigned i=0;i<count;++i)xrtFutureDestroy(futures[i]);
    balance(&before);++queued_cases;
}
static xtaskoutcome blocker(xcancel* cancel,ptr data,xtaskvalue* result)
{
    PoolCase* test=data;(void)cancel;(void)result;active_probe(test);
    assert(xrtCancelRequest(test->started));xdeadline deadline=xrtDeadlineAfter(5000000);
    while(!xrtCancelRequested(test->gate)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    return XTASK_SUCCESS;
}
static void opaque(void)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);PoolCase test={0};
    test.started=xrtCancelCreate();test.gate=xrtCancelCreate();assert(test.started&&test.gate);
    xtaskpoolconfig config={1,4,0};test.pool=xrtTaskPoolCreate(&config);assert(test.pool);
    xrtownershipscope freeze={0};idle_freeze(test.pool,&freeze);xrtownershipref pool=xrtTaskPoolOwnership(test.pool);
    xfuture* future=xrtTaskSubmit(test.pool,blocker,&test,NULL);assert(future);
    xrtownershipsnapshot* snapshot=NULL;
    assert(!xrtOwnershipSnapshotCreate(&pool,1,NULL,0,admit,&test,&snapshot)&&!snapshot);xrtClearError();
    assert(xrtOwnershipScopeEnd(&freeze));
    xdeadline deadline=xrtDeadlineAfter(5000000);
    while(!xrtCancelRequested(test.started)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    (void)refuse_active(&test);assert(xrtCancelRequest(test.gate));
    assert(xrtTaskPoolDestroy(test.pool));xrtFutureDestroy(future);
    xrtCancelDestroy(test.started);xrtCancelDestroy(test.gate);balance(&before);++opaque_cases;
}
static void reentrant_entry(void)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);PoolCase test={0};test.mode=2;
    xtaskpoolconfig config={1,2,0};test.pool=xrtTaskPoolCreate(&config);assert(test.pool);
    xcancel* cancel=xrtCancelCreate();assert(cancel&&xrtCancelRequest(cancel));
    PoolData* item=xrtCalloc(1,sizeof(*item));assert(item);item->refs=1;item->test=&test;
    /* An already-cancelled accepted Job releases Data on the submitting native
     * stack. Reentrant Destroy must not free the pool before submit returns. */
    xfuture* future=xrtTaskSubmitOwnedJobV1(test.pool,item,cancel,&data_policy,&result_policy);assert(future);
    assert(test.drops==1&&!test.runs&&test.probes==1);xtaskpoolstats stats={0};
    assert(xrtTaskPoolGet(test.pool,&stats)&&!stats.Closed&&stats.Cancelled==1&&stats.Completed==1);
    xrtFutureDestroy(future);xrtCancelDestroy(cancel);assert(xrtTaskPoolDestroy(test.pool));
    balance(&before);++entry_cases;
}
static void finalizer_run(ptr data)
{
    PoolCase* test=data;active_probe(test);assert(xrtCancelRequest(test->started));
    xdeadline deadline=xrtDeadlineAfter(5000000);
    while(!xrtCancelRequested(test->gate)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    ++test->drops;
}
static void opaque_finalizer(void)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);PoolCase test={0};
    test.started=xrtCancelCreate();test.gate=xrtCancelCreate();assert(test.started&&test.gate);
    xtaskpoolconfig config={2,2,0};test.pool=xrtTaskPoolCreate(&config);assert(test.pool);
    xrtownershipscope freeze={0};idle_freeze(test.pool,&freeze);
    xrt_task_finalizer finalizer={0};__xrtTaskPoolFinalize(test.pool,&finalizer,finalizer_run,&test);
    xrtownershipref pool=xrtTaskPoolOwnership(test.pool);size_t refs=43;
    const xrtownershippreparationv1* preparation=(const xrtownershippreparationv1*)&test;
    assert(!pool.Ops->Count(pool.Data,&refs)&&refs==43);
    assert(!xrtTaskPoolOwnershipAdapterV1(pool,&preparation)&&preparation==(const xrtownershippreparationv1*)&test);
    assert(xrtOwnershipScopeEnd(&freeze));
    xdeadline deadline=xrtDeadlineAfter(5000000);
    while(!xrtCancelRequested(test.started)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    (void)refuse_active(&test);assert(xrtCancelRequest(test.gate));
    assert(xrtTaskPoolDestroy(test.pool)&&test.drops==1&&test.probes==1);
    xrtCancelDestroy(test.started);xrtCancelDestroy(test.gate);balance(&before);++finalizer_cases;
}
static void create_oom(unsigned budget)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);xtaskpoolconfig config={3,4,0};
    assert(xrtMemDebugFailAfter(budget));xtaskpool* pool=xrtTaskPoolCreate(&config);
    bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    if(pool){assert(!triggered&&xrtTaskPoolDestroy(pool));++create_successes;}
    else{assert(triggered);++create_failures;xrtClearError();}
    balance(&before);
}
static void delayed_resource(bool destroy_attempt)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);PoolCase test={0};
    xtaskpoolconfig config={1,2,0};test.pool=xrtTaskPoolCreate(&config);assert(test.pool);
    str path=NULL;xfile native=xrtFileTemp(NULL,"xlir_pool_credit_",".tmp",&path);assert(native&&path);
    xrtownershipscope freeze={0};idle_freeze(test.pool,&freeze);xrtownershipref pool=xrtTaskPoolOwnership(test.pool);
    const xrtownershippreparationv1* preparation=NULL;test.adapter=xrtTaskPoolOwnershipAdapterV1(pool,&preparation);assert(test.adapter);
    assert(test.adapter->Hold(pool.Data));
    if(!destroy_attempt)assert(test.adapter->Claim(pool.Data,&test));
    assert(xrtOwnershipScopeEnd(&freeze));
    xasyncfile* file=xrtAsyncFileAdopt(test.pool,native);assert(file);
    idle_freeze(test.pool,&freeze);size_t refs;xrtownershipresult graph={0};
    assert(pool.Ops->Count(pool.Data,&refs)&&refs==3); /* owner, plan, actual file credit */
    xrtownershipref internal[2]={{0}};internal[0]=pool;internal[1]=pool;
    /* Exactly the owner and plan slots, NOT the file. Explicit assignments
     * also avoid the bundled TCC's runtime-struct array initializer bug. */
    assert(xrtOwnershipInspect(&pool,1,internal,2,&graph)&&graph.ExternalRootCount==1);
    assert(!test.adapter->Claim(pool.Data,&test));assert(xrtOwnershipScopeEnd(&freeze));
    if(destroy_attempt){assert(!xrtTaskPoolDestroy(test.pool));xrtClearError();}
    else{
        assert(preparation->Prepare(pool.Data,&test)==XRT_OWNERSHIP_PREPARE_BUSY);
        xtaskpoolstats stats={0};assert(xrtTaskPoolGet(test.pool,&stats)&&!stats.Closed);
    }
    xfuture* closed=xrtAsyncFileClose(file);assert(closed&&xrtFutureWaitFor(closed,5000000)==XWAIT_OK);
    idle_freeze(test.pool,&freeze);assert(pool.Ops->Count(pool.Data,&refs)&&refs==2);
    assert(test.adapter->Claim(pool.Data,&test));assert(xrtOwnershipScopeEnd(&freeze));
    prepare_pool(&test,preparation);idle_freeze(test.pool,&freeze);
    test.adapter->Clear(pool.Data,&test);assert(xrtOwnershipScopeEnd(&freeze));
    assert(test.adapter->Finish(pool.Data,&test)&&xrtTaskPoolDestroy(test.pool));test.adapter->Drop(pool.Data);
    xrtFutureDestroy(closed);assert(xrtFileDelete(path));xrtFree(path);balance(&before);++resource_cases;
}
int main(void)
{
    testRequire(xrtMemDebugEnable(true),"enable native pool ownership accounting");
    for(unsigned i=0;i<20;++i){
        for(unsigned threads=1;threads<=3;++threads){idle(threads,(i&1)!=0);queued(threads,i%3,i==0);}
        opaque();reentrant_entry();opaque_finalizer();delayed_resource((i&1)!=0);
    }
    for(unsigned budget=0;budget<48;++budget)create_oom(budget);
    assert(idle_cases==60&&queued_cases==60&&opaque_cases==20&&entry_cases==20&&finalizer_cases==20&&resource_cases==20&&snapshot_budgets==96&&snapshot_failures&&create_failures&&create_successes);
    printf("Native pool ownership: 60 parked/joined shells, 60 exact queued graphs, 20 opaque/active refusals, 20 reentrant native entries, 20 opaque finalizers, 20 real delayed-file roots; %u snapshot budgets/%u failures, 48 creation budgets/%u failures/%u successes; actual queue Job edges, no cancellation substitution, exact live count/bytes balance\n",snapshot_budgets,snapshot_failures,create_failures,create_successes);
    return 0;
}
