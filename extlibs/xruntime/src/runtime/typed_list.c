#include "../internal/xrt_int_map.h"
#include "../internal/xrt_runtime_type.h"
#include "../internal/xrt_typed_container.h"
#include <xrt/typed_list.h>



#if defined(XRUNTIME_FEATURE_TYPED_LIST)

#define XRT_TYPED_LIST_FLAG_READY 0x0001u
#define XRT_TYPED_LIST_FLAG_BUSY  0x0002u
#define XRT_TYPED_LIST_FLAGS      0x0003u



/* 设置类型列表模块结构化错误。 */
static void __xrtTypedListError(
	xerrkind Kind,
	xtypedlisterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.typed-list";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为下层类型或整数映射错误补充类型列表上下文。 */
static void __xrtTypedListWrap(
	xerrkind DefaultKind,
	xtypedlisterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.typed-list";
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



/* 验证元素类型可由稳定节点列表安全拥有。 */
static bool __xrtTypedListItemTypeValidate(
	const xrttype* pItemType,
	cstr sOperation
)
{
	if ( !xrtTypeValidate(pItemType) ) {
		__xrtTypedListWrap(XERR_ARGUMENT, XTYPED_LIST_ERROR_TYPE,
			sOperation, "the list item type is invalid");
		return false;
	}
	if ( pItemType->Size == 0u ) {
		__xrtTypedListError(XERR_TYPE, XTYPED_LIST_ERROR_TYPE,
			sOperation, "a typed list item must occupy storage");
		return false;
	}
	if ( !xrtTypeIsCopyable(pItemType) ) {
		__xrtTypedListError(XERR_UNSUPPORTED, XTYPED_LIST_ERROR_TYPE,
			sOperation, "the list item type is not copyable");
		return false;
	}
	return true;
}



/* 销毁整数映射中一个完整初始化的类型值。 */
static void __xrtTypedListDrop(
	int64 iKey,
	ptr pValue,
	ptr pUserData
)
{
	xtypedlist* pList = (xtypedlist*)pUserData;
	(void)iKey;

	if ( (pList != NULL) && (pList->ItemType != NULL) ) {
		xrtTypeDropValue(pList->ItemType, pValue);
	}
}



/* 检查公开类型列表状态、布局和释放回调是否一致。 */
static bool __xrtTypedListValid(
	const xtypedlist* pList,
	cstr sOperation
)
{
	if ( (pList == NULL) || (pList->ItemType == NULL) ) {
		__xrtTypedListError(XERR_ARGUMENT, XTYPED_LIST_ERROR_ARGUMENT,
			sOperation, "the typed list is null or uninitialized");
		return false;
	}
	if (
		((pList->Flags & XRT_TYPED_LIST_FLAG_READY) == 0u) ||
		((pList->Flags & XRT_TYPED_LIST_FLAG_BUSY) != 0u) ||
		((pList->Flags & ~XRT_TYPED_LIST_FLAGS) != 0u)
	) {
		__xrtTypedListError(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			sOperation, "the typed list is not available for API access");
		return false;
	}
	if ( !__xrtIntMapValid(&pList->Storage) ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			sOperation, "the typed list storage is invalid");
		return false;
	}
	if ( (pList->Storage.ValueSize != pList->ItemType->Size) ||
		 (pList->Storage.Alignment < pList->ItemType->Align) ||
		 (pList->Storage.Drop != __xrtTypedListDrop) ||
		 (pList->Storage.UserData != pList) ) {
		__xrtTypedListError(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			sOperation, "the typed list item layout or ownership is invalid");
		return false;
	}
	return true;
}



/* 在用户类型回调期间拒绝当前列表的全部 API 重入。 */
void __xrtTypedListCallbackBegin(const xtypedlist* pList)
{
	((xtypedlist*)pList)->Flags |= XRT_TYPED_LIST_FLAG_BUSY;
}



/* 结束当前列表的用户类型回调门禁。 */
void __xrtTypedListCallbackEnd(const xtypedlist* pList)
{
	((xtypedlist*)pList)->Flags &= ~XRT_TYPED_LIST_FLAG_BUSY;
}



/* 检查类型列表当前是否允许结构和生命周期修改。 */
static bool __xrtTypedListCanMutate(
	const xtypedlist* pList,
	cstr sOperation
)
{
	if ( !__xrtTypedListValid(pList, sOperation) ) {
		return false;
	}
	if ( !__xrtIntMapCanMutate(&pList->Storage) ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			sOperation, "the typed list is currently being visited");
		return false;
	}
	return true;
}



/* 判断字节区间是否触及类型列表自身或拥有的节点池。 */
static bool __xrtTypedListOwnsRange(
	const xtypedlist* pList,
	const void* pMemory,
	size_t iSize
)
{
	return __xrtRangesOverlap(pMemory, iSize, pList, sizeof(*pList)) ||
		__xrtIntMapOwnsRange(&pList->Storage, pMemory, iSize);
}



