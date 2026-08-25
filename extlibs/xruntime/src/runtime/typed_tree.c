#include "../internal/xrt_avl.h"
#include "../internal/xrt_runtime_type.h"

#include <xrt/typed_tree.h>



#if defined(XRUNTIME_FEATURE_TYPED_TREE)

#define XRT_TYPED_TREE_FLAG_READY 0x0001u
#define XRT_TYPED_TREE_FLAG_BUSY  0x0002u
#define XRT_TYPED_TREE_FLAGS      0x0003u



/* 设置类型树模块结构化错误。 */
static void __xrtTypedTreeError(
	xerrkind Kind,
	xtypedtreeerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.typed-tree";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为下层类型、AVL 或节点池错误补充类型树上下文。 */
static void __xrtTypedTreeWrap(
	xerrkind DefaultKind,
	xtypedtreeerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.typed-tree";
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



/* 在清理资源后恢复进入清理阶段时持有的根错误。 */
static void __xrtTypedTreeRestoreError(xerror* pError)
{
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 验证键或值类型能够由类型树安全拥有。 */
static bool __xrtTypedTreeValueTypeValidate(
	const xrttype* pType,
	bool bKey,
	cstr sOperation
)
{
	if ( !xrtTypeValidate(pType) ) {
		__xrtTypedTreeWrap(XERR_ARGUMENT, XTYPED_TREE_ERROR_TYPE,
			sOperation, bKey ? "the tree key type is invalid" :
			"the tree value type is invalid");
		return false;
	}
	if ( pType->Size == 0u ) {
		__xrtTypedTreeError(XERR_TYPE, XTYPED_TREE_ERROR_TYPE,
			sOperation, bKey ? "a tree key must occupy storage" :
			"a tree value must occupy storage");
		return false;
	}
	if ( !xrtTypeIsCopyable(pType) ) {
		__xrtTypedTreeError(XERR_UNSUPPORTED, XTYPED_TREE_ERROR_TYPE,
			sOperation, bKey ? "the tree key type is not copyable" :
			"the tree value type is not copyable");
		return false;
	}
	if ( bKey && !xrtTypeIsComparable(pType) ) {
		__xrtTypedTreeError(XERR_UNSUPPORTED, XTYPED_TREE_ERROR_TYPE,
			sOperation, "the tree key type is not comparable");
		return false;
	}
	return true;
}



/* 向上对齐布局偏移并检查大小溢出。 */
static bool __xrtTypedTreeAlign(
	size_t iValue,
	size_t iAlignment,
	size_t* pResult
)
{
	size_t iMask = iAlignment - 1u;

	if ( iValue > (SIZE_MAX - iMask) ) {
		return false;
	}
	*pResult = (iValue + iMask) & ~iMask;
	return true;
}



/* 计算键和值在一个节点对象中的紧凑对齐布局。 */
static bool __xrtTypedTreeLayout(
	const xrttype* pKeyType,
	const xrttype* pValueType,
	size_t* pValueOffset,
	size_t* pEntrySize,
	size_t* pAlignment
)
{
	size_t iOffset;
	size_t iAlignment = pKeyType->Align > pValueType->Align ?
		pKeyType->Align : pValueType->Align;

	if ( iAlignment < XRT_POOL_ALIGNMENT_DEFAULT ) {
		iAlignment = XRT_POOL_ALIGNMENT_DEFAULT;
	}
	if ( !__xrtTypedTreeAlign(
		pKeyType->Size, pValueType->Align, &iOffset
	) || (pValueType->Size > (SIZE_MAX - iOffset)) ) {
		__xrtTypedTreeError(XERR_RANGE, XTYPED_TREE_ERROR_LAYOUT,
			"init", "the typed tree entry layout overflows");
		return false;
	}
	*pValueOffset = iOffset;
	*pEntrySize = iOffset + pValueType->Size;
	*pAlignment = iAlignment;
	return true;
}



/* 在用户类型回调期间拒绝当前树的全部公开 API，并保留外层门禁状态。 */
static bool __xrtTypedTreeCallbackBegin(const xtypedtree* pTree)
{
	bool bBusy = (pTree->Flags & XRT_TYPED_TREE_FLAG_BUSY) != 0u;

	((xtypedtree*)pTree)->Flags |= XRT_TYPED_TREE_FLAG_BUSY;
	return bBusy;
}



/* 结束当前树的用户类型回调门禁，并恢复进入前的嵌套状态。 */
static void __xrtTypedTreeCallbackEnd(const xtypedtree* pTree, bool bBusy)
{
	if ( !bBusy ) {
		((xtypedtree*)pTree)->Flags &= ~XRT_TYPED_TREE_FLAG_BUSY;
	}
}



/* 返回节点对象中的键槽。 */
static ptr __xrtTypedTreeKey(const xtypedtree* pTree, const void* pEntry)
{
	return pEntry != NULL ?
		(ptr)((const bytes)pEntry + pTree->KeyOffset) : NULL;
}



/* 返回节点对象中的值槽。 */
static ptr __xrtTypedTreeValue(const xtypedtree* pTree, const void* pEntry)
{
	return pEntry != NULL ?
		(ptr)((const bytes)pEntry + pTree->ValueOffset) : NULL;
}



/* 使用运行时键类型比较查询键和节点规范键。 */
static int __xrtTypedTreeCompare(
	const void* pKey,
	const void* pEntry,
	ptr pUserData
)
{
	xtypedtree* pTree = (xtypedtree*)pUserData;
	int iCompare = 0;
	bool bBusy;

	bBusy = __xrtTypedTreeCallbackBegin(pTree);
	(void)xrtTypeCompareValue(
		pTree->KeyType,
		pKey,
		__xrtTypedTreeKey(pTree, pEntry),
		&iCompare
	);
	__xrtTypedTreeCallbackEnd(pTree, bBusy);
	return iCompare;
}



/* 销毁节点拥有的完整值和键。 */
static void __xrtTypedTreeDrop(ptr pEntry, ptr pUserData)
{
	xtypedtree* pTree = (xtypedtree*)pUserData;
	bool bBusy;

	bBusy = __xrtTypedTreeCallbackBegin(pTree);
	xrtTypeDropValue(
		pTree->ValueType, __xrtTypedTreeValue(pTree, pEntry)
	);
	xrtTypeDropValue(
		pTree->KeyType, __xrtTypedTreeKey(pTree, pEntry)
	);
	__xrtTypedTreeCallbackEnd(pTree, bBusy);
}



/* 检查公开类型树状态、布局和底层回调是否一致。 */
static bool __xrtTypedTreeValid(
	const xtypedtree* pTree,
	cstr sOperation
)
{
	if ( (pTree == NULL) || (pTree->KeyType == NULL) ||
		 (pTree->ValueType == NULL) ) {
		__xrtTypedTreeError(XERR_ARGUMENT, XTYPED_TREE_ERROR_ARGUMENT,
			sOperation, "the typed tree is null or uninitialized");
		return false;
	}
	if (
		((pTree->Flags & XRT_TYPED_TREE_FLAG_READY) == 0u) ||
		((pTree->Flags & XRT_TYPED_TREE_FLAG_BUSY) != 0u) ||
		((pTree->Flags & ~XRT_TYPED_TREE_FLAGS) != 0u)
	) {
		__xrtTypedTreeError(XERR_STATE, XTYPED_TREE_ERROR_STATE,
			sOperation, "the typed tree is not available for API access");
		return false;
	}
	if ( !__xrtAVLTreeValid(&pTree->Storage) ) {
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_STATE,
			sOperation, "the typed tree storage is invalid");
		return false;
	}
	if (
		(pTree->KeyOffset != 0u) ||
		(pTree->ValueOffset < pTree->KeyType->Size) ||
		(pTree->EntrySize != pTree->Storage.ItemSize) ||
		(pTree->Alignment != pTree->Storage.Alignment) ||
		(pTree->Storage.Compare != __xrtTypedTreeCompare) ||
		(pTree->Storage.Drop != __xrtTypedTreeDrop) ||
		(pTree->Storage.UserData != pTree)
	) {
		__xrtTypedTreeError(XERR_STATE, XTYPED_TREE_ERROR_LAYOUT,
			sOperation, "the typed tree layout or callbacks are invalid");
		return false;
	}
	return true;
}



/* 检查当前树是否允许结构和生命周期修改。 */
static bool __xrtTypedTreeCanMutate(
	const xtypedtree* pTree,
	cstr sOperation
)
{
	if ( !__xrtTypedTreeValid(pTree, sOperation) ) {
		return false;
	}
	if ( !__xrtAVLTreeCanMutate(&pTree->Storage) ) {
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_STATE,
			sOperation, "the typed tree is currently being visited");
		return false;
	}
	return true;
}



/* 判断字节区间是否触及类型树结构或节点池。 */
static bool __xrtTypedTreeOwnsRange(
	const xtypedtree* pTree,
	const void* pMemory,
	size_t iSize
)
{
	return __xrtRangesOverlap(pMemory, iSize, pTree, sizeof(*pTree)) ||
		__xrtAVLTreeOwnsRange(&pTree->Storage, pMemory, iSize);
}



/* 验证内部来源正好指向一个活动键槽或值槽。 */
static bool __xrtTypedTreeSourceValid(
	xtypedtree* pTree,
	const void* pSource,
	bool bKey,
	cstr sOperation
)
{
	xavltreeiter Iterator;
	ptr pEntry;
	size_t iSize = bKey ? pTree->KeyType->Size : pTree->ValueType->Size;
	bool bExact = false;

	if ( pSource == NULL ) {
		__xrtTypedTreeError(XERR_ARGUMENT, XTYPED_TREE_ERROR_ARGUMENT,
			sOperation, bKey ? "the source key is null" :
			"the source value is null");
		return false;
	}
	if ( !__xrtTypedTreeOwnsRange(pTree, pSource, iSize) ) {
		return true;
	}
	if ( !xrtAVLTreeIterBegin(&pTree->Storage, &Iterator) ) {
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_STATE,
			sOperation, "the typed tree source scan could not start");
		return false;
	}
	while ( (pEntry = xrtAVLTreeIterNext(&Iterator)) != NULL ) {
		ptr pExpected = bKey ? __xrtTypedTreeKey(pTree, pEntry) :
			__xrtTypedTreeValue(pTree, pEntry);

		if ( pExpected == pSource ) {
			bExact = true;
			break;
		}
	}
	xrtAVLTreeIterEnd(&Iterator);
	if ( !bExact ) {
		__xrtTypedTreeError(XERR_ARGUMENT, XTYPED_TREE_ERROR_ARGUMENT,
			sOperation, bKey ?
			"an internal key source must be an active key boundary" :
			"an internal value source must be an active value boundary");
		return false;
	}
	return true;
}



