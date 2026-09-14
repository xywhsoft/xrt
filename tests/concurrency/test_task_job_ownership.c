#ifdef TASK_JOB_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"
#include <assert.h>

typedef struct JobContext {
    xcancel *gate,*started;
    xrtownershipref job;
    unsigned mode,runs,drops,probes,counts,traces;
} JobContext;
typedef struct JobData { size_t refs;xfuture* held;JobContext* context; } JobData;
static const xrtownershipops* cancel_ops;
static unsigned queued_cases,refusals,allocation_refusals,allocation_successes;
static xtaskoutcome run(xcancel*,ptr,xtaskvalue*);
static void drop_data(ptr,ptr);
static bool count_data(const void* data,size_t* count)
{ const JobData* item=data;++item->context->counts;if(!item->refs)return false;*count=item->refs;return true; }
static bool trace_data(const void* data,xrtownershipvisitor visit,ptr context)
{ const JobData* item=data;++item->context->traces;return visit(xrtFutureOwnership(item->held),context); }
static const xrtownershipops data_ops={count_data,trace_data};
static const xtaskdataownershipv1 policy={sizeof(policy),run,drop_data,&data_ops};
static const xtaskdataownershipv1 other={sizeof(other),run,drop_data,&data_ops};
static const xtaskdataownershipv1* const accepted[]={&policy};
static const xtaskdataownershipv1* const wrong[]={&other};
static void drop_value(ptr value,ptr unused) { assert(!unused);xrtFree(value); }
static void wrong_drop(ptr value,ptr unused) { assert(!unused);xrtFree(value); }
static bool trace_value(const void* value,const void* unused,xrtownershipvisitor visit,ptr context)
{ (void)value;(void)context;return !unused&&visit; }
static const xfuturepayloadownershipv1 result_policy={sizeof(result_policy),drop_value,trace_value};
static const xfuturepayloadownershipv1* const results[]={&result_policy};
static void freeze_begin(xrtownershipscope* scope)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);
    while(!xrtOwnershipFreezeTryBegin(scope)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
}
static int32 active_probe(ptr data)
{
    JobContext* context=data;xrtownershipscope freeze={0};size_t count=0;
    const xrtownershippreparationv1* preparation=NULL;freeze_begin(&freeze);
    assert(!xrtTaskOwnershipAdapterV1(context->job,accepted,1,&preparation)&&!preparation);
    assert(!context->job.Ops->Count(context->job.Data,&count));
    assert(xrtOwnershipScopeEnd(&freeze));return 0;
}
static void probe(JobContext* context)
{
    xthread* thread=xrtThreadCreate(active_probe,context,0);assert(thread);
    assert(xrtThreadWaitFor(thread,5000000)==XWAIT_OK);xrtThreadDestroy(thread);++context->probes;
}
static xtaskoutcome run(xcancel* cancel,ptr data,xtaskvalue* result)
{
    JobData* item=data;JobContext* context=item->context;(void)cancel;
    assert(item->refs==1&&!context->runs&&!context->drops);++context->runs;probe(context);
    if(context->mode==1){xrtSetErrorKind(XERR_STATE);return XTASK_FAILED;}
    assert(context->mode!=2);
    int64* value=xrtMalloc(sizeof(*value));assert(value);*value=79;
    result->Value=value;result->Destroy=context->mode==3?wrong_drop:drop_value;return XTASK_SUCCESS;
}
static void drop_data(ptr data,ptr unused)
{
    JobData* item=data;JobContext* context=item->context;xfuture* held;xrtownershipscope mutation={0};
    assert(!unused&&item->refs==1&&!context->drops);probe(context);++context->drops;
    assert(xrtOwnershipMutationBegin(&mutation));held=item->held;item->held=NULL;item->refs=0;xrtFree(item);
    assert(xrtOwnershipScopeEnd(&mutation));xrtFutureDestroy(held);
}
static xtaskoutcome blocker(xcancel* cancel,ptr data,xtaskvalue* result)
{
    JobContext* context=data;xdeadline deadline=xrtDeadlineAfter(5000000);(void)cancel;(void)result;
    assert(xrtCancelRequest(context->started));
    while(!xrtCancelRequested(context->gate)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    return XTASK_SUCCESS;
}
static bool find_job(xrtownershipref child,ptr data)
{ if(child.Data&&child.Ops!=cancel_ops){xrtownershipref* output=data;assert(!output->Data);*output=child;}return true; }
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static void queued(unsigned mode,int fail_after)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    JobContext context={0};context.mode=mode;
    context.gate=xrtCancelCreate();context.started=xrtCancelCreate();assert(context.gate&&context.started);
    cancel_ops=xrtCancelOwnership(context.gate).Ops;
    xtaskpoolconfig config={0};config.Threads=1;config.QueueLimit=1;
    xtaskpool* pool=xrtTaskPoolCreate(&config);assert(pool);
    xfuture* first=xrtTaskSubmit(pool,blocker,&context,NULL);assert(first);
    xdeadline deadline=xrtDeadlineAfter(5000000);
    while(!xrtCancelRequested(context.started)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    /* A legacy native job still has a real producer edge. It must be opaque,
     * never a producerless source that preparation can close underneath it. */
    const xfutureproducerownershipv1* producers[]={xrtTaskProducerPolicyV1Get()};
    xrtownershipscope freeze={0};freeze_begin(&freeze);
    xrtownershipref legacy={0},legacy_future=xrtFutureOwnership(first);
    const xrtownershippreparationv1* none=NULL;
    assert(!xrtFutureOwnershipAdapterV1(legacy_future,NULL,0));
    assert(xrtFutureOwnershipAdapterV2(legacy_future,NULL,0,producers,1));
    assert(legacy_future.Ops->Trace(legacy_future.Data,find_job,&legacy)&&legacy.Data);
    assert(!xrtTaskOwnershipAdapterV1(legacy,accepted,1,&none)&&!none);
    assert(xrtOwnershipScopeEnd(&freeze));
    JobData* item=xrtCalloc(1,sizeof(*item));assert(item);item->refs=1;item->context=&context;
    if(fail_after>=0)assert(xrtMemDebugFailAfter((uint64)fail_after));
    xfuture* future=xrtTaskSubmitOwnedJobV1(pool,item,NULL,&policy,&result_policy);
    bool triggered=fail_after>=0&&xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    if(!future){
        assert(fail_after>=0&&triggered&&xrtGetError()&&!context.runs&&!context.drops&&item->refs==1);
        xrtClearError();xrtFree(item);++allocation_refusals;
    }else{
        assert(!triggered&&!context.runs&&!context.drops);
        xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));
        item->held=xrtFutureRef(future);assert(item->held);assert(xrtOwnershipScopeEnd(&mutation));
        freeze_begin(&freeze);xrtownershipref root=xrtFutureOwnership(future);xrtownershipresult graph={0};
        assert(root.Ops->Trace(root.Data,find_job,&context.job)&&context.job.Data);
        const xrtownershippreparationv1 *preparation=NULL,*unchanged=(const xrtownershippreparationv1*)&context;
        assert(!xrtTaskOwnershipAdapterV1(context.job,wrong,1,&unchanged)&&unchanged==(const xrtownershippreparationv1*)&context);
        assert(!xrtTaskOwnershipAdapterV1(context.job,NULL,1,&unchanged));
        assert(!xrtTaskOwnershipAdapterV1((xrtownershipref){item,&data_ops},accepted,1,&unchanged));
        assert(!context.counts&&!context.traces);
        const xrtownershipadapterv1* adapter=xrtTaskOwnershipAdapterV1(context.job,accepted,1,&preparation);
        assert(adapter&&preparation&&preparation->Adapter==adapter&&!preparation->Ready(context.job.Data));
        assert(!context.counts&&!context.traces);
        assert(xrtOwnershipInspect(&root,1,NULL,0,&graph)&&graph.NodeCount==4&&graph.EdgeCount==6&&graph.ExternalRootCount==2);
        assert(xrtOwnershipScopeEnd(&freeze));
        /* Return the original caller result reference. Only the accepted
         * executor is now an external root; Data really owns the cycle edge. */
        xrtFutureDestroy(future);freeze_begin(&freeze);
        assert(xrtOwnershipInspect(&root,1,NULL,0,&graph)&&graph.NodeCount==4&&graph.EdgeCount==6&&graph.ExternalRootCount==1);
        size_t refs=0;assert(context.job.Ops->Count(context.job.Data,&refs)&&refs==2);
        assert(adapter->Hold(context.job.Data)&&adapter->Claim(context.job.Data,&context));
        assert(context.job.Ops->Count(context.job.Data,&refs)&&refs==3);
        assert(xrtOwnershipScopeEnd(&freeze));
        assert(preparation->Prepare(context.job.Data,&context)==XRT_OWNERSHIP_PREPARE_BUSY);
        future=xrtFutureRef((xfuture*)root.Data);assert(future);
        if(mode==2)assert(xrtFutureCancel(future));
        assert(xrtCancelRequest(context.gate));
        assert(xrtFutureWaitFor(future,5000000)==XWAIT_OK&&xrtFutureWaitFor(first,5000000)==XWAIT_OK);
        assert(xrtTaskPoolDestroy(pool));pool=NULL;
        assert(context.drops==1&&context.runs==(mode==2?0u:1u)&&context.probes==(mode==2?1u:2u));
        xfutureresult result={0};assert(xrtFutureResult(future,&result));
        assert(result.State==(mode==2?XFUTURE_CANCELLED:mode==1||mode==3?XFUTURE_FAILED:XFUTURE_RESOLVED));
        if(mode==0)assert(result.Value&&*(int64*)result.Value==79);
        if(mode==1||mode==3)assert(result.Error&&xrtErrorKind(result.Error)==XERR_STATE);
        freeze_begin(&freeze);const xrtownershippreparationv1* later=NULL;
        assert(xrtTaskOwnershipAdapterV1(context.job,accepted,1,&later)==adapter&&later==preparation);
        assert(preparation->Ready(context.job.Data)&&context.job.Ops->Count(context.job.Data,&refs)&&refs==1);
        adapter->Restore(context.job.Data,&context);assert(adapter->Claim(context.job.Data,&context));
        assert(xrtOwnershipScopeEnd(&freeze));
        assert(preparation->Prepare(context.job.Data,&context)==XRT_OWNERSHIP_PREPARE_READY);
        freeze_begin(&freeze);adapter->Clear(context.job.Data,&context);assert(xrtOwnershipScopeEnd(&freeze));
        assert(adapter->Finish(context.job.Data,&context)&&adapter->Finish(context.job.Data,&context));adapter->Drop(context.job.Data);
        freeze_begin(&freeze);assert(xrtFutureOwnershipAdapterV1(xrtFutureOwnership(future),results,1));
        assert(xrtOwnershipScopeEnd(&freeze));
        xrtFutureDestroy(future);
        if(fail_after<0)++queued_cases;else ++allocation_successes;
    }
    if(pool){assert(xrtCancelRequest(context.gate));assert(xrtTaskPoolDestroy(pool));}
    xrtFutureDestroy(first);xrtCancelDestroy(context.started);xrtCancelDestroy(context.gate);balance(&before);
}
static void refuse(xtaskpool* pool,ptr data,const xtaskdataownershipv1* selected,const xfuturepayloadownershipv1* value)
{ assert(!xrtTaskSubmitOwnedJobV1(pool,data,NULL,selected,value)&&xrtGetError());xrtClearError();++refusals; }
static void boundaries(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);JobContext context={0};JobData data={1,NULL,&context};
    xtaskpoolconfig config={0};config.Threads=1;xtaskpool* pool=xrtTaskPoolCreate(&config);assert(pool);
    xtaskdataownershipv1 bad=policy;xfuturepayloadownershipv1 bad_result=result_policy;
    xrtownershipops missing_count={NULL,trace_data},missing_trace={count_data,NULL};
    refuse(NULL,&data,&policy,&result_policy);refuse(pool,NULL,&policy,&result_policy);refuse(pool,&data,NULL,&result_policy);
    bad.size=0;refuse(pool,&data,&bad,&result_policy);bad=policy;bad.Proc=NULL;refuse(pool,&data,&bad,&result_policy);
    bad=policy;bad.Drop=NULL;refuse(pool,&data,&bad,&result_policy);bad=policy;bad.Ops=NULL;refuse(pool,&data,&bad,&result_policy);
    bad=policy;bad.Ops=&missing_count;refuse(pool,&data,&bad,&result_policy);bad.Ops=&missing_trace;refuse(pool,&data,&bad,&result_policy);
    refuse(pool,&data,&policy,NULL);bad_result.size=0;refuse(pool,&data,&policy,&bad_result);
    bad_result=result_policy;bad_result.Drop=NULL;refuse(pool,&data,&policy,&bad_result);
    bad_result=result_policy;bad_result.Trace=NULL;refuse(pool,&data,&policy,&bad_result);
    assert(xrtTaskPoolClose(pool));refuse(pool,&data,&policy,&result_policy);
    assert(data.refs==1&&!context.counts&&!context.traces&&!context.runs&&!context.drops);
    assert(xrtTaskPoolDestroy(pool));balance(&before);
}
int main(void)
{
    testRequire(xrtMemDebugEnable(true),"enable native task ownership accounting");
    for(unsigned n=0;n<25;++n){boundaries();for(unsigned mode=0;mode<4;++mode)queued(mode,-1);}
    for(int fail_after=0;fail_after<24;++fail_after)queued(0,fail_after);
    assert(queued_cases==100&&refusals==350&&allocation_refusals&&allocation_successes&&allocation_refusals+allocation_successes==24);
    printf("Native Job ownership: 100 exact four-node/six-edge graphs, 350 no-consume refusals, %u allocation refusals and %u accepted allocations; real executor/producer/pin references, active pre-trace refusal, BUSY/READY preparation, repeated Finish and zero live delta\n",allocation_refusals,allocation_successes);return 0;
}
