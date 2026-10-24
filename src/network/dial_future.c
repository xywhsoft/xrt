#include "../internal/xrt_dial_future.h"

#if defined(XRT_FEATURE_NET_DIAL_FUTURE_COMMON)

/* Constructor, producer, Watch and accepted Done own real references.
 * Early completion transfers its result to a slot and returns. Either that
 * callback or the submitter consumes it after publication; neither waits. */
typedef struct xrt_net_stream_dial_future {
    volatile int32 References;
    xmutex Lock;
    size_t Active;
    bool Publishing, Received, Processing, Completed, Cleared, CancelRequested;
    bool OwnerHeld, Finishing, Finished;
    const void* Claim;
    xpromise* Promise;
    xcancelwatch* Watch;
    const xrt_net_dial_future_policy* Policy;
    ptr Dial, Value;
    xerror* Error;
    xnetresult Result;
    xrtownershipref Owner;
    xerror* FinishError;
} xrt_net_stream_dial_future;

static void __xrtNetStreamDialFutureBegin(xrt_net_stream_dial_future* c, xrtownershipscope* s)
{ if (!xrtOwnershipMutationBegin(s) || !xrtMutexLock(&c->Lock)) abort(); }
static void __xrtNetStreamDialFutureEnd(xrt_net_stream_dial_future* c, xrtownershipscope* s)
{ if (!xrtMutexUnlock(&c->Lock) || !xrtOwnershipScopeEnd(s)) abort(); }
static bool __xrtNetStreamDialFutureCount(const void* data, size_t* count)
{
    const xrt_net_stream_dial_future* c = data;
    if (!c || !count || c->Publishing || c->Active || c->Processing || c->Cleared || c->Value || c->Error ||
        (c->Owner.Data && !c->OwnerHeld) || (!c->Completed && (!c->Dial || !c->Promise || !c->Watch)) ||
        (c->Completed && (c->Dial || c->Promise || c->Watch))) return false;
    int32 refs = __xrtAtomicRefLoad(&c->References); if (refs <= 0) return false;
    *count = (size_t)refs; return true;
}
static bool __xrtNetStreamDialFutureTrace(const void* data, xrtownershipvisitor visit, ptr context)
{
    const xrt_net_stream_dial_future* c = data; size_t count;
    return visit && __xrtNetStreamDialFutureCount(c, &count) &&
        (!c->Promise || visit(xrtPromiseOwnership(c->Promise), context)) &&
        (!c->Dial || visit(c->Policy->Ownership(c->Dial), context)) &&
        (!c->Watch || visit(xrtCancelWatchOwnership(c->Watch), context)) &&
        (!c->OwnerHeld || visit(c->Owner, context));
}
const xrtownershipops __xrtNetStreamDialFutureOps = {__xrtNetStreamDialFutureCount, __xrtNetStreamDialFutureTrace};
static bool __xrtNetStreamDialFutureBusy(const void* data)
{ const xrt_net_stream_dial_future* c = data; return c && !c->Cleared && (c->Publishing || c->Active || c->Processing); }
const xrtownershipquiescencev1* __xrtNetStreamDialFutureOwnershipQuiescenceV1Get(void)
{
    static const xrtownershipquiescencev1 p = {sizeof(p), &__xrtNetStreamDialFutureOps, __xrtNetStreamDialFutureBusy}; return &p;
}
static xrtownershipref __xrtNetStreamDialFutureOwnership(const xrt_net_stream_dial_future* c)
{ return (xrtownershipref){c, c ? &__xrtNetStreamDialFutureOps : NULL}; }
xrtownershipref __xrtNetStreamDialFutureOwner(const void* data)
{
    const xrt_net_stream_dial_future* c = data;
    /* Accepted operation/callback or submitter keeps this immutable slot alive.
     * A provisional owner remains the caller's credit until acceptance. */
    if (!c || !c->Owner.Data || !c->Owner.Ops || !c->Policy->DropOwner) abort();
    return c->Owner;
}
static bool __xrtNetStreamDialFutureHold(const void* data)
{
    xrt_net_stream_dial_future* c = (xrt_net_stream_dial_future*)data; xrtownershipscope s = {0};
    __xrtNetStreamDialFutureBegin(c, &s);
    bool held = !c->Cleared && xrtRefRetain(&c->References) >= 0;
    __xrtNetStreamDialFutureEnd(c, &s); return held;
}
void __xrtNetStreamDialFutureDrop(const void* data)
{
    xrt_net_stream_dial_future* c = (xrt_net_stream_dial_future*)data; xrtownershipscope s = {0};
    __xrtNetStreamDialFutureBegin(c, &s); int32 left = xrtRefRelease(&c->References);
    if (left < 0 || (!left && (c->Publishing || !c->Completed || c->Active || c->Processing ||
        c->Promise || c->Dial || c->Watch || c->Value || c->Error))) abort();
    xrtownershipref owner = {0};
    if (!left && c->OwnerHeld) { owner = c->Owner; c->OwnerHeld = false; c->Owner = (xrtownershipref){0}; }
    __xrtNetStreamDialFutureEnd(c, &s);
    if (!left) {
        if (owner.Data) c->Policy->DropOwner(owner.Data);
        xrtErrorFree(c->FinishError);
        if (!xrtMutexUnit(&c->Lock)) abort();
        xrtFree(c);
    }
}
static void __xrtNetStreamDialFutureFree(ptr value, ptr data)
{ if (data) abort(); xrtNetStreamDestroy(value); }
static bool __xrtNetStreamDialFuturePayloadTrace(const void* value, const void* data, xrtownershipvisitor visit, ptr context)
{ return value && !data && visit && visit(xrtNetStreamOwnership(value), context); }
const xfuturepayloadownershipv1 __xrtNetStreamDialFuturePayloadPolicy = {
    sizeof(xfuturepayloadownershipv1), __xrtNetStreamDialFutureFree, __xrtNetStreamDialFuturePayloadTrace};
