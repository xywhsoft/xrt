#include "../internal/xrt_map.h"
#include "../internal/xrt_runtime_type.h"
#include "../internal/xrt_typed_container.h"
#include "../internal/xrt_typed_dict.h"
#include <xrt/typed_dict.h>



#if defined(XRUNTIME_FEATURE_TYPED_DICT)

/* 在跨模块用户回调期间拒绝当前类型字典的全部 API 重入。 */
bool __xrtTypedDictCallbackBegin(const xtypeddict* pDict)
{
	return (pDict != NULL) && __xrtMapCallbackBegin(&pDict->Storage);
}



/* 结束当前类型字典的跨模块用户回调门禁。 */
void __xrtTypedDictCallbackEnd(const xtypeddict* pDict)
{
	if ( pDict != NULL ) {
		__xrtMapCallbackEnd(&pDict->Storage);
	}
}




/* 设置类型字典模块结构化错误。 */
static void __xrtTypedDictError(
	xerrkind Kind,
	xtypeddicterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.typed-dict";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为下层类型或映射错误补充类型字典上下文。 */
static void __xrtTypedDictWrap(
	xerrkind DefaultKind,
	xtypeddicterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.typed-dict";
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



/* 验证元素类型可由稳定条目字典安全拥有。 */
static bool __xrtTypedDictItemTypeValidate(
	const xrttype* pItemType,
	cstr sOperation
)
{
	if ( !xrtTypeValidate(pItemType) ) {
		__xrtTypedDictWrap(XERR_ARGUMENT, XTYPED_DICT_ERROR_TYPE,
			sOperation, "the dictionary item type is invalid");
		return false;
	}
	if ( pItemType->Size == 0u ) {
		__xrtTypedDictError(XERR_TYPE, XTYPED_DICT_ERROR_TYPE,
			sOperation, "a typed dictionary item must occupy storage");
		return false;
	}
	if ( !xrtTypeIsCopyable(pItemType) ) {
		__xrtTypedDictError(XERR_UNSUPPORTED, XTYPED_DICT_ERROR_TYPE,
			sOperation, "the dictionary item type is not copyable");
		return false;
	}
	return true;
}



/* 检查文本键视图形态并保留长度明确的内嵌零。 */
static bool __xrtTypedDictKeyValid(xstrview Key, cstr sOperation)
{
	if ( (Key.Data == NULL) && (Key.Size != 0u) ) {
		__xrtTypedDictError(XERR_ARGUMENT, XTYPED_DICT_ERROR_ARGUMENT,
			sOperation, "the dictionary key view is invalid");
		return false;
	}
	return true;
}



/* 把文本键视图转换为不改变字节边界的底层键。 */
static xbytesview __xrtTypedDictKey(xstrview Key)
{
	xbytesview Result = { (cbytes)Key.Data, Key.Size };

	return Result;
}



/* 把底层规范键转换为文本键视图。 */
static xstrview __xrtTypedDictTextKey(xbytesview Key)
{
	xstrview Result = { (cstr)Key.Data, Key.Size };

	return Result;
}



/* 销毁映射拥有的一个完整初始化类型值。 */
static void __xrtTypedDictDrop(
	xbytesview Key,
	ptr pValue,
	ptr pUserData
)
{
	(void)Key;
	xrtTypeDropValue((const xrttype*)pUserData, pValue);
}



/* 默认初始化一个尚未提交的新字典值。 */
static bool __xrtTypedDictInitValue(
	xbytesview Key,
	ptr pValue,
	ptr pUserData
)
{
	(void)Key;
	return xrtTypeInitValue((const xrttype*)pUserData, pValue);
}



/* 新值复制上下文保存元素类型和借用来源。 */
typedef struct xtypeddictcopycontext {
	const xrttype* ItemType;
	const void* Item;
} xtypeddictcopycontext;



/* 移入上下文保存元素类型和调用方持有的已初始化来源。 */
typedef struct xtypeddictmovecontext {
	const xrttype* ItemType;
	ptr Item;
} xtypeddictmovecontext;



/* 初始化并复制一个尚未提交的新字典值。 */
static bool __xrtTypedDictInitCopy(
	xbytesview Key,
	ptr pValue,
	ptr pUserData
)
{
	xtypeddictcopycontext* pContext =
		(xtypeddictcopycontext*)pUserData;
	xerror* pError;
	(void)Key;

	if ( !xrtTypeInitValue(pContext->ItemType, pValue) ) {
		return false;
	}
	if ( xrtTypeCopyValue(
		pContext->ItemType, pValue, pContext->Item
	) ) {
		return true;
	}
	pError = xrtTakeError();
	xrtTypeDropValue(pContext->ItemType, pValue);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return false;
}



/* 初始化新值槽，并仅在全部存储准备完成后移入来源值。 */
static bool __xrtTypedDictInitMove(
	xbytesview Key,
	ptr pValue,
	ptr pUserData
)
{
	xtypeddictmovecontext* pContext =
		(xtypeddictmovecontext*)pUserData;
	xerror* pError;
	(void)Key;

	if ( !xrtTypeInitValue(pContext->ItemType, pValue) ) {
		return false;
	}
	if ( xrtTypeMoveValue(
		pContext->ItemType, pValue, pContext->Item
	) ) {
		return true;
	}
	pError = xrtTakeError();
	xrtTypeDropValue(pContext->ItemType, pValue);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return false;
}



/* 失败原子地替换一个完整初始化字典值。 */
static bool __xrtTypedDictReplace(
	ptr pTarget,
	const void* pSource,
	ptr pUserData
)
{
	return xrtTypeCopyValue(
		(const xrttype*)pUserData, pTarget, pSource
	);
}



/* 适配 map 的只读回调形态，把调用方明确提供的可写来源移入旧值槽。 */
static bool __xrtTypedDictReplaceMove(
	ptr pTarget,
	const void* pSource,
	ptr pUserData
)
{
	return xrtTypeMoveValue(
		(const xrttype*)pUserData, pTarget, (ptr)pSource
	);
}



/* 把字典值移动到调用方已经初始化的外部值。 */
static bool __xrtTypedDictMove(
	ptr pTarget,
	ptr pSource,
	ptr pUserData
)
{
	return xrtTypeMoveValue(
		(const xrttype*)pUserData, pTarget, pSource
	);
}



/* 检查公开类型字典状态、布局和底层策略是否一致。 */
static bool __xrtTypedDictValid(
	const xtypeddict* pDict,
	cstr sOperation
)
{
	if ( (pDict == NULL) || (pDict->ItemType == NULL) ) {
		__xrtTypedDictError(XERR_ARGUMENT, XTYPED_DICT_ERROR_ARGUMENT,
			sOperation, "the typed dictionary is null or uninitialized");
		return false;
	}
	if ( !__xrtMapValid(&pDict->Storage) ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			sOperation, "the typed dictionary storage is invalid");
		return false;
	}
	if (
		!__xrtMapUsesDefaultKeyPolicy(&pDict->Storage) ||
		(pDict->Storage.ValueSize != pDict->ItemType->Size) ||
		(pDict->Storage.Alignment < pDict->ItemType->Align) ||
		(pDict->Storage.Drop != __xrtTypedDictDrop) ||
		(pDict->Storage.DropUserData != pDict->ItemType)
	) {
		__xrtTypedDictError(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			sOperation, "the typed dictionary layout or policies are invalid");
		return false;
	}
	return true;
}



/* 检查类型字典当前是否允许读取和推进迭代器。 */
static bool __xrtTypedDictCanRead(
	const xtypeddict* pDict,
	cstr sOperation
)
{
	if ( !__xrtTypedDictValid(pDict, sOperation) ) {
		return false;
	}
	if ( !__xrtMapCanRead(&pDict->Storage) ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			sOperation, "the typed dictionary is executing an item callback");
		return false;
	}
	return true;
}



