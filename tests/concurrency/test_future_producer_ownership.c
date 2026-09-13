#ifdef FUTURE_PRODUCER_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"
#include <assert.h>

typedef struct Producer {
    size_t refs;
    xfuture* borrowedFuture;
    xpromise* ownedPromise;
    unsigned drops, counts, traces, notifications, releases;
    bool cleared;
} Producer;
static unsigned refused, probes, completions, cycles;
static bool count(const void* data, size_t* result)
{ Producer* p=(Producer*)data; ++p->counts; if(!p->refs)return false; *result=p->refs; return true; }
static bool trace(const void* data, xrtownershipvisitor visit, ptr context)
{ Producer* p=(Producer*)data; ++p->traces; return visit(xrtPromiseOwnership(p->ownedPromise),context); }
static const xrtownershipops ops={count,trace};
static xrtownershipref reference(Producer* p) { return (xrtownershipref){p,&ops}; }
static bool no_producer(xrtownershipref child, ptr context)
{ assert(child.Data!=context); return true; }
static int32 probe(ptr data)
{
    Producer* p=data; xrtownershipscope freeze={0}; bool acquired=false;
    for(unsigned n=0;n<1000000 && !(acquired=xrtOwnershipFreezeTryBegin(&freeze));++n)xrtThreadYield();
    assert(acquired && xrtFutureState(p->borrowedFuture)!=XFUTURE_PENDING);
    xrtownershipref future=xrtFutureOwnership(p->borrowedFuture);
    if(p->cleared){size_t n=0;assert(!future.Ops->Count(future.Data,&n));}
    else assert(future.Ops->Trace(future.Data,no_producer,p));
    assert(xrtOwnershipScopeEnd(&freeze)); return 0;
}
static void drop(const void* data)
{
    Producer* p=(Producer*)data; xpromise* owned; xrtownershipscope mutation={0};
    assert(xrtOwnershipMutationBegin(&mutation) && p->refs && !p->drops);
    --p->refs; ++p->drops; owned=p->ownedPromise; p->ownedPromise=NULL;
    assert(xrtOwnershipScopeEnd(&mutation));
    /* Another native thread takes the graph freeze AND the Future lock while
     * this actual producer release is on the stack. No enclosing scope is
     * suspended. The endpoint/pin still owns the Future through this tail. */
    xthread* worker=xrtThreadCreate(probe,p,0); assert(worker);
    assert(xrtThreadWaitFor(worker,UINT64_C(5000000))==XWAIT_OK); xrtThreadDestroy(worker); ++probes;
    xrtPromiseDestroy(owned);
    xrtSetErrorKind(XERR_ARGUMENT); /* Mechanical Drop must not replace caller error. */
}
static const xfutureproducerownershipv1 policy={sizeof(policy),drop};
static const xfutureproducerownershipv1 other={sizeof(other),drop};
static const xfutureproducerownershipv1* const allowed[]={&policy};
static const xfutureproducerownershipv1* const wrong[]={&other};
static void notify(ptr data)
{ Producer* p=data; assert(p->drops==1 && p->refs==1); ++p->notifications; }
static void release_watch(ptr data) { ++((Producer*)data)->releases; }
static bool poison_watch(const void* data, xrtownershipvisitor visit, ptr context)
{ (void)data;(void)visit;(void)context;assert(!"unapproved waiter trace");return false; }
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtClearError(); xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static void refuse(xpromise* promise, xrtownershipref ref, const xfutureproducerownershipv1* selected, Producer* p)
{
    assert(!xrtPromiseProducerBindTakeV1(promise,ref,selected) && xrtGetError());
    assert(!p->drops && !p->counts && !p->traces); xrtClearError(); ++refused;
}
static void boundaries(void)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xfuture* future=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL); assert(promise);
    Producer p={0}; p.refs=2; p.borrowedFuture=future;
    xfutureproducerownershipv1 badSize={0,drop}, badDrop={sizeof(badDrop),NULL};
    xrtownershipops noCount={NULL,trace}, noTrace={count,NULL};
    refuse(NULL,reference(&p),&policy,&p);
    refuse(promise,(xrtownershipref){0},&policy,&p);
    refuse(promise,(xrtownershipref){&p,NULL},&policy,&p);
    refuse(promise,(xrtownershipref){&p,&noCount},&policy,&p);
    refuse(promise,(xrtownershipref){&p,&noTrace},&policy,&p);
    refuse(promise,reference(&p),NULL,&p);
    refuse(promise,reference(&p),&badSize,&p);
    refuse(promise,reference(&p),&badDrop,&p);
    xfuture* alias=xrtFutureRef(future); assert(alias); refuse(promise,reference(&p),&policy,&p); xrtFutureDestroy(alias);
    xpromise* producerAlias=xrtPromiseRef(promise); assert(producerAlias);
    refuse(promise,reference(&p),&policy,&p); xrtPromiseDestroy(producerAlias);
    xfuturewatch watch={0}; assert(xrtFutureWatchInitPhased(&watch,notify,release_watch,&p,poison_watch));
    assert(xrtFutureWatchAdd(future,&watch)==XFUTURE_WATCH_PENDING);
    refuse(promise,reference(&p),&policy,&p); xrtFutureWatchRemove(future,&watch);
    assert(p.releases==1 && !p.notifications);
    assert(xrtMemDebugFailAfter(0));
    assert(xrtPromiseProducerBindTakeV1(promise,reference(&p),&policy));
    assert(!xrtMemDebugFailTriggered() && !p.counts && !p.traces); xrtMemDebugFailClear();
    refuse(promise,reference(&p),&other,&p);
    xrtownershipscope freeze={0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    xrtownershipref root=xrtFutureOwnership(future);
    assert(!xrtFutureOwnershipAdapterV1(root,NULL,0));
    assert(!xrtFutureOwnershipAdapterV2(root,NULL,0,wrong,1));
    assert(!xrtFutureOwnershipAdapterV2(root,NULL,0,NULL,1));
    assert(xrtFutureOwnershipAdapterV2(root,NULL,0,allowed,1));
    assert(!p.counts && !p.traces); /* Admission does not certify or trace child. */
    assert(xrtOwnershipScopeEnd(&freeze));
    assert(xrtPromiseClose(promise) && p.drops==1 && p.refs==1);
    /* Failed bind after a terminal result consumes no second reference. */
    Producer terminal={0}; terminal.refs=1;
    refuse(promise,reference(&terminal),&policy,&terminal);
    --p.refs; --terminal.refs; xrtPromiseDestroy(promise); xrtFutureDestroy(future); balance(&before);
}
static void terminal(unsigned mode)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xfuture* future=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL); assert(promise);
    Producer p={0}; p.refs=2; p.borrowedFuture=future;
    assert(xrtPromiseProducerBindTakeV1(promise,reference(&p),&policy));
    xfuturewatch watch={0}; assert(xrtFutureWatchInitPhased(&watch,notify,release_watch,&p,poison_watch));
    assert(xrtFutureWatchAdd(future,&watch)==XFUTURE_WATCH_PENDING);
    xrtownershipscope freeze={0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(!xrtFutureOwnershipAdapterV2(xrtFutureOwnership(future),NULL,0,allowed,1));
    assert(!p.counts && !p.traces); assert(xrtOwnershipScopeEnd(&freeze));
    xrtSetErrorKind(XERR_STATE); const xerror* previous=xrtGetError(); assert(previous);
    if(mode==0)assert(xrtPromiseResolve(promise,NULL));
    else if(mode==1)assert(xrtPromiseReject(promise,previous));
    else if(mode==2)assert(xrtPromiseCancel(promise));
    else if(mode==3)assert(xrtPromiseClose(promise));
    else {xrtPromiseDestroy(promise); promise=NULL;}
    assert(xrtGetError()==previous && p.drops==1 && p.refs==1 && p.notifications==1 && p.releases==1);
    assert(xrtFutureState(future)==(mode==0?XFUTURE_RESOLVED:mode==1?XFUTURE_FAILED:mode==2?XFUTURE_CANCELLED:XFUTURE_CLOSED));
    xrtClearError();
    if(promise){assert(!xrtPromiseResolve(promise,NULL) && p.drops==1);xrtClearError();}
    --p.refs; xrtPromiseDestroy(promise); xrtFutureDestroy(future); ++completions; balance(&before);
}
static void producer_cycle(void)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xfuture* future=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL); assert(promise);
    Producer p={0}; p.refs=1; p.borrowedFuture=future;
    /* Transfer the existing Promise endpoint to the producer, not a borrowed
     * pseudo-edge or a duplicated endpoint count. Bind transfers its real ref. */
    p.ownedPromise=promise; assert(xrtPromiseProducerBindTakeV1(promise,reference(&p),&policy));
    xrtFutureDestroy(future);
    xrtownershipscope freeze={0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    xrtownershipref root=xrtFutureOwnership(future); xrtownershipresult graph={0};
    assert(xrtOwnershipInspect(&root,1,NULL,0,&graph));
    assert(graph.NodeCount==3 && graph.EdgeCount==3 && graph.ExternalRootCount==0);
    const xrtownershipadapterv1* adapter=xrtFutureOwnershipAdapterV2(root,NULL,0,allowed,1); assert(adapter);
    assert(adapter->Hold(root.Data)); ++p.refs; /* Real child pin held until Finish returns. */
    assert(adapter->Claim(root.Data,&graph) && !adapter->Claim(root.Data,&p));
    adapter->Restore(root.Data,&graph); assert(adapter->Claim(root.Data,&graph));
    adapter->Clear(root.Data,&graph); p.cleared=true;
    assert(!p.drops && p.refs==2 && p.ownedPromise==promise);
    assert(!xrtFutureOwnershipAdapterV2(root,NULL,0,allowed,1));
    assert(xrtOwnershipScopeEnd(&freeze));
    xrtSetErrorKind(XERR_STATE); const xerror* previous=xrtGetError();
    assert(adapter->Finish(root.Data,&graph) && adapter->Finish(root.Data,&graph));
    assert(p.drops==1 && p.refs==1 && !p.ownedPromise && xrtGetError()==previous);
    --p.refs; adapter->Drop(root.Data); ++cycles; balance(&before);
}
static unsigned watch_cycles,watch_probes;
static void certified_notify(ptr data)
{
    Producer* p=data;assert(p->refs==2&&!p->notifications&&!p->releases);
    assert(xrtFutureState(p->borrowedFuture)==XFUTURE_CLOSED);++p->notifications;
    xthread* worker=xrtThreadCreate(probe,p,0);assert(worker);
    assert(xrtThreadWaitFor(worker,5000000)==XWAIT_OK);xrtThreadDestroy(worker);++watch_probes;
}
static void certified_release(ptr data)
{
    Producer* p=data;xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));
    assert(p->refs==2&&p->notifications==1&&!p->releases);--p->refs;++p->releases;
    assert(xrtOwnershipScopeEnd(&mutation));
}
static const xfuturewatchownershipv1 watch_policy={sizeof(watch_policy),certified_notify,certified_release,&ops};
static const xfuturewatchownershipv1 other_watch={sizeof(other_watch),certified_notify,certified_release,&ops};
static void watch_preparation(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    Producer p={0};xfuture* future=NULL;xpromise* promise=xrtPromiseCreate(&future,NULL);assert(promise);
    p.refs=1;p.borrowedFuture=future;p.ownedPromise=promise;
    xfuturewatch watch={0};const xfuturewatchownershipv1* approved[]={&watch_policy};
    const xfuturewatchownershipv1* unknown[]={&other_watch};
    assert(xrtFutureWatchInitOwnershipV1(&watch,&p,&watch_policy));
    assert(xrtFutureWatchAdd(future,&watch)==XFUTURE_WATCH_PENDING);xrtFutureDestroy(future);
    xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
    xrtownershipref root=xrtFutureOwnership(future);const xrtownershippreparationv1* prep=NULL;
    assert(!xrtFutureOwnershipAdapterV1(root,NULL,0));
    assert(!xrtFutureOwnershipAdapterV2(root,NULL,0,NULL,0));
    assert(!xrtFutureOwnershipAdapterV3(root,NULL,0,NULL,0,unknown,1,&prep)&&!prep);
    assert(!xrtFutureOwnershipAdapterV3(root,NULL,0,NULL,0,NULL,1,&prep)&&!prep);
    assert(!p.counts&&!p.traces&&!p.notifications&&!p.releases);
    const xrtownershipadapterv1* adapter=xrtFutureOwnershipAdapterV3(root,NULL,0,NULL,0,approved,1,&prep);
    assert(adapter&&prep&&prep->Adapter==adapter&&!prep->Ready(root.Data));
    xrtownershipresult graph={0};assert(xrtOwnershipInspect(&root,1,NULL,0,&graph));
    assert(graph.NodeCount==3&&graph.EdgeCount==3&&!graph.ExternalRootCount);
    assert(adapter->Hold(root.Data));++p.refs; /* Actual independent child pin. */
    assert(adapter->Claim(root.Data,&graph));adapter->Restore(root.Data,&graph);
    assert(adapter->Claim(root.Data,&graph));assert(xrtOwnershipScopeEnd(&freeze));
    assert(prep->Prepare(root.Data,&graph)==XRT_OWNERSHIP_PREPARE_READY);
    assert(p.notifications==1&&p.releases==1&&p.refs==1);
    assert(xrtOwnershipFreezeTryBegin(&freeze));
    const xrtownershippreparationv1* again=NULL;
    assert(xrtFutureOwnershipAdapterV3(root,NULL,0,NULL,0,approved,1,&again)==adapter&&again==prep&&prep->Ready(root.Data));
    adapter->Clear(root.Data,&graph);assert(xrtOwnershipScopeEnd(&freeze));
    assert(adapter->Finish(root.Data,&graph)&&adapter->Finish(root.Data,&graph));
    --p.refs;xrtPromiseDestroy(p.ownedPromise);p.ownedPromise=NULL;adapter->Drop(root.Data);
    ++watch_cycles;balance(&before);
}
int main(void)
{
    testRequire(xrtMemDebugEnable(true), "enable producer allocation accounting"); xrtSetErrorKind(XERR_STATE); xrtClearError();
    for(unsigned n=0;n<100;++n){boundaries();for(unsigned m=0;m<5;++m)terminal(m);producer_cycle();watch_preparation();}
    assert(refused==1300 && completions==500 && cycles==100 && probes==700);
    puts("Future producer ownership: 1300 no-consume refusals, 100 allocation-free transfers, 500 terminal-before-notify releases, 100 physical cycles, 700 cross-thread lock/freeze probes; exact policy admission, restore/clear/repeat Finish and zero live delta");
    assert(watch_cycles==100&&watch_probes==100);
    puts("Certified Watch preparation: 100 actual pending cycles, exact pre-trace policy refusals, 100 CLOSED notifications before release, stable preparation identity, 100 cross-thread freezes, restore/clear/repeat Finish and zero live delta");
    return 0;
}
