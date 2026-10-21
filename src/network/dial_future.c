#include "../internal/xrt_dial_future.h"

#if defined(XRT_FEATURE_NET_DIAL_FUTURE_COMMON)

/* One physical producer. Promise is an actual owning endpoint; Bridge borrows
 * that endpoint and owns the cancellation Watch, never a second Promise.
 * Constructor, producer, request and Watch each hold a real context reference. */
typedef struct xrt_net_stream_dial_future {
    xfuturebridge Bridge;
    volatile int32 References;
    xmutex Lock;
    size_t Active;
    bool Publishing, Completed, Cleared;
    const void* Claim;
    xpromise* Promise;
    const xrt_net_dial_future_policy* Policy;
    ptr Dial;
} xrt_net_stream_dial_future;

static void __xrtNetStreamDialFutureBegin(xrt_net_stream_dial_future* context, xrtownershipscope* scope)
{
    if (!xrtOwnershipMutationBegin(scope) || !xrtMutexLock(&context->Lock)) abort();
}
static void __xrtNetStreamDialFutureEnd(xrt_net_stream_dial_future* context, xrtownershipscope* scope)
{
    if (!xrtMutexUnlock(&context->Lock) || !xrtOwnershipScopeEnd(scope)) abort();
}
static bool __xrtNetStreamDialFutureCount(const void* data, size_t* count)
{
    const xrt_net_stream_dial_future* context = data; xrtownershipref watch; int32 references;
    if (!context || !count || context->Publishing || context->Active || context->Cleared ||
        !xrtFutureBridgeWatchOwnershipV1(&context->Bridge, &watch)) return false;
    references = __xrtAtomicRefLoad(&context->References);
    if (references <= 0) return false;
    *count = (size_t)references; return true;
}
static bool __xrtNetStreamDialFutureTrace(const void* data, xrtownershipvisitor visit, ptr visitor)
{
    const xrt_net_stream_dial_future* context = data; xrtownershipref watch; size_t count;
    if (!visit || !__xrtNetStreamDialFutureCount(data, &count) ||
        !xrtFutureBridgeWatchOwnershipV1(&context->Bridge, &watch)) return false;
    if (context->Promise && !visit(xrtPromiseOwnership(context->Promise), visitor)) return false;
    if (context->Dial && !visit(context->Policy->Ownership(context->Dial), visitor)) return false;
    return !watch.Data || visit(watch, visitor);
}
const xrtownershipops __xrtNetStreamDialFutureOps = {
    __xrtNetStreamDialFutureCount, __xrtNetStreamDialFutureTrace
};
static bool __xrtNetStreamDialFutureBusy(const void* data)
{
    const xrt_net_stream_dial_future* context = data; xrtownershipref watch;
    return context && !context->Cleared && (context->Publishing || context->Active ||
        !xrtFutureBridgeWatchOwnershipV1(&context->Bridge, &watch));
}
const xrtownershipquiescencev1* __xrtNetStreamDialFutureOwnershipQuiescenceV1Get(void)
{
    static const xrtownershipquiescencev1 policy = {sizeof(policy),
        &__xrtNetStreamDialFutureOps, __xrtNetStreamDialFutureBusy};
    return &policy;
}
static xrtownershipref __xrtNetStreamDialFutureOwnership(const xrt_net_stream_dial_future* context)
{ return (xrtownershipref){context, context ? &__xrtNetStreamDialFutureOps : NULL}; }
static bool __xrtNetStreamDialFutureHold(const void* data)
{
    xrt_net_stream_dial_future* context = (xrt_net_stream_dial_future*)data;
    xrtownershipscope scope = {0}; bool held;
    __xrtNetStreamDialFutureBegin(context, &scope);
    held = !context->Cleared && xrtRefRetain(&context->References) > 0;
    __xrtNetStreamDialFutureEnd(context, &scope); return held;
}
void __xrtNetStreamDialFutureDrop(const void* data)
{
    xrt_net_stream_dial_future* context = (xrt_net_stream_dial_future*)data;
    xrtownershipscope scope = {0}; xrtownershipref watch; int32 left;
    __xrtNetStreamDialFutureBegin(context, &scope); left = xrtRefRelease(&context->References);
    if (left < 0 || (!left && (context->Publishing || !context->Completed || context->Active ||
        context->Promise || context->Dial ||
        !xrtFutureBridgeWatchOwnershipV1(&context->Bridge, &watch) || watch.Data))) abort();
    __xrtNetStreamDialFutureEnd(context, &scope);
    if (!left) {
        if (!xrtOwnershipMutationBegin(&scope)) abort();
        if (!xrtMutexUnit(&context->Lock)) abort();
        xrtFree(context);
        if (!xrtOwnershipScopeEnd(&scope)) abort();
    }
}

