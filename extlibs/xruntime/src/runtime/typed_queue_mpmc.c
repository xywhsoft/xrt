#include "../internal/xrt_typed_queue.h"



#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_MPMC)

/* 清理初始化中途失败的 MPMC 类型队列并保留根错误。 */
static void __xrtTypedMPMCQueueRollback(xtypedmpmcqueue* pQueue)
{
	xerror* pError = xrtTakeError();

	xrtMPMCQueueUnit(&pQueue->Retry);
	xrtMPMCQueueUnit(&pQueue->Free);
	xrtMPMCQueueUnit(&pQueue->Ready);
	__xrtTypedQueueCoreUnit(&pQueue->Core);
	memset(pQueue, 0, sizeof(*pQueue));
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 把全部固定值槽发布到并发空闲环。 */
static bool __xrtTypedMPMCQueueFillFree(xtypedmpmcqueue* pQueue)
{
	for ( size_t i = 0u; i < pQueue->Core.Capacity; i++ ) {
		if ( xrtMPMCQueueTryPush(
			&pQueue->Free, __xrtTypedQueueCell(&pQueue->Core, i)
		) != XQUEUE_OK ) {
			__xrtTypedQueueWrap(
				XERR_STATE, XTYPED_QUEUE_ERROR_STATE, "init",
				"an MPMC free value slot could not be published"
			);
			return false;
		}
	}
	return true;
}



/* 把空值槽归还给并发空闲环。 */
static bool __xrtTypedMPMCQueueRecycle(
	xtypedmpmcqueue* pQueue,
	ptr pCell,
	cstr sOperation
)
{
	xqueueresult Result;

	for ( ;; ) {
		Result = xrtMPMCQueueTryPush(&pQueue->Free, pCell);
		if ( Result == XQUEUE_OK ) {
			return true;
		}
		if ( Result != XQUEUE_FULL ) {
			break;
		}
		xrtAtomicPause();
	}
	__xrtTypedQueueCoreBreak(
		&pQueue->Core, sOperation,
		"an MPMC value slot could not be recycled"
	);
	return false;
}



/* 把移动失败后仍拥有值的槽发布到并发重试环。 */
static bool __xrtTypedMPMCQueueRetry(
	xtypedmpmcqueue* pQueue,
	ptr pCell,
	cstr sOperation
)
{
	xqueueresult Result;

	for ( ;; ) {
		Result = xrtMPMCQueueTryPush(&pQueue->Retry, pCell);
		if ( Result == XQUEUE_OK ) {
			return true;
		}
		if ( Result != XQUEUE_FULL ) {
			break;
		}
		xrtAtomicPause();
	}
	__xrtTypedQueueCoreBreak(
		&pQueue->Core, sOperation,
		"an MPMC value slot could not be retained for retry"
	);
	return false;
}



/* 结束一个已初始化 MPMC 类型队列，失败表示仍有并发访问。 */
static bool __xrtTypedMPMCQueueUnit(xtypedmpmcqueue* pQueue)
{
	uint32 iPrevious;

	if ( pQueue == NULL ) {
		return true;
	}
	if ( pQueue->Core.ItemType == NULL ) {
		memset(pQueue, 0, sizeof(*pQueue));
		return true;
	}
	if ( !__xrtTypedQueueCoreExclusive(
		&pQueue->Core, true, &iPrevious, "unit"
	) ) {
		return false;
	}
	(void)iPrevious;
	xrtMPMCQueueUnit(&pQueue->Retry);
	xrtMPMCQueueUnit(&pQueue->Free);
	xrtMPMCQueueUnit(&pQueue->Ready);
	__xrtTypedQueueCoreUnit(&pQueue->Core);
	memset(pQueue, 0, sizeof(*pQueue));
	return true;
}



/* 共用复制和移动两条 MPMC 入队路径。 */
static xqueueresult __xrtTypedMPMCQueuePush(
	xtypedmpmcqueue* pQueue,
	ptr pItem,
	bool bTake,
	cstr sOperation
)
{
	ptr pCell = NULL;
	xqueueresult Result;
	bool bStored;

	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), sOperation
	) || !__xrtTypedQueueValueValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), pItem, sOperation
	) || !__xrtTypedQueueCoreEnter(
		&pQueue->Core, pQueue, sizeof(*pQueue), sOperation
	) ) {
		return XQUEUE_ERROR;
	}
	if ( xrtMPMCQueueIsClosed(&pQueue->Ready) ) {
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		return XQUEUE_CLOSED;
	}
	Result = xrtMPMCQueueTryPop(&pQueue->Free, &pCell);
	if ( Result != XQUEUE_OK ) {
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		if ( Result == XQUEUE_EMPTY ) {
			return XQUEUE_FULL;
		}
		__xrtTypedQueueCoreBreak(
			&pQueue->Core, sOperation,
			"the MPMC free value-slot queue is invalid"
		);
		return XQUEUE_ERROR;
	}
	bStored = bTake ?
		__xrtTypedQueueMoveIn(&pQueue->Core, pCell, pItem, sOperation) :
		__xrtTypedQueueCopyIn(&pQueue->Core, pCell, pItem, sOperation);
	if ( !bStored ) {
		(void)__xrtTypedMPMCQueueRecycle(pQueue, pCell, sOperation);
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		return XQUEUE_ERROR;
	}
	do {
		Result = xrtMPMCQueueTryPush(&pQueue->Ready, pCell);
		if ( Result == XQUEUE_FULL ) {
			xrtAtomicPause();
		}
	} while ( Result == XQUEUE_FULL );
	if ( Result != XQUEUE_OK ) {
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		__xrtTypedQueueCoreBreak(
			&pQueue->Core, sOperation,
			"a prepared MPMC value slot could not be published"
		);
		return XQUEUE_ERROR;
	}
	__xrtTypedQueueCoreLeave(&pQueue->Core);
	return XQUEUE_OK;
}