/* 检查类型字典当前是否允许结构和生命周期修改。 */
static bool __xrtTypedDictCanMutate(
	xtypeddict* pDict,
	cstr sOperation
)
{
	if ( !__xrtTypedDictValid(pDict, sOperation) ) {
		return false;
	}
	if ( !__xrtMapCanMutate(&pDict->Storage) ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			sOperation, "the typed dictionary is currently being visited");
		return false;
	}
	return true;
}



/* 判断字节区间是否触及类型字典自身或拥有的底层存储。 */
static bool __xrtTypedDictOwnsRange(
	const xtypeddict* pDict,
	const void* pMemory,
	size_t iSize
)
{
	return __xrtRangesOverlap(pMemory, iSize, pDict, sizeof(*pDict)) ||
		__xrtMapOwnsRange(&pDict->Storage, pMemory, iSize);
}



/* 验证来源是外部值或字典中的准确活动值槽。 */
static bool __xrtTypedDictSourceValid(
	const xtypeddict* pDict,
	const void* pItem,
	cstr sOperation
)
{
	xmapiter Iterator;
	ptr pStored;
	bool bExact = false;

	if ( pItem == NULL ) {
		__xrtTypedDictError(XERR_ARGUMENT, XTYPED_DICT_ERROR_ARGUMENT,
			sOperation, "the source item is null");
		return false;
	}
	if ( !__xrtTypedDictOwnsRange(
		pDict, pItem, pDict->ItemType->Size
	) ) {
		return true;
	}
	if ( !xrtMapIterBegin((xmap*)&pDict->Storage, &Iterator) ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			sOperation, "the typed dictionary source scan could not start");
		return false;
	}
	while ( (pStored = xrtMapIterNext(&Iterator, NULL)) != NULL ) {
		if ( pStored == pItem ) {
			bExact = true;
			break;
		}
	}
	xrtMapIterEnd(&Iterator);
	if ( !bExact ) {
		__xrtTypedDictError(XERR_ARGUMENT, XTYPED_DICT_ERROR_ARGUMENT,
			sOperation, "an internal source must be an active value boundary");
		return false;
	}
	return true;
}



