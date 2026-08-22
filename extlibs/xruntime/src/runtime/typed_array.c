#include "../internal/xrt_array.h"
#include "../internal/xrt_runtime_type.h"
#include "../internal/xrt_typed_container.h"
#include <xrt/typed_array.h>



#if defined(XRUNTIME_FEATURE_TYPED_ARRAY)

#define XRT_TYPED_ARRAY_FLAG_READY 0x0001u
#define XRT_TYPED_ARRAY_FLAG_BUSY  0x0002u
#define XRT_TYPED_ARRAY_FLAGS      0x0003u



/* 设置类型数组模块结构化错误。 */
static void __xrtTypedArrayError(
	xerrkind Kind,
	xtypedarrayerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.typed-array";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为下层类型或存储错误补充类型数组上下文。 */
static void __xrtTypedArrayWrap(
	xerrkind DefaultKind,
	xtypedarrayerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.typed-array";
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



/* 验证元素类型可被连续拥有式数组安全使用。 */
static bool __xrtTypedArrayItemTypeValidate(
	const xrttype* pItemType,
	cstr sOperation
)
{
	if ( !xrtTypeValidate(pItemType) ) {
		__xrtTypedArrayWrap(XERR_ARGUMENT, XTYPED_ARRAY_ERROR_TYPE,
			sOperation, "the array item type is invalid");
		return false;
	}
	if ( pItemType->Size == 0u ) {
		__xrtTypedArrayError(XERR_TYPE, XTYPED_ARRAY_ERROR_TYPE,
			sOperation, "a typed array item must occupy storage");
		return false;
	}
	if ( !xrtTypeIsCopyable(pItemType) ) {
		__xrtTypedArrayError(XERR_UNSUPPORTED, XTYPED_ARRAY_ERROR_TYPE,
			sOperation, "the array item type is not copyable");
		return false;
	}
	if ( !xrtTypeIsRelocatable(pItemType) ) {
		__xrtTypedArrayError(XERR_UNSUPPORTED, XTYPED_ARRAY_ERROR_TYPE,
			sOperation, "the array item type is not relocatable");
		return false;
	}
	return true;
}



/* 检查公开类型数组状态与元素布局是否一致。 */
static bool __xrtTypedArrayValid(
	const xtypedarray* pArray,
	cstr sOperation
)
{
	if ( (pArray == NULL) || (pArray->ItemType == NULL) ) {
		__xrtTypedArrayError(XERR_ARGUMENT, XTYPED_ARRAY_ERROR_ARGUMENT,
			sOperation, "the typed array is null or uninitialized");
		return false;
	}
	if (
		((pArray->Flags & XRT_TYPED_ARRAY_FLAG_READY) == 0u) ||
		((pArray->Flags & XRT_TYPED_ARRAY_FLAG_BUSY) != 0u) ||
		((pArray->Flags & ~XRT_TYPED_ARRAY_FLAGS) != 0u)
	) {
		__xrtTypedArrayError(XERR_STATE, XTYPED_ARRAY_ERROR_STATE,
			sOperation, "the typed array is not available for API access");
		return false;
	}
	if ( !__xrtArrayValid(&pArray->Storage) ) {
		__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_STATE,
			sOperation, "the typed array storage is invalid");
		return false;
	}
	if ( (pArray->Storage.ItemSize != pArray->ItemType->Size) ||
		 (pArray->Storage.Alignment < pArray->ItemType->Align) ) {
		__xrtTypedArrayError(XERR_STATE, XTYPED_ARRAY_ERROR_STATE,
			sOperation, "the typed array item layout does not match its type");
		return false;
	}
	return true;
}



/* 在用户类型回调期间拒绝当前数组的全部 API 重入。 */
void __xrtTypedArrayCallbackBegin(const xtypedarray* pArray)
{
	((xtypedarray*)pArray)->Flags |= XRT_TYPED_ARRAY_FLAG_BUSY;
}



/* 结束当前数组的用户类型回调门禁。 */
void __xrtTypedArrayCallbackEnd(const xtypedarray* pArray)
{
	((xtypedarray*)pArray)->Flags &= ~XRT_TYPED_ARRAY_FLAG_BUSY;
}



/* 判断字节区间是否触及类型数组结构或完整底层分配。 */
static bool __xrtTypedArrayOwnsRange(
	const xtypedarray* pArray,
	const void* pMemory,
	size_t iSize
)
{
	size_t iAllocationSize;

	if ( __xrtRangesOverlap(
		pMemory, iSize, pArray, sizeof(*pArray)
	) ) {
		return true;
	}
	if ( pArray->Storage.Capacity == 0u ) {
		return false;
	}
	iAllocationSize = pArray->Storage.Capacity *
		pArray->Storage.ItemSize;
	if ( pArray->Storage.Alignment > XRT_ARRAY_ALIGNMENT_DEFAULT ) {
		iAllocationSize += pArray->Storage.Alignment - 1u;
	}
	return __xrtRangesOverlap(
		pMemory,
		iSize,
		pArray->Storage.Allocation,
		iAllocationSize
	);
}



/* 判断来源是否是外部值或数组内部的准确活动元素。 */
static bool __xrtTypedArraySource(
	const xtypedarray* pArray,
	const void* pItem,
	size_t* pIndex,
	bool* pInternal,
	cstr sOperation
)
{
	uintptr_t iBegin;
	uintptr_t iItem;
	size_t iOffset;

	if ( pItem == NULL ) {
		__xrtTypedArrayError(XERR_ARGUMENT, XTYPED_ARRAY_ERROR_ARGUMENT,
			sOperation, "the source item is null");
		return false;
	}
	*pInternal = false;
	if ( !__xrtTypedArrayOwnsRange(
		pArray, pItem, pArray->ItemType->Size
	) ) {
		return true;
	}
	iBegin = (uintptr_t)pArray->Storage.Data;
	iItem = (uintptr_t)pItem;
	if ( iItem >= iBegin ) {
		iOffset = (size_t)(iItem - iBegin);
		if ( ((iOffset % pArray->Storage.ItemSize) == 0u) &&
			 ((iOffset / pArray->Storage.ItemSize) < pArray->Storage.Count) ) {
			*pIndex = iOffset / pArray->Storage.ItemSize;
			*pInternal = true;
			return true;
		}
	}
	if ( pArray->Storage.Count == 0u ) {
		__xrtTypedArrayError(XERR_ARGUMENT, XTYPED_ARRAY_ERROR_ARGUMENT,
			sOperation, "the source item aliases empty array storage");
		return false;
	}
	__xrtTypedArrayError(XERR_ARGUMENT, XTYPED_ARRAY_ERROR_ARGUMENT,
		sOperation, "an internal source must be an active element boundary");
	return false;
}



/* 判断输出是否完全位于类型数组拥有的内存之外。 */
static bool __xrtTypedArrayOutputExternal(
	const xtypedarray* pArray,
	const void* pValue,
	cstr sOperation
)
{
	if ( pValue == NULL ) {
		__xrtTypedArrayError(XERR_ARGUMENT, XTYPED_ARRAY_ERROR_ARGUMENT,
			sOperation, "the output value is null");
		return false;
	}
	if ( !__xrtRangeValid(pValue, pArray->Storage.ItemSize) ) {
		__xrtTypedArrayError(XERR_ARGUMENT, XTYPED_ARRAY_ERROR_ARGUMENT,
			sOperation, "the output value range overflows");
		return false;
	}
	if ( __xrtTypedArrayOwnsRange(
		pArray, pValue, pArray->Storage.ItemSize
	) ) {
		__xrtTypedArrayError(XERR_ARGUMENT, XTYPED_ARRAY_ERROR_ARGUMENT,
			sOperation, "the output value must not alias typed array storage");
		return false;
	}
	return true;
}



/* 以指定公开操作名移动并删除一个元素。 */
static bool __xrtTypedArrayTake(
	xtypedarray* pArray,
	size_t iIndex,
	ptr pValue,
	cstr sOperation
)
{
	ptr pItem;

	if ( !__xrtTypedArrayValid(pArray, sOperation) ||
		 !__xrtTypedArrayOutputExternal(pArray, pValue, sOperation) ) {
		return false;
	}
	if ( iIndex >= pArray->Storage.Count ) {
		__xrtTypedArrayError(XERR_RANGE, XTYPED_ARRAY_ERROR_RANGE,
			sOperation, "the typed array index is out of range");
		return false;
	}
	pItem = xrtArrayGet(&pArray->Storage, iIndex);
	__xrtTypedArrayCallbackBegin(pArray);
	if ( !xrtTypeMoveValue(pArray->ItemType, pValue, pItem) ) {
		__xrtTypedArrayCallbackEnd(pArray);
		__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
			sOperation, "the typed array item could not be moved");
		return false;
	}
	if ( !xrtArrayRemove(&pArray->Storage, iIndex, 1u) ) {
		__xrtTypedArrayCallbackEnd(pArray);
		__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_STATE,
			sOperation, "the moved typed array item could not be removed");
		return false;
	}
	__xrtTypedArrayCallbackEnd(pArray);
	return true;
}