/* The terminal payload owns exactly one native Stream reference. */
static void __xrtNetStreamDialFutureFree(ptr value, ptr data)
{
    if (data) abort();
    xrtNetStreamDestroy((xnetstream*)value);
}
static bool __xrtNetStreamDialFuturePayloadTrace(const void* value, const void* data,
    xrtownershipvisitor visit, ptr visitor)
{ return value && !data && visit && visit(xrtNetStreamOwnership(value), visitor); }
static const xfuturepayloadownershipv1 __xrtNetStreamDialFuturePayloadPolicy = {
    sizeof(xfuturepayloadownershipv1), __xrtNetStreamDialFutureFree, __xrtNetStreamDialFuturePayloadTrace
};
static const xfutureproducerownershipv1 __xrtNetStreamDialFutureProducerPolicy = {
    sizeof(xfutureproducerownershipv1), __xrtNetStreamDialFutureDrop
};

/* The Watch owns this activation. Independently retain the Dial while
 * locked, so normal completion can move/release its field concurrently. */
static void __xrtNetStreamDialFutureCancel(ptr data)
{
    xrt_net_stream_dial_future* context = data; xrtownershipscope scope = {0}; ptr dial;
    xerror* previous = xrtTakeError();
    __xrtNetStreamDialFutureBegin(context, &scope); ++context->Active;
    dial = context->Dial ? context->Policy->Ref(context->Dial) : NULL;
    __xrtNetStreamDialFutureEnd(context, &scope);
    if (dial) { (void)context->Policy->Cancel(dial); context->Policy->Destroy(dial); }
    __xrtNetStreamDialFutureBegin(context, &scope); --context->Active;
    __xrtNetStreamDialFutureEnd(context, &scope);
    xrtClearError(); xrtSetErrorTake(previous);
}
static const xcancelwatchownershipv1 __xrtNetStreamDialFutureCancelPolicy = {
    sizeof(xcancelwatchownershipv1), __xrtNetStreamDialFutureCancel,
    __xrtNetStreamDialFutureDrop, &__xrtNetStreamDialFutureOps
};

/* The native Dial owns the activation through Done AND its release tail.
 * Acquire Ready before reading constructor-published fields. Waits, callbacks,
 * Promise notification and semantic releases run outside the frame lock and
 * shared mutation scope. No native Dial is accepted on bridge setup failure. */