/* 验证移动输出完全位于类型字典拥有的内存之外。 */
static bool __xrtTypedDictOutputExternal(
	const xtypeddict* pDict,
	const void* pValue,
	cstr sOperation
)
{
	if ( pValue == NULL ) {
		__xrtTypedDictError(XERR_ARGUMENT, XTYPED_DICT_ERROR_ARGUMENT,
			sOperation, "the output value is null");
		return false;
	}
	if ( __xrtTypedDictOwnsRange(
		pDict, pValue, pDict->ItemType->Size
	) ) {
		__xrtTypedDictError(XERR_ARGUMENT, XTYPED_DICT_ERROR_ARGUMENT,
			sOperation, "the output value must not alias dictionary storage");
		return false;
	}
	return true;
}



/* 验证两个字典借用完全相同的值类型生命周期描述。 */
static bool __xrtTypedDictSameType(
	const xtypeddict* pLeft,
	const xtypeddict* pRight,
	cstr sOperation
)
{
	if ( pLeft->ItemType != pRight->ItemType ) {
		__xrtTypedDictError(XERR_TYPE, XTYPED_DICT_ERROR_TYPE,
			sOperation, "typed dictionary operands must share one item type descriptor");
		return false;
	}
	return true;
}



/* 在清理临时字典期间保留原始失败。 */
static void __xrtTypedDictDestroyPreserveError(xtypeddict* pDict)
{
	xerror* pError = xrtTakeError();

	xrtTypedDictDestroy(pDict);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 初始化一个拥有类型值的空字典。 */
XRT_API bool xrtTypedDictInit(
	xtypeddict* pDict,
	const xrttype* pItemType
)
{
	size_t iAlignment;

	if ( pDict == NULL ) {
		__xrtTypedDictError(XERR_ARGUMENT, XTYPED_DICT_ERROR_ARGUMENT,
			"init", "the typed dictionary is null");
		return false;
	}
	memset(pDict, 0, sizeof(*pDict));
	if ( !__xrtTypedDictItemTypeValidate(pItemType, "init") ) {
		return false;
	}
	iAlignment = pItemType->Align > XRT_MAP_ALIGNMENT_DEFAULT ?
		pItemType->Align : XRT_MAP_ALIGNMENT_DEFAULT;
	if ( !xrtMapInitAligned(
		&pDict->Storage, pItemType->Size, iAlignment
	) ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_OPERATION,
			"init", "the typed dictionary storage could not be initialized");
		return false;
	}
	pDict->ItemType = pItemType;
	if ( !xrtMapSetDrop(
		&pDict->Storage, __xrtTypedDictDrop, (ptr)pItemType
	) ) {
		xrtMapUnit(&pDict->Storage);
		memset(pDict, 0, sizeof(*pDict));
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_OPERATION,
			"init", "the typed dictionary ownership policy could not be installed");
		return false;
	}
	return true;
}



/* 创建一个堆分配的空类型字典。 */
XRT_API xtypeddict* xrtTypedDictCreate(const xrttype* pItemType)
{
	xtypeddict* pDict = (xtypeddict*)xrtMalloc(sizeof(*pDict));

	if ( pDict == NULL ) {
		return NULL;
	}
	if ( !xrtTypedDictInit(pDict, pItemType) ) {
		xrtFree(pDict);
		return NULL;
	}
	return pDict;
}



/* 释放全部键值和存储，但不释放字典结构。 */
XRT_API void xrtTypedDictUnit(xtypeddict* pDict)
{
	if ( pDict == NULL ) {
		return;
	}
	if ( !__xrtTypedDictCanMutate(pDict, "unit") ) {
		return;
	}
	xrtMapUnit(&pDict->Storage);
	pDict->ItemType = NULL;
}



/* 释放类型字典持有的全部资源和堆结构。 */
XRT_API void xrtTypedDictDestroy(xtypeddict* pDict)
{
	if ( pDict == NULL ) {
		return;
	}
	if ( !__xrtTypedDictCanMutate(pDict, "destroy") ) {
		return;
	}
	xrtTypedDictUnit(pDict);
	xrtFree(pDict);
}



/* 返回字典借用的元素类型描述。 */
XRT_API const xrttype* xrtTypedDictItemType(const xtypeddict* pDict)
{
	return __xrtTypedDictCanRead(pDict, "item-type") ?
		pDict->ItemType : NULL;
}



/* 返回字典当前键值数量。 */
XRT_API size_t xrtTypedDictCount(const xtypeddict* pDict)
{
	return __xrtTypedDictCanRead(pDict, "count") ?
		xrtMapCount(&pDict->Storage) : 0u;
}



/* 返回字典再次扩容前可容纳的键值数量。 */
XRT_API size_t xrtTypedDictCapacity(const xtypeddict* pDict)
{
	return __xrtTypedDictCanRead(pDict, "capacity") ?
		xrtMapCapacity(&pDict->Storage) : 0u;
}



/* 清空全部键值并保留桶数组供后续复用。 */
XRT_API bool xrtTypedDictClear(xtypeddict* pDict)
{
	if ( !__xrtTypedDictCanMutate(pDict, "clear") ) {
		return false;
	}
	xrtMapClear(&pDict->Storage);
	return true;
}



/* 确保字典无需扩容即可容纳指定数量的键。 */
XRT_API bool xrtTypedDictReserve(xtypeddict* pDict, size_t iCapacity)
{
	if ( !__xrtTypedDictCanMutate(pDict, "reserve") ) {
		return false;
	}
	if ( !xrtMapReserve(&pDict->Storage, iCapacity) ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_OPERATION,
			"reserve", "the typed dictionary capacity could not be reserved");
		return false;
	}
	return true;
}



