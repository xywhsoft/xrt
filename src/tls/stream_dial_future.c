#include "../internal/xrt_tls_stream.h"
#include "../internal/xrt_dial_future.h"

#if defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE)

/* The owner is independent of the producer's terminal slots: delivered Streams
 * may still borrow configuration/events after Promise, Watch and Dial retire. */
typedef struct xrt_tls_dial_future_owner {
    volatile int32 References;
    const xtlsstreamownershipv1* Policy;
    ptr Data;
    const void* Claim;
    bool Publishing, DataHeld, Cleared, Finishing, Finished;
    xerror* FinishError;
} xrt_tls_dial_future_owner;
static bool __xrtTlsDialFutureOwnerCount(const void* data, size_t* count)
{
    const xrt_tls_dial_future_owner* o = data;
    if (!o || !count || o->Publishing || o->Cleared || !o->DataHeld || !o->Data || !o->Policy) return false;
    int32 refs = __xrtAtomicRefLoad(&o->References); if (refs <= 0) return false;
    *count = (size_t)refs; return true;
}
static bool __xrtTlsDialFutureOwnerTrace(const void* data, xrtownershipvisitor visit, ptr context)
{
    const xrt_tls_dial_future_owner* o = data; size_t count;
    return visit && __xrtTlsDialFutureOwnerCount(o, &count) && visit((xrtownershipref){o->Data, o->Policy->Ops}, context);
}
static const xrtownershipops __xrtTlsDialFutureOwnerOps = {__xrtTlsDialFutureOwnerCount, __xrtTlsDialFutureOwnerTrace};
static bool __xrtTlsDialFutureOwnerHold(const void* data)
{
    xrt_tls_dial_future_owner* o = (xrt_tls_dial_future_owner*)data; xrtownershipscope m = {0};
    if (!xrtOwnershipMutationBegin(&m)) return false;
    bool held = !o->Cleared && xrtRefRetain(&o->References) >= 0;
    if (!xrtOwnershipScopeEnd(&m)) abort();
    return held;
}
static void __xrtTlsDialFutureOwnerDrop(const void* data)
{
    xrt_tls_dial_future_owner* o = (xrt_tls_dial_future_owner*)data; xrtownershipscope m = {0};
    if (!xrtOwnershipMutationBegin(&m)) abort();
    int32 left = xrtRefRelease(&o->References); if (left < 0 || (!left && o->Publishing)) abort();
    ptr user = NULL;
    if (!left && o->DataHeld) { user = o->Data; o->Data = NULL; o->DataHeld = false; }
    if (!xrtOwnershipScopeEnd(&m)) abort();
    if (!left) { if (user) o->Policy->Drop(user); xrtErrorFree(o->FinishError); xrtFree(o); }
}
static bool __xrtTlsDialFutureOwnerBusy(const void* data)
{ const xrt_tls_dial_future_owner* o = data; return o && !o->Cleared && o->Publishing; }
XRT_API const xrtownershipquiescencev1* xrtTlsDialFutureContextOwnershipQuiescenceV1Get(void)
{
    static const xrtownershipquiescencev1 p = {sizeof(p), &__xrtTlsDialFutureOwnerOps, __xrtTlsDialFutureOwnerBusy}; return &p;
}
static bool __xrtTlsDialFutureOwnerClaim(const void* data, const void* token)
{
    xrt_tls_dial_future_owner* o = (xrt_tls_dial_future_owner*)data; size_t count;
    if (!token || (o->Claim && o->Claim != token) || !__xrtTlsDialFutureOwnerCount(o, &count)) return false;
    o->Claim = token; return true;
}
static void __xrtTlsDialFutureOwnerRestore(const void* data, const void* token)
{
    xrt_tls_dial_future_owner* o = (xrt_tls_dial_future_owner*)data;
    if (!token || o->Claim != token || o->Cleared) abort();
    o->Claim = NULL;
}
static bool __xrtTlsDialFutureOwnerReady(const void* data)
{ size_t count; return __xrtTlsDialFutureOwnerCount(data, &count); }
static xrtownershipprepareresult __xrtTlsDialFutureOwnerPrepare(const void* data, const void* token)
{
    const xrt_tls_dial_future_owner* o = data; xrtownershipscope f = {0};
    if (!xrtOwnershipFreezeTryBegin(&f)) return XRT_OWNERSHIP_PREPARE_BUSY;
    if (!token || o->Claim != token || o->Cleared) abort();
    bool ready = __xrtTlsDialFutureOwnerReady(o);
    if (!xrtOwnershipScopeEnd(&f)) abort();
    return ready ? XRT_OWNERSHIP_PREPARE_READY : XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtTlsDialFutureOwnerClear(const void* data, const void* token)
{
    xrt_tls_dial_future_owner* o = (xrt_tls_dial_future_owner*)data;
    if (!token || o->Claim != token || !__xrtTlsDialFutureOwnerReady(o)) abort();
    o->Cleared = true;
}
static bool __xrtTlsDialFutureOwnerFinish(const void* data, const void* token)
{
    xrt_tls_dial_future_owner* o = (xrt_tls_dial_future_owner*)data; xrtownershipscope m = {0};
    if (!xrtOwnershipMutationBegin(&m)) return false;
    if (!token || o->Claim != token || !o->Cleared) abort();
    if (o->Finishing) { if (!xrtOwnershipScopeEnd(&m)) abort(); return false; }
    if (o->Finished) {
        xerror* error = o->FinishError; if (!xrtOwnershipScopeEnd(&m)) abort();
        if (error && !xrtGetError()) xrtSetError(error);
        return error == NULL;
    }
    o->Finishing = true; ptr user = o->DataHeld ? o->Data : NULL; o->Data = NULL; o->DataHeld = false;
    if (!xrtOwnershipScopeEnd(&m)) abort();
    xerror* prior = xrtTakeError(); if (user) o->Policy->Drop(user); xerror* error = xrtTakeError();
    if (!xrtOwnershipMutationBegin(&m)) abort();
    o->FinishError = error; o->Finishing = false; o->Finished = true;
    if (!xrtOwnershipScopeEnd(&m)) abort();
    if (prior) xrtSetErrorTake(prior); else if (error) xrtSetError(error);
    return error == NULL;
}
XRT_API const xrtownershipadapterv1* xrtTlsDialFutureContextOwnershipAdapterV1(xrtownershipref ref,
    const xtlsstreamownershipv1* const* policies, size_t count, const xrtownershippreparationv1** preparation)
{
    static const xrtownershipadapterv1 a = {sizeof(a), __xrtTlsDialFutureOwnerHold, __xrtTlsDialFutureOwnerDrop,
        __xrtTlsDialFutureOwnerClaim, __xrtTlsDialFutureOwnerRestore, NULL, __xrtTlsDialFutureOwnerClear, __xrtTlsDialFutureOwnerFinish};
    static const xrtownershippreparationv1 p = {sizeof(p), &a, __xrtTlsDialFutureOwnerReady, __xrtTlsDialFutureOwnerPrepare};
    if (!preparation || ref.Ops != &__xrtTlsDialFutureOwnerOps || !ref.Data || (count && !policies)) return NULL;
    const xrt_tls_dial_future_owner* o = ref.Data; bool known = false;
    for (size_t i = 0; i < count; ++i) if (policies[i] == o->Policy) { known = true; break; }
    if (!known || !o->Policy || o->Policy->size != sizeof(*o->Policy) || !o->Policy->Drop ||
        !o->Policy->Ops || !o->Policy->Ops->Count || !o->Policy->Ops->Trace || !__xrtTlsDialFutureOwnerReady(o)) return NULL;
    *preparation = &p; return &a;
}
static xrt_tls_dial_future_owner* __xrtTlsDialFutureOwner(ptr context)
{
    xrtownershipref ref = __xrtNetStreamDialFutureOwner(context);
    if (ref.Ops != &__xrtTlsDialFutureOwnerOps) abort();
    return (xrt_tls_dial_future_owner*)ref.Data;
}
#define XRT_TLS_FUTURE_EVENT(Name) \
static void __xrtTlsDialFuture##Name(xtlsstream* stream, ptr context) \
{ xrt_tls_dial_future_owner* o = __xrtTlsDialFutureOwner(context); \
  if (o->Policy->Events.Name) o->Policy->Events.Name(stream, o->Data); }
XRT_TLS_FUTURE_EVENT(Open)
XRT_TLS_FUTURE_EVENT(End)
XRT_TLS_FUTURE_EVENT(Writable)
XRT_TLS_FUTURE_EVENT(Drain)
XRT_TLS_FUTURE_EVENT(Ticket)
#undef XRT_TLS_FUTURE_EVENT
static void __xrtTlsDialFutureRead(xtlsstream* stream, const xnetbuf* buffer, ptr context)
{
    xrt_tls_dial_future_owner* o = __xrtTlsDialFutureOwner(context);
    if (o->Policy->Events.Read) o->Policy->Events.Read(stream, buffer, o->Data);
}
static void __xrtTlsDialFutureClose(xtlsstream* stream, xnetresult result, const xerror* error, ptr context)
{
    xrt_tls_dial_future_owner* o = __xrtTlsDialFutureOwner(context);
    if (o->Policy->Events.Close) o->Policy->Events.Close(stream, result, error, o->Data);
}
static void __xrtTlsDialFutureDone(xtlsdial* dial, xnetresult result, xtlsstream* stream, const xerror* error, ptr context)
{ (void)dial; __xrtNetStreamDialFutureComplete(context, result, stream, error); }
static void __xrtTlsDialFutureLegacyDone(xtlsdial* dial, xnetresult result, xtlsstream* stream, const xerror* error, ptr context)
{
    __xrtTlsDialFutureDone(dial, result, stream, error, context);
    __xrtNetStreamDialFutureDrop(context); /* legacy Done data owns no policy tail */
}
XRT_API const xtlsdialownershipv1* xrtTlsDialFutureDialPolicyV1Get(void)
{
    static const xtlsdialownershipv1 p = {sizeof(p), {__xrtTlsDialFutureOpen, NULL, __xrtTlsDialFutureEnd,
        __xrtTlsDialFutureWritable, __xrtTlsDialFutureDrain, __xrtTlsDialFutureClose, __xrtTlsDialFutureTicket},
        __xrtTlsDialFutureDone, __xrtNetStreamDialFutureDrop, &__xrtNetStreamDialFutureOps}; return &p;
}
XRT_API const xtlsdialownershipv1* xrtTlsDialFutureDialPushPolicyV1Get(void)
{
    static const xtlsdialownershipv1 p = {sizeof(p), {__xrtTlsDialFutureOpen, __xrtTlsDialFutureRead, __xrtTlsDialFutureEnd,
        __xrtTlsDialFutureWritable, __xrtTlsDialFutureDrain, __xrtTlsDialFutureClose, __xrtTlsDialFutureTicket},
        __xrtTlsDialFutureDone, __xrtNetStreamDialFutureDrop, &__xrtNetStreamDialFutureOps}; return &p;
}
static ptr __xrtTlsDialFutureRef(ptr dial) { return xrtTlsDialRef(dial); }
static void __xrtTlsDialFutureDestroy(ptr dial) { xrtTlsDialDestroy(dial); }
static bool __xrtTlsDialFutureCancel(ptr dial) { return xrtTlsDialCancel(dial); }
static xrtownershipref __xrtTlsDialFutureOwnership(const void* dial) { return xrtTlsDialOwnership(dial); }
static void __xrtTlsDialFutureAbortValue(ptr value) { (void)__xrtTlsStreamAbortAccepted(value); }
static void __xrtTlsDialFutureFree(ptr value, ptr data)
{ if (data) abort(); xrtTlsStreamDestroy(value); }
static bool __xrtTlsDialFuturePayloadTrace(const void* value, const void* data, xrtownershipvisitor visit, ptr context)
{ return value && !data && visit && visit(xrtTlsStreamOwnership(value), context); }
static const xfuturepayloadownershipv1 __xrtTlsDialFuturePayload = {
    sizeof(xfuturepayloadownershipv1), __xrtTlsDialFutureFree, __xrtTlsDialFuturePayloadTrace};
static const xrt_net_dial_future_policy __xrtTlsDialFutureOperations = {
    __xrtTlsDialFutureRef, __xrtTlsDialFutureDestroy, __xrtTlsDialFutureCancel, __xrtTlsDialFutureOwnership,
    &__xrtTlsDialFuturePayload, __xrtTlsDialFutureAbortValue, __xrtTlsDialFutureOwnerDrop};

typedef struct xrt_tls_dial_future_arguments {
    xnetengine* Engine; xnetresolver* Resolver; const void* Proxy;
    cstr Host; uint16 Port; const xtlsclientconfig* Tls; const xtlsdialconfig* Config;
    const xtlsstreamevents* Events; ptr Data; const xtlsstreamownershipv1* Policy;
} xrt_tls_dial_future_arguments;
static ptr __xrtTlsDialFutureSubmit(ptr context, const void* arguments)
{
    const xrt_tls_dial_future_arguments* a = arguments;
    if (!a->Policy) return xrtTlsDial(a->Engine, a->Resolver, a->Host, a->Port, a->Tls, a->Config,
        a->Events, a->Data, __xrtTlsDialFutureLegacyDone, context);
    const xtlsdialownershipv1* policy = a->Policy->Events.Read ?
        xrtTlsDialFutureDialPushPolicyV1Get() : xrtTlsDialFutureDialPolicyV1Get();
    #if defined(XRT_FEATURE_TLS_STREAM_DIAL_PROXY)
        if (a->Proxy) return xrtTlsDialProxyOwnedV1(a->Engine, a->Resolver, a->Proxy, a->Host, a->Port,
            a->Tls, a->Config, context, policy);
    #endif
    return xrtTlsDialOwnedV1(a->Engine, a->Resolver, a->Host, a->Port, a->Tls, a->Config, context, policy);
}
static xfuture* __xrtTlsDialFutureCreate(const xrt_tls_dial_future_arguments* args)
{
    if (!args->Policy) return __xrtNetStreamDialFutureCreate(&__xrtTlsDialFutureOperations, __xrtTlsDialFutureSubmit, args);
    const xtlsstreamownershipv1* policy = args->Policy;
    if (!args->Data || policy->size != sizeof(*policy) || !policy->Drop || !policy->Ops || !policy->Ops->Count || !policy->Ops->Trace) {
        __xrtErrorSetInvalidArgument(); return NULL;
    }
    xrt_tls_dial_future_owner* o = xrtCalloc(1, sizeof(*o)); if (!o) return NULL;
    /* Wrapper and provisional transferred slot each own one credit. The extra
     * wrapper credit keeps this record alive even if accepted work finishes
     * and releases its entire graph before Create returns. */
    o->References = 2; o->Publishing = true; o->Policy = policy; o->Data = args->Data;
    xfuture* future = __xrtNetStreamDialFutureCreateOwned(&__xrtTlsDialFutureOperations,
        __xrtTlsDialFutureSubmit, args, (xrtownershipref){o, &__xrtTlsDialFutureOwnerOps});
    xrtownershipscope m = {0}; if (!xrtOwnershipMutationBegin(&m)) abort();
    o->Publishing = false; o->DataHeld = future != NULL;
    if (!xrtOwnershipScopeEnd(&m)) abort();
    if (!future) __xrtTlsDialFutureOwnerDrop(o); /* failed transfer retains both credits */
    __xrtTlsDialFutureOwnerDrop(o); return future;
}
XRT_API xfuture* xrtTlsDialAsync(xnetengine* engine, xnetresolver* resolver, cstr host, uint16 port,
    const xtlsclientconfig* tls, const xtlsdialconfig* config, const xtlsstreamevents* events, ptr data)
{
    xrt_tls_dial_future_arguments args = {engine, resolver, NULL, host, port, tls, config, events, data, NULL};
    return __xrtTlsDialFutureCreate(&args);
}
XRT_API xfuture* xrtTlsDialAsyncOwnedV1(xnetengine* engine, xnetresolver* resolver, cstr host, uint16 port,
    const xtlsclientconfig* tls, const xtlsdialconfig* config, ptr data, const xtlsstreamownershipv1* policy)
{
    if (!policy) { __xrtErrorSetInvalidArgument(); return NULL; }
    xrt_tls_dial_future_arguments args = {engine, resolver, NULL, host, port, tls, config, NULL, data, policy};
    return __xrtTlsDialFutureCreate(&args);
}
#if defined(XRT_FEATURE_TLS_STREAM_DIAL_PROXY)
XRT_API xfuture* xrtTlsDialProxyAsyncOwnedV1(xnetengine* engine, xnetresolver* resolver, const xnetproxy* proxy,
    cstr host, uint16 port, const xtlsclientconfig* tls, const xtlsdialconfig* config, ptr data, const xtlsstreamownershipv1* policy)
{
    if (!proxy || !policy) { __xrtErrorSetInvalidArgument(); return NULL; }
    xrt_tls_dial_future_arguments args = {engine, resolver, proxy, host, port, tls, config, NULL, data, policy};
    return __xrtTlsDialFutureCreate(&args);
}
#endif
XRT_API const xfutureproducerownershipv1* xrtTlsDialFutureProducerPolicyV1Get(void) { return __xrtNetStreamDialFutureProducerPolicyV1Get(); }
XRT_API const xcancelwatchownershipv1* xrtTlsDialFutureCancelPolicyV1Get(void) { return __xrtNetStreamDialFutureCancelPolicyV1Get(); }
XRT_API const xfuturepayloadownershipv1* xrtTlsDialFuturePayloadPolicyV1Get(void) { return &__xrtTlsDialFuturePayload; }
XRT_API const xrtownershipquiescencev1* xrtTlsDialFutureOwnershipQuiescenceV1Get(void) { return __xrtNetStreamDialFutureOwnershipQuiescenceV1Get(); }
XRT_API const xrtownershipadapterv1* xrtTlsDialFutureOwnershipAdapterV1(xrtownershipref ref,
    const xrtownershippreparationv1** preparation)
{ return __xrtNetStreamDialFutureOwnershipAdapterV1(ref, &__xrtTlsDialFutureOperations, preparation); }
#endif
