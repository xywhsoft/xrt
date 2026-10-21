#include "../internal/xrt_dial_future.h"
#include <xrt/proxy.h>

#if defined(XRT_FEATURE_NET_PROXY_DIAL_FUTURE)

static ptr __xrtNetProxyDialFutureRef(ptr operation)
{ return xrtNetProxyDialRef(operation); }
static void __xrtNetProxyDialFutureDestroy(ptr operation)
{ xrtNetProxyDialDestroy(operation); }
static bool __xrtNetProxyDialFutureCancel(ptr operation)
{ return xrtNetProxyDialCancel(operation); }
static xrtownershipref __xrtNetProxyDialFutureTraceOperation(const void* operation)
{ return xrtNetProxyDialOwnership(operation); }
static const xrt_net_dial_future_policy __xrtNetProxyDialFutureOperations = {
    __xrtNetProxyDialFutureRef, __xrtNetProxyDialFutureDestroy,
    __xrtNetProxyDialFutureCancel, __xrtNetProxyDialFutureTraceOperation
};
static void __xrtNetProxyDialFutureDone(xnetproxydial* dial, xnetresult result,
    xnetstream* stream, const xerror* failure, ptr data)
{
    (void)dial;
    __xrtNetStreamDialFutureComplete(data, result, stream, failure);
}
static const xnetproxydialownershipv1 __xrtNetProxyDialFutureDialPolicy = {
    sizeof(xnetproxydialownershipv1), __xrtNetProxyDialFutureDone,
    __xrtNetStreamDialFutureDrop, &__xrtNetStreamDialFutureOps
};
typedef struct xrt_net_proxy_dial_future_arguments {
    xnetengine* Engine;
    xnetresolver* Resolver;
    const xnetproxy* Proxy;
    cstr TargetHost;
    uint16 TargetPort;
    const xnetproxydialconfig* Config;
    const xnetstreamevents* Events;
    ptr StreamData;
} xrt_net_proxy_dial_future_arguments;
static ptr __xrtNetProxyDialFutureSubmit(ptr context, const void* data)
{
    const xrt_net_proxy_dial_future_arguments* a = data;
    return xrtNetProxyDialOwnedV1(a->Engine, a->Resolver, a->Proxy,
        a->TargetHost, a->TargetPort, a->Config, a->Events, a->StreamData,
        context, &__xrtNetProxyDialFutureDialPolicy);
}
XRT_API xfuture* xrtNetProxyDialAsync(xnetengine* engine, xnetresolver* resolver,
    const xnetproxy* proxy, cstr targetHost, uint16 targetPort,
    const xnetproxydialconfig* config, const xnetstreamevents* events, ptr streamData)
{
    xrt_net_proxy_dial_future_arguments args = {
        engine, resolver, proxy, targetHost, targetPort, config, events, streamData
    };
    return __xrtNetStreamDialFutureCreate(&__xrtNetProxyDialFutureOperations,
        __xrtNetProxyDialFutureSubmit, &args);
}
XRT_API const xnetproxydialownershipv1* xrtNetProxyDialFutureDialPolicyV1Get(void)
{ return &__xrtNetProxyDialFutureDialPolicy; }
XRT_API const xfutureproducerownershipv1* xrtNetProxyDialFutureProducerPolicyV1Get(void)
{ return __xrtNetStreamDialFutureProducerPolicyV1Get(); }
XRT_API const xcancelwatchownershipv1* xrtNetProxyDialFutureCancelPolicyV1Get(void)
{ return __xrtNetStreamDialFutureCancelPolicyV1Get(); }
XRT_API const xfuturepayloadownershipv1* xrtNetProxyDialFuturePayloadPolicyV1Get(void)
{ return __xrtNetStreamDialFuturePayloadPolicyV1Get(); }
XRT_API const xrtownershipquiescencev1* xrtNetProxyDialFutureOwnershipQuiescenceV1Get(void)
{ return __xrtNetStreamDialFutureOwnershipQuiescenceV1Get(); }
XRT_API const xrtownershipadapterv1* xrtNetProxyDialFutureOwnershipAdapterV1(
    xrtownershipref ref, const xrtownershippreparationv1** preparation)
{
    return __xrtNetStreamDialFutureOwnershipAdapterV1(ref,
        &__xrtNetProxyDialFutureOperations, preparation);
}
#endif