/* 把桶数组收缩到当前键数需要的最小容量。 */
XRT_API bool xrtTypedDictTrim(xtypeddict* pDict)
{
	if ( !__xrtTypedDictCanMutate(pDict, "trim") ) {
		return false;
	}
	if ( !xrtMapTrim(&pDict->Storage) ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_OPERATION,
			"trim", "the typed dictionary storage could not be trimmed");
		return false;
	}
	return true;
}



/* 返回已有值槽，或按元素类型默认初始化一个新值。 */
XRT_API ptr xrtTypedDictGetOrAdd(
	xtypeddict* pDict,
	xstrview Key,
	bool* pNew
)
{
	ptr pValue;

	if ( pNew != NULL ) {
		*pNew = false;
	}
	if ( !__xrtTypedDictCanMutate(pDict, "get-or-add") ||
		 !__xrtTypedDictKeyValid(Key, "get-or-add") ) {
		return NULL;
	}
	pValue = xrtMapGetOrInit(
		&pDict->Storage,
		__xrtTypedDictKey(Key),
		__xrtTypedDictInitValue,
		(ptr)pDict->ItemType,
		pNew
	);
	if ( pValue == NULL ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_OPERATION,
			"get-or-add", "the typed dictionary value could not be initialized");
	}
	return pValue;
}



/* 失败原子地复制插入或替换一个键值。 */
XRT_API bool xrtTypedDictSet(
	xtypeddict* pDict,
	xstrview Key,
	const void* pItem
)
{
	xtypeddictcopycontext Context;
	ptr pStored;
	bool bNew;

	if ( !__xrtTypedDictCanMutate(pDict, "set") ||
		 !__xrtTypedDictKeyValid(Key, "set") ||
		 !__xrtTypedDictSourceValid(pDict, pItem, "set") ) {
		return false;
	}
	Context.ItemType = pDict->ItemType;
	Context.Item = pItem;
	pStored = __xrtMapSetOrInit(
		&pDict->Storage,
		__xrtTypedDictKey(Key),
		pItem,
		__xrtTypedDictReplace,
		(ptr)pDict->ItemType,
		__xrtTypedDictInitCopy,
		&Context,
		&bNew
	);
	if ( pStored == NULL ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_OPERATION,
			"set", "the typed dictionary value could not be set");
		return false;
	}
	(void)bNew;
	return true;
}



/* 失败原子地把外部已初始化值移入指定键。 */
XRT_API bool xrtTypedDictSetTake(
	xtypeddict* pDict,
	xstrview Key,
	ptr pItem
)
{
	xtypeddictmovecontext Context;
	ptr pStored;
	bool bNew;

	if ( !__xrtTypedDictCanMutate(pDict, "set-take") ||
		 !__xrtTypedDictKeyValid(Key, "set-take") ||
		 !__xrtTypedDictOutputExternal(pDict, pItem, "set-take") ) {
		return false;
	}
	Context.ItemType = pDict->ItemType;
	Context.Item = pItem;
	pStored = __xrtMapSetOrInit(
		&pDict->Storage,
		__xrtTypedDictKey(Key),
		pItem,
		__xrtTypedDictReplaceMove,
		(ptr)pDict->ItemType,
		__xrtTypedDictInitMove,
		&Context,
		&bNew
	);
	if ( pStored == NULL ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_OPERATION,
			"set-take", "the typed dictionary value could not receive the source");
		return false;
	}
	(void)bNew;
	return true;
}



/* 返回指定键的可写借用值槽。 */
XRT_API ptr xrtTypedDictGet(xtypeddict* pDict, xstrview Key)
{
	if ( !__xrtTypedDictCanRead(pDict, "get") ||
		 !__xrtTypedDictKeyValid(Key, "get") ) {
		return NULL;
	}
	return xrtMapGet(&pDict->Storage, __xrtTypedDictKey(Key));
}



/* 返回指定键的只读借用值槽。 */
XRT_API const void* xrtTypedDictConstGet(
	const xtypeddict* pDict,
	xstrview Key
)
{
	if ( !__xrtTypedDictCanRead(pDict, "const-get") ||
		 !__xrtTypedDictKeyValid(Key, "const-get") ) {
		return NULL;
	}
	return xrtMapConstGet(&pDict->Storage, __xrtTypedDictKey(Key));
}



/* 判断指定文本键是否存在。 */
XRT_API bool xrtTypedDictHas(const xtypeddict* pDict, xstrview Key)
{
	if ( !__xrtTypedDictCanRead(pDict, "has") ||
		 !__xrtTypedDictKeyValid(Key, "has") ) {
		return false;
	}
	return xrtMapHas(&pDict->Storage, __xrtTypedDictKey(Key));
}



