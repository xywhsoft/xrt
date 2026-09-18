#include "../internal/xrt_net_resolver.h"
#include <xrt/future_bridge.h>

#if defined(XRT_FEATURE_NET_RESOLVER_FUTURE)

/* One physical producer. Promise is an actual owning endpoint; Bridge borrows
 * that endpoint and owns the cancellation Watch, never a second Promise.
 * Constructor, producer, request and Watch each hold a real context reference. */
typedef struct xrt_net_resolver_future {
    xfuturebridge Bridge;
    volatile int32 References;
    xmutex Lock;
    size_t Active;
    bool Publishing, Completed, Cleared;
    const void* Claim;
    xpromise* Promise;
    xnetresolveop* Operation;
} xrt_net_resolver_future;

static void __xrtNetResolverFutureBegin(xrt_net_resolver_future* context, xrtownershipscope* scope)
{
    if (!xrtOwnershipMutationBegin(scope) || !xrtMutexLock(&context->Lock)) abort();
}
static void __xrtNetResolverFutureEnd(xrt_net_resolver_future* context, xrtownershipscope* scope)
{
    if (!xrtMutexUnlock(&context->Lock) || !xrtOwnershipScopeEnd(scope)) abort();
}
static bool __xrtNetResolverFutureCount(const void* data, size_t* count)
{
    const xrt_net_resolver_future* context = data; xrtownershipref watch; int32 references;
    if (!context || !count || context->Publishing || context->Active || context->Cleared ||
        !xrtFutureBridgeWatchOwnershipV1(&context->Bridge, &watch)) return false;
    references = __xrtAtomicRefLoad(&context->References);
    if (references <= 0) return false;
    *count = (size_t)references; return true;
}
static bool __xrtNetResolverFutureTrace(const void* data, xrtownershipvisitor visit, ptr visitor)
{
    const xrt_net_resolver_future* context = data; xrtownershipref watch; size_t count;
    if (!visit || !__xrtNetResolverFutureCount(data, &count) ||
        !xrtFutureBridgeWatchOwnershipV1(&context->Bridge, &watch)) return false;
    if (context->Promise && !visit(xrtPromiseOwnership(context->Promise), visitor)) return false;
    if (context->Operation && !visit(xrtNetResolveOpOwnership(context->Operation), visitor)) return false;
    return !watch.Data || visit(watch, visitor);
}
static const xrtownershipops __xrtNetResolverFutureOps = {
    __xrtNetResolverFutureCount, __xrtNetResolverFutureTrace
};
static xrtownershipref __xrtNetResolverFutureOwnership(const xrt_net_resolver_future* context)
{ return (xrtownershipref){context, context ? &__xrtNetResolverFutureOps : NULL}; }
static bool __xrtNetResolverFutureHold(const void* data)
{
    xrt_net_resolver_future* context = (xrt_net_resolver_future*)data;
    xrtownershipscope scope = {0}; bool held;
    __xrtNetResolverFutureBegin(context, &scope);
    held = !context->Cleared && xrtRefRetain(&context->References) > 0;
    __xrtNetResolverFutureEnd(context, &scope); return held;
}
static void __xrtNetResolverFutureDrop(const void* data)
{
    xrt_net_resolver_future* context = (xrt_net_resolver_future*)data;
    xrtownershipscope scope = {0}; xrtownershipref watch; int32 left;
    __xrtNetResolverFutureBegin(context, &scope); left = xrtRefRelease(&context->References);
    if (left < 0 || (!left && (context->Publishing || !context->Completed || context->Active ||
        context->Promise || context->Operation ||
        !xrtFutureBridgeWatchOwnershipV1(&context->Bridge, &watch) || watch.Data))) abort();
    __xrtNetResolverFutureEnd(context, &scope);
    if (!left) {
        if (!xrtOwnershipMutationBegin(&scope)) abort();
        if (!xrtMutexUnit(&context->Lock)) abort();
        xrtFree(context);
        if (!xrtOwnershipScopeEnd(&scope)) abort();
    }
}

/* The terminal payload owns exactly one immutable address-list reference. */
static void __xrtNetResolverFutureFree(ptr value, ptr data)
{
    if (data) abort();
    xrtNetAddrListDestroy((xnetaddrlist*)value);
}
static bool __xrtNetResolverFuturePayloadTrace(const void* value, const void* data,
    xrtownershipvisitor visit, ptr visitor)
{ return value && !data && visit && visit(xrtNetAddrListOwnership(value), visitor); }
static const xfuturepayloadownershipv1 __xrtNetResolverFuturePayloadPolicy = {
    sizeof(xfuturepayloadownershipv1), __xrtNetResolverFutureFree, __xrtNetResolverFuturePayloadTrace
};
static const xfutureproducerownershipv1 __xrtNetResolverFutureProducerPolicy = {
    sizeof(xfutureproducerownershipv1), __xrtNetResolverFutureDrop
};

/* The Watch owns this activation. Independently retain the Operation while
 * locked, so normal completion can move/release its field concurrently. */
