#ifdef NET_RESOLVER_FUTURE_OWNERSHIP_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"
#include <assert.h>

typedef struct Pins {
    xrtownershipref refs[32];
    const xrtownershipadapterv1* adapters[32];
    const xrtownershippreparationv1* preparations[32];
    size_t count;
} Pins;
static unsigned graph_cases, budgets, failures, race_cases, active_cases;
static void freeze_begin(xrtownershipscope* scope)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);
    while(!xrtOwnershipFreezeTryBegin(scope)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
}
static void idle_freeze(xnetresolver* resolver,xrtownershipscope* scope)
{
    xrtownershipref ref=xrtNetResolverOwnership(resolver);size_t count;
    xdeadline deadline=xrtDeadlineAfter(5000000);
    for(;;){
        freeze_begin(scope);
        if(ref.Ops->Count(ref.Data,&count))return;
        assert(xrtOwnershipScopeEnd(scope));assert(!xrtDeadlineExpired(deadline));xrtThreadYield();
    }
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static const xrtownershipadapterv1* adapter(xrtownershipref ref,const xrtownershippreparationv1** preparation)
{
    const xfutureproducerownershipv1* producer=xrtNetResolverFutureProducerPolicyV1Get();
    const xfuturepayloadownershipv1* payload=xrtNetResolverFuturePayloadPolicyV1Get();
    const xcancelwatchownershipv1* watch=xrtNetResolverFutureCancelPolicyV1Get();
    const xnetresolveownershipv1* request=xrtNetResolverFutureRequestPolicyV1Get();
    xfutureownershipadmissionv1 admission={0};admission.size=sizeof(admission);
    admission.ProducerPolicies=&producer;admission.ProducerPolicyCount=1;
    admission.PayloadPolicies=&payload;admission.PayloadPolicyCount=1;
    admission.CancelWatchPolicies=&watch;admission.CancelWatchPolicyCount=1;
    const xrtownershipadapterv1* result;
    *preparation=NULL;
    if((result=xrtNetResolverOwnershipAdapterV1(ref,NULL,0,preparation)))return result;
    if((result=xrtNetResolveOpOwnershipAdapterV1(ref,&request,1,preparation)))return result;
    if((result=xrtNetResolverFutureOwnershipAdapterV1(ref,preparation)))return result;
    if((result=xrtFutureOwnershipAdapterV4(ref,&admission,preparation)))return result;
    if((result=xrtCancelOwnershipAdapterV2(ref,&watch,1,preparation)))return result;
    if((result=xrtCancelWatchOwnershipAdapterV1(ref,&watch,1,preparation)))return result;
    if((result=xrtNetAddrListOwnershipAdapterV1(ref)))return result;
    return xrtErrorOwnershipAdapterV1(ref);
}
static bool admit(xrtownershipref ref,ptr data)
{const xrtownershippreparationv1* preparation;(void)data;return adapter(ref,&preparation)!=NULL;}
static bool same(xrtownershipref a,xrtownershipref b)
{return a.Data==b.Data&&a.Ops==b.Ops;}
static void collect(Pins* pins,const xrtownershipref* anchors,size_t count)
{
    xrtownershipsnapshot* snapshot=NULL;
    assert(xrtOwnershipSnapshotCreate(anchors,count,NULL,0,admit,NULL,&snapshot));
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(snapshot);++i){
        xrtownershipnode node;assert(xrtOwnershipSnapshotNode(snapshot,i,&node));bool found=false;
        for(size_t j=0;j<pins->count;++j)if(same(pins->refs[j],node.Reference)){found=true;break;}
        if(found)continue;
        size_t at=pins->count++;assert(at<32);pins->refs[at]=node.Reference;
        pins->adapters[at]=adapter(node.Reference,&pins->preparations[at]);assert(pins->adapters[at]);
        assert(pins->adapters[at]->Hold(node.Reference.Data));
        assert(pins->adapters[at]->Claim(node.Reference.Data,pins));
    }
    xrtOwnershipSnapshotDestroy(snapshot);
}
static void prepare_all(Pins* pins)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);
    for(;;){
        bool ready=true;
        for(size_t i=0;i<pins->count;++i)if(pins->preparations[i]){
            xrtownershipprepareresult result=pins->preparations[i]->Prepare(pins->refs[i].Data,pins);
            assert(result!=XRT_OWNERSHIP_PREPARE_FAILED);
            if(result!=XRT_OWNERSHIP_PREPARE_READY)ready=false;
        }
        if(ready)return;
        assert(!xrtDeadlineExpired(deadline));xrtThreadYield();
    }
}
static void pending(unsigned mode,bool reverse)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    xnetresolverconfig config;xrtNetResolverConfigInit(&config);config.Workers=1;config.CacheEntries=0;
    xnetresolver* resolver=xrtNetResolverCreate(&config);assert(resolver);
    xrtownershipscope freeze={0};idle_freeze(resolver,&freeze);
    xfuture* future=xrtNetResolveAsync(resolver,"127.0.0.1",XNET_FAMILY_IPV4);assert(future);
    xrtownershipref roots[2];roots[0]=xrtNetResolverOwnership(resolver);roots[1]=xrtFutureOwnership(future);
    xrtownershipresult graph={0};assert(xrtOwnershipInspect(roots,2,NULL,0,&graph));
    assert(graph.NodeCount==7&&graph.EdgeCount==11&&graph.ExternalRootCount==2);
    assert(xrtOwnershipInspect(roots,2,roots,2,&graph));
    assert(graph.NodeCount==7&&graph.EdgeCount==13&&graph.ExternalRootCount==0);
    Pins pins={0};collect(&pins,roots,2);assert(pins.count==7);
    xrtownershipref frame={0};bool operation=false,watch=false;size_t strong=0;
    for(size_t i=0;i<pins.count;++i){
        const xrtownershippreparationv1* prep=NULL;
        if(xrtNetResolverFutureOwnershipAdapterV1(pins.refs[i],&prep)){
            frame=pins.refs[i];assert(frame.Ops->Count(frame.Data,&strong)&&strong==4);
            assert(!prep->Ready(frame.Data));assert(!pins.adapters[i]->Claim(frame.Data,&config));
            pins.adapters[i]->Restore(frame.Data,&pins);assert(pins.adapters[i]->Claim(frame.Data,&pins));
        }
        const xnetresolveownershipv1* request=xrtNetResolverFutureRequestPolicyV1Get();
        if(xrtNetResolveOpOwnershipAdapterV1(pins.refs[i],&request,1,&prep))operation=true;
        const xcancelwatchownershipv1* cancel=xrtNetResolverFutureCancelPolicyV1Get();
        if(xrtCancelWatchOwnershipAdapterV1(pins.refs[i],&cancel,1,&prep))watch=true;
    }
    assert(frame.Data&&operation&&watch&&xrtFutureState(future)==XFUTURE_PENDING);
    /* Native tests drive individual participants. These are real test-owned
     * slots, not a claim that a module root or generic collector was exercised. */
    /* Preparation is exercised outside freeze in blocked_prepare below.
     * Under freeze only its read-only Ready query is a valid observation. */
    for(size_t i=0;i<pins.count;++i)if(same(pins.refs[i],frame)||same(pins.refs[i],roots[1]))
        assert(!pins.preparations[i]->Ready(pins.refs[i].Data));
    assert(xrtFutureState(future)==XFUTURE_PENDING);
    if(mode&1)assert(xrtFutureCancel(future));
    if(mode&2)xrtFutureDestroy(future);
    assert(xrtOwnershipScopeEnd(&freeze));prepare_all(&pins);
    assert(xrtFutureState(future)==((mode&1)?XFUTURE_CANCELLED:XFUTURE_RESOLVED));
    freeze_begin(&freeze);collect(&pins,pins.refs,pins.count);
    xrtownershipref slots[34];size_t slots_count=0;slots[slots_count++]=roots[0];
    if(!(mode&2))slots[slots_count++]=roots[1];
    for(size_t i=0;i<pins.count;++i)slots[slots_count++]=pins.refs[i];
    assert(xrtOwnershipInspect(pins.refs,pins.count,slots,slots_count,&graph)&&graph.ExternalRootCount==0);
    xnetaddrlist* independent=NULL;
    if(!(mode&1)){
        independent=xrtNetAddrListRef((xnetaddrlist*)xrtFutureValue(future));assert(independent);
        assert(xrtOwnershipInspect(pins.refs,pins.count,slots,slots_count,&graph)&&graph.ExternalRootCount==1);
        assert(xrtNetAddrListCount(independent)==1);
        xrtNetAddrListDestroy(independent);independent=NULL;
    }
    for(size_t i=0;i<pins.count;++i){
        size_t at=reverse?pins.count-i-1:i;
        if(pins.preparations[at])assert(pins.preparations[at]->Ready(pins.refs[at].Data));
        pins.adapters[at]->Clear(pins.refs[at].Data,&pins);
    }
    assert(xrtOwnershipScopeEnd(&freeze));
    for(size_t i=0;i<pins.count;++i){
        size_t at=reverse?pins.count-i-1:i;
        if(pins.adapters[at]->Finish){
            assert(pins.adapters[at]->Finish(pins.refs[at].Data,&pins));
            assert(pins.adapters[at]->Finish(pins.refs[at].Data,&pins));
        }
    }
    assert(xrtNetResolverTryDestroy(resolver)==XNET_RETIRE_READY);
    if(!(mode&2))xrtFutureDestroy(future);
    for(size_t i=0;i<pins.count;++i){
        size_t at=reverse?pins.count-i-1:i;pins.adapters[at]->Drop(pins.refs[at].Data);
    }
    balanced(&before);++graph_cases;
}
static void unknown(void)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    xnetresolver* resolver=xrtNetResolverCreate(NULL);assert(resolver);
    xrtownershipscope freeze={0};idle_freeze(resolver,&freeze);
    xfuture* future=xrtNetResolveAsync(resolver,"127.0.0.1",XNET_FAMILY_IPV4);assert(future);
    const xfutureproducerownershipv1* good=xrtNetResolverFutureProducerPolicyV1Get();
    xfutureproducerownershipv1 copy=*good;const xfutureproducerownershipv1* wrong=&copy;
    const xcancelwatchownershipv1* cancel=xrtNetResolverFutureCancelPolicyV1Get();
    xfutureownershipadmissionv1 admission={0};admission.size=sizeof(admission);
    admission.ProducerPolicies=&wrong;admission.ProducerPolicyCount=1;
    admission.CancelWatchPolicies=&cancel;admission.CancelWatchPolicyCount=1;
    const xrtownershippreparationv1* prep=(const xrtownershippreparationv1*)resolver;
    xerror* marker=xrtErrorCreate(XERR_STATE,"future.ownership",19,"marker");assert(marker);xrtSetError(marker);
    assert(!xrtFutureOwnershipAdapterV4(xrtFutureOwnership(future),&admission,&prep));
    assert(prep==(const xrtownershippreparationv1*)resolver&&xrtGetError()==marker);
    admission.ProducerPolicies=&good;
    assert(xrtFutureOwnershipAdapterV4(xrtFutureOwnership(future),&admission,&prep));
    xrtClearError();xrtErrorFree(marker);
    assert(xrtOwnershipScopeEnd(&freeze));assert(xrtNetResolverDestroy(resolver));
    freeze_begin(&freeze);
    xfuturepayloadownershipv1 payload_copy=*xrtNetResolverFuturePayloadPolicyV1Get();
    const xfuturepayloadownershipv1* payload=&payload_copy;
    admission.PayloadPolicies=&payload;admission.PayloadPolicyCount=1;
    prep=(const xrtownershippreparationv1*)future;
    assert(!xrtFutureOwnershipAdapterV4(xrtFutureOwnership(future),&admission,&prep)&&prep==(const xrtownershippreparationv1*)future);
    payload=xrtNetResolverFuturePayloadPolicyV1Get();
    assert(xrtFutureOwnershipAdapterV4(xrtFutureOwnership(future),&admission,&prep));
    xnetaddrlist* result=xrtNetAddrListRef((xnetaddrlist*)xrtFutureValue(future));assert(result);
    assert(xrtOwnershipScopeEnd(&freeze));xrtFutureDestroy(future);
    assert(xrtNetAddrListCount(result)==1);xrtNetAddrListDestroy(result);balanced(&before);
}
static void allocation_failures(void)
{
    for(unsigned budget=0;budget<48;++budget){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
        xnetresolver* resolver=xrtNetResolverCreate(NULL);assert(resolver);xrtownershipscope freeze={0};idle_freeze(resolver,&freeze);
        assert(xrtMemDebugFailAfter(budget));++budgets;
        xfuture* future=xrtNetResolveAsync(resolver,"127.0.0.1",XNET_FAMILY_IPV4);
        bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(!future){
            assert(failed&&xrtErrorKind(xrtGetError())==XERR_MEMORY);++failures;
            xnetresolverstats stats;assert(xrtNetResolverStats(resolver,&stats)&&stats.Outstanding==0);
        }else assert(!failed);
        xrtClearError();assert(xrtOwnershipScopeEnd(&freeze));
        assert(xrtNetResolverDestroy(resolver));xrtFutureDestroy(future);balanced(&before);
    }
    assert(failures);
}
typedef struct Race {xfuture* future;} Race;
static int32 cancel_thread(ptr data)
{Race* race=data;(void)xrtFutureCancel(race->future);return 0;}
static void races(void)
{
    for(unsigned i=0;i<96;++i){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
        xnetresolver* resolver=xrtNetResolverCreate(NULL);assert(resolver);
        xfuture* future=xrtNetResolveAsync(resolver,"127.0.0.1",XNET_FAMILY_IPV4);assert(future);
        Race race={future};xthread* thread=xrtThreadCreate(cancel_thread,&race,0);assert(thread);
        assert(xrtThreadWaitFor(thread,5000000)==XWAIT_OK);xrtThreadDestroy(thread);
        assert(xrtNetResolverDestroy(resolver));
        assert(xrtFutureState(future)==XFUTURE_RESOLVED||xrtFutureState(future)==XFUTURE_CANCELLED);
        xrtFutureDestroy(future);balanced(&before);++race_cases;
    }
}
typedef struct Probe {unsigned refs; xrtownershipref frame; xnetresolver* resolver; unsigned called;} Probe;
static bool probe_count(const void* data,size_t* count)
{const Probe* probe=data;*count=probe->refs;return probe->refs!=0;}
static bool probe_trace(const void* data,xrtownershipvisitor visit,ptr other)
{(void)data;(void)visit;(void)other;return true;}
static const xrtownershipops probe_ops={probe_count,probe_trace};
static int32 probe_thread(ptr data)
{
    Probe* probe=data;xrtownershipscope freeze={0};freeze_begin(&freeze);size_t count=917;
    assert(!probe->frame.Ops->Count(probe->frame.Data,&count)&&count==917);
    const xrtownershippreparationv1* prep=(const xrtownershippreparationv1*)probe;
    assert(!xrtNetResolverFutureOwnershipAdapterV1(probe->frame,&prep)&&prep==(const xrtownershippreparationv1*)probe);
    xrtownershipref resolver=xrtNetResolverOwnership(probe->resolver);
    assert(!resolver.Ops->Count(resolver.Data,&count)&&count==917);
    assert(xrtOwnershipScopeEnd(&freeze));return 0;
}
static void probe_notify(ptr data)
{
    Probe* probe=data;++probe->called;
    xthread* thread=xrtThreadCreate(probe_thread,probe,0);assert(thread);
    assert(xrtThreadWaitFor(thread,5000000)==XWAIT_OK);xrtThreadDestroy(thread);
}
static void probe_release(ptr data)
{Probe* probe=data;xrtownershipscope scope={0};assert(xrtOwnershipMutationBegin(&scope));assert(probe->refs==2);--probe->refs;assert(xrtOwnershipScopeEnd(&scope));}
static const xfuturewatchownershipv1 probe_policy={sizeof(probe_policy),probe_notify,probe_release,&probe_ops};
static void active(void)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    xnetresolver* resolver=xrtNetResolverCreate(NULL);assert(resolver);xrtownershipscope freeze={0};idle_freeze(resolver,&freeze);
    xfuture* future=xrtNetResolveAsync(resolver,"127.0.0.1",XNET_FAMILY_IPV4);assert(future);
    xrtownershipref root=xrtFutureOwnership(future);xrtownershipsnapshot* snapshot=NULL;
    assert(xrtOwnershipSnapshotCreate(&root,1,NULL,0,admit,NULL,&snapshot));
    Probe probe={1,{0},resolver,0};const xrtownershipadapterv1* frame_adapter=NULL;
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(snapshot);++i){
        xrtownershipnode node;const xrtownershippreparationv1* prep=NULL;
        assert(xrtOwnershipSnapshotNode(snapshot,i,&node));
        frame_adapter=xrtNetResolverFutureOwnershipAdapterV1(node.Reference,&prep);
        if(frame_adapter){probe.frame=node.Reference;assert(frame_adapter->Hold(probe.frame.Data));break;}
    }
    xrtOwnershipSnapshotDestroy(snapshot);assert(frame_adapter);
    xfuturewatch watch;assert(xrtFutureWatchInitOwnershipV1(&watch,&probe,&probe_policy));++probe.refs;
    assert(xrtFutureWatchAdd(future,&watch)==XFUTURE_WATCH_PENDING);
    assert(xrtOwnershipScopeEnd(&freeze));assert(xrtNetResolverDestroy(resolver));
    assert(probe.called==1&&probe.refs==1);frame_adapter->Drop(probe.frame.Data);probe.refs=0;
    xrtFutureDestroy(future);balanced(&before);++active_cases;
}
typedef struct Blocked {
    xatomic32 entered,release;
    xrtownershipref frame;
    const xrtownershipadapterv1* adapter;
    const xrtownershippreparationv1* preparation;
} Blocked;
static xnetaddrlist* blocked_lookup(cstr host,xnetfamily family,ptr data)
{
    Blocked* block=data;(void)host;(void)family;
    xrtAtomic32Store(&block->entered,1,XMEMORY_RELEASE);
    xdeadline deadline=xrtDeadlineAfter(5000000);
    while(!xrtAtomic32Load(&block->release,XMEMORY_ACQUIRE)){
        assert(!xrtDeadlineExpired(deadline));xrtThreadYield();
    }
    xnetaddr address;assert(xrtNetAddrLoopback(&address,XNET_FAMILY_IPV4,0));
    return xrtNetAddrListCreate(&address,1);
}
static bool select_frame(xrtownershipref ref,ptr data)
{
    Blocked* block=data;const xrtownershippreparationv1* preparation=NULL;
    const xrtownershipadapterv1* candidate=xrtNetResolverFutureOwnershipAdapterV1(ref,&preparation);
    if(candidate){block->frame=ref;block->adapter=candidate;block->preparation=preparation;}
    return true;
}
static void blocked_prepare(void)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Blocked block={0};
    xnetresolverconfig config;xrtNetResolverConfigInit(&config);config.Workers=1;
    config.Lookup=blocked_lookup;config.LookupData=&block;
    xnetresolver* resolver=xrtNetResolverCreate(&config);assert(resolver);
    xfuture* future=xrtNetResolveAsync(resolver,"blocked.test",XNET_FAMILY_IPV4);assert(future);
    xdeadline deadline=xrtDeadlineAfter(5000000);
    while(!xrtAtomic32Load(&block.entered,XMEMORY_ACQUIRE)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    xrtownershipscope freeze={0};freeze_begin(&freeze);xrtownershipref ref=xrtFutureOwnership(future);
    assert(ref.Ops->Trace(ref.Data,select_frame,&block)&&block.adapter);
    const xrtownershippreparationv1* future_preparation=NULL;
    const xrtownershipadapterv1* future_adapter=adapter(ref,&future_preparation);assert(future_adapter);
    assert(block.adapter->Hold(block.frame.Data)&&block.adapter->Claim(block.frame.Data,&block));
    assert(future_adapter->Hold(future)&&future_adapter->Claim(future,&block));
    assert(xrtOwnershipScopeEnd(&freeze));
    xerror* marker=xrtErrorCreate(XERR_STATE,"resolver.future",81,"busy marker");assert(marker);xrtSetError(marker);
    for(unsigned i=0;i<32;++i){
        assert(block.preparation->Prepare(block.frame.Data,&block)==XRT_OWNERSHIP_PREPARE_BUSY);
        assert(future_preparation->Prepare(future,&block)==XRT_OWNERSHIP_PREPARE_BUSY);
        assert(xrtFutureState(future)==XFUTURE_PENDING&&xrtGetError()==marker);
    }
    xrtClearError();xrtErrorFree(marker);
    xrtAtomic32Store(&block.release,1,XMEMORY_RELEASE);
    assert(xrtNetResolverDestroy(resolver)&&xrtFutureState(future)==XFUTURE_RESOLVED);
    freeze_begin(&freeze);
    assert(block.preparation->Ready(block.frame.Data)&&future_preparation->Ready(future));
    block.adapter->Restore(block.frame.Data,&block);future_adapter->Restore(future,&block);
    assert(xrtOwnershipScopeEnd(&freeze));block.adapter->Drop(block.frame.Data);
    future_adapter->Drop(future);xrtFutureDestroy(future);balanced(&before);
}
typedef struct Cleanup {
    unsigned refs;
    xatomic32 entered,release;
    xthreadkey* key;
    xthread* worker;
} Cleanup;
static bool cleanup_count(const void* data,size_t* count)
{const Cleanup* item=data;*count=item->refs;return item->refs!=0;}
static const xrtownershipops cleanup_ops={cleanup_count,probe_trace};
static void cleanup_release(ptr data)
{
    Cleanup* item=data;xrtownershipscope scope={0};assert(xrtOwnershipMutationBegin(&scope));
    assert(item->refs>1);--item->refs;assert(xrtOwnershipScopeEnd(&scope));
}
static void cleanup_tail(ptr data)
{
    Cleanup* item=data;xrtAtomic32Store(&item->entered,1,XMEMORY_RELEASE);
    xdeadline deadline=xrtDeadlineAfter(5000000);
    while(!xrtAtomic32Load(&item->release,XMEMORY_ACQUIRE)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    cleanup_release(item);
}
static void cleanup_notify(ptr data)
{
    Cleanup* item=data;xrtownershipscope scope={0};assert(xrtOwnershipMutationBegin(&scope));
    ++item->refs;item->worker=xrtThreadRef(xrtThreadCurrent());assert(item->worker);
    assert(xrtOwnershipScopeEnd(&scope));assert(xrtThreadKeySet(item->key,item));
}
static const xfuturewatchownershipv1 cleanup_policy={sizeof(cleanup_policy),cleanup_notify,cleanup_release,&cleanup_ops};
static void exit_observation(void)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Cleanup item={0};item.refs=1;
    item.key=xrtThreadKeyCreate(cleanup_tail);assert(item.key);
    xnetresolverconfig config;xrtNetResolverConfigInit(&config);config.Workers=1;
    xnetresolver* resolver=xrtNetResolverCreate(&config);assert(resolver);
    xrtownershipscope freeze={0};idle_freeze(resolver,&freeze);
    xfuture* future=xrtNetResolveAsync(resolver,"127.0.0.1",XNET_FAMILY_IPV4);assert(future);
    xfuturewatch watch;assert(xrtFutureWatchInitOwnershipV1(&watch,&item,&cleanup_policy));++item.refs;
    assert(xrtFutureWatchAdd(future,&watch)==XFUTURE_WATCH_PENDING);
    xrtownershipref ref=xrtNetResolverOwnership(resolver);const xrtownershippreparationv1* prep=NULL;
    const xrtownershipadapterv1* resolver_adapter=xrtNetResolverOwnershipAdapterV1(ref,NULL,0,&prep);
    assert(resolver_adapter&&resolver_adapter->Hold(resolver)&&resolver_adapter->Claim(resolver,&item));
    assert(xrtOwnershipScopeEnd(&freeze));xdeadline deadline=xrtDeadlineAfter(5000000);
    /* Start nonblocking retirement; a real user thread-key tail stays active. */
    while(!xrtAtomic32Load(&item.entered,XMEMORY_ACQUIRE)){
        assert(prep->Prepare(resolver,&item)==XRT_OWNERSHIP_PREPARE_BUSY);
        assert(!xrtDeadlineExpired(deadline));xrtThreadYield();
    }
    freeze_begin(&freeze);size_t count=731;xthreadstate state=XTHREAD_FINISHED;
    xerror* marker=xrtErrorCreate(XERR_STATE,"resolver.exit",92,"exit marker");assert(marker);xrtSetError(marker);
    assert(xrtThreadStateTry(item.worker,&state)&&state==XTHREAD_RUNNING);
    assert(!xrtThreadStateTry(NULL,&state)&&state==XTHREAD_RUNNING&&xrtGetError()==marker);
    assert(!ref.Ops->Count(ref.Data,&count)&&count==731&&!prep->Ready(resolver)&&xrtGetError()==marker);
    assert(xrtOwnershipScopeEnd(&freeze));xrtAtomic32Store(&item.release,1,XMEMORY_RELEASE);
    /* No additional Prepare call: the real completion must become observable
     * by read-only revalidation, or a planner can never schedule its next step. */
    for(;;){
        freeze_begin(&freeze);
        if(ref.Ops->Count(ref.Data,&count))break;
        assert(xrtOwnershipScopeEnd(&freeze));assert(!xrtDeadlineExpired(deadline));xrtThreadYield();
    }
    assert(count==2&&!prep->Ready(resolver)&&xrtGetError()==marker);
    assert(xrtThreadStateTry(item.worker,&state)&&state==XTHREAD_FINISHED);
    assert(xrtOwnershipScopeEnd(&freeze));assert(prep->Prepare(resolver,&item)==XRT_OWNERSHIP_PREPARE_READY);
    assert(xrtGetError()==marker);xrtClearError();xrtErrorFree(marker);
    freeze_begin(&freeze);resolver_adapter->Restore(resolver,&item);assert(xrtOwnershipScopeEnd(&freeze));
    assert(xrtNetResolverDestroy(resolver));resolver_adapter->Drop(resolver);
    assert(item.refs==1&&xrtFutureState(future)==XFUTURE_RESOLVED);item.refs=0;
    xrtThreadDestroy(item.worker);assert(xrtThreadKeyDestroy(item.key));xrtFutureDestroy(future);balanced(&before);
}
int main(int argc,char** argv)
{
    const char* selected=argc>1?argv[1]:NULL;
    for(unsigned repeat=0;repeat<6;++repeat){
        if(!selected||!strcmp(selected,"pending"))for(unsigned mode=0;mode<4;++mode)pending(mode,(repeat&1)!=0);
        if(!selected||!strcmp(selected,"unknown"))unknown();
        if(!selected||!strcmp(selected,"active"))active();
        if(!selected||!strcmp(selected,"busy"))blocked_prepare();
        if(!selected||!strcmp(selected,"exit"))exit_observation();
    }
    if(!selected||!strcmp(selected,"oom"))allocation_failures();
    if(!selected||!strcmp(selected,"race"))races();
    testMemoryDebugDrain("resolver Future ownership balance");
    printf("resolver Future graph: pending=%u active=%u races=%u budgets=%u failures=%u\n",graph_cases,active_cases,race_cases,budgets,failures);
    return 0;
}
