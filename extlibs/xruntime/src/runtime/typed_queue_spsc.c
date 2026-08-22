#include "../internal/xrt_typed_queue.h"



#if defined(XRUNTIME_FEATURE_TYPED_QUEUE_SPSC)

/* 清理初始化中途失败的 SPSC 类型队列并保留根错误。 */
static void __xrtTypedSPSCQueueRollback(xtypedspscqueue* pQueue)
{
	xerror* pError = xrtTakeError();

	xrtSPSCQueueUnit(&pQueue->Free);
	xrtSPSCQueueUnit(&pQueue->Ready);
	__xrtTypedQueueCoreUnit(&pQueue->Core);
	memset(pQueue, 0, sizeof(*pQueue));
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 把全部固定值槽发布到反向 SPSC 空闲环。 */
static bool __xrtTypedSPSCQueueFillFree(xtypedspscqueue* pQueue)
{
	for ( size_t i = 0u; i < pQueue->Core.Capacity; i++ ) {
		if ( xrtSPSCQueueTryPush(
			&pQueue->Free, __xrtTypedQueueCell(&pQueue->Core, i)
		) != XQUEUE_OK ) {
			__xrtTypedQueueWrap(
				XERR_STATE, XTYPED_QUEUE_ERROR_STATE, "init",
				"an SPSC free value slot could not be published"
			);
			return false;
		}
	}
	return true;
}



/* 结束一个已初始化 SPSC 类型队列，失败表示仍有并发访问。 */
static bool __xrtTypedSPSCQueueUnit(xtypedspscqueue* pQueue)
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
	xrtSPSCQueueUnit(&pQueue->Free);
	xrtSPSCQueueUnit(&pQueue->Ready);
	__xrtTypedQueueCoreUnit(&pQueue->Core);
	memset(pQueue, 0, sizeof(*pQueue));
	return true;
}



/* 共用复制和移动两条 SPSC 入队路径。 */
static xqueueresult __xrtTypedSPSCQueuePush(
	xtypedspscqueue* pQueue,
	ptr pItem,
	bool bTake,
	cstr sOperation
)
{
	ptr pCell;
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
	if ( xrtSPSCQueueIsClosed(&pQueue->Ready) ) {
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		return XQUEUE_CLOSED;
	}
	pCell = xrtAtomicPtrExchange(
		&pQueue->PushCell, NULL, XMEMORY_ACQ_REL
	);
	if ( pCell == NULL ) {
		Result = xrtSPSCQueueTryPop(&pQueue->Free, &pCell);
		if ( Result != XQUEUE_OK ) {
			__xrtTypedQueueCoreLeave(&pQueue->Core);
			if ( Result == XQUEUE_EMPTY ) {
				return XQUEUE_FULL;
			}
			__xrtTypedQueueCoreBreak(
				&pQueue->Core, sOperation,
				"the SPSC free value-slot queue is invalid"
			);
			return XQUEUE_ERROR;
		}
	}
	bStored = bTake ?
		__xrtTypedQueueMoveIn(&pQueue->Core, pCell, pItem, sOperation) :
		__xrtTypedQueueCopyIn(&pQueue->Core, pCell, pItem, sOperation);
	if ( !bStored ) {
		xrtAtomicPtrStore(
			&pQueue->PushCell, pCell, XMEMORY_RELEASE
		);
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		return XQUEUE_ERROR;
	}
	Result = xrtSPSCQueueTryPush(&pQueue->Ready, pCell);
	if ( Result != XQUEUE_OK ) {
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		__xrtTypedQueueCoreBreak(
			&pQueue->Core, sOperation,
			"a prepared SPSC value slot could not be published"
		);
		return XQUEUE_ERROR;
	}
	__xrtTypedQueueCoreLeave(&pQueue->Core);
	return XQUEUE_OK;
}



/* 初始化一个拥有固定值槽的 SPSC 类型队列。 */
XRT_API bool xrtTypedSPSCQueueInit(
	xtypedspscqueue* pQueue,
	const xrttype* pItemType,
	size_t iCapacity
)
{
	if ( pQueue == NULL ) {
		__xrtTypedQueueError(
			XERR_ARGUMENT, XTYPED_QUEUE_ERROR_ARGUMENT, "init",
			"the SPSC typed queue is null"
		);
		return false;
	}
	memset(pQueue, 0, sizeof(*pQueue));
	if ( !xrtSPSCQueueInit(&pQueue->Ready, iCapacity) ) {
		return false;
	}
	if ( !__xrtTypedQueueCoreInit(
		&pQueue->Core, pItemType, pQueue->Ready.Capacity, "init"
	) || !xrtSPSCQueueInit(
		&pQueue->Free, pQueue->Ready.Capacity
	) ) {
		__xrtTypedSPSCQueueRollback(pQueue);
		return false;
	}
	xrtAtomicPtrInit(&pQueue->PushCell, NULL);
	xrtAtomicPtrInit(&pQueue->PopCell, NULL);
	if ( !__xrtTypedSPSCQueueFillFree(pQueue) ) {
		__xrtTypedSPSCQueueRollback(pQueue);
		return false;
	}
	__xrtTypedQueueCoreActivate(&pQueue->Core);
	return true;
}



