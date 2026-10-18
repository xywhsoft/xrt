#include "../test.h"
#include "../test_thread.h"

typedef struct ownedcontext {
    int refs, notifications, releases;
    bool cleared;
    xchannelsubscription* subscription;
} ownedcontext;

static void outside_scope(void)
{
    xrtownershipscope freeze={0};
    testRequire(xrtOwnershipFreezeTryBegin(&freeze),"owned notification/Drop inherited an API mutation");
    testRequire(xrtOwnershipScopeEnd(&freeze),"owned callback freeze end failed");
}
static bool context_count(const void* data,size_t* count)
{
    const ownedcontext* context=data;
    if(!context || !count || context->refs<=0 || context->cleared) return false;
    *count=(size_t)context->refs;
    return true;
}
static bool context_trace(const void* data,xrtownershipvisitor visit,ptr opaque)
{
    const ownedcontext* context=data;
    return !context->subscription || visit(xrtChannelSubscribeOwnership(context->subscription),opaque);
}
static const xrtownershipops context_ops={context_count,context_trace};
static void context_drop(const void* data)
{
    ownedcontext* context=(ownedcontext*)data;
    xrtownershipscope mutation={0};
    outside_scope();
    testRequire(xrtOwnershipMutationBegin(&mutation),"context mechanical release admission failed");
    testRequire(context->refs>0,"context reference underflow");
    --context->refs;
    ++context->releases;
    testRequire(xrtOwnershipScopeEnd(&mutation),"context mechanical release end failed");
}
static void notify(xchannelsubscription* subscription,ptr data)
{
    ownedcontext* context=data;
    xrtownershipscope freeze={0};
    const xrtownershipquiescencev1* quiescence=xrtChannelSubscribeOwnershipQuiescenceV1Get();
    xrtownershipref ref=xrtChannelSubscribeOwnership(subscription);
    size_t untouched=77u;
    outside_scope();
    testRequire(++context->notifications==1,"owned callback duplicated");
    testRequire(xrtOwnershipFreezeTryBegin(&freeze),"owned callback freeze failed");
    testRequire(quiescence->Ops==ref.Ops && quiescence->Busy(ref.Data),"active dispatcher not reported busy");
    testRequire(!ref.Ops->Count(ref.Data,&untouched) && untouched==77u,"active count was exposed");
    testRequire(xrtOwnershipScopeEnd(&freeze),"active callback freeze end failed");
}
static const xchannelsubscribeownershipv1 policy={sizeof(policy),notify,context_drop,&context_ops};
static const xchannelsubscribepolicyv1 raw_policy={sizeof(raw_policy),notify,context_drop};

static bool admit(xrtownershipref ref,ptr unused)
{
    (void)unused;
    const xchannelsubscribeownershipv1* policies[]={&policy};
    return ref.Ops==&context_ops ||
        xrtChannelSubscribeOwnershipAdapterV1(ref,policies,1u)!=NULL;
}
static void inspect(ownedcontext* context,bool rooted)
{
    xrtownershipscope freeze={0};
    xrtownershipsnapshot* snapshot=NULL;
    xrtownershipref anchor={context,&context_ops};
    testRequire(xrtOwnershipFreezeTryBegin(&freeze),"physical graph freeze failed");
    testRequire(xrtOwnershipSnapshotCreate(&anchor,1u,NULL,0u,admit,NULL,&snapshot),"closed owned graph rejected");
    testRequire(xrtOwnershipSnapshotNodeCount(snapshot)==2u,"borrowed Channel/cases invented ownership nodes");
    for(size_t i=0;i<2u;i++) {
        xrtownershipnode node;
        testRequire(xrtOwnershipSnapshotNode(snapshot,i,&node),"physical node query failed");
        testRequire(node.InternalCount==1u && node.Reachable==rooted,"physical Data/external/registration rights miscounted");
        testRequire(node.StrongCount==(node.Reference.Data==context ? 1u : rooted ? 2u : 1u),"native execution credit is not exact");
    }
    xrtOwnershipSnapshotDestroy(snapshot);
    testRequire(xrtOwnershipScopeEnd(&freeze),"physical graph freeze end failed");
}