/* 验证来源是外部值或列表中的准确活动值槽。 */
static bool __xrtTypedListSourceValid(
	const xtypedlist* pList,
	const void* pItem,
	cstr sOperation
)
{
	xintmapiter Iterator;
	ptr pValue;
	bool bExact = false;

	if ( pItem == NULL ) {
		__xrtTypedListError(XERR_ARGUMENT, XTYPED_LIST_ERROR_ARGUMENT,
			sOperation, "the source item is null");
		return false;
	}
	if ( !__xrtTypedListOwnsRange(
		pList, pItem, pList->ItemType->Size
	) ) {
		return true;
	}
	if ( !xrtIntMapIterBegin((xintmap*)&pList->Storage, &Iterator) ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			sOperation, "the typed list source scan could not start");
		return false;
	}
	while ( (pValue = xrtIntMapIterNext(&Iterator, NULL)) != NULL ) {
		if ( pValue == pItem ) {
			bExact = true;
			break;
		}
	}
	xrtIntMapIterEnd(&Iterator);
	if ( !bExact ) {
		__xrtTypedListError(XERR_ARGUMENT, XTYPED_LIST_ERROR_ARGUMENT,
			sOperation, "an internal source must be an active value boundary");
		return false;
	}
	return true;
}



/* 验证移动输出完全位于类型列表拥有的内存之外。 */
static bool __xrtTypedListOutputExternal(
	const xtypedlist* pList,
	const void* pValue,
	cstr sOperation
)
{
	if ( pValue == NULL ) {
		__xrtTypedListError(XERR_ARGUMENT, XTYPED_LIST_ERROR_ARGUMENT,
			sOperation, "the output value is null");
		return false;
	}
	if ( __xrtTypedListOwnsRange(
		pList, pValue, pList->ItemType->Size
	) ) {
		__xrtTypedListError(XERR_ARGUMENT, XTYPED_LIST_ERROR_ARGUMENT,
			sOperation, "the output value must not alias typed list storage");
		return false;
	}
	return true;
}



/* 新节点初始化上下文保存来源值和元素类型。 */
typedef struct xtypedlistinitcontext {
	xtypedlist* List;
	const void* Item;
} xtypedlistinitcontext;



/* 初始化并复制一个尚未提交的新列表值。 */
static bool __xrtTypedListInitValue(
	int64 iKey,
	ptr pValue,
	ptr pUserData
)
{
	xtypedlistinitcontext* pContext = (xtypedlistinitcontext*)pUserData;
	xerror* pError;
	(void)iKey;

	__xrtTypedListCallbackBegin(pContext->List);
	if ( !xrtTypeInitValue(pContext->List->ItemType, pValue) ) {
		__xrtTypedListCallbackEnd(pContext->List);
		return false;
	}
	if ( xrtTypeCopyValue(
		pContext->List->ItemType, pValue, pContext->Item
	) ) {
		__xrtTypedListCallbackEnd(pContext->List);
		return true;
	}
	pError = xrtTakeError();
	xrtTypeDropValue(pContext->List->ItemType, pValue);
	__xrtTypedListCallbackEnd(pContext->List);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return false;
}



/* 在调用方已经验证列表和来源后复制设置一个键值。 */
static bool __xrtTypedListSetReady(
	xtypedlist* pList,
	int64 iKey,
	const void* pItem,
	cstr sOperation
)
{
	xtypedlistinitcontext Context;
	ptr pStored;
	bool bNew;

	pStored = xrtIntMapGet(&pList->Storage, iKey);
	if ( pStored != NULL ) {
		__xrtTypedListCallbackBegin(pList);
		if ( !xrtTypeCopyValue(pList->ItemType, pStored, pItem) ) {
			__xrtTypedListCallbackEnd(pList);
			__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_OPERATION,
				sOperation, "the existing typed list item could not be replaced");
			return false;
		}
		__xrtTypedListCallbackEnd(pList);
		return true;
	}
	Context.List = pList;
	Context.Item = pItem;
	pStored = xrtIntMapGetOrInit(
		&pList->Storage,
		iKey,
		__xrtTypedListInitValue,
		&Context,
		&bNew
	);
	if ( (pStored == NULL) || !bNew ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_OPERATION,
			sOperation, "the new typed list item could not be inserted");
		return false;
	}
	return true;
}



/* 修复类型列表按值交换后保存的内部自引用。 */
static void __xrtTypedListRepair(xtypedlist* pList)
{
	pList->Storage.Tree.UserData = &pList->Storage;
	pList->Storage.UserData = pList;
}



