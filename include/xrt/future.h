#ifndef XRT_FUTURE_H
#define XRT_FUTURE_H

#include <xrt/cancel.h>
#include <xrt/error.h>



#if defined(XRT_FEATURE_FUTURE) && !defined(XRT_FEATURE_CANCEL)
	#error "XRT_FEATURE_FUTURE requires XRT_FEATURE_CANCEL"
#endif

#if defined(XRT_FEATURE_FUTURE_COROUTINE) && !defined(XRT_FEATURE_FUTURE)
	#error "XRT_FEATURE_FUTURE_COROUTINE requires XRT_FEATURE_FUTURE"
#endif

#if defined(XRT_FEATURE_FUTURE_COROUTINE) && !defined(XRT_FEATURE_COROUTINE_SCHEDULER)
	#error "XRT_FEATURE_FUTURE_COROUTINE requires XRT_FEATURE_COROUTINE_SCHEDULER"
#endif

#if defined(XRT_FEATURE_FUTURE_COMBINE) && !defined(XRT_FEATURE_FUTURE)
	#error "XRT_FEATURE_FUTURE_COMBINE requires XRT_FEATURE_FUTURE"
#endif

#if defined(XRT_FEATURE_FUTURE_CONTINUE) && !defined(XRT_FEATURE_FUTURE)
	#error "XRT_FEATURE_FUTURE_CONTINUE requires XRT_FEATURE_FUTURE"
#endif



#if defined(XRT_FEATURE_FUTURE)

/* Future 是只读共享结果，Promise 是唯一终态写入端。 */
typedef struct xfuture xfuture;
typedef struct xpromise xpromise;



/* Future 终态明确区分成功、失败、协作取消和生产端关闭。 */
typedef enum xfuturestate {
	XFUTURE_PENDING = 0,
	XFUTURE_RESOLVED = 1,
	XFUTURE_FAILED = 2,
	XFUTURE_CANCELLED = 3,
	XFUTURE_CLOSED = 4
} xfuturestate;



/* Future 结果只借用值和错误，其生命周期由 Future 引用保护。 */
typedef struct xfutureresult {
	xfuturestate State;
	ptr Value;
	const xerror* Error;
} xfutureresult;



/* Future Watch 使用调用方存储提供无分配终态通知。 */
#define XRT_FUTURE_WATCH_STORAGE_SIZE 64u



/* Watch 回调在线程安全的 Future 完成路径中执行，不得重入同一个 Watch。 */
typedef void (*xfuturewatchproc)(ptr pData);



/* Watch 释放过程在线性化完成通知或成功摘除后执行一次。 */
typedef void (*xfuturewatchreleaseproc)(ptr pData);



/* Watch 的内部链表和并发状态保持不透明。 */
typedef union xfuturewatch {
	uint64 Alignment;
	uint8 Storage[XRT_FUTURE_WATCH_STORAGE_SIZE];
} xfuturewatch;



/* 注册结果区分错误、Future 已完成和成功进入等待链。 */
typedef enum xfuturewatchresult {
	XFUTURE_WATCH_ERROR = -1,
	XFUTURE_WATCH_READY = 0,
	XFUTURE_WATCH_PENDING = 1
} xfuturewatchresult;



/* 成功值析构过程接收创建者提供的值和上下文。 */
typedef void (*xfuturefreeproc)(ptr pValue, ptr pData);

/* Describe exactly the owning slots released by an owned result's destructor,
 * including its context. Unique boxes are folded into this Future's edges;
 * shared reference-counted boxes must be reported as physical nodes. */
typedef bool (*xfutureownershiptrace)(const void* pValue, const void* pData,
	xrtownershipvisitor pVisit, ptr pContext);

/* Explicit lifecycle certification, separate from an inspection-only trace.
 * Immutable policy/code outlives the Future. Drop consumes a unique result
 * box and exactly the owning slots described by Trace; context is NULL.
 * Drop coordinates its own graph transitions, runs no new semantic finalizer
 * after child finalization, and may execute outside XRT's mutation scope.
 * Trace is allocation/callback-free apart from the supplied visitor. */
