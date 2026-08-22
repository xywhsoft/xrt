#include "../internal/xrt_runtime_type.h"
#include "../internal/xrt_set.h"
#include "../internal/xrt_typed_container.h"
#include <xrt/typed_set.h>



#if defined(XRUNTIME_FEATURE_TYPED_SET)

/* 在跨模块用户回调期间拒绝当前类型集合的全部 API 重入。 */
bool __xrtTypedSetCallbackBegin(const xtypedset* pSet)
{
	return (pSet != NULL) && __xrtSetCallbackBegin(&pSet->Storage);
}



/* 结束当前类型集合的跨模块用户回调门禁。 */
void __xrtTypedSetCallbackEnd(const xtypedset* pSet)
{
	if ( pSet != NULL ) {
		__xrtSetCallbackEnd(&pSet->Storage);
	}
}




/* 设置类型集合模块结构化错误。 */
static void __xrtTypedSetError(
	xerrkind Kind,
	xtypedseterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.typed-set";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为下层类型或集合错误补充类型集合上下文。 */
static void __xrtTypedSetWrap(
	xerrkind DefaultKind,
	xtypedseterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.typed-set";
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



/* 验证元素类型满足唯一值集合的生命周期和键规则。 */
static bool __xrtTypedSetItemTypeValidate(
	const xrttype* pItemType,
	cstr sOperation
)
{
	if ( !xrtTypeValidate(pItemType) ) {
		__xrtTypedSetWrap(XERR_ARGUMENT, XTYPED_SET_ERROR_TYPE,
			sOperation, "the set item type is invalid");
		return false;
	}
	if ( pItemType->Size == 0u ) {
		__xrtTypedSetError(XERR_TYPE, XTYPED_SET_ERROR_TYPE,
			sOperation, "a typed set item must occupy storage");
		return false;
	}
	if ( !xrtTypeIsCopyable(pItemType) ) {
		__xrtTypedSetError(XERR_UNSUPPORTED, XTYPED_SET_ERROR_TYPE,
			sOperation, "the set item type is not copyable");
		return false;
	}
	if ( !xrtTypeIsComparable(pItemType) ) {
		__xrtTypedSetError(XERR_UNSUPPORTED, XTYPED_SET_ERROR_TYPE,
			sOperation, "the set item type is not comparable");
		return false;
	}
	if ( !xrtTypeIsHashable(pItemType) ) {
		__xrtTypedSetError(XERR_UNSUPPORTED, XTYPED_SET_ERROR_TYPE,
			sOperation, "the set item type is not hashable");
		return false;
	}
	return true;
}



/* 按已验证类型的散列操作计算集合键散列。 */
static uint64 __xrtTypedSetHash(const void* pItem, ptr pUserData)
{
	const xrttype* pItemType = (const xrttype*)pUserData;
	uint64 iHash = 0u;

	(void)xrtTypeHashValue(pItemType, pItem, &iHash);
	return iHash;
}



/* 按已验证类型的比较操作判断两个集合键是否相等。 */
static bool __xrtTypedSetEqual(
	const void* pLeft,
	const void* pRight,
	ptr pUserData
)
{
	const xrttype* pItemType = (const xrttype*)pUserData;
	int iCompare = 0;

	return xrtTypeCompareValue(
		pItemType, pLeft, pRight, &iCompare
	) && (iCompare == 0);
}



/* 初始化并复制一个尚未提交的集合值。 */
static bool __xrtTypedSetCopy(
	ptr pTarget,
	const void* pSource,
	ptr pUserData
)
{
	const xrttype* pItemType = (const xrttype*)pUserData;
	xerror* pError;

	if ( !xrtTypeInitValue(pItemType, pTarget) ) {
		return false;
	}
	if ( xrtTypeCopyValue(pItemType, pTarget, pSource) ) {
		return true;
	}
	pError = xrtTakeError();
	xrtTypeDropValue(pItemType, pTarget);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return false;
}



/* 销毁集合拥有的一个完整初始化类型值。 */
static void __xrtTypedSetDrop(ptr pItem, ptr pUserData)
{
	const xrttype* pItemType = (const xrttype*)pUserData;

	xrtTypeDropValue(pItemType, pItem);
}



/* 把规范集合值移动到调用方已经初始化的外部值。 */
static bool __xrtTypedSetMove(
	ptr pTarget,
	ptr pSource,
	ptr pUserData
)
{
	return xrtTypeMoveValue(
		(const xrttype*)pUserData, pTarget, pSource
	);
}



/* 检查公开类型集合状态、布局和底层策略是否一致。 */
static bool __xrtTypedSetValid(
	const xtypedset* pSet,
	cstr sOperation
)
{
	if ( (pSet == NULL) || (pSet->ItemType == NULL) ) {
		__xrtTypedSetError(XERR_ARGUMENT, XTYPED_SET_ERROR_ARGUMENT,
			sOperation, "the typed set is null or uninitialized");
		return false;
	}
	if ( !__xrtSetValid(&pSet->Storage) ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_STATE,
			sOperation, "the typed set storage is invalid");
		return false;
	}
	if (
		(pSet->Storage.ItemSize != pSet->ItemType->Size) ||
		(pSet->Storage.Alignment < pSet->ItemType->Align) ||
		(pSet->Storage.Hash != __xrtTypedSetHash) ||
		(pSet->Storage.Equal != __xrtTypedSetEqual) ||
		(pSet->Storage.Copy != __xrtTypedSetCopy) ||
		(pSet->Storage.Drop != __xrtTypedSetDrop) ||
		(pSet->Storage.KeyUserData != pSet->ItemType) ||
		(pSet->Storage.LifecycleUserData != pSet->ItemType)
	) {
		__xrtTypedSetError(XERR_STATE, XTYPED_SET_ERROR_STATE,
			sOperation, "the typed set layout or type policies are invalid");
		return false;
	}
	return true;
}



/* 检查类型集合当前是否允许读取和推进迭代器。 */
static bool __xrtTypedSetCanRead(
	const xtypedset* pSet,
	cstr sOperation
)
{
	if ( !__xrtTypedSetValid(pSet, sOperation) ) {
		return false;
	}
	if ( !__xrtSetCanRead(&pSet->Storage) ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_STATE,
			sOperation, "the typed set is executing an item callback");
		return false;
	}
	return true;
}