/* 返回与查询等价的内部规范键视图。 */
XRT_API bool xrtTypedDictStoredKey(
	const xtypeddict* pDict,
	xstrview Key,
	xstrview* pStoredKey
)
{
	xbytesview Stored;

	if ( pStoredKey != NULL ) {
		pStoredKey->Data = NULL;
		pStoredKey->Size = 0u;
	}
	if ( (pStoredKey == NULL) ||
		 !__xrtTypedDictCanRead(pDict, "stored-key") ||
		 !__xrtTypedDictKeyValid(Key, "stored-key") ) {
		if ( pStoredKey == NULL ) {
			__xrtTypedDictError(XERR_ARGUMENT, XTYPED_DICT_ERROR_ARGUMENT,
				"stored-key", "the stored key output is null");
		}
		return false;
	}
	if ( !xrtMapStoredKey(
		&pDict->Storage, __xrtTypedDictKey(Key), &Stored
	) ) {
		return false;
	}
	*pStoredKey = __xrtTypedDictTextKey(Stored);
	return true;
}



/* 删除指定键并执行类型资源释放。 */
XRT_API bool xrtTypedDictRemove(xtypeddict* pDict, xstrview Key)
{
	if ( !__xrtTypedDictCanMutate(pDict, "remove") ||
		 !__xrtTypedDictKeyValid(Key, "remove") ) {
		return false;
	}
	return xrtMapRemove(&pDict->Storage, __xrtTypedDictKey(Key));
}



/* 把值移动到外部已初始化输出后删除指定键。 */
XRT_API bool xrtTypedDictTake(
	xtypeddict* pDict,
	xstrview Key,
	ptr pValue
)
{
	if ( !__xrtTypedDictCanMutate(pDict, "take") ||
		 !__xrtTypedDictKeyValid(Key, "take") ||
		 !__xrtTypedDictOutputExternal(pDict, pValue, "take") ) {
		return false;
	}
	if ( !xrtMapHas(&pDict->Storage, __xrtTypedDictKey(Key)) ) {
		return false;
	}
	if ( !__xrtMapMoveOut(
		&pDict->Storage,
		__xrtTypedDictKey(Key),
		pValue,
		__xrtTypedDictMove,
		(ptr)pDict->ItemType
	) ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_OPERATION,
			"take", "the typed dictionary value could not be moved out");
		return false;
	}
	return true;
}



/* 按最短插入顺序方向查找指定位置的值。 */
static ptr __xrtTypedDictAt(
	const xtypeddict* pDict,
	size_t iIndex,
	xstrview* pKey,
	cstr sOperation
)
{
	xmapiter Iterator;
	xbytesview Key;
	ptr pValue = NULL;
	size_t iSteps;

	if ( pKey != NULL ) {
		pKey->Data = NULL;
		pKey->Size = 0u;
	}
	if ( !__xrtTypedDictCanRead(pDict, sOperation) ) {
		return NULL;
	}
	if ( iIndex >= pDict->Storage.Count ) {
		__xrtTypedDictError(XERR_RANGE, XTYPED_DICT_ERROR_RANGE,
			sOperation, "the typed dictionary index is out of range");
		return NULL;
	}
	if ( iIndex <= ((pDict->Storage.Count - 1u) >> 1u) ) {
		iSteps = iIndex;
		if ( !xrtMapIterBegin((xmap*)&pDict->Storage, &Iterator) ) {
			__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
				sOperation, "the typed dictionary iterator could not start");
			return NULL;
		}
	} else {
		iSteps = pDict->Storage.Count - iIndex - 1u;
		if ( !xrtMapIterRBegin((xmap*)&pDict->Storage, &Iterator) ) {
			__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
				sOperation, "the typed dictionary reverse iterator could not start");
			return NULL;
		}
	}
	for ( size_t i = 0u; i <= iSteps; i++ ) {
		pValue = xrtMapIterNext(&Iterator, &Key);
	}
	xrtMapIterEnd(&Iterator);
	if ( pValue == NULL ) {
		__xrtTypedDictError(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			sOperation, "the typed dictionary ended before the requested index");
		return NULL;
	}
	if ( pKey != NULL ) {
		*pKey = __xrtTypedDictTextKey(Key);
	}
	return pValue;
}



/* 按插入顺序返回指定位置的可写值槽和可选键。 */
XRT_API ptr xrtTypedDictAt(
	xtypeddict* pDict,
	size_t iIndex,
	xstrview* pKey
)
{
	return __xrtTypedDictAt(pDict, iIndex, pKey, "at");
}



/* 按插入顺序返回指定位置的只读值槽和可选键。 */
XRT_API const void* xrtTypedDictConstAt(
	const xtypeddict* pDict,
	size_t iIndex,
	xstrview* pKey
)
{
	return __xrtTypedDictAt(pDict, iIndex, pKey, "const-at");
}



/* 使用指定底层方向初始化类型字典迭代器。 */
static bool __xrtTypedDictIterStart(
	xtypeddict* pDict,
	xtypeddictiter* pIterator,
	bool bReverse,
	cstr sOperation
)
{
	bool bSuccess;

	if ( pIterator != NULL ) {
		memset(pIterator, 0, sizeof(*pIterator));
	}
	if ( (pIterator == NULL) ||
		 !__xrtTypedDictCanRead(pDict, sOperation) ) {
		if ( pIterator == NULL ) {
			__xrtTypedDictError(XERR_ARGUMENT, XTYPED_DICT_ERROR_ARGUMENT,
				sOperation, "the typed dictionary iterator is null");
		}
		return false;
	}
	bSuccess = bReverse ?
		xrtMapIterRBegin(&pDict->Storage, &pIterator->Base) :
		xrtMapIterBegin(&pDict->Storage, &pIterator->Base);
	if ( !bSuccess ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			sOperation, "the typed dictionary iterator could not start");
		return false;
	}
	pIterator->Dict = pDict;
	return true;
}



