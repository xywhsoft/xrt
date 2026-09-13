#ifndef XRT_MODULE_FUTURE_COMBINE
#define XRT_MODULE_FUTURE_COMBINE
#endif
#ifndef XRT_MODULE_FUTURE_CONTINUE
#define XRT_MODULE_FUTURE_CONTINUE
#endif
#ifndef XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_VALUE_GRAPH
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#ifdef FUTURE_SCOPE_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"
#include "../test_thread.h"

static bool scope_empty(const xrtownershipscope* scope)
{
    return scope->Self == NULL && scope->Parent == NULL && scope->Thread == 0 &&
        scope->Children == 0 && scope->Mode == 0;
}
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    testRequire(xrtGetError() == NULL, "no residual async-scope error");
    xrtMemDebugSnapshot(&after);
    testRequire(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
        after.AllocCount-before->AllocCount == after.FreeCount-before->FreeCount &&
        before->InvalidFreeCount == after.InvalidFreeCount &&
        before->DoubleFreeCount == after.DoubleFreeCount, "async scope exact allocator balance");
}
static bool no_edges(const void* data, xrtownershipvisitor visit, ptr context)
{ (void)data; (void)visit; (void)context; return true; }
static bool no_payload_edges(const void* value, const void* data, xrtownershipvisitor visit, ptr context)
{ (void)value; return no_edges(data, visit, context); }
static void value_drop(ptr value, ptr data)
{ (void)data; xrtValueRelease(value); }
static bool value_trace(const void* value, const void* data, xrtownershipvisitor visit, ptr context)
{ return data == NULL && visit(xrtValueOwnership(value), context); }

