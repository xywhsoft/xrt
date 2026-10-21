#include "../internal/xrt_tcp.h"
#include <xrt/future_bridge.h>

#if defined(XRT_FEATURE_NET_TCP_DIAL_FUTURE)

/* One physical producer. Promise is an actual owning endpoint; Bridge borrows
 * that endpoint and owns the cancellation Watch, never a second Promise.
 * Constructor, producer, request and Watch each hold a real context reference. */
typedef struct xrt_net_dial_future {
    xfuturebridge Bridge;
    volatile int32 References;
    xmutex Lock;
    size_t Active;
    bool Publishing, Completed, Cleared;
    const void* Claim;
    xpromise* Promise;
    xnetdial* Dial;
} xrt_net_dial_future;

static void __xrtNetDialFutureBegin(xrt_net_dial_future* context, xrtownershipscope* scope)
{
    if (!xrtOwnershipMutationBegin(scope) || !xrtMutexLock(&context->Lock)) abort();
}
static void __xrtNetDialFutureEnd(xrt_net_dial_future* context, xrtownershipscope* scope)
{
    if (!xrtMutexUnlock(&context->Lock) || !xrtOwnershipScopeEnd(scope)) abort();
}
static bool __xrtNetDialFutureCount(const void* data, size_t* count)
{
    const xrt_net_dial_future* context = data; xrtownershipref watch; int32 references;
    if (!context || !count || context->Publishing || context->Active || context->Cleared ||
        !xrtFutureBridgeWatchOwnershipV1(&context->Bridge, &watch)) return false;
    references = __xrtAtomicRefLoad(&context->References);
    if (references <= 0) return false;
    *count = (size_t)references; return true;
}
static bool __xrtNetDialFutureTrace(const void* data, xrtownershipvisitor visit, ptr visitor)
{
    const xrt_net_dial_future* context = data; xrtownershipref watch; size_t count;
    if (!visit || !__xrtNetDialFutureCount(data, &count) ||
        !xrtFutureBridgeWatchOwnershipV1(&context->Bridge, &watch)) return false;
    if (context->Promise && !visit(xrtPromiseOwnership(context->Promise), visitor)) return false;
    if (context->Dial && !visit(xrtNetDialOwnership(context->Dial), visitor)) return false;
    return !watch.Data || visit(watch, visitor);
}
static const xrtownershipops __xrtNetDialFutureOps = {
    __xrtNetDialFutureCount, __xrtNetDialFutureTrace
};
static bool __xrtNetDialFutureBusy(const void* data)
{
    const xrt_net_dial_future* context = data; xrtownershipref watch;
    return context && !context->Cleared && (context->Publishing || context->Active ||
        !xrtFutureBridgeWatchOwnershipV1(&context->Bridge, &watch));
}
XRT_API const xrtownershipquiescencev1* xrtNetDialFutureOwnershipQuiescenceV1Get(void)
{
    static const xrtownershipquiescencev1 policy = {sizeof(policy),
        &__xrtNetDialFutureOps, __xrtNetDialFutureBusy};
    return &policy;
}
static xrtownershipref __xrtNetDialFutureOwnership(const xrt_net_dial_future* context)
{ return (xrtownershipref){context, context ? &__xrtNetDialFutureOps : NULL}; }
static bool __xrtNetDialFutureHold(const void* data)
{
    xrt_net_dial_future* context = (xrt_net_dial_future*)data;
    xrtownershipscope scope = {0}; bool held;
    __xrtNetDialFutureBegin(context, &scope);
    held = !context->Cleared && xrtRefRetain(&context->References) > 0;
    __xrtNetDialFutureEnd(context, &scope); return held;
}
static void __xrtNetDialFutureDrop(const void* data)
{
    xrt_net_dial_future* context = (xrt_net_dial_future*)data;
    xrtownershipscope scope = {0}; xrtownershipref watch; int32 left;
    __xrtNetDialFutureBegin(context, &scope); left = xrtRefRelease(&context->References);
    if (left < 0 || (!left && (context->Publishing || !context->Completed || context->Active ||
        context->Promise || context->Dial ||
        !xrtFutureBridgeWatchOwnershipV1(&context->Bridge, &watch) || watch.Data))) abort();
    __xrtNetDialFutureEnd(context, &scope);
    if (!left) {
        if (!xrtOwnershipMutationBegin(&scope)) abort();
        if (!xrtMutexUnit(&context->Lock)) abort();
        xrtFree(context);
        if (!xrtOwnershipScopeEnd(&scope)) abort();
    }
}

