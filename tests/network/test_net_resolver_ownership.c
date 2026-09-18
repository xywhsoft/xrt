#ifdef NET_RESOLVER_OWNERSHIP_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"
#include <assert.h>

typedef struct ResolverCase ResolverCase;
typedef struct ResolverData {
    size_t refs;
    ResolverCase* test;
    xnetresolver* resolver;
    xnetresolveop* operation;
} ResolverData;
struct ResolverCase {
    xnetresolver* resolver;
    const xrtownershipadapterv1* adapter;
    unsigned counts, traces, lookups, done, drops, probes, lookup_drops, self_retire;
    bool reject_lookup, reject_request, probe, resurrect;
    xnetresolveop* escaped;
    xatomic32 entered, release;
    bool block;
};
static unsigned idle_cases, queue_cases, opaque_cases, active_cases, oom_cases, oom_failures, resurrect_cases;
static void freeze_begin(xrtownershipscope* scope)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);
    while(!xrtOwnershipFreezeTryBegin(scope)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
}
static void idle_freeze(xnetresolver* resolver,xrtownershipscope* scope)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);xrtownershipref ref=xrtNetResolverOwnership(resolver);size_t count;
    for(;;){freeze_begin(scope);if(ref.Ops->Count(ref.Data,&count))return;
        assert(xrtOwnershipScopeEnd(scope));assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static bool data_count(const void* data,size_t* count)
{const ResolverData* item=data;++item->test->counts;if(!item->refs)return false;*count=item->refs;return true;}
static bool data_trace(const void* data,xrtownershipvisitor visit,ptr context)
{
    const ResolverData* item=data;++item->test->traces;
    return visit(xrtNetResolverOwnership(item->resolver),context)&&visit(xrtNetResolveOpOwnership(item->operation),context);
}
static const xrtownershipops data_ops={data_count,data_trace};
static int32 probe_thread(ptr data)
{
    ResolverData* item=data;ResolverCase* test=item->test;xrtownershipscope freeze={0};freeze_begin(&freeze);
    xrtownershipref ref=xrtNetResolverOwnership(test->resolver);size_t count=197;
    const xrtownershippreparationv1* preparation=(const xrtownershippreparationv1*)test;
    assert(!ref.Ops->Count(ref.Data,&count)&&count==197);
    if(item->operation){ref=xrtNetResolveOpOwnership(item->operation);assert(!ref.Ops->Count(ref.Data,&count)&&count==197);}
    assert(!xrtNetResolverOwnershipAdapterV1(xrtNetResolverOwnership(test->resolver),NULL,0,&preparation));
    assert(preparation==(const xrtownershippreparationv1*)test);
    assert(xrtOwnershipScopeEnd(&freeze));return 0;
}
static void probe(ResolverData* item)
{
    xthread* thread=xrtThreadCreate(probe_thread,item,0);assert(thread);
    assert(xrtThreadWaitFor(thread,5000000)==XWAIT_OK);xrtThreadDestroy(thread);++item->test->probes;
}
static void data_drop(const void* data)
{
    ResolverData* item=(ResolverData*)data;ResolverCase* test=item->test;
    if(test->probe)probe(item);
    xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));
    assert(item->refs==1);item->refs=0;
    xnetresolver* resolver=item->resolver;xnetresolveop* operation=item->operation;
    item->resolver=NULL;item->operation=NULL;
    if(resolver)++test->lookup_drops;else ++test->drops;
    xrtFree(item);assert(xrtOwnershipScopeEnd(&mutation));
    if(resolver)test->adapter->Drop(resolver);
    xrtNetResolveOpDestroy(operation);
}
static xnetaddrlist* lookup(cstr host,xnetfamily family,ptr data)
{
    ResolverData* item=data;ResolverCase* test=item->test;(void)host;(void)family;++test->lookups;
    if(test->probe)probe(item);
    xrtAtomic32Store(&test->entered,1,XMEMORY_RELEASE);
    if(test->block){xdeadline deadline=xrtDeadlineAfter(5000000);
        while(!xrtAtomic32Load(&test->release,XMEMORY_ACQUIRE)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}}
    xnetaddr address;assert(xrtNetAddrLoopback(&address,XNET_FAMILY_IPV4,0));
    return xrtNetAddrListCreate(&address,1);
}
static void done(xnetresolveop* operation,ptr data)
{
    ResolverData* item=data;ResolverCase* test=item->test;++test->done;
    assert(item->operation==operation);
    if(test->probe)probe(item);
    xnetresolveopstate state=xrtNetResolveOpState(operation);
    assert(state==XNET_RESOLVE_RESOLVED||state==XNET_RESOLVE_CANCELLED);
    /* A plan claim must not turn the existing own-worker BUSY contract into a
     * semantic error. This worker cannot join or consume its creator owner. */
    xerror* previous=xrtErrorCreate(XERR_STATE,"resolver.ownership",37,"callback marker");assert(previous);xrtSetError(previous);
    xnetretireresult retired=xrtNetResolverTryDestroy(test->resolver);
    if(retired!=XNET_RETIRE_BUSY)fprintf(stderr,"own-worker retirement while claimed: result=%d\n",(int)retired);
    assert(retired==XNET_RETIRE_BUSY&&xrtGetError()==previous);
    xrtClearError();xrtErrorFree(previous);++test->self_retire;
    if(test->resurrect&&!test->escaped){test->escaped=xrtNetResolveOpRef(operation);assert(test->escaped);}
}
static const xnetresolverlookupownershipv1 lookup_policy={sizeof(lookup_policy),lookup,data_drop,&data_ops};
static const xnetresolverlookupownershipv1 wrong_lookup={sizeof(wrong_lookup),lookup,data_drop,&data_ops};
static const xnetresolveownershipv1 request_policy={sizeof(request_policy),done,data_drop,&data_ops};
static const xnetresolveownershipv1 wrong_request={sizeof(wrong_request),done,data_drop,&data_ops};
static bool admit(xrtownershipref ref,ptr data)
{
    ResolverCase* test=data;const xrtownershippreparationv1* preparation=NULL;
    const xnetresolverlookupownershipv1* lookups[]={test->reject_lookup?&wrong_lookup:&lookup_policy};
    const xnetresolveownershipv1* requests[]={test->reject_request?&wrong_request:&request_policy};
    return xrtNetResolverOwnershipAdapterV1(ref,lookups,1,&preparation)||
        xrtNetResolveOpOwnershipAdapterV1(ref,requests,1,&preparation)||ref.Ops==&data_ops||
        xrtErrorOwnershipAdapterV1(ref)||xrtNetAddrListOwnershipAdapterV1(ref);
}
static void prepare(const xrtownershippreparationv1* preparation,const void* data,const void* token)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);
    for(;;){xrtownershipprepareresult result=preparation->Prepare(data,token);if(result==XRT_OWNERSHIP_PREPARE_READY)return;
        assert(result==XRT_OWNERSHIP_PREPARE_BUSY&&!xrtDeadlineExpired(deadline));xrtThreadYield();}
}
static void idle(unsigned workers,bool destroy_first)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    xnetresolverconfig config;xrtNetResolverConfigInit(&config);config.Workers=workers;
    xnetresolver* resolver=xrtNetResolverCreate(&config);assert(resolver);
    xrtownershipscope freeze={0};idle_freeze(resolver,&freeze);xrtownershipref ref=xrtNetResolverOwnership(resolver);
    size_t count;const xrtownershippreparationv1* preparation=NULL;
    const xrtownershipadapterv1* adapter=xrtNetResolverOwnershipAdapterV1(ref,NULL,0,&preparation);assert(adapter&&preparation);
    assert(ref.Ops->Count(ref.Data,&count)&&count==1&&adapter->Hold(ref.Data));
    assert(xrtOwnershipScopeEnd(&freeze));if(destroy_first)assert(xrtNetResolverDestroy(resolver));
    idle_freeze(resolver,&freeze);assert(ref.Ops->Count(ref.Data,&count)&&count==(destroy_first?1u:2u));
    assert(adapter->Claim(resolver,&config)&&!adapter->Claim(resolver,&before));adapter->Restore(resolver,&config);
    assert(adapter->Claim(resolver,&config));
    assert(!xrtNetResolverResolve(resolver,"127.0.0.1",XNET_FAMILY_IPV4,NULL,NULL));xrtClearError();
    assert(xrtOwnershipScopeEnd(&freeze));prepare(preparation,resolver,&config);
    idle_freeze(resolver,&freeze);assert(preparation->Ready(resolver));adapter->Clear(resolver,&config);
    assert(!adapter->Hold(resolver));assert(xrtOwnershipScopeEnd(&freeze));
    assert(adapter->Finish(resolver,&config)&&adapter->Finish(resolver,&config));
    if(!destroy_first)assert(xrtNetResolverTryDestroy(resolver)==XNET_RETIRE_READY);
    adapter->Drop(resolver);balanced(&before);++idle_cases;
}
static void queued(unsigned count,bool cancel,bool resurrect,bool oom)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);ResolverCase test={0};test.resurrect=resurrect;test.probe=true;
    ResolverData* owner=xrtCalloc(1,sizeof(*owner));assert(owner);owner->refs=1;owner->test=&test;
    xnetresolverconfig config;xrtNetResolverConfigInit(&config);config.Workers=1;config.CacheEntries=4;
    test.resolver=xrtNetResolverCreateOwnedV1(&config,owner,&lookup_policy);assert(test.resolver);
    xrtownershipscope freeze={0};idle_freeze(test.resolver,&freeze);
    xrtownershipref refs[4]={xrtNetResolverOwnership(test.resolver)};xnetresolveop* operations[3]={0};
    const xrtownershippreparationv1 *preparation=NULL,*op_preparations[3]={0};const xrtownershipadapterv1* op_adapters[3]={0};
    const xnetresolverlookupownershipv1* lookups[]={&lookup_policy};const xnetresolveownershipv1* requests[]={&request_policy};
    test.adapter=xrtNetResolverOwnershipAdapterV1(refs[0],lookups,1,&preparation);assert(test.adapter&&test.adapter->Hold(test.resolver));owner->resolver=test.resolver;
    for(unsigned i=0;i<count;++i){ResolverData* item=xrtCalloc(1,sizeof(*item));assert(item);item->refs=1;item->test=&test;
        operations[i]=xrtNetResolverResolveOwnedV1(test.resolver,"loopback.test",XNET_FAMILY_IPV4,item,&request_policy);assert(operations[i]);
        item->operation=xrtNetResolveOpRef(operations[i]);assert(item->operation);refs[i+1]=xrtNetResolveOpOwnership(operations[i]);
        op_adapters[i]=xrtNetResolveOpOwnershipAdapterV1(refs[i+1],requests,1,&op_preparations[i]);assert(op_adapters[i]);}
    xrtownershipresult graph={0};assert(xrtOwnershipInspect(refs,count+1,NULL,0,&graph));
    assert(graph.NodeCount==3+2*count&&graph.EdgeCount==3+4*count&&graph.ExternalRootCount==count+1);
    assert(xrtOwnershipInspect(refs,count+1,refs,count+1,&graph));
    assert(graph.NodeCount==3+2*count&&graph.EdgeCount==4+5*count&&graph.ExternalRootCount==0);
    xrtownershipsnapshot* snapshot=NULL;unsigned calls=test.counts+test.traces;
    test.reject_lookup=true;assert(!xrtOwnershipSnapshotCreate(refs,count+1,refs,count+1,admit,&test,&snapshot)&&!snapshot);
    assert(calls==test.counts+test.traces);test.reject_lookup=false;xrtClearError();
    test.reject_request=true;calls=test.counts+test.traces;
    /* Resolver lookup data may be visited first, but unknown request data must
     * not be inspected. Put the operation first to assert pre-trace refusal. */
    assert(!xrtOwnershipSnapshotCreate(&refs[1],1,refs,count+1,admit,&test,&snapshot)&&!snapshot);
    assert(calls==test.counts+test.traces);test.reject_request=false;xrtClearError();
    if(oom)for(unsigned budget=0;budget<24;++budget){assert(xrtMemDebugFailAfter(budget));++oom_cases;
        bool ok=xrtOwnershipSnapshotCreate(refs,count+1,refs,count+1,admit,&test,&snapshot);bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(ok){assert(snapshot&&!failed);xrtOwnershipSnapshotDestroy(snapshot);snapshot=NULL;}else{assert(failed&&!snapshot);++oom_failures;xrtClearError();}}
    assert(xrtOwnershipSnapshotCreate(refs,count+1,refs,count+1,admit,&test,&snapshot));xrtOwnershipSnapshotDestroy(snapshot);
    if(cancel)assert(xrtNetResolveOpCancel(operations[0]));
    assert(test.adapter->Hold(test.resolver)&&test.adapter->Claim(test.resolver,&test));
    for(unsigned i=0;i<count;++i){assert(op_adapters[i]->Hold(operations[i])&&op_adapters[i]->Claim(operations[i],&test));xrtNetResolveOpDestroy(operations[i]);}
    assert(xrtOwnershipScopeEnd(&freeze));prepare(preparation,test.resolver,&test);
    assert(test.done==count&&test.drops==count&&test.self_retire==count&&test.lookup_drops==0&&test.lookups==(cancel&&count==1?0u:1u));
    idle_freeze(test.resolver,&freeze);
    xrtownershipref held_slots[5];held_slots[0]=refs[0];held_slots[1]=refs[0];
    for(unsigned i=0;i<count;++i)held_slots[i+2]=refs[i+1];
    assert(xrtOwnershipInspect(refs,count+1,held_slots,count+2,&graph)&&graph.ExternalRootCount==(resurrect?1u:0u));
    /* Cache and each successful operation really share the same immutable
     * result owner. Keeping it independently roots only the leaf, not service. */
    xnetaddrlist* result=NULL;
    if(!cancel||count>1){
        result=xrtNetResolveOpResult(operations[cancel?1u:0u]);assert(result);
        size_t result_refs;xrtownershipref result_ref=xrtNetAddrListOwnership(result);
        assert(result_ref.Ops->Count(result_ref.Data,&result_refs)&&result_refs==2+count-(cancel?1u:0u));
    }
    assert(xrtOwnershipInspect(refs,count+1,held_slots,count+2,&graph)&&graph.ExternalRootCount==(resurrect?1u:0u)+(result?1u:0u));
    if(!resurrect)assert(graph.ReachableAnchorCount==0);
    if(resurrect){
        assert(test.escaped&&graph.ReachableAnchorCount>=2);
        for(unsigned i=0;i<count;++i)op_adapters[i]->Restore(operations[i],&test);
        test.adapter->Restore(test.resolver,&test);
        assert(xrtOwnershipScopeEnd(&freeze));
        xrtNetResolveOpDestroy(test.escaped);test.escaped=NULL;
        for(unsigned i=0;i<count;++i)op_adapters[i]->Drop(operations[i]);
        assert(xrtNetResolverDestroy(test.resolver));test.adapter->Drop(test.resolver);++resurrect_cases;
    }else{
        for(unsigned i=0;i<count;++i){assert(op_preparations[i]->Ready(operations[i]));op_adapters[i]->Clear(operations[i],&test);}
        test.adapter->Clear(test.resolver,&test);assert(xrtOwnershipScopeEnd(&freeze));
        assert(test.adapter->Finish(test.resolver,&test)&&test.adapter->Finish(test.resolver,&test));
        assert(test.lookup_drops==1);
        for(unsigned i=0;i<count;++i){assert(op_adapters[i]->Finish(operations[i],&test)&&op_adapters[i]->Finish(operations[i],&test));op_adapters[i]->Drop(operations[i]);}
        assert(xrtNetResolverTryDestroy(test.resolver)==XNET_RETIRE_READY);test.adapter->Drop(test.resolver);
    }
    if(result)assert(xrtNetAddrListCount(result)==1);
    xrtNetAddrListDestroy(result);
    balanced(&before);++queue_cases;
}
static void opaque(void)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);ResolverCase test={0};ResolverData data={1,&test,NULL,NULL};
    xnetresolverconfig config;xrtNetResolverConfigInit(&config);config.Workers=1;config.Lookup=lookup;config.LookupData=&data;
    test.resolver=xrtNetResolverCreate(&config);assert(test.resolver);xrtownershipscope freeze={0};freeze_begin(&freeze);
    xrtownershipref ref=xrtNetResolverOwnership(test.resolver);size_t count=19;const xrtownershippreparationv1* preparation=NULL;
    assert(!ref.Ops->Count(ref.Data,&count)&&count==19&&!xrtNetResolverOwnershipAdapterV1(ref,NULL,0,&preparation)&&!preparation);
    assert(xrtOwnershipScopeEnd(&freeze));assert(xrtNetResolverDestroy(test.resolver));balanced(&before);++opaque_cases;
}
static void borrowed_done(xnetresolveop* operation,ptr data)
{assert(xrtNetResolveOpState(operation)==XNET_RESOLVE_RESOLVED);++*(unsigned*)data;}
static void opaque_request(void)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);unsigned calls=0;
    xnetresolver* resolver=xrtNetResolverCreate(NULL);assert(resolver);xrtownershipscope freeze={0};idle_freeze(resolver,&freeze);
    xnetresolveop* operation=xrtNetResolverResolve(resolver,"127.0.0.1",XNET_FAMILY_IPV4,borrowed_done,&calls);assert(operation);
    xrtownershipref ref=xrtNetResolveOpOwnership(operation);size_t count=75;
    const xrtownershippreparationv1* preparation=(const xrtownershippreparationv1*)&calls;
    assert(!ref.Ops->Count(ref.Data,&count)&&count==75&&!xrtNetResolveOpOwnershipAdapterV1(ref,NULL,0,&preparation));
    assert(preparation==(const xrtownershippreparationv1*)&calls&&calls==0);
    assert(xrtOwnershipScopeEnd(&freeze));assert(xrtNetResolverDestroy(resolver)&&calls==1);
    freeze_begin(&freeze);assert(xrtNetResolveOpOwnershipAdapterV1(ref,NULL,0,&preparation)&&preparation->Ready(operation));
    assert(ref.Ops->Count(ref.Data,&count)&&count==1);assert(xrtOwnershipScopeEnd(&freeze));
    xrtNetResolveOpDestroy(operation);balanced(&before);++opaque_cases;
}
static void active(void)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);ResolverCase test={0};test.block=true;
    ResolverData* owner=xrtCalloc(1,sizeof(*owner));assert(owner);owner->refs=1;owner->test=&test;
    test.resolver=xrtNetResolverCreateOwnedV1(NULL,owner,&lookup_policy);assert(test.resolver);
    xrtownershipscope freeze={0};idle_freeze(test.resolver,&freeze);const xrtownershippreparationv1* preparation=NULL;
    const xnetresolverlookupownershipv1* lookups[]={&lookup_policy};test.adapter=xrtNetResolverOwnershipAdapterV1(xrtNetResolverOwnership(test.resolver),lookups,1,&preparation);assert(test.adapter);
    assert(test.adapter->Hold(test.resolver));owner->resolver=test.resolver;assert(xrtOwnershipScopeEnd(&freeze));
    xnetresolveop* operation=xrtNetResolverResolve(test.resolver,"blocked.test",XNET_FAMILY_IPV4,NULL,NULL);assert(operation);
    xdeadline deadline=xrtDeadlineAfter(5000000);while(!xrtAtomic32Load(&test.entered,XMEMORY_ACQUIRE)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    freeze_begin(&freeze);size_t count=91;xrtownershipref ref=xrtNetResolverOwnership(test.resolver);
    assert(!ref.Ops->Count(ref.Data,&count)&&count==91);assert(xrtOwnershipScopeEnd(&freeze));
    xerror* error=xrtErrorCreate(XERR_STATE,"ownership",7,"preserve");assert(error);xrtSetError(error);
    assert(xrtNetResolverTryDestroy(test.resolver)==XNET_RETIRE_BUSY&&xrtGetError()==error);xrtClearError();xrtErrorFree(error);
    xrtAtomic32Store(&test.release,1,XMEMORY_RELEASE);assert(xrtNetResolverDestroy(test.resolver));
    xrtNetResolveOpDestroy(operation);balanced(&before);++active_cases;
}
static void create_oom(void)
{
    for(unsigned budget=0;budget<20;++budget){xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
        ResolverCase test={0};ResolverData* owner=xrtCalloc(1,sizeof(*owner));assert(owner);owner->refs=1;owner->test=&test;
        assert(xrtMemDebugFailAfter(budget));xnetresolver* resolver=xrtNetResolverCreateOwnedV1(NULL,owner,&lookup_policy);
        bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(!resolver){assert(failed&&owner->refs==1&&!test.drops);xrtClearError();xrtFree(owner);++oom_failures;}
        else{assert(xrtNetResolverDestroy(resolver)&&test.drops==1);}++oom_cases;balanced(&before);}
}
static void leaf(void)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);xnetaddr address;
    assert(xrtNetAddrLoopback(&address,XNET_FAMILY_IPV4,0));xnetaddrlist* list=xrtNetAddrListCreate(&address,1);assert(list);
    xrtownershipref ref=xrtNetAddrListOwnership(list);const xrtownershipadapterv1* adapter=xrtNetAddrListOwnershipAdapterV1(ref);assert(adapter);
    xrtownershipscope freeze={0};freeze_begin(&freeze);size_t count;xrtownershipresult graph={0};
    assert(ref.Ops->Count(ref.Data,&count)&&count==1&&adapter->Hold(list));
    xrtownershipref slots[2]={ref,ref};assert(xrtOwnershipInspect(&ref,1,slots,2,&graph)&&graph.NodeCount==1&&graph.EdgeCount==2&&graph.ExternalRootCount==0);
    assert(adapter->Claim(list,&before)&&adapter->Claim(list,&graph));adapter->Clear(list,&before);adapter->Restore(list,&graph);
    assert(xrtOwnershipScopeEnd(&freeze));adapter->Drop(list);assert(xrtNetAddrListCount(list)==1);xrtNetAddrListDestroy(list);balanced(&before);
}
static void submit_oom(void)
{
    for(unsigned budget=0;budget<8;++budget){xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
        ResolverCase test={0};test.resolver=xrtNetResolverCreate(NULL);assert(test.resolver);
        xrtownershipscope freeze={0};idle_freeze(test.resolver,&freeze);
        ResolverData* item=xrtCalloc(1,sizeof(*item));assert(item);item->refs=1;item->test=&test;
        xrtownershipref ref=xrtNetResolverOwnership(test.resolver);size_t original,count;assert(ref.Ops->Count(ref.Data,&original));
        assert(xrtMemDebugFailAfter(budget));
        xnetresolveop* operation=xrtNetResolverResolveOwnedV1(test.resolver,"127.0.0.1",XNET_FAMILY_IPV4,item,&request_policy);
        bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++oom_cases;
        if(!operation){assert(failed&&item->refs==1&&!test.drops&&!test.done&&ref.Ops->Count(ref.Data,&count)&&count==original);
            xrtFree(item);xrtClearError();++oom_failures;}
        else{assert(!failed);item->operation=xrtNetResolveOpRef(operation);assert(item->operation);}
        assert(xrtOwnershipScopeEnd(&freeze));assert(xrtNetResolverDestroy(test.resolver));
        if(operation){assert(test.drops==1&&test.done==1);xrtNetResolveOpDestroy(operation);}balanced(&before);
    }
}
static void cache_shared(void)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    xnetresolver* resolver=xrtNetResolverCreate(NULL);assert(resolver);xnetresolveop* operations[2]={0};xnetaddrlist* lists[2]={0};
    for(unsigned i=0;i<2;++i){operations[i]=xrtNetResolverResolve(resolver,"127.0.0.1",XNET_FAMILY_IPV4,NULL,NULL);assert(operations[i]);
        xdeadline deadline=xrtDeadlineAfter(5000000);
        for(;;){xrtownershipscope freeze={0};idle_freeze(resolver,&freeze);xnetresolverstats stats;assert(xrtNetResolverStats(resolver,&stats));
            bool ready=stats.Outstanding==0;assert(xrtOwnershipScopeEnd(&freeze));if(ready)break;assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
        lists[i]=xrtNetResolveOpResult(operations[i]);assert(lists[i]);}
    assert(lists[0]==lists[1]);xrtownershipscope freeze={0};idle_freeze(resolver,&freeze);
    xrtownershipref ref=xrtNetAddrListOwnership(lists[0]);size_t count;assert(ref.Ops->Count(ref.Data,&count)&&count==5);
    xnetresolverstats stats;assert(xrtNetResolverStats(resolver,&stats)&&stats.CacheHits==1&&stats.CachedResults==1);
    assert(xrtOwnershipScopeEnd(&freeze));assert(xrtNetResolverClear(resolver));
    idle_freeze(resolver,&freeze);assert(ref.Ops->Count(ref.Data,&count)&&count==4);assert(xrtOwnershipScopeEnd(&freeze));
    assert(xrtNetResolverDestroy(resolver));
    for(unsigned i=0;i<2;++i){xrtNetResolveOpDestroy(operations[i]);xrtNetAddrListDestroy(lists[i]);}balanced(&before);
}
int main(int argc,char** argv)
{
    const char* selected=argc>1?argv[1]:NULL;
    for(unsigned i=0;i<8;++i){
        if(!selected||!strcmp(selected,"idle")){idle(1,false);idle(2,true);idle(4,false);}
        if(!selected||!strcmp(selected,"queued"))queued(1,false,false,i==0);
        if(!selected||!strcmp(selected,"cancel"))queued(3,true,false,false);
        if(!selected||!strcmp(selected,"cancel-all"))queued(1,true,false,false);
        if(!selected||!strcmp(selected,"resurrect"))queued(2,false,true,false);
        if(!selected||!strcmp(selected,"opaque")){opaque();opaque_request();}
        if(!selected||!strcmp(selected,"active"))active();
        if(!selected||!strcmp(selected,"leaf"))leaf();
        if(!selected||!strcmp(selected,"cache"))cache_shared();
    }
    if(!selected||!strcmp(selected,"create-oom"))create_oom();
    if(!selected||!strcmp(selected,"submit-oom"))submit_oom();
    if(!selected)assert(oom_failures&&resurrect_cases==8);
    testMemoryDebugDrain("resolver ownership balance");
    printf("resolver graph: idle=%u queued=%u opaque=%u active=%u resurrected=%u allocation-budgets=%u failures=%u\n",idle_cases,queue_cases,opaque_cases,active_cases,resurrect_cases,oom_cases,oom_failures);
    return 0;
}
