#include "../internal/xrt_typed_queue.h"



#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_MPSC)

/* 清理初始化中途失败的 MPSC 类型队列并保留根错误。 */
static void __xrtTypedMPSCQueueRollback(xtypedmpscqueue* pQueue)
{
	xerror* pError = xrtTakeError();

	xrtMPMCQueueUnit(&pQueue->Free);
	xrtMPSCQueueUnit(&pQueue->Ready);
	__xrtTypedQueueCoreUnit(&pQueue->Core);
	memset(pQueue, 0, sizeof(*pQueue));
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 把全部固定值槽发布到并发空闲环。 */
static bool __xrtTypedMPSCQueueFillFree(xtypedmpscqueue* pQueue)
{
	for ( size_t i = 0u; i < pQueue->Core.Capacity; i++ ) {
		if ( xrtMPMCQueueTryPush(
			&pQueue->Free, __xrtTypedQueueCell(&pQueue->Core, i)
		) != XQUEUE_OK ) {
			__xrtTypedQueueWrap(
				XERR_STATE, XTYPED_QUEUE_ERROR_STATE, "init",
				"an MPSC free value slot could not be published"
			);
			return false;
		}
	}
	return true;
}



/* 把空值槽归还给允许多个生产者领取的空闲环。 */
static bool __xrtTypedMPSCQueueRecycle(
	xtypedmpscqueue* pQueue,
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
		"an MPSC value slot could not be recycled"
	);
	return false;
}



/* 结束一个已初始化 MPSC 类型队列，失败表示仍有并发访问。 */
static bool __xrtTypedMPSCQueueUnit(xtypedmpscqueue* pQueue)
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
	xrtMPMCQueueUnit(&pQueue->Free);
	xrtMPSCQueueUnit(&pQueue->Ready);
	__xrtTypedQueueCoreUnit(&pQueue->Core);
	memset(pQueue, 0, sizeof(*pQueue));
	return true;
}



/* 共用复制和移动两条 MPSC 入队路径。 */
static xqueueresult __xrtTypedMPSCQueuePush(
	xtypedmpscqueue* pQueue,
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
	if ( xrtMPSCQueueIsClosed(&pQueue->Ready) ) {
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
			"the MPSC free value-slot queue is invalid"
		);
		return XQUEUE_ERROR;
	}
	bStored = bTake ?
		__xrtTypedQueueMoveIn(&pQueue->Core, pCell, pItem, sOperation) :
		__xrtTypedQueueCopyIn(&pQueue->Core, pCell, pItem, sOperation);
	if ( !bStored ) {
		(void)__xrtTypedMPSCQueueRecycle(pQueue, pCell, sOperation);
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		return XQUEUE_ERROR;
	}
	do {
		Result = xrtMPSCQueueTryPush(&pQueue->Ready, pCell);
		if ( Result == XQUEUE_FULL ) {
			xrtAtomicPause();
		}
	} while ( Result == XQUEUE_FULL );
	if ( Result != XQUEUE_OK ) {
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		__xrtTypedQueueCoreBreak(
			&pQueue->Core, sOperation,
			"a prepared MPSC value slot could not be published"
		);
		return XQUEUE_ERROR;
	}
	__xrtTypedQueueCoreLeave(&pQueue->Core);
	return XQUEUE_OK;
}



/* 初始化一个拥有固定值槽的 MPSC 类型队列。 */
XRT_API bool xrtTypedMPSCQueueInit(
	xtypedmpscqueue* pQueue,
	const xrttype* pItemType,
	size_t iCapacity
)
{
	if ( pQueue == NULL ) {
		__xrtTypedQueueError(
			XERR_ARGUMENT, XTYPED_QUEUE_ERROR_ARGUMENT, "init",
			"the MPSC typed queue is null"
		);
		return false;
	}
	memset(pQueue, 0, sizeof(*pQueue));
	if ( !xrtMPSCQueueInit(&pQueue->Ready, iCapacity) ) {
		return false;
	}
	if ( !__xrtTypedQueueCoreInit(
		&pQueue->Core, pItemType, pQueue->Ready.Capacity, "init"
	) || !xrtMPMCQueueInit(
		&pQueue->Free, pQueue->Ready.Capacity
	) ) {
		__xrtTypedMPSCQueueRollback(pQueue);
		return false;
	}
	xrtAtomicPtrInit(&pQueue->PopCell, NULL);
	if ( !__xrtTypedMPSCQueueFillFree(pQueue) ) {
		__xrtTypedMPSCQueueRollback(pQueue);
		return false;
	}
	__xrtTypedQueueCoreActivate(&pQueue->Core);
	return true;
}



