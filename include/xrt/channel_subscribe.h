#ifndef XRT_CHANNEL_SUBSCRIBE_H
#define XRT_CHANNEL_SUBSCRIBE_H

#include <xrt/channel.h>

#if defined(XRT_FEATURE_CHANNEL_SUBSCRIBE) && !defined(XRT_FEATURE_CHANNEL_SELECT)
	#error "XRT_FEATURE_CHANNEL_SUBSCRIBE requires XRT_FEATURE_CHANNEL_SELECT"
#endif

#if defined(XRT_FEATURE_CHANNEL_SUBSCRIBE)

typedef struct xchannelsubscription xchannelsubscription;

/* Copied at Create: no caller output address is retained. Channel and payload
 * remain caller-owned, as for ChannelSelect. Keep every Channel alive until
 * completion and until all concurrent originating Channel calls have returned. */
typedef struct xchannelsubscribecase {
	xchannel* Channel;
	xchannelop Operation;
	ptr Value;
} xchannelsubscribecase;

/* Success consumes ONE real Data reference, failure none. Notify borrows it
 * and runs exactly once, after physical unlink, on Start/Stop or a Channel
 * caller's thread, OUTSIDE all Channel locks. It may reenter and release the
 * subscription's external reference. Drop returns Data exactly once, after all
 * dispatch/call tails and the last external reference retire. Policy and code
 * must outlive that Drop. This raw observer is opaque to ownership collection;
 * merely registering it does not certify its context or invent graph edges. */
typedef struct xchannelsubscribepolicyv1 {
	size_t size;
	void (*Notify)(xchannelsubscription* pSubscription, ptr pData);
	void (*Drop)(const void* pData);
} xchannelsubscribepolicyv1;

XRT_EXTERN_C_BEGIN

/* All storage and case validation precede publication. No Channel operation,
 * callback, worker, coroutine stack or timer is started here. */
XRT_API xchannelsubscription* xrtChannelSubscribeCreateV1(
	const xchannelsubscribecase* pCases, size_t iCount,
	ptr pData, const xchannelsubscribepolicyv1* pPolicy);

/* Admit once. May complete and notify inline before returning. On success no
 * allocation is needed to wait, wake, select, stop or unlink. */
XRT_API bool xrtChannelSubscribeStart(xchannelsubscription* pSubscription);

XRT_API xchannelsubscription* xrtChannelSubscribeRef(xchannelsubscription* pSubscription);

/* Consume one external reference. Last release requests cancellation of a
 * pending operation, but does not undo a committed send/receive. Never waits
 * for a callback; internal dispatch credits delay physical destruction. */
XRT_API void xrtChannelSubscribeDestroy(xchannelsubscription* pSubscription);

/* Record the first CANCELLED/TIMEOUT/ERROR stop request; true means recorded,
 * not that it beat a ready case. Can be requested before Start. Runtime timers
 * call this API: this module does not allocate a per-wait timer/worker/thread. */
XRT_API bool xrtChannelSubscribeStop(xchannelsubscription* pSubscription, xwaitresult iReason);

/* false while pending; outputs remain unchanged. After Ready, copies the
 * immutable result and received raw pointer (NULL also is a valid message).
 * Output addresses are used only during this call, never retained. */
XRT_API bool xrtChannelSubscribeResult(const xchannelsubscription* pSubscription,
	xchannelselectresult* pResult, ptr* pValue);

XRT_EXTERN_C_END

#endif
#endif