typedef struct xfuturepayloadownershipv1 {
	size_t size;
	xfuturefreeproc Drop;
	xfutureownershiptrace Trace;
} xfuturepayloadownershipv1;

/* One ACTUAL producer reference owned by a pending result, separate from
 * PromiseRefs and from the terminal payload. The immutable resident Drop
 * releases that reference outside this Future's lock/mutation; it coordinates
 * its own transitions and code lifetime. It must not cancel accepted work or
 * substitute for its semantic callback/finally. Completion preserves its own
 * diagnostic across this mechanical release. The physical child is still
 * independently admitted by the collector, never certified by this policy. */
typedef struct xfutureproducerownershipv1 {
	size_t size;
	void (*Drop)(const void* pProducer);
} xfutureproducerownershipv1;

/* A registered Watch owns exactly ONE Data node reference, returned by Release.
 * Immutable resident callbacks coordinate their own activity and code lifetime;
 * Ops describes that same physical node, not a synthetic watch leaf. The core
 * traces this actual slot, but a collector independently admits Data and all
 * of its captures. Legacy traced/phased watches are not silently certified. */
typedef struct xfuturewatchownershipv1 {
	size_t size;
	xfuturewatchproc Notify;
	xfuturewatchreleaseproc Release;
	const xrtownershipops* Ops;
} xfuturewatchownershipv1;

/* The callback address may point inside the physical owner (for example an
 * aggregate input slot). Reference projects that address to the ONE actual
 * reference returned by Release; it neither acquires a reference nor traces
 * captures. The projection is immutable, resident, allocation/lock/callback
 * free and remains valid through the Release tail. It must not invent an
 * alias RC node. Admission matches policy identity before invoking it. */
typedef struct xfuturewatchownershipv2 {
	size_t size;
	xfuturewatchproc Notify;
	xfuturewatchreleaseproc Release;
	xrtownershipref (*Reference)(const void* pData);
} xfuturewatchownershipv2;

/* Closed policy sets for one collector domain. This is admission, not a
 * registry granting unknown Data/code lifecycle authority. Children remain
 * independently admitted. V1-V3 retain their original refusal boundaries. */
typedef struct xfutureownershipadmissionv1 {
	size_t size;
	const xfuturepayloadownershipv1* const* PayloadPolicies;
	size_t PayloadPolicyCount;
	const xfutureproducerownershipv1* const* ProducerPolicies;
	size_t ProducerPolicyCount;
	const xfuturewatchownershipv1* const* WatchPolicies;
	size_t WatchPolicyCount;
	const xfuturewatchownershipv2* const* ProjectedWatchPolicies;
	size_t ProjectedWatchPolicyCount;
	const xcancelwatchownershipv1* const* CancelWatchPolicies;
	size_t CancelWatchPolicyCount;
} xfutureownershipadmissionv1;



XRT_EXTERN_C_BEGIN



/* 创建一对 Future/Promise；父取消令牌为空时使用独立取消源。 */
XRT_API xpromise* xrtPromiseCreate(xfuture** ppFuture, xcancel* pParentCancel);

/* Bind once to a private, pending, newly created Future/Promise pair (one
 * reference per endpoint, no waiters). No allocation or user callback.
 * Success consumes ONE existing Producer reference; failure consumes nothing.
 * The result traces that real edge until terminal publication detaches it;
 * completion releases it exactly once outside lock/mutation before notifying
 * the result's waiters. Merely dropping a Future observer does not cancel work.
 * The caller keeps an independent activation/registration reference until
 * its own callback and release tails finish. An enclosing caller-owned scope
 * is never suspended. A trace-only or unknown policy is not collection proof. */
XRT_API bool xrtPromiseProducerBindTakeV1(xpromise* pPromise, xrtownershipref Producer,
	const xfutureproducerownershipv1* pPolicy);



/* 增加 Promise 生产端引用并返回原指针。 */
XRT_API xpromise* xrtPromiseRef(xpromise* pPromise);