/* 释放指定活动元素区间，不改变数组结构。 */
static void __xrtTypedArrayDropRange(
	xtypedarray* pArray,
	size_t iIndex,
	size_t iCount
)
{
	while ( iCount != 0u ) {
		iCount--;
		xrtTypeDropValue(
			pArray->ItemType,
			xrtArrayGet(&pArray->Storage, iIndex + iCount)
		);
	}
}



/* 回滚追加区间，并在清理后恢复原始错误。 */
static void __xrtTypedArrayRollback(
	xtypedarray* pArray,
	size_t iOriginalCount
)
{
	xerror* pError = xrtTakeError();
	size_t iAdded = pArray->Storage.Count - iOriginalCount;

	__xrtTypedArrayCallbackBegin(pArray);
	__xrtTypedArrayDropRange(pArray, iOriginalCount, iAdded);
	(void)xrtArrayRemove(&pArray->Storage, iOriginalCount, iAdded);
	__xrtTypedArrayCallbackEnd(pArray);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 销毁临时数组，并在清理后恢复原始错误。 */
static void __xrtTypedArrayDestroyPreserveError(xtypedarray* pArray)
{
	xerror* pError = xrtTakeError();

	xrtTypedArrayDestroy(pArray);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 初始化一个拥有类型值的空连续数组。 */
XRT_API bool xrtTypedArrayInit(
	xtypedarray* pArray,
	const xrttype* pItemType
)
{
	bool bSuccess;

	if ( pArray == NULL ) {
		__xrtTypedArrayError(XERR_ARGUMENT, XTYPED_ARRAY_ERROR_ARGUMENT,
			"init", "the typed array is null");
		return false;
	}
	memset(pArray, 0, sizeof(*pArray));
	if ( !__xrtTypedArrayItemTypeValidate(pItemType, "init") ) {
		return false;
	}
	bSuccess = pItemType->Align > XRT_ARRAY_ALIGNMENT_DEFAULT ?
		xrtArrayInitAligned(
			&pArray->Storage, pItemType->Size, pItemType->Align
		) :
		xrtArrayInit(&pArray->Storage, pItemType->Size);
	if ( !bSuccess ) {
		__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
			"init", "the typed array storage could not be initialized");
		return false;
	}
	pArray->ItemType = pItemType;
	pArray->Flags = XRT_TYPED_ARRAY_FLAG_READY;
	return true;
}



/* 创建一个堆分配的空类型数组。 */
XRT_API xtypedarray* xrtTypedArrayCreate(const xrttype* pItemType)
{
	xtypedarray* pArray = (xtypedarray*)xrtMalloc(sizeof(*pArray));

	if ( pArray == NULL ) {
		return NULL;
	}
	if ( !xrtTypedArrayInit(pArray, pItemType) ) {
		xrtFree(pArray);
		return NULL;
	}
	return pArray;
}



/* 释放全部元素和存储，但不释放数组结构。 */
XRT_API void xrtTypedArrayUnit(xtypedarray* pArray)
{
	if ( pArray == NULL ) {
		return;
	}
	if ( (pArray->ItemType == NULL) && (pArray->Flags == 0u) ) {
		return;
	}
	if ( !__xrtTypedArrayValid(pArray, "unit") ) {
		return;
	}
	__xrtTypedArrayCallbackBegin(pArray);
	__xrtTypedArrayDropRange(pArray, 0u, pArray->Storage.Count);
	xrtArrayUnit(&pArray->Storage);
	memset(pArray, 0, sizeof(*pArray));
}



/* 释放类型数组持有的全部资源和结构。 */
XRT_API void xrtTypedArrayDestroy(xtypedarray* pArray)
{
	if ( pArray == NULL ) {
		return;
	}
	if ( !__xrtTypedArrayValid(pArray, "destroy") ) {
		return;
	}
	xrtTypedArrayUnit(pArray);
	xrtFree(pArray);
}



/* 返回数组借用的元素类型。 */
XRT_API const xrttype* xrtTypedArrayItemType(const xtypedarray* pArray)
{
	return __xrtTypedArrayValid(pArray, "item-type") ?
		pArray->ItemType : NULL;
}



/* 返回当前元素数。 */
XRT_API size_t xrtTypedArrayCount(const xtypedarray* pArray)
{
	return __xrtTypedArrayValid(pArray, "count") ?
		pArray->Storage.Count : 0u;
}



/* 返回当前元素容量。 */
XRT_API size_t xrtTypedArrayCapacity(const xtypedarray* pArray)
{
	return __xrtTypedArrayValid(pArray, "capacity") ?
		pArray->Storage.Capacity : 0u;
}



/* 返回活动元素连续区的可写借用。 */
XRT_API ptr xrtTypedArrayData(xtypedarray* pArray)
{
	return __xrtTypedArrayValid(pArray, "data") ?
		pArray->Storage.Data : NULL;
}



/* 返回活动元素连续区的只读借用。 */
XRT_API const void* xrtTypedArrayConstData(const xtypedarray* pArray)
{
	return __xrtTypedArrayValid(pArray, "const-data") ?
		pArray->Storage.Data : NULL;
}



/* 保证数组至少具有指定元素容量。 */
XRT_API bool xrtTypedArrayReserve(xtypedarray* pArray, size_t iCapacity)
{
	if ( !__xrtTypedArrayValid(pArray, "reserve") ) {
		return false;
	}
	if ( !xrtArrayReserve(&pArray->Storage, iCapacity) ) {
		__xrtTypedArrayWrap(XERR_MEMORY, XTYPED_ARRAY_ERROR_OPERATION,
			"reserve", "the typed array capacity could not be reserved");
		return false;
	}
	return true;
}



/* 调整元素数量，增长部分逐项初始化，失败时恢复原数量。 */
XRT_API bool xrtTypedArrayResize(xtypedarray* pArray, size_t iCount)
{
	size_t iOriginalCount;
	size_t iAdded;

	if ( !__xrtTypedArrayValid(pArray, "resize") ) {
		return false;
	}
	iOriginalCount = pArray->Storage.Count;
	if ( iCount < iOriginalCount ) {
		bool bRemoved;

		__xrtTypedArrayCallbackBegin(pArray);
		__xrtTypedArrayDropRange(
			pArray, iCount, iOriginalCount - iCount
		);
		bRemoved = xrtArrayRemove(
			&pArray->Storage, iCount, iOriginalCount - iCount
		);
		__xrtTypedArrayCallbackEnd(pArray);
		return bRemoved;
	}
	if ( iCount == iOriginalCount ) {
		return true;
	}
	if ( !xrtTypedArrayReserve(pArray, iCount) ) {
		return false;
	}
	iAdded = iCount - iOriginalCount;
	if ( xrtArrayAdd(&pArray->Storage, iAdded) == NULL ) {
		__xrtTypedArrayWrap(XERR_MEMORY, XTYPED_ARRAY_ERROR_OPERATION,
			"resize", "the typed array growth failed");
		return false;
	}
	__xrtTypedArrayCallbackBegin(pArray);
	for ( size_t i = 0; i < iAdded; i++ ) {
		ptr pItem = xrtArrayGet(&pArray->Storage, iOriginalCount + i);

		if ( !xrtTypeInitValue(pArray->ItemType, pItem) ) {
			xerror* pError = xrtTakeError();

			for ( size_t j = 0; j < i; j++ ) {
				xrtTypeDropValue(pArray->ItemType,
					xrtArrayGet(&pArray->Storage, iOriginalCount + j));
			}
			(void)xrtArrayRemove(&pArray->Storage, iOriginalCount, iAdded);
			__xrtTypedArrayCallbackEnd(pArray);
			if ( pError != NULL ) {
				xrtSetError(pError);
				xrtErrorFree(pError);
			}
			__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
				"resize", "a new typed array item could not be initialized");
			return false;
		}
	}
	__xrtTypedArrayCallbackEnd(pArray);
	return true;
}