static void __xrtNetResolverFutureCancel(ptr data)
{
    xrt_net_resolver_future* context = data; xrtownershipscope scope = {0}; xnetresolveop* operation;
    xerror* previous = xrtTakeError();
    __xrtNetResolverFutureBegin(context, &scope); ++context->Active;
    operation = context->Operation ? xrtNetResolveOpRef(context->Operation) : NULL;
    __xrtNetResolverFutureEnd(context, &scope);
    if (operation) { (void)xrtNetResolveOpCancel(operation); xrtNetResolveOpDestroy(operation); }
    __xrtNetResolverFutureBegin(context, &scope); --context->Active;
    __xrtNetResolverFutureEnd(context, &scope);
    xrtClearError(); xrtSetErrorTake(previous);
}
static const xcancelwatchownershipv1 __xrtNetResolverFutureCancelPolicy = {
    sizeof(xcancelwatchownershipv1), __xrtNetResolverFutureCancel,
    __xrtNetResolverFutureDrop, &__xrtNetResolverFutureOps
};

/* The request owns the activation through Done AND its release tail.
 * Acquire Ready before reading constructor-published fields. Waits, callbacks,
 * Promise notification and semantic releases run outside the frame lock and
 * shared mutation scope. No resolver request is accepted on setup failure. */
static void __xrtNetResolverFutureDone(xnetresolveop* operation, ptr data)
{
    xrt_net_resolver_future* context = data; xrtownershipscope scope = {0};
    xpromise* promise; xnetresolveop* held; xnetaddrlist* addresses = NULL;
    xerror* error = NULL; xerror* previous = xrtTakeError(); xnetresolveopstate state;
    bool ready = xrtFutureBridgeWait(&context->Bridge);
    __xrtNetResolverFutureBegin(context, &scope); ++context->Active;
    __xrtNetResolverFutureEnd(context, &scope);
    xrtFutureBridgeUnwatch(&context->Bridge);
    state = xrtNetResolveOpState(operation);
    if (state == XNET_RESOLVE_RESOLVED) {
        addresses = xrtNetResolveOpResult(operation);
        if (!addresses) {
            __xrtNetSetError(XERR_INTERNAL, XNET_ERROR_RESOLVER_QUERY, "complete-resolver-future",
                "resolver reported success without an address list", 0);
            error = xrtTakeError();
        }
    } else if (state == XNET_RESOLVE_FAILED) {
        error = xrtErrorRef(xrtNetResolveOpError(operation));
        if (!error) {
            __xrtNetSetError(XERR_INTERNAL, XNET_ERROR_RESOLVER_QUERY, "complete-resolver-future",
                "resolver failed without an error", 0);
            error = xrtTakeError();
        }
    }
    __xrtNetResolverFutureBegin(context, &scope);
    promise = context->Promise; context->Promise = NULL;
    held = context->Operation; context->Operation = NULL; context->Completed = true;
    __xrtNetResolverFutureEnd(context, &scope);
    xrtNetResolveOpDestroy(held);
    if (!ready) xrtNetAddrListDestroy(addresses);
    else if (state == XNET_RESOLVE_RESOLVED && addresses) {
        if (!xrtPromiseResolveOwnedPolicyV1(promise, addresses, &__xrtNetResolverFuturePayloadPolicy))
            xrtNetAddrListDestroy(addresses);
    } else if (error) (void)xrtPromiseReject(promise, error);
    else if (state == XNET_RESOLVE_FAILED || state == XNET_RESOLVE_RESOLVED) (void)xrtPromiseClose(promise);
    else (void)xrtPromiseCancel(promise);
    xrtErrorFree(error); xrtPromiseDestroy(promise);
    __xrtNetResolverFutureBegin(context, &scope); --context->Active;
    __xrtNetResolverFutureEnd(context, &scope);
    xrtClearError(); xrtSetErrorTake(previous);
}
static const xnetresolveownershipv1 __xrtNetResolverFutureRequestPolicy = {
    sizeof(xnetresolveownershipv1), __xrtNetResolverFutureDone,
    __xrtNetResolverFutureDrop, &__xrtNetResolverFutureOps
};
XRT_API const xfutureproducerownershipv1* xrtNetResolverFutureProducerPolicyV1Get(void)
{ return &__xrtNetResolverFutureProducerPolicy; }
XRT_API const xcancelwatchownershipv1* xrtNetResolverFutureCancelPolicyV1Get(void)
{ return &__xrtNetResolverFutureCancelPolicy; }
XRT_API const xnetresolveownershipv1* xrtNetResolverFutureRequestPolicyV1Get(void)
{ return &__xrtNetResolverFutureRequestPolicy; }
XRT_API const xfuturepayloadownershipv1* xrtNetResolverFuturePayloadPolicyV1Get(void)
{ return &__xrtNetResolverFuturePayloadPolicy; }