/* 释放生产端引用；最后一个未完成生产端会关闭 Future 并请求取消。 */
XRT_API void xrtPromiseDestroy(xpromise* pPromise);



/* 增加 Future 消费端引用并返回原指针。 */
XRT_API xfuture* xrtFutureRef(xfuture* pFuture);



/* 释放 Future 消费端引用；空指针视为空操作。 */
XRT_API void xrtFutureDestroy(xfuture* pFuture);

/* Borrowed views of the SAME physical control block: every FutureRef and
 * PromiseRef owns one reference. Do not invent a second Promise node.
 * Trace includes cancellation parents, error causes, forwarded source and
 * explicitly described owned payload/context. Unknown owned payloads and
 * registered waiters fail closed unless their complete adapters are supplied.
 * Pending with no waiters is inspectable; producer references remain roots.
 * Whole-graph quiescence through any later commit and callback code residency
 * remain the caller's responsibility; this API does not establish either. */
XRT_API xrtownershipref xrtFutureOwnership(const xfuture* pFuture);
XRT_API xrtownershipref xrtPromiseOwnership(const xpromise* pPromise);

/* Publish an explicitly certified owned result atomically. Failure does not
 * consume the box. This does not authorize collection by itself: a resolver
 * must recognize the exact policy identity and independently admit children. */
XRT_API bool xrtPromiseResolveOwnedPolicyV1(xpromise* pPromise, ptr pValue,
	const xfuturepayloadownershipv1* pPolicy);

/* Query under whole-graph freeze before Count/Trace. Pending, forwarded and
 * terminal control blocks share one physical adapter. Completing operations,
 * all registered waiters, unrecognized owned policies and observed cancel
 * tokens are refused without invoking any payload/waiter trace. An empty
 * allowlist admits only results without an owned payload. Pending retirement
 * preserves last-producer CLOSED/cancellation semantics at Clear, without
 * notification. Finish releases actual retired slots outside freeze. */
XRT_API const xrtownershipadapterv1* xrtFutureOwnershipAdapterV1(xrtownershipref Reference,
	const xfuturepayloadownershipv1* const* pPolicies, size_t iPolicyCount);
/* Add an explicit producer-policy allowlist. V1 continues to refuse any live
 * producer edge. Neither entry admits registered waiters or an unknown child;
 * identity is matched before reading a policy or invoking any child callback. */
XRT_API const xrtownershipadapterv1* xrtFutureOwnershipAdapterV2(xrtownershipref Reference,
	const xfuturepayloadownershipv1* const* pPolicies, size_t iPolicyCount,
	const xfutureproducerownershipv1* const* pProducerPolicies, size_t iProducerPolicyCount);
/* V3 additionally admits exact Watch policies and returns mandatory semantic
 * preparation alongside the SAME lifecycle adapter. Before object Finalize,
 * prepare producerless pending sources through normal CLOSED notification,
 * then rebuild the whole graph. Produced results defer to their real producer;
 * never close an intermediate result before its source's catch/finally runs.
 * Closed dependency cycles may require a further explicit shutdown protocol;
 * BUSY does not authorize clearing a live Watch or skipping accepted work.
 * Output is unchanged on refusal. Use both descriptors, never V1-only planning. */
XRT_API const xrtownershipadapterv1* xrtFutureOwnershipAdapterV3(xrtownershipref Reference,
	const xfuturepayloadownershipv1* const* pPolicies, size_t iPolicyCount,
	const xfutureproducerownershipv1* const* pProducerPolicies, size_t iProducerPolicyCount,
	const xfuturewatchownershipv1* const* pWatchPolicies, size_t iWatchPolicyCount,
	const xrtownershippreparationv1** ppPreparation);
/* Preserve 64-byte Watch storage and READY/PENDING/ERROR ownership rules.
 * Initialization alone does not consume Data; registration commits the same
 * caller-owned reference that Release returns. READY is still caller-driven. */
XRT_API bool xrtFutureWatchInitOwnershipV1(xfuturewatch* pWatch, ptr pData,
	const xfuturewatchownershipv1* pPolicy);
