#include "../internal/xrt_tls_stream.h"



#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)

/* 每个节点只表达一次完整发送、拉取接收或条件观察。 */
typedef enum __xrt_tls_stream_async_kind {
	__XRT_TLS_STREAM_ASYNC_SEND = 1,
	__XRT_TLS_STREAM_ASYNC_RECV,
	__XRT_TLS_STREAM_ASYNC_WAIT
} __xrt_tls_stream_async_kind;



/* Worker 把节点线性化到唯一 Future 终态。 */
typedef enum __xrt_tls_stream_async_result {
	__XRT_TLS_STREAM_ASYNC_PENDING = 0,
	__XRT_TLS_STREAM_ASYNC_READY,
	__XRT_TLS_STREAM_ASYNC_RECEIVE,
	__XRT_TLS_STREAM_ASYNC_FAILED,
	__XRT_TLS_STREAM_ASYNC_CANCELLED,
	__XRT_TLS_STREAM_ASYNC_CLOSED
} __xrt_tls_stream_async_result;



/* 预算保留只返回无分配状态，结构化错误必须在释放 AsyncLock 后创建。 */
typedef enum __xrt_tls_stream_reserve_result {
	__XRT_TLS_STREAM_RESERVE_OK = 0,
	__XRT_TLS_STREAM_RESERVE_PAYLOAD_LIMIT,
	__XRT_TLS_STREAM_RESERVE_COUNT_LIMIT,
	__XRT_TLS_STREAM_RESERVE_BYTES_LIMIT
} __xrt_tls_stream_reserve_result;



/*
	节点元数据和发送副本采用单一分配，Promise 独立拥有。
	Offset 只由所属 Worker 推进，已开始的发送不会被中途取消。
*/
struct __xrt_tls_stream_async {
	volatile int32 References;
	volatile int32 Active;
	const void* Claim;
	__xrt_tls_stream_async* Next;
	xtlsstream* Stream;
	xatomicptr CacheWorker;
	xpromise* Promise;
	xcancelwatch* Watch;
	xatomic32 Done;
	size_t Size;
	size_t Offset;
	size_t MaxBytes;
	uint8 Kind;
	uint8 Wait;
	bool Linked;
	bool TerminalRead;
	bool Publishing;
	bool Cleared;
	uint8 Data[];
};

_Static_assert(
	sizeof(__xrt_tls_stream_async) <= 128u,
	"TLS Stream Future metadata left the 128-byte node class"
);



/* 从 Worker 缓存或普通堆取得一个已经清零的异步节点。 */
static __xrt_tls_stream_async* __xrtTlsStreamAsyncAlloc(
	xnetworker* pWorker,
	size_t iAllocation
)
{
	__xrt_tls_stream_async* pAsync = pWorker != NULL ?
		(__xrt_tls_stream_async*)__xrtNetWorkerNodeAlloc(
			pWorker,
			iAllocation
		) : (__xrt_tls_stream_async*)xrtCalloc(
			1,
			iAllocation
		);

	if ( pAsync != NULL ) {
		pAsync->References = 1;
		pAsync->Active = 1;
		pAsync->Publishing = true;
		xrtAtomicPtrInit(&pAsync->CacheWorker, pWorker);
		xrtAtomic32Init(&pAsync->Done, 0);
	} else if ( pWorker != NULL ) {
		__xrtNetEngineObjectRelease(pWorker->Engine);
	}
	return pAsync;
}



/* 设置 TLS Stream Future 层结构化错误。 */
static void __xrtTlsStreamAsyncSetError(
	xerrkind Kind,
	xtlserror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	__xrtTlsErrorCause(
		Kind,
		Code,
		sOperation,
		sMessage,
		SIZE_MAX,
		pCause
	);
}



/* 释放 Future 持有的接收结果。 */
static void __xrtTlsStreamBytesFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtNetBytesDestroy((xnetbytes*)pValue);
}

/* The constructor/list, producer binding and owned cancellation registration
 * each hold ONE actual activation reference. Next is structural, not owning. */
static void __xrtTlsStreamAsyncCancel(ptr pData);
static void __xrtTlsStreamAsyncEnter(__xrt_tls_stream_async* a)
{
	xrtownershipscope scope = {0};
	if (!xrtOwnershipMutationBegin(&scope)) abort();
	int32 active = __xrtAtomicRefLoad(&a->Active);
	for (;;) {
		if (active < 0 || active == INT32_MAX || a->Cleared) abort();
		int32 previous = __xrtAtomicRefCompareExchange(&a->Active, active + 1, active);
		if (previous == active) break;
		active = previous;
	}
	if (!xrtOwnershipScopeEnd(&scope)) abort();
}
static void __xrtTlsStreamAsyncLeave(__xrt_tls_stream_async* a)
{
	xrtownershipscope scope = {0};
	if (!xrtOwnershipMutationBegin(&scope)) abort();
	if (xrtRefRelease(&a->Active) < 0) abort();
	if (!xrtOwnershipScopeEnd(&scope)) abort();
}
static bool __xrtTlsStreamAsyncHold(const void* data)
{
	__xrt_tls_stream_async* a = (__xrt_tls_stream_async*)data; xrtownershipscope scope = {0};
	if (!xrtOwnershipMutationBegin(&scope)) return false;
	bool held = !a->Cleared && xrtRefRetain(&a->References) >= 0;
	if (!xrtOwnershipScopeEnd(&scope)) abort();
	return held;
}
static void __xrtTlsStreamAsyncDrop(const void* data)
{
	__xrt_tls_stream_async* a = (__xrt_tls_stream_async*)data; xrtownershipscope scope = {0};
	if (!xrtOwnershipMutationBegin(&scope)) abort();
	xnetworker* worker = NULL;
	size_t allocation = sizeof(*a) + a->Size;
	if (xrtAtomic32Load(&a->Done, XMEMORY_ACQUIRE) && !__xrtAtomicRefLoad(&a->Active)) {
		if (__xrtAtomicRefCompareExchange(&a->References, 0, 1) == 1) {
			worker = xrtAtomicPtrExchange(&a->CacheWorker, NULL, XMEMORY_ACQ_REL);
			if (!xrtOwnershipScopeEnd(&scope)) abort();
			if (worker) __xrtNetWorkerNodeRecycleHeld(worker, a, allocation);
			else xrtFree(a);
			return;
		}
		/* Pins may preserve an inert shell, never its operational Engine lease. */
		worker = xrtAtomicPtrExchange(&a->CacheWorker, NULL, XMEMORY_ACQ_REL);
	}
	int32 left = xrtRefRelease(&a->References);
	if (left < 0) abort();
	if (!left && (!xrtAtomic32Load(&a->Done, XMEMORY_ACQUIRE) || a->Publishing || a->Linked ||
		a->Promise || a->Watch || a->Stream || __xrtAtomicRefLoad(&a->Active) ||
		xrtAtomicPtrLoad(&a->CacheWorker, XMEMORY_ACQUIRE))) abort();
	if (!xrtOwnershipScopeEnd(&scope)) abort();
	if (!left) xrtFree(a);
	if (worker) __xrtNetEngineObjectRelease(worker->Engine); /* last Worker access */
}
static bool __xrtTlsStreamAsyncBusy(const void* data)
{
	const __xrt_tls_stream_async* a = data;
	if (!a || a->Cleared) return false;
	if (a->Publishing || __xrtAtomicRefLoad(&a->Active)) return true;
	return !xrtAtomic32Load(&a->Done, XMEMORY_ACQUIRE) &&
		xrtTlsStreamOwnershipQuiescenceV1Get()->Busy(a->Stream);
}
static bool __xrtTlsStreamAsyncCount(const void* data, size_t* count)
{
	const __xrt_tls_stream_async* a = data;
	if (!a || !count || a->Cleared || __xrtTlsStreamAsyncBusy(a)) return false;
	bool done = xrtAtomic32Load(&a->Done, XMEMORY_ACQUIRE) != 0;
	if (done && (a->Promise || a->Watch || a->Stream || a->Linked ||
		xrtAtomicPtrLoad(&a->CacheWorker, XMEMORY_ACQUIRE))) return false;
	if (!done && (!a->Linked || !a->Promise || !a->Watch || !a->Stream)) return false;
	int32 refs = __xrtAtomicRefLoad(&a->References);
	if (refs <= 0) return false;
	*count = (size_t)refs; return true;
}
static bool __xrtTlsStreamAsyncTrace(const void* data, xrtownershipvisitor visit, ptr context)
{
	const __xrt_tls_stream_async* a = data; size_t count;
	if (!visit || !__xrtTlsStreamAsyncCount(a, &count)) return false;
	if (a->Stream && !visit(xrtTlsStreamOwnership(a->Stream), context)) return false;
	xnetworker* worker = xrtAtomicPtrLoad(&a->CacheWorker, XMEMORY_ACQUIRE);
	if (worker && !visit(xrtNetEngineOwnership(worker->Engine), context)) return false;
	if (a->Promise && !visit(xrtPromiseOwnership(a->Promise), context)) return false;
	return !a->Watch || visit(xrtCancelWatchOwnership(a->Watch), context);
}
static const xrtownershipops __xrtTlsStreamAsyncOps = {__xrtTlsStreamAsyncCount, __xrtTlsStreamAsyncTrace};
static xrtownershipref __xrtTlsStreamAsyncOwnership(const __xrt_tls_stream_async* a)
{ return (xrtownershipref){a, a ? &__xrtTlsStreamAsyncOps : NULL}; }
static void __xrtTlsStreamAsyncOwnedCancel(ptr data)
{
	__xrt_tls_stream_async* a = data; __xrtTlsStreamAsyncEnter(a);
	__xrtTlsStreamAsyncCancel(a); __xrtTlsStreamAsyncLeave(a);
}
static const xfutureproducerownershipv1 __xrtTlsStreamAsyncProducer = {
	sizeof(xfutureproducerownershipv1), __xrtTlsStreamAsyncDrop};