/* 检查类型集合当前是否允许结构和生命周期修改。 */
static bool __xrtTypedSetCanMutate(
	xtypedset* pSet,
	cstr sOperation
)
{
	if ( !__xrtTypedSetValid(pSet, sOperation) ) {
		return false;
	}
	if ( !__xrtSetCanMutate(&pSet->Storage) ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_STATE,
			sOperation, "the typed set is currently being visited");
		return false;
	}
	return true;
}



/* 判断字节区间是否触及类型集合自身或拥有的底层存储。 */
static bool __xrtTypedSetOwnsRange(
	const xtypedset* pSet,
	const void* pMemory,
	size_t iSize
)
{
	return __xrtRangesOverlap(pMemory, iSize, pSet, sizeof(*pSet)) ||
		__xrtSetOwnsRange(&pSet->Storage, pMemory, iSize);
}



/* 验证来源是外部值或集合中的准确规范值槽。 */
static bool __xrtTypedSetSourceValid(
	const xtypedset* pSet,
	const void* pItem,
	cstr sOperation
)
{
	xsetiter Iterator;
	const void* pStored;
	bool bExact = false;

	if ( pItem == NULL ) {
		__xrtTypedSetError(XERR_ARGUMENT, XTYPED_SET_ERROR_ARGUMENT,
			sOperation, "the source item is null");
		return false;
	}
	if ( !__xrtTypedSetOwnsRange(
		pSet, pItem, pSet->ItemType->Size
	) ) {
		return true;
	}
	if ( !xrtSetIterBegin((xset*)&pSet->Storage, &Iterator) ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_STATE,
			sOperation, "the typed set source scan could not start");
		return false;
	}
	while ( (pStored = xrtSetIterNext(&Iterator)) != NULL ) {
		if ( pStored == pItem ) {
			bExact = true;
			break;
		}
	}
	xrtSetIterEnd(&Iterator);
	if ( !bExact ) {
		__xrtTypedSetError(XERR_ARGUMENT, XTYPED_SET_ERROR_ARGUMENT,
			sOperation, "an internal source must be a canonical item boundary");
		return false;
	}
	return true;
}