/* Same 64-byte storage and ERROR/READY/PENDING transfer rules as V1. No
 * projection is invoked by initialization or adapter admission. */
XRT_API bool xrtFutureWatchInitOwnershipV2(xfuturewatch* pWatch, ptr pData,
	const xfuturewatchownershipv2* pPolicy);
XRT_API const xrtownershipadapterv1* xrtFutureOwnershipAdapterV4(xrtownershipref Reference,
	const xfutureownershipadmissionv1* pAdmission,
	const xrtownershippreparationv1** ppPreparation);

/* Atomically publish value, destructor, context and non-NULL ownership trace.
 * Success transfers the same ownership as ResolveOwned. Failure (including
 * duplicate completion) transfers nothing and never calls Destroy/Trace.
 * The immutable trace and destructor remain resident for the result lifetime;
 * trace follows the read-only xrtOwnershipInspect callback contract. */
XRT_API bool xrtPromiseResolveOwnedTraced(xpromise* pPromise, ptr pValue,
	xfuturefreeproc pDestroy, ptr pDestroyData, xfutureownershiptrace pTrace);



/* 返回 Future 状态快照；参数无效时返回 CLOSED 并设置错误。 */
XRT_API xfuturestate xrtFutureState(const xfuture* pFuture);



/* 判断 Future 是否已经进入任一不可变终态。 */
XRT_API bool xrtFutureDone(const xfuture* pFuture);



/* 复制借用结果；尚未完成时返回 false 并设置 AGAIN。 */
XRT_API bool xrtFutureResult(const xfuture* pFuture, xfutureresult* pResult);



/* 返回成功值；非成功终态会把对应错误设置到当前执行上下文。 */
XRT_API ptr xrtFutureValue(const xfuture* pFuture);



/* 返回失败终态借用的结构化错误，其他状态返回空指针。 */
XRT_API const xerror* xrtFutureError(const xfuture* pFuture);



/* 请求生产过程协作取消；请求本身不伪造 Future 终态。 */
XRT_API bool xrtFutureCancel(xfuture* pFuture);



/* 返回增加引用后的取消令牌，调用方使用完毕后必须释放。 */
XRT_API xcancel* xrtFutureCancelToken(const xfuture* pFuture);



/* 返回增加引用后的生产端取消令牌。 */
XRT_API xcancel* xrtPromiseCancelToken(const xpromise* pPromise);



/* 初始化一个尚未注册的无分配 Future Watch。 */
XRT_API bool xrtFutureWatchInit(
	xfuturewatch* pWatch,
	xfuturewatchproc pNotify,
	xfuturewatchreleaseproc pRelease,
	ptr pData
);



/* Initialize with an immutable description of the strong slots released by
 * pRelease(pData). Both callbacks are required. Unique context storage is
 * folded into the Future's edges; shared state is a physical node. Init is
 * allocation-free and consumes nothing; only WatchAdd(PENDING) transfers the
 * registration/release right. READY and ERROR keep it with the caller.
 * Storage size and old Init semantics are unchanged. A linked adapter may be
 * inspected only at a whole-graph quiescent point with callback code resident;
 * this does not establish a safepoint or permit concurrent frame inspection. */
XRT_API bool xrtFutureWatchInitTraced(xfuturewatch* pWatch,
    xfuturewatchproc pNotify, xfuturewatchreleaseproc pRelease, ptr pData,
    xrtownershiptrace pTrace);

/* Explicit cooperative callback admission, not inferred from a Trace callback.
 * Like InitTraced, but Notify/Release run outside the Future mutation domain.
 * The resident callbacks must coordinate every count/edge/storage transition,
 * reject inspection of active/private data, and independently pin their code.
 * Detached notification/release callers retain their real ownership until the
 * callback returns. No mutation scope may span a wait or arbitrary callback.
 * Unphased Watch, internal waiters and payload finalizers keep conservative
 * mutation scopes. Storage size, READY/PENDING/ERROR and release rules match
 * InitTraced; READY still leaves Notify/Release to the registering caller. */