typedef struct transition {
    unsigned Kind;
    xfuture *Future, *Source;
    xpromise* Promise;
    xvalue *Value, *WatchValue;
    xerror* Error;
    xfuturewatch Watch;
    xatomic32 Stage;
    unsigned Releases;
} transition;
static void transition_notify(ptr data) { (void)data; testRequire(false, "a removed watch cannot notify"); }
static bool transition_trace(const void* data, xrtownershipvisitor visit, ptr context)
{ return visit(xrtValueOwnership(((const transition*)data)->WatchValue), context); }
static void transition_release(ptr data)
{
    transition* work=data; ++work->Releases;
    xrtValueRelease(work->WatchValue); work->WatchValue=NULL;
}
static int publish_transition(ptr data)
{
    transition* work=data; xrtownershipscope refused={0};
    testRequire(!xrtOwnershipFreezeTryBegin(&refused) && scope_empty(&refused), "native writer observes an existing freeze");
    xrtAtomic32Store(&work->Stage,1,XMEMORY_RELEASE);
    switch(work->Kind) {
        case 0:
            testRequire(xrtPromiseResolveOwnedTraced(work->Promise,work->Value,value_drop,NULL,value_trace), "traced payload commit");
            work->Value=NULL; break;
        case 1: testRequire(xrtPromiseReject(work->Promise,work->Error), "error commit"); break;
        case 2: testRequire(xrtPromiseForward(work->Promise,work->Source), "forward owner commit"); break;
        case 3: testRequire(xrtPromiseCancel(work->Promise), "cancel terminal commit"); break;
        case 4: testRequire(xrtPromiseClose(work->Promise), "closed terminal commit"); break;
        case 5: xrtPromiseDestroy(work->Promise); work->Promise=NULL; break;
        case 6:
            testRequire(xrtPromiseRef(work->Promise)==work->Promise,"producer alias"); xrtPromiseDestroy(work->Promise); break;
        case 7: testRequire(xrtFutureWatchAdd(work->Future,&work->Watch)==XFUTURE_WATCH_PENDING,"watch insertion"); break;
        case 8: testRequire(xrtFutureWatchDetach(work->Future,&work->Watch),"watch detachment"); break;
        case 9: xrtFutureWatchRemove(work->Future,&work->Watch); break;
        default: testRequire(false,"unexpected transition");
    }
    testRequire(xrtGetError()==NULL,"native transition completed without diagnostic");
    xrtAtomic32Store(&work->Stage,2,XMEMORY_RELEASE); return 0;
}
static void frozen_transition(unsigned kind)
{
    static const xfuturestate states[10]={XFUTURE_RESOLVED,XFUTURE_FAILED,XFUTURE_RESOLVED,
        XFUTURE_CANCELLED,XFUTURE_CLOSED,XFUTURE_CLOSED,XFUTURE_PENDING,XFUTURE_PENDING,XFUTURE_PENDING,XFUTURE_PENDING};
    transition Work={0}; testthread Thread={0}; xrtownershipscope Freeze={0};
    xmemdebugsnapshot Before; xrtownershipresult Graph={0}, During={0};
    xrtownershipref anchor; xpromise* sourcePromise;
    xrtMemDebugSnapshot(&Before); Work.Kind=kind;
    Work.Promise=xrtPromiseCreate(&Work.Future,NULL); sourcePromise=xrtPromiseCreate(&Work.Source,NULL);
    Work.Value=xrtValueInt(17000); Work.Error=xrtErrorCreate(XERR_STATE,"scope",1,"fixture");
    testRequire(Work.Promise && sourcePromise && Work.Value && Work.Error,"transition fixtures");
    testRequire(xrtPromiseResolveOwnedTraced(sourcePromise,xrtValueRetain(Work.Value),value_drop,NULL,value_trace),"source payload");
    xrtPromiseDestroy(sourcePromise);
    if(kind>=7) {
        Work.WatchValue=xrtValueRetain(Work.Value);
        testRequire(Work.WatchValue && xrtFutureWatchInitTraced(&Work.Watch,transition_notify,transition_release,&Work,transition_trace),"traced owning waiter");
        if(kind!=7) testRequire(xrtFutureWatchAdd(Work.Future,&Work.Watch)==XFUTURE_WATCH_PENDING,"existing waiter");
    }
    xrtAtomic32Init(&Work.Stage,0); Thread.Proc=publish_transition; Thread.Data=&Work;
    anchor=xrtFutureOwnership(Work.Future);
    testRequire(xrtOwnershipFreezeTryBegin(&Freeze) && xrtOwnershipInspect(&anchor,1,NULL,0,&Graph),"freeze known future graph");
    testThreadsStart(&Thread,1);
    while(xrtAtomic32Load(&Work.Stage,XMEMORY_ACQUIRE)==0) testThreadYield();
    for(unsigned i=0;i<2000;++i) testThreadYield();
    testRequire(xrtAtomic32Load(&Work.Stage,XMEMORY_ACQUIRE)==1,"entire native Future/Promise/watch transition waits outside freeze");
    testRequire(xrtFutureState(Work.Future)==XFUTURE_PENDING &&
        xrtOwnershipInspect(&anchor,1,NULL,0,&During) && !memcmp(&Graph,&During,sizeof(Graph)),
        "frozen state and physical owning edges remain unchanged");
    testRequire(xrtOwnershipScopeEnd(&Freeze),"release graph before joining native transition");
    testThreadsJoin(&Thread,1);
    testRequire(Thread.Result==0 && xrtFutureState(Work.Future)==states[kind],"expected complete terminal or waiter state");
    if(kind==7) testRequire(xrtFutureWatchDetach(Work.Future,&Work.Watch),"remove inserted waiter");
    if(kind>=7) testRequire(Work.Releases==1 && Work.WatchValue==NULL,"one owning waiter release");
    xrtPromiseDestroy(Work.Promise); xrtFutureDestroy(Work.Future); xrtFutureDestroy(Work.Source);
    xrtErrorFree(Work.Error); xrtValueRelease(Work.Value); balance(&Before);
}

