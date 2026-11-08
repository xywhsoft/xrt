#ifndef XRT_INTERNAL_DIAL_FUTURE_H
#define XRT_INTERNAL_DIAL_FUTURE_H

#include "xrt_tcp.h"
#include <xrt/future.h>

#if defined(XRT_FEATURE_NET_DIAL_FUTURE_COMMON)
/* Resident, exact-family operations only. No caller callbacks or borrowed
 * submission arguments survive Create. One shared physical Future producer
 * is used by TCP and Proxy; the operation retains its own native state. */
typedef struct xrt_net_dial_future_policy {
    ptr (*Ref)(ptr operation);
    void (*Destroy)(ptr operation);
    bool (*Cancel)(ptr operation);
    xrtownershipref (*Ownership)(const void* operation);
    const xfuturepayloadownershipv1* Payload;
    void (*AbortValue)(ptr value);
    void (*DropOwner)(const void* owner);
} xrt_net_dial_future_policy;
typedef ptr (*xrt_net_dial_future_submit)(ptr context, const void* arguments);

extern const xrtownershipops __xrtNetStreamDialFutureOps;
extern const xfuturepayloadownershipv1 __xrtNetStreamDialFuturePayloadPolicy;
void __xrtNetStreamDialFutureDrop(const void* data);
void __xrtNetStreamDialFutureComplete(ptr context, xnetresult result,
    ptr value, const xerror* failure);
xfuture* __xrtNetStreamDialFutureCreate(const xrt_net_dial_future_policy* policy,
    xrt_net_dial_future_submit submit, const void* arguments);
/* Success transfers one optional owner credit; failure transfers none.
 * The owner is independently admitted rather than flattening its graph. */
xfuture* __xrtNetStreamDialFutureCreateOwned(const xrt_net_dial_future_policy* policy,
    xrt_net_dial_future_submit submit, const void* arguments, xrtownershipref owner);
xrtownershipref __xrtNetStreamDialFutureOwner(const void* context);
const xfutureproducerownershipv1* __xrtNetStreamDialFutureProducerPolicyV1Get(void);
const xcancelwatchownershipv1* __xrtNetStreamDialFutureCancelPolicyV1Get(void);
const xfuturepayloadownershipv1* __xrtNetStreamDialFuturePayloadPolicyV1Get(void);
const xrtownershipquiescencev1* __xrtNetStreamDialFutureOwnershipQuiescenceV1Get(void);
const xrtownershipadapterv1* __xrtNetStreamDialFutureOwnershipAdapterV1(
    xrtownershipref ref, const xrt_net_dial_future_policy* policy,
    const xrtownershippreparationv1** preparation);
#endif
#endif
