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

/* Explicit resident ownership contract, not an extension of the raw policy.
 * ONE real Data reference is consumed on success. Ops describes that same
 * physical owner and all its owning children; no trace-only callback is opted
 * in. Drop mechanically returns that reference, without starting a new semantic
 * finalizer after graph Clear. Notify/Drop coordinate their own mutation/code
 * lifetime and must return normally. Both execute outside API-owned scopes.
 * Channels and messages remain BORROWED, never synthetic graph edges. */
typedef struct xchannelsubscribeownershipv1 {
	size_t size;
	void (*Notify)(xchannelsubscription* pSubscription, ptr pData);
	void (*Drop)(const void* pData);
	const xrtownershipops* Ops;
} xchannelsubscribeownershipv1;

XRT_EXTERN_C_BEGIN

/* All storage and case validation precede publication. No Channel operation,
 * callback, worker, coroutine stack or timer is started here. */
XRT_API xchannelsubscription* xrtChannelSubscribeCreateV1(
	const xchannelsubscribecase* pCases, size_t iCount,
	ptr pData, const xchannelsubscribepolicyv1* pPolicy);

XRT_API xchannelsubscription* xrtChannelSubscribeCreateOwnedV1(
	const xchannelsubscribecase* pCases, size_t iCount,
	ptr pData, const xchannelsubscribeownershipv1* pPolicy);

/* Read-only physical views under independently held whole-graph freeze.
 * A live registration owns ONE execution credit in addition to external
 * handles. It is an actual pending-work root, not an incoming edge invented
 * for each borrowed Channel/case. No collection API cancels accepted work.
 * Raw-policy contexts are refused. Adapter admission matches the exact policy
 * identity before reading it; its Data remains independently admitted.
 * Active dispatch/publication refuses inspection. Clear is legal only before
 * Start or after completion; Finish returns the Data reference outside freeze.
 * Ref/Start/Stop quarantine new admission while claimed, Restore reopens it. */
XRT_API xrtownershipref xrtChannelSubscribeOwnership(const xchannelsubscription* pSubscription);
XRT_API const xrtownershipquiescencev1* xrtChannelSubscribeOwnershipQuiescenceV1Get(void);
XRT_API const xrtownershipadapterv1* xrtChannelSubscribeOwnershipAdapterV1(
	xrtownershipref Reference,
	const xchannelsubscribeownershipv1* const* pPolicies, size_t iPolicyCount);

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