/* 启动按插入顺序的完整迭代。 */
XRT_API bool xrtTypedDictIterBegin(
	xtypeddict* pDict,
	xtypeddictiter* pIterator
)
{
	return __xrtTypedDictIterStart(
		pDict, pIterator, false, "iter-begin"
	);
}



/* 启动按插入顺序逆序的完整迭代。 */
XRT_API bool xrtTypedDictIterRBegin(
	xtypeddict* pDict,
	xtypeddictiter* pIterator
)
{
	return __xrtTypedDictIterStart(
		pDict, pIterator, true, "iter-rbegin"
	);
}



/* 返回下一借用值槽和可选规范键，并检测结构修改。 */
XRT_API ptr xrtTypedDictIterNext(
	xtypeddictiter* pIterator,
	xstrview* pKey
)
{
	xbytesview Key;
	ptr pValue;

	if ( pKey != NULL ) {
		pKey->Data = NULL;
		pKey->Size = 0u;
	}
	if ( (pIterator == NULL) || (pIterator->Dict == NULL) ) {
		if ( pIterator == NULL ) {
			__xrtTypedDictError(XERR_ARGUMENT, XTYPED_DICT_ERROR_ARGUMENT,
				"iter-next", "the typed dictionary iterator is null");
		}
		return NULL;
	}
	if (
		(pIterator->Base.Map != &pIterator->Dict->Storage) ||
		(pIterator->Base.Version != pIterator->Dict->Storage.Version)
	) {
		xrtMapIterEnd(&pIterator->Base);
		pIterator->Dict = NULL;
		__xrtTypedDictError(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			"iter-next", "the typed dictionary changed during iteration");
		return NULL;
	}
	pValue = xrtMapIterNext(&pIterator->Base, &Key);
	if ( pValue == NULL ) {
		pIterator->Dict = NULL;
		return NULL;
	}
	if ( pKey != NULL ) {
		*pKey = __xrtTypedDictTextKey(Key);
	}
	return pValue;
}



/* 提前结束迭代并清除全部借用状态。 */
XRT_API void xrtTypedDictIterEnd(xtypeddictiter* pIterator)
{
	if ( pIterator == NULL ) {
		return;
	}
	xrtMapIterEnd(&pIterator->Base);
	pIterator->Dict = NULL;
}



/* 在调用方保护来源结构期间深复制字典。 */
static xtypeddict* __xrtTypedDictCloneProtected(
	const xtypeddict* pDict,
	cstr sOperation
)
{
	xtypeddict* pClone;
	xmapiter Iterator;
	xbytesview Key;
	ptr pValue;

	pClone = xrtTypedDictCreate(pDict->ItemType);
	if ( pClone == NULL ) {
		__xrtTypedDictWrap(XERR_MEMORY, XTYPED_DICT_ERROR_OPERATION,
			sOperation, "the typed dictionary clone could not be created");
		return NULL;
	}
	if ( !xrtTypedDictReserve(pClone, pDict->Storage.Count) ||
		 !xrtMapIterBegin((xmap*)&pDict->Storage, &Iterator) ) {
		__xrtTypedDictDestroyPreserveError(pClone);
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_OPERATION,
			sOperation, "the typed dictionary clone could not be prepared");
		return NULL;
	}
	while ( (pValue = xrtMapIterNext(&Iterator, &Key)) != NULL ) {
		if ( !xrtTypedDictSet(
			pClone, __xrtTypedDictTextKey(Key), pValue
		) ) {
			xrtMapIterEnd(&Iterator);
			__xrtTypedDictDestroyPreserveError(pClone);
			__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_OPERATION,
				sOperation, "a typed dictionary value could not be cloned");
			return NULL;
		}
	}
	xrtMapIterEnd(&Iterator);
	return pClone;
}



/* 深复制一个独立堆类型字典。 */
XRT_API xtypeddict* xrtTypedDictClone(const xtypeddict* pDict)
{
	xtypeddict* pClone;
	bool bProtected;

	if ( !__xrtTypedDictCanRead(pDict, "clone") ) {
		return NULL;
	}
	if ( !__xrtMapProtectRead(&pDict->Storage, &bProtected) ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			"clone", "the typed dictionary source could not be protected");
		return NULL;
	}
	pClone = __xrtTypedDictCloneProtected(pDict, "clone");
	__xrtMapUnprotectRead(&pDict->Storage, bProtected);
	return pClone;
}



/* 递增结构版本并跳过外置迭代器保留的零值。 */
static uint64 __xrtTypedDictNextVersion(uint64 iVersion)
{
	iVersion++;
	return iVersion != 0u ? iVersion : 1u;
}