void __xrtNetStreamDialFutureComplete(ptr data, xnetresult result,
    xnetstream* stream, const xerror* failure)
{
    xrt_net_stream_dial_future* context=data; xrtownershipscope scope={0};
    xpromise* promise; ptr held; xerror* error=NULL;
    xerror* previous=xrtTakeError();
    bool ready=xrtFutureBridgeWait(&context->Bridge);
    __xrtNetStreamDialFutureBegin(context,&scope); ++context->Active;
    __xrtNetStreamDialFutureEnd(context,&scope);
    xrtFutureBridgeUnwatch(&context->Bridge);
    if (result!=XNET_RESULT_OK && failure) error=xrtErrorRef(failure);
    else if (result==XNET_RESULT_OK && !stream) {
        __xrtNetSetError(XERR_INTERNAL,XNET_ERROR_DIAL_CONNECT,"complete-stream-dial-future",
            "native Dial reported success without a Stream",0);
        error=xrtTakeError();
    }
    __xrtNetStreamDialFutureBegin(context,&scope);
    promise=context->Promise; context->Promise=NULL;
    held=context->Dial; context->Dial=NULL; context->Completed=true;
    __xrtNetStreamDialFutureEnd(context,&scope);
    context->Policy->Destroy(held);
    if (!ready || result!=XNET_RESULT_OK) {
        if (stream) { (void)xrtNetStreamAbort(stream); xrtNetStreamDestroy(stream); stream=NULL; }
    }
    if (ready && result==XNET_RESULT_OK && stream) {
        if (!xrtPromiseResolveOwnedPolicyV1(promise,stream,&__xrtNetStreamDialFuturePayloadPolicy)) {
            (void)xrtNetStreamAbort(stream); xrtNetStreamDestroy(stream);
        }
    } else if (ready && result==XNET_RESULT_CANCELLED) (void)xrtPromiseCancel(promise);
    else if (ready && error) (void)xrtPromiseReject(promise,error);
    else if (ready) (void)xrtPromiseClose(promise);
    xrtErrorFree(error); xrtPromiseDestroy(promise);
    __xrtNetStreamDialFutureBegin(context,&scope); --context->Active;
    __xrtNetStreamDialFutureEnd(context,&scope);
    xrtClearError(); xrtSetErrorTake(previous);
}
const xfutureproducerownershipv1* __xrtNetStreamDialFutureProducerPolicyV1Get(void)
{ return &__xrtNetStreamDialFutureProducerPolicy; }
const xcancelwatchownershipv1* __xrtNetStreamDialFutureCancelPolicyV1Get(void)
{ return &__xrtNetStreamDialFutureCancelPolicy; }
const xfuturepayloadownershipv1* __xrtNetStreamDialFuturePayloadPolicyV1Get(void)
{ return &__xrtNetStreamDialFuturePayloadPolicy; }