/* 验证移动来源或输出完全位于类型树拥有的内存之外。 */
static bool __xrtTypedTreeExternalValue(
	const xtypedtree* pTree,
	const void* pValue,
	cstr sOperation
)
{
	if ( pValue == NULL ) {
		__xrtTypedTreeError(XERR_ARGUMENT, XTYPED_TREE_ERROR_ARGUMENT,
			sOperation, "the movable value is null");
		return false;
	}
	if ( __xrtTypedTreeOwnsRange(
		pTree, pValue, pTree->ValueType->Size
	) ) {
		__xrtTypedTreeError(XERR_ARGUMENT, XTYPED_TREE_ERROR_ARGUMENT,
			sOperation, "the movable value must not alias typed tree storage");
		return false;
	}
	return true;
}



/* 验证移动值或输出不会在后续树查询前改写外部查询键。 */
static bool __xrtTypedTreeMoveSeparate(
	const xtypedtree* pTree,
	const void* pKey,
	const void* pValue,
	cstr sOperation
)
{
	if ( __xrtRangesOverlap(
		pKey,
		pTree->KeyType->Size,
		pValue,
		pTree->ValueType->Size
	) ) {
		__xrtTypedTreeError(XERR_ARGUMENT, XTYPED_TREE_ERROR_ARGUMENT,
			sOperation, "the movable value must not overlap the search key");
		return false;
	}
	return true;
}



/* 节点初始化模式区分默认值、复制值和移动值。 */
typedef enum __xrt_typed_tree_init_mode {
	XRT_TYPED_TREE_INIT_DEFAULT = 0,
	XRT_TYPED_TREE_INIT_COPY,
	XRT_TYPED_TREE_INIT_MOVE
} __xrt_typed_tree_init_mode;