/* 验证移动输出完全位于类型集合拥有的内存之外。 */
static bool __xrtTypedSetOutputExternal(
	const xtypedset* pSet,
	const void* pValue,
	cstr sOperation
)
{
	if ( pValue == NULL ) {
		__xrtTypedSetError(XERR_ARGUMENT, XTYPED_SET_ERROR_ARGUMENT,
			sOperation, "the output value is null");
		return false;
	}
	if ( __xrtTypedSetOwnsRange(
		pSet, pValue, pSet->ItemType->Size
	) ) {
		__xrtTypedSetError(XERR_ARGUMENT, XTYPED_SET_ERROR_ARGUMENT,
			sOperation, "the output value must not alias typed set storage");
		return false;
	}
	return true;
}



/* 验证两个集合借用完全相同的类型描述和生命周期 ABI。 */
static bool __xrtTypedSetSameType(
	const xtypedset* pLeft,
	const xtypedset* pRight,
	cstr sOperation
)
{
	if ( pLeft->ItemType != pRight->ItemType ) {
		__xrtTypedSetError(XERR_TYPE, XTYPED_SET_ERROR_TYPE,
			sOperation, "typed set operands must share one item type descriptor");
		return false;
	}
	return true;
}



/* 在清理底层临时集合期间保留原始失败。 */
static void __xrtTypedSetDestroyRaw(xset* pStorage)
{
	xerror* pError = xrtTakeError();

	xrtSetDestroy(pStorage);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 把底层集合结果包装为不再复制元素的类型集合。 */
static xtypedset* __xrtTypedSetFromRaw(
	const xrttype* pItemType,
	xset* pStorage,
	cstr sOperation
)
{
	xtypedset* pResult;

	if ( pStorage == NULL ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_OPERATION,
			sOperation, "the typed set operation could not build its result");
		return NULL;
	}
	pResult = (xtypedset*)xrtMalloc(sizeof(*pResult));
	if ( pResult == NULL ) {
		__xrtTypedSetDestroyRaw(pStorage);
		__xrtTypedSetWrap(XERR_MEMORY, XTYPED_SET_ERROR_OPERATION,
			sOperation, "the typed set result could not be allocated");
		return NULL;
	}
	if ( !xrtTypedSetInit(pResult, pItemType) ) {
		xrtFree(pResult);
		__xrtTypedSetDestroyRaw(pStorage);
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_OPERATION,
			sOperation, "the typed set result could not be initialized");
		return NULL;
	}
	if ( !__xrtSetAdoptHeap(&pResult->Storage, pStorage) ) {
		xrtTypedSetUnit(pResult);
		xrtFree(pResult);
		__xrtTypedSetDestroyRaw(pStorage);
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_OPERATION,
			sOperation, "the typed set result storage could not be adopted");
		return NULL;
	}
	return pResult;
}



/* 初始化一个拥有类型值的空集合。 */
XRT_API bool xrtTypedSetInit(
	xtypedset* pSet,
	const xrttype* pItemType
)
{
	size_t iAlignment;

	if ( pSet == NULL ) {
		__xrtTypedSetError(XERR_ARGUMENT, XTYPED_SET_ERROR_ARGUMENT,
			"init", "the typed set is null");
		return false;
	}
	memset(pSet, 0, sizeof(*pSet));
	if ( !__xrtTypedSetItemTypeValidate(pItemType, "init") ) {
		return false;
	}
	iAlignment = pItemType->Align > XRT_SET_ALIGNMENT_DEFAULT ?
		pItemType->Align : XRT_SET_ALIGNMENT_DEFAULT;
	if ( !xrtSetInitAligned(
		&pSet->Storage, pItemType->Size, iAlignment
	) ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_OPERATION,
			"init", "the typed set storage could not be initialized");
		return false;
	}
	pSet->ItemType = pItemType;
	if ( !xrtSetSetKeyPolicy(
		&pSet->Storage,
		__xrtTypedSetHash,
		__xrtTypedSetEqual,
		(ptr)pItemType
	) || !xrtSetSetLifecycle(
		&pSet->Storage,
		__xrtTypedSetCopy,
		__xrtTypedSetDrop,
		(ptr)pItemType
	) ) {
		xrtSetUnit(&pSet->Storage);
		memset(pSet, 0, sizeof(*pSet));
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_OPERATION,
			"init", "the typed set type policies could not be installed");
		return false;
	}
	return true;
}



