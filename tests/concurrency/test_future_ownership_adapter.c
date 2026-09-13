#ifdef FUTURE_ADAPTER_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"
#include <assert.h>

static unsigned drops, traces, notifications;
static void drop_cycle(ptr value, ptr data)
{
    xrtownershipscope freeze = {0};
    assert(data == NULL && xrtOwnershipFreezeTryBegin(&freeze));
    assert(xrtOwnershipScopeEnd(&freeze));
    ++drops; xrtFutureDestroy((xfuture*)value);
}
static bool trace_cycle(const void* value, const void* data, xrtownershipvisitor visit, ptr context)
{ ++traces; return data == NULL && visit(xrtFutureOwnership((const xfuture*)value), context); }
static bool poison(const void* value, const void* data, xrtownershipvisitor visit, ptr context)
{ (void)value; (void)data; (void)visit; (void)context; assert(!"unapproved payload trace called"); return false; }
static bool poison_wait(const void* data, xrtownershipvisitor visit, ptr context)
{ return poison(NULL, data, visit, context); }
static void notify(ptr data) { (void)data; ++notifications; }
static void release_watch(ptr data) { (void)data; }
static void drop_empty(ptr value, ptr data) { (void)value; (void)data; }
static const xfuturepayloadownershipv1 policy = {sizeof(policy), drop_cycle, trace_cycle};
static const xfuturepayloadownershipv1 opaque = {sizeof(opaque), drop_empty, poison};
static const xfuturepayloadownershipv1* const allowed[] = {&policy};
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtClearError(); xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(after.AllocCount-before->AllocCount == after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount);
}
static void cycle(void)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xfuture* future = NULL; xpromise* promise = xrtPromiseCreate(&future, NULL); assert(promise);
    assert(xrtFutureOwnership(future).Data == xrtPromiseOwnership(promise).Data);
    assert(xrtPromiseResolveOwnedPolicyV1(promise, xrtFutureRef(future), &policy));
    xrtPromiseDestroy(promise); xrtFutureDestroy(future); /* The self slot remains. */
    xrtownershipscope freeze = {0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    xrtownershipref ref = xrtFutureOwnership(future); size_t count = 0;
    const xrtownershipadapterv1* adapter = xrtFutureOwnershipAdapterV1(ref, allowed, 1);
    assert(adapter && ref.Ops->Count(ref.Data, &count) && count == 1);
    xrtownershipresult graph = {0}; assert(xrtOwnershipInspect(&ref, 1, NULL, 0, &graph));
    assert(graph.NodeCount == 2 && graph.EdgeCount == 2 && graph.ExternalRootCount == 0);
    assert(adapter->Hold(ref.Data) && adapter->Claim(ref.Data, &graph));
    assert(!adapter->Claim(ref.Data, &count));
    adapter->Restore(ref.Data, &graph); assert(adapter->Claim(ref.Data, &graph));
    adapter->Clear(ref.Data, &graph);
    assert(!xrtFutureOwnershipAdapterV1(ref, allowed, 1) && !ref.Ops->Count(ref.Data, &count));
    assert(!xrtFutureRef(future) && !xrtPromiseRef(promise)); xrtClearError();
    xfutureresult result = {0}; assert(!xrtFutureResult(future, &result)); xrtClearError();
    assert(xrtFutureState(future) == XFUTURE_CLOSED);
    assert(xrtOwnershipScopeEnd(&freeze));
    assert(adapter->Finish(ref.Data, &graph) && adapter->Finish(ref.Data, &graph));
    adapter->Drop(ref.Data); balance(&before);
}
static void boundaries(void)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    for(unsigned kind = 0; kind != 4; ++kind) {
        xfuture* future = NULL; xpromise* promise = xrtPromiseCreate(&future, NULL); assert(promise);
        xfuturewatch watch = {0}; xcancelwatch* observer = NULL;
        xcancel* cancel = xrtFutureCancelToken(future); assert(cancel);
        if(kind == 0) assert(xrtPromiseResolveOwnedTraced(promise, NULL, drop_empty, NULL, poison));
        if(kind == 1) assert(xrtPromiseResolveOwnedPolicyV1(promise, NULL, &opaque));
        if(kind == 2) {
            assert(xrtFutureWatchInitPhased(&watch, notify, release_watch, NULL, poison_wait));
            assert(xrtFutureWatchAdd(future, &watch) == XFUTURE_WATCH_PENDING);
        }
        if(kind == 3) { observer = xrtCancelWatch(cancel, notify, NULL); assert(observer); }
        xrtownershipscope freeze = {0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
        assert(!xrtFutureOwnershipAdapterV1(xrtFutureOwnership(future), allowed, 1));
        if(observer) assert(!xrtCancelOwnershipAdapterV1(xrtCancelOwnership(cancel)));
        assert(xrtOwnershipScopeEnd(&freeze));
        if(kind == 2) xrtFutureWatchRemove(future, &watch);
        if(observer) xrtCancelUnwatch(observer);
        xrtCancelDestroy(cancel); xrtPromiseDestroy(promise); xrtFutureDestroy(future);
    }
    /* Ordinary certified destruction also exits its own mutation first. */
    xfuture* future = NULL; xpromise* promise = xrtPromiseCreate(&future, NULL); assert(promise);
    assert(xrtPromiseResolveOwnedPolicyV1(promise, NULL, &policy));
    xrtPromiseDestroy(promise); xrtFutureDestroy(future); balance(&before);
}
int main(void)
{
    testRequire(xrtMemDebugEnable(true), "enable Future adapter allocation accounting");
    for(unsigned i = 0; i != 100; ++i) { cycle(); boundaries(); }
    assert(drops == 200 && traces == 100 && notifications == 0);
    puts("Future lifecycle adapter: 100 physical self cycles, 400 pre-trace refusals, 200 phased exactly-once drops; restore, quarantine and repeat Finish; zero live delta");
    return 0;
}