static const xfutureproducerownershipv1 __xrtNetStreamDialFutureProducerPolicy = {
    sizeof(xfutureproducerownershipv1), __xrtNetStreamDialFutureDrop};
static void __xrtNetStreamDialFutureCancel(ptr data)
{
    xrt_net_stream_dial_future* c = data; xrtownershipscope s = {0}; xerror* prior = xrtTakeError();
    __xrtNetStreamDialFutureBegin(c, &s); if (c->Active == SIZE_MAX) abort(); ++c->Active;
    c->CancelRequested = true; ptr dial = c->Dial ? c->Policy->Ref(c->Dial) : NULL;
    __xrtNetStreamDialFutureEnd(c, &s);
    if (dial) { (void)c->Policy->Cancel(dial); c->Policy->Destroy(dial); }
    __xrtNetStreamDialFutureBegin(c, &s); --c->Active; __xrtNetStreamDialFutureEnd(c, &s);
    xrtClearError(); xrtSetErrorTake(prior);
}
static const xcancelwatchownershipv1 __xrtNetStreamDialFutureCancelPolicy = {
    sizeof(xcancelwatchownershipv1), __xrtNetStreamDialFutureCancel,
    __xrtNetStreamDialFutureDrop, &__xrtNetStreamDialFutureOps};

/* Selected constructor OR callback consumes the slots while retaining its
 * genuine caller/Done context reference and an Active credit. */