/* 创建一个堆分配的空类型集合。 */
XRT_API xtypedset* xrtTypedSetCreate(const xrttype* pItemType)
{
	xtypedset* pSet = (xtypedset*)xrtMalloc(sizeof(*pSet));

	if ( pSet == NULL ) {
		return NULL;
	}
	if ( !xrtTypedSetInit(pSet, pItemType) ) {
		xrtFree(pSet);
		return NULL;
	}
	return pSet;
}



/* 释放全部元素和存储，但不释放集合结构。 */
XRT_API void xrtTypedSetUnit(xtypedset* pSet)
{
	if ( pSet == NULL ) {
		return;
	}
	if ( !__xrtTypedSetCanMutate(pSet, "unit") ) {
		return;
	}
	xrtSetUnit(&pSet->Storage);
	pSet->ItemType = NULL;
}



/* 释放类型集合持有的全部资源和堆结构。 */
XRT_API void xrtTypedSetDestroy(xtypedset* pSet)
{
	if ( pSet == NULL ) {
		return;
	}
	if ( !__xrtTypedSetCanMutate(pSet, "destroy") ) {
		return;
	}
	xrtTypedSetUnit(pSet);
	xrtFree(pSet);
}



/* 返回集合借用的元素类型描述。 */
XRT_API const xrttype* xrtTypedSetItemType(const xtypedset* pSet)
{
	return __xrtTypedSetCanRead(pSet, "item-type") ?
		pSet->ItemType : NULL;
}



/* 返回集合当前元素数量。 */
XRT_API size_t xrtTypedSetCount(const xtypedset* pSet)
{
	return __xrtTypedSetCanRead(pSet, "count") ?
		xrtSetCount(&pSet->Storage) : 0u;
}



/* 返回集合再次扩容前可容纳的元素数量。 */
XRT_API size_t xrtTypedSetCapacity(const xtypedset* pSet)
{
	return __xrtTypedSetCanRead(pSet, "capacity") ?
		xrtSetCapacity(&pSet->Storage) : 0u;
}



/* 清空全部元素并保留桶数组供后续复用。 */
XRT_API bool xrtTypedSetClear(xtypedset* pSet)
{
	if ( !__xrtTypedSetCanMutate(pSet, "clear") ) {
		return false;
	}
	xrtSetClear(&pSet->Storage);
	return true;
}



/* 确保集合无需扩容即可容纳指定数量的元素。 */
XRT_API bool xrtTypedSetReserve(xtypedset* pSet, size_t iCapacity)
{
	if ( !__xrtTypedSetCanMutate(pSet, "reserve") ) {
		return false;
	}
	if ( !xrtSetReserve(&pSet->Storage, iCapacity) ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_OPERATION,
			"reserve", "the typed set capacity could not be reserved");
		return false;
	}
	return true;
}



/* 把桶数组收缩到当前元素数量需要的最小容量。 */
XRT_API bool xrtTypedSetTrim(xtypedset* pSet)
{
	if ( !__xrtTypedSetCanMutate(pSet, "trim") ) {
		return false;
	}
	if ( !xrtSetTrim(&pSet->Storage) ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_OPERATION,
			"trim", "the typed set storage could not be trimmed");
		return false;
	}
	return true;
}



/* 返回已有或失败原子地复制加入的只读规范值。 */
XRT_API const void* xrtTypedSetGetOrAdd(
	xtypedset* pSet,
	const void* pItem,
	bool* pNew
)
{
	const void* pStored;

	if ( pNew != NULL ) {
		*pNew = false;
	}
	if ( !__xrtTypedSetCanMutate(pSet, "get-or-add") ||
		 !__xrtTypedSetSourceValid(pSet, pItem, "get-or-add") ) {
		return NULL;
	}
	pStored = xrtSetGetOrAdd(&pSet->Storage, pItem, pNew);
	if ( pStored == NULL ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_OPERATION,
			"get-or-add", "the typed set item could not be inserted");
	}
	return pStored;
}