/* 新节点初始化上下文保存来源键值和生命周期模式。 */
typedef struct __xrt_typed_tree_init_context {
	xtypedtree* Tree;
	const void* Key;
	ptr Value;
	__xrt_typed_tree_init_mode Mode;
} __xrt_typed_tree_init_context;



/* 初始化并复制一个尚未提交的新树节点。 */
static bool __xrtTypedTreeInitEntry(
	ptr pEntry,
	const void* pKey,
	ptr pUserData
)
{
	__xrt_typed_tree_init_context* pContext =
		(__xrt_typed_tree_init_context*)pUserData;
	xtypedtree* pTree = pContext->Tree;
	ptr pStoredKey = __xrtTypedTreeKey(pTree, pEntry);
	ptr pStoredValue = __xrtTypedTreeValue(pTree, pEntry);
	xerror* pError;
	bool bSuccess;
	bool bBusy;

	(void)pKey;
	bBusy = __xrtTypedTreeCallbackBegin(pTree);
	if ( !xrtTypeInitValue(pTree->KeyType, pStoredKey) ) {
		__xrtTypedTreeCallbackEnd(pTree, bBusy);
		return false;
	}
	if ( !xrtTypeCopyValue(pTree->KeyType, pStoredKey, pContext->Key) ) {
		pError = xrtTakeError();
		xrtTypeDropValue(pTree->KeyType, pStoredKey);
		__xrtTypedTreeCallbackEnd(pTree, bBusy);
		__xrtTypedTreeRestoreError(pError);
		return false;
	}
	if ( !xrtTypeInitValue(pTree->ValueType, pStoredValue) ) {
		pError = xrtTakeError();
		xrtTypeDropValue(pTree->KeyType, pStoredKey);
		__xrtTypedTreeCallbackEnd(pTree, bBusy);
		__xrtTypedTreeRestoreError(pError);
		return false;
	}
	bSuccess = pContext->Mode == XRT_TYPED_TREE_INIT_DEFAULT;
	if ( pContext->Mode == XRT_TYPED_TREE_INIT_COPY ) {
		bSuccess = xrtTypeCopyValue(
			pTree->ValueType, pStoredValue, pContext->Value
		);
	} else if ( pContext->Mode == XRT_TYPED_TREE_INIT_MOVE ) {
		bSuccess = xrtTypeMoveValue(
			pTree->ValueType, pStoredValue, pContext->Value
		);
	}
	if ( !bSuccess ) {
		pError = xrtTakeError();
		xrtTypeDropValue(pTree->ValueType, pStoredValue);
		xrtTypeDropValue(pTree->KeyType, pStoredKey);
		__xrtTypedTreeCallbackEnd(pTree, bBusy);
		__xrtTypedTreeRestoreError(pError);
		return false;
	}
	__xrtTypedTreeCallbackEnd(pTree, bBusy);
	return true;
}



/* 回滚一个已经完整初始化但尚未提交的节点。 */
static void __xrtTypedTreeRollbackEntry(ptr pEntry, ptr pUserData)
{
	__xrt_typed_tree_init_context* pContext =
		(__xrt_typed_tree_init_context*)pUserData;

	__xrtTypedTreeDrop(pEntry, pContext->Tree);
}



/* 为缺失键建立节点，并返回值槽。 */
static ptr __xrtTypedTreeInsert(
	xtypedtree* pTree,
	const void* pKey,
	ptr pValue,
	__xrt_typed_tree_init_mode Mode,
	bool* pNew,
	cstr sOperation
)
{
	__xrt_typed_tree_init_context Context = {
		pTree,
		pKey,
		pValue,
		Mode
	};
	ptr pEntry = __xrtAVLTreeGetOrAdd(
		&pTree->Storage,
		pKey,
		__xrtTypedTreeInitEntry,
		&Context,
		__xrtTypedTreeRollbackEntry,
		&Context,
		pNew
	);

	if ( pEntry == NULL ) {
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_OPERATION,
			sOperation, "the typed tree node could not be inserted");
		return NULL;
	}
	return __xrtTypedTreeValue(pTree, pEntry);
}



/* 修复类型树按值交换后保存的内部自引用。 */
static void __xrtTypedTreeRepair(xtypedtree* pTree)
{
	pTree->Storage.UserData = pTree;
}



/* 递增结构版本并跳过外置迭代器保留的零值。 */
static uint64 __xrtTypedTreeNextVersion(uint64 iVersion)
{
	iVersion++;
	return iVersion != 0u ? iVersion : 1u;
}



/* 交换两个静止且有效的类型树，并修复底层自引用。 */
static void __xrtTypedTreeSwap(xtypedtree* pLeft, xtypedtree* pRight)
{
	xtypedtree Tree = *pLeft;

	*pLeft = *pRight;
	*pRight = Tree;
	__xrtTypedTreeRepair(pLeft);
	__xrtTypedTreeRepair(pRight);
}



/* 初始化一个拥有类型键值的空树。 */
XRT_API bool xrtTypedTreeInit(
	xtypedtree* pTree,
	const xrttype* pKeyType,
	const xrttype* pValueType
)
{
	size_t iValueOffset;
	size_t iEntrySize;
	size_t iAlignment;

	if ( pTree == NULL ) {
		__xrtTypedTreeError(XERR_ARGUMENT, XTYPED_TREE_ERROR_ARGUMENT,
			"init", "the typed tree is null");
		return false;
	}
	memset(pTree, 0, sizeof(*pTree));
	if ( !__xrtTypedTreeValueTypeValidate(pKeyType, true, "init") ||
		 !__xrtTypedTreeValueTypeValidate(pValueType, false, "init") ||
		 !__xrtTypedTreeLayout(
			pKeyType, pValueType, &iValueOffset, &iEntrySize, &iAlignment
		) ) {
		return false;
	}
	pTree->KeyType = pKeyType;
	pTree->ValueType = pValueType;
	pTree->KeyOffset = 0u;
	pTree->ValueOffset = iValueOffset;
	pTree->EntrySize = iEntrySize;
	pTree->Alignment = iAlignment;
	if ( !xrtAVLTreeInitAligned(
		&pTree->Storage,
		iEntrySize,
		iAlignment,
		__xrtTypedTreeCompare,
		pTree
	) || !xrtAVLTreeSetDrop(&pTree->Storage, __xrtTypedTreeDrop) ) {
		if ( pTree->Storage.Compare != NULL ) {
			xrtAVLTreeUnit(&pTree->Storage);
		}
		memset(pTree, 0, sizeof(*pTree));
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_OPERATION,
			"init", "the typed tree storage could not be initialized");
		return false;
	}
	pTree->Flags = XRT_TYPED_TREE_FLAG_READY;
	return true;
}