/* 事务合并同类型字典，并按策略处理冲突键。 */
XRT_API bool xrtTypedDictMerge(
	xtypeddict* pTarget,
	const xtypeddict* pSource,
	bool bReplace
)
{
	xtypeddict* pWork;
	xmapiter Iterator;
	xbytesview Key;
	ptr pValue;
	bool bTargetProtected;
	bool bSourceProtected;
	bool bSuccess = true;
	uint64 iTargetVersion;

	if ( !__xrtTypedDictCanMutate(pTarget, "merge") ||
		 !__xrtTypedDictCanRead(pSource, "merge") ||
		 !__xrtTypedDictSameType(pTarget, pSource, "merge") ) {
		return false;
	}
	if ( pTarget == pSource ) {
		return true;
	}
	if ( !__xrtMapProtectRead(
		&pTarget->Storage, &bTargetProtected
	) ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			"merge", "the target dictionary could not be protected");
		return false;
	}
	if ( !__xrtMapProtectRead(
		&pSource->Storage, &bSourceProtected
	) ) {
		__xrtMapUnprotectRead(&pTarget->Storage, bTargetProtected);
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			"merge", "the source dictionary could not be protected");
		return false;
	}
	pWork = __xrtTypedDictCloneProtected(pTarget, "merge");
	if ( pWork == NULL ) {
		__xrtMapUnprotectRead(&pSource->Storage, bSourceProtected);
		__xrtMapUnprotectRead(&pTarget->Storage, bTargetProtected);
		return false;
	}
	if ( !xrtMapIterBegin((xmap*)&pSource->Storage, &Iterator) ) {
		bSuccess = false;
	} else {
		while ( (pValue = xrtMapIterNext(&Iterator, &Key)) != NULL ) {
			xstrview TextKey = __xrtTypedDictTextKey(Key);

			if ( !bReplace && xrtTypedDictHas(pWork, TextKey) ) {
				continue;
			}
			if ( !xrtTypedDictSet(pWork, TextKey, pValue) ) {
				bSuccess = false;
				break;
			}
		}
		xrtMapIterEnd(&Iterator);
	}
	__xrtMapUnprotectRead(&pSource->Storage, bSourceProtected);
	__xrtMapUnprotectRead(&pTarget->Storage, bTargetProtected);
	if ( !bSuccess ) {
		__xrtTypedDictDestroyPreserveError(pWork);
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_OPERATION,
			"merge", "a source dictionary value could not be merged");
		return false;
	}

	iTargetVersion = pTarget->Storage.Version;
	{
		xmap Temporary = pTarget->Storage;

		pTarget->Storage = pWork->Storage;
		pWork->Storage = Temporary;
	}
	pTarget->Storage.Version = __xrtTypedDictNextVersion(iTargetVersion);
	xrtTypedDictDestroy(pWork);
	return true;
}



/* 比较两个字典的类型、键集合和值内容。 */
XRT_API bool xrtTypedDictEquals(
	const xtypeddict* pLeft,
	const xtypeddict* pRight
)
{
	xmapiter Iterator;
	xbytesview Key;
	ptr pLeftValue;
	const void* pRightValue;
	bool bLeftProtected;
	bool bRightProtected;
	bool bEqual = true;
	bool bFailed = false;
	int iCompare;

	if ( !__xrtTypedDictCanRead(pLeft, "equals") ||
		 !__xrtTypedDictCanRead(pRight, "equals") ||
		 !__xrtTypedDictSameType(pLeft, pRight, "equals") ) {
		return false;
	}
	if ( pLeft == pRight ) {
		return true;
	}
	if ( pLeft->Storage.Count != pRight->Storage.Count ) {
		return false;
	}
	if ( !xrtTypeIsComparable(pLeft->ItemType) ) {
		__xrtTypedDictError(XERR_UNSUPPORTED, XTYPED_DICT_ERROR_TYPE,
			"equals", "the dictionary item type is not comparable");
		return false;
	}
	if ( !__xrtMapProtectRead(&pLeft->Storage, &bLeftProtected) ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			"equals", "the left dictionary could not be protected");
		return false;
	}
	if ( !__xrtMapProtectRead(&pRight->Storage, &bRightProtected) ) {
		__xrtMapUnprotectRead(&pLeft->Storage, bLeftProtected);
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			"equals", "the right dictionary could not be protected");
		return false;
	}
	if ( !xrtMapIterBegin((xmap*)&pLeft->Storage, &Iterator) ) {
		bEqual = false;
		bFailed = true;
	} else {
		while ( (pLeftValue = xrtMapIterNext(&Iterator, &Key)) != NULL ) {
			pRightValue = xrtMapConstGet(&pRight->Storage, Key);
			if ( pRightValue == NULL ) {
				bEqual = false;
				break;
			}
			if ( !__xrtMapCallbackBegin(&pLeft->Storage) ) {
				bEqual = false;
				bFailed = true;
				break;
			}
			if ( !__xrtMapCallbackBegin(&pRight->Storage) ) {
				__xrtMapCallbackEnd(&pLeft->Storage);
				bEqual = false;
				bFailed = true;
				break;
			}
			if ( !xrtTypeCompareValue(
				pLeft->ItemType,
				pLeftValue,
				pRightValue,
				&iCompare
			) ) {
				__xrtMapCallbackEnd(&pRight->Storage);
				__xrtMapCallbackEnd(&pLeft->Storage);
				bEqual = false;
				bFailed = true;
				break;
			}
			__xrtMapCallbackEnd(&pRight->Storage);
			__xrtMapCallbackEnd(&pLeft->Storage);
			if ( iCompare != 0 ) {
				bEqual = false;
				break;
			}
		}
		xrtMapIterEnd(&Iterator);
	}
	__xrtMapUnprotectRead(&pRight->Storage, bRightProtected);
	__xrtMapUnprotectRead(&pLeft->Storage, bLeftProtected);
	if ( bFailed ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			"equals", "the dictionary values could not be compared");
	}
	return bEqual;
}