/* The terminal payload owns exactly one native Stream reference. */
static void __xrtNetDialFutureFree(ptr value, ptr data)
{
    if (data) abort();
    xrtNetStreamDestroy((xnetstream*)value);
}
static bool __xrtNetDialFuturePayloadTrace(const void* value, const void* data,
    xrtownershipvisitor visit, ptr visitor)
{ return value && !data && visit && visit(xrtNetStreamOwnership(value), visitor); }
static const xfuturepayloadownershipv1 __xrtNetDialFuturePayloadPolicy = {
    sizeof(xfuturepayloadownershipv1), __xrtNetDialFutureFree, __xrtNetDialFuturePayloadTrace
};
static const xfutureproducerownershipv1 __xrtNetDialFutureProducerPolicy = {
    sizeof(xfutureproducerownershipv1), __xrtNetDialFutureDrop
};

/* The Watch owns this activation. Independently retain the Dial while
 * locked, so normal completion can move/release its field concurrently. */
static void __xrtNetDialFutureCancel(ptr data)
{
    xrt_net_dial_future* context = data; xrtownershipscope scope = {0}; xnetdial* dial;
    xerror* previous = xrtTakeError();
    __xrtNetDialFutureBegin(context, &scope); ++context->Active;
    dial = context->Dial ? xrtNetDialRef(context->Dial) : NULL;
    __xrtNetDialFutureEnd(context, &scope);
    if (dial) { (void)xrtNetDialCancel(dial); xrtNetDialDestroy(dial); }
    __xrtNetDialFutureBegin(context, &scope); --context->Active;
    __xrtNetDialFutureEnd(context, &scope);
    xrtClearError(); xrtSetErrorTake(previous);
}
static const xcancelwatchownershipv1 __xrtNetDialFutureCancelPolicy = {
    sizeof(xcancelwatchownershipv1), __xrtNetDialFutureCancel,
    __xrtNetDialFutureDrop, &__xrtNetDialFutureOps
};

/* The native Dial owns the activation through Done AND its release tail.
 * Acquire Ready before reading constructor-published fields. Waits, callbacks,
 * Promise notification and semantic releases run outside the frame lock and
 * shared mutation scope. No native Dial is accepted on bridge setup failure. */