static const xcancelwatchownershipv1 __xrtTlsStreamAsyncCancelPolicy = {
	sizeof(xcancelwatchownershipv1), __xrtTlsStreamAsyncOwnedCancel, __xrtTlsStreamAsyncDrop, &__xrtTlsStreamAsyncOps};
static bool __xrtTlsStreamBytesTrace(const void* value, const void* data, xrtownershipvisitor visit, ptr context)
{ return value && !data && visit && visit(xrtNetBytesOwnership(value), context); }
static const xfuturepayloadownershipv1 __xrtTlsStreamBytesPayload = {
	sizeof(xfuturepayloadownershipv1), __xrtTlsStreamBytesFree, __xrtTlsStreamBytesTrace};
XRT_API const xfutureproducerownershipv1* xrtTlsStreamFutureProducerPolicyV1Get(void) { return &__xrtTlsStreamAsyncProducer; }
XRT_API const xcancelwatchownershipv1* xrtTlsStreamFutureCancelPolicyV1Get(void) { return &__xrtTlsStreamAsyncCancelPolicy; }
XRT_API const xfuturepayloadownershipv1* xrtTlsStreamFutureBytesPayloadPolicyV1Get(void) { return &__xrtTlsStreamBytesPayload; }
XRT_API const xrtownershipquiescencev1* xrtTlsStreamFutureOwnershipQuiescenceV1Get(void)
{
	static const xrtownershipquiescencev1 policy = {sizeof(policy), &__xrtTlsStreamAsyncOps, __xrtTlsStreamAsyncBusy};
	return &policy;
}
static bool __xrtTlsStreamAsyncClaim(const void* data, const void* token)
{
	__xrt_tls_stream_async* a = (__xrt_tls_stream_async*)data; size_t count;
	if (!token || (a->Claim && a->Claim != token) || !__xrtTlsStreamAsyncCount(a, &count)) return false;
	a->Claim = token; return true;
}
static void __xrtTlsStreamAsyncRestore(const void* data, const void* token)
{
	__xrt_tls_stream_async* a = (__xrt_tls_stream_async*)data;
	if (!token || a->Claim != token || a->Cleared) abort();
	a->Claim = NULL;
}
static bool __xrtTlsStreamAsyncReady(const void* data)
{
	const __xrt_tls_stream_async* a = data; size_t count;
	return __xrtTlsStreamAsyncCount(a, &count) && xrtAtomic32Load(&a->Done, XMEMORY_ACQUIRE);
}
static xrtownershipprepareresult __xrtTlsStreamAsyncPrepare(const void* data, const void* token)
{
	const __xrt_tls_stream_async* a = data; xrtownershipscope freeze = {0};
	if (!xrtOwnershipFreezeTryBegin(&freeze)) return XRT_OWNERSHIP_PREPARE_BUSY;
	if (!token || a->Claim != token || a->Cleared) abort();
	bool ready = __xrtTlsStreamAsyncReady(a);
	if (!xrtOwnershipScopeEnd(&freeze)) abort();
	/* Only the actual source may complete accepted work. No adapter cancellation. */
	return ready ? XRT_OWNERSHIP_PREPARE_READY : XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtTlsStreamAsyncClear(const void* data, const void* token)
{
	__xrt_tls_stream_async* a = (__xrt_tls_stream_async*)data;
	if (!token || a->Claim != token || a->Cleared || !__xrtTlsStreamAsyncReady(a)) abort();
	a->Cleared = true;
}
XRT_API const xrtownershipadapterv1* xrtTlsStreamFutureOwnershipAdapterV1(xrtownershipref ref,
	const xrtownershippreparationv1** preparation)
{
	static const xrtownershipadapterv1 adapter = {sizeof(adapter), __xrtTlsStreamAsyncHold, __xrtTlsStreamAsyncDrop,
		__xrtTlsStreamAsyncClaim, __xrtTlsStreamAsyncRestore, NULL, __xrtTlsStreamAsyncClear, NULL};
	static const xrtownershippreparationv1 prepare = {sizeof(prepare), &adapter, __xrtTlsStreamAsyncReady, __xrtTlsStreamAsyncPrepare};
	size_t count;
	if (!preparation || ref.Ops != &__xrtTlsStreamAsyncOps || !__xrtTlsStreamAsyncCount(ref.Data, &count)) return NULL;
	*preparation = &prepare; return &adapter;
}
static bool __xrtTlsStreamAsyncSetup(__xrt_tls_stream_async* a, xfuture** future)
{
	a->Promise = xrtPromiseCreate(future, NULL);
	if (!a->Promise || !__xrtTlsStreamAsyncHold(a)) return false;
	if (!xrtPromiseProducerBindTakeV1(a->Promise, __xrtTlsStreamAsyncOwnership(a), &__xrtTlsStreamAsyncProducer)) {
		__xrtTlsStreamAsyncDrop(a); return false;
	}
	xcancel* cancel = xrtPromiseCancelToken(a->Promise);
	if (!cancel) return false;
	if (!__xrtTlsStreamAsyncHold(a)) { xrtCancelDestroy(cancel); return false; }
	a->Watch = xrtCancelWatchOwnedV1(cancel, a, &__xrtTlsStreamAsyncCancelPolicy);
	xrtCancelDestroy(cancel);
	if (!a->Watch) { __xrtTlsStreamAsyncDrop(a); return false; }
	return true;
}
static void __xrtTlsStreamAsyncRetire(__xrt_tls_stream_async* a)
{
	xrtownershipscope scope = {0};
	if (!xrtOwnershipMutationBegin(&scope)) abort();
	if (a->Promise || a->Watch || a->Linked || xrtAtomic32Load(&a->Done, XMEMORY_ACQUIRE)) abort();
	xtlsstream* stream = a->Stream; a->Stream = NULL;
	a->Publishing = false;
	xrtAtomic32Store(&a->Done, 1, XMEMORY_RELEASE);
	if (!xrtOwnershipScopeEnd(&scope)) abort();
	xrtTlsStreamDestroy(stream);
	__xrtTlsStreamAsyncLeave(a); __xrtTlsStreamAsyncDrop(a);
}



/* 返回节点是否占用异步发送字节预算。 */
static bool __xrtTlsStreamAsyncIsSend(
	const __xrt_tls_stream_async* pAsync
)
{
	return pAsync->Kind == __XRT_TLS_STREAM_ASYNC_SEND;
}



/* 返回节点是否占用拉取读取门。 */
static bool __xrtTlsStreamAsyncIsRead(
	const __xrt_tls_stream_async* pAsync
)
{
	return (pAsync->Kind == __XRT_TLS_STREAM_ASYNC_RECV) ||
		((pAsync->Kind == __XRT_TLS_STREAM_ASYNC_WAIT) &&
		 (pAsync->Wait == XTLS_STREAM_WAIT_READ));
}



/* 释放一次尚未挂入节点的原子预算预留。 */
static void __xrtTlsStreamAsyncBudgetRelease(
	xtlsstream* pStream,
	size_t iBytes,
	bool bSend,
	bool bRead
)
{
	if ( iBytes != 0 ) {
		(void)xrtAtomic64FetchSub(
			&pStream->AsyncBytes,
			(uint64)iBytes,
			XMEMORY_ACQ_REL
		);
	}
	if ( bSend ) {
		(void)xrtAtomic32FetchSub(
			&pStream->AsyncSends,
			1,
			XMEMORY_ACQ_REL
		);
	}
	if ( bRead ) {
		(void)xrtAtomic32FetchSub(
			&pStream->AsyncReads,
			1,
			XMEMORY_ACQ_REL
		);
	}
	(void)xrtAtomic32FetchSub(
		&pStream->AsyncCount,
		1,
		XMEMORY_ACQ_REL
	);
}



/* 构造失败归还发送预算后，唤醒可能正在等待该预留的 Close。 */
static void __xrtTlsStreamAsyncRollback(
	xtlsstream* pStream,
	size_t iBytes,
	bool bSend,
	bool bRead
)
{
	__xrtTlsStreamAsyncBudgetRelease(
		pStream,
		iBytes,
		bSend,
		bRead
	);
	if ( bSend &&
		xrtAtomic32Load(&pStream->CloseGate, XMEMORY_ACQUIRE) &&
		(xrtAtomic32Load(
			&pStream->AsyncSends,
			XMEMORY_ACQUIRE
		) == 0) ) {
		__xrtTlsStreamFutureNotify(pStream);
	}
}

/* Roll back only unpublished work. Retiring Watch synchronizes all callbacks
 * before the Stream slot is removed; the producer owns its own node credit. */
static void __xrtTlsStreamAsyncDiscard(__xrt_tls_stream_async* a, xfuture** future)
{
	xerror* error = xrtTakeError();
	xcancelwatch* watch = a->Watch; a->Watch = NULL;
	xpromise* promise = a->Promise; a->Promise = NULL;
	xrtCancelUnwatch(watch);
	__xrtTlsStreamAsyncRollback(a->Stream, a->Size,
		__xrtTlsStreamAsyncIsSend(a), __xrtTlsStreamAsyncIsRead(a));
	xrtPromiseDestroy(promise); xrtFutureDestroy(*future); *future = NULL;
	__xrtTlsStreamAsyncRetire(a);
	xrtClearError(); xrtSetErrorTake(error);
}



/* 为一个待定操作原子保留操作数、读取门和发送负载硬预算。 */
static __xrt_tls_stream_reserve_result __xrtTlsStreamAsyncReserve(
	xtlsstream* pStream,
	size_t iBytes,
	bool bSend,
	bool bRead
)
{
	uint32 iCount = xrtAtomic32Load(
		&pStream->AsyncCount,
		XMEMORY_ACQUIRE
	);
	uint64 iQueued;

	if ( iBytes > pStream->Config.AsyncBytesLimit ) {
		return __XRT_TLS_STREAM_RESERVE_PAYLOAD_LIMIT;
	}
	for ( ;; ) {
		uint32 iExpected = iCount;

		if ( iCount >= pStream->Config.AsyncCountLimit ) {
			return __XRT_TLS_STREAM_RESERVE_COUNT_LIMIT;
		}
		if ( xrtAtomic32CompareExchange(
			&pStream->AsyncCount,
			&iExpected,
			iCount + 1u,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
			break;
		}
		iCount = iExpected;
	}
	if ( bSend ) {
		(void)xrtAtomic32FetchAdd(
			&pStream->AsyncSends,
			1,
			XMEMORY_ACQ_REL
		);
	}
	if ( bRead ) {
		(void)xrtAtomic32FetchAdd(
			&pStream->AsyncReads,
			1,
			XMEMORY_ACQ_REL
		);
	}

	iQueued = xrtAtomic64Load(
		&pStream->AsyncBytes,
		XMEMORY_ACQUIRE
	);
	for ( ;; ) {
		uint64 iExpected = iQueued;
		uint64 iLimit = (uint64)pStream->Config.AsyncBytesLimit;

		if ( (iQueued > iLimit) ||
			((uint64)iBytes > (iLimit - iQueued)) ) {
			__xrtTlsStreamAsyncBudgetRelease(
				pStream,
				0,
				bSend,
				bRead
			);
			return __XRT_TLS_STREAM_RESERVE_BYTES_LIMIT;
		}
		if ( xrtAtomic64CompareExchange(
			&pStream->AsyncBytes,
			&iExpected,
			iQueued + (uint64)iBytes,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
			return __XRT_TLS_STREAM_RESERVE_OK;
		}
		iQueued = iExpected;
	}
}



/* 在节点离开全部内部队列后释放其异步硬预算。 */
static void __xrtTlsStreamAsyncRelease(
	__xrt_tls_stream_async* pAsync
)
{
	__xrtTlsStreamAsyncBudgetRelease(
		pAsync->Stream,
		pAsync->Size,
		__xrtTlsStreamAsyncIsSend(pAsync),
		__xrtTlsStreamAsyncIsRead(pAsync)
	);
}



/* 从指定 FIFO 常数时间摘除已知前驱的节点；调用方持有 AsyncLock。 */
static void __xrtTlsStreamAsyncDetach(
	__xrt_tls_stream_async** ppHead,
	__xrt_tls_stream_async** ppTail,
	__xrt_tls_stream_async* pPrevious,
	__xrt_tls_stream_async* pAsync
)
{
	if ( pPrevious != NULL ) {
		pPrevious->Next = pAsync->Next;
	} else {
		*ppHead = pAsync->Next;
	}
	if ( *ppTail == pAsync ) {
		*ppTail = pPrevious;
	}
	pAsync->Next = NULL;
	pAsync->Linked = false;
}



/* 从指定 FIFO 查找并摘除节点；调用方持有 AsyncLock。 */
static bool __xrtTlsStreamAsyncRemove(
	__xrt_tls_stream_async** ppHead,
	__xrt_tls_stream_async** ppTail,
	__xrt_tls_stream_async* pAsync
)
{
	__xrt_tls_stream_async* pCurrent;
	__xrt_tls_stream_async* pPrevious = NULL;

	for ( pCurrent = *ppHead;
		pCurrent != NULL; pCurrent = pCurrent->Next ) {
		if ( pCurrent == pAsync ) {
			__xrtTlsStreamAsyncDetach(
				ppHead,
				ppTail,
				pPrevious,
				pCurrent
			);
			return true;
		}
		pPrevious = pCurrent;
	}
	return false;
}



/* 把发送节点追加到严格 FIFO 队尾；调用方持有 AsyncLock。 */
static void __xrtTlsStreamAsyncSendAppend(
	xtlsstream* pStream,
	__xrt_tls_stream_async* pAsync
)
{
	pAsync->Next = NULL;
	pAsync->Linked = true;
	if ( pStream->AsyncSendTail != NULL ) {
		pStream->AsyncSendTail->Next = pAsync;
	} else {
		pStream->AsyncSendHead = pAsync;
	}
	pStream->AsyncSendTail = pAsync;
}



/* 短写重试回到队头，保持已经受理操作的严格顺序。 */
static void __xrtTlsStreamAsyncSendPrepend(
	xtlsstream* pStream,
	__xrt_tls_stream_async* pAsync
)
{
	pAsync->Next = pStream->AsyncSendHead;
	pAsync->Linked = true;
	pStream->AsyncSendHead = pAsync;
	if ( pStream->AsyncSendTail == NULL ) {
		pStream->AsyncSendTail = pAsync;
	}
}



/* 把接收或条件等待追加到独立观察 FIFO。 */
static void __xrtTlsStreamAsyncWaitAppend(
	xtlsstream* pStream,
	__xrt_tls_stream_async* pAsync
)
{
	pAsync->Next = NULL;
	pAsync->Linked = true;
	if ( pStream->AsyncWaitTail != NULL ) {
		pStream->AsyncWaitTail->Next = pAsync;
	} else {
		pStream->AsyncWaitHead = pAsync;
	}
	pStream->AsyncWaitTail = pAsync;
}



/* 把 TLS Stream 终止原因映射到统一 Future 结果。 */
static __xrt_tls_stream_async_result __xrtTlsStreamAsyncTerminal(
	const xtlsstream* pStream
)
{
	xnetresult Result = (xnetresult)xrtAtomic32Load(
		&pStream->TerminalResult,
		XMEMORY_ACQUIRE
	);

	if ( Result == XNET_RESULT_CANCELLED ) {
		return __XRT_TLS_STREAM_ASYNC_CANCELLED;
	}
	if ( (xrtTlsStreamError(pStream) != NULL) ||
		(Result == XNET_RESULT_ERROR) ||
		(Result == XNET_RESULT_TIMEOUT) ||
		(xrtTlsStreamState(pStream) == XTLS_STREAM_FAILED) ) {
		return __XRT_TLS_STREAM_ASYNC_FAILED;
	}
	return __XRT_TLS_STREAM_ASYNC_CLOSED;
}



/* 判断当前至少还能受理一个明文字节。 */
static bool __xrtTlsStreamAsyncWritable(const xtlsstream* pStream)
{
	const xtlslimits* pLimits;

	if ( (xrtTlsStreamState(pStream) != XTLS_STREAM_OPEN) ||
		xrtAtomic32Load(&pStream->CloseGate, XMEMORY_ACQUIRE) ||
		xrtAtomic32Load(&pStream->AbortGate, XMEMORY_ACQUIRE) ) {
		return false;
	}
	pLimits = xrtTlsContextLimits(pStream->Session->Context);
	return (pLimits != NULL) &&
		(xrtTlsSessionSendSize(pStream->Session) < pLimits->SendLimit);
}



/* 在所属 Worker 上判断接收或条件等待是否已满足。 */
static __xrt_tls_stream_async_result __xrtTlsStreamAsyncWaitResult(
	const __xrt_tls_stream_async* pAsync
)
{
	const xtlsstream* pStream = pAsync->Stream;
	xtlsstreamstate State = xrtTlsStreamState(pStream);

	if ( xrtCancelTriggered(pAsync->Watch) ) {
		return __XRT_TLS_STREAM_ASYNC_CANCELLED;
	}
	if ( (pAsync->Kind == __XRT_TLS_STREAM_ASYNC_RECV) &&
		(xrtTlsStreamAvailable(pStream) != 0) ) {
		return __XRT_TLS_STREAM_ASYNC_RECEIVE;
	}
	if ( (pAsync->Kind == __XRT_TLS_STREAM_ASYNC_WAIT) &&
		(pAsync->Wait == XTLS_STREAM_WAIT_READ) &&
		(xrtTlsStreamAvailable(pStream) != 0) ) {
		return __XRT_TLS_STREAM_ASYNC_READY;
	}
	if ( State == XTLS_STREAM_FAILED ) {
		return __xrtTlsStreamAsyncTerminal(pStream);
	}
	if ( State == XTLS_STREAM_CLOSED ) {
		if ( (pAsync->Kind == __XRT_TLS_STREAM_ASYNC_WAIT) &&
			(pAsync->Wait == XTLS_STREAM_WAIT_CLOSE) ) {
			return __XRT_TLS_STREAM_ASYNC_READY;
		}
		if ( (pAsync->Kind == __XRT_TLS_STREAM_ASYNC_WAIT) &&
			(pAsync->Wait == XTLS_STREAM_WAIT_END) &&
			(pStream->EndEmitted ||
			 (xrtTlsStreamAvailable(pStream) == 0)) ) {
			/* CLOSED 已确认双向 close_notify。密文排空可同步重入 Close，
			   先于 End 事件记账；明文已交付时不能因此丢失认证 EOF。 */
			return __XRT_TLS_STREAM_ASYNC_READY;
		}
		if ( (pAsync->Kind == __XRT_TLS_STREAM_ASYNC_WAIT) &&
			(pAsync->Wait == XTLS_STREAM_WAIT_DRAIN) &&
			(xrtTlsStreamPending(pStream) == 0) ) {
			return __XRT_TLS_STREAM_ASYNC_READY;
		}
		return __XRT_TLS_STREAM_ASYNC_CLOSED;
	}
	if ( pAsync->Kind == __XRT_TLS_STREAM_ASYNC_RECV ) {
		return pStream->EndEmitted ?
			__XRT_TLS_STREAM_ASYNC_CLOSED :
			__XRT_TLS_STREAM_ASYNC_PENDING;
	}
	if ( pAsync->Wait == XTLS_STREAM_WAIT_OPEN ) {
		if ( (State == XTLS_STREAM_OPEN) &&
			!xrtAtomic32Load(
				&pStream->CloseGate,
				XMEMORY_ACQUIRE
			) && !xrtAtomic32Load(
				&pStream->AbortGate,
				XMEMORY_ACQUIRE
			) ) {
			return __XRT_TLS_STREAM_ASYNC_READY;
		}
		return (State >= XTLS_STREAM_CLOSING) ?
			__xrtTlsStreamAsyncTerminal(pStream) :
			__XRT_TLS_STREAM_ASYNC_PENDING;
	}
	if ( pAsync->Wait == XTLS_STREAM_WAIT_READ ) {
		return pStream->EndEmitted ?
			__XRT_TLS_STREAM_ASYNC_CLOSED :
			__XRT_TLS_STREAM_ASYNC_PENDING;
	}
	if ( pAsync->Wait == XTLS_STREAM_WAIT_WRITE ) {
		if ( (xrtAtomic32Load(
			&pStream->AsyncSends,
			XMEMORY_ACQUIRE
		) == 0) && __xrtTlsStreamAsyncWritable(pStream) ) {
			return __XRT_TLS_STREAM_ASYNC_READY;
		}
		return State >= XTLS_STREAM_CLOSING ?
			__xrtTlsStreamAsyncTerminal(pStream) :
			__XRT_TLS_STREAM_ASYNC_PENDING;
	}
	if ( pAsync->Wait == XTLS_STREAM_WAIT_DRAIN ) {
		if ( (xrtAtomic32Load(
			&pStream->AsyncSends,
			XMEMORY_ACQUIRE
		) == 0) && (xrtTlsStreamPending(pStream) == 0) ) {
			return __XRT_TLS_STREAM_ASYNC_READY;
		}
		return __XRT_TLS_STREAM_ASYNC_PENDING;
	}
	if ( pAsync->Wait == XTLS_STREAM_WAIT_END ) {
		return pStream->EndEmitted ?
			__XRT_TLS_STREAM_ASYNC_READY :
			__XRT_TLS_STREAM_ASYNC_PENDING;
	}
	return __XRT_TLS_STREAM_ASYNC_PENDING;
}



/* 为缺少底层原因的异步故障建立稳定 TLS 错误。 */
static xerror* __xrtTlsStreamAsyncError(
	const __xrt_tls_stream_async* pAsync,
	cstr sOperation,
	cstr sMessage
)
{
	xnetresult Result = (xnetresult)xrtAtomic32Load(
		&pAsync->Stream->TerminalResult,
		XMEMORY_ACQUIRE
	);

	__xrtTlsStreamAsyncSetError(
		Result == XNET_RESULT_TIMEOUT ? XERR_TIMEOUT : XERR_IO,
		XTLS_ERROR_CLOSED,
		sOperation,
		sMessage,
		NULL
	);
	return xrtTakeError();
}



/* 在消费前为接收 Future 建立独立拥有的字节结果。 */
static xnetbytes* __xrtTlsStreamAsyncReceive(
	__xrt_tls_stream_async* pAsync,
	xerror** ppError
)
{
	xtlsstream* pStream = pAsync->Stream;
	size_t iSize = pAsync->TerminalRead ?
		xrtTlsSessionPlainSize(pStream->Session) :
		xrtTlsStreamAvailable(pStream);
	size_t iRead = 0;
	xnetbytes* pBytes;
	xnetwspan Span;
	xtlsresult Result;

	*ppError = NULL;
	if ( (pAsync->MaxBytes != 0) &&
		(iSize > pAsync->MaxBytes) ) {
		iSize = pAsync->MaxBytes;
	}
	pBytes = __xrtNetBytesAlloc(iSize, &Span);
	if ( pBytes == NULL ) {
		*ppError = xrtTakeError();
		return NULL;
	}
	xrtClearError();
	if ( pAsync->TerminalRead ) {
		Result = xrtTlsSessionRead(
			pStream->Session,
			Span.Data,
			iSize,
			&iRead
		);
		xrtAtomic64Store(
			&pStream->Available,
			(uint64)xrtTlsSessionPlainSize(pStream->Session),
			XMEMORY_RELEASE
		);
	} else {
		Result = __xrtTlsStreamReadAccepted(
			pStream,
			Span.Data,
			iSize,
			&iRead
		);
	}
	if ( (Result != XTLS_OK) || (iRead != iSize) ) {
		xrtNetBytesDestroy(pBytes);
		*ppError = xrtTakeError();
		if ( *ppError == NULL ) {
			*ppError = __xrtTlsStreamAsyncError(
				pAsync,
				"receive-tls-stream-async",
				"TLS stream asynchronous receive failed"
			);
		}
		return NULL;
	}
	return pBytes;
}



/* 完成唯一 Future 终态，并在唤醒观察者前归还全部队列预算。 */
static void __xrtTlsStreamAsyncFinish(
	__xrt_tls_stream_async* pAsync,
	__xrt_tls_stream_async_result Result,
	xerror* pError
)
{
	const xerror* pFailure = pError;
	xerror* pCreated = NULL;
	xnetbytes* pBytes = NULL;
	xpromise* pPromise = pAsync->Promise;
	xtlsstream* pStream = pAsync->Stream;

	__xrtTlsStreamAsyncEnter(pAsync);
	if ( Result == __XRT_TLS_STREAM_ASYNC_RECEIVE ) {
		pBytes = __xrtTlsStreamAsyncReceive(
			pAsync,
			&pCreated
		);
		if ( pBytes == NULL ) {
			Result = __XRT_TLS_STREAM_ASYNC_FAILED;
			pFailure = pCreated;
		}
	}
	xrtCancelUnwatch(pAsync->Watch);
	pAsync->Watch = NULL;
	pAsync->Promise = NULL;
	__xrtTlsStreamAsyncRelease(pAsync);
	if ( (Result == __XRT_TLS_STREAM_ASYNC_FAILED) &&
		(pFailure == NULL) ) {
		pCreated = xrtErrorRef(xrtTlsStreamError(pStream));
		pFailure = pCreated;
		if ( pFailure == NULL ) {
			pCreated = __xrtTlsStreamAsyncError(
				pAsync,
				"complete-tls-stream-async",
				"TLS stream asynchronous operation failed"
			);
			pFailure = pCreated;
		}
	}
	if ( Result == __XRT_TLS_STREAM_ASYNC_READY ) {
		(void)xrtPromiseResolve(pPromise, NULL);
	} else if ( Result == __XRT_TLS_STREAM_ASYNC_RECEIVE ) {
		if ( !xrtPromiseResolveOwnedPolicyV1(
			pPromise,
			pBytes,
			&__xrtTlsStreamBytesPayload
		) ) {
			xrtNetBytesDestroy(pBytes);
		}
	} else if ( Result == __XRT_TLS_STREAM_ASYNC_FAILED ) {
		if ( pFailure != NULL ) {
			(void)xrtPromiseReject(pPromise, pFailure);
		} else {
			(void)xrtPromiseClose(pPromise);
		}
	} else if ( Result == __XRT_TLS_STREAM_ASYNC_CANCELLED ) {
		(void)xrtPromiseCancel(pPromise);
	} else {
		(void)xrtPromiseClose(pPromise);
	}

	xrtErrorFree(pCreated);
	xrtErrorFree(pError);
	xrtPromiseDestroy(pPromise);
	__xrtTlsStreamAsyncRetire(pAsync);
}



static void __xrtTlsStreamAsyncTask(xnetworker* pWorker, ptr pData);



/* 调用方持有 AsyncLock 时无分配地唤醒所属 Worker。 */
static bool __xrtTlsStreamAsyncSchedule(xtlsstream* pStream)
{
	xnetstream* pTransport;

	if ( pStream->AsyncPosted ) {
		return true;
	}
	pTransport = (xnetstream*)xrtAtomicPtrLoad(
		&pStream->Transport,
		XMEMORY_ACQUIRE
	);
	if ( pTransport == NULL ) {
		return false;
	}
	if (!xrtTlsStreamRef(pStream)) return false;
	pStream->AsyncPosted = true;
	if ( !__xrtNetEnginePostInternal(
		pTransport->Worker,
		&pStream->AsyncCommand,
		__xrtTlsStreamAsyncTask,
		pStream
	) ) {
		pStream->AsyncPosted = false;
		xrtTlsStreamDestroy(pStream);
		return false;
	}
	return true;
}



/* 取消回调只唤醒 Worker，不从请求取消的线程完成 Promise。 */
static void __xrtTlsStreamAsyncCancel(ptr pData)
{
	__xrt_tls_stream_async* pAsync =
		(__xrt_tls_stream_async*)pData;
	xtlsstream* pStream = pAsync->Stream;

	if (!__xrtTlsStreamOwnershipEnter(pStream, false)) return;
	__xrtSpinLock(&pStream->AsyncLock);
	if ( pAsync->Linked ) {
		(void)__xrtTlsStreamAsyncSchedule(pStream);
	}
	__xrtSpinUnlock(&pStream->AsyncLock);
	__xrtTlsStreamOwnershipLeave(pStream);
}



/* 创建 Promise、取消监听和单一分配节点。 */
static __xrt_tls_stream_async* __xrtTlsStreamAsyncCreate(
	xtlsstream* pStream,
	__xrt_tls_stream_async_kind Kind,
	xtlsstreamwait Wait,
	size_t iSize,
	xfuture** ppFuture
)
{
	__xrt_tls_stream_async* pAsync;
	xtlsstream* pOwned;
	xnetstream* pTransport;
	xnetworker* pCacheWorker = NULL;
	size_t iAllocation;
	xtlsstreamstate State;
	__xrt_tls_stream_reserve_result Reserve;
	bool bSend = Kind == __XRT_TLS_STREAM_ASYNC_SEND;
	bool bRead = (Kind == __XRT_TLS_STREAM_ASYNC_RECV) ||
		((Kind == __XRT_TLS_STREAM_ASYNC_WAIT) &&
		 (Wait == XTLS_STREAM_WAIT_READ));
	bool bTerminalRead;

	*ppFuture = NULL;
	if ( iSize > (SIZE_MAX - sizeof(*pAsync)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iAllocation = sizeof(*pAsync) + iSize;
	pOwned = xrtTlsStreamRef(pStream);
	if ( pOwned == NULL ) {
		return NULL;
	}
	__xrtSpinLock(&pOwned->AsyncLock);
	State = xrtTlsStreamState(pOwned);
	if ( bSend &&
		(State != XTLS_STREAM_CLOSED) &&
		(State != XTLS_STREAM_FAILED) &&
		(xrtAtomic32Load(
			&pOwned->CloseGate,
			XMEMORY_ACQUIRE
		) || xrtAtomic32Load(
			&pOwned->AbortGate,
			XMEMORY_ACQUIRE
		)) ) {
		__xrtSpinUnlock(&pOwned->AsyncLock);
		xrtTlsStreamDestroy(pOwned);
		__xrtTlsStreamAsyncSetError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			"submit-tls-stream-async",
			"TLS stream no longer accepts asynchronous sends",
			NULL
		);
		return NULL;
	}
	if ( bRead && (pOwned->Events.Read != NULL) ) {
		__xrtSpinUnlock(&pOwned->AsyncLock);
		xrtTlsStreamDestroy(pOwned);
		__xrtTlsStreamAsyncSetError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			"submit-tls-stream-async",
			"TLS stream pull reads require no Read callback",
			NULL
		);
		return NULL;
	}
	Reserve = __xrtTlsStreamAsyncReserve(
		pOwned,
		iSize,
		bSend,
		bRead
	);
	if ( Reserve != __XRT_TLS_STREAM_RESERVE_OK ) {
		__xrtSpinUnlock(&pOwned->AsyncLock);
		xrtTlsStreamDestroy(pOwned);
		if ( Reserve == __XRT_TLS_STREAM_RESERVE_PAYLOAD_LIMIT ) {
			__xrtTlsStreamAsyncSetError(
				XERR_RANGE,
				XTLS_ERROR_LIMIT,
				"submit-tls-stream-async",
				"TLS stream asynchronous payload exceeds its hard byte limit",
				NULL
			);
		} else if (
			Reserve == __XRT_TLS_STREAM_RESERVE_COUNT_LIMIT
		) {
			__xrtTlsStreamAsyncSetError(
				XERR_AGAIN,
				XTLS_ERROR_LIMIT,
				"submit-tls-stream-async",
				"TLS stream asynchronous operation queue is full",
				NULL
			);
		} else {
			__xrtTlsStreamAsyncSetError(
				XERR_AGAIN,
				XTLS_ERROR_LIMIT,
				"submit-tls-stream-async",
				"TLS stream asynchronous payload queue is full",
				NULL
			);
		}
		return NULL;
	}
	bTerminalRead = pOwned->AsyncClosed ||
		(State == XTLS_STREAM_CLOSED) ||
		(State == XTLS_STREAM_FAILED);
	if ( !bTerminalRead &&
		 (iAllocation <= XRT_NET_ENGINE_NODE_SIZE_MAX) ) {
		pTransport = (xnetstream*)xrtAtomicPtrLoad(
			&pOwned->Transport,
			XMEMORY_ACQUIRE
		);
		if ( pTransport != NULL ) {
			pCacheWorker = pTransport->Worker;
			if ( !__xrtNetEngineObjectHold(pTransport->Engine) ) {
				__xrtSpinUnlock(&pOwned->AsyncLock);
				__xrtTlsStreamAsyncRollback(
					pOwned,
					iSize,
					bSend,
					bRead
				);
				xrtTlsStreamDestroy(pOwned);
				return NULL;
			}
		}
	}
	__xrtSpinUnlock(&pOwned->AsyncLock);

	pAsync = __xrtTlsStreamAsyncAlloc(pCacheWorker, iAllocation);
	if ( pAsync == NULL ) {
		__xrtTlsStreamAsyncRollback(
			pOwned,
			iSize,
			bSend,
			bRead
		);
		xrtTlsStreamDestroy(pOwned);
		return NULL;
	}
	pAsync->Stream = pOwned;
	pAsync->Kind = (uint8)Kind;
	pAsync->Wait = (uint8)Wait;
	pAsync->Size = iSize;
	pAsync->TerminalRead = bTerminalRead;
	if (!__xrtTlsStreamAsyncSetup(pAsync, ppFuture)) {
		__xrtTlsStreamAsyncDiscard(pAsync, ppFuture); return NULL;
	}
	return pAsync;
}



/* 发布完整节点；终态对象不依赖已经停止的 Worker。 */
static void __xrtTlsStreamAsyncSubmit(
	__xrt_tls_stream_async* pAsync
)
{
	xtlsstream* pStream = pAsync->Stream;
	xtlsstreamstate State;
	bool bScheduled = true;
	bool bTerminal;
	bool bTerminalReceive;
	bool bTerminalQueued = false;

	xrtownershipscope publish = {0};
	if (!xrtOwnershipMutationBegin(&publish)) abort();
	__xrtSpinLock(&pStream->AsyncLock);
	State = xrtTlsStreamState(pStream);
	bTerminal = (State == XTLS_STREAM_CLOSED) ||
		(State == XTLS_STREAM_FAILED);
	bTerminalReceive =
		((State == XTLS_STREAM_CLOSED) ||
		 (State == XTLS_STREAM_FAILED)) &&
		(pAsync->Kind == __XRT_TLS_STREAM_ASYNC_RECV) &&
		(xrtTlsStreamAvailable(pStream) != 0);
	if ( !bTerminal ||
		 (bTerminalReceive && !pAsync->TerminalRead) ) {
		if ( pAsync->Kind == __XRT_TLS_STREAM_ASYNC_SEND ) {
			__xrtTlsStreamAsyncSendAppend(pStream, pAsync);
		} else {
			__xrtTlsStreamAsyncWaitAppend(pStream, pAsync);
		}
		bScheduled = __xrtTlsStreamAsyncSchedule(pStream);
		if ( !bScheduled ) {
			if ( pAsync->Kind == __XRT_TLS_STREAM_ASYNC_SEND ) {
				(void)__xrtTlsStreamAsyncRemove(
					&pStream->AsyncSendHead,
					&pStream->AsyncSendTail,
					pAsync
				);
			} else {
				(void)__xrtTlsStreamAsyncRemove(
					&pStream->AsyncWaitHead,
					&pStream->AsyncWaitTail,
					pAsync
				);
			}
		}
	} else if ( bTerminalReceive ) {
		__xrtTlsStreamAsyncWaitAppend(pStream, pAsync);
		bTerminalQueued = true;
	}
	pAsync->Publishing = false;
	if (xrtRefRelease(&pAsync->Active) != 0) abort();
	__xrtSpinUnlock(&pStream->AsyncLock);
	if (!xrtOwnershipScopeEnd(&publish)) abort();

	if ( bTerminalQueued ) {
		__xrtTlsStreamFutureNotify(pStream);
	} else if ( bTerminal && !bTerminalReceive ) {
		__xrt_tls_stream_async_result Result =
			pAsync->Kind == __XRT_TLS_STREAM_ASYNC_SEND ?
			__xrtTlsStreamAsyncTerminal(pStream) :
			__xrtTlsStreamAsyncWaitResult(pAsync);

		if ( Result == __XRT_TLS_STREAM_ASYNC_PENDING ) {
			Result = __xrtTlsStreamAsyncTerminal(pStream);
		}
		__xrtTlsStreamAsyncFinish(pAsync, Result, NULL);
	} else if ( !bScheduled ) {
		xerror* pError;

		__xrtTlsStreamAsyncSetError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			"submit-tls-stream-async",
			"TLS stream has no active network worker",
			NULL
		);
		pError = xrtTakeError();
		__xrtTlsStreamAsyncFinish(
			pAsync,
			__XRT_TLS_STREAM_ASYNC_FAILED,
			pError
		);
	}
}



/* 优先摘除尚未开始且已经取消的发送，否则按状态取严格 FIFO 队头。 */
static __xrt_tls_stream_async* __xrtTlsStreamAsyncSendTake(
	xtlsstream* pStream,
	bool* pbCancelled
)
{
	__xrt_tls_stream_async* pAsync;
	__xrt_tls_stream_async* pPrevious = NULL;
	xtlsstreamstate State;

	*pbCancelled = false;
	__xrtSpinLock(&pStream->AsyncLock);
	for ( pAsync = pStream->AsyncSendHead;
		pAsync != NULL; pAsync = pAsync->Next ) {
		if ( (pAsync->Offset == 0) &&
			xrtCancelTriggered(pAsync->Watch) ) {
			*pbCancelled = true;
			break;
		}
		pPrevious = pAsync;
	}
	State = xrtTlsStreamState(pStream);
	if ( pAsync == NULL ) {
		if ( (State == XTLS_STREAM_OPEN) ||
			(State >= XTLS_STREAM_CLOSING) ) {
			pAsync = pStream->AsyncSendHead;
			pPrevious = NULL;
		}
	}
	if ( pAsync != NULL ) {
		__xrtTlsStreamAsyncDetach(
			&pStream->AsyncSendHead,
			&pStream->AsyncSendTail,
			pPrevious,
			pAsync
		);
	}
	__xrtSpinUnlock(&pStream->AsyncLock);
	return pAsync;
}



/* 摘除第一个已经满足、取消或终止的接收和条件等待。 */
static __xrt_tls_stream_async* __xrtTlsStreamAsyncWaitTake(
	xtlsstream* pStream,
	__xrt_tls_stream_async_result* pResult
)
{
	__xrt_tls_stream_async* pAsync;
	__xrt_tls_stream_async* pPrevious = NULL;

	__xrtSpinLock(&pStream->AsyncLock);
	for ( pAsync = pStream->AsyncWaitHead;
		pAsync != NULL; pAsync = pAsync->Next ) {
		*pResult = __xrtTlsStreamAsyncWaitResult(pAsync);
		if ( *pResult != __XRT_TLS_STREAM_ASYNC_PENDING ) {
			__xrtTlsStreamAsyncDetach(
				&pStream->AsyncWaitHead,
				&pStream->AsyncWaitTail,
				pPrevious,
				pAsync
			);
			break;
		}
		pPrevious = pAsync;
	}
	__xrtSpinUnlock(&pStream->AsyncLock);
	return pAsync;
}



/* 在所属 Worker 上推进一次发送节点，保留成功短写的剩余后缀。 */
static __xrt_tls_stream_async_result __xrtTlsStreamAsyncSend(
	__xrt_tls_stream_async* pAsync,
	xerror** ppError
)
{
	xtlsstream* pStream = pAsync->Stream;
	xtlsstreamstate State = xrtTlsStreamState(pStream);
	size_t iWritten = 0;
	xtlsresult Result;

	*ppError = NULL;
	if ( State != XTLS_STREAM_OPEN ) {
		return State >= XTLS_STREAM_CLOSING ?
			__xrtTlsStreamAsyncTerminal(pStream) :
			__XRT_TLS_STREAM_ASYNC_PENDING;
	}
	if ( pAsync->Size == 0 ) {
		return __XRT_TLS_STREAM_ASYNC_READY;
	}
	xrtClearError();
	Result = __xrtTlsStreamSendAccepted(
		pStream,
		pAsync->Data + pAsync->Offset,
		pAsync->Size - pAsync->Offset,
		&iWritten
	);
	*ppError = xrtTakeError();
	pAsync->Offset += iWritten;
	if ( pAsync->Offset == pAsync->Size ) {
		return __XRT_TLS_STREAM_ASYNC_READY;
	}
	if ( (Result == XTLS_OK) || (Result == XTLS_AGAIN) ) {
		xrtErrorFree(*ppError);
		*ppError = NULL;
		return __XRT_TLS_STREAM_ASYNC_PENDING;
	}
	if ( Result == XTLS_CLOSED ) {
		return __xrtTlsStreamAsyncTerminal(pStream);
	}
	return __XRT_TLS_STREAM_ASYNC_FAILED;
}



/* 在所属 Worker 上按公平批次推进发送，再完成全部已满足的观察操作。 */
static void __xrtTlsStreamFutureNotifyImpl(xtlsstream* pStream)
{
	uint32 iCompleted = 0;
	uint32 iBatch;
	xnetstream* pTransport;
	bool bContinue = false;
	bool bClosed;
	bool bRunAgain = false;

	if ( pStream == NULL ) {
		return;
	}
	__xrtSpinLock(&pStream->AsyncLock);
	bClosed = pStream->AsyncClosed;
	pTransport = (xnetstream*)xrtAtomicPtrLoad(
		&pStream->Transport,
		XMEMORY_ACQUIRE
	);
	if ( !bClosed && (pTransport != NULL) &&
		!xrtNetWorkerIsCurrent(xrtNetStreamWorker(pTransport)) ) {
		(void)__xrtTlsStreamAsyncSchedule(pStream);
		__xrtSpinUnlock(&pStream->AsyncLock);
		return;
	}
	if ( !bClosed && (pTransport == NULL) &&
		(xrtTlsStreamState(pStream) != XTLS_STREAM_CLOSED) &&
		(xrtTlsStreamState(pStream) != XTLS_STREAM_FAILED) ) {
		__xrtSpinUnlock(&pStream->AsyncLock);
		return;
	}
	if ( pStream->AsyncRunning ) {
		pStream->AsyncAgain = true;
		__xrtSpinUnlock(&pStream->AsyncLock);
		return;
	}
	pStream->AsyncRunning = true;
	__xrtSpinUnlock(&pStream->AsyncLock);
	iBatch = bClosed ? UINT32_MAX : pStream->Config.AsyncBatch;

	while ( iCompleted < iBatch ) {
		__xrt_tls_stream_async* pAsync;
		__xrt_tls_stream_async_result Result;
		xerror* pError = NULL;
		bool bCancelled;

		pAsync = __xrtTlsStreamAsyncSendTake(
			pStream,
			&bCancelled
		);
		if ( pAsync == NULL ) {
			break;
		}
		if ( bCancelled ) {
			__xrtTlsStreamAsyncFinish(
				pAsync,
				__XRT_TLS_STREAM_ASYNC_CANCELLED,
				NULL
			);
			iCompleted++;
			continue;
		}
		Result = __xrtTlsStreamAsyncSend(pAsync, &pError);
		if ( Result == __XRT_TLS_STREAM_ASYNC_PENDING ) {
			__xrtSpinLock(&pStream->AsyncLock);
			__xrtTlsStreamAsyncSendPrepend(pStream, pAsync);
			__xrtSpinUnlock(&pStream->AsyncLock);
			break;
		}
		__xrtTlsStreamAsyncFinish(pAsync, Result, pError);
		iCompleted++;
	}

	while ( iCompleted < iBatch ) {
		__xrt_tls_stream_async_result Result =
			__XRT_TLS_STREAM_ASYNC_PENDING;
		__xrt_tls_stream_async* pAsync =
			__xrtTlsStreamAsyncWaitTake(
				pStream,
				&Result
			);

		if ( pAsync == NULL ) {
			break;
		}
		__xrtTlsStreamAsyncFinish(pAsync, Result, NULL);
		iCompleted++;
	}

	__xrtSpinLock(&pStream->AsyncLock);
	pStream->AsyncRunning = false;
	bContinue = pStream->AsyncAgain ||
		((iCompleted == iBatch) &&
		 ((pStream->AsyncSendHead != NULL) ||
		  (pStream->AsyncWaitHead != NULL)));
	pStream->AsyncAgain = false;
	if ( bContinue ) {
		if ( pStream->AsyncClosed ) {
			bRunAgain = true;
		} else {
			(void)__xrtTlsStreamAsyncSchedule(pStream);
		}
	}
	__xrtSpinUnlock(&pStream->AsyncLock);
	__xrtTlsStreamCloseReady(pStream);
	if ( bRunAgain ) {
		__xrtTlsStreamFutureNotify(pStream);
	}
}

void __xrtTlsStreamFutureNotify(xtlsstream* stream)
{
	if (!stream) return;
	xtlsstream* held = xrtTlsStreamRef(stream);
	if (!held) return;
	if (__xrtTlsStreamOwnershipEnter(held, false)) {
		__xrtTlsStreamFutureNotifyImpl(held);
		__xrtTlsStreamOwnershipLeave(held);
	}
	xrtTlsStreamDestroy(held);
}



/* 内嵌命令进入 Worker 后允许后续提交和取消再次投递。 */
static void __xrtTlsStreamAsyncTask(xnetworker* pWorker, ptr pData)
{
	xtlsstream* pStream = (xtlsstream*)pData;

	(void)pWorker;
	__xrtSpinLock(&pStream->AsyncLock);
	pStream->AsyncPosted = false;
	__xrtSpinUnlock(&pStream->AsyncLock);
	__xrtTlsStreamFutureNotify(pStream);
	xrtTlsStreamDestroy(pStream);
}



/* 返回当前异步发送负载字节数。 */
XRT_API size_t xrtTlsStreamAsyncBytes(const xtlsstream* pStream)
{
	uint64 iBytes;

	if ( pStream == NULL ) {
		return 0;
	}
	iBytes = xrtAtomic64Load(
		&pStream->AsyncBytes,
		XMEMORY_ACQUIRE
	);
	return iBytes > (uint64)SIZE_MAX ?
		SIZE_MAX : (size_t)iBytes;
}



/* 返回当前异步操作总数。 */
XRT_API uint32 xrtTlsStreamAsyncCount(const xtlsstream* pStream)
{
	return pStream != NULL ?
		xrtAtomic32Load(
			&pStream->AsyncCount,
			XMEMORY_ACQUIRE
		) : 0;
}



/* 创建并发布一个 TLS Stream 条件 Future。 */
static xfuture* __xrtTlsStreamWaitAsync(
	xtlsstream* pStream,
	xtlsstreamwait Wait
)
{
	__xrt_tls_stream_async* pAsync;
	xfuture* pFuture;

	if ( (pStream == NULL) || (Wait < XTLS_STREAM_WAIT_OPEN) ||
		(Wait > XTLS_STREAM_WAIT_CLOSE) ) {
		__xrtTlsStreamAsyncSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"wait-tls-stream-async",
			"TLS stream asynchronous wait argument is invalid",
			NULL
		);
		return NULL;
	}
	pAsync = __xrtTlsStreamAsyncCreate(
		pStream,
		__XRT_TLS_STREAM_ASYNC_WAIT,
		Wait,
		0,
		&pFuture
	);
	if ( pAsync == NULL ) {
		return NULL;
	}
	__xrtTlsStreamAsyncSubmit(pAsync);
	return pFuture;
}



/* 创建并发布一个拥有结果的拉取接收 Future。 */
static xfuture* __xrtTlsStreamRecvAsync(
	xtlsstream* pStream,
	size_t iMaxBytes
)
{
	__xrt_tls_stream_async* pAsync;
	xfuture* pFuture;

	if ( pStream == NULL ) {
		__xrtTlsStreamAsyncSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"receive-tls-stream-async",
			"TLS stream is null",
			NULL
		);
		return NULL;
	}
	pAsync = __xrtTlsStreamAsyncCreate(
		pStream,
		__XRT_TLS_STREAM_ASYNC_RECV,
		XTLS_STREAM_WAIT_READ,
		0,
		&pFuture
	);
	if ( pAsync == NULL ) {
		return NULL;
	}
	pAsync->MaxBytes = iMaxBytes;
	__xrtTlsStreamAsyncSubmit(pAsync);
	return pFuture;
}



