#ifndef XRT_CANCEL_H
#define XRT_CANCEL_H

#include <xrt/core.h>
#include <xrt/sync.h>



#if defined(XRT_FEATURE_CANCEL) && !defined(XRT_FEATURE_MUTEX)
	#error "XRT_FEATURE_CANCEL requires XRT_FEATURE_MUTEX"
#endif

#if defined(XRT_FEATURE_CANCEL) && !defined(XRT_FEATURE_COND)
	#error "XRT_FEATURE_CANCEL requires XRT_FEATURE_COND"
#endif



#if defined(XRT_FEATURE_CANCEL)
/* 取消令牌保存一次性取消状态，并可通过不可变父链继承取消。 */
typedef struct xcancel xcancel;



/* 取消监听保存一次回调注册及其并发生命周期。 */
typedef struct xcancelwatch xcancelwatch;



/* 取消回调由命中的取消请求线程或迟注册线程同步执行。 */
typedef void (*xcancelproc)(ptr pData);

/* Certified resident observer. A successful registration consumes ONE real
 * Data reference described by Ops. Notify borrows it; Drop returns it once
 * after Unwatch and every dispatch/plan pin have finished. Both callbacks
 * coordinate their own graph transitions and code lifetime. They run outside
 * this API's mutation scope, never by suspending a caller-owned outer scope.
 * Policy identity is immutable and outlives the registration. A trace alone
 * is not certification, nor permission to cancel or skip accepted work. */
typedef struct xcancelwatchownershipv1 {
	size_t size;
	xcancelproc Notify;
	void (*Drop)(const void* pData);
	const xrtownershipops* Ops;
} xcancelwatchownershipv1;



XRT_EXTERN_C_BEGIN



/* 创建一个独立的取消令牌。 */
XRT_API xcancel* xrtCancelCreate(void);



/* 创建一个继承父令牌取消状态的子令牌；父令牌可为空。 */
XRT_API xcancel* xrtCancelChild(xcancel* pParent);



/* 增加取消令牌引用并返回原指针。 */
XRT_API xcancel* xrtCancelRef(xcancel* pCancel);



/* 释放取消令牌引用；空指针视为空操作。 */
XRT_API void xrtCancelDestroy(xcancel* pCancel);

/* Borrowed physical ownership view; the retained parent is one owning edge.
 * Watch-list links borrow registration storage and are not strong references.
 * Caller provides whole-graph quiescence and code residency. */
XRT_API xrtownershipref xrtCancelOwnership(const xcancel* pCancel);

/* Explicit native adapter, queried under whole-graph freeze. Registered
 * observers are refused before Trace, not treated as empty owning slots.
 * The immutable parent tail remains owned through Finish and final Drop;
 * each parent must independently be admitted. No CancelWatch certification. */
XRT_API const xrtownershipadapterv1* xrtCancelOwnershipAdapterV1(xrtownershipref Reference);



/* 请求取消；仅首次请求返回 true 并触发监听。 */
XRT_API bool xrtCancelRequest(xcancel* pCancel);



/* 查询令牌或任一祖先是否已请求取消；空指针表示未取消。 */
XRT_API bool xrtCancelRequested(const xcancel* pCancel);



/* 监听令牌及其不可变父链；回调至多同步执行一次。 */
XRT_API xcancelwatch* xrtCancelWatch(
	xcancel* pCancel,
	xcancelproc pProc,
	ptr pData
);

/* Failure consumes nothing. Success may notify synchronously if an ancestor
 * is already cancelled, but retains Data until registration release. Legacy
 * Watch remains borrowed/opaque and keeps its conservative callback scope. */
XRT_API xcancelwatch* xrtCancelWatchOwnedV1(xcancel* pCancel, ptr pData,
	const xcancelwatchownershipv1* pPolicy);

/* Whole-graph freeze queries, matching policy identity before dereferencing
 * it or tracing Data. Both lifecycle AND semantic preparation are required.
 * Token list nodes borrow Watch storage: V2 does not invent token->Watch RC
 * edges. Watch owns its Cancel and its certified Data reference independently.
 * Active dispatch, publication/unlink, legacy/unknown observers are refused.
 * Prepare only waits for the owner's actual Unwatch; it never requests
 * cancellation or silently removes an accepted callback. Output preparation
 * remains unchanged on refusal. Data/parents must be independently admitted. */
XRT_API const xrtownershipadapterv1* xrtCancelOwnershipAdapterV2(xrtownershipref Reference,
	const xcancelwatchownershipv1* const* pPolicies, size_t iPolicyCount,
	const xrtownershippreparationv1** ppPreparation);
XRT_API const xrtownershipadapterv1* xrtCancelWatchOwnershipAdapterV1(xrtownershipref Reference,
	const xcancelwatchownershipv1* const* pPolicies, size_t iPolicyCount,
	const xrtownershippreparationv1** ppPreparation);

/* Post-Clear owner-driven retirement. Unlike the earlier adapters, these do
 * not require or perform Unwatch before semantic finalization. Whole-graph
 * freeze admits the same closed owned-policy set and rejects active/unknown
 * observers. Clear prevents new notification without dropping any slot.
 * The actual registration owner must detach and Unwatch during its Finish;
 * Watch completion stays BUSY until that right is consumed and dispatch tails
 * retire. Token completion stays BUSY while linked registrations remain.
 * The returned completion is mandatory and must be admitted by exact Adapter
 * identity; refusal leaves its output unchanged. No legacy observer is opted
 * in, no request is delivered or undone, and abort leaves registration armed. */
XRT_API const xrtownershipadapterv1* xrtCancelWatchOwnershipAdapterV2(xrtownershipref Reference,
	const xcancelwatchownershipv1* const* pPolicies, size_t iPolicyCount,
	const xrtownershipfinishv1** ppCompletion);
XRT_API const xrtownershipadapterv1* xrtCancelOwnershipAdapterV3(xrtownershipref Reference,
	const xcancelwatchownershipv1* const* pPolicies, size_t iPolicyCount,
	const xrtownershipfinishv1** ppCompletion);



/* 查询监听是否已命中取消。 */
XRT_API bool xrtCancelTriggered(const xcancelwatch* pWatch);



/* Borrowed physical view: Unwatch owns this registration; it retains its
 * Cancel (and thereby its parents). Proc/Data and linked cancellation nodes
 * are borrowed, not additional owning slots. Active/destroying callbacks
 * reject inspection. Whole-graph quiescence and code residency are required. */
XRT_API xrtownershipref xrtCancelWatchOwnership(const xcancelwatch* pWatch);



/* 注销并释放监听；从其他线程调用时等待正在执行的回调返回。 */
XRT_API void xrtCancelUnwatch(xcancelwatch* pWatch);

/* Consume the same registration right, reporting whether unlink/retirement
 * was accepted even when no diagnostic can be allocated. NULL succeeds.
 * False leaves the owner's right unconsumed; retry while storage is valid.
 * Self-unwatch succeeds with its physical release deferred until callback
 * return. Success does not imply that all dispatcher references have left.
 * Repeated calls require an independent real lifetime pin; never reuse a
 * pointer whose last right was consumed by an earlier call. */
XRT_API bool xrtCancelUnwatchChecked(xcancelwatch* pWatch);



XRT_EXTERN_C_END
#endif

#endif