/* 交换两个静止且有效的类型列表，并修复底层自引用。 */
static void __xrtTypedListSwap(
	xtypedlist* pLeft,
	xtypedlist* pRight
)
{
	xintmap Storage = pLeft->Storage;
	const xrttype* pItemType = pLeft->ItemType;

	pLeft->Storage = pRight->Storage;
	pLeft->ItemType = pRight->ItemType;
	pRight->Storage = Storage;
	pRight->ItemType = pItemType;
	__xrtTypedListRepair(pLeft);
	__xrtTypedListRepair(pRight);
}



/* 递增结构版本并跳过外置迭代器保留的零值。 */
static uint64 __xrtTypedListNextVersion(uint64 iVersion)
{
	iVersion++;
	return iVersion != 0u ? iVersion : 1u;
}



/* 初始化一个拥有类型值的空稀疏列表。 */
XRT_API bool xrtTypedListInit(
	xtypedlist* pList,
	const xrttype* pItemType
)
{
	if ( pList == NULL ) {
		__xrtTypedListError(XERR_ARGUMENT, XTYPED_LIST_ERROR_ARGUMENT,
			"init", "the typed list is null");
		return false;
	}
	memset(pList, 0, sizeof(*pList));
	if ( !__xrtTypedListItemTypeValidate(pItemType, "init") ) {
		return false;
	}
	if ( !xrtIntMapInitAligned(
		&pList->Storage, pItemType->Size, pItemType->Align
	) ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_OPERATION,
			"init", "the typed list storage could not be initialized");
		return false;
	}
	pList->ItemType = pItemType;
	if ( !xrtIntMapSetDrop(
		&pList->Storage, __xrtTypedListDrop, pList
	) ) {
		xrtIntMapUnit(&pList->Storage);
		memset(pList, 0, sizeof(*pList));
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_OPERATION,
			"init", "the typed list ownership callback could not be installed");
		return false;
	}
	pList->Flags = XRT_TYPED_LIST_FLAG_READY;
	return true;
}



/* 在堆上创建一个拥有类型值的空稀疏列表。 */
XRT_API xtypedlist* xrtTypedListCreate(const xrttype* pItemType)
{
	xtypedlist* pList = (xtypedlist*)xrtMalloc(sizeof(*pList));

	if ( pList == NULL ) {
		return NULL;
	}
	if ( !xrtTypedListInit(pList, pItemType) ) {
		xrtFree(pList);
		return NULL;
	}
	return pList;
}



/* 释放全部元素和节点池，但不释放列表结构。 */
XRT_API void xrtTypedListUnit(xtypedlist* pList)
{
	if ( pList == NULL ) {
		return;
	}
	if ( (pList->ItemType == NULL) && (pList->Flags == 0u) ) {
		return;
	}
	if ( !__xrtTypedListCanMutate(pList, "unit") ) {
		return;
	}
	xrtIntMapUnit(&pList->Storage);
	memset(pList, 0, sizeof(*pList));
}



/* 释放全部元素、节点池和堆列表结构。 */
XRT_API void xrtTypedListDestroy(xtypedlist* pList)
{
	if ( pList == NULL ) {
		return;
	}
	if ( !__xrtTypedListCanMutate(pList, "destroy") ) {
		return;
	}
	xrtTypedListUnit(pList);
	xrtFree(pList);
}



/* 返回列表借用的元素类型描述。 */
XRT_API const xrttype* xrtTypedListItemType(const xtypedlist* pList)
{
	return __xrtTypedListValid(pList, "item-type") ? pList->ItemType : NULL;
}



/* 返回列表当前键值数量。 */
XRT_API size_t xrtTypedListCount(const xtypedlist* pList)
{
	return __xrtTypedListValid(pList, "count") ?
		xrtIntMapCount(&pList->Storage) : 0u;
}



/* 释放全部值并保留节点池供复用。 */
XRT_API bool xrtTypedListClear(xtypedlist* pList)
{
	if ( !__xrtTypedListCanMutate(pList, "clear") ) {
		return false;
	}
	xrtIntMapClear(&pList->Storage);
	return true;
}



/* 释放空闲节点池页并返回实际释放页数。 */
XRT_API size_t xrtTypedListTrim(xtypedlist* pList, size_t iRetainEmpty)
{
	if ( !__xrtTypedListCanMutate(pList, "trim") ) {
		return 0u;
	}
	return xrtIntMapTrim(&pList->Storage, iRetainEmpty);
}



/* 复制插入或失败原子地替换指定键的类型值。 */
XRT_API bool xrtTypedListSet(
	xtypedlist* pList,
	int64 iKey,
	const void* pItem
)
{
	if ( !__xrtTypedListCanMutate(pList, "set") ||
		 !__xrtTypedListSourceValid(pList, pItem, "set") ) {
		return false;
	}
	return __xrtTypedListSetReady(pList, iKey, pItem, "set");
}