/* 初始化对象负载中的类型字典。 */
static bool __xrtTypedDictInstanceInit(
	ptr pInstance,
	const xrttype* pType
)
{
	if ( !xrtTypedDictTypeValidate(pType) ) {
		return false;
	}
	return xrtTypedDictInit(
		(xtypeddict*)pInstance, pType->Arguments[0]
	);
}



/* 销毁对象负载中的类型字典。 */
static void __xrtTypedDictInstanceDrop(
	ptr pInstance,
	const xrttype* pType
)
{
	(void)pType;
	xrtTypedDictUnit((xtypeddict*)pInstance);
}



/* 类型字典追踪适配器保存对象访问器和失败状态。 */
typedef struct xtypeddicttracecontext {
	const xtypeddict* Dict;
	const xrttype* ItemType;
	xrtobjectvisitor Visit;
	ptr UserData;
	bool Failed;
} xtypeddicttracecontext;



/* 枚举一个字典值直接拥有的强对象引用。 */
static bool __xrtTypedDictTraceValue(
	xbytesview Key,
	ptr pValue,
	ptr pUserData
)
{
	xtypeddicttracecontext* pContext =
		(xtypeddicttracecontext*)pUserData;
	bool bTraced;
	(void)Key;

	if ( !__xrtMapCallbackBegin(&pContext->Dict->Storage) ) {
		pContext->Failed = true;
		return false;
	}
	bTraced = xrtTypeTraceValue(
		pContext->ItemType,
		pValue,
		pContext->Visit,
		pContext->UserData
	);
	__xrtMapCallbackEnd(&pContext->Dict->Storage);
	if ( !bTraced ) {
		pContext->Failed = true;
		return false;
	}
	return true;
}



/* 枚举类型字典所有值直接拥有的强对象引用。 */
static bool __xrtTypedDictInstanceTrace(
	const void* pInstance,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	xtypeddict* pDict = (xtypeddict*)pInstance;
	xtypeddicttracecontext Context;
	size_t iExpected;
	size_t iVisited;
	(void)pType;

	if ( !__xrtTypedDictCanRead(pDict, "instance-trace") ) {
		return false;
	}
	Context.Dict = pDict;
	Context.ItemType = pDict->ItemType;
	Context.Visit = pVisit;
	Context.UserData = pContext;
	Context.Failed = false;
	iExpected = pDict->Storage.Count;
	iVisited = xrtMapVisit(
		&pDict->Storage, __xrtTypedDictTraceValue, &Context
	);
	if ( Context.Failed ) {
		return false;
	}
	if ( iVisited != iExpected ) {
		__xrtTypedDictWrap(XERR_STATE, XTYPED_DICT_ERROR_STATE,
			"instance-trace", "the typed dictionary trace visit was incomplete");
		return false;
	}
	return true;
}



/* 返回对象字典负载共享的实例操作表。 */
const xrtinstanceops __xrtTypedDictInstanceOperations = {
	.Init = __xrtTypedDictInstanceInit,
	.Drop = __xrtTypedDictInstanceDrop,
	.Trace = __xrtTypedDictInstanceTrace
};



/* 返回对象字典载荷共享的实例操作表。 */
XRT_API const xrtinstanceops* xrtTypedDictInstanceOps(void)
{
	return &__xrtTypedDictInstanceOperations;
}



/* 验证可由对象系统承载的泛型字典类型描述。 */
XRT_API bool xrtTypedDictTypeValidate(const xrttype* pType)
{
	if ( !xrtTypeValidate(pType) ) {
		__xrtTypedDictWrap(XERR_ARGUMENT, XTYPED_DICT_ERROR_TYPE,
			"type-validate", "the typed dictionary object type is invalid");
		return false;
	}
	if (
		(pType->Kind != XRT_TYPE_DICT) ||
		((pType->Flags & XRT_TYPE_FLAG_REFERENCE) == 0u) ||
		(pType->ArgumentCount != 1u) ||
		(pType->Arguments == NULL) ||
		(pType->InstanceSize != sizeof(xtypeddict)) ||
		(pType->InstanceAlign <
		 XRT_INTERNAL_OBJECT_ALIGNOF(xtypeddict)) ||
		(pType->InstanceOps != xrtTypedDictInstanceOps())
	) {
		__xrtTypedDictError(XERR_TYPE, XTYPED_DICT_ERROR_TYPE,
			"type-validate", "the typed dictionary object type contract is invalid");
		return false;
	}
	return __xrtTypedDictItemTypeValidate(
		pType->Arguments[0], "type-validate"
	);
}

#endif