/* 初始化一个拥有固定值槽的 MPMC 类型队列。 */
XRT_API bool xrtTypedMPMCQueueInit(
	xtypedmpmcqueue* pQueue,
	const xrttype* pItemType,
	size_t iCapacity
)
{
	if ( pQueue == NULL ) {
		__xrtTypedQueueError(
			XERR_ARGUMENT, XTYPED_QUEUE_ERROR_ARGUMENT, "init",
			"the MPMC typed queue is null"
		);
		return false;
	}
	memset(pQueue, 0, sizeof(*pQueue));
	if ( !xrtMPMCQueueInit(&pQueue->Ready, iCapacity) ) {
		return false;
	}
	if ( !__xrtTypedQueueCoreInit(
		&pQueue->Core, pItemType, pQueue->Ready.Capacity, "init"
	) || !xrtMPMCQueueInit(
		&pQueue->Free, pQueue->Ready.Capacity
	) || !xrtMPMCQueueInit(
		&pQueue->Retry, pQueue->Ready.Capacity
	) ) {
		__xrtTypedMPMCQueueRollback(pQueue);
		return false;
	}
	if ( !__xrtTypedMPMCQueueFillFree(pQueue) ) {
		__xrtTypedMPMCQueueRollback(pQueue);
		return false;
	}
	__xrtTypedQueueCoreActivate(&pQueue->Core);
	return true;
}



/* 在堆上创建一个 MPMC 类型队列。 */
XRT_API xtypedmpmcqueue* xrtTypedMPMCQueueCreate(
	const xrttype* pItemType,
	size_t iCapacity
)
{
	xtypedmpmcqueue* pQueue = (xtypedmpmcqueue*)xrtMalloc(
		sizeof(xtypedmpmcqueue)
	);

	if ( pQueue == NULL ) {
		return NULL;
	}
	if ( !xrtTypedMPMCQueueInit(pQueue, pItemType, iCapacity) ) {
		xrtFree(pQueue);
		return NULL;
	}
	return pQueue;
}