/* 把容量裁剪到当前元素数量。 */
XRT_API bool xrtTypedArrayTrim(xtypedarray* pArray)
{
	if ( !__xrtTypedArrayValid(pArray, "trim") ) {
		return false;
	}
	if ( !xrtArrayTrim(&pArray->Storage) ) {
		__xrtTypedArrayWrap(XERR_MEMORY, XTYPED_ARRAY_ERROR_OPERATION,
			"trim", "the typed array capacity could not be trimmed");
		return false;
	}
	return true;
}



/* 清空全部元素并保留存储容量。 */
XRT_API void xrtTypedArrayClear(xtypedarray* pArray)
{
	if ( !__xrtTypedArrayValid(pArray, "clear") ) {
		return;
	}
	__xrtTypedArrayCallbackBegin(pArray);
	__xrtTypedArrayDropRange(pArray, 0u, pArray->Storage.Count);
	xrtArrayClear(&pArray->Storage);
	__xrtTypedArrayCallbackEnd(pArray);
}



/* 返回指定下标的可写借用元素。 */
XRT_API ptr xrtTypedArrayGet(xtypedarray* pArray, size_t iIndex)
{
	if ( !__xrtTypedArrayValid(pArray, "get") ) {
		return NULL;
	}
	if ( iIndex >= pArray->Storage.Count ) {
		__xrtTypedArrayError(XERR_RANGE, XTYPED_ARRAY_ERROR_RANGE,
			"get", "the typed array index is out of range");
		return NULL;
	}
	return xrtArrayGet(&pArray->Storage, iIndex);
}