/* 校验、复制并发布一个完整明文发送 Future。 */
static xfuture* __xrtTlsStreamSendAsync(
	xtlsstream* pStream,
	const void* pData,
	size_t iSize
)
{
	__xrt_tls_stream_async* pAsync;
	xfuture* pFuture;

	if ( (pStream == NULL) || ((pData == NULL) && (iSize != 0)) ) {
		__xrtTlsStreamAsyncSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"send-tls-stream-async",
			"TLS stream asynchronous send argument is invalid",
			NULL
		);
		return NULL;
	}
	pAsync = __xrtTlsStreamAsyncCreate(
		pStream,
		__XRT_TLS_STREAM_ASYNC_SEND,
		XTLS_STREAM_WAIT_WRITE,
		iSize,
		&pFuture
	);
	if ( pAsync == NULL ) {
		return NULL;
	}
	if ( iSize != 0 ) {
		memcpy(pAsync->Data, pData, iSize);
	}
	__xrtTlsStreamAsyncSubmit(pAsync);
	return pFuture;
}



/* 失败原子地校验、展平并发布一个分片发送 Future。 */
static xfuture* __xrtTlsStreamSendVecAsync(
	xtlsstream* pStream,
	const xnetspan* pSpans,
	size_t iCount
)
{
	__xrt_tls_stream_async* pAsync;
	xfuture* pFuture;
	size_t iTotal = 0;
	size_t iOffset = 0;

	if ( (pStream == NULL) ||
		((pSpans == NULL) && (iCount != 0)) ) {
		__xrtTlsStreamAsyncSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"send-tls-stream-vector-async",
			"TLS stream asynchronous send vector is invalid",
			NULL
		);
		return NULL;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( (pSpans[i].Data == NULL) &&
			(pSpans[i].Size != 0) ) {
			__xrtTlsStreamAsyncSetError(
				XERR_ARGUMENT,
				XTLS_ERROR_ARGUMENT,
				"send-tls-stream-vector-async",
				"TLS stream asynchronous send span is invalid",
				NULL
			);
			return NULL;
		}
		if ( pSpans[i].Size > (SIZE_MAX - iTotal) ) {
			__xrtTlsStreamAsyncSetError(
				XERR_RANGE,
				XTLS_ERROR_LIMIT,
				"send-tls-stream-vector-async",
				"TLS stream asynchronous send vector size overflows",
				NULL
			);
			return NULL;
		}
		iTotal += pSpans[i].Size;
	}
	pAsync = __xrtTlsStreamAsyncCreate(
		pStream,
		__XRT_TLS_STREAM_ASYNC_SEND,
		XTLS_STREAM_WAIT_WRITE,
		iTotal,
		&pFuture
	);
	if ( pAsync == NULL ) {
		return NULL;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( pSpans[i].Size != 0 ) {
			memcpy(
				pAsync->Data + iOffset,
				pSpans[i].Data,
				pSpans[i].Size
			);
			iOffset += pSpans[i].Size;
		}
	}
	__xrtTlsStreamAsyncSubmit(pAsync);
	return pFuture;
}