/* 在堆上创建一个 MPSC 类型队列。 */
XRT_API xtypedmpscqueue* xrtTypedMPSCQueueCreate(
	const xrttype* pItemType,
	size_t iCapacity
)
{
	xtypedmpscqueue* pQueue = (xtypedmpscqueue*)xrtMalloc(
		sizeof(xtypedmpscqueue)
	);

	if ( pQueue == NULL ) {
		return NULL;
	}
	if ( !xrtTypedMPSCQueueInit(pQueue, pItemType, iCapacity) ) {
		xrtFree(pQueue);
		return NULL;
	}
	return pQueue;
}



/* 释放全部 MPSC 队列值和内部环，但不释放结构。 */
XRT_API void xrtTypedMPSCQueueUnit(xtypedmpscqueue* pQueue)
{
	(void)__xrtTypedMPSCQueueUnit(pQueue);
}



/* 释放 Create 返回的 MPSC 类型队列。 */
XRT_API void xrtTypedMPSCQueueDestroy(xtypedmpscqueue* pQueue)
{
	if ( (pQueue != NULL) && __xrtTypedMPSCQueueUnit(pQueue) ) {
		xrtFree(pQueue);
	}
}



/* 返回 MPSC 队列借用的元素类型。 */
XRT_API const xrttype* xrtTypedMPSCQueueItemType(
	const xtypedmpscqueue* pQueue
)
{
	return (pQueue != NULL) && __xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "item-type"
	) ? pQueue->Core.ItemType : NULL;
}



/* 返回 MPSC 队列实际固定容量。 */
XRT_API size_t xrtTypedMPSCQueueCapacity(const xtypedmpscqueue* pQueue)
{
	return (pQueue != NULL) && __xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "capacity"
	) ? pQueue->Core.Capacity : 0u;
}



/* 返回包含移动失败重试槽的并发近似元素数量。 */
XRT_API size_t xrtTypedMPSCQueueCount(const xtypedmpscqueue* pQueue)
{
	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "count"
	) ) {
		return 0u;
	}
	return __xrtTypedQueueCount(
		&pQueue->Core,
		xrtMPSCQueueCount(&pQueue->Ready),
		xrtAtomicPtrLoad(&pQueue->PopCell, XMEMORY_ACQUIRE) != NULL ? 1u : 0u
	);
}



/* 失败原子地复制压入一个 MPSC 类型值。 */
XRT_API xqueueresult xrtTypedMPSCQueueTryPush(
	xtypedmpscqueue* pQueue,
	const void* pItem
)
{
	return __xrtTypedMPSCQueuePush(
		pQueue, (ptr)pItem, false, "try-push"
	);
}



/* 移动压入一个外部已初始化 MPSC 类型值。 */
XRT_API xqueueresult xrtTypedMPSCQueueTryPushTake(
	xtypedmpscqueue* pQueue,
	ptr pItem
)
{
	return __xrtTypedMPSCQueuePush(
		pQueue, pItem, true, "try-push-take"
	);
}



/* 由唯一消费者移动弹出，移动失败时保留重试槽。 */
XRT_API xqueueresult xrtTypedMPSCQueueTryPop(
	xtypedmpscqueue* pQueue,
	ptr pValue
)
{
	ptr pCell;
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
	pCell = xrtAtomicPtrExchange(
		&pQueue->PopCell, NULL, XMEMORY_ACQ_REL
	);
	if ( pCell == NULL ) {
		Result = xrtMPSCQueueTryPop(&pQueue->Ready, &pCell);
		if ( Result != XQUEUE_OK ) {
			__xrtTypedQueueCoreLeave(&pQueue->Core);
			return Result;
		}
	}
	if ( !__xrtTypedQueueMoveOut(
		&pQueue->Core, pValue, pCell, "try-pop"
	) ) {
		xrtAtomicPtrStore(&pQueue->PopCell, pCell, XMEMORY_RELEASE);
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		return XQUEUE_ERROR;
	}
	if ( !__xrtTypedMPSCQueueRecycle(pQueue, pCell, "try-pop") ) {
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		return XQUEUE_ERROR;
	}
	__xrtTypedQueueCoreLeave(&pQueue->Core);
	return XQUEUE_OK;
}