/* 返回指定下标的只读借用元素。 */
XRT_API const void* xrtTypedArrayConstGet(
	const xtypedarray* pArray,
	size_t iIndex
)
{
	if ( !__xrtTypedArrayValid(pArray, "const-get") ) {
		return NULL;
	}
	if ( iIndex >= pArray->Storage.Count ) {
		__xrtTypedArrayError(XERR_RANGE, XTYPED_ARRAY_ERROR_RANGE,
			"const-get", "the typed array index is out of range");
		return NULL;
	}
	return xrtArrayConstGet(&pArray->Storage, iIndex);
}



/* 在指定位置复制插入一个值，并处理数组内部来源。 */
static bool __xrtTypedArrayInsert(
	xtypedarray* pArray,
	size_t iIndex,
	const void* pItem,
	cstr sOperation
)
{
	size_t iSourceIndex = 0u;
	bool bInternal;
	ptr pSlot;

	if ( !__xrtTypedArrayValid(pArray, sOperation) ||
		 !__xrtTypedArraySource(
			pArray, pItem, &iSourceIndex, &bInternal, sOperation
		) ) {
		return false;
	}
	if ( iIndex > pArray->Storage.Count ) {
		__xrtTypedArrayError(XERR_RANGE, XTYPED_ARRAY_ERROR_RANGE,
			sOperation, "the typed array insertion index is out of range");
		return false;
	}
	if ( pArray->Storage.Count == SIZE_MAX ) {
		__xrtTypedArrayError(XERR_RANGE, XTYPED_ARRAY_ERROR_RANGE,
			sOperation, "the typed array element count overflows");
		return false;
	}
	if ( !xrtArrayReserve(&pArray->Storage, pArray->Storage.Count + 1u) ) {
		__xrtTypedArrayWrap(XERR_MEMORY, XTYPED_ARRAY_ERROR_OPERATION,
			sOperation, "the typed array insertion could not reserve storage");
		return false;
	}
	if ( bInternal ) {
		pItem = xrtArrayConstGet(&pArray->Storage, iSourceIndex);
	}
	pSlot = xrtArrayInsertSpace(&pArray->Storage, iIndex, 1u);
	if ( pSlot == NULL ) {
		__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
			sOperation, "the typed array insertion failed");
		return false;
	}
	if ( bInternal ) {
		size_t iMovedIndex = iSourceIndex >= iIndex ?
			iSourceIndex + 1u : iSourceIndex;

		pItem = xrtArrayConstGet(&pArray->Storage, iMovedIndex);
	}
	__xrtTypedArrayCallbackBegin(pArray);
	if ( !xrtTypeInitValue(pArray->ItemType, pSlot) ) {
		xerror* pError = xrtTakeError();

		(void)xrtArrayRemove(&pArray->Storage, iIndex, 1u);
		__xrtTypedArrayCallbackEnd(pArray);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
			sOperation, "the inserted array item could not be initialized");
		return false;
	}
	if ( !xrtTypeCopyValue(pArray->ItemType, pSlot, pItem) ) {
		xerror* pError = xrtTakeError();

		xrtTypeDropValue(pArray->ItemType, pSlot);
		(void)xrtArrayRemove(&pArray->Storage, iIndex, 1u);
		__xrtTypedArrayCallbackEnd(pArray);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
			sOperation, "the inserted array item could not be copied");
		return false;
	}
	__xrtTypedArrayCallbackEnd(pArray);
	return true;
}