static void __xrtNetDialFutureDone(xnetdial* dial, xnetresult result,
    xnetstream* stream, const xerror* failure, ptr data)
{
    (void)dial;
    xrt_net_dial_future* context=data; xrtownershipscope scope={0};
    xpromise* promise; xnetdial* held; xerror* error=NULL;
    xerror* previous=xrtTakeError();
    bool ready=xrtFutureBridgeWait(&context->Bridge);
    __xrtNetDialFutureBegin(context,&scope); ++context->Active;
    __xrtNetDialFutureEnd(context,&scope);
    xrtFutureBridgeUnwatch(&context->Bridge);
    if (result!=XNET_RESULT_OK && failure) error=xrtErrorRef(failure);
    else if (result==XNET_RESULT_OK && !stream) {
        __xrtNetSetError(XERR_INTERNAL,XNET_ERROR_DIAL_CONNECT,"complete-dial-future",
            "TCP Dial reported success without a Stream",0);
        error=xrtTakeError();
    }
    __xrtNetDialFutureBegin(context,&scope);
    promise=context->Promise; context->Promise=NULL;
    held=context->Dial; context->Dial=NULL; context->Completed=true;
    __xrtNetDialFutureEnd(context,&scope);
    xrtNetDialDestroy(held);
    if (!ready || result!=XNET_RESULT_OK) {
        if (stream) { (void)xrtNetStreamAbort(stream); xrtNetStreamDestroy(stream); stream=NULL; }
    }
    if (ready && result==XNET_RESULT_OK && stream) {
        if (!xrtPromiseResolveOwnedPolicyV1(promise,stream,&__xrtNetDialFuturePayloadPolicy)) {
            (void)xrtNetStreamAbort(stream); xrtNetStreamDestroy(stream);
        }
    } else if (ready && result==XNET_RESULT_CANCELLED) (void)xrtPromiseCancel(promise);
    else if (ready && error) (void)xrtPromiseReject(promise,error);
    else if (ready) (void)xrtPromiseClose(promise);
    xrtErrorFree(error); xrtPromiseDestroy(promise);
    __xrtNetDialFutureBegin(context,&scope); --context->Active;
    __xrtNetDialFutureEnd(context,&scope);
    xrtClearError(); xrtSetErrorTake(previous);
}
static const xnetdialownershipv1 __xrtNetDialFutureDialPolicy = {
    sizeof(xnetdialownershipv1), __xrtNetDialFutureDone,
    __xrtNetDialFutureDrop, &__xrtNetDialFutureOps
};
XRT_API const xfutureproducerownershipv1* xrtNetDialFutureProducerPolicyV1Get(void)
{ return &__xrtNetDialFutureProducerPolicy; }
XRT_API const xcancelwatchownershipv1* xrtNetDialFutureCancelPolicyV1Get(void)
{ return &__xrtNetDialFutureCancelPolicy; }
XRT_API const xnetdialownershipv1* xrtNetDialFutureDialPolicyV1Get(void)
{ return &__xrtNetDialFutureDialPolicy; }
XRT_API const xfuturepayloadownershipv1* xrtNetDialFuturePayloadPolicyV1Get(void)
{ return &__xrtNetDialFuturePayloadPolicy; }