XRT_API xfuture* xrtTlsStreamWaitAsync(xtlsstream* s, xtlsstreamwait wait)
{
	if (!__xrtTlsStreamOwnershipEnter(s, true)) return NULL;
	xfuture* result = __xrtTlsStreamWaitAsync(s, wait); __xrtTlsStreamOwnershipLeave(s); return result;
}
XRT_API xfuture* xrtTlsStreamRecvAsync(xtlsstream* s, size_t maximum)
{
	if (!__xrtTlsStreamOwnershipEnter(s, true)) return NULL;
	xfuture* result = __xrtTlsStreamRecvAsync(s, maximum); __xrtTlsStreamOwnershipLeave(s); return result;
}
XRT_API xfuture* xrtTlsStreamSendAsync(xtlsstream* s, const void* data, size_t size)
{
	if (!__xrtTlsStreamOwnershipEnter(s, true)) return NULL;
	xfuture* result = __xrtTlsStreamSendAsync(s, data, size); __xrtTlsStreamOwnershipLeave(s); return result;
}
XRT_API xfuture* xrtTlsStreamSendVecAsync(xtlsstream* s, const xnetspan* spans, size_t count)
{
	if (!__xrtTlsStreamOwnershipEnter(s, true)) return NULL;
	xfuture* result = __xrtTlsStreamSendVecAsync(s, spans, count); __xrtTlsStreamOwnershipLeave(s); return result;
}