typedef struct callback_work {
    unsigned Kind;
    xfuture *Future,*Source,*Output,*ProbeFuture;
    xpromise *Promise,*SourcePromise,*Probe;
    xvalue* Value;
    xcancel* Cancel;
    xcancelwatch* CancelWatch;
    xfuturewatch Watch;
    xatomic32 Entered, Continue, Removing, Removed;
    unsigned Calls, Drops;
} callback_work;
static void hold_callback(callback_work* work)
{
    ++work->Calls;
    xrtAtomic32Store(&work->Entered,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&work->Continue,XMEMORY_ACQUIRE)) testThreadYield();
    testRequire(xrtValueRetain(work->Value)==work->Value,"nested Value admission after refused freeze");
    xrtValueRelease(work->Value);
    testRequire(xrtPromiseRef(work->Probe)==work->Probe,"nested producer admission after refused freeze");
    xrtPromiseDestroy(work->Probe);
}
static void notify_hold(ptr data) { hold_callback(data); }
static void notify_noop(ptr data) { (void)data; }
static void release_count(ptr data) { ++((callback_work*)data)->Drops; }
static void release_hold(ptr data) { hold_callback(data); release_count(data); }
static void payload_hold(ptr value,ptr data) { hold_callback(data); xrtValueRelease(value); release_count(data); }
static bool held_payload_trace(const void* value,const void* data,xrtownershipvisitor visit,ptr context)
{ (void)data; return visit(xrtValueOwnership(value),context); }
static void context_drop(ptr data,ptr other) { (void)other; release_count(data); }
static void map_publish(xpromise* output,callback_work* work)
{
    hold_callback(work);
    testRequire(xrtPromiseResolveOwnedTraced(output,xrtValueRetain(work->Value),value_drop,NULL,value_trace),"mapped result publication");
}
static void map_all(const xfutureall* input,xpromise* output,ptr data)
{ testRequire(input && input->Count==1 && xrtFutureDone(input->Futures[0]),"All observes terminal input"); map_publish(output,data); }
static void map_pick(const xfuturepick* input,xpromise* output,ptr data)
{ testRequire(input && input->Index==0 && xrtFutureDone(input->Future),"Any/Race observes terminal input"); map_publish(output,data); }
static void continue_hold(const xfutureresult* input,xpromise* output,ptr data)
{ testRequire(input && input->State==XFUTURE_RESOLVED,"continuation sees terminal input"); hold_callback(data); testRequire(xrtPromiseResolve(output,NULL),"continuation output"); }
static xfuture* mapped_output(callback_work* work)
{
    xfuture* inputs[1]={work->Source};
    switch((work->Kind-5)%3) {
        case 0: return xrtFutureAllMapOwnedTraced(inputs,1,map_all,work,context_drop,NULL,no_payload_edges);
        case 1: return xrtFutureAnyMapOwnedTraced(inputs,1,map_pick,work,context_drop,NULL,no_payload_edges);
        default: return xrtFutureRaceMapOwnedTraced(inputs,1,map_pick,work,context_drop,NULL,no_payload_edges);
    }
}
static int invoke_callback(ptr data)
{
    callback_work* work=data;
    if(work->Kind==0) { xrtFutureDestroy(work->Future); work->Future=NULL; }
    else if(work->Kind==1) testRequire(xrtPromiseResolve(work->Promise,NULL),"notify source");
    else if(work->Kind==2) testRequire(xrtFutureWatchDetach(work->Future,&work->Watch),"release detached context");
    else if(work->Kind==3) testRequire(xrtCancelRequest(work->Cancel),"cancel notifier");
    else if(work->Kind==4) { work->CancelWatch=xrtCancelWatch(work->Cancel,notify_hold,work); testRequire(work->CancelWatch!=NULL,"immediate cancel watch"); }
    else if(work->Kind<=7) { work->Output=mapped_output(work); testRequire(work->Output!=NULL,"immediate mapper factory"); }
    else if(work->Kind<=10 || work->Kind==12) testRequire(xrtPromiseResolve(work->SourcePromise,NULL),"deferred mapper/continuation source");
    else { work->Output=xrtFutureContinueOwned(work->Source,continue_hold,work,context_drop,NULL); testRequire(work->Output!=NULL,"immediate continuation factory"); }
    testRequire(xrtGetError()==NULL,"callback initiator no error"); return 0;
}
static int remove_callback(ptr data)
{
    callback_work* work=data;
    xrtAtomic32Store(&work->Removing,1,XMEMORY_RELEASE);
    if(work->Kind==1) xrtFutureWatchRemove(work->Future,&work->Watch);
    else { xrtCancelUnwatch(work->CancelWatch); work->CancelWatch=NULL; }
    testRequire(xrtGetError()==NULL,"concurrent remover no error");
    xrtAtomic32Store(&work->Removed,1,XMEMORY_RELEASE); return 0;
}
static void in_flight_callback(unsigned kind)
{
    callback_work Work={0}; testthread Thread={0}, Remover={0}; xmemdebugsnapshot Before;
    xrtMemDebugSnapshot(&Before); Work.Kind=kind;
    Work.Value=xrtValueInt(99); Work.Probe=xrtPromiseCreate(&Work.ProbeFuture,NULL);
    Work.Promise=xrtPromiseCreate(&Work.Future,NULL); Work.SourcePromise=xrtPromiseCreate(&Work.Source,NULL);
    Work.Cancel=xrtCancelCreate();
    testRequire(Work.Value && Work.Probe && Work.Promise && Work.SourcePromise && Work.Cancel,"callback fixtures");
    xrtAtomic32Init(&Work.Entered,0); xrtAtomic32Init(&Work.Continue,0);
    xrtAtomic32Init(&Work.Removing,0); xrtAtomic32Init(&Work.Removed,0);
    if(kind==0) {
        testRequire(xrtPromiseResolveOwnedTraced(Work.Promise,xrtValueRetain(Work.Value),payload_hold,&Work,held_payload_trace),"destructor fixture");
        xrtPromiseDestroy(Work.Promise); Work.Promise=NULL;
    } else if(kind==1 || kind==2) {
        testRequire(xrtFutureWatchInitTraced(&Work.Watch,kind==1?notify_hold:notify_noop,
            kind==1?release_count:release_hold,&Work,no_edges) &&
            xrtFutureWatchAdd(Work.Future,&Work.Watch)==XFUTURE_WATCH_PENDING,"callback waiter fixture");
    } else if(kind==3) {
        Work.CancelWatch=xrtCancelWatch(Work.Cancel,notify_hold,&Work);
        testRequire(Work.CancelWatch!=NULL,"cancellation callback fixture");
    } else if(kind==4) testRequire(xrtCancelRequest(Work.Cancel),"already requested token");
    else if(kind<=7 || kind==11) testRequire(xrtPromiseResolve(Work.SourcePromise,NULL),"already terminal input");
    else if(kind<=10) { Work.Output=mapped_output(&Work); testRequire(Work.Output!=NULL,"pending mapped output"); }
    else { Work.Output=xrtFutureContinueOwned(Work.Source,continue_hold,&Work,context_drop,NULL); testRequire(Work.Output!=NULL,"pending continuation output"); }
    Thread.Proc=invoke_callback; Thread.Data=&Work; testThreadsStart(&Thread,1);
    while(!xrtAtomic32Load(&Work.Entered,XMEMORY_ACQUIRE)) testThreadYield();
    if(kind==1 || kind==3) {
        Remover.Proc=remove_callback; Remover.Data=&Work; testThreadsStart(&Remover,1);
        while(!xrtAtomic32Load(&Work.Removing,XMEMORY_ACQUIRE)) testThreadYield();
    }
    for(unsigned i=0;i<100;++i) {
        xrtownershipscope Freeze={0};
        testRequire(!xrtOwnershipFreezeTryBegin(&Freeze) && scope_empty(&Freeze) && xrtGetError()==NULL,
            "no freeze in unfinished callback, destruction, or synchronous factory");
        testThreadYield();
    }
    testRequire(xrtAtomic32Load(&Work.Removed,XMEMORY_ACQUIRE)==0,"removal cannot outrun active callback");
    xrtAtomic32Store(&Work.Continue,1,XMEMORY_RELEASE);
    testThreadsJoin(&Thread,1); testRequire(Thread.Result==0,"callback worker ended");
    if(kind==1 || kind==3) { testThreadsJoin(&Remover,1); testRequire(Remover.Result==0 && xrtAtomic32Load(&Work.Removed,XMEMORY_ACQUIRE)==1,"remover ended after callback"); }
    testRequire(Work.Calls==1 && Work.Drops==((kind<=2 || kind>=5)?1u:0u),"exact callback and release population");
    { xrtownershipscope Freeze={0};
      testRequire(xrtOwnershipFreezeTryBegin(&Freeze) && xrtOwnershipScopeEnd(&Freeze),"no leaked callback/factory scope"); }
    if(kind>=5) testRequire(Work.Output && xrtFutureState(Work.Output)==XFUTURE_RESOLVED,"mapped/continued output remains usable");
    xrtCancelUnwatch(Work.CancelWatch); xrtCancelDestroy(Work.Cancel);
    xrtPromiseDestroy(Work.Promise); xrtFutureDestroy(Work.Future);
    xrtPromiseDestroy(Work.SourcePromise); xrtFutureDestroy(Work.Source); xrtFutureDestroy(Work.Output);
    xrtPromiseDestroy(Work.Probe); xrtFutureDestroy(Work.ProbeFuture); xrtValueRelease(Work.Value);
    balance(&Before);
}