/* 释放全部 MPMC 队列值和内部环，但不释放结构。 */
XRT_API void xrtTypedMPMCQueueUnit(xtypedmpmcqueue* pQueue)
{
	(void)__xrtTypedMPMCQueueUnit(pQueue);
}



/* 释放 Create 返回的 MPMC 类型队列。 */
XRT_API void xrtTypedMPMCQueueDestroy(xtypedmpmcqueue* pQueue)
{
	if ( (pQueue != NULL) && __xrtTypedMPMCQueueUnit(pQueue) ) {
		xrtFree(pQueue);
	}
}



/* 返回 MPMC 队列借用的元素类型。 */
XRT_API const xrttype* xrtTypedMPMCQueueItemType(
	const xtypedmpmcqueue* pQueue
)
{
	return (pQueue != NULL) && __xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "item-type"
	) ? pQueue->Core.ItemType : NULL;
}



/* 返回 MPMC 队列实际固定容量。 */
XRT_API size_t xrtTypedMPMCQueueCapacity(const xtypedmpmcqueue* pQueue)
{
	return (pQueue != NULL) && __xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "capacity"
	) ? pQueue->Core.Capacity : 0u;
}



/* 返回就绪环与移动失败重试环的并发近似元素数量。 */
XRT_API size_t xrtTypedMPMCQueueCount(const xtypedmpmcqueue* pQueue)
{
	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "count"
	) ) {
		return 0u;
	}
	return __xrtTypedQueueCount(
		&pQueue->Core,
		xrtMPMCQueueCount(&pQueue->Ready),
		xrtMPMCQueueCount(&pQueue->Retry)
	);
}



/* 失败原子地复制压入一个 MPMC 类型值。 */
XRT_API xqueueresult xrtTypedMPMCQueueTryPush(
	xtypedmpmcqueue* pQueue,
	const void* pItem
)
{
	return __xrtTypedMPMCQueuePush(
		pQueue, (ptr)pItem, false, "try-push"
	);
}



/* 移动压入一个外部已初始化 MPMC 类型值。 */
XRT_API xqueueresult xrtTypedMPMCQueueTryPushTake(
	xtypedmpmcqueue* pQueue,
	ptr pItem
)
{
	return __xrtTypedMPMCQueuePush(
		pQueue, pItem, true, "try-push-take"
	);
}



/* 由任意消费者移动弹出，失败时把元素放入重试环。 */
XRT_API xqueueresult xrtTypedMPMCQueueTryPop(
	xtypedmpmcqueue* pQueue,
	ptr pValue
)
{
	ptr pCell = NULL;
	xqueueresult Result;

	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "try-pop"
	) || !__xrtTypedQueueValueValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), pValue, "try-pop"
	) || !__xrtTypedQueueCoreEnter(
		&pQueue->Core, pQueue, sizeof(*pQueue), "try-pop"
	) ) {
		return XQUEUE_ERROR;
	}
	Result = xrtMPMCQueueTryPop(&pQueue->Retry, &pCell);
	if ( Result == XQUEUE_EMPTY ) {
		Result = xrtMPMCQueueTryPop(&pQueue->Ready, &pCell);
	}
	if ( Result == XQUEUE_CLOSED ) {
		Result = xrtMPMCQueueTryPop(&pQueue->Retry, &pCell);
		if ( Result == XQUEUE_EMPTY ) {
			Result = xrtAtomic32Load(
				&pQueue->Core.Active, XMEMORY_ACQUIRE
			) == 1u ? XQUEUE_CLOSED : XQUEUE_EMPTY;
		}
	}
	if ( Result != XQUEUE_OK ) {
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		return Result;
	}
	if ( !__xrtTypedQueueMoveOut(
		&pQueue->Core, pValue, pCell, "try-pop"
	) ) {
		(void)__xrtTypedMPMCQueueRetry(pQueue, pCell, "try-pop");
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		return XQUEUE_ERROR;
	}
	if ( !__xrtTypedMPMCQueueRecycle(pQueue, pCell, "try-pop") ) {
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		return XQUEUE_ERROR;
	}
	__xrtTypedQueueCoreLeave(&pQueue->Core);
	return XQUEUE_OK;
}



