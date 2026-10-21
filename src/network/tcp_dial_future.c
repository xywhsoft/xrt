#include "../internal/xrt_dial_future.h"

#if defined(XRT_FEATURE_NET_TCP_DIAL_FUTURE)

static ptr __xrtNetDialFutureRef(ptr operation)
{ return xrtNetDialRef(operation); }
static void __xrtNetDialFutureDestroy(ptr operation)
{ xrtNetDialDestroy(operation); }
static bool __xrtNetDialFutureCancel(ptr operation)
{ return xrtNetDialCancel(operation); }
static xrtownershipref __xrtNetDialFutureTraceOperation(const void* operation)
{ return xrtNetDialOwnership(operation); }
static const xrt_net_dial_future_policy __xrtNetDialFutureOperations = {
    __xrtNetDialFutureRef, __xrtNetDialFutureDestroy,
    __xrtNetDialFutureCancel, __xrtNetDialFutureTraceOperation
};
static void __xrtNetDialFutureDone(xnetdial* dial, xnetresult result,
    xnetstream* stream, const xerror* failure, ptr data)
{
    (void)dial;
    __xrtNetStreamDialFutureComplete(data, result, stream, failure);
}
static const xnetdialownershipv1 __xrtNetDialFutureDialPolicy = {
    sizeof(xnetdialownershipv1), __xrtNetDialFutureDone,
    __xrtNetStreamDialFutureDrop, &__xrtNetStreamDialFutureOps
};

/* Arguments are borrowed only while the native constructor deep-copies them.
 * A successful submission consumes the actual Done-context reference. */
typedef struct xrt_net_dial_future_arguments {
    xnetengine* Engine;
    xnetresolver* Resolver;
    cstr Host;
    uint16 Port;
    const xnetdialconfig* Config;
    const xnetstreamevents* Events;
    ptr StreamData;
    const xnetaddrlist* Addresses;
} xrt_net_dial_future_arguments;
static ptr __xrtNetDialFutureSubmit(ptr context, const void* data)
{
    const xrt_net_dial_future_arguments* a = data;
    return a->Addresses ?
        xrtNetDialAddressesOwnedV1(a->Engine, a->Addresses, a->Config,
            a->Events, a->StreamData, context, &__xrtNetDialFutureDialPolicy) :
        xrtNetDialOwnedV1(a->Engine, a->Resolver, a->Host, a->Port, a->Config,
            a->Events, a->StreamData, context, &__xrtNetDialFutureDialPolicy);
}
XRT_API xfuture* xrtNetDialAsync(xnetengine* engine, xnetresolver* resolver,
    cstr host, uint16 port, const xnetdialconfig* config,
    const xnetstreamevents* events, ptr streamData)
{
    xrt_net_dial_future_arguments args = {
        engine, resolver, host, port, config, events, streamData, NULL
    };
    return __xrtNetStreamDialFutureCreate(&__xrtNetDialFutureOperations,
        __xrtNetDialFutureSubmit, &args);
}
XRT_API xfuture* xrtNetDialAddressesAsync(xnetengine* engine,
    const xnetaddrlist* addresses, const xnetdialconfig* config,
    const xnetstreamevents* events, ptr streamData)
{
    if (!addresses) { __xrtErrorSetInvalidArgument(); return NULL; }
    xrt_net_dial_future_arguments args = {
        engine, NULL, NULL, 0, config, events, streamData, addresses
    };
    return __xrtNetStreamDialFutureCreate(&__xrtNetDialFutureOperations,
        __xrtNetDialFutureSubmit, &args);
}
XRT_API const xnetdialownershipv1* xrtNetDialFutureDialPolicyV1Get(void)
{ return &__xrtNetDialFutureDialPolicy; }
XRT_API const xfutureproducerownershipv1* xrtNetDialFutureProducerPolicyV1Get(void)
{ return __xrtNetStreamDialFutureProducerPolicyV1Get(); }
XRT_API const xcancelwatchownershipv1* xrtNetDialFutureCancelPolicyV1Get(void)
{ return __xrtNetStreamDialFutureCancelPolicyV1Get(); }
XRT_API const xfuturepayloadownershipv1* xrtNetDialFuturePayloadPolicyV1Get(void)
{ return __xrtNetStreamDialFuturePayloadPolicyV1Get(); }
XRT_API const xrtownershipquiescencev1* xrtNetDialFutureOwnershipQuiescenceV1Get(void)
{ return __xrtNetStreamDialFutureOwnershipQuiescenceV1Get(); }
XRT_API const xrtownershipadapterv1* xrtNetDialFutureOwnershipAdapterV1(
    xrtownershipref ref, const xrtownershippreparationv1** preparation)
{
    return __xrtNetStreamDialFutureOwnershipAdapterV1(ref,
        &__xrtNetDialFutureOperations, preparation);
}
#endif