/* 在堆上创建一个拥有类型键值的空树。 */
XRT_API xtypedtree* xrtTypedTreeCreate(
	const xrttype* pKeyType,
	const xrttype* pValueType
)
{
	xtypedtree* pTree = (xtypedtree*)xrtMalloc(sizeof(*pTree));

	if ( pTree == NULL ) {
		return NULL;
	}
	if ( !xrtTypedTreeInit(pTree, pKeyType, pValueType) ) {
		xrtFree(pTree);
		return NULL;
	}
	return pTree;
}



/* 释放全部键值和节点池，但不释放树结构。 */
XRT_API void xrtTypedTreeUnit(xtypedtree* pTree)
{
	if ( pTree == NULL ) {
		return;
	}
	if ( (pTree->KeyType == NULL) && (pTree->ValueType == NULL) &&
		 (pTree->Flags == 0u) ) {
		return;
	}
	if ( !__xrtTypedTreeCanMutate(pTree, "unit") ) {
		return;
	}
	xrtAVLTreeUnit(&pTree->Storage);
	memset(pTree, 0, sizeof(*pTree));
}



/* 释放全部键值、节点池和堆树结构。 */
XRT_API void xrtTypedTreeDestroy(xtypedtree* pTree)
{
	if ( pTree == NULL ) {
		return;
	}
	if ( !__xrtTypedTreeCanMutate(pTree, "destroy") ) {
		return;
	}
	xrtTypedTreeUnit(pTree);
	xrtFree(pTree);
}



/* 返回树借用的键类型描述。 */
XRT_API const xrttype* xrtTypedTreeKeyType(const xtypedtree* pTree)
{
	return __xrtTypedTreeValid(pTree, "key-type") ? pTree->KeyType : NULL;
}



/* 返回树借用的值类型描述。 */
XRT_API const xrttype* xrtTypedTreeValueType(const xtypedtree* pTree)
{
	return __xrtTypedTreeValid(pTree, "value-type") ? pTree->ValueType : NULL;
}



/* 返回当前键值数量。 */
XRT_API size_t xrtTypedTreeCount(const xtypedtree* pTree)
{
	return __xrtTypedTreeValid(pTree, "count") ?
		xrtAVLTreeCount(&pTree->Storage) : 0u;
}



/* 销毁全部键值并保留节点池供复用。 */
XRT_API bool xrtTypedTreeClear(xtypedtree* pTree)
{
	if ( !__xrtTypedTreeCanMutate(pTree, "clear") ) {
		return false;
	}
	xrtAVLTreeClear(&pTree->Storage);
	return true;
}



/* 释放多余空节点池页并返回实际释放页数。 */
XRT_API size_t xrtTypedTreeTrim(xtypedtree* pTree, size_t iRetainEmpty)
{
	if ( !__xrtTypedTreeCanMutate(pTree, "trim") ) {
		return 0u;
	}
	return xrtPoolTrim(&pTree->Storage.Pool, iRetainEmpty);
}



/* 返回已有值槽，或复制键并默认初始化一个新值。 */
XRT_API ptr xrtTypedTreeGetOrAdd(
	xtypedtree* pTree,
	const void* pKey,
	bool* pNew
)
{
	if ( pNew != NULL ) {
		*pNew = false;
	}
	if ( !__xrtTypedTreeCanMutate(pTree, "get-or-add") ||
		 !__xrtTypedTreeSourceValid(pTree, pKey, true, "get-or-add") ) {
		return NULL;
	}
	return __xrtTypedTreeInsert(
		pTree, pKey, NULL, XRT_TYPED_TREE_INIT_DEFAULT, pNew, "get-or-add"
	);
}



/* 失败原子地复制插入或替换一个键值。 */
XRT_API bool xrtTypedTreeSet(
	xtypedtree* pTree,
	const void* pKey,
	const void* pValue
)
{
	ptr pStored;
	bool bNew;
	bool bBusy;

	if ( !__xrtTypedTreeCanMutate(pTree, "set") ||
		 !__xrtTypedTreeSourceValid(pTree, pKey, true, "set") ||
		 !__xrtTypedTreeSourceValid(pTree, pValue, false, "set") ) {
		return false;
	}
	pStored = __xrtTypedTreeInsert(
		pTree,
		pKey,
		(ptr)pValue,
		XRT_TYPED_TREE_INIT_COPY,
		&bNew,
		"set"
	);
	if ( pStored == NULL ) {
		return false;
	}
	if ( bNew ) {
		return true;
	}
	if ( pStored == pValue ) {
		return true;
	}
	bBusy = __xrtTypedTreeCallbackBegin(pTree);
	if ( !xrtTypeCopyValue(pTree->ValueType, pStored, pValue) ) {
		__xrtTypedTreeCallbackEnd(pTree, bBusy);
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_OPERATION,
			"set", "the existing typed tree value could not be replaced");
		return false;
	}
	__xrtTypedTreeCallbackEnd(pTree, bBusy);
	return true;
}



/* 移动外部已初始化值插入或替换一个键值。 */
XRT_API bool xrtTypedTreeSetTake(
	xtypedtree* pTree,
	const void* pKey,
	ptr pValue
)
{
	ptr pStored;
	bool bNew;
	bool bBusy;

	if ( !__xrtTypedTreeCanMutate(pTree, "set-take") ||
		 !__xrtTypedTreeSourceValid(pTree, pKey, true, "set-take") ||
		 !__xrtTypedTreeExternalValue(pTree, pValue, "set-take") ||
		 !__xrtTypedTreeMoveSeparate(
			pTree, pKey, pValue, "set-take"
		) ) {
		return false;
	}
	pStored = __xrtTypedTreeInsert(
		pTree,
		pKey,
		pValue,
		XRT_TYPED_TREE_INIT_MOVE,
		&bNew,
		"set-take"
	);
	if ( pStored == NULL ) {
		return false;
	}
	if ( bNew ) {
		return true;
	}
	bBusy = __xrtTypedTreeCallbackBegin(pTree);
	if ( !xrtTypeMoveValue(pTree->ValueType, pStored, pValue) ) {
		__xrtTypedTreeCallbackEnd(pTree, bBusy);
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_OPERATION,
			"set-take", "the existing typed tree value could not be moved");
		return false;
	}
	__xrtTypedTreeCallbackEnd(pTree, bBusy);
	return true;
}