static void collect_cycle(ownedcontext* context)
{
    const xchannelsubscribeownershipv1* policies[]={&policy};
    xrtownershipref ref=xrtChannelSubscribeOwnership(context->subscription);
    xrtownershipscope freeze={0},mutation={0};
    const xrtownershipadapterv1* adapter;
    int token=0,foreign=1;
    testRequire(xrtOwnershipFreezeTryBegin(&freeze),"cycle claim freeze failed");
    adapter=xrtChannelSubscribeOwnershipAdapterV1(ref,policies,1u);
    testRequire(adapter && adapter->Hold(ref.Data) && adapter->Claim(ref.Data,&token),"settled subscription cannot be claimed");
    ++context->refs; /* One actual planner hold on the physical context. */
    testRequire(!adapter->Claim(ref.Data,&foreign),"foreign claim accepted");
    testRequire(xrtOwnershipScopeEnd(&freeze),"cycle claim end failed");
    testRequire(!xrtChannelSubscribeRef(context->subscription) &&
        !xrtChannelSubscribeStart(context->subscription) &&
        !xrtChannelSubscribeStop(context->subscription,XWAIT_CANCELLED),"claimed raw admission not quarantined");
    xrtClearError();
    testRequire(xrtOwnershipFreezeTryBegin(&freeze),"cycle clear freeze failed");
    adapter->Clear(ref.Data,&token);
    context->cleared=true;
    testRequire(context->refs==2 && context->subscription && !context->releases,"Clear ran Drop or erased owning slots early");
    testRequire(xrtOwnershipScopeEnd(&freeze),"cycle clear end failed");
    testRequire(adapter->Finish(ref.Data,&token) && adapter->Finish(ref.Data,&token),"mechanical Finish not retry-safe");
    testRequire(context->refs==1 && context->releases==1,"Finish did not consume exactly one Data reference");
    testRequire(xrtOwnershipMutationBegin(&mutation),"context slot detach failed");
    xchannelsubscription* subscription=context->subscription;
    context->subscription=NULL;
    testRequire(xrtOwnershipScopeEnd(&mutation),"context slot detach end failed");
    xrtChannelSubscribeDestroy(subscription); /* Returns context's one external subscription right. */
    adapter->Drop(ref.Data); /* Returns the planner's independent native hold. */
    testRequire(context->refs==1 && context->releases==1,"last native hold repeated Data Drop");
    context_drop(context);
    testRequire(!context->refs,"planner context hold leaked");
}

static void test_owned(bool start)
{
    xchannel channel;
    ownedcontext context={.refs=1};
    xchannelsubscribecase item={&channel,XCHANNEL_OP_RECV,NULL};
    testRequire(xrtChannelInit(&channel,0u),"owned channel init failed");
    context.subscription=xrtChannelSubscribeCreateOwnedV1(&item,1u,&context,&policy);
    testRequire(context.subscription!=NULL,"owned subscription create failed");
    inspect(&context,false);
    {
        xchannelsubscribeownershipv1 copy=policy;
        const xchannelsubscribeownershipv1* wrong[]={&copy};
        xrtownershipscope freeze={0};
        xrtownershipref ref=xrtChannelSubscribeOwnership(context.subscription);
        testRequire(xrtOwnershipFreezeTryBegin(&freeze),"policy identity freeze failed");
        testRequire(!xrtChannelSubscribeOwnershipAdapterV1(ref,wrong,1u),"structurally equal unknown policy admitted");
        const xchannelsubscribeownershipv1* allowed[]={&policy};
        const xrtownershipadapterv1* adapter=xrtChannelSubscribeOwnershipAdapterV1(ref,allowed,1u);
        int token=0;
        testRequire(adapter && adapter->Hold(ref.Data) && adapter->Claim(ref.Data,&token),"dormant claim failed");
        adapter->Restore(ref.Data,&token);
        testRequire(xrtOwnershipScopeEnd(&freeze),"restore freeze end failed");
        adapter->Drop(ref.Data);
    }
    if(start) {
        testRequire(xrtChannelSubscribeStart(context.subscription),"owned Start failed");
        inspect(&context,true); /* The accepted registration is a real execution root. */
        xrtownershipscope freeze={0};
        int token=0;
        const xchannelsubscribeownershipv1* allowed[]={&policy};
        xrtownershipref ref=xrtChannelSubscribeOwnership(context.subscription);
        testRequire(xrtOwnershipFreezeTryBegin(&freeze),"pending claim probe freeze failed");
        const xrtownershipadapterv1* adapter=xrtChannelSubscribeOwnershipAdapterV1(ref,allowed,1u);
        testRequire(adapter && !adapter->Claim(ref.Data,&token),"collector may silently cancel pending accepted work");
        testRequire(xrtOwnershipScopeEnd(&freeze),"pending claim probe end failed");
        testRequire(xrtChannelTrySend(&channel,(ptr)(uintptr_t)71u)==XCHANNEL_OK,"owned rendezvous failed");
        testRequire(context.notifications==1 && !context.releases,"Ready prematurely dropped retained Data");
        inspect(&context,false);
    }
    collect_cycle(&context);
    testRequire(context.notifications==(start ? 1 : 0),"collection invented/skipped notification");
    testRequire(xrtChannelUnit(&channel),"owned cycle left a registered waiter");
}