/* 复制追加一个值。 */
XRT_API bool xrtTypedArrayPush(xtypedarray* pArray, const void* pItem)
{
	return __xrtTypedArrayInsert(
		pArray,
		pArray != NULL ? pArray->Storage.Count : 0u,
		pItem,
		"push"
	);
}



/* 在指定下标复制插入一个值。 */
XRT_API bool xrtTypedArrayInsert(
	xtypedarray* pArray,
	size_t iIndex,
	const void* pItem
)
{
	return __xrtTypedArrayInsert(pArray, iIndex, pItem, "insert");
}



/* 失败原子地替换指定元素值。 */
XRT_API bool xrtTypedArraySet(
	xtypedarray* pArray,
	size_t iIndex,
	const void* pItem
)
{
	ptr pTarget;
	size_t iSourceIndex;
	bool bInternal;

	if ( !__xrtTypedArrayValid(pArray, "set") ||
		 !__xrtTypedArraySource(
			pArray, pItem, &iSourceIndex, &bInternal, "set"
		) ) {
		return false;
	}
	if ( iIndex >= pArray->Storage.Count ) {
		__xrtTypedArrayError(XERR_RANGE, XTYPED_ARRAY_ERROR_RANGE,
			"set", "the typed array index is out of range");
		return false;
	}
	pTarget = xrtArrayGet(&pArray->Storage, iIndex);
	__xrtTypedArrayCallbackBegin(pArray);
	if ( !xrtTypeCopyValue(pArray->ItemType, pTarget, pItem) ) {
		__xrtTypedArrayCallbackEnd(pArray);
		__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
			"set", "the typed array item could not be replaced");
		return false;
	}
	__xrtTypedArrayCallbackEnd(pArray);
	return true;
}