/* 返回指定键的可写借用值槽。 */
XRT_API ptr xrtTypedTreeGet(xtypedtree* pTree, const void* pKey)
{
	ptr pEntry;

	if ( !__xrtTypedTreeValid(pTree, "get") ||
		 !__xrtTypedTreeSourceValid(pTree, pKey, true, "get") ) {
		return NULL;
	}
	pEntry = xrtAVLTreeFind(&pTree->Storage, pKey);
	return __xrtTypedTreeValue(pTree, pEntry);
}



/* 返回指定键的只读借用值槽。 */
XRT_API const void* xrtTypedTreeConstGet(
	const xtypedtree* pTree,
	const void* pKey
)
{
	const void* pEntry;

	if ( !__xrtTypedTreeValid(pTree, "const-get") ||
		 !__xrtTypedTreeSourceValid(
			(xtypedtree*)pTree, pKey, true, "const-get"
		) ) {
		return NULL;
	}
	pEntry = xrtAVLTreeConstFind(&pTree->Storage, pKey);
	return __xrtTypedTreeValue(pTree, pEntry);
}



/* 判断指定键是否存在。 */
XRT_API bool xrtTypedTreeHas(
	const xtypedtree* pTree,
	const void* pKey
)
{
	if ( !__xrtTypedTreeValid(pTree, "has") ||
		 !__xrtTypedTreeSourceValid(
			(xtypedtree*)pTree, pKey, true, "has"
		) ) {
		return false;
	}
	return xrtAVLTreeHas(&pTree->Storage, pKey);
}



/* 返回指定查询对应的内部规范键。 */
XRT_API const void* xrtTypedTreeStoredKey(
	const xtypedtree* pTree,
	const void* pKey
)
{
	const void* pEntry;

	if ( !__xrtTypedTreeValid(pTree, "stored-key") ||
		 !__xrtTypedTreeSourceValid(
			(xtypedtree*)pTree, pKey, true, "stored-key"
		) ) {
		return NULL;
	}
	pEntry = xrtAVLTreeConstFind(&pTree->Storage, pKey);
	return __xrtTypedTreeKey(pTree, pEntry);
}



/* 删除指定键并销毁对应键值。 */
XRT_API bool xrtTypedTreeRemove(xtypedtree* pTree, const void* pKey)
{
	if ( !__xrtTypedTreeCanMutate(pTree, "remove") ||
		 !__xrtTypedTreeSourceValid(pTree, pKey, true, "remove") ) {
		return false;
	}
	return xrtAVLTreeRemove(&pTree->Storage, pKey);
}



/* 把值移动到外部已初始化输出后删除键值。 */
XRT_API bool xrtTypedTreeTake(
	xtypedtree* pTree,
	const void* pKey,
	ptr pValue
)
{
	ptr pEntry;
	ptr pStored;
	bool bBusy;

	if ( !__xrtTypedTreeCanMutate(pTree, "take") ||
		 !__xrtTypedTreeSourceValid(pTree, pKey, true, "take") ||
		 !__xrtTypedTreeExternalValue(pTree, pValue, "take") ||
		 !__xrtTypedTreeMoveSeparate(pTree, pKey, pValue, "take") ) {
		return false;
	}
	pEntry = xrtAVLTreeFind(&pTree->Storage, pKey);
	if ( pEntry == NULL ) {
		return false;
	}
	pStored = __xrtTypedTreeValue(pTree, pEntry);
	bBusy = __xrtTypedTreeCallbackBegin(pTree);
	if ( !xrtTypeMoveValue(pTree->ValueType, pValue, pStored) ) {
		__xrtTypedTreeCallbackEnd(pTree, bBusy);
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_OPERATION,
			"take", "the typed tree value could not be moved out");
		return false;
	}
	__xrtTypedTreeCallbackEnd(pTree, bBusy);
	if ( !xrtAVLTreeRemove(&pTree->Storage, pKey) ) {
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_STATE,
			"take", "the moved typed tree entry could not be removed");
		return false;
	}
	return true;
}



/* 将底层树条目投影为借用值，并可返回对应的内部规范键。 */
static ptr __xrtTypedTreeResult(
	xtypedtree* pTree,
	ptr pEntry,
	const void** pKey
)
{
	if ( pKey != NULL ) {
		*pKey = pEntry != NULL ? __xrtTypedTreeKey(pTree, pEntry) : NULL;
	}
	return __xrtTypedTreeValue(pTree, pEntry);
}



/* 返回按键升序排列的第一项。 */
XRT_API ptr xrtTypedTreeFirst(xtypedtree* pTree, const void** pKey)
{
	if ( pKey != NULL ) {
		*pKey = NULL;
	}
	if ( !__xrtTypedTreeValid(pTree, "first") ) {
		return NULL;
	}
	return __xrtTypedTreeResult(
		pTree, xrtAVLTreeFirst(&pTree->Storage), pKey
	);
}



/* 返回按键升序排列的最后一项。 */
XRT_API ptr xrtTypedTreeLast(xtypedtree* pTree, const void** pKey)
{
	if ( pKey != NULL ) {
		*pKey = NULL;
	}
	if ( !__xrtTypedTreeValid(pTree, "last") ) {
		return NULL;
	}
	return __xrtTypedTreeResult(
		pTree, xrtAVLTreeLast(&pTree->Storage), pKey
	);
}