static bool __xrtNetStreamDialFutureClaim(const void* data, const void* token)
{
    xrt_net_stream_dial_future* context = (xrt_net_stream_dial_future*)data; size_t count;
    if (!token || !__xrtNetStreamDialFutureCount(data, &count) ||
        (context->Claim && context->Claim != token)) return false;
    context->Claim = token; return true;
}
static void __xrtNetStreamDialFutureRestore(const void* data, const void* token)
{
    xrt_net_stream_dial_future* context = (xrt_net_stream_dial_future*)data;
    if (!token || context->Claim != token || context->Cleared) abort();
    context->Claim = NULL;
}
static bool __xrtNetStreamDialFutureReady(const void* data)
{
    const xrt_net_stream_dial_future* context = data; size_t count; xrtownershipref watch;
    return __xrtNetStreamDialFutureCount(data, &count) && context->Completed && !context->Promise &&
        !context->Dial && xrtFutureBridgeWatchOwnershipV1(&context->Bridge, &watch) && !watch.Data;
}
static xrtownershipprepareresult __xrtNetStreamDialFuturePrepare(const void* data, const void* token)
{
    const xrt_net_stream_dial_future* context = data; xrtownershipscope freeze = {0}; bool ready;
    if (!xrtOwnershipFreezeTryBegin(&freeze)) return XRT_OWNERSHIP_PREPARE_BUSY;
    if (!token || context->Claim != token || context->Cleared) abort();
    ready = __xrtNetStreamDialFutureReady(data);
    if (!xrtOwnershipScopeEnd(&freeze)) abort();
    return ready ? XRT_OWNERSHIP_PREPARE_READY : XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtNetStreamDialFutureClear(const void* data, const void* token)
{
    xrt_net_stream_dial_future* context = (xrt_net_stream_dial_future*)data;
    if (!token || context->Claim != token || context->Cleared || !__xrtNetStreamDialFutureReady(data)) abort();
    context->Cleared = true;
}
const xrtownershipadapterv1* __xrtNetStreamDialFutureOwnershipAdapterV1(xrtownershipref ref,
    const xrt_net_dial_future_policy* policy, const xrtownershippreparationv1** preparation)
{
    static const xrtownershipadapterv1 adapter = {sizeof(adapter), __xrtNetStreamDialFutureHold,
        __xrtNetStreamDialFutureDrop, __xrtNetStreamDialFutureClaim, __xrtNetStreamDialFutureRestore,
        NULL, __xrtNetStreamDialFutureClear, NULL};
    static const xrtownershippreparationv1 prepare = {sizeof(prepare), &adapter,
        __xrtNetStreamDialFutureReady, __xrtNetStreamDialFuturePrepare};
    size_t count;
    if (!preparation || ref.Ops != &__xrtNetStreamDialFutureOps || !ref.Data ||
        ((const xrt_net_stream_dial_future*)ref.Data)->Policy != policy ||
        !__xrtNetStreamDialFutureCount(ref.Data, &count)) return NULL;
    *preparation = &prepare; return &adapter;
}

xfuture* __xrtNetStreamDialFutureCreate(const xrt_net_dial_future_policy* policy,
    xrt_net_dial_future_submit submit, const void* arguments)
{
    xrt_net_stream_dial_future* context; xfuture* future; xpromise* promise;
    ptr dial; xerror* error; xrtownershipscope scope = {0};
    if (!policy || !policy->Ref || !policy->Destroy || !policy->Cancel ||
        !policy->Ownership || !submit) abort();
    context = xrtCalloc(1, sizeof(*context));
    if (!context) return NULL;
    context->References = 1; context->Publishing = true; context->Policy = policy;
    if (!xrtMutexInit(&context->Lock)) { xrtFree(context); return NULL; }
    future = xrtFutureBridgeCreate(&context->Bridge, NULL);
    if (!future) { (void)xrtMutexUnit(&context->Lock); xrtFree(context); return NULL; }
    __xrtNetStreamDialFutureBegin(context, &scope);
    context->Promise = xrtFutureBridgePromise(&context->Bridge);
    __xrtNetStreamDialFutureEnd(context, &scope);
    if (!__xrtNetStreamDialFutureHold(context)) abort();
    if (!xrtPromiseProducerBindTakeV1(context->Promise, __xrtNetStreamDialFutureOwnership(context),
        &__xrtNetStreamDialFutureProducerPolicy)) { __xrtNetStreamDialFutureDrop(context); goto failed; }
    if (!__xrtNetStreamDialFutureHold(context)) abort();
    if (!xrtFutureBridgeWatchOwnedV1(&context->Bridge, context,
        &__xrtNetStreamDialFutureCancelPolicy)) { __xrtNetStreamDialFutureDrop(context); goto failed; }
    /* No fallible bridge setup remains after work acceptance. Dial consumes
     * this reference only on success; an immediate callback waits for Ready. */
    if (!__xrtNetStreamDialFutureHold(context)) abort();
    dial = submit(context, arguments);
    if (!dial) { __xrtNetStreamDialFutureDrop(context); goto failed; }
    __xrtNetStreamDialFutureBegin(context, &scope);
    context->Dial = dial; context->Publishing = false;
    __xrtNetStreamDialFutureEnd(context, &scope);
    if (!xrtFutureBridgeReady(&context->Bridge)) abort();
    __xrtNetStreamDialFutureDrop(context); return future;
failed:
    error = xrtTakeError();
    if (!xrtFutureBridgeFail(&context->Bridge)) abort();
    xrtFutureBridgeUnwatch(&context->Bridge);
    __xrtNetStreamDialFutureBegin(context, &scope);
    promise = context->Promise; context->Promise = NULL;
    context->Publishing = false; context->Completed = true;
    __xrtNetStreamDialFutureEnd(context, &scope);
    xrtPromiseDestroy(promise); xrtFutureDestroy(future); __xrtNetStreamDialFutureDrop(context);
    xrtClearError(); xrtSetErrorTake(error); return NULL;
}

#endif