/* Read only after source quiescence. Both FIFO chains own their original
 * constructor credits, not per-child estimates. Reservations under creation
 * are excluded by the Stream's native submission frame. */
bool __xrtTlsStreamAsyncOwnershipWalk(const xtlsstream* stream, xrtownershipvisitor visit, ptr context)
{
	if (stream->AsyncPosted || stream->AsyncRunning || stream->AsyncAgain || stream->AsyncCommand.Task) return false;
	uint64 bytes = 0; size_t count = 0, sends = 0, reads = 0;
	for (size_t queue = 0; queue < 2; ++queue) {
		const __xrt_tls_stream_async* head = queue ? stream->AsyncWaitHead : stream->AsyncSendHead;
		const __xrt_tls_stream_async* tail = queue ? stream->AsyncWaitTail : stream->AsyncSendTail;
		const __xrt_tls_stream_async *slow = head, *fast = head, *last = NULL;
		while (fast && fast->Next) {
			slow = slow->Next; fast = fast->Next->Next; if (slow == fast) return false;
		}
		for (const __xrt_tls_stream_async* a = head; a; a = a->Next) {
			if (!a->Linked || a->Publishing || a->Cleared || a->Stream != stream ||
				!a->Promise || !a->Watch || __xrtAtomicRefLoad(&a->Active) ||
				xrtAtomic32Load(&a->Done, XMEMORY_ACQUIRE) || __xrtAtomicRefLoad(&a->References) <= 0 ||
				a->Offset > a->Size || count == SIZE_MAX) return false;
			if (!queue) {
				if (a->Kind != __XRT_TLS_STREAM_ASYNC_SEND || a->Size > UINT64_MAX - bytes) return false;
				bytes += a->Size; ++sends;
			} else {
				if (a->Kind != __XRT_TLS_STREAM_ASYNC_RECV && a->Kind != __XRT_TLS_STREAM_ASYNC_WAIT) return false;
				if (a->Kind == __XRT_TLS_STREAM_ASYNC_WAIT && a->Wait > XTLS_STREAM_WAIT_CLOSE) return false;
				if (__xrtTlsStreamAsyncIsRead(a)) ++reads;
			}
			++count; last = a;
			if (visit && !visit(__xrtTlsStreamAsyncOwnership(a), context)) return false;
		}
		if (last != tail) return false;
	}
	return count == xrtAtomic32Load(&stream->AsyncCount, XMEMORY_ACQUIRE) &&
		sends == xrtAtomic32Load(&stream->AsyncSends, XMEMORY_ACQUIRE) &&
		reads == xrtAtomic32Load(&stream->AsyncReads, XMEMORY_ACQUIRE) &&
		bytes == xrtAtomic64Load(&stream->AsyncBytes, XMEMORY_ACQUIRE);
}

#endif