/* 复制加入元素，已有等价元素时成功且不替换规范值。 */
XRT_API bool xrtTypedSetAdd(xtypedset* pSet, const void* pItem)
{
	return xrtTypedSetGetOrAdd(pSet, pItem, NULL) != NULL;
}



/* 返回集合内部的只读规范值，缺失是正常结果。 */
XRT_API const void* xrtTypedSetGet(
	const xtypedset* pSet,
	const void* pItem
)
{
	if ( !__xrtTypedSetCanRead(pSet, "get") ||
		 !__xrtTypedSetSourceValid(pSet, pItem, "get") ) {
		return NULL;
	}
	return xrtSetGet(&pSet->Storage, pItem);
}



/* 判断集合是否拥有等价值。 */
XRT_API bool xrtTypedSetHas(
	const xtypedset* pSet,
	const void* pItem
)
{
	return xrtTypedSetGet(pSet, pItem) != NULL;
}



/* 删除等价值并执行类型资源释放。 */
XRT_API bool xrtTypedSetRemove(xtypedset* pSet, const void* pItem)
{
	if ( !__xrtTypedSetCanMutate(pSet, "remove") ||
		 !__xrtTypedSetSourceValid(pSet, pItem, "remove") ) {
		return false;
	}
	return xrtSetRemove(&pSet->Storage, pItem);
}



/* 把规范值移动到外部已初始化输出后删除。 */
XRT_API bool xrtTypedSetTake(
	xtypedset* pSet,
	const void* pItem,
	ptr pValue
)
{
	const void* pStored;

	if ( !__xrtTypedSetCanMutate(pSet, "take") ||
		 !__xrtTypedSetSourceValid(pSet, pItem, "take") ||
		 !__xrtTypedSetOutputExternal(pSet, pValue, "take") ) {
		return false;
	}
	pStored = xrtSetGet(&pSet->Storage, pItem);
	if ( pStored == NULL ) {
		return false;
	}
	if ( !__xrtSetMoveOut(
		&pSet->Storage,
		pStored,
		pValue,
		__xrtTypedSetMove,
		(ptr)pSet->ItemType
	) ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_OPERATION,
			"take", "the typed set item could not be moved out");
		return false;
	}
	return true;
}



/* 按最短插入顺序方向查找指定位置的规范值。 */
XRT_API const void* xrtTypedSetAt(
	const xtypedset* pSet,
	size_t iIndex
)
{
	xsetiter Iterator;
	const void* pItem = NULL;
	size_t iSteps;

	if ( !__xrtTypedSetCanRead(pSet, "at") ) {
		return NULL;
	}
	if ( iIndex >= pSet->Storage.Count ) {
		__xrtTypedSetError(XERR_RANGE, XTYPED_SET_ERROR_RANGE,
			"at", "the typed set index is out of range");
		return NULL;
	}
	if ( iIndex <= ((pSet->Storage.Count - 1u) >> 1u) ) {
		iSteps = iIndex;
		if ( !xrtSetIterBegin((xset*)&pSet->Storage, &Iterator) ) {
			__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_STATE,
				"at", "the typed set iterator could not start");
			return NULL;
		}
	} else {
		iSteps = pSet->Storage.Count - iIndex - 1u;
		if ( !xrtSetIterRBegin((xset*)&pSet->Storage, &Iterator) ) {
			__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_STATE,
				"at", "the typed set reverse iterator could not start");
			return NULL;
		}
	}
	for ( size_t i = 0u; i <= iSteps; i++ ) {
		pItem = xrtSetIterNext(&Iterator);
	}
	xrtSetIterEnd(&Iterator);
	if ( pItem == NULL ) {
		__xrtTypedSetError(XERR_STATE, XTYPED_SET_ERROR_STATE,
			"at", "the typed set ended before the requested index");
	}
	return pItem;
}