/* 销毁并删除精确元素区间。 */
XRT_API bool xrtTypedArrayRemove(
	xtypedarray* pArray,
	size_t iIndex,
	size_t iCount
)
{
	if ( !__xrtTypedArrayValid(pArray, "remove") ) {
		return false;
	}
	if ( (iIndex > pArray->Storage.Count) ||
		 (iCount > (pArray->Storage.Count - iIndex)) ) {
		__xrtTypedArrayError(XERR_RANGE, XTYPED_ARRAY_ERROR_RANGE,
			"remove", "the typed array removal range is invalid");
		return false;
	}
	if ( iCount == 0u ) {
		return true;
	}
	__xrtTypedArrayCallbackBegin(pArray);
	__xrtTypedArrayDropRange(pArray, iIndex, iCount);
	if ( !xrtArrayRemove(&pArray->Storage, iIndex, iCount) ) {
		__xrtTypedArrayCallbackEnd(pArray);
		__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_STATE,
			"remove", "the dropped typed array range could not be removed");
		return false;
	}
	__xrtTypedArrayCallbackEnd(pArray);
	return true;
}



/* 把指定元素移动到外部已初始化值后删除。 */
XRT_API bool xrtTypedArrayTake(
	xtypedarray* pArray,
	size_t iIndex,
	ptr pValue
)
{
	return __xrtTypedArrayTake(pArray, iIndex, pValue, "take");
}



/* 把末尾元素移动到外部已初始化值后删除。 */
XRT_API bool xrtTypedArrayPop(xtypedarray* pArray, ptr pValue)
{
	if ( !__xrtTypedArrayValid(pArray, "pop") ) {
		return false;
	}
	if ( pArray->Storage.Count == 0u ) {
		__xrtTypedArrayError(XERR_RANGE, XTYPED_ARRAY_ERROR_RANGE,
			"pop", "the typed array is empty");
		return false;
	}
	return __xrtTypedArrayTake(
		pArray, pArray->Storage.Count - 1u, pValue, "pop"
	);
}



/* 交换两个元素的字节位置。 */
XRT_API bool xrtTypedArraySwap(
	xtypedarray* pArray,
	size_t iLeft,
	size_t iRight
)
{
	if ( !__xrtTypedArrayValid(pArray, "swap") ) {
		return false;
	}
	if ( !xrtArraySwap(&pArray->Storage, iLeft, iRight) ) {
		__xrtTypedArrayWrap(XERR_RANGE, XTYPED_ARRAY_ERROR_RANGE,
			"swap", "the typed array swap indices are invalid");
		return false;
	}
	return true;
}



/* 原地反转元素顺序。 */
XRT_API bool xrtTypedArrayReverse(xtypedarray* pArray)
{
	if ( !__xrtTypedArrayValid(pArray, "reverse") ) {
		return false;
	}
	if ( !xrtArrayReverse(&pArray->Storage) ) {
		__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
			"reverse", "the typed array could not be reversed");
		return false;
	}
	return true;
}



/* 使用类型比较操作查找第一个相等元素。 */
XRT_API size_t xrtTypedArrayFind(
	const xtypedarray* pArray,
	const void* pItem
)
{
	size_t iSourceIndex;
	bool bInternal;

	if ( !__xrtTypedArrayValid(pArray, "find") ||
		 !__xrtTypedArraySource(
			pArray, pItem, &iSourceIndex, &bInternal, "find"
		) ) {
		return SIZE_MAX;
	}
	if ( !xrtTypeIsComparable(pArray->ItemType) ) {
		__xrtTypedArrayError(XERR_UNSUPPORTED, XTYPED_ARRAY_ERROR_TYPE,
			"find", "the array item type is not comparable");
		return SIZE_MAX;
	}
	__xrtTypedArrayCallbackBegin(pArray);
	for ( size_t i = 0; i < pArray->Storage.Count; i++ ) {
		int iCompare;

		if ( !xrtTypeCompareValue(
			pArray->ItemType,
			xrtArrayConstGet(&pArray->Storage, i),
			pItem,
			&iCompare
		) ) {
			__xrtTypedArrayCallbackEnd(pArray);
			__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
				"find", "typed array item comparison failed");
			return SIZE_MAX;
		}
		if ( iCompare == 0 ) {
			__xrtTypedArrayCallbackEnd(pArray);
			return i;
		}
	}
	__xrtTypedArrayCallbackEnd(pArray);
	return SIZE_MAX;
}