/* 返回第一项不小于查询键的值和内部规范键。 */
XRT_API ptr xrtTypedTreeLowerBound(
	xtypedtree* pTree,
	const void* pSearchKey,
	const void** pStoredKey
)
{
	if ( pStoredKey != NULL ) {
		*pStoredKey = NULL;
	}
	if ( !__xrtTypedTreeValid(pTree, "lower-bound") ||
		 !__xrtTypedTreeSourceValid(
			pTree, pSearchKey, true, "lower-bound"
		) ) {
		return NULL;
	}
	return __xrtTypedTreeResult(
		pTree,
		xrtAVLTreeLowerBound(&pTree->Storage, pSearchKey),
		pStoredKey
	);
}



/* 返回第一项严格大于查询键的值和内部规范键。 */
XRT_API ptr xrtTypedTreeUpperBound(
	xtypedtree* pTree,
	const void* pSearchKey,
	const void** pStoredKey
)
{
	if ( pStoredKey != NULL ) {
		*pStoredKey = NULL;
	}
	if ( !__xrtTypedTreeValid(pTree, "upper-bound") ||
		 !__xrtTypedTreeSourceValid(
			pTree, pSearchKey, true, "upper-bound"
		) ) {
		return NULL;
	}
	return __xrtTypedTreeResult(
		pTree,
		xrtAVLTreeUpperBound(&pTree->Storage, pSearchKey),
		pStoredKey
	);
}



/* 启动完整或带包含边界的正反零分配迭代。 */
static bool __xrtTypedTreeIterStart(
	xtypedtree* pTree,
	const void* pKey,
	bool bHasKey,
	bool bReverse,
	xtypedtreeiter* pIterator,
	cstr sOperation
)
{
	bool bSuccess;

	if ( pIterator != NULL ) {
		memset(pIterator, 0, sizeof(*pIterator));
	}
	if ( !__xrtTypedTreeValid(pTree, sOperation) ||
		 (pIterator == NULL) ||
		 (bHasKey && !__xrtTypedTreeSourceValid(
			pTree, pKey, true, sOperation
		)) ) {
		if ( pIterator == NULL ) {
			__xrtTypedTreeError(XERR_ARGUMENT, XTYPED_TREE_ERROR_ARGUMENT,
				sOperation, "the typed tree iterator is null");
		}
		return false;
	}
	bSuccess = bHasKey ?
		(bReverse ?
			xrtAVLTreeIterRFrom(&pTree->Storage, pKey, &pIterator->Base) :
			xrtAVLTreeIterFrom(&pTree->Storage, pKey, &pIterator->Base)) :
		(bReverse ?
			xrtAVLTreeIterRBegin(&pTree->Storage, &pIterator->Base) :
			xrtAVLTreeIterBegin(&pTree->Storage, &pIterator->Base));
	if ( !bSuccess ) {
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_STATE,
			sOperation, "the typed tree iterator could not start");
		return false;
	}
	pIterator->Tree = pTree;
	return true;
}



/* 启动按键升序的完整迭代。 */
XRT_API bool xrtTypedTreeIterBegin(
	xtypedtree* pTree,
	xtypedtreeiter* pIterator
)
{
	return __xrtTypedTreeIterStart(
		pTree, NULL, false, false, pIterator, "iter-begin"
	);
}



/* 启动按键降序的完整迭代。 */
XRT_API bool xrtTypedTreeIterRBegin(
	xtypedtree* pTree,
	xtypedtreeiter* pIterator
)
{
	return __xrtTypedTreeIterStart(
		pTree, NULL, false, true, pIterator, "iter-rbegin"
	);
}



/* 从第一项不小于查询键的位置开始升序迭代。 */
XRT_API bool xrtTypedTreeIterFrom(
	xtypedtree* pTree,
	const void* pKey,
	xtypedtreeiter* pIterator
)
{
	return __xrtTypedTreeIterStart(
		pTree, pKey, true, false, pIterator, "iter-from"
	);
}



/* 从第一项不大于查询键的位置开始降序迭代。 */
XRT_API bool xrtTypedTreeIterRFrom(
	xtypedtree* pTree,
	const void* pKey,
	xtypedtreeiter* pIterator
)
{
	return __xrtTypedTreeIterStart(
		pTree, pKey, true, true, pIterator, "iter-rfrom"
	);
}



/* 返回下一借用值和可选规范键，并检测结构修改。 */
XRT_API ptr xrtTypedTreeIterNext(
	xtypedtreeiter* pIterator,
	const void** pKey
)
{
	xtypedtree* pTree;
	ptr pEntry;

	if ( pKey != NULL ) {
		*pKey = NULL;
	}
	if ( (pIterator == NULL) || (pIterator->Tree == NULL) ) {
		if ( pIterator == NULL ) {
			__xrtTypedTreeError(XERR_ARGUMENT, XTYPED_TREE_ERROR_ARGUMENT,
				"iter-next", "the typed tree iterator is null");
		}
		return NULL;
	}
	pTree = pIterator->Tree;
	if ( !__xrtTypedTreeValid(pTree, "iter-next") ) {
		xrtAVLTreeIterEnd(&pIterator->Base);
		pIterator->Tree = NULL;
		return NULL;
	}
	if (
		(pIterator->Base.Tree != &pTree->Storage) ||
		(pIterator->Base.Base.Tree != &pTree->Storage.Base) ||
		(pIterator->Base.Base.Version != pTree->Storage.Base.Version)
	) {
		xrtAVLTreeIterEnd(&pIterator->Base);
		pIterator->Tree = NULL;
		__xrtTypedTreeError(XERR_STATE, XTYPED_TREE_ERROR_STATE,
			"iter-next", "the typed tree changed during iteration");
		return NULL;
	}
	pEntry = xrtAVLTreeIterNext(&pIterator->Base);
	if ( pEntry == NULL ) {
		pIterator->Tree = NULL;
		return NULL;
	}
	return __xrtTypedTreeResult(pTree, pEntry, pKey);
}



/* 提前结束迭代并清除全部借用状态。 */
XRT_API void xrtTypedTreeIterEnd(xtypedtreeiter* pIterator)
{
	if ( pIterator == NULL ) {
		return;
	}
	xrtAVLTreeIterEnd(&pIterator->Base);
	pIterator->Tree = NULL;
}