static bool __xrtNetResolverFutureClaim(const void* data, const void* token)
{
    xrt_net_resolver_future* context = (xrt_net_resolver_future*)data; size_t count;
    if (!token || !__xrtNetResolverFutureCount(data, &count) ||
        (context->Claim && context->Claim != token)) return false;
    context->Claim = token; return true;
}
static void __xrtNetResolverFutureRestore(const void* data, const void* token)
{
    xrt_net_resolver_future* context = (xrt_net_resolver_future*)data;
    if (!token || context->Claim != token || context->Cleared) abort();
    context->Claim = NULL;
}
static bool __xrtNetResolverFutureReady(const void* data)
{
    const xrt_net_resolver_future* context = data; size_t count; xrtownershipref watch;
    return __xrtNetResolverFutureCount(data, &count) && context->Completed && !context->Promise &&
        !context->Operation && xrtFutureBridgeWatchOwnershipV1(&context->Bridge, &watch) && !watch.Data;
}
static xrtownershipprepareresult __xrtNetResolverFuturePrepare(const void* data, const void* token)
{
    const xrt_net_resolver_future* context = data; xrtownershipscope freeze = {0}; bool ready;
    if (!xrtOwnershipFreezeTryBegin(&freeze)) return XRT_OWNERSHIP_PREPARE_BUSY;
    if (!token || context->Claim != token || context->Cleared) abort();
    ready = __xrtNetResolverFutureReady(data);
    if (!xrtOwnershipScopeEnd(&freeze)) abort();
    return ready ? XRT_OWNERSHIP_PREPARE_READY : XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtNetResolverFutureClear(const void* data, const void* token)
{
    xrt_net_resolver_future* context = (xrt_net_resolver_future*)data;
    if (!token || context->Claim != token || context->Cleared || !__xrtNetResolverFutureReady(data)) abort();
    context->Cleared = true;
}
XRT_API const xrtownershipadapterv1* xrtNetResolverFutureOwnershipAdapterV1(xrtownershipref ref,
    const xrtownershippreparationv1** preparation)
{
    static const xrtownershipadapterv1 adapter = {sizeof(adapter), __xrtNetResolverFutureHold,
        __xrtNetResolverFutureDrop, __xrtNetResolverFutureClaim, __xrtNetResolverFutureRestore,
        NULL, __xrtNetResolverFutureClear, NULL};
    static const xrtownershippreparationv1 prepare = {sizeof(prepare), &adapter,
        __xrtNetResolverFutureReady, __xrtNetResolverFuturePrepare};
    size_t count;
    if (!preparation || ref.Ops != &__xrtNetResolverFutureOps || !__xrtNetResolverFutureCount(ref.Data, &count)) return NULL;
    *preparation = &prepare; return &adapter;
}

XRT_API xfuture* xrtNetResolveAsync(xnetresolver* resolver, cstr host, xnetfamily family)
{
    xrt_net_resolver_future* context; xfuture* future; xpromise* promise;
    xnetresolveop* operation; xerror* error; xrtownershipscope scope = {0};
    context = xrtCalloc(1, sizeof(*context));
    if (!context) return NULL;
    context->References = 1; context->Publishing = true;
    if (!xrtMutexInit(&context->Lock)) { xrtFree(context); return NULL; }
    future = xrtFutureBridgeCreate(&context->Bridge, NULL);
    if (!future) { (void)xrtMutexUnit(&context->Lock); xrtFree(context); return NULL; }
    __xrtNetResolverFutureBegin(context, &scope);
    context->Promise = xrtFutureBridgePromise(&context->Bridge);
    __xrtNetResolverFutureEnd(context, &scope);
    if (!__xrtNetResolverFutureHold(context)) abort();
    if (!xrtPromiseProducerBindTakeV1(context->Promise, __xrtNetResolverFutureOwnership(context),
        &__xrtNetResolverFutureProducerPolicy)) { __xrtNetResolverFutureDrop(context); goto failed; }
    if (!__xrtNetResolverFutureHold(context)) abort();
    if (!xrtFutureBridgeWatchOwnedV1(&context->Bridge, context,
        &__xrtNetResolverFutureCancelPolicy)) { __xrtNetResolverFutureDrop(context); goto failed; }
    /* No fallible bridge setup remains after work acceptance. Resolve consumes
     * this reference only on success; an immediate callback waits for Ready. */
    if (!__xrtNetResolverFutureHold(context)) abort();
    operation = xrtNetResolverResolveOwnedV1(resolver, host, family, context, &__xrtNetResolverFutureRequestPolicy);
    if (!operation) { __xrtNetResolverFutureDrop(context); goto failed; }
    __xrtNetResolverFutureBegin(context, &scope);
    context->Operation = operation; context->Publishing = false;
    __xrtNetResolverFutureEnd(context, &scope);
    if (!xrtFutureBridgeReady(&context->Bridge)) abort();
    __xrtNetResolverFutureDrop(context); return future;
failed:
    error = xrtTakeError();
    if (!xrtFutureBridgeFail(&context->Bridge)) abort();
    xrtFutureBridgeUnwatch(&context->Bridge);
    __xrtNetResolverFutureBegin(context, &scope);
    promise = context->Promise; context->Promise = NULL;
    context->Publishing = false; context->Completed = true;
    __xrtNetResolverFutureEnd(context, &scope);
    xrtPromiseDestroy(promise); xrtFutureDestroy(future); __xrtNetResolverFutureDrop(context);
    xrtClearError(); xrtSetErrorTake(error); return NULL;
}

#endif