static void __xrtNetStreamDialFutureProcess(xrt_net_stream_dial_future* c)
{
    xrtownershipscope s = {0}; xerror* prior = xrtTakeError();
    __xrtNetStreamDialFutureBegin(c, &s);
    if (c->Publishing || !c->Received || !c->Processing || !c->Active || c->Completed) abort();
    xpromise* promise = c->Promise; c->Promise = NULL;
    xcancelwatch* watch = c->Watch; c->Watch = NULL;
    ptr dial = c->Dial; c->Dial = NULL; ptr value = c->Value; c->Value = NULL;
    xerror* error = c->Error; c->Error = NULL; xnetresult result = c->Result;
    c->Completed = true;
    __xrtNetStreamDialFutureEnd(c, &s);
    xrtCancelUnwatch(watch); c->Policy->Destroy(dial);
    if (result == XNET_RESULT_OK && value) {
        if (!xrtPromiseResolveOwnedPolicyV1(promise, value, c->Policy->Payload)) {
            c->Policy->AbortValue(value); c->Policy->Payload->Drop(value, NULL);
        }
    } else {
        if (value) { c->Policy->AbortValue(value); c->Policy->Payload->Drop(value, NULL); }
        if (result == XNET_RESULT_CANCELLED) (void)xrtPromiseCancel(promise);
        else if (error) (void)xrtPromiseReject(promise, error);
        else (void)xrtPromiseClose(promise);
    }
    xrtErrorFree(error); xrtPromiseDestroy(promise);
    __xrtNetStreamDialFutureBegin(c, &s); c->Processing = false; --c->Active; __xrtNetStreamDialFutureEnd(c, &s);
    xrtClearError(); xrtSetErrorTake(prior);
}
void __xrtNetStreamDialFutureComplete(ptr data, xnetresult result, ptr value, const xerror* failure)
{
    xrt_net_stream_dial_future* c = data; xrtownershipscope s = {0}; xerror* prior = xrtTakeError();
    xerror* error = failure ? xrtErrorRef(failure) : NULL;
    if (result == XNET_RESULT_OK && !value) {
        __xrtNetSetError(XERR_INTERNAL, XNET_ERROR_DIAL_CONNECT, "complete-dial-future",
            "native Dial reported success without a value", 0); xrtErrorFree(error); error = xrtTakeError();
    }
    __xrtNetStreamDialFutureBegin(c, &s);
    if (c->Received || c->Completed || c->Cleared) abort();
    c->Received = true; c->Result = result; c->Value = value; c->Error = error;
    bool process = !c->Publishing;
    if (process) { c->Processing = true; if (c->Active == SIZE_MAX) abort(); ++c->Active; }
    __xrtNetStreamDialFutureEnd(c, &s);
    if (process) __xrtNetStreamDialFutureProcess(c);
    xrtClearError(); xrtSetErrorTake(prior);
}
const xfutureproducerownershipv1* __xrtNetStreamDialFutureProducerPolicyV1Get(void) { return &__xrtNetStreamDialFutureProducerPolicy; }
const xcancelwatchownershipv1* __xrtNetStreamDialFutureCancelPolicyV1Get(void) { return &__xrtNetStreamDialFutureCancelPolicy; }
const xfuturepayloadownershipv1* __xrtNetStreamDialFuturePayloadPolicyV1Get(void) { return &__xrtNetStreamDialFuturePayloadPolicy; }
static bool __xrtNetStreamDialFutureClaim(const void* data, const void* token)
{
    xrt_net_stream_dial_future* c = (xrt_net_stream_dial_future*)data; size_t count;
    if (!token || (c->Claim && c->Claim != token) || !__xrtNetStreamDialFutureCount(c, &count)) return false;
    c->Claim = token; return true;
}
static void __xrtNetStreamDialFutureRestore(const void* data, const void* token)
{ xrt_net_stream_dial_future* c = (xrt_net_stream_dial_future*)data; if (!token || c->Claim != token || c->Cleared) abort(); c->Claim = NULL; }
static bool __xrtNetStreamDialFutureReady(const void* data)
{ const xrt_net_stream_dial_future* c = data; size_t count; return __xrtNetStreamDialFutureCount(c, &count) && c->Completed; }
static xrtownershipprepareresult __xrtNetStreamDialFuturePrepare(const void* data, const void* token)
{
    const xrt_net_stream_dial_future* c = data; xrtownershipscope f = {0};
    if (!xrtOwnershipFreezeTryBegin(&f)) return XRT_OWNERSHIP_PREPARE_BUSY;
    if (!token || c->Claim != token || c->Cleared) abort();
    bool ready = __xrtNetStreamDialFutureReady(data);
    if (!xrtOwnershipScopeEnd(&f)) abort();
    return ready ? XRT_OWNERSHIP_PREPARE_READY : XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtNetStreamDialFutureClear(const void* data, const void* token)
{ xrt_net_stream_dial_future* c = (xrt_net_stream_dial_future*)data; if (!token || c->Claim != token || c->Cleared || !__xrtNetStreamDialFutureReady(data)) abort(); c->Cleared = true; }
static bool __xrtNetStreamDialFutureFinish(const void* data, const void* token)
{
    xrt_net_stream_dial_future* c = (xrt_net_stream_dial_future*)data; xrtownershipscope s = {0};
    __xrtNetStreamDialFutureBegin(c, &s);
    if (!token || c->Claim != token || !c->Cleared) abort();
    if (c->Finishing) { __xrtNetStreamDialFutureEnd(c, &s); return false; }
    if (c->Finished) {
        xerror* error = c->FinishError; __xrtNetStreamDialFutureEnd(c, &s);
        if (error && !xrtGetError()) xrtSetError(error);
        return error == NULL;
    }
    c->Finishing = true; xrtownershipref owner = c->OwnerHeld ? c->Owner : (xrtownershipref){0};
    c->Owner = (xrtownershipref){0}; c->OwnerHeld = false; __xrtNetStreamDialFutureEnd(c, &s);
    xerror* prior = xrtTakeError(); if (owner.Data) c->Policy->DropOwner(owner.Data); xerror* error = xrtTakeError();
    __xrtNetStreamDialFutureBegin(c, &s); c->FinishError = error; c->Finished = true; c->Finishing = false; __xrtNetStreamDialFutureEnd(c, &s);
    if (prior) xrtSetErrorTake(prior); else if (error) xrtSetError(error);
    return error == NULL;
}
const xrtownershipadapterv1* __xrtNetStreamDialFutureOwnershipAdapterV1(xrtownershipref ref,
    const xrt_net_dial_future_policy* policy, const xrtownershippreparationv1** preparation)
{
    static const xrtownershipadapterv1 a = {sizeof(a), __xrtNetStreamDialFutureHold, __xrtNetStreamDialFutureDrop,
        __xrtNetStreamDialFutureClaim, __xrtNetStreamDialFutureRestore, NULL, __xrtNetStreamDialFutureClear, __xrtNetStreamDialFutureFinish};
    static const xrtownershippreparationv1 p = {sizeof(p), &a, __xrtNetStreamDialFutureReady, __xrtNetStreamDialFuturePrepare};
    size_t count;
    if (!preparation || ref.Ops != &__xrtNetStreamDialFutureOps || !ref.Data ||
        ((const xrt_net_stream_dial_future*)ref.Data)->Policy != policy || !__xrtNetStreamDialFutureCount(ref.Data, &count)) return NULL;
    *preparation = &p; return &a;
}
xfuture* __xrtNetStreamDialFutureCreateOwned(const xrt_net_dial_future_policy* policy,
    xrt_net_dial_future_submit submit, const void* arguments, xrtownershipref owner)
{
    if (!policy || !policy->Ref || !policy->Destroy || !policy->Cancel || !policy->Ownership ||
        !policy->Payload || policy->Payload->size != sizeof(*policy->Payload) || !policy->Payload->Drop ||
        !policy->Payload->Trace || !policy->AbortValue || !submit ||
        (owner.Data ? (!owner.Ops || !policy->DropOwner) : owner.Ops != NULL)) abort();
    xrt_net_stream_dial_future* c = xrtCalloc(1, sizeof(*c)); if (!c) return NULL;
    c->References = 1; c->Publishing = true; c->Policy = policy; c->Owner = owner;
    if (!xrtMutexInit(&c->Lock)) { xrtFree(c); return NULL; }
    xfuture* future = NULL; c->Promise = xrtPromiseCreate(&future, NULL);
    if (!c->Promise) { (void)xrtMutexUnit(&c->Lock); xrtFree(c); return NULL; }
    if (!__xrtNetStreamDialFutureHold(c)) abort();
    if (!xrtPromiseProducerBindTakeV1(c->Promise, __xrtNetStreamDialFutureOwnership(c), &__xrtNetStreamDialFutureProducerPolicy)) {
        __xrtNetStreamDialFutureDrop(c); goto failed;
    }
    xcancel* cancel = xrtPromiseCancelToken(c->Promise);
    if (!cancel) goto failed;
    if (!__xrtNetStreamDialFutureHold(c)) abort();
    c->Watch = xrtCancelWatchOwnedV1(cancel, c, &__xrtNetStreamDialFutureCancelPolicy); xrtCancelDestroy(cancel);
    if (!c->Watch) { __xrtNetStreamDialFutureDrop(c); goto failed; }
    if (!__xrtNetStreamDialFutureHold(c)) abort();
    ptr dial = submit(c, arguments);
    if (!dial) { __xrtNetStreamDialFutureDrop(c); goto failed; }
    xrtownershipscope s = {0}; __xrtNetStreamDialFutureBegin(c, &s);
    c->Dial = dial; c->OwnerHeld = owner.Data != NULL; c->Publishing = false;
    bool process = c->Received; bool cancelPending = c->CancelRequested && !process;
    if (process) { c->Processing = true; ++c->Active; }
    ptr cancelling = cancelPending ? policy->Ref(dial) : NULL;
    __xrtNetStreamDialFutureEnd(c, &s);
    if (cancelling) { (void)policy->Cancel(cancelling); policy->Destroy(cancelling); }
    if (process) __xrtNetStreamDialFutureProcess(c);
    __xrtNetStreamDialFutureDrop(c); return future;
failed: {
    xerror* error = xrtTakeError(); xrtownershipscope s = {0}; __xrtNetStreamDialFutureBegin(c, &s);
    if (c->Received || c->Value || c->Error) abort();
    xpromise* promise = c->Promise; c->Promise = NULL; xcancelwatch* watch = c->Watch; c->Watch = NULL;
    c->Publishing = false; c->Completed = true; ++c->Active; __xrtNetStreamDialFutureEnd(c, &s);
    xrtCancelUnwatch(watch); xrtPromiseDestroy(promise); xrtFutureDestroy(future);
    __xrtNetStreamDialFutureBegin(c, &s); --c->Active; __xrtNetStreamDialFutureEnd(c, &s);
    __xrtNetStreamDialFutureDrop(c); xrtClearError(); xrtSetErrorTake(error); return NULL;
}}
xfuture* __xrtNetStreamDialFutureCreate(const xrt_net_dial_future_policy* policy,
    xrt_net_dial_future_submit submit, const void* arguments)
{ return __xrtNetStreamDialFutureCreateOwned(policy, submit, arguments, (xrtownershipref){0}); }
#endif