/* 判断数组是否包含相等元素。 */
XRT_API bool xrtTypedArrayContains(
	const xtypedarray* pArray,
	const void* pItem
)
{
	return xrtTypedArrayFind(pArray, pItem) != SIZE_MAX;
}



/* 事务追加另一个同类型数组，允许自追加。 */
XRT_API bool xrtTypedArrayAppend(
	xtypedarray* pTarget,
	const xtypedarray* pSource
)
{
	size_t iOriginalCount;
	size_t iSourceCount;

	if ( !__xrtTypedArrayValid(pTarget, "append") ||
		 !__xrtTypedArrayValid(pSource, "append") ) {
		return false;
	}
	if ( !xrtTypeSame(pTarget->ItemType, pSource->ItemType) ) {
		__xrtTypedArrayError(XERR_TYPE, XTYPED_ARRAY_ERROR_TYPE,
			"append", "typed arrays have different item types");
		return false;
	}
	iOriginalCount = pTarget->Storage.Count;
	iSourceCount = pSource->Storage.Count;
	if ( iSourceCount > (SIZE_MAX - iOriginalCount) ) {
		__xrtTypedArrayError(XERR_RANGE, XTYPED_ARRAY_ERROR_RANGE,
			"append", "the typed array element count overflows");
		return false;
	}
	if ( !xrtTypedArrayReserve(pTarget, iOriginalCount + iSourceCount) ) {
		return false;
	}
	if ( pTarget != pSource ) {
		__xrtTypedArrayCallbackBegin(pSource);
	}
	for ( size_t i = 0; i < iSourceCount; i++ ) {
		const void* pItem = xrtArrayConstGet(&pSource->Storage, i);

		if ( !xrtTypedArrayPush(pTarget, pItem) ) {
			__xrtTypedArrayRollback(pTarget, iOriginalCount);
			if ( pTarget != pSource ) {
				__xrtTypedArrayCallbackEnd(pSource);
			}
			__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
				"append", "a typed array item could not be appended");
			return false;
		}
	}
	if ( pTarget != pSource ) {
		__xrtTypedArrayCallbackEnd(pSource);
	}
	return true;
}



/* 深复制一个独立类型数组。 */
XRT_API xtypedarray* xrtTypedArrayClone(const xtypedarray* pArray)
{
	xtypedarray* pClone;

	if ( !__xrtTypedArrayValid(pArray, "clone") ) {
		return NULL;
	}
	pClone = xrtTypedArrayCreate(pArray->ItemType);
	if ( pClone == NULL ) {
		return NULL;
	}
	if ( !xrtTypedArrayAppend(pClone, pArray) ) {
		__xrtTypedArrayDestroyPreserveError(pClone);
		return NULL;
	}
	return pClone;
}



/* 深复制并拼接两个同类型数组。 */
XRT_API xtypedarray* xrtTypedArrayConcat(
	const xtypedarray* pLeft,
	const xtypedarray* pRight
)
{
	xtypedarray* pResult;

	if ( !__xrtTypedArrayValid(pLeft, "concat") ||
		 !__xrtTypedArrayValid(pRight, "concat") ) {
		return NULL;
	}
	if ( !xrtTypeSame(pLeft->ItemType, pRight->ItemType) ) {
		__xrtTypedArrayError(XERR_TYPE, XTYPED_ARRAY_ERROR_TYPE,
			"concat", "typed arrays have different item types");
		return NULL;
	}
	pResult = xrtTypedArrayClone(pLeft);
	if ( pResult == NULL ) {
		__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
			"concat", "the left typed array could not be cloned");
		return NULL;
	}
	if ( !xrtTypedArrayAppend(pResult, pRight) ) {
		__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
			"concat", "the right typed array could not be appended");
		__xrtTypedArrayDestroyPreserveError(pResult);
		return NULL;
	}
	return pResult;
}