XRT_API bool xrtFutureWatchInitPhased(xfuturewatch* pWatch,
    xfuturewatchproc pNotify, xfuturewatchreleaseproc pRelease, ptr pData,
    xrtownershiptrace pTrace);



/* Future 未完成时注册 Watch；READY 时 Watch 未被接管且不执行 Release。 */
XRT_API xfuturewatchresult xrtFutureWatchAdd(
	xfuture* pFuture,
	xfuturewatch* pWatch
);



/* 尝试摘除尚未开始通知的 Watch；成功时同步执行 Release。 */
XRT_API bool xrtFutureWatchDetach(
	xfuture* pFuture,
	xfuturewatch* pWatch
);



/* 摘除 Watch 并等待已经开始的通知结束；禁止从自身通知中调用。 */
XRT_API void xrtFutureWatchRemove(
	xfuture* pFuture,
	xfuturewatch* pWatch
);



/* 等待 Future 进入任一终态。 */
XRT_API xwaitresult xrtFutureWait(xfuture* pFuture);



/* 在相对微秒数内等待 Future 进入任一终态。 */
XRT_API xwaitresult xrtFutureWaitFor(xfuture* pFuture, uint64 iTimeout);



/* 等待 Future 到指定单调时钟截止时间。 */
XRT_API xwaitresult xrtFutureWaitUntil(xfuture* pFuture, xdeadline iDeadline);



/* 等待首个线性化事件；取消先取得等待锁后不会被迟到终态覆盖。 */
XRT_API xwaitresult xrtFutureWaitUntilCancel(
	xfuture* pFuture,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 以借用方式完成成功结果，值的生命周期由调用方保证。 */
XRT_API bool xrtPromiseResolve(xpromise* pPromise, ptr pValue);



/* 转移成功值所有权；完成失败时所有权仍归调用方。 */
XRT_API bool xrtPromiseResolveOwned(
	xpromise* pPromise,
	ptr pValue,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
);



/* 以增加引用方式完成失败结果。 */
XRT_API bool xrtPromiseReject(xpromise* pPromise, const xerror* pError);



/* 把已进入终态的源 Future 结果安全透传到 Promise。 */
XRT_API bool xrtPromiseForward(xpromise* pPromise, xfuture* pSource);



/* 完成取消终态；令牌请求通知结束后才向等待者发布取消终态。 */
XRT_API bool xrtPromiseCancel(xpromise* pPromise);



/* 请求生产过程停止，并在令牌通知结束后发布关闭终态。 */
XRT_API bool xrtPromiseClose(xpromise* pPromise);



/* 判断 Promise 对应的 Future 是否已经完成。 */
XRT_API bool xrtPromiseDone(const xpromise* pPromise);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_FUTURE_COMBINE)

/* Any 与 Race 的结果借用胜出源 Future；组合 Future 负责保留该引用。 */
typedef struct xfuturepick {
	size_t Index;
	xfuture* Future;
} xfuturepick;



/* All 的结果按输入顺序借用全部源 Future；组合 Future 负责保留这些引用。 */
typedef struct xfutureall {
	size_t Count;
	xfuture* const* Futures;
} xfutureall;



XRT_EXTERN_C_BEGIN



/* 在任一源进入终态后，以 xfuturepick 成功完成；不改变其余源。 */
XRT_API xfuture* xrtFutureAny(xfuture* const* pFutures, size_t iCount);



/* 在全部源进入终态后，以保序的 xfutureall 成功完成；空集合立即完成。 */
XRT_API xfuture* xrtFutureAll(xfuture* const* pFutures, size_t iCount);



/* 在任一源进入终态后完成，并向其余未完成源发出协作取消请求。 */
XRT_API xfuture* xrtFutureRace(xfuture* const* pFutures, size_t iCount);

/* Synchronous result mapping is part of the aggregate's activation, not a
 * separately allocated continuation attached after the sources start.
 * Inputs and output Promise are borrowed for the call. The mapper must
 * complete/forward the output before returning; an uncompleted output closes.
 * It must not retain the input descriptors or defer work on this Promise. */