/* 使用指定底层方向初始化类型集合迭代器。 */
static bool __xrtTypedSetIterStart(
	xtypedset* pSet,
	xtypedsetiter* pIterator,
	bool bReverse,
	cstr sOperation
)
{
	bool bSuccess;

	if ( pIterator != NULL ) {
		memset(pIterator, 0, sizeof(*pIterator));
	}
	if ( (pIterator == NULL) ||
		 !__xrtTypedSetCanRead(pSet, sOperation) ) {
		if ( pIterator == NULL ) {
			__xrtTypedSetError(XERR_ARGUMENT, XTYPED_SET_ERROR_ARGUMENT,
				sOperation, "the typed set iterator is null");
		}
		return false;
	}
	bSuccess = bReverse ?
		xrtSetIterRBegin(&pSet->Storage, &pIterator->Base) :
		xrtSetIterBegin(&pSet->Storage, &pIterator->Base);
	if ( !bSuccess ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_STATE,
			sOperation, "the typed set iterator could not start");
		return false;
	}
	pIterator->Set = pSet;
	return true;
}



/* 启动按插入顺序的完整迭代。 */
XRT_API bool xrtTypedSetIterBegin(
	xtypedset* pSet,
	xtypedsetiter* pIterator
)
{
	return __xrtTypedSetIterStart(
		pSet, pIterator, false, "iter-begin"
	);
}



/* 启动按插入顺序逆序的完整迭代。 */
XRT_API bool xrtTypedSetIterRBegin(
	xtypedset* pSet,
	xtypedsetiter* pIterator
)
{
	return __xrtTypedSetIterStart(
		pSet, pIterator, true, "iter-rbegin"
	);
}



/* 返回下一只读规范值，并检测结构修改。 */
XRT_API const void* xrtTypedSetIterNext(xtypedsetiter* pIterator)
{
	const void* pItem;

	if ( (pIterator == NULL) || (pIterator->Set == NULL) ) {
		if ( pIterator == NULL ) {
			__xrtTypedSetError(XERR_ARGUMENT, XTYPED_SET_ERROR_ARGUMENT,
				"iter-next", "the typed set iterator is null");
		}
		return NULL;
	}
	if (
		(pIterator->Base.Set != &pIterator->Set->Storage) ||
		(pIterator->Base.Version != pIterator->Set->Storage.Version)
	) {
		xrtSetIterEnd(&pIterator->Base);
		pIterator->Set = NULL;
		__xrtTypedSetError(XERR_STATE, XTYPED_SET_ERROR_STATE,
			"iter-next", "the typed set changed during iteration");
		return NULL;
	}
	pItem = xrtSetIterNext(&pIterator->Base);
	if ( pItem == NULL ) {
		pIterator->Set = NULL;
	}
	return pItem;
}



/* 提前结束迭代并清除全部借用状态。 */
XRT_API void xrtTypedSetIterEnd(xtypedsetiter* pIterator)
{
	if ( pIterator == NULL ) {
		return;
	}
	xrtSetIterEnd(&pIterator->Base);
	pIterator->Set = NULL;
}



/* 失败原子地把源集合缺失值合并到目标集合。 */
XRT_API bool xrtTypedSetMerge(
	xtypedset* pTarget,
	const xtypedset* pSource
)
{
	if ( !__xrtTypedSetCanMutate(pTarget, "merge") ||
		 !__xrtTypedSetCanRead(pSource, "merge") ||
		 !__xrtTypedSetSameType(pTarget, pSource, "merge") ) {
		return false;
	}
	if ( !xrtSetMerge(&pTarget->Storage, &pSource->Storage) ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_OPERATION,
			"merge", "the typed set merge could not be completed");
		return false;
	}
	return true;
}



/* 深度复制集合结构和值。 */
XRT_API xtypedset* xrtTypedSetClone(const xtypedset* pSet)
{
	if ( !__xrtTypedSetCanRead(pSet, "clone") ) {
		return NULL;
	}
	return __xrtTypedSetFromRaw(
		pSet->ItemType, xrtSetClone(&pSet->Storage), "clone"
	);
}



/* 底层集合二元构造器共享统一调用形态。 */
typedef xset* (*xtypedsetbinaryproc)(
	const xset* pLeft,
	const xset* pRight
);