/* 验证两个树使用完全相同的键和值类型描述。 */
static bool __xrtTypedTreeSameType(
	const xtypedtree* pLeft,
	const xtypedtree* pRight,
	cstr sOperation
)
{
	if ( (pLeft->KeyType != pRight->KeyType) ||
		 (pLeft->ValueType != pRight->ValueType) ) {
		__xrtTypedTreeError(XERR_TYPE, XTYPED_TREE_ERROR_TYPE,
			sOperation, "the typed tree key or value types do not match");
		return false;
	}
	return true;
}



/* 销毁临时堆树，同时保留进入清理阶段前的根错误。 */
static void __xrtTypedTreeDestroyPreserveError(xtypedtree* pTree)
{
	xerror* pError = xrtTakeError();

	xrtTypedTreeDestroy(pTree);
	__xrtTypedTreeRestoreError(pError);
}



/* 在调用方保护来源期间深复制一棵树。 */
static xtypedtree* __xrtTypedTreeCloneProtected(
	const xtypedtree* pTree,
	cstr sOperation
)
{
	xtypedtree* pClone;
	xavltreeiter Iterator;
	ptr pEntry;

	pClone = xrtTypedTreeCreate(pTree->KeyType, pTree->ValueType);
	if ( pClone == NULL ) {
		__xrtTypedTreeWrap(XERR_MEMORY, XTYPED_TREE_ERROR_OPERATION,
			sOperation, "the typed tree clone could not be created");
		return NULL;
	}
	if ( !xrtAVLTreeIterBegin((xavltree*)&pTree->Storage, &Iterator) ) {
		__xrtTypedTreeDestroyPreserveError(pClone);
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_STATE,
			sOperation, "the typed tree clone iterator could not start");
		return NULL;
	}
	while ( (pEntry = xrtAVLTreeIterNext(&Iterator)) != NULL ) {
		if ( !xrtTypedTreeSet(
			pClone,
			__xrtTypedTreeKey(pTree, pEntry),
			__xrtTypedTreeValue(pTree, pEntry)
		) ) {
			xrtAVLTreeIterEnd(&Iterator);
			__xrtTypedTreeDestroyPreserveError(pClone);
			__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_OPERATION,
				sOperation, "a typed tree entry could not be cloned");
			return NULL;
		}
	}
	xrtAVLTreeIterEnd(&Iterator);
	return pClone;
}



/* 深复制一个独立堆类型树。 */
XRT_API xtypedtree* xrtTypedTreeClone(const xtypedtree* pTree)
{
	xtypedtree* pClone;
	bool bBusy;

	if ( !__xrtTypedTreeValid(pTree, "clone") ) {
		return NULL;
	}
	bBusy = __xrtTypedTreeCallbackBegin(pTree);
	pClone = __xrtTypedTreeCloneProtected(pTree, "clone");
	__xrtTypedTreeCallbackEnd(pTree, bBusy);
	return pClone;
}



/* 事务合并同类型树，并按策略保留或替换冲突键。 */
XRT_API bool xrtTypedTreeMerge(
	xtypedtree* pTarget,
	const xtypedtree* pSource,
	bool bReplace
)
{
	xtypedtree* pWork;
	xavltreeiter Iterator;
	ptr pEntry;
	bool bTargetBusy;
	bool bSourceBusy;
	bool bSuccess = true;
	uint64 iTargetVersion;

	if ( !__xrtTypedTreeCanMutate(pTarget, "merge") ||
		 !__xrtTypedTreeValid(pSource, "merge") ||
		 !__xrtTypedTreeSameType(pTarget, pSource, "merge") ) {
		return false;
	}
	if ( pTarget == pSource ) {
		return true;
	}
	bTargetBusy = __xrtTypedTreeCallbackBegin(pTarget);
	bSourceBusy = __xrtTypedTreeCallbackBegin(pSource);
	pWork = __xrtTypedTreeCloneProtected(pTarget, "merge");
	if ( pWork == NULL ) {
		__xrtTypedTreeCallbackEnd(pSource, bSourceBusy);
		__xrtTypedTreeCallbackEnd(pTarget, bTargetBusy);
		return false;
	}
	if ( !xrtAVLTreeIterBegin((xavltree*)&pSource->Storage, &Iterator) ) {
		bSuccess = false;
	} else {
		while ( (pEntry = xrtAVLTreeIterNext(&Iterator)) != NULL ) {
			const void* pKey = __xrtTypedTreeKey(pSource, pEntry);

			if ( !bReplace && xrtTypedTreeHas(pWork, pKey) ) {
				continue;
			}
			if ( !xrtTypedTreeSet(
				pWork, pKey, __xrtTypedTreeValue(pSource, pEntry)
			) ) {
				bSuccess = false;
				break;
			}
		}
		xrtAVLTreeIterEnd(&Iterator);
	}
	__xrtTypedTreeCallbackEnd(pSource, bSourceBusy);
	__xrtTypedTreeCallbackEnd(pTarget, bTargetBusy);
	if ( !bSuccess ) {
		__xrtTypedTreeDestroyPreserveError(pWork);
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_OPERATION,
			"merge", "a source typed tree entry could not be merged");
		return false;
	}

	iTargetVersion = pTarget->Storage.Base.Version;
	__xrtTypedTreeSwap(pTarget, pWork);
	pTarget->Storage.Base.Version = __xrtTypedTreeNextVersion(iTargetVersion);
	xrtTypedTreeDestroy(pWork);
	return true;
}