typedef void (*xfutureallmapproc)(const xfutureall* pInput, xpromise* pOutput, ptr pData);
typedef void (*xfuturepickmapproc)(const xfuturepick* pInput, xpromise* pOutput, ptr pData);

/* All preparation succeeds before source notification/cancellation is possible.
 * NULL return does not consume data, invoke map/destroy/trace, or cancel inputs.
 * Non-NULL accepts data even if synchronous mapping fails: that failure is the
 * returned Future's outcome. Destroy(data, destroyData) runs exactly once after
 * mapping or cancellation and after the last source callback returns.
 * Trace(data, destroyData) describes the exact strong slots Destroy releases;
 * callbacks/code and borrowed pointers are not fictitious owning edges.
 * Destroy and Trace are required, including for an empty context. The caller
 * still supplies whole-graph quiescence and callback/code residency.
 * Any/All/Race retain their existing selection/order/cancellation contracts. */
XRT_API xfuture* xrtFutureAllMapOwnedTraced(xfuture* const* pFutures, size_t iCount,
    xfutureallmapproc pMap, ptr pData, xfuturefreeproc pDestroy,
    ptr pDestroyData, xfutureownershiptrace pTrace);
XRT_API xfuture* xrtFutureAnyMapOwnedTraced(xfuture* const* pFutures, size_t iCount,
    xfuturepickmapproc pMap, ptr pData, xfuturefreeproc pDestroy,
    ptr pDestroyData, xfutureownershiptrace pTrace);
XRT_API xfuture* xrtFutureRaceMapOwnedTraced(xfuture* const* pFutures, size_t iCount,
    xfuturepickmapproc pMap, ptr pData, xfuturefreeproc pDestroy,
    ptr pDestroyData, xfutureownershiptrace pTrace);

/* A certified mapper owns one independently admitted physical Data reference.
 * AllMap/PickMap borrow it and must synchronously finish the output. Drop
 * returns that reference, never substitutes for Data's semantic preparation.
 * Policy and callbacks are immutable/resident, and callbacks coordinate their
 * own graph transitions and code lifetime outside API-owned scopes. */
typedef struct xfuturecombineownershipv1 {
	size_t size;
	xfutureallmapproc AllMap;
	xfuturepickmapproc PickMap;
	void (*Drop)(const void* pData);
	const xrtownershipops* Ops;
} xfuturecombineownershipv1;
XRT_API xfuture* xrtFutureAllMapOwnedPolicyV1(xfuture* const* pFutures, size_t iCount,
	ptr pData, const xfuturecombineownershipv1* pPolicy);
XRT_API xfuture* xrtFutureAnyMapOwnedPolicyV1(xfuture* const* pFutures, size_t iCount,
	ptr pData, const xfuturecombineownershipv1* pPolicy);
XRT_API xfuture* xrtFutureRaceMapOwnedPolicyV1(xfuture* const* pFutures, size_t iCount,
	ptr pData, const xfuturecombineownershipv1* pPolicy);

/* These exact resident policies describe real producer, source-registration,
 * cancellation and raw-result references. Their presence does not admit the
 * group or its Data: the collector independently resolves every node. */
XRT_API const xfutureproducerownershipv1* xrtFutureCombineProducerPolicyV1Get(void);
XRT_API const xfuturewatchownershipv2* xrtFutureCombineWatchPolicyV2Get(void);
XRT_API const xcancelwatchownershipv1* xrtFutureCombineCancelPolicyV1Get(void);
XRT_API const xfuturepayloadownershipv1* xrtFutureCombineAllPayloadPolicyV1Get(void);
XRT_API const xfuturepayloadownershipv1* xrtFutureCombinePickPayloadPolicyV1Get(void);
XRT_API const xrtownershipadapterv1* xrtFutureCombineOwnershipAdapterV1(xrtownershipref Reference,
	const xfuturecombineownershipv1* const* pPolicies, size_t iPolicyCount,
	const xrtownershippreparationv1** ppPreparation);