/* 在当前最大键之后复制追加一个值。 */
XRT_API bool xrtTypedListAppend(
	xtypedlist* pList,
	const void* pItem,
	int64* pKey
)
{
	int64 iKey = 0;

	if ( pKey != NULL ) {
		*pKey = 0;
	}
	if ( !__xrtTypedListCanMutate(pList, "append") ||
		 !__xrtTypedListSourceValid(pList, pItem, "append") ) {
		return false;
	}
	if ( xrtIntMapCount(&pList->Storage) != 0u ) {
		if ( xrtIntMapLast(&pList->Storage, &iKey) == NULL ) {
			__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
				"append", "the typed list maximum key could not be read");
			return false;
		}
		if ( iKey == INT64_MAX ) {
			__xrtTypedListError(XERR_RANGE, XTYPED_LIST_ERROR_KEY,
				"append", "the typed list append key overflows");
			return false;
		}
		iKey++;
	}
	if ( !__xrtTypedListSetReady(pList, iKey, pItem, "append") ) {
		return false;
	}
	if ( pKey != NULL ) {
		*pKey = iKey;
	}
	return true;
}



/* 返回指定键的可写借用值槽。 */
XRT_API ptr xrtTypedListGet(xtypedlist* pList, int64 iKey)
{
	return __xrtTypedListValid(pList, "get") ?
		xrtIntMapGet(&pList->Storage, iKey) : NULL;
}



/* 返回指定键的只读借用值槽。 */
XRT_API const void* xrtTypedListConstGet(
	const xtypedlist* pList,
	int64 iKey
)
{
	return __xrtTypedListValid(pList, "const-get") ?
		xrtIntMapConstGet(&pList->Storage, iKey) : NULL;
}



/* 判断指定整数键是否存在。 */
XRT_API bool xrtTypedListHas(const xtypedlist* pList, int64 iKey)
{
	return __xrtTypedListValid(pList, "has") &&
		xrtIntMapHas(&pList->Storage, iKey);
}



/* 从较近的一端按键顺序取得指定位置的借用值槽。 */
static ptr __xrtTypedListAt(
	xtypedlist* pList,
	size_t iIndex,
	int64* pKey,
	cstr sOperation
)
{
	xintmapiter Iterator;
	size_t iCount;
	size_t iSteps;
	ptr pValue = NULL;
	bool bReverse;

	if ( pKey != NULL ) {
		*pKey = 0;
	}
	if ( !__xrtTypedListValid(pList, sOperation) ) {
		return NULL;
	}
	iCount = xrtIntMapCount(&pList->Storage);
	if ( iIndex >= iCount ) {
		return NULL;
	}
	bReverse = iIndex > (iCount / 2u);
	iSteps = bReverse ? (iCount - iIndex - 1u) : iIndex;
	if ( !(bReverse ?
		xrtIntMapIterRBegin(&pList->Storage, &Iterator) :
		xrtIntMapIterBegin(&pList->Storage, &Iterator)) ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			sOperation, "the typed list position iterator could not start");
		return NULL;
	}
	for ( size_t i = 0u; i <= iSteps; i++ ) {
		pValue = xrtIntMapIterNext(&Iterator, pKey);
	}
	xrtIntMapIterEnd(&Iterator);
	if ( pValue == NULL ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			sOperation, "the typed list position could not be reached");
	}
	return pValue;
}



/* 按键顺序返回指定位置的可写借用值槽。 */
XRT_API ptr xrtTypedListAt(
	xtypedlist* pList,
	size_t iIndex,
	int64* pKey
)
{
	return __xrtTypedListAt(pList, iIndex, pKey, "at");
}



/* 按键顺序返回指定位置的只读借用值槽。 */
XRT_API const void* xrtTypedListConstAt(
	const xtypedlist* pList,
	size_t iIndex,
	int64* pKey
)
{
	return __xrtTypedListAt(
		(xtypedlist*)pList, iIndex, pKey, "const-at"
	);
}



/* 删除指定键并销毁其值。 */
XRT_API bool xrtTypedListRemove(xtypedlist* pList, int64 iKey)
{
	if ( !__xrtTypedListCanMutate(pList, "remove") ) {
		return false;
	}
	return xrtIntMapRemove(&pList->Storage, iKey);
}



/* 把值移动到外部已初始化输出后删除指定键。 */
XRT_API bool xrtTypedListTake(
	xtypedlist* pList,
	int64 iKey,
	ptr pValue
)
{
	ptr pStored;

	if ( !__xrtTypedListCanMutate(pList, "take") ||
		 !__xrtTypedListOutputExternal(pList, pValue, "take") ) {
		return false;
	}
	pStored = xrtIntMapGet(&pList->Storage, iKey);
	if ( pStored == NULL ) {
		return false;
	}
	__xrtTypedListCallbackBegin(pList);
	if ( !xrtTypeMoveValue(pList->ItemType, pValue, pStored) ) {
		__xrtTypedListCallbackEnd(pList);
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_OPERATION,
			"take", "the typed list item could not be moved");
		return false;
	}
	__xrtTypedListCallbackEnd(pList);
	if ( !xrtIntMapRemove(&pList->Storage, iKey) ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			"take", "the moved typed list item could not be removed");
		return false;
	}
	return true;
}