static void test_raw_refusal(void)
{
    xchannel channel;
    ownedcontext context={.refs=1};
    xchannelsubscribecase item={&channel,XCHANNEL_OP_RECV,NULL};
    const xchannelsubscribeownershipv1* allowed[]={&policy};
    testRequire(xrtChannelInit(&channel,0u),"raw channel init failed");
    context.subscription=xrtChannelSubscribeCreateV1(&item,1u,&context,&raw_policy);
    testRequire(context.subscription!=NULL,"raw create failed");
    xrtownershipref ref=xrtChannelSubscribeOwnership(context.subscription);
    xrtownershipscope freeze={0};
    size_t unchanged=71u;
    testRequire(xrtOwnershipFreezeTryBegin(&freeze),"raw inspection freeze failed");
    testRequire(!ref.Ops->Count(ref.Data,&unchanged) && unchanged==71u &&
        !xrtChannelSubscribeOwnershipAdapterV1(ref,allowed,1u),"raw observer falsely certified");
    testRequire(xrtOwnershipScopeEnd(&freeze),"raw inspection end failed");
    xrtChannelSubscribeDestroy(context.subscription);
    testRequire(context.releases==1 && !context.refs && !context.notifications,"raw constructor cleanup changed");
    testRequire(xrtChannelUnit(&channel),"raw unit failed");
}

typedef struct frozenref {
    xchannelsubscription* subscription;
    xevent armed,done;
} frozenref;
static int reference_worker(ptr data)
{
    frozenref* work=data;
    if(!xrtEventSet(&work->armed)) return 1;
    xchannelsubscription* held=xrtChannelSubscribeRef(work->subscription);
    if(!held) return 2;
    if(!xrtEventSet(&work->done)) return 3;
    xrtChannelSubscribeDestroy(held);
    return 0;
}
static void test_mutation_domain(void)
{
    xchannel channel;
    ownedcontext context={.refs=1};
    xchannelsubscribecase item={&channel,XCHANNEL_OP_RECV,NULL};
    frozenref work={0};
    testthread thread={.Proc=reference_worker,.Data=&work};
    xrtownershipscope freeze={0};
    size_t count=0;
    testRequire(xrtChannelInit(&channel,0u) && xrtEventInit(&work.armed,true,false) &&
        xrtEventInit(&work.done,true,false),"freeze worker setup failed");
    context.subscription=xrtChannelSubscribeCreateOwnedV1(&item,1u,&context,&policy);
    testRequire(context.subscription!=NULL,"freeze subscription create failed");
    work.subscription=context.subscription;
    xrtownershipref ref=xrtChannelSubscribeOwnership(context.subscription);
    testRequire(xrtOwnershipFreezeTryBegin(&freeze),"cross-thread freeze failed");
    testThreadsStart(&thread,1u);
    testRequire(xrtEventWaitFor(&work.armed,2000)==XWAIT_OK,"reference worker did not enter");
    xrtSleep(5);
    testRequire(xrtEventTryWait(&work.done)==XWAIT_TIMEOUT && ref.Ops->Count(ref.Data,&count) && count==1u,
        "reference transition escaped the complete ownership freeze");
    testRequire(xrtOwnershipScopeEnd(&freeze),"cross-thread freeze end failed");
    testThreadsJoin(&thread,1u);
    testRequire(thread.Result==0 && xrtEventTryWait(&work.done)==XWAIT_OK,"reference transition did not resume");
    xrtChannelSubscribeDestroy(context.subscription);
    testRequire(!context.refs && context.releases==1,"cross-thread public reference leaked");
    testRequire(xrtChannelUnit(&channel) && xrtEventUnit(&work.armed) &&
        xrtEventUnit(&work.done),"cross-thread freeze resources leaked");
}

static void test_owned_failure_prefix(void)
{
    #if defined(XRT_FEATURE_MEMORY_DEBUG)
    xchannel channel;
    ownedcontext context={.refs=1};
    xchannelsubscribecase item={&channel,XCHANNEL_OP_RECV,NULL};
    xmemdebugsnapshot before,after;
    testRequire(xrtChannelInit(&channel,0u),"owned OOM channel init failed");
    xrtMemDebugSnapshot(&before);
    for(uint64 i=0;i<2u;i++) {
        testRequire(xrtMemDebugFailAfter(i),"owned OOM injection failed");
        xchannelsubscription* result=xrtChannelSubscribeCreateOwnedV1(&item,1u,&context,&policy);
        testRequire(!result && xrtMemDebugFailTriggered() && context.refs==1 &&
            !context.releases && !context.notifications,"failed owned constructor consumed Data");
        xrtMemDebugFailClear(); xrtClearError();
        xrtMemDebugSnapshot(&after);
        testRequire(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes,"owned constructor failed to roll back");
    }
    context_drop(&context);
    testRequire(xrtChannelUnit(&channel),"owned OOM residual node");
    #endif
}

int main(void)
{
    test_owned(false);
    test_owned(true);
    test_raw_refusal();
    test_mutation_domain();
    test_owned_failure_prefix();
    puts("[PASS] subscription exact physical graph, pending execution root, policy refusal and settled cycle retirement");
    return 0;
}