/* 在堆上创建一个 SPSC 类型队列。 */
XRT_API xtypedspscqueue* xrtTypedSPSCQueueCreate(
	const xrttype* pItemType,
	size_t iCapacity
)
{
	xtypedspscqueue* pQueue = (xtypedspscqueue*)xrtMalloc(
		sizeof(xtypedspscqueue)
	);

	if ( pQueue == NULL ) {
		return NULL;
	}
	if ( !xrtTypedSPSCQueueInit(pQueue, pItemType, iCapacity) ) {
		xrtFree(pQueue);
		return NULL;
	}
	return pQueue;
}



/* 释放全部 SPSC 队列值和内部环，但不释放结构。 */
XRT_API void xrtTypedSPSCQueueUnit(xtypedspscqueue* pQueue)
{
	(void)__xrtTypedSPSCQueueUnit(pQueue);
}



/* 释放 Create 返回的 SPSC 类型队列。 */
XRT_API void xrtTypedSPSCQueueDestroy(xtypedspscqueue* pQueue)
{
	if ( (pQueue != NULL) && __xrtTypedSPSCQueueUnit(pQueue) ) {
		xrtFree(pQueue);
	}
}



/* 返回 SPSC 队列借用的元素类型。 */
XRT_API const xrttype* xrtTypedSPSCQueueItemType(
	const xtypedspscqueue* pQueue
)
{
	return (pQueue != NULL) && __xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "item-type"
	) ? pQueue->Core.ItemType : NULL;
}



/* 返回 SPSC 队列实际固定容量。 */
XRT_API size_t xrtTypedSPSCQueueCapacity(const xtypedspscqueue* pQueue)
{
	return (pQueue != NULL) && __xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "capacity"
	) ? pQueue->Core.Capacity : 0u;
}



/* 返回包含移动失败重试槽的并发近似元素数量。 */
XRT_API size_t xrtTypedSPSCQueueCount(const xtypedspscqueue* pQueue)
{
	if ( (pQueue == NULL) || !__xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "count"
	) ) {
		return 0u;
	}
	return __xrtTypedQueueCount(
		&pQueue->Core,
		xrtSPSCQueueCount(&pQueue->Ready),
		xrtAtomicPtrLoad(&pQueue->PopCell, XMEMORY_ACQUIRE) != NULL ? 1u : 0u
	);
}



/* 失败原子地复制压入一个 SPSC 类型值。 */
XRT_API xqueueresult xrtTypedSPSCQueueTryPush(
	xtypedspscqueue* pQueue,
	const void* pItem
)
{
	return __xrtTypedSPSCQueuePush(
		pQueue, (ptr)pItem, false, "try-push"
	);
}



/* 移动压入一个外部已初始化 SPSC 类型值。 */
XRT_API xqueueresult xrtTypedSPSCQueueTryPushTake(
	xtypedspscqueue* pQueue,
	ptr pItem
)
{
	return __xrtTypedSPSCQueuePush(
		pQueue, pItem, true, "try-push-take"
	);
}



/* 移动弹出一个 SPSC 类型值，移动失败时保留重试槽。 */
XRT_API xqueueresult xrtTypedSPSCQueueTryPop(
	xtypedspscqueue* pQueue,
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
		Result = xrtSPSCQueueTryPop(&pQueue->Ready, &pCell);
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
	Result = xrtSPSCQueueTryPush(&pQueue->Free, pCell);
	if ( Result != XQUEUE_OK ) {
		__xrtTypedQueueCoreLeave(&pQueue->Core);
		__xrtTypedQueueCoreBreak(
			&pQueue->Core, "try-pop",
			"a consumed SPSC value slot could not be recycled"
		);
		return XQUEUE_ERROR;
	}
	__xrtTypedQueueCoreLeave(&pQueue->Core);
	return XQUEUE_OK;
}