/* 比较两个树的类型、键集合和值内容。 */
XRT_API bool xrtTypedTreeEquals(
	const xtypedtree* pLeft,
	const xtypedtree* pRight
)
{
	xavltreeiter Iterator;
	ptr pLeftEntry;
	const void* pRightEntry;
	bool bLeftBusy;
	bool bRightBusy;
	bool bEqual = true;
	bool bFailed = false;
	int iCompare;

	if ( !__xrtTypedTreeValid(pLeft, "equals") ||
		 !__xrtTypedTreeValid(pRight, "equals") ||
		 !__xrtTypedTreeSameType(pLeft, pRight, "equals") ) {
		return false;
	}
	if ( pLeft == pRight ) {
		return true;
	}
	if ( pLeft->Storage.Base.Count != pRight->Storage.Base.Count ) {
		return false;
	}
	if ( !xrtTypeIsComparable(pLeft->ValueType) ) {
		__xrtTypedTreeError(XERR_UNSUPPORTED, XTYPED_TREE_ERROR_TYPE,
			"equals", "the typed tree value type is not comparable");
		return false;
	}
	bLeftBusy = __xrtTypedTreeCallbackBegin(pLeft);
	bRightBusy = __xrtTypedTreeCallbackBegin(pRight);
	if ( !xrtAVLTreeIterBegin((xavltree*)&pLeft->Storage, &Iterator) ) {
		bEqual = false;
		bFailed = true;
	} else {
		while ( (pLeftEntry = xrtAVLTreeIterNext(&Iterator)) != NULL ) {
			pRightEntry = xrtAVLTreeConstFind(
				&pRight->Storage,
				__xrtTypedTreeKey(pLeft, pLeftEntry)
			);
			if ( pRightEntry == NULL ) {
				bEqual = false;
				break;
			}
			if ( !xrtTypeCompareValue(
				pLeft->ValueType,
				__xrtTypedTreeValue(pLeft, pLeftEntry),
				__xrtTypedTreeValue(pRight, pRightEntry),
				&iCompare
			) ) {
				bEqual = false;
				bFailed = true;
				break;
			}
			if ( iCompare != 0 ) {
				bEqual = false;
				break;
			}
		}
		xrtAVLTreeIterEnd(&Iterator);
	}
	__xrtTypedTreeCallbackEnd(pRight, bRightBusy);
	__xrtTypedTreeCallbackEnd(pLeft, bLeftBusy);
	if ( bFailed ) {
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_STATE,
			"equals", "the typed tree entries could not be compared");
	}
	return bEqual;
}



/* 初始化对象负载中的类型树。 */
static bool __xrtTypedTreeInstanceInit(
	ptr pInstance,
	const xrttype* pType
)
{
	if ( !xrtTypedTreeTypeValidate(pType) ) {
		return false;
	}
	return xrtTypedTreeInit(
		(xtypedtree*)pInstance, pType->Arguments[0], pType->Arguments[1]
	);
}



/* 销毁对象负载中的类型树。 */
static void __xrtTypedTreeInstanceDrop(
	ptr pInstance,
	const xrttype* pType
)
{
	(void)pType;
	xrtTypedTreeUnit((xtypedtree*)pInstance);
}



/* 类型树追踪适配器保存对象访问器和失败状态。 */
typedef struct xtypedtreetracecontext {
	const xtypedtree* Tree;
	xrtobjectvisitor Visit;
	ptr UserData;
	bool Failed;
} xtypedtreetracecontext;



/* 枚举一个树条目的键和值直接拥有的强对象引用。 */
static bool __xrtTypedTreeTraceEntry(ptr pEntry, ptr pUserData)
{
	xtypedtreetracecontext* pContext =
		(xtypedtreetracecontext*)pUserData;
	const xtypedtree* pTree = pContext->Tree;
	bool bBusy;
	bool bTraced;

	bBusy = __xrtTypedTreeCallbackBegin(pTree);
	bTraced = xrtTypeTraceValue(
		pTree->KeyType,
		__xrtTypedTreeKey(pTree, pEntry),
		pContext->Visit,
		pContext->UserData
	) && xrtTypeTraceValue(
		pTree->ValueType,
		__xrtTypedTreeValue(pTree, pEntry),
		pContext->Visit,
		pContext->UserData
	);
	__xrtTypedTreeCallbackEnd(pTree, bBusy);
	if ( !bTraced ) {
		pContext->Failed = true;
		return false;
	}
	return true;
}



/* 枚举类型树所有键值直接拥有的强对象引用。 */
static bool __xrtTypedTreeInstanceTrace(
	const void* pInstance,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	xtypedtree* pTree = (xtypedtree*)pInstance;
	xtypedtreetracecontext Context;
	size_t iExpected;
	size_t iVisited;
	(void)pType;

	if ( !__xrtTypedTreeValid(pTree, "instance-trace") ) {
		return false;
	}
	Context.Tree = pTree;
	Context.Visit = pVisit;
	Context.UserData = pContext;
	Context.Failed = false;
	iExpected = pTree->Storage.Base.Count;
	iVisited = xrtAVLTreeVisit(
		&pTree->Storage, __xrtTypedTreeTraceEntry, &Context
	);
	if ( Context.Failed ) {
		return false;
	}
	if ( iVisited != iExpected ) {
		__xrtTypedTreeWrap(XERR_STATE, XTYPED_TREE_ERROR_STATE,
			"instance-trace", "the typed tree trace visit was incomplete");
		return false;
	}
	return true;
}



/* 返回对象树负载共享的实例操作表。 */
XRT_API const xrtinstanceops* xrtTypedTreeInstanceOps(void)
{
	static const xrtinstanceops Ops = {
		.Init = __xrtTypedTreeInstanceInit,
		.Drop = __xrtTypedTreeInstanceDrop,
		.Trace = __xrtTypedTreeInstanceTrace
	};

	return &Ops;
}



/* 验证可由对象系统承载的泛型有序字典类型描述。 */
XRT_API bool xrtTypedTreeTypeValidate(const xrttype* pType)
{
	if ( !xrtTypeValidate(pType) ) {
		__xrtTypedTreeWrap(XERR_ARGUMENT, XTYPED_TREE_ERROR_TYPE,
			"type-validate", "the typed tree object type is invalid");
		return false;
	}
	if (
		(pType->Kind != XRT_TYPE_DICT) ||
		((pType->Flags & XRT_TYPE_FLAG_REFERENCE) == 0u) ||
		(pType->ArgumentCount != 2u) ||
		(pType->Arguments == NULL) ||
		(pType->InstanceSize != sizeof(xtypedtree)) ||
		(pType->InstanceAlign <
		 XRT_INTERNAL_OBJECT_ALIGNOF(xtypedtree)) ||
		(pType->InstanceOps != xrtTypedTreeInstanceOps())
	) {
		__xrtTypedTreeError(XERR_TYPE, XTYPED_TREE_ERROR_TYPE,
			"type-validate", "the typed tree object type contract is invalid");
		return false;
	}
	return __xrtTypedTreeValueTypeValidate(
		pType->Arguments[0], true, "type-validate"
	) && __xrtTypedTreeValueTypeValidate(
		pType->Arguments[1], false, "type-validate"
	);
}

#endif