/* 查找按键顺序出现的第一个相等值。 */
XRT_API bool xrtTypedListFind(
	const xtypedlist* pList,
	const void* pItem,
	int64* pKey
)
{
	xintmapiter Iterator;
	ptr pValue;
	int64 iKey;
	int iCompare;
	uint64 iVersion;

	if ( pKey != NULL ) {
		*pKey = 0;
	}
	if ( !__xrtTypedListValid(pList, "find") ||
		 !__xrtTypedListSourceValid(pList, pItem, "find") ) {
		return false;
	}
	if ( !xrtTypeIsComparable(pList->ItemType) ) {
		__xrtTypedListError(XERR_UNSUPPORTED, XTYPED_LIST_ERROR_TYPE,
			"find", "the list item type is not comparable");
		return false;
	}
	if ( !xrtIntMapIterBegin((xintmap*)&pList->Storage, &Iterator) ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			"find", "the typed list iterator could not start");
		return false;
	}
	__xrtTypedListCallbackBegin(pList);
	iVersion = pList->Storage.Tree.Base.Version;
	while ( (pValue = xrtIntMapIterNext(&Iterator, &iKey)) != NULL ) {
		if ( !xrtTypeCompareValue(
			pList->ItemType, pValue, pItem, &iCompare
		) ) {
			xrtIntMapIterEnd(&Iterator);
			__xrtTypedListCallbackEnd(pList);
			__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_OPERATION,
				"find", "typed list item comparison failed");
			return false;
		}
		if ( pList->Storage.Tree.Base.Version != iVersion ) {
			xrtIntMapIterEnd(&Iterator);
			__xrtTypedListCallbackEnd(pList);
			__xrtTypedListError(XERR_STATE, XTYPED_LIST_ERROR_STATE,
				"find", "the typed list changed during item comparison");
			return false;
		}
		if ( iCompare == 0 ) {
			xrtIntMapIterEnd(&Iterator);
			__xrtTypedListCallbackEnd(pList);
			if ( pKey != NULL ) {
				*pKey = iKey;
			}
			return true;
		}
	}
	xrtIntMapIterEnd(&Iterator);
	__xrtTypedListCallbackEnd(pList);
	return false;
}



/* 判断列表中是否包含相等值。 */
XRT_API bool xrtTypedListContains(
	const xtypedlist* pList,
	const void* pItem
)
{
	return xrtTypedListFind(pList, pItem, NULL);
}



/* 把来源全部键值复制到已经初始化的空目标。 */
static bool __xrtTypedListCopyInto(
	xtypedlist* pTarget,
	const xtypedlist* pSource,
	cstr sOperation
)
{
	xintmapiter Iterator;
	ptr pValue;
	int64 iKey;
	uint64 iVersion;

	if ( !xrtIntMapIterBegin((xintmap*)&pSource->Storage, &Iterator) ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			sOperation, "the source typed list iterator could not start");
		return false;
	}
	__xrtTypedListCallbackBegin(pSource);
	iVersion = pSource->Storage.Tree.Base.Version;
	while ( (pValue = xrtIntMapIterNext(&Iterator, &iKey)) != NULL ) {
		if ( !__xrtTypedListSetReady(
			pTarget, iKey, pValue, sOperation
		) ) {
			xrtIntMapIterEnd(&Iterator);
			__xrtTypedListCallbackEnd(pSource);
			return false;
		}
		if ( pSource->Storage.Tree.Base.Version != iVersion ) {
			xrtIntMapIterEnd(&Iterator);
			__xrtTypedListCallbackEnd(pSource);
			__xrtTypedListError(XERR_STATE, XTYPED_LIST_ERROR_STATE,
				sOperation, "the source typed list changed while being copied");
			return false;
		}
	}
	xrtIntMapIterEnd(&Iterator);
	__xrtTypedListCallbackEnd(pSource);
	return true;
}