/* 复制压入连续 SPSC 类型值，允许处理可用前缀。 */
XRT_API xqueuebatchresult xrtTypedSPSCQueuePushBatch(
	xtypedspscqueue* pQueue,
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
		xqueueresult Result = xrtTypedSPSCQueueTryPush(
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



/* 移动弹出到连续已初始化 SPSC 类型值，允许处理可用前缀。 */
XRT_API xqueuebatchresult xrtTypedSPSCQueuePopBatch(
	xtypedspscqueue* pQueue,
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
		xqueueresult Result = xrtTypedSPSCQueueTryPop(
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



/* 由唯一生产者停止写入后关闭 SPSC 类型队列。 */
XRT_API void xrtTypedSPSCQueueClose(xtypedspscqueue* pQueue)
{
	if ( (pQueue != NULL) && __xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "close"
	) ) {
		xrtSPSCQueueClose(&pQueue->Ready);
	}
}



/* 判断 SPSC 类型队列是否已经关闭写端。 */
XRT_API bool xrtTypedSPSCQueueIsClosed(const xtypedspscqueue* pQueue)
{
	return (pQueue != NULL) && __xrtTypedQueueCoreValid(
		&pQueue->Core, pQueue, sizeof(*pQueue), "is-closed"
	) && xrtSPSCQueueIsClosed(&pQueue->Ready);
}



/* 判断 SPSC 类型队列是否关闭、排空且无进行中的值回调。 */
XRT_API bool xrtTypedSPSCQueueIsDrained(const xtypedspscqueue* pQueue)
{
	return xrtTypedSPSCQueueIsClosed(pQueue) &&
		(xrtTypedSPSCQueueCount(pQueue) == 0u) &&
		(xrtAtomic32Load(&pQueue->Core.Active, XMEMORY_ACQUIRE) == 0u);
}



/* 在独占且排空后重置全部 SPSC 环并重新开放。 */
XRT_API bool xrtTypedSPSCQueueReset(xtypedspscqueue* pQueue)
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
	if ( (xrtSPSCQueueCount(&pQueue->Ready) != 0u) ||
		 (xrtAtomicPtrLoad(&pQueue->PopCell, XMEMORY_ACQUIRE) != NULL) ) {
		__xrtTypedQueueCoreShared(&pQueue->Core, iPrevious);
		__xrtTypedQueueError(
			XERR_AGAIN, XTYPED_QUEUE_ERROR_STATE, "reset",
			"the SPSC typed queue must be drained before reset"
		);
		return false;
	}
	while ( xrtSPSCQueueTryPop(&pQueue->Free, &pCell) == XQUEUE_OK ) {
	}
	if ( !xrtSPSCQueueReset(&pQueue->Ready) ||
		 !xrtSPSCQueueReset(&pQueue->Free) ) {
		__xrtTypedQueueCoreBreak(
			&pQueue->Core, "reset",
			"the SPSC pointer rings could not be reset"
		);
		return false;
	}
	xrtAtomicPtrStore(&pQueue->PushCell, NULL, XMEMORY_RELAXED);
	xrtAtomicPtrStore(&pQueue->PopCell, NULL, XMEMORY_RELAXED);
	if ( !__xrtTypedSPSCQueueFillFree(pQueue) ) {
		__xrtTypedQueueCoreBreak(
			&pQueue->Core, "reset",
			"the SPSC free value slots could not be restored"
		);
		return false;
	}
	__xrtTypedQueueCoreShared(&pQueue->Core, iPrevious);
	return true;
}



/* 从对象类型参数和元数据初始化 SPSC 队列负载。 */
static bool __xrtTypedSPSCQueueInstanceInit(
	ptr pInstance,
	const xrttype* pType
)
{
	const xtypedqueuemeta* pMeta;

	if ( !xrtTypedSPSCQueueTypeValidate(pType) ) {
		return false;
	}
	pMeta = (const xtypedqueuemeta*)pType->Metadata;
	return xrtTypedSPSCQueueInit(
		(xtypedspscqueue*)pInstance, pType->Arguments[0], pMeta->Capacity
	);
}



/* 销毁对象负载中的 SPSC 类型队列。 */
static void __xrtTypedSPSCQueueInstanceDrop(
	ptr pInstance,
	const xrttype* pType
)
{
	(void)pType;
	xrtTypedSPSCQueueUnit((xtypedspscqueue*)pInstance);
}



/* 枚举 SPSC 固定值槽直接拥有的全部强对象引用。 */
static bool __xrtTypedSPSCQueueInstanceTrace(
	const void* pInstance,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	(void)pType;
	return __xrtTypedQueueTrace(
		&((xtypedspscqueue*)pInstance)->Core,
		pInstance,
		sizeof(xtypedspscqueue),
		pVisit,
		pContext,
		"instance-trace"
	);
}



/* 返回 SPSC 类型队列共享实例操作表。 */
XRT_API const xrtinstanceops* xrtTypedSPSCQueueInstanceOps(void)
{
	static const xrtinstanceops Ops = {
		.Init = __xrtTypedSPSCQueueInstanceInit,
		.Drop = __xrtTypedSPSCQueueInstanceDrop,
		.Trace = __xrtTypedSPSCQueueInstanceTrace
	};

	return &Ops;
}



/* 验证可由对象系统承载的 SPSC 类型队列描述。 */
XRT_API bool xrtTypedSPSCQueueTypeValidate(const xrttype* pType)
{
	return __xrtTypedQueueTypeValidate(
		pType,
		sizeof(xtypedspscqueue),
		XRT_INTERNAL_OBJECT_ALIGNOF(xtypedspscqueue),
		xrtTypedSPSCQueueInstanceOps(),
		"type-validate"
	);
}

#endif