/* 复制压入连续 MPMC 类型值，允许处理可用前缀。 */
XRT_API xqueuebatchresult xrtTypedMPMCQueuePushBatch(
	xtypedmpmcqueue* pQueue,
	const void* pItems,
	size_t iCount
)
{
	xqueuebatchresult Batch = { XQUEUE_OK, 0u };
	const bytes pValues = (const bytes)pItems;

	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "push-batch"
	) || !__xrtTypedQueueValuesValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), pItems, iCount,
		"push-batch"
	) ) {
		Batch.Result = XQUEUE_ERROR;
		return Batch;
	}
	while ( Batch.Count < iCount ) {
		xqueueresult Result = xrtTypedMPMCQueueTryPush(
			pQueue,
			pValues + (Batch.Count * pQueue->Core.ItemType->Size)
		);

		if ( Result != XQUEUE_OK ) {
			Batch.Result = Batch.Count != 0u ? XQUEUE_OK : Result;
			return Batch;
		}
		Batch.Count++;
	}
	return Batch;
}



/* 移动弹出到连续已初始化 MPMC 类型值，允许处理可用前缀。 */
XRT_API xqueuebatchresult xrtTypedMPMCQueuePopBatch(
	xtypedmpmcqueue* pQueue,
	ptr pValues,
	size_t iCapacity
)
{
	xqueuebatchresult Batch = { XQUEUE_OK, 0u };
	bytes pOutput = (bytes)pValues;

	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "pop-batch"
	) || !__xrtTypedQueueValuesValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), pValues, iCapacity,
		"pop-batch"
	) ) {
		Batch.Result = XQUEUE_ERROR;
		return Batch;
	}
	while ( Batch.Count < iCapacity ) {
		xqueueresult Result = xrtTypedMPMCQueueTryPop(
			pQueue,
			pOutput + (Batch.Count * pQueue->Core.ItemType->Size)
		);

		if ( Result != XQUEUE_OK ) {
			Batch.Result = Batch.Count != 0u ? XQUEUE_OK : Result;
			return Batch;
		}
		Batch.Count++;
	}
	return Batch;
}



/* 在全部生产者退出后关闭 MPMC 类型队列。 */
XRT_API void xrtTypedMPMCQueueClose(xtypedmpmcqueue* pQueue)
{
	if ( (pQueue != NULL) && __xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "close"
	) ) {
		xrtMPMCQueueClose(&pQueue->Ready);
	}
}



/* 判断 MPMC 类型队列是否已经关闭写端。 */
XRT_API bool xrtTypedMPMCQueueIsClosed(const xtypedmpmcqueue* pQueue)
{
	return (pQueue != NULL) && __xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "is-closed"
	) && xrtMPMCQueueIsClosed(&pQueue->Ready);
}



/* 判断 MPMC 类型队列是否关闭、排空且无进行中的值回调。 */
XRT_API bool xrtTypedMPMCQueueIsDrained(const xtypedmpmcqueue* pQueue)
{
	return xrtTypedMPMCQueueIsClosed(pQueue) &&
		(xrtTypedMPMCQueueCount(pQueue) == 0u) &&
		(xrtAtomic32Load(&pQueue->Core.Active, XMEMORY_ACQUIRE) == 0u);
}