/* 失败原子地合并同类型列表。 */
XRT_API bool xrtTypedListMerge(
	xtypedlist* pTarget,
	const xtypedlist* pSource,
	bool bReplace
)
{
	xtypedlist Work = { 0 };
	xintmapiter Iterator;
	ptr pValue;
	int64 iKey;
	uint64 iVersion;
	uint64 iTargetVersion;

	if ( !__xrtTypedListCanMutate(pTarget, "merge") ||
		 !__xrtTypedListValid(pSource, "merge") ) {
		return false;
	}
	if ( !xrtTypeSame(pTarget->ItemType, pSource->ItemType) ) {
		__xrtTypedListError(XERR_TYPE, XTYPED_LIST_ERROR_TYPE,
			"merge", "typed lists have different item types");
		return false;
	}
	if ( pTarget == pSource ) {
		return true;
	}
	if ( !xrtTypedListInit(&Work, pTarget->ItemType) ||
		 !__xrtTypedListCopyInto(&Work, pTarget, "merge") ) {
		if ( Work.ItemType != NULL ) {
			xrtTypedListUnit(&Work);
		}
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_OPERATION,
			"merge", "the typed list merge snapshot failed");
		return false;
	}
	if ( !xrtIntMapIterBegin((xintmap*)&pSource->Storage, &Iterator) ) {
		xrtTypedListUnit(&Work);
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			"merge", "the source typed list iterator could not start");
		return false;
	}
	__xrtTypedListCallbackBegin(pTarget);
	__xrtTypedListCallbackBegin(pSource);
	iVersion = pSource->Storage.Tree.Base.Version;
	while ( (pValue = xrtIntMapIterNext(&Iterator, &iKey)) != NULL ) {
		if ( !bReplace && xrtTypedListHas(&Work, iKey) ) {
			continue;
		}
		if ( !__xrtTypedListSetReady(
			&Work, iKey, pValue, "merge"
		) ) {
			xerror* pError = xrtTakeError();

			xrtIntMapIterEnd(&Iterator);
			xrtTypedListUnit(&Work);
			__xrtTypedListCallbackEnd(pSource);
			__xrtTypedListCallbackEnd(pTarget);
			if ( pError != NULL ) {
				xrtSetError(pError);
				xrtErrorFree(pError);
			}
			__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_OPERATION,
				"merge", "a source typed list item could not be merged");
			return false;
		}
		if ( pSource->Storage.Tree.Base.Version != iVersion ) {
			xrtIntMapIterEnd(&Iterator);
			xrtTypedListUnit(&Work);
			__xrtTypedListCallbackEnd(pSource);
			__xrtTypedListCallbackEnd(pTarget);
			__xrtTypedListError(XERR_STATE, XTYPED_LIST_ERROR_STATE,
				"merge", "the source typed list changed while being merged");
			return false;
		}
	}
	xrtIntMapIterEnd(&Iterator);
	__xrtTypedListCallbackEnd(pSource);
	__xrtTypedListCallbackEnd(pTarget);
	iTargetVersion = pTarget->Storage.Tree.Base.Version;
	__xrtTypedListSwap(pTarget, &Work);
	pTarget->Storage.Tree.Base.Version =
		__xrtTypedListNextVersion(iTargetVersion);
	xrtTypedListUnit(&Work);
	return true;
}



/* 深复制一个独立堆类型列表。 */
XRT_API xtypedlist* xrtTypedListClone(const xtypedlist* pList)
{
	xtypedlist* pClone;

	if ( !__xrtTypedListValid(pList, "clone") ) {
		return NULL;
	}
	pClone = xrtTypedListCreate(pList->ItemType);
	if ( pClone == NULL ) {
		__xrtTypedListWrap(XERR_MEMORY, XTYPED_LIST_ERROR_OPERATION,
			"clone", "the typed list clone could not be created");
		return NULL;
	}
	if ( !__xrtTypedListCopyInto(pClone, pList, "clone") ) {
		xrtTypedListDestroy(pClone);
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_OPERATION,
			"clone", "the typed list clone could not be populated");
		return NULL;
	}
	return pClone;
}



/* 比较两个列表的类型、键集合和值内容。 */
XRT_API bool xrtTypedListEquals(
	const xtypedlist* pLeft,
	const xtypedlist* pRight
)
{
	xintmapiter Iterator;
	ptr pLeftValue;
	const void* pRightValue;
	int64 iKey;
	int iCompare;
	uint64 iLeftVersion;
	uint64 iRightVersion;

	if ( !__xrtTypedListValid(pLeft, "equals") ||
		 !__xrtTypedListValid(pRight, "equals") ) {
		return false;
	}
	if ( pLeft == pRight ) {
		return true;
	}
	if ( !xrtTypeSame(pLeft->ItemType, pRight->ItemType) ||
		 (xrtIntMapCount(&pLeft->Storage) !=
		  xrtIntMapCount(&pRight->Storage)) ) {
		return false;
	}
	if ( !xrtTypeIsComparable(pLeft->ItemType) ) {
		__xrtTypedListError(XERR_UNSUPPORTED, XTYPED_LIST_ERROR_TYPE,
			"equals", "the list item type is not comparable");
		return false;
	}
	if ( !xrtIntMapIterBegin((xintmap*)&pLeft->Storage, &Iterator) ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			"equals", "the typed list iterator could not start");
		return false;
	}
	__xrtTypedListCallbackBegin(pLeft);
	__xrtTypedListCallbackBegin(pRight);
	iLeftVersion = pLeft->Storage.Tree.Base.Version;
	iRightVersion = pRight->Storage.Tree.Base.Version;
	while ( (pLeftValue = xrtIntMapIterNext(&Iterator, &iKey)) != NULL ) {
		pRightValue = xrtIntMapConstGet(&pRight->Storage, iKey);
		if ( pRightValue == NULL ) {
			xrtIntMapIterEnd(&Iterator);
			__xrtTypedListCallbackEnd(pRight);
			__xrtTypedListCallbackEnd(pLeft);
			return false;
		}
		if ( !xrtTypeCompareValue(
			pLeft->ItemType, pLeftValue, pRightValue, &iCompare
		) ) {
			xrtIntMapIterEnd(&Iterator);
			__xrtTypedListCallbackEnd(pRight);
			__xrtTypedListCallbackEnd(pLeft);
			__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_OPERATION,
				"equals", "typed list item comparison failed");
			return false;
		}
		if ( (pLeft->Storage.Tree.Base.Version != iLeftVersion) ||
			 (pRight->Storage.Tree.Base.Version != iRightVersion) ) {
			xrtIntMapIterEnd(&Iterator);
			__xrtTypedListCallbackEnd(pRight);
			__xrtTypedListCallbackEnd(pLeft);
			__xrtTypedListError(XERR_STATE, XTYPED_LIST_ERROR_STATE,
				"equals", "a typed list changed during item comparison");
			return false;
		}
		if ( iCompare != 0 ) {
			xrtIntMapIterEnd(&Iterator);
			__xrtTypedListCallbackEnd(pRight);
			__xrtTypedListCallbackEnd(pLeft);
			return false;
		}
	}
	xrtIntMapIterEnd(&Iterator);
	__xrtTypedListCallbackEnd(pRight);
	__xrtTypedListCallbackEnd(pLeft);
	return true;
}