/* 验证类型 ABI 后执行一个底层集合二元构造器。 */
static xtypedset* __xrtTypedSetBinary(
	const xtypedset* pLeft,
	const xtypedset* pRight,
	xtypedsetbinaryproc pOperation,
	cstr sOperation
)
{
	if ( !__xrtTypedSetCanRead(pLeft, sOperation) ||
		 !__xrtTypedSetCanRead(pRight, sOperation) ||
		 !__xrtTypedSetSameType(pLeft, pRight, sOperation) ) {
		return NULL;
	}
	return __xrtTypedSetFromRaw(
		pLeft->ItemType,
		pOperation(&pLeft->Storage, &pRight->Storage),
		sOperation
	);
}



/* 创建两个同类型集合的并集。 */
XRT_API xtypedset* xrtTypedSetUnion(
	const xtypedset* pLeft,
	const xtypedset* pRight
)
{
	return __xrtTypedSetBinary(
		pLeft, pRight, xrtSetUnion, "union"
	);
}



/* 创建两个同类型集合的交集。 */
XRT_API xtypedset* xrtTypedSetIntersection(
	const xtypedset* pLeft,
	const xtypedset* pRight
)
{
	return __xrtTypedSetBinary(
		pLeft, pRight, xrtSetIntersection, "intersection"
	);
}



/* 创建左集合相对右集合的差集。 */
XRT_API xtypedset* xrtTypedSetDifference(
	const xtypedset* pLeft,
	const xtypedset* pRight
)
{
	return __xrtTypedSetBinary(
		pLeft, pRight, xrtSetDifference, "difference"
	);
}



/* 创建两个同类型集合的对称差集。 */
XRT_API xtypedset* xrtTypedSetSymmetricDifference(
	const xtypedset* pLeft,
	const xtypedset* pRight
)
{
	return __xrtTypedSetBinary(
		pLeft, pRight,
		xrtSetSymmetricDifference,
		"symmetric-difference"
	);
}



/* 验证两个只读集合操作数和共享类型描述。 */
static bool __xrtTypedSetOperandsValid(
	const xtypedset* pLeft,
	const xtypedset* pRight,
	cstr sOperation
)
{
	return __xrtTypedSetCanRead(pLeft, sOperation) &&
		__xrtTypedSetCanRead(pRight, sOperation) &&
		__xrtTypedSetSameType(pLeft, pRight, sOperation);
}



/* 判断左集合是否为右集合的子集。 */
XRT_API bool xrtTypedSetIsSubset(
	const xtypedset* pLeft,
	const xtypedset* pRight,
	bool bProper
)
{
	return __xrtTypedSetOperandsValid(pLeft, pRight, "is-subset") &&
		xrtSetIsSubset(&pLeft->Storage, &pRight->Storage, bProper);
}



/* 判断左集合是否为右集合的超集。 */
XRT_API bool xrtTypedSetIsSuperset(
	const xtypedset* pLeft,
	const xtypedset* pRight,
	bool bProper
)
{
	return __xrtTypedSetOperandsValid(pLeft, pRight, "is-superset") &&
		xrtSetIsSuperset(&pLeft->Storage, &pRight->Storage, bProper);
}



/* 判断两个集合是否没有任何共同值。 */
XRT_API bool xrtTypedSetIsDisjoint(
	const xtypedset* pLeft,
	const xtypedset* pRight
)
{
	return __xrtTypedSetOperandsValid(pLeft, pRight, "is-disjoint") &&
		xrtSetIsDisjoint(&pLeft->Storage, &pRight->Storage);
}



/* 判断两个集合是否拥有相同的唯一值。 */
XRT_API bool xrtTypedSetEquals(
	const xtypedset* pLeft,
	const xtypedset* pRight
)
{
	return __xrtTypedSetOperandsValid(pLeft, pRight, "equals") &&
		xrtSetEqual(&pLeft->Storage, &pRight->Storage);
}



/* 初始化对象负载中的类型集合。 */
static bool __xrtTypedSetInstanceInit(
	ptr pInstance,
	const xrttype* pType
)
{
	if ( !xrtTypedSetTypeValidate(pType) ) {
		return false;
	}
	return xrtTypedSetInit(
		(xtypedset*)pInstance, pType->Arguments[0]
	);
}