/* 复制压入连续 MPSC 类型值，允许处理可用前缀。 */
XRT_API xqueuebatchresult xrtTypedMPSCQueuePushBatch(
	xtypedmpscqueue* pQueue,
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
		xqueueresult Result = xrtTypedMPSCQueueTryPush(
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



/* 移动弹出到连续已初始化 MPSC 类型值，允许处理可用前缀。 */
XRT_API xqueuebatchresult xrtTypedMPSCQueuePopBatch(
	xtypedmpscqueue* pQueue,
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
		xqueueresult Result = xrtTypedMPSCQueueTryPop(
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



/* 在全部生产者退出后关闭 MPSC 类型队列。 */
XRT_API void xrtTypedMPSCQueueClose(xtypedmpscqueue* pQueue)
{
	if ( (pQueue != NULL) && __xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "close"
	) ) {
		xrtMPSCQueueClose(&pQueue->Ready);
	}
}



/* 判断 MPSC 类型队列是否已经关闭写端。 */
XRT_API bool xrtTypedMPSCQueueIsClosed(const xtypedmpscqueue* pQueue)
{
	return (pQueue != NULL) && __xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "is-closed"
	) && xrtMPSCQueueIsClosed(&pQueue->Ready);
}



/* 判断 MPSC 类型队列是否关闭、排空且无进行中的值回调。 */
XRT_API bool xrtTypedMPSCQueueIsDrained(const xtypedmpscqueue* pQueue)
{
	return xrtTypedMPSCQueueIsClosed(pQueue) &&
		(xrtTypedMPSCQueueCount(pQueue) == 0u) &&
		(xrtAtomic32Load(&pQueue->Core.Active, XMEMORY_ACQUIRE) == 0u);
}



/* 在独占且排空后重置全部 MPSC 环并重新开放。 */
XRT_API bool xrtTypedMPSCQueueReset(xtypedmpscqueue* pQueue)
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
	if ( (xrtMPSCQueueCount(&pQueue->Ready) != 0u) ||
		 (xrtAtomicPtrLoad(&pQueue->PopCell, XMEMORY_ACQUIRE) != NULL) ) {
		__xrtTypedQueueCoreShared(&pQueue->Core, iPrevious);
		__xrtTypedQueueError(
			XERR_AGAIN, XTYPED_QUEUE_ERROR_STATE, "reset",
			"the MPSC typed queue must be drained before reset"
		);
		return false;
	}
	while ( xrtMPMCQueueTryPop(&pQueue->Free, &pCell) == XQUEUE_OK ) {
	}
	if ( !xrtMPSCQueueReset(&pQueue->Ready) ||
		 !xrtMPMCQueueReset(&pQueue->Free) ) {
		__xrtTypedQueueCoreBreak(
			&pQueue->Core, "reset",
			"the MPSC pointer rings could not be reset"
		);
		return false;
	}
	xrtAtomicPtrStore(&pQueue->PopCell, NULL, XMEMORY_RELAXED);
	if ( !__xrtTypedMPSCQueueFillFree(pQueue) ) {
		__xrtTypedQueueCoreBreak(
			&pQueue->Core, "reset",
			"the MPSC free value slots could not be restored"
		);
		return false;
	}
	__xrtTypedQueueCoreShared(&pQueue->Core, iPrevious);
	return true;
}



/* 从对象类型参数和元数据初始化 MPSC 队列负载。 */
static bool __xrtTypedMPSCQueueInstanceInit(
	ptr pInstance,
	const xrttype* pType
)
{
	const xtypedqueuemeta* pMeta;

	if ( !xrtTypedMPSCQueueTypeValidate(pType) ) {
		return false;
	}
	pMeta = (const xtypedqueuemeta*)pType->Metadata;
	return xrtTypedMPSCQueueInit(
		(xtypedmpscqueue*)pInstance, pType->Arguments[0], pMeta->Capacity
	);
}



/* 销毁对象负载中的 MPSC 类型队列。 */
static void __xrtTypedMPSCQueueInstanceDrop(
	ptr pInstance,
	const xrttype* pType
)
{
	(void)pType;
	xrtTypedMPSCQueueUnit((xtypedmpscqueue*)pInstance);
}



/* 枚举 MPSC 固定值槽直接拥有的全部强对象引用。 */
static bool __xrtTypedMPSCQueueInstanceTrace(
	const void* pInstance,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	(void)pType;
	return __xrtTypedQueueTrace(
		&((xtypedmpscqueue*)pInstance)->Core,
		pInstance,
		sizeof(xtypedmpscqueue),
		pVisit,
		pContext,
		"instance-trace"
	);
}



/* 返回 MPSC 类型队列共享实例操作表。 */
XRT_API const xrtinstanceops* xrtTypedMPSCQueueInstanceOps(void)
{
	static const xrtinstanceops Ops = {
		.Init = __xrtTypedMPSCQueueInstanceInit,
		.Drop = __xrtTypedMPSCQueueInstanceDrop,
		.Trace = __xrtTypedMPSCQueueInstanceTrace
	};

	return &Ops;
}



/* 验证可由对象系统承载的 MPSC 类型队列描述。 */
XRT_API bool xrtTypedMPSCQueueTypeValidate(const xrttype* pType)
{
	return __xrtTypedQueueTypeValidate(
		pType,
		sizeof(xtypedmpscqueue),
		XRT_INTERNAL_OBJECT_ALIGNOF(xtypedmpscqueue),
		xrtTypedMPSCQueueInstanceOps(),
		"type-validate"
	);
}

#endif