/* 以指定底层起点初始化类型列表迭代器。 */
static bool __xrtTypedListIterStart(
	xtypedlist* pList,
	xtypedlistiter* pIterator,
	int iDirection,
	bool bBounded,
	int64 iKey,
	cstr sOperation
)
{
	bool bSuccess;

	if ( pIterator != NULL ) {
		memset(pIterator, 0, sizeof(*pIterator));
	}
	if ( (pIterator == NULL) || !__xrtTypedListValid(pList, sOperation) ) {
		if ( pIterator == NULL ) {
			__xrtTypedListError(XERR_ARGUMENT, XTYPED_LIST_ERROR_ARGUMENT,
				sOperation, "the typed list iterator is null");
		}
		return false;
	}
	if ( iDirection > 0 ) {
		bSuccess = bBounded ?
			xrtIntMapIterFrom(&pList->Storage, iKey, &pIterator->Base) :
			xrtIntMapIterBegin(&pList->Storage, &pIterator->Base);
	} else {
		bSuccess = bBounded ?
			xrtIntMapIterRFrom(&pList->Storage, iKey, &pIterator->Base) :
			xrtIntMapIterRBegin(&pList->Storage, &pIterator->Base);
	}
	if ( !bSuccess ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			sOperation, "the typed list iterator could not start");
		return false;
	}
	pIterator->List = pList;
	return true;
}



/* 启动按键升序的完整迭代。 */
XRT_API bool xrtTypedListIterBegin(
	xtypedlist* pList,
	xtypedlistiter* pIterator
)
{
	return __xrtTypedListIterStart(
		pList, pIterator, 1, false, 0, "iter-begin"
	);
}



/* 启动按键降序的完整迭代。 */
XRT_API bool xrtTypedListIterRBegin(
	xtypedlist* pList,
	xtypedlistiter* pIterator
)
{
	return __xrtTypedListIterStart(
		pList, pIterator, -1, false, 0, "iter-rbegin"
	);
}



/* 从第一个不小于边界的键开始升序迭代。 */
XRT_API bool xrtTypedListIterFrom(
	xtypedlist* pList,
	int64 iKey,
	xtypedlistiter* pIterator
)
{
	return __xrtTypedListIterStart(
		pList, pIterator, 1, true, iKey, "iter-from"
	);
}



/* 从第一个不大于边界的键开始降序迭代。 */
XRT_API bool xrtTypedListIterRFrom(
	xtypedlist* pList,
	int64 iKey,
	xtypedlistiter* pIterator
)
{
	return __xrtTypedListIterStart(
		pList, pIterator, -1, true, iKey, "iter-rfrom"
	);
}



/* 返回下一借用值槽及其整数键。 */
XRT_API ptr xrtTypedListIterNext(
	xtypedlistiter* pIterator,
	int64* pKey
)
{
	ptr pValue;

	if ( pKey != NULL ) {
		*pKey = 0;
	}
	if ( (pIterator == NULL) || (pIterator->List == NULL) ) {
		if ( pIterator == NULL ) {
			__xrtTypedListError(XERR_ARGUMENT, XTYPED_LIST_ERROR_ARGUMENT,
				"iter-next", "the typed list iterator is null");
		}
		return NULL;
	}
	if ( pIterator->Base.Base.Base.Version !=
		 pIterator->List->Storage.Tree.Base.Version ) {
		xrtIntMapIterEnd(&pIterator->Base);
		pIterator->List = NULL;
		__xrtTypedListError(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			"iter-next", "the typed list changed during iteration");
		return NULL;
	}
	pValue = xrtIntMapIterNext(&pIterator->Base, pKey);
	if ( pValue == NULL ) {
		pIterator->List = NULL;
	}
	return pValue;
}