typedef struct waiting_work { xfuture* Future; xatomic32 Started,Done; } waiting_work;
static int wait_natively(ptr data)
{
    waiting_work* work=data; xwaitresult result;
    xrtAtomic32Store(&work->Started,1,XMEMORY_RELEASE);
    result=xrtFutureWait(work->Future);
    testRequire(result==XWAIT_OK,"native blocking wait completes");
    xrtAtomic32Store(&work->Done,1,XMEMORY_RELEASE); return 0;
}
static void pending_wait_does_not_freeze_domain(void)
{
    waiting_work Work={0}; testthread Thread={0}; xmemdebugsnapshot Before;
    xpromise* promise; xrtownershipref view; bool admitted=false;
    xrtMemDebugSnapshot(&Before);
    promise=xrtPromiseCreate(&Work.Future,NULL); testRequire(promise!=NULL,"waiting fixture");
    view=xrtFutureOwnership(Work.Future); xrtAtomic32Init(&Work.Started,0); xrtAtomic32Init(&Work.Done,0);
    Thread.Proc=wait_natively; Thread.Data=&Work; testThreadsStart(&Thread,1);
    while(!xrtAtomic32Load(&Work.Started,XMEMORY_ACQUIRE)) testThreadYield();
    for(unsigned i=0;i<100000 && !admitted;++i) {
        xrtownershipscope Freeze={0}; size_t refs=0;
        if(xrtOwnershipFreezeTryBegin(&Freeze)) {
            testRequire(view.Ops->Count(view.Data,&refs),"frozen waiting count");
            admitted=refs==3; /* producer + caller + real native wait's extra ref */
            testRequire(xrtOwnershipScopeEnd(&Freeze),"end wait observation freeze");
        }
        testThreadYield();
    }
    /* Resolve and join even when admission failed; report liveness after the
     * pending worker can unwind instead of leaving a verification process hung. */
    testRequire(xrtAtomic32Load(&Work.Done,XMEMORY_ACQUIRE)==0,"wait was genuinely pending");
    testRequire(xrtPromiseResolve(promise,NULL),"wake pending native waiter"); testThreadsJoin(&Thread,1);
    testRequire(admitted && Thread.Result==0,"pending wait owns a real root without monopolizing mutation admission");
    xrtPromiseDestroy(promise); xrtFutureDestroy(Work.Future); balance(&Before);
}
int main(void)
{
    testRequire(xrtMemDebugEnable(true),"memory accounting");
    for(unsigned round=0;round<10;++round) {
        for(unsigned kind=0;kind<10;++kind) frozen_transition(kind);
        for(unsigned kind=0;kind<13;++kind) in_flight_callback(kind);
        pending_wait_does_not_freeze_domain();
    }
    testMemoryDebugDrain("future scope balance");
    puts("Future scopes: 100 frozen publication/ref/watch transitions, 130 in-flight callback/factory/destructor boundaries, 20 concurrent removers, 10 real pending native waits; exact graph/allocator balance, no native root registration; not module collection");
    return 0;
}