/* 销毁对象负载中的类型集合。 */
static void __xrtTypedSetInstanceDrop(
	ptr pInstance,
	const xrttype* pType
)
{
	(void)pType;
	xrtTypedSetUnit((xtypedset*)pInstance);
}



/* 类型集合追踪适配器保存对象访问器和失败状态。 */
typedef struct xtypedsettracecontext {
	const xtypedset* Set;
	const xrttype* ItemType;
	xrtobjectvisitor Visit;
	ptr UserData;
	bool Failed;
} xtypedsettracecontext;



/* 枚举一个集合值直接拥有的强对象引用。 */
static bool __xrtTypedSetTraceValue(
	const void* pItem,
	ptr pUserData
	)
{
	xtypedsettracecontext* pContext =
		(xtypedsettracecontext*)pUserData;
	bool bTraced;

	if ( !__xrtSetCallbackBegin(&pContext->Set->Storage) ) {
		pContext->Failed = true;
		return false;
	}
	bTraced = xrtTypeTraceValue(
		pContext->ItemType,
		pItem,
		pContext->Visit,
		pContext->UserData
	);
	__xrtSetCallbackEnd(&pContext->Set->Storage);
	if ( !bTraced ) {
		pContext->Failed = true;
		return false;
	}
	return true;
}



/* 枚举类型集合所有值直接拥有的强对象引用。 */
static bool __xrtTypedSetInstanceTrace(
	const void* pInstance,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	xtypedset* pSet = (xtypedset*)pInstance;
	xtypedsettracecontext Context;
	size_t iExpected;
	size_t iVisited;
	(void)pType;

	if ( !__xrtTypedSetCanRead(pSet, "instance-trace") ) {
		return false;
	}
	Context.Set = pSet;
	Context.ItemType = pSet->ItemType;
	Context.Visit = pVisit;
	Context.UserData = pContext;
	Context.Failed = false;
	iExpected = pSet->Storage.Count;
	iVisited = xrtSetVisit(
		&pSet->Storage, __xrtTypedSetTraceValue, &Context
	);
	if ( Context.Failed ) {
		return false;
	}
	if ( iVisited != iExpected ) {
		__xrtTypedSetWrap(XERR_STATE, XTYPED_SET_ERROR_STATE,
			"instance-trace", "the typed set trace visit was incomplete");
		return false;
	}
	return true;
}



/* 返回对象集合负载共享的实例操作表。 */
XRT_API const xrtinstanceops* xrtTypedSetInstanceOps(void)
{
	static const xrtinstanceops Ops = {
		.Init = __xrtTypedSetInstanceInit,
		.Drop = __xrtTypedSetInstanceDrop,
		.Trace = __xrtTypedSetInstanceTrace
	};

	return &Ops;
}



/* 验证可由对象系统承载的泛型集合类型描述。 */
XRT_API bool xrtTypedSetTypeValidate(const xrttype* pType)
{
	if ( !xrtTypeValidate(pType) ) {
		__xrtTypedSetWrap(XERR_ARGUMENT, XTYPED_SET_ERROR_TYPE,
			"type-validate", "the typed set object type is invalid");
		return false;
	}
	if (
		(pType->Kind != XRT_TYPE_SET) ||
		((pType->Flags & XRT_TYPE_FLAG_REFERENCE) == 0u) ||
		(pType->ArgumentCount != 1u) ||
		(pType->Arguments == NULL) ||
		(pType->InstanceSize != sizeof(xtypedset)) ||
		(pType->InstanceAlign <
		 XRT_INTERNAL_OBJECT_ALIGNOF(xtypedset)) ||
		(pType->InstanceOps != xrtTypedSetInstanceOps())
	) {
		__xrtTypedSetError(XERR_TYPE, XTYPED_SET_ERROR_TYPE,
			"type-validate", "the typed set object type contract is invalid");
		return false;
	}
	return __xrtTypedSetItemTypeValidate(
		pType->Arguments[0], "type-validate"
	);
}

#endif