/* 提前结束迭代并清除全部借用状态。 */
XRT_API void xrtTypedListIterEnd(xtypedlistiter* pIterator)
{
	if ( pIterator == NULL ) {
		return;
	}
	xrtIntMapIterEnd(&pIterator->Base);
	pIterator->List = NULL;
}



/* 初始化对象负载中的类型列表。 */
static bool __xrtTypedListInstanceInit(
	ptr pInstance,
	const xrttype* pType
)
{
	if ( !xrtTypedListTypeValidate(pType) ) {
		return false;
	}
	return xrtTypedListInit(
		(xtypedlist*)pInstance, pType->Arguments[0]
	);
}



/* 销毁对象负载中的类型列表。 */
static void __xrtTypedListInstanceDrop(
	ptr pInstance,
	const xrttype* pType
)
{
	(void)pType;
	xrtTypedListUnit((xtypedlist*)pInstance);
}



/* 类型列表追踪适配器上下文保存对象访问器和失败状态。 */
typedef struct xtypedlisttracecontext {
	const xrttype* ItemType;
	xrtobjectvisitor Visit;
	ptr UserData;
	bool Failed;
} xtypedlisttracecontext;



/* 枚举一个列表值直接拥有的强对象引用。 */
static bool __xrtTypedListTraceValue(
	int64 iKey,
	ptr pValue,
	ptr pUserData
)
{
	xtypedlisttracecontext* pContext =
		(xtypedlisttracecontext*)pUserData;
	(void)iKey;

	if ( !xrtTypeTraceValue(
		pContext->ItemType,
		pValue,
		pContext->Visit,
		pContext->UserData
	) ) {
		pContext->Failed = true;
		return false;
	}
	return true;
}



/* 枚举类型列表所有值直接拥有的强对象引用。 */
static bool __xrtTypedListInstanceTrace(
	const void* pInstance,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	xtypedlist* pList = (xtypedlist*)pInstance;
	xtypedlisttracecontext Context;
	size_t iExpected;
	size_t iVisited;
	(void)pType;

	if ( !__xrtTypedListValid(pList, "instance-trace") ) {
		return false;
	}
	Context.ItemType = pList->ItemType;
	Context.Visit = pVisit;
	Context.UserData = pContext;
	Context.Failed = false;
	iExpected = xrtIntMapCount(&pList->Storage);
	__xrtTypedListCallbackBegin(pList);
	iVisited = xrtIntMapVisit(
		&pList->Storage, __xrtTypedListTraceValue, &Context
	);
	__xrtTypedListCallbackEnd(pList);
	if ( Context.Failed ) {
		return false;
	}
	if ( iVisited != iExpected ) {
		__xrtTypedListWrap(XERR_STATE, XTYPED_LIST_ERROR_STATE,
			"instance-trace", "the typed list trace visit was incomplete");
		return false;
	}
	return true;
}



/* 返回对象列表负载共享的实例操作表。 */
XRT_API const xrtinstanceops* xrtTypedListInstanceOps(void)
{
	static const xrtinstanceops Ops = {
		.Init = __xrtTypedListInstanceInit,
		.Drop = __xrtTypedListInstanceDrop,
		.Trace = __xrtTypedListInstanceTrace
	};

	return &Ops;
}



/* 验证可由对象系统承载的泛型列表类型描述。 */
XRT_API bool xrtTypedListTypeValidate(const xrttype* pType)
{
	if ( !xrtTypeValidate(pType) ) {
		__xrtTypedListWrap(XERR_ARGUMENT, XTYPED_LIST_ERROR_TYPE,
			"type-validate", "the typed list object type is invalid");
		return false;
	}
	if (
		(pType->Kind != XRT_TYPE_LIST) ||
		((pType->Flags & XRT_TYPE_FLAG_REFERENCE) == 0u) ||
		(pType->ArgumentCount != 1u) ||
		(pType->Arguments == NULL) ||
		(pType->InstanceSize != sizeof(xtypedlist)) ||
		(pType->InstanceAlign <
		 XRT_INTERNAL_OBJECT_ALIGNOF(xtypedlist)) ||
		(pType->InstanceOps != xrtTypedListInstanceOps())
	) {
		__xrtTypedListError(XERR_TYPE, XTYPED_LIST_ERROR_TYPE,
			"type-validate", "the typed list object type contract is invalid");
		return false;
	}
	return __xrtTypedListItemTypeValidate(
		pType->Arguments[0], "type-validate"
	);
}

#endif