static bool __xrtNetDialFutureClaim(const void* data, const void* token)
{
    xrt_net_dial_future* context = (xrt_net_dial_future*)data; size_t count;
    if (!token || !__xrtNetDialFutureCount(data, &count) ||
        (context->Claim && context->Claim != token)) return false;
    context->Claim = token; return true;
}
static void __xrtNetDialFutureRestore(const void* data, const void* token)
{
    xrt_net_dial_future* context = (xrt_net_dial_future*)data;
    if (!token || context->Claim != token || context->Cleared) abort();
    context->Claim = NULL;
}
static bool __xrtNetDialFutureReady(const void* data)
{
    const xrt_net_dial_future* context = data; size_t count; xrtownershipref watch;
    return __xrtNetDialFutureCount(data, &count) && context->Completed && !context->Promise &&
        !context->Dial && xrtFutureBridgeWatchOwnershipV1(&context->Bridge, &watch) && !watch.Data;
}
static xrtownershipprepareresult __xrtNetDialFuturePrepare(const void* data, const void* token)
{
    const xrt_net_dial_future* context = data; xrtownershipscope freeze = {0}; bool ready;
    if (!xrtOwnershipFreezeTryBegin(&freeze)) return XRT_OWNERSHIP_PREPARE_BUSY;
    if (!token || context->Claim != token || context->Cleared) abort();
    ready = __xrtNetDialFutureReady(data);
    if (!xrtOwnershipScopeEnd(&freeze)) abort();
    return ready ? XRT_OWNERSHIP_PREPARE_READY : XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtNetDialFutureClear(const void* data, const void* token)
{
    xrt_net_dial_future* context = (xrt_net_dial_future*)data;
    if (!token || context->Claim != token || context->Cleared || !__xrtNetDialFutureReady(data)) abort();
    context->Cleared = true;
}
XRT_API const xrtownershipadapterv1* xrtNetDialFutureOwnershipAdapterV1(xrtownershipref ref,
    const xrtownershippreparationv1** preparation)
{
    static const xrtownershipadapterv1 adapter = {sizeof(adapter), __xrtNetDialFutureHold,
        __xrtNetDialFutureDrop, __xrtNetDialFutureClaim, __xrtNetDialFutureRestore,
        NULL, __xrtNetDialFutureClear, NULL};
    static const xrtownershippreparationv1 prepare = {sizeof(prepare), &adapter,
        __xrtNetDialFutureReady, __xrtNetDialFuturePrepare};
    size_t count;
    if (!preparation || ref.Ops != &__xrtNetDialFutureOps || !__xrtNetDialFutureCount(ref.Data, &count)) return NULL;
    *preparation = &prepare; return &adapter;
}

static xfuture* __xrtNetDialFutureCreate(xnetengine* engine, xnetresolver* resolver,
    cstr host, uint16 port, const xnetdialconfig* config,
    const xnetstreamevents* events, ptr streamData, const xnetaddrlist* addresses)
{
    xrt_net_dial_future* context; xfuture* future; xpromise* promise;
    xnetdial* dial; xerror* error; xrtownershipscope scope = {0};
    context = xrtCalloc(1, sizeof(*context));
    if (!context) return NULL;
    context->References = 1; context->Publishing = true;
    if (!xrtMutexInit(&context->Lock)) { xrtFree(context); return NULL; }
    future = xrtFutureBridgeCreate(&context->Bridge, NULL);
    if (!future) { (void)xrtMutexUnit(&context->Lock); xrtFree(context); return NULL; }
    __xrtNetDialFutureBegin(context, &scope);
    context->Promise = xrtFutureBridgePromise(&context->Bridge);
    __xrtNetDialFutureEnd(context, &scope);
    if (!__xrtNetDialFutureHold(context)) abort();
    if (!xrtPromiseProducerBindTakeV1(context->Promise, __xrtNetDialFutureOwnership(context),
        &__xrtNetDialFutureProducerPolicy)) { __xrtNetDialFutureDrop(context); goto failed; }
    if (!__xrtNetDialFutureHold(context)) abort();
    if (!xrtFutureBridgeWatchOwnedV1(&context->Bridge, context,
        &__xrtNetDialFutureCancelPolicy)) { __xrtNetDialFutureDrop(context); goto failed; }
    /* No fallible bridge setup remains after work acceptance. Dial consumes
     * this reference only on success; an immediate callback waits for Ready. */
    if (!__xrtNetDialFutureHold(context)) abort();
    dial = addresses?xrtNetDialAddressesOwnedV1(engine,addresses,config,events,streamData,context,&__xrtNetDialFutureDialPolicy):
        xrtNetDialOwnedV1(engine,resolver,host,port,config,events,streamData,context,&__xrtNetDialFutureDialPolicy);
    if (!dial) { __xrtNetDialFutureDrop(context); goto failed; }
    __xrtNetDialFutureBegin(context, &scope);
    context->Dial = dial; context->Publishing = false;
    __xrtNetDialFutureEnd(context, &scope);
    if (!xrtFutureBridgeReady(&context->Bridge)) abort();
    __xrtNetDialFutureDrop(context); return future;
failed:
    error = xrtTakeError();
    if (!xrtFutureBridgeFail(&context->Bridge)) abort();
    xrtFutureBridgeUnwatch(&context->Bridge);
    __xrtNetDialFutureBegin(context, &scope);
    promise = context->Promise; context->Promise = NULL;
    context->Publishing = false; context->Completed = true;
    __xrtNetDialFutureEnd(context, &scope);
    xrtPromiseDestroy(promise); xrtFutureDestroy(future); __xrtNetDialFutureDrop(context);
    xrtClearError(); xrtSetErrorTake(error); return NULL;
}

XRT_API xfuture* xrtNetDialAsync(xnetengine* engine,xnetresolver* resolver,
    cstr host,uint16 port,const xnetdialconfig* config,const xnetstreamevents* events,ptr streamData)
{return __xrtNetDialFutureCreate(engine,resolver,host,port,config,events,streamData,NULL);}
XRT_API xfuture* xrtNetDialAddressesAsync(xnetengine* engine,const xnetaddrlist* addresses,
    const xnetdialconfig* config,const xnetstreamevents* events,ptr streamData)
{
    if(!addresses){__xrtErrorSetInvalidArgument();return NULL;}
    return __xrtNetDialFutureCreate(engine,NULL,NULL,0,config,events,streamData,addresses);
}

#endif
