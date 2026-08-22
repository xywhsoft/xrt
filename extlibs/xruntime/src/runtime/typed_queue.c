#include "../internal/xrt_typed_queue.h"



#if defined(XRUNTIME_FEATURE_TYPED_QUEUE)

/* 设置类型队列模块结构化错误。 */
void __xrtTypedQueueError(
	xerrkind Kind,
	xtypedqueueerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.typed-queue";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为下层类型或基础队列错误补充类型队列上下文。 */
void __xrtTypedQueueWrap(
	xerrkind DefaultKind,
	xtypedqueueerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.typed-queue";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 验证元素类型具有固定值槽需要的复制和移动能力。 */
static bool __xrtTypedQueueItemTypeValidate(
	const xrttype* pItemType,
	cstr sOperation
)
{
	bool bMovable;

	if ( !xrtTypeValidate(pItemType) ) {
		__xrtTypedQueueWrap(
			XERR_ARGUMENT, XTYPED_QUEUE_ERROR_TYPE, sOperation,
			"the queue item type is invalid"
		);
		return false;
	}
	if ( pItemType->Size == 0u ) {
		__xrtTypedQueueError(
			XERR_TYPE, XTYPED_QUEUE_ERROR_TYPE, sOperation,
			"a typed queue item must occupy storage"
		);
		return false;
	}
	if ( !xrtTypeIsCopyable(pItemType) ) {
		__xrtTypedQueueError(
			XERR_UNSUPPORTED, XTYPED_QUEUE_ERROR_TYPE, sOperation,
			"the queue item type is not copyable"
		);
		return false;
	}
	bMovable = ((pItemType->Ops != NULL) &&
		(pItemType->Ops->Move != NULL)) ||
		((pItemType->Flags & XRT_TYPE_FLAG_TRIVIAL_COPY) != 0u);
	if ( !bMovable ) {
		__xrtTypedQueueError(
			XERR_UNSUPPORTED, XTYPED_QUEUE_ERROR_TYPE, sOperation,
			"the queue item type has no move operation"
		);
		return false;
	}
	return true;
}



/* 按元素对齐向上计算一个固定值槽跨度。 */
static bool __xrtTypedQueueStride(
	const xrttype* pItemType,
	size_t* pStride
)
{
	size_t iMask = pItemType->Align - 1u;

	if ( pItemType->Size > (SIZE_MAX - iMask) ) {
		__xrtTypedQueueError(
			XERR_RANGE, XTYPED_QUEUE_ERROR_LAYOUT, "init",
			"the queue item stride overflows size_t"
		);
		return false;
	}
	*pStride = (pItemType->Size + iMask) & ~iMask;
	return true;
}



/* 初始化固定数量的对齐值槽，但暂不发布核心状态。 */
bool __xrtTypedQueueCoreInit(
	xtypedqueuecore* pCore,
	const xrttype* pItemType,
	size_t iCapacity,
	cstr sOperation
)
{
	uintptr_t iRaw;
	uintptr_t iAligned;
	size_t iAllocationSize;
	size_t iInitialized = 0u;

	if ( pCore == NULL ) {
		__xrtTypedQueueError(
			XERR_ARGUMENT, XTYPED_QUEUE_ERROR_ARGUMENT, sOperation,
			"the queue core is null"
		);
		return false;
	}
	memset(pCore, 0, sizeof(*pCore));
	if ( !__xrtTypedQueueItemTypeValidate(pItemType, sOperation) ) {
		return false;
	}
	if ( (iCapacity == 0u) || (iCapacity > XRT_QUEUE_MAX_CAPACITY) ) {
		__xrtTypedQueueError(
			XERR_RANGE, XTYPED_QUEUE_ERROR_LAYOUT, sOperation,
			"the queue capacity is outside the supported range"
		);
		return false;
	}
	if ( !__xrtTypedQueueStride(pItemType, &pCore->Stride) ||
		 (iCapacity > (SIZE_MAX / pCore->Stride)) ) {
		if ( xrtGetError() == NULL ) {
			__xrtTypedQueueError(
				XERR_RANGE, XTYPED_QUEUE_ERROR_LAYOUT, sOperation,
				"the queue value storage size overflows size_t"
			);
		}
		memset(pCore, 0, sizeof(*pCore));
		return false;
	}
	pCore->ValueBytes = iCapacity * pCore->Stride;
	if ( pCore->ValueBytes > (SIZE_MAX - (pItemType->Align - 1u)) ) {
		__xrtTypedQueueError(
			XERR_RANGE, XTYPED_QUEUE_ERROR_LAYOUT, sOperation,
			"the aligned queue allocation size overflows size_t"
		);
		memset(pCore, 0, sizeof(*pCore));
		return false;
	}
	iAllocationSize = pCore->ValueBytes + pItemType->Align - 1u;
	pCore->Allocation = xrtMalloc(iAllocationSize);
	if ( pCore->Allocation == NULL ) {
		memset(pCore, 0, sizeof(*pCore));
		return false;
	}
	iRaw = (uintptr_t)pCore->Allocation;
	iAligned = (iRaw + pItemType->Align - 1u) &
		~(uintptr_t)(pItemType->Align - 1u);
	pCore->Values = (bytes)iAligned;
	pCore->ItemType = pItemType;
	pCore->Capacity = iCapacity;
	xrtAtomic32Init(&pCore->State, XRT_TYPED_QUEUE_STATE_EMPTY);
	xrtAtomic32Init(&pCore->Active, 0u);

	while ( iInitialized < iCapacity ) {
		ptr pCell = pCore->Values + (iInitialized * pCore->Stride);

		if ( !xrtTypeInitValue(pItemType, pCell) ) {
			xerror* pError = xrtTakeError();

			while ( iInitialized != 0u ) {
				iInitialized--;
				xrtTypeDropValue(
					pItemType,
					pCore->Values + (iInitialized * pCore->Stride)
				);
			}
			xrtFree(pCore->Allocation);
			memset(pCore, 0, sizeof(*pCore));
			if ( pError != NULL ) {
				xrtSetError(pError);
				xrtErrorFree(pError);
			}
			__xrtTypedQueueWrap(
				XERR_STATE, XTYPED_QUEUE_ERROR_OPERATION, sOperation,
				"a queue value slot could not be initialized"
			);
			return false;
		}
		iInitialized++;
	}
	return true;
}



/* 发布已经完成全部基础队列初始化的类型队列核心。 */
void __xrtTypedQueueCoreActivate(xtypedqueuecore* pCore)
{
	xrtAtomic32Store(
		&pCore->State, XRT_TYPED_QUEUE_STATE_READY, XMEMORY_RELEASE
	);
}



/* 检查类型队列核心公开状态、布局和拥有者边界。 */
bool __xrtTypedQueueCoreValid(
	const xtypedqueuecore* pCore,
	const void* pOwner,
	size_t iOwnerSize,
	cstr sOperation
)
{
	uint32 iState;
	uintptr_t iAllocation;
	uintptr_t iValues;
	size_t iGap;

	if ( (pCore == NULL) || (pOwner == NULL) || (pCore->ItemType == NULL) ) {
		__xrtTypedQueueError(
			XERR_ARGUMENT, XTYPED_QUEUE_ERROR_ARGUMENT, sOperation,
			"the typed queue is null or uninitialized"
		);
		return false;
	}
	iState = xrtAtomic32Load(&pCore->State, XMEMORY_ACQUIRE);
	if ( iState != XRT_TYPED_QUEUE_STATE_READY ) {
		__xrtTypedQueueError(
			XERR_STATE, XTYPED_QUEUE_ERROR_STATE, sOperation,
			iState == XRT_TYPED_QUEUE_STATE_BROKEN ?
				"the typed queue is broken" :
				"the typed queue is not available for API access"
		);
		return false;
	}
	iAllocation = (uintptr_t)pCore->Allocation;
	iValues = (uintptr_t)pCore->Values;
	iGap = iValues >= iAllocation ? (size_t)(iValues - iAllocation) : SIZE_MAX;
	if (
		(pCore->Allocation == NULL) ||
		(pCore->Values == NULL) ||
		(pCore->Stride < pCore->ItemType->Size) ||
		((pCore->Stride & (pCore->ItemType->Align - 1u)) != 0u) ||
		(pCore->Capacity == 0u) ||
		(pCore->Capacity > (SIZE_MAX / pCore->Stride)) ||
		(pCore->ValueBytes != (pCore->Stride * pCore->Capacity)) ||
		(iGap > (pCore->ItemType->Align - 1u)) ||
		(iAllocation > (UINTPTR_MAX - iGap)) ||
		(iValues > (UINTPTR_MAX - pCore->ValueBytes)) ||
		(((uintptr_t)pCore->Values & (pCore->ItemType->Align - 1u)) != 0u) ||
		!__xrtRangesOverlap(pCore, sizeof(*pCore), pOwner, iOwnerSize)
	) {
		__xrtTypedQueueError(
			XERR_STATE, XTYPED_QUEUE_ERROR_STATE, sOperation,
			"the typed queue value storage layout is invalid"
		);
		return false;
	}
	return true;
}



/* 进入一次允许并发的类型值操作，并与独占生命周期操作握手。 */
bool __xrtTypedQueueCoreEnter(
	xtypedqueuecore* pCore,
	const void* pOwner,
	size_t iOwnerSize,
	cstr sOperation
)
{
	if ( !__xrtTypedQueueCoreValid(
		pCore, pOwner, iOwnerSize, sOperation
	) ) {
		return false;
	}
	xrtAtomic32FetchAdd(&pCore->Active, 1u, XMEMORY_ACQ_REL);
	{
		uint32 iState = xrtAtomic32Load(
			&pCore->State, XMEMORY_ACQUIRE
		);

		if ( iState == XRT_TYPED_QUEUE_STATE_READY ) {
			return true;
		}
		xrtAtomic32FetchSub(&pCore->Active, 1u, XMEMORY_RELEASE);
		__xrtTypedQueueError(
			XERR_STATE, XTYPED_QUEUE_ERROR_STATE, sOperation,
			iState == XRT_TYPED_QUEUE_STATE_BROKEN ?
				"the typed queue became broken while entering an operation" :
				"the typed queue entered an exclusive lifecycle operation"
		);
		return false;
	}
}



/* 退出一次允许并发的类型值操作。 */
void __xrtTypedQueueCoreLeave(xtypedqueuecore* pCore)
{
	(void)xrtAtomic32FetchSub(&pCore->Active, 1u, XMEMORY_RELEASE);
}



/* 独占核心状态并等待已进入操作离开。 */
bool __xrtTypedQueueCoreExclusive(
	xtypedqueuecore* pCore,
	bool bAllowBroken,
	uint32* pPrevious,
	cstr sOperation
)
{
	uint32 iExpected;

	if ( (pCore == NULL) || (pPrevious == NULL) ) {
		__xrtTypedQueueError(
			XERR_ARGUMENT, XTYPED_QUEUE_ERROR_ARGUMENT, sOperation,
			"the typed queue or exclusive-state output is null"
		);
		return false;
	}
	iExpected = xrtAtomic32Load(&pCore->State, XMEMORY_ACQUIRE);
	for ( ;; ) {
		if (
			(iExpected != XRT_TYPED_QUEUE_STATE_READY) &&
			(!bAllowBroken || (iExpected != XRT_TYPED_QUEUE_STATE_BROKEN))
		) {
			__xrtTypedQueueError(
				XERR_STATE, XTYPED_QUEUE_ERROR_STATE, sOperation,
				"the typed queue cannot enter an exclusive operation"
			);
			return false;
		}
		*pPrevious = iExpected;
		if ( xrtAtomic32CompareExchange(
			&pCore->State,
			&iExpected,
			XRT_TYPED_QUEUE_STATE_EXCLUSIVE,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
			break;
		}
	}
	while ( xrtAtomic32Load(&pCore->Active, XMEMORY_ACQUIRE) != 0u ) {
		xrtAtomicPause();
	}
	return true;
}



/* 结束临时独占并恢复此前的稳定状态。 */
void __xrtTypedQueueCoreShared(
	xtypedqueuecore* pCore,
	uint32 iPrevious
)
{
	xrtAtomic32Store(&pCore->State, iPrevious, XMEMORY_RELEASE);
}



/* 标记不可恢复的内部基础队列状态错误。 */
void __xrtTypedQueueCoreBreak(
	xtypedqueuecore* pCore,
	cstr sOperation,
	cstr sMessage
)
{
	xrtAtomic32Store(
		&pCore->State, XRT_TYPED_QUEUE_STATE_BROKEN, XMEMORY_RELEASE
	);
	__xrtTypedQueueWrap(
		XERR_STATE, XTYPED_QUEUE_ERROR_STATE, sOperation, sMessage
	);
}



/* 销毁每一个已初始化值槽并释放连续存储。 */
void __xrtTypedQueueCoreUnit(xtypedqueuecore* pCore)
{
	const xrttype* pItemType;

	if ( (pCore == NULL) || (pCore->ItemType == NULL) ) {
		return;
	}
	pItemType = pCore->ItemType;
	for ( size_t i = pCore->Capacity; i != 0u; i-- ) {
		xrtTypeDropValue(
			pItemType,
			pCore->Values + ((i - 1u) * pCore->Stride)
		);
	}
	xrtFree(pCore->Allocation);
	memset(pCore, 0, sizeof(*pCore));
}



/* 返回指定固定槽的地址。 */
ptr __xrtTypedQueueCell(xtypedqueuecore* pCore, size_t iIndex)
{
	return pCore->Values + (iIndex * pCore->Stride);
}



/* 验证一段值内存可被本队列安全访问。 */
static bool __xrtTypedQueueRangeValid(
	const xtypedqueuecore* pCore,
	const void* pOwner,
	size_t iOwnerSize,
	const void* pValues,
	size_t iBytes,
	cstr sOperation
)
{
	uintptr_t iValue = (uintptr_t)pValues;

	if (
		(pValues == NULL) ||
		((iValue & (pCore->ItemType->Align - 1u)) != 0u) ||
		(iValue > (UINTPTR_MAX - iBytes)) ||
		__xrtRangesOverlap(pValues, iBytes, pOwner, iOwnerSize) ||
		__xrtRangesOverlap(
			pValues, iBytes, pCore->Values, pCore->ValueBytes
		)
	) {
		__xrtTypedQueueError(
			XERR_ARGUMENT, XTYPED_QUEUE_ERROR_ARGUMENT, sOperation,
			"the value range is null, unaligned, overflowing, or internal"
		);
		return false;
	}
	return true;
}



/* 验证单值不与队列对象和内部值槽重叠。 */
bool __xrtTypedQueueValueValid(
	const xtypedqueuecore* pCore,
	const void* pOwner,
	size_t iOwnerSize,
	const void* pValue,
	cstr sOperation
)
{
	return __xrtTypedQueueRangeValid(
		pCore, pOwner, iOwnerSize, pValue, pCore->ItemType->Size,
		sOperation
	);
}



/* 验证连续值区间的尺寸、地址和外部所有权。 */
bool __xrtTypedQueueValuesValid(
	const xtypedqueuecore* pCore,
	const void* pOwner,
	size_t iOwnerSize,
	const void* pValues,
	size_t iCount,
	cstr sOperation
)
{
	if ( iCount == 0u ) {
		return true;
	}
	if ( iCount > (SIZE_MAX / pCore->ItemType->Size) ) {
		__xrtTypedQueueError(
			XERR_RANGE, XTYPED_QUEUE_ERROR_LAYOUT, sOperation,
			"the contiguous value range size overflows size_t"
		);
		return false;
	}
	return __xrtTypedQueueRangeValid(
		pCore, pOwner, iOwnerSize, pValues,
		iCount * pCore->ItemType->Size, sOperation
	);
}



/* 失败原子地复制一个外部值到空内部槽。 */
bool __xrtTypedQueueCopyIn(
	const xtypedqueuecore* pCore,
	ptr pCell,
	const void* pItem,
	cstr sOperation
)
{
	if ( !xrtTypeCopyValue(pCore->ItemType, pCell, pItem) ) {
		__xrtTypedQueueWrap(
			XERR_STATE, XTYPED_QUEUE_ERROR_OPERATION, sOperation,
			"the queue item could not be copied"
		);
		return false;
	}
	return true;
}



/* 失败原子地移动一个外部值到空内部槽。 */
bool __xrtTypedQueueMoveIn(
	const xtypedqueuecore* pCore,
	ptr pCell,
	ptr pItem,
	cstr sOperation
)
{
	if ( !xrtTypeMoveValue(pCore->ItemType, pCell, pItem) ) {
		__xrtTypedQueueWrap(
			XERR_STATE, XTYPED_QUEUE_ERROR_OPERATION, sOperation,
			"the queue item could not be moved into a value slot"
		);
		return false;
	}
	return true;
}



/* 失败原子地把内部槽移动到外部已初始化值。 */
bool __xrtTypedQueueMoveOut(
	const xtypedqueuecore* pCore,
	ptr pValue,
	ptr pCell,
	cstr sOperation
)
{
	if ( !xrtTypeMoveValue(pCore->ItemType, pValue, pCell) ) {
		__xrtTypedQueueWrap(
			XERR_STATE, XTYPED_QUEUE_ERROR_OPERATION, sOperation,
			"the queue item could not be moved to the output value"
		);
		return false;
	}
	return true;
}



/* 合并两个并发近似数量并限制在固定容量内。 */
size_t __xrtTypedQueueCount(
	const xtypedqueuecore* pCore,
	size_t iReady,
	size_t iRetry
)
{
	if ( iReady >= pCore->Capacity ) {
		return pCore->Capacity;
	}
	return iRetry < (pCore->Capacity - iReady) ?
		iReady + iRetry : pCore->Capacity;
}



/* 验证对象队列类型描述中的元素、容量和实例操作。 */
bool __xrtTypedQueueTypeValidate(
	const xrttype* pType,
	size_t iInstanceSize,
	size_t iInstanceAlign,
	const xrtinstanceops* pInstanceOps,
	cstr sOperation
)
{
	const xtypedqueuemeta* pMeta;

	if ( !xrtTypeValidate(pType) ) {
		__xrtTypedQueueWrap(
			XERR_ARGUMENT, XTYPED_QUEUE_ERROR_TYPE, sOperation,
			"the typed queue object type is invalid"
		);
		return false;
	}
	if (
		(pType->Kind != XRT_TYPE_LIST) ||
		((pType->Flags & XRT_TYPE_FLAG_REFERENCE) == 0u) ||
		(pType->ArgumentCount != 1u) ||
		(pType->Arguments == NULL) ||
		(pType->InstanceSize != iInstanceSize) ||
		(pType->InstanceAlign < iInstanceAlign) ||
		(pType->InstanceOps != pInstanceOps) ||
		(pType->Metadata == NULL)
	) {
		__xrtTypedQueueError(
			XERR_TYPE, XTYPED_QUEUE_ERROR_TYPE, sOperation,
			"the typed queue object type contract is invalid"
		);
		return false;
	}
	pMeta = (const xtypedqueuemeta*)pType->Metadata;
	if ( (pMeta->Capacity == 0u) ||
		 (pMeta->Capacity > XRT_QUEUE_MAX_CAPACITY) ) {
		__xrtTypedQueueError(
			XERR_RANGE, XTYPED_QUEUE_ERROR_LAYOUT, sOperation,
			"the typed queue object capacity is invalid"
		);
		return false;
	}
	return __xrtTypedQueueItemTypeValidate(
		pType->Arguments[0], sOperation
	);
}



/* 在独占状态下追踪全部固定值槽。 */
bool __xrtTypedQueueTrace(
	xtypedqueuecore* pCore,
	const void* pOwner,
	size_t iOwnerSize,
	xrtobjectvisitor pVisit,
	ptr pContext,
	cstr sOperation
)
{
	uint32 iPrevious;
	bool bSuccess = true;

	if ( (pVisit == NULL) ||
		 !__xrtTypedQueueCoreValid(
			pCore, pOwner, iOwnerSize, sOperation
		 ) ||
		 !__xrtTypedQueueCoreExclusive(
			pCore, false, &iPrevious, sOperation
		 ) ) {
		if ( pVisit == NULL ) {
			__xrtTypedQueueError(
				XERR_ARGUMENT, XTYPED_QUEUE_ERROR_ARGUMENT, sOperation,
				"the object visitor is null"
			);
		}
		return false;
	}
	for ( size_t i = 0u; i < pCore->Capacity; i++ ) {
		if ( !xrtTypeTraceValue(
			pCore->ItemType,
			pCore->Values + (i * pCore->Stride),
			pVisit,
			pContext
		) ) {
			bSuccess = false;
			break;
		}
	}
	__xrtTypedQueueCoreShared(pCore, iPrevious);
	if ( !bSuccess ) {
		__xrtTypedQueueWrap(
			XERR_STATE, XTYPED_QUEUE_ERROR_OPERATION, sOperation,
			"a queue value slot could not be traced"
		);
	}
	return bSuccess;
}

#endif