typedef enum xfuturewaitrulev1 {
	XFUTURE_WAIT_ALL_TERMINAL = 1,
	XFUTURE_WAIT_ANY_TERMINAL = 2
} xfuturewaitrulev1;
typedef struct xfuturecombinewaitv1 {
	size_t size;
	xpromise* Output;
	xfuture* const* Sources;
	size_t Count;
	xfuturewaitrulev1 Rule;
	bool CancelRemaining;
} xfuturecombinewaitv1;
/* Borrowed semantic input slots, not arbitrary capture edges. The complete
 * physical graph must be frozen and claimed unreachable by the same token.
 * Only a stable pending group with real pending registrations is described;
 * all completed slots are retained in order, including duplicates. ALL needs
 * every input terminal; ANY/RACE need one, with Race alone cancelling losers.
 * Output stays unchanged on refusal. This read-only fact is not cancellation
 * authority, a deadlock proof, or permission to skip normal callbacks. */
XRT_API bool xrtFutureCombineWaitV1(xrtownershipref Reference, const void* pToken,
	xfuturecombinewaitv1* pWait);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_FUTURE_CONTINUE)

/* 延续过程借用源结果和输出 Promise；保留 Promise 时必须先增加引用。 */
typedef void (*xfuturecontinueproc)(
	const xfutureresult* pInput,
	xpromise* pOutput,
	ptr pData
);



/* Finally 过程只观察源结果，输出 Future 自动安全透传源终态。 */
typedef void (*xfuturefinallyproc)(const xfutureresult* pInput, ptr pData);



XRT_EXTERN_C_BEGIN



/* 对源的任意终态执行延续过程；过程负责完成或保留输出 Promise。 */
XRT_API xfuture* xrtFutureContinue(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData
);



/* 执行任意终态延续，并在执行、跳过或取消后释放受理的数据。 */
XRT_API xfuture* xrtFutureContinueOwned(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
);



/*
	对独占源的任意终态执行延续；取消输出时同时请求取消源。
	该入口只适用于调用方拥有完整生产链、不与其他消费者共享源的组合层。
*/
XRT_API xfuture* xrtFutureContinueOwnedCancelSource(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
);



/* 仅在源成功时执行延续；其他终态自动透传。 */
XRT_API xfuture* xrtFutureThen(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData
);



/* 仅在源成功时执行延续，并负责释放受理的数据。 */
XRT_API xfuture* xrtFutureThenOwned(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
);



/*
	仅在独占源成功时执行延续；取消输出时同时请求取消源。
	源的失败、取消和关闭仍按 Then 契约自动透传。
*/
XRT_API xfuture* xrtFutureThenOwnedCancelSource(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
);



/* 仅在源失败时执行延续；成功、取消和关闭终态自动透传。 */
XRT_API xfuture* xrtFutureCatch(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData
);



/* 仅在源失败时执行延续，并负责释放受理的数据。 */
XRT_API xfuture* xrtFutureCatchOwned(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
);



/* 观察源的任意终态，再把原结果安全透传到输出 Future。 */
XRT_API xfuture* xrtFutureFinally(
	xfuture* pSource,
	xfuturefinallyproc pProc,
	ptr pData
);



/* 观察源的任意终态、透传结果，并负责释放受理的数据。 */
XRT_API xfuture* xrtFutureFinallyOwned(
	xfuture* pSource,
	xfuturefinallyproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_FUTURE_COROUTINE)

XRT_EXTERN_C_BEGIN



/* 挂起当前调度协程，直到 Future 进入终态或协程被取消。 */
XRT_API xwaitresult xrtFutureAwait(xfuture* pFuture);



/* 在相对微秒数内挂起当前调度协程等待 Future。 */
XRT_API xwaitresult xrtFutureAwaitFor(xfuture* pFuture, uint64 iTimeout);



/* 挂起当前调度协程等待 Future 到指定截止时间。 */
XRT_API xwaitresult xrtFutureAwaitUntil(xfuture* pFuture, xdeadline iDeadline);



XRT_EXTERN_C_END

#endif

#endif