/* 比较两个数组的类型、数量和有序元素内容。 */
XRT_API bool xrtTypedArrayEquals(
	const xtypedarray* pLeft,
	const xtypedarray* pRight
)
{
	int iCompare;

	if ( !__xrtTypedArrayValid(pLeft, "equals") ||
		 !__xrtTypedArrayValid(pRight, "equals") ) {
		return false;
	}
	if ( pLeft == pRight ) {
		return true;
	}
	if ( !xrtTypeSame(pLeft->ItemType, pRight->ItemType) ||
		 (pLeft->Storage.Count != pRight->Storage.Count) ) {
		return false;
	}
	if ( !xrtTypeIsComparable(pLeft->ItemType) ) {
		__xrtTypedArrayError(XERR_UNSUPPORTED, XTYPED_ARRAY_ERROR_TYPE,
			"equals", "the array item type is not comparable");
		return false;
	}
	__xrtTypedArrayCallbackBegin(pLeft);
	__xrtTypedArrayCallbackBegin(pRight);
	for ( size_t i = 0; i < pLeft->Storage.Count; i++ ) {
		if ( !xrtTypeCompareValue(
			pLeft->ItemType,
			xrtArrayConstGet(&pLeft->Storage, i),
			xrtArrayConstGet(&pRight->Storage, i),
			&iCompare
		) ) {
			__xrtTypedArrayCallbackEnd(pRight);
			__xrtTypedArrayCallbackEnd(pLeft);
			__xrtTypedArrayWrap(XERR_STATE, XTYPED_ARRAY_ERROR_OPERATION,
				"equals", "typed array item comparison failed");
			return false;
		}
		if ( iCompare != 0 ) {
			__xrtTypedArrayCallbackEnd(pRight);
			__xrtTypedArrayCallbackEnd(pLeft);
			return false;
		}
	}
	__xrtTypedArrayCallbackEnd(pRight);
	__xrtTypedArrayCallbackEnd(pLeft);
	return true;
}



/* 按数组类型实参初始化对象负载。 */
static bool __xrtTypedArrayInstanceInit(
	ptr pInstance,
	const xrttype* pType
)
{
	if ( !xrtTypedArrayTypeValidate(pType) ) {
		return false;
	}
	return xrtTypedArrayInit(
		(xtypedarray*)pInstance, pType->Arguments[0]
	);
}



/* 销毁对象负载中的类型数组。 */
static void __xrtTypedArrayInstanceDrop(
	ptr pInstance,
	const xrttype* pType
)
{
	(void)pType;
	xrtTypedArrayUnit((xtypedarray*)pInstance);
}



/* 枚举类型数组所有元素值直接拥有的强对象引用。 */
static bool __xrtTypedArrayInstanceTrace(
	const void* pInstance,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	const xtypedarray* pArray = (const xtypedarray*)pInstance;
	(void)pType;

	if ( !__xrtTypedArrayValid(pArray, "instance-trace") ) {
		return false;
	}
	__xrtTypedArrayCallbackBegin(pArray);
	for ( size_t i = 0; i < pArray->Storage.Count; i++ ) {
		if ( !xrtTypeTraceValue(
			pArray->ItemType,
			xrtArrayConstGet(&pArray->Storage, i),
			pVisit,
			pContext
		) ) {
			__xrtTypedArrayCallbackEnd(pArray);
			return false;
		}
	}
	__xrtTypedArrayCallbackEnd(pArray);
	return true;
}



/* 返回对象数组负载共享的实例操作表。 */
XRT_API const xrtinstanceops* xrtTypedArrayInstanceOps(void)
{
	static const xrtinstanceops Ops = {
		.Init = __xrtTypedArrayInstanceInit,
		.Drop = __xrtTypedArrayInstanceDrop,
		.Trace = __xrtTypedArrayInstanceTrace
	};

	return &Ops;
}



/* 验证可由对象系统承载的泛型数组类型描述。 */
XRT_API bool xrtTypedArrayTypeValidate(const xrttype* pType)
{
	if ( !xrtTypeValidate(pType) ) {
		__xrtTypedArrayWrap(XERR_ARGUMENT, XTYPED_ARRAY_ERROR_TYPE,
			"type-validate", "the typed array object type is invalid");
		return false;
	}
	if (
		(pType->Kind != XRT_TYPE_ARRAY) ||
		((pType->Flags & XRT_TYPE_FLAG_REFERENCE) == 0u) ||
		(pType->ArgumentCount != 1u) ||
		(pType->Arguments == NULL) ||
		(pType->InstanceSize != sizeof(xtypedarray)) ||
		(pType->InstanceAlign <
		 XRT_INTERNAL_OBJECT_ALIGNOF(xtypedarray)) ||
		(pType->InstanceOps != xrtTypedArrayInstanceOps())
	) {
		__xrtTypedArrayError(XERR_TYPE, XTYPED_ARRAY_ERROR_TYPE,
			"type-validate", "the typed array object type contract is invalid");
		return false;
	}
	return __xrtTypedArrayItemTypeValidate(
		pType->Arguments[0], "type-validate"
	);
}

#endif