/* 在独占且排空后重置全部 MPMC 环并重新开放。 */
XRT_API bool xrtTypedMPMCQueueReset(xtypedmpmcqueue* pQueue)
{
	uint32 iPrevious;
	ptr pCell;

	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "reset"
	) || !__xrtTypedQueueCoreExclusive(
		&pQueue->Core, false, &iPrevious, "reset"
	) ) {
		return false;
	}
	if ( (xrtMPMCQueueCount(&pQueue->Ready) != 0u) ||
		 (xrtMPMCQueueCount(&pQueue->Retry) != 0u) ) {
		__xrtTypedQueueCoreShared(&pQueue->Core, iPrevious);
		__xrtTypedQueueError(
			XERR_AGAIN, XTYPED_QUEUE_ERROR_STATE, "reset",
			"the MPMC typed queue must be drained before reset"
		);
		return false;
	}
	while ( xrtMPMCQueueTryPop(&pQueue->Free, &pCell) == XQUEUE_OK ) {
	}
	if ( !xrtMPMCQueueReset(&pQueue->Ready) ||
		 !xrtMPMCQueueReset(&pQueue->Free) ||
		 !xrtMPMCQueueReset(&pQueue->Retry) ) {
		__xrtTypedQueueCoreBreak(
			&pQueue->Core, "reset",
			"the MPMC pointer rings could not be reset"
		);
		return false;
	}
	if ( !__xrtTypedMPMCQueueFillFree(pQueue) ) {
		__xrtTypedQueueCoreBreak(
			&pQueue->Core, "reset",
			"the MPMC free value slots could not be restored"
		);
		return false;
	}
	__xrtTypedQueueCoreShared(&pQueue->Core, iPrevious);
	return true;
}



/* 从对象类型参数和元数据初始化 MPMC 队列负载。 */
static bool __xrtTypedMPMCQueueInstanceInit(
	ptr pInstance,
	const xrttype* pType
)
{
	const xtypedqueuemeta* pMeta;

	if ( !xrtTypedMPMCQueueTypeValidate(pType) ) {
		return false;
	}
	pMeta = (const xtypedqueuemeta*)pType->Metadata;
	return xrtTypedMPMCQueueInit(
		(xtypedmpmcqueue*)pInstance, pType->Arguments[0], pMeta->Capacity
	);
}



/* 销毁对象负载中的 MPMC 类型队列。 */
static void __xrtTypedMPMCQueueInstanceDrop(
	ptr pInstance,
	const xrttype* pType
)
{
	(void)pType;
	xrtTypedMPMCQueueUnit((xtypedmpmcqueue*)pInstance);
}



/* 枚举 MPMC 固定值槽直接拥有的全部强对象引用。 */
static bool __xrtTypedMPMCQueueInstanceTrace(
	const void* pInstance,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	(void)pType;
	return __xrtTypedQueueTrace(
		&((xtypedmpmcqueue*)pInstance)->Core,
		pInstance,
		sizeof(xtypedmpmcqueue),
		pVisit,
		pContext,
		"instance-trace"
	);
}



/* 返回 MPMC 类型队列共享实例操作表。 */
XRT_API const xrtinstanceops* xrtTypedMPMCQueueInstanceOps(void)
{
	static const xrtinstanceops Ops = {
		.Init = __xrtTypedMPMCQueueInstanceInit,
		.Drop = __xrtTypedMPMCQueueInstanceDrop,
		.Trace = __xrtTypedMPMCQueueInstanceTrace
	};

	return &Ops;
}



/* 验证可由对象系统承载的 MPMC 类型队列描述。 */
XRT_API bool xrtTypedMPMCQueueTypeValidate(const xrttype* pType)
{
	return __xrtTypedQueueTypeValidate(
		pType,
		sizeof(xtypedmpmcqueue),
		XRT_INTERNAL_OBJECT_ALIGNOF(xtypedmpmcqueue),
		xrtTypedMPMCQueueInstanceOps(),
		"type-validate"
	);
}

#endif
