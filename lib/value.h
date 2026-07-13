


// 静态值 : null、true、false
static xvalue_struct XVO_VALUE_NULL = {
	.Header = XVO_HEADER_INIT(XVO_DT_NULL, TRUE, FALSE, 0),
	.Size = 0,
	.vInt = 0
};
static xvalue_struct XVO_VALUE_TRUE = {
	.Header = XVO_HEADER_INIT(XVO_DT_BOOL, TRUE, FALSE, 0),
	.Size = sizeof(bool),
	.vBool = TRUE
};
static xvalue_struct XVO_VALUE_FALSE = {
	.Header = XVO_HEADER_INIT(XVO_DT_BOOL, TRUE, FALSE, 0),
	.Size = sizeof(bool),
	.vBool = FALSE
};

#ifndef MAKE_COLL_KEY
	#if defined(__x86_64__) || defined(_M_X64) || UINTPTR_MAX > 0xffffffffu
		#define MAKE_COLL_KEY(k, v) if ( (v)->Type == XVO_DT_TEXT ) { uint64 iHash = xrtHash64((v)->vText, (v)->Size); (k).Hash = ((uint64)(v)->Type << 60) | ((uint64)(v)->Size << 28) | (iHash & 0xFFFFFFF); } else if ( (v)->Type == XVO_DT_BOOL ) { (k).Hash = ((uint64)(v)->Type << 60) | (v)->vBool; } else if ( (v)->Type == XVO_DT_NULL ) { (k).Hash = (uint64)(v)->Type << 60; } else { (k).Hash = ((uint64)(v)->Type << 60) | ((v)->vInt & 0xFFFFFFFFFFFFFFF); } (k).Value = (v);
	#else
		#define MAKE_COLL_KEY(k, v) if ( (v)->Type == XVO_DT_TEXT ) { uint32 iHash = xrtHash32((v)->vText, (v)->Size); (k).Hash = ((uint64)(v)->Type << 60) | ((uint64)(v)->Size << 28) | (iHash & 0xFFFFFFF); } else if ( (v)->Type == XVO_DT_BOOL ) { (k).Hash = ((uint64)(v)->Type << 60) | (v)->vBool; } else if ( (v)->Type == XVO_DT_NULL ) { (k).Hash = (uint64)(v)->Type << 60; } else { (k).Hash = ((uint64)(v)->Type << 60) | ((v)->vInt & 0xFFFFFFFFFFFFFFF); } (k).Value = (v);
	#endif
#endif

static bool __xvoEnsureContainerUnique(xvalue pVal);



// 引用计数操作
XXAPI void xvoAddRef(xvalue pVal)
{
	xvoAddRef_Inline(pVal);
}


// 判断动态值是否已经进入共享所有权模式
XXAPI bool xvoIsShared(xvalue pVal)
{
	return xvoIsShared_Inline(pVal);
}


// 释放列表 clear 进程
bool xvoListClear_FreeProc(int64 pKey, xvalue* ppVal, xlist pList)
{
	(void)pKey;
	(void)pList;
	xvoUnref(*ppVal);
	return FALSE;
}


// 释放 coll 节点进程
static void xvoCollNode_FreeProc(xavltree pColl, Coll_Key* pKey)
{
	(void)pColl;
	xvoUnref(pKey->Value);
}


// xvoTableClear_FreeProc 相关处理
bool xvoTableClear_FreeProc(Dict_Key* pKey, xvalue* ppVal, xdict pTbl)
{
	(void)pKey;
	(void)pTbl;
	xvoUnref(*ppVal);
	return FALSE;
}


// 判断函数值是否使用新的 callable 对象承载
static bool __xvoIsCallableValue(xvalue pVal)
{
	return (pVal != NULL) && (pVal->Type == XVO_DT_FUNC) && (pVal->Size == sizeof(xrt_callable)) && (pVal->vCallable != NULL);
}


// 判断集合值是否使用新的 XRT set 对象承载
static bool __xvoIsSetValue(xvalue pVal)
{
	return (pVal != NULL) && (pVal->Type == XVO_DT_COLL) && (pVal->Size == sizeof(xset_struct)) && (pVal->vSet != NULL);
}


// 判断复合值是否仍由本地容器根承载
static bool __xvoNeedsSharedRootCopy(xvalue pVal)
{
	if ( pVal == NULL ) {
		return FALSE;
	}
	switch ( pVal->Type ) {
		case XVO_DT_ARRAY:
			return !xrtOwnerIsRealShared(&pVal->vArray->Owner);
		case XVO_DT_LIST:
			return !xrtOwnerIsRealShared(&pVal->vList->Owner);
		case XVO_DT_TABLE:
			return !xrtOwnerIsRealShared(&pVal->vTable->Owner);
		case XVO_DT_COLL:
			return __xvoIsSetValue(pVal)
				? !xrtOwnerIsRealShared(&pVal->vSet->Owner)
				: !xrtOwnerIsRealShared(&pVal->vColl->Owner);
		default:
			return FALSE;
	}
}


/*
	共享容器不能直接持有本地容器根。这里在写入边界创建共享图副本，
	让 array/list/set/dict 的所有公开写入 API 使用同一套发布规则。
*/
static bool __xvoPrepareStoredValue(
	const xrtOwnerInfo* pOwner,
	xvalue pSource,
	xvalue* ppStored,
	bool* pCopied)
{
	if ( pSource == NULL || ppStored == NULL || pCopied == NULL ) {
		return FALSE;
	}
	*ppStored = pSource;
	*pCopied = FALSE;
	if ( !xrtOwnerIsRealShared(pOwner) ) {
		return TRUE;
	}
	if ( __xvoNeedsSharedRootCopy(pSource) ) {
		*ppStored = xvoDeepCopyEx(pSource, XRT_OBJMODE_SHARED);
		*pCopied = *ppStored != NULL;
		return *pCopied;
	}
	return xvoMakeShared_Inline(pSource);
}


// 直接存储指针的容器在提交成功后统一处理引用所有权
static void __xvoCommitStoredValue(xvalue pSource, xvalue pStored, bool bCopied, bool bColloc)
{
	if ( bCopied ) {
		if ( bColloc && pSource != NULL && !pSource->IsStatic ) {
			xvoUnref(pSource);
		}
	} else if ( !bColloc && pStored != NULL && !pStored->IsStatic ) {
		xvoAddRef_Inline(pStored);
	}
}


static void __xvoDiscardPreparedValue(xvalue pStored, bool bCopied)
{
	if ( bCopied && pStored != NULL ) {
		xvoUnref(pStored);
	}
}


// XRT set 中的 xvalue 元素哈希
static uint64 __xvoSetValueHash(const ptr pObj)
{
	xvalue pVal = pObj ? *(const xvalue*)pObj : NULL;
	Coll_Key objKey;
	if ( pVal == NULL ) {
		pVal = &XVO_VALUE_NULL;
	}
	MAKE_COLL_KEY(objKey, pVal);
	return objKey.Hash;
}


// XRT set 中的 xvalue 元素比较
static int __xvoSetValueCompare(const ptr pA, const ptr pB)
{
	xvalue pLeft = pA ? *(const xvalue*)pA : NULL;
	xvalue pRight = pB ? *(const xvalue*)pB : NULL;
	Coll_Key leftKey;
	Coll_Key rightKey;

	if ( pLeft == NULL ) {
		pLeft = &XVO_VALUE_NULL;
	}
	if ( pRight == NULL ) {
		pRight = &XVO_VALUE_NULL;
	}
	if ( pLeft == pRight ) {
		return 0;
	}
	MAKE_COLL_KEY(leftKey, pLeft);
	MAKE_COLL_KEY(rightKey, pRight);
	if ( leftKey.Hash != rightKey.Hash ) {
		return (leftKey.Hash > rightKey.Hash) ? 1 : -1;
	}
	if ( pLeft->Type != pRight->Type ) {
		return (pLeft->Type > pRight->Type) ? 1 : -1;
	}
	if ( pLeft->Type == XVO_DT_TEXT ) {
		uint32 iMinSize = (pLeft->Size < pRight->Size) ? pLeft->Size : pRight->Size;
		int iCmp = memcmp(pLeft->vText, pRight->vText, iMinSize);
		if ( iCmp != 0 ) {
			return iCmp;
		}
		return (pLeft->Size > pRight->Size) ? 1 : ((pLeft->Size < pRight->Size) ? -1 : 0);
	}
	return (pLeft->vInt > pRight->vInt) ? 1 : ((pLeft->vInt < pRight->vInt) ? -1 : 0);
}


// XRT set 中的 xvalue 元素复制
static void __xvoSetValueCopy(ptr pDst, const ptr pSrc)
{
	xvalue pVal = pSrc ? *(const xvalue*)pSrc : NULL;
	if ( pVal == NULL ) {
		pVal = &XVO_VALUE_NULL;
	}
	*(xvalue*)pDst = pVal;
	xvoAddRef_Inline(pVal);
}


// XRT set 中的 xvalue 元素释放
static void __xvoSetValueDrop(ptr pObj)
{
	xvalue pVal = pObj ? *(xvalue*)pObj : NULL;
	if ( pVal ) {
		xvoUnref(pVal);
	}
}


// 将 xvalue 句柄装箱为自身，并为返回值增加一份引用。
static xvalue __xvoSetValueBox(const ptr pObj, const xrt_type_desc* pType)
{
	xvalue pVal = pObj ? *(const xvalue*)pObj : NULL;
	(void)pType;
	if ( pVal == NULL ) {
		pVal = &XVO_VALUE_NULL;
	}
	xvoAddRef_Inline(pVal);
	return pVal;
}


// 将任意动态值解箱为 xvalue 句柄；目标槽拥有新增的引用。
static bool __xvoSetValueUnbox(xvalue pVal, ptr pOut, const xrt_type_desc* pType)
{
	(void)pType;
	if ( pOut == NULL ) {
		return FALSE;
	}
	if ( pVal == NULL ) {
		pVal = &XVO_VALUE_NULL;
	}
	*(xvalue*)pOut = pVal;
	xvoAddRef_Inline(pVal);
	return TRUE;
}


static const xrt_type_ops __xvoSetValueOps = {
	.init = NULL,
	.copy = __xvoSetValueCopy,
	.move = NULL,
	.drop = __xvoSetValueDrop,
	.compare = __xvoSetValueCompare,
	.hash = __xvoSetValueHash,
	.to_string = NULL,
	.box = __xvoSetValueBox,
	.unbox = __xvoSetValueUnbox
};

/*
	所有动态值外壳统一经过 XRT 的线程本地 size-class 内存池。
	单独保留入口，既避免 value.h 散落分配策略，也便于后续统计和调优。
*/
static xvalue __xvoAllocValue()
{
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal != NULL ) {
		memset(pVal, 0, sizeof(xvalue_struct));
	}
	return pVal;
}


static void __xvoFreeValue(xvalue pVal)
{
	xrtFree(pVal);
}

static const xrt_type_desc __xvoSetValueType = {
	.TypeId = 0,
	.Kind = XRT_TYPE_KIND_INVALID,
	.Name = "xvalue",
	.NameSize = 6,
	.AbiName = "xvalue",
	.AbiNameSize = 6,
	.Size = sizeof(xvalue),
	.Align = sizeof(ptr),
	.Ops = &__xvoSetValueOps,
	.Extra = NULL
};


// XRT 的通用动态值类型，可直接作为静态容器元素类型使用。
XXAPI const xrt_type_desc* xrtTypeValue()
{
	return &__xvoSetValueType;
}

typedef struct {
	str Key;
	uint32 KeyLen;
} xvo_table_order_item;

typedef struct {
	volatile uint32 RefCount;
	const xrt_type_desc* TypeDesc;
	xarray Order;
} xvo_container_extra;

typedef xvo_container_extra xvo_table_extra;


static bool __xvoIsContainerValue(xvalue pVal)
{
	return pVal != NULL &&
		(pVal->Type == XVO_DT_ARRAY ||
		 pVal->Type == XVO_DT_LIST ||
		 pVal->Type == XVO_DT_COLL ||
		 pVal->Type == XVO_DT_TABLE);
}


static xvo_container_extra* __xvoContainerExtra(xvalue pVal)
{
	return __xvoIsContainerValue(pVal) ? (xvo_container_extra*)pVal->vExtra : NULL;
}


static xvo_container_extra* __xvoContainerExtraCreate(uint32 iMode, bool bTable)
{
	xvo_container_extra* pExtra = xrtCalloc(1, sizeof(xvo_container_extra));
	if ( pExtra == NULL ) {
		return NULL;
	}
	pExtra->RefCount = 1;
	if ( bTable ) {
		pExtra->Order = xrtArrayCreate(sizeof(xvo_table_order_item), iMode);
		if ( pExtra->Order == NULL ) {
			xrtFree(pExtra);
			return NULL;
		}
	}
	return pExtra;
}


static void __xvoContainerExtraAddRef(xvo_container_extra* pExtra)
{
	uint32 iOld;
	uint32 iNew;
	if ( pExtra == NULL ) {
		return;
	}
	do {
		iOld = pExtra->RefCount;
		iNew = iOld == UINT32_MAX ? UINT32_MAX : iOld + 1;
	} while ( __xvoAtomicCompareExchange32(&pExtra->RefCount, iNew, iOld) != iOld );
}


/* 返回 TRUE 表示调用方持有最后一个 backing 引用。 */
static bool __xvoContainerExtraRelease(xvo_container_extra* pExtra)
{
	uint32 iOld;
	uint32 iNew;
	if ( pExtra == NULL ) {
		return TRUE;
	}
	do {
		iOld = pExtra->RefCount;
		if ( iOld == UINT32_MAX ) {
			return FALSE;
		}
		if ( iOld == 0 ) {
			return FALSE;
		}
		iNew = iOld - 1;
	} while ( __xvoAtomicCompareExchange32(&pExtra->RefCount, iNew, iOld) != iOld );
	return iOld == 1;
}

static xvo_table_extra* __xvoTableExtra(xvalue pTbl)
{
	if ( pTbl == NULL || pTbl->Type != XVO_DT_TABLE ) {
		return NULL;
	}
	return (xvo_table_extra*)pTbl->vExtra;
}

static const xrt_type_desc* __xvoValueTypeDesc(xvalue pVal)
{
	xvo_container_extra* pExtra;

	if ( pVal == NULL ) {
		return NULL;
	}
	if ( __xvoIsContainerValue(pVal) ) {
		pExtra = __xvoContainerExtra(pVal);
		return pExtra ? pExtra->TypeDesc : NULL;
	}
	return pVal->vTypeDesc;
}

static bool __xvoSetValueTypeDesc(xvalue pVal, const xrt_type_desc* pType)
{
	xvo_container_extra* pExtra;

	if ( pVal == NULL ) {
		return FALSE;
	}
	if ( __xvoIsContainerValue(pVal) ) {
		pExtra = __xvoContainerExtra(pVal);
		if ( pExtra == NULL ) {
			return FALSE;
		}
		pExtra->TypeDesc = pType;
		return TRUE;
	}
	pVal->vTypeDesc = pType;
	return TRUE;
}

static xarray __xvoTableOrder(xvalue pTbl)
{
	xvo_table_extra* pExtra = __xvoTableExtra(pTbl);
	return pExtra ? pExtra->Order : NULL;
}

static bool __xvoTableOrderAppend(xvalue pTbl, str sKey, uint32 iKeyLen)
{
	xarray pOrder = __xvoTableOrder(pTbl);
	xvo_table_order_item* pItem;
	uint32 iPos;
	str sCopy;

	if ( pOrder == NULL ) {
		return FALSE;
	}
	sCopy = xrtMalloc((size_t)iKeyLen + 1);
	if ( sCopy == NULL ) {
		return FALSE;
	}
	if ( iKeyLen > 0 && sKey != NULL ) {
		memcpy(sCopy, sKey, iKeyLen);
	}
	sCopy[iKeyLen] = '\0';
	iPos = xrtArrayAppend(pOrder, 1);
	if ( iPos == 0 ) {
		xrtFree(sCopy);
		return FALSE;
	}
	pItem = (xvo_table_order_item*)xrtArrayGet_Unsafe(pOrder, iPos);
	if ( pItem == NULL ) {
		xrtFree(sCopy);
		xrtArrayRemove(pOrder, iPos, 1);
		return FALSE;
	}
	pItem->Key = sCopy;
	pItem->KeyLen = iKeyLen;
	return TRUE;
}

static bool __xvoTableOrderRemove(xvalue pTbl, str sKey, uint32 iKeyLen)
{
	xarray pOrder = __xvoTableOrder(pTbl);
	uint32 i;

	if ( pOrder == NULL ) {
		return FALSE;
	}
	for ( i = 1; i <= pOrder->Count; ++i ) {
		xvo_table_order_item* pItem = (xvo_table_order_item*)xrtArrayGet_Unsafe(pOrder, i);
		if ( pItem != NULL &&
			 pItem->KeyLen == iKeyLen &&
			 memcmp(pItem->Key, sKey, iKeyLen) == 0 ) {
			xrtFree(pItem->Key);
			xrtArrayRemove(pOrder, i, 1);
			return TRUE;
		}
	}
	return FALSE;
}

static void __xvoTableOrderClear(xvalue pTbl)
{
	xarray pOrder = __xvoTableOrder(pTbl);
	uint32 i;

	if ( pOrder == NULL ) {
		return;
	}
	for ( i = 1; i <= pOrder->Count; ++i ) {
		xvo_table_order_item* pItem = (xvo_table_order_item*)xrtArrayGet_Unsafe(pOrder, i);
		if ( pItem != NULL ) {
			xrtFree(pItem->Key);
			pItem->Key = NULL;
			pItem->KeyLen = 0;
		}
	}
	if ( pOrder->Count > 0 ) {
		xrtArrayRemove(pOrder, 1, pOrder->Count);
	}
}

static void __xvoTableExtraDestroy(xvalue pTbl)
{
	xvo_table_extra* pExtra = __xvoTableExtra(pTbl);

	if ( pExtra == NULL ) {
		return;
	}
	__xvoTableOrderClear(pTbl);
	if ( pExtra->Order != NULL ) {
		xrtArrayDestroy(pExtra->Order);
	}
	xrtFree(pExtra);
	pTbl->vExtra = NULL;
}

static bool xvoTableWalkOrdered(xvalue pTbl, Dict_EachProc procEach, ptr pArg)
{
	xarray pOrder = __xvoTableOrder(pTbl);
	uint32 i;

	if ( pTbl == NULL || pTbl->Type != XVO_DT_TABLE || pOrder == NULL || procEach == NULL ) {
		return FALSE;
	}
	for ( i = 1; i <= pOrder->Count; ++i ) {
		xvo_table_order_item* pItem = (xvo_table_order_item*)xrtArrayGet_Unsafe(pOrder, i);
		xvalue* ppVal;
		Dict_Key tKey;

		if ( pItem == NULL || pItem->Key == NULL ) {
			continue;
		}
		ppVal = (xvalue*)xrtDictGet(pTbl->vTable, pItem->Key, pItem->KeyLen);
		if ( ppVal == NULL ) {
			continue;
		}
		tKey.Key = pItem->Key;
		tKey.KeyLen = pItem->KeyLen;
		tKey.Hash = 0;
		if ( procEach(&tKey, ppVal, pArg) ) {
			return TRUE;
		}
	}
	return TRUE;
}


// 释放 record/class box
static void __xvoDestroyRecordValue(xvalue pVal)
{
	xrt_record_value* pRecord;
	if ( pVal->vTypeDesc == NULL ) {
		xrtFree(pVal->vStruct);
		return;
	}
	pRecord = (xrt_record_value*)pVal->vStruct;
	if ( pRecord ) {
		if ( pRecord->Type && pRecord->Type->Ops && pRecord->Type->Ops->drop ) {
			pRecord->Type->Ops->drop(pRecord->Data);
		}
		xrtFree(pRecord);
	}
}


// 释放 handle/custom box
static void __xvoDestroyHandleValue(xvalue pVal)
{
	xrt_handle_value* pHandle;
	if ( pVal->vTypeDesc == NULL ) {
		return;
	}
	pHandle = (xrt_handle_value*)pVal->vCustom;
	if ( pHandle ) {
		if ( (pHandle->Flags & XRT_HANDLE_FLAG_OWNED) && pHandle->Type && pHandle->Type->Ops && pHandle->Type->Ops->drop ) {
			pHandle->Type->Ops->drop(&pHandle->Handle);
		}
		xrtFree(pHandle);
	}
}


// 内部函数：销毁值
static void __xvoDestroyValue(xvalue pVal)
{
	xvo_container_extra* pContainerExtra = __xvoContainerExtra(pVal);

	/* 多个 xvalue 外壳可以共享同一个容器 backing，只有最后一个外壳销毁根。 */
	if ( pContainerExtra != NULL && !__xvoContainerExtraRelease(pContainerExtra) ) {
		__xvoFreeValue(pVal);
		return;
	}
	// 根据数据类型释放对应的资源
	if ( pVal->Type == XVO_DT_TEXT ) {
		// 释放文本字符串
		xrtFree(pVal->vText);
	} else if ( pVal->Type == XVO_DT_ARRAY ) {
		// 释放数组中所有元素的引用
		for ( uint32 i = 1; i <= pVal->vArray->Count; i++ ) {
			xvalue pItem = xrtPtrArrayGet_Inline(pVal->vArray, i);
			xvoUnref(pItem);
		}
		// 销毁数组容器
		xrtPtrArrayDestroy(pVal->vArray);
	} else if ( pVal->Type == XVO_DT_LIST ) {
		// 释放列表中所有元素的引用
		xrtListWalk(pVal->vList, (ptr)xvoListClear_FreeProc, pVal->vList);
		// 销毁列表容器
		(xrtListDestroy)(pVal->vList);
	} else if ( pVal->Type == XVO_DT_COLL ) {
		if ( __xvoIsSetValue(pVal) ) {
			// 新集合由 XRT set 负责元素生命周期
			(xrtSetDestroy)(pVal->vSet);
		} else {
			// 旧集合容器（节点释放由 FreeProc 回调处理）
			(xrtAVLTreeDestroy)(pVal->vColl);
		}
	} else if ( pVal->Type == XVO_DT_TABLE ) {
		// 释放字典中所有值的引用
		xrtDictWalk(pVal->vTable, (ptr)xvoTableClear_FreeProc, pVal->vTable);
		// 释放字典插入顺序与类型描述扩展块
		__xvoTableExtraDestroy(pVal);
		// 销毁字典容器
		(xrtDictDestroy)(pVal->vTable);
	} else if ( pVal->Type == XVO_DT_FUNC ) {
		if ( __xvoIsCallableValue(pVal) ) {
			xrtCallableUnref(pVal->vCallable);
		} else if ( pVal->vFuncEnv != NULL ) {
			xvoUnref(pVal->vFuncEnv);
		}
	} else if ( pVal->Type == XVO_DT_CLASS ) {
		// 释放类数据块或 typed record box
		__xvoDestroyRecordValue(pVal);
	} else if ( pVal->Type == XVO_DT_CUSTOM ) {
		// 释放 typed handle box；裸 custom 不拥有资源
		__xvoDestroyHandleValue(pVal);
	}
	if ( pContainerExtra != NULL && pVal->Type != XVO_DT_TABLE ) {
		xrtFree(pContainerExtra);
		pVal->vExtra = NULL;
	}
	// 释放值结构体自身
	__xvoFreeValue(pVal);
	#ifdef DEBUG_TRACE
		printf("free value : %x\n", pVal);
	#endif
}


// xvoUnref 相关处理
XXAPI void xvoUnref(xvalue pVal)
{
	uint32 iOldHeader;
	uint32 iNewHeader;
	uint32 iRefCount;
	if ( pVal ) {
		// 非共享对象走快速路径
		if ( !xvoIsShared_Inline(pVal) ) {
			if ( pVal->IsStatic == 0 ) {
				// 减少引用计数
				pVal->RefCount--;
				// 引用计数归零则销毁值
				if ( pVal->RefCount == 0 ) {
					__xvoDestroyValue(pVal);
				}
			}
			return;
		}
		// 共享对象使用 CAS 原子操作减少引用计数
		while ( TRUE ) {
			// 读取当前头部信息
			iOldHeader = pVal->Header;
			// 静态值不需要释放
			if ( iOldHeader & XVO_HEADER_STATIC_MASK ) {
				return;
			}
			// 提取当前引用计数
			iRefCount = (iOldHeader & XVO_HEADER_REFCOUNT_MASK) >> XVO_HEADER_REFCOUNT_SHIFT;
			// 引用计数为零则无需操作
			if ( iRefCount == 0 ) {
				return;
			}
			// 计算减少引用计数后的新头部值
			iNewHeader = (iOldHeader & ~XVO_HEADER_REFCOUNT_MASK) | ((iRefCount - 1) << XVO_HEADER_REFCOUNT_SHIFT);
			// 尝试原子交换，成功则退出循环
			if ( __xvoAtomicCompareExchange32(&pVal->Header, iNewHeader, iOldHeader) == iOldHeader ) {
				break;
			}
		}
		// 引用计数从 1 减为 0，销毁值
		if ( iRefCount == 1 ) {
			__xvoDestroyValue(pVal);
		}
	}
}



// 创建值
XXAPI xvalue xvoCreateNull()
{
	return &XVO_VALUE_NULL;
}


// 创建布尔值
XXAPI xvalue xvoCreateBool(bool bVal)
{
	if ( bVal ) {
		return &XVO_VALUE_TRUE;
	} else {
		return &XVO_VALUE_FALSE;
	}
}


// 创建整数
XXAPI xvalue xvoCreateInt(int64 iVal)
{
	xvalue pVal = __xvoAllocValue();
	if ( pVal ) {
		xvoInitOwnedHeader_Inline(pVal, XVO_DT_INT, XRT_OBJMODE_LOCAL);
		pVal->Size = sizeof(int64);
		pVal->vInt = iVal;
	}
	return pVal;
}


// 创建浮点数
XXAPI xvalue xvoCreateFloat(double fVal)
{
	xvalue pVal = __xvoAllocValue();
	if ( pVal ) {
		xvoInitOwnedHeader_Inline(pVal, XVO_DT_FLOAT, XRT_OBJMODE_LOCAL);
		pVal->Size = sizeof(double);
		pVal->vFloat = fVal;
	}
	return pVal;
}


// 创建文本
XXAPI xvalue xvoCreateText(ptr sVal, uint32 iSize, bool bColloc)
{
	ptr pOwnedEmpty = NULL;

	// 处理空指针输入，使用共享空字符串
	if ( sVal == NULL ) {
		sVal = xCore.sNull;
		iSize = 0;
		bColloc = TRUE;
	} else if ( iSize == 0 ) {
		// 计算字符串长度
		iSize = strlen(sVal);
		// 空字符串使用共享空字符串
		if ( iSize == 0 ) {
			if ( bColloc && (sVal != xCore.sNull) ) {
				// 记录需要释放的原有空字符串
				pOwnedEmpty = sVal;
			}
			sVal = xCore.sNull;
			bColloc = TRUE;
		}
	}
	// 分配值结构体
	xvalue pVal = __xvoAllocValue();
	if ( pVal ) {
		// 初始化头部信息
		xvoInitOwnedHeader_Inline(pVal, XVO_DT_TEXT, XRT_OBJMODE_LOCAL);
		pVal->Size = iSize;
		// 设置文本数据指针
		if ( bColloc ) {
			// 直接使用传入的指针（所有权转移）
			pVal->vText = sVal;
		} else {
			// 复制字符串内容
			pVal->vText = xrtCopyStr(sVal, iSize);
			if ( pVal->vText == xCore.sNull ) {
				// 复制失败，释放已分配的值结构体
				__xvoFreeValue(pVal);
				return NULL;
			}
		}
		// 释放之前记录的原有空字符串
		if ( pOwnedEmpty != NULL ) {
			xrtFree(pOwnedEmpty);
		}
	}
	return pVal;
}


// xvoCreateTime 相关处理
XXAPI xvalue xvoCreateTime(xtime tVal)
{
	xvalue pVal = __xvoAllocValue();
	if ( pVal ) {
		xvoInitOwnedHeader_Inline(pVal, XVO_DT_TIME, XRT_OBJMODE_LOCAL);
		pVal->Size = sizeof(xtime);
		pVal->vTime = tVal;
	}
	return pVal;
}


// xvoCreateTimeSerial 相关处理
XXAPI xvalue xvoCreateTimeSerial(int64 iYear, int iMonth, int iDay, int iHour, int iMinute, int iSecond)
{
	xvalue pVal = __xvoAllocValue();
	if ( pVal ) {
		xvoInitOwnedHeader_Inline(pVal, XVO_DT_TIME, XRT_OBJMODE_LOCAL);
		pVal->Size = sizeof(xtime);
		pVal->vTime = xrtDateTimeSerial(iYear, iMonth, iDay, iHour, iMinute, iSecond);
	}
	return pVal;
}


// xvoCreatePoint 相关处理
XXAPI xvalue xvoCreatePoint(ptr point)
{
	xvalue pVal = __xvoAllocValue();
	if ( pVal ) {
		xvoInitOwnedHeader_Inline(pVal, XVO_DT_POINT, XRT_OBJMODE_LOCAL);
		pVal->Size = sizeof(ptr);
		pVal->vPoint = point;
	}
	return pVal;
}


// xvoCreateFunc 相关处理
XXAPI xvalue xvoCreateFunc(xfunction pFunc)
{
	xvalue pVal = __xvoAllocValue();
	if ( pVal ) {
		xvoInitOwnedHeader_Inline(pVal, XVO_DT_FUNC, XRT_OBJMODE_LOCAL);
		pVal->Size = sizeof(ptr);
		pVal->vFunc = pFunc;
		pVal->vFuncEnv = NULL;
	}
	return pVal;
}


// 创建 callable 函数值
XXAPI xvalue xvoCreateCallable(xrt_callable* pCallable, bool bColloc)
{
	xvalue pVal;
	if ( pCallable == NULL ) {
		return NULL;
	}
	pVal = __xvoAllocValue();
	if ( pVal ) {
		xvoInitOwnedHeader_Inline(pVal, XVO_DT_FUNC, XRT_OBJMODE_LOCAL);
		pVal->Size = sizeof(xrt_callable);
		pVal->vCallable = pCallable;
		pVal->vExtra = NULL;
		if ( !bColloc ) {
			xrtCallableAddRef(pCallable);
		}
	}
	return pVal;
}


// 创建数组
XXAPI xvalue xvoCreateArray()
{
	return xvoCreateArrayEx(XRT_OBJMODE_LOCAL);
}


// 创建数组扩展
XXAPI xvalue xvoCreateArrayEx(uint32 iMode)
{
	xvalue pVal = __xvoAllocValue();
	if ( pVal ) {
		xparray objArr = xrtPtrArrayCreate(iMode);
		xvo_container_extra* pExtra = __xvoContainerExtraCreate(iMode, FALSE);
		if ( objArr == NULL || pExtra == NULL ) {
			if ( objArr != NULL ) {
				xrtPtrArrayDestroy(objArr);
			}
			if ( pExtra != NULL ) {
				xrtFree(pExtra);
			}
			__xvoFreeValue(pVal);
			return NULL;
		}
		xvoInitOwnedHeader_Inline(pVal, XVO_DT_ARRAY, iMode);
		pVal->Size = 0;
		pVal->vArray = objArr;
		pVal->vExtra = pExtra;
	}
	return pVal;
}


// 创建列表
XXAPI xvalue xvoCreateList()
{
	return xvoCreateListEx(XRT_OBJMODE_LOCAL);
}


// 创建列表扩展
XXAPI xvalue xvoCreateListEx(uint32 iMode)
{
	xvalue pVal = __xvoAllocValue();
	if ( pVal ) {
		xlist objList = xrtListCreate(sizeof(xvalue), iMode);
		xvo_container_extra* pExtra = __xvoContainerExtraCreate(iMode, FALSE);
		if ( objList == NULL || pExtra == NULL ) {
			if ( objList != NULL ) {
				xrtListDestroy(objList);
			}
			if ( pExtra != NULL ) {
				xrtFree(pExtra);
			}
			__xvoFreeValue(pVal);
			return NULL;
		}
		xvoInitOwnedHeader_Inline(pVal, XVO_DT_LIST, iMode);
		pVal->Size = 0;
		pVal->vList = objList;
		pVal->vExtra = pExtra;
	}
	return pVal;
}


// xvoCreateColl 相关处理
XXAPI xvalue xvoCreateColl()
{
	return xvoCreateCollEx(XRT_OBJMODE_LOCAL);
}


// xvoCreateCollEx 相关处理
XXAPI xvalue xvoCreateCollEx(uint32 iMode)
{
	xvalue pVal = __xvoAllocValue();
	if ( pVal ) {
		int Coll_CompProc(Coll_Key* pNode, Coll_Key* pObjKey);	// 比较函数定义
		xavltree objColl = xrtAVLTreeCreate(sizeof(Coll_Key), (ptr)Coll_CompProc, iMode);
		xvo_container_extra* pExtra = __xvoContainerExtraCreate(iMode, FALSE);
		if ( objColl == NULL || pExtra == NULL ) {
			if ( objColl != NULL ) {
				xrtAVLTreeDestroy(objColl);
			}
			if ( pExtra != NULL ) {
				xrtFree(pExtra);
			}
			__xvoFreeValue(pVal);
			return NULL;
		}
		objColl->FreeProc = (ptr)xvoCollNode_FreeProc;
		xvoInitOwnedHeader_Inline(pVal, XVO_DT_COLL, iMode);
		pVal->Size = 0;
		pVal->vColl = objColl;
		pVal->vExtra = pExtra;
	}
	return pVal;
}


// 创建基于 XRT set 的集合值
XXAPI xvalue xvoCreateSet()
{
	return xvoCreateSetEx(XRT_OBJMODE_LOCAL);
}


// 创建基于 XRT set 且带所有权模式的集合值
XXAPI xvalue xvoCreateSetEx(uint32 iMode)
{
	xvalue pVal = __xvoAllocValue();
	if ( pVal ) {
		xset pSet = xrtSetCreate(&__xvoSetValueType, iMode);
		xvo_container_extra* pExtra = __xvoContainerExtraCreate(iMode, FALSE);
		if ( pSet == NULL || pExtra == NULL ) {
			if ( pSet != NULL ) {
				xrtSetDestroy(pSet);
			}
			if ( pExtra != NULL ) {
				xrtFree(pExtra);
			}
			__xvoFreeValue(pVal);
			return NULL;
		}
		xvoInitOwnedHeader_Inline(pVal, XVO_DT_COLL, iMode);
		pVal->Size = sizeof(xset_struct);
		pVal->vSet = pSet;
		pVal->vExtra = pExtra;
	}
	return pVal;
}


// xvoCreateTable 相关处理
XXAPI xvalue xvoCreateTable()
{
	return xvoCreateTableEx(XRT_OBJMODE_LOCAL);
}


// xvoCreateTableEx 相关处理
XXAPI xvalue xvoCreateTableEx(uint32 iMode)
{
	xvalue pVal = __xvoAllocValue();
	if ( pVal ) {
		xdict objTbl = xrtDictCreate(sizeof(xvalue), iMode);
		xvo_table_extra* pExtra;
		if ( objTbl == NULL ) {
			__xvoFreeValue(pVal);
			return NULL;
		}
		pExtra = __xvoContainerExtraCreate(iMode, TRUE);
		if ( pExtra == NULL ) {
			(xrtDictDestroy)(objTbl);
			__xvoFreeValue(pVal);
			return NULL;
		}
		xvoInitOwnedHeader_Inline(pVal, XVO_DT_TABLE, iMode);
		pVal->Size = 0;
		pVal->vTable = objTbl;
		pVal->vExtra = pExtra;
	}
	return pVal;
}


// 创建分类
XXAPI xvalue xvoCreateClass(uint32 iSize)
{
	if ( iSize == 0 ) {
		return NULL;
	}
	xvalue pVal = __xvoAllocValue();
	if ( pVal ) {
		ptr pStruct = xrtMalloc(iSize);
		if ( pStruct == NULL ) {
			__xvoFreeValue(pVal);
			return NULL;
		}
		xvoInitOwnedHeader_Inline(pVal, XVO_DT_CLASS, XRT_OBJMODE_LOCAL);
		pVal->Size = iSize;
		pVal->vStruct = pStruct;
		pVal->vTypeDesc = NULL;
	}
	return pVal;
}


// 创建 record/class 动态值
static xvalue __xvoCreateRecord(const xrt_type_desc* pType, ptr pData, bool bMove, bool bClone)
{
	xvalue pVal;
	xrt_record_value* pRecord;
	size_t iAllocSize;

	if ( (pType == NULL) || (pType->Size == 0) ) {
		return NULL;
	}
	/* class 的 payload 只能通过显式复制操作创建副本，禁止退化为 memcpy。 */
	if ( pData != NULL && !bMove && pType->Kind == XRT_TYPE_KIND_CLASS &&
		 (pType->Ops == NULL || (bClone ? pType->Ops->clone == NULL : pType->Ops->copy == NULL)) ) {
		return NULL;
	}
	iAllocSize = sizeof(xrt_record_value) + pType->Size;
	pRecord = xrtMalloc(iAllocSize);
	if ( pRecord == NULL ) {
		return NULL;
	}
	pVal = __xvoAllocValue();
	if ( pVal == NULL ) {
		xrtFree(pRecord);
		return NULL;
	}
	pRecord->Type = pType;
	pRecord->Owner = pVal;
	pRecord->Flags = 0;
	pRecord->Size = (uint32)pType->Size;
	if ( pData ) {
		if ( bClone && pType->Ops && pType->Ops->clone ) {
			pType->Ops->clone(pRecord->Data, pData);
		} else if ( bMove && pType->Ops && pType->Ops->move ) {
			pType->Ops->move(pRecord->Data, pData);
		} else if ( bMove ) {
			memcpy(pRecord->Data, pData, pType->Size);
			memset(pData, 0, pType->Size);
		} else if ( pType->Ops && pType->Ops->copy ) {
			pType->Ops->copy(pRecord->Data, pData);
		} else {
			memcpy(pRecord->Data, pData, pType->Size);
		}
	} else {
		if ( pType->Ops && pType->Ops->init ) {
			pType->Ops->init(pRecord->Data);
		} else {
			memset(pRecord->Data, 0, pType->Size);
		}
	}

	xvoInitOwnedHeader_Inline(pVal, XVO_DT_CLASS, XRT_OBJMODE_LOCAL);
	pVal->Size = (uint32)pType->Size;
	pVal->vStruct = pRecord;
	pVal->vTypeDesc = pType;
	return pVal;
}


XXAPI xvalue xvoCreateRecord(const xrt_type_desc* pType, const ptr pData)
{
	return __xvoCreateRecord(pType, (ptr)pData, false, false);
}


// 转移 record/class 所有权，成功后源对象进入零值状态
XXAPI xvalue xvoCreateRecordMove(const xrt_type_desc* pType, ptr pData)
{
	return __xvoCreateRecord(pType, pData, true, false);
}


// 创建具有稳定身份的对象；普通赋值只共享返回的句柄，不复制 payload
XXAPI xvalue xvoCreateObject(const xrt_type_desc* pType)
{
	xvalue pVal;
	if ( (pType == NULL) || (pType->Kind != XRT_TYPE_KIND_CLASS) ) {
		return NULL;
	}
	pVal = __xvoCreateRecord(pType, NULL, false, false);
	if ( pVal != NULL ) {
		xvoSetShared_Inline(pVal);
	}
	return pVal;
}


// xvoCreateCustom 相关处理
XXAPI xvalue xvoCreateCustom(ptr pObj)
{
	xvalue pVal = __xvoAllocValue();
	if ( pVal ) {
		xvoInitOwnedHeader_Inline(pVal, XVO_DT_CUSTOM, XRT_OBJMODE_LOCAL);
		pVal->Size = 0;
		pVal->vCustom = pObj;
		pVal->vTypeDesc = NULL;
	}
	return pVal;
}


// 创建 handle/custom 动态值
XXAPI xvalue xvoCreateHandle(const xrt_type_desc* pType, ptr pHandle, uint32 iFlags)
{
	xvalue pVal;
	xrt_handle_value* pHandleValue;
	if ( pType == NULL ) {
		return NULL;
	}
	if ( (pHandle == NULL) && ((iFlags & XRT_HANDLE_FLAG_NULLABLE) == 0) ) {
		return NULL;
	}
	pHandleValue = xrtMalloc(sizeof(xrt_handle_value));
	if ( pHandleValue == NULL ) {
		return NULL;
	}
	pHandleValue->Type = pType;
	pHandleValue->Handle = pHandle;
	pHandleValue->Flags = iFlags;

	pVal = __xvoAllocValue();
	if ( pVal == NULL ) {
		xrtFree(pHandleValue);
		return NULL;
	}
	xvoInitOwnedHeader_Inline(pVal, XVO_DT_CUSTOM, XRT_OBJMODE_LOCAL);
	pVal->Size = sizeof(ptr);
	pVal->vCustom = pHandleValue;
	pVal->vTypeDesc = pType;
	return pVal;
}



// 读取值
XXAPI bool xvoGetBool(xvalue pVal)
{
	if ( (pVal == NULL) || (pVal->Type == XVO_DT_NULL) ) {
		return FALSE;
	} else if ( pVal->Type == XVO_DT_BOOL ) {
		return pVal->vBool;
	} else if ( pVal->Type == XVO_DT_INT ) {
		return pVal->vInt != 0;
	} else if ( pVal->Type == XVO_DT_FLOAT ) {
		return pVal->vFloat != 0.0;
	} else {
		return TRUE;
	}
}


// 获取整数
XXAPI int64 xvoGetInt(xvalue pVal)
{
	if ( pVal == NULL ) {
		return 0;
	} else if ( pVal->Type == XVO_DT_INT ) {
		return pVal->vInt;
	} else if ( pVal->Type == XVO_DT_FLOAT ) {
		return pVal->vFloat;
	} else if ( pVal->Type == XVO_DT_BOOL ) {
		return pVal->vBool ? 1 : 0;
	} else if ( pVal->Type == XVO_DT_TEXT ) {
		return xrtStrToI64(pVal->vText);
	} else {
		return 0;
	}
}


// 获取浮点数
XXAPI double xvoGetFloat(xvalue pVal)
{
	if ( pVal == NULL ) {
		return 0.0;
	} else if ( pVal->Type == XVO_DT_FLOAT ) {
		return pVal->vFloat;
	} else if ( pVal->Type == XVO_DT_INT ) {
		return pVal->vInt;
	} else if ( pVal->Type == XVO_DT_BOOL ) {
		return pVal->vBool ? 1.0 : 0.0;
	} else if ( pVal->Type == XVO_DT_TEXT ) {
		return xrtStrToNum(pVal->vText);
	} else {
		return 0.0;
	}
}


// 获取文本
XXAPI str xvoGetText(xvalue pVal)
{
	// 处理空值和 null 类型
	if ( (pVal == NULL) || (pVal->Type == XVO_DT_NULL) ) {
		return xCore.sNull;
	// 文本类型直接返回
	} else if ( pVal->Type == XVO_DT_TEXT ) {
		return pVal->vText;
	// 整数类型转换为字符串
	} else if ( pVal->Type == XVO_DT_INT ) {
		str sRet = xrtTempMemory(24);
		xrtI64ToStr(pVal->vInt, __xrt_str(sRet));
		return sRet;
	// 浮点数类型转换为字符串
	} else if ( pVal->Type == XVO_DT_FLOAT ) {
		str sRet = xrtTempMemory(32);
		xrtNumToStr(pVal->vFloat, __xrt_str(sRet));
		return sRet;
	// 布尔类型返回 true/false 字面量
	} else if ( pVal->Type == XVO_DT_BOOL ) {
		return (str)(pVal->vBool ? "true" : "false");
	// 时间类型格式化为日期时间字符串
	} else if ( pVal->Type == XVO_DT_TIME ) {
		str sRet = xrtTempMemory(32);
		int64 iYear;
		int iMonth, iDay, iHour, iMinute, iSecond;
		xrtDecodeSerial(pVal->vTime, &iYear, &iMonth, &iDay, &iHour, &iMinute, &iSecond, NULL, NULL);
		sprintf(__xrt_str(sRet), "%lld-%02d-%02d %02d:%02d:%02d", iYear, iMonth, iDay, iHour, iMinute, iSecond);
		return sRet;
	// 以下复合类型返回地址格式描述字符串
	} else if ( pVal->Type == XVO_DT_POINT ) {
		str sRet = xrtTempMemory(48);
		sprintf(__xrt_str(sRet), "[point:%p]", pVal->vPoint);
		return sRet;
	} else if ( pVal->Type == XVO_DT_FUNC ) {
		str sRet = xrtTempMemory(48);
		sprintf(__xrt_str(sRet), "[function:%p]", pVal->vFunc);
		return sRet;
	} else if ( pVal->Type == XVO_DT_ARRAY ) {
		str sRet = xrtTempMemory(48);
		sprintf(__xrt_str(sRet), "[array:%p]", pVal->vArray);
		return sRet;
	} else if ( pVal->Type == XVO_DT_LIST ) {
		str sRet = xrtTempMemory(48);
		sprintf(__xrt_str(sRet), "[list:%p]", pVal->vList);
		return sRet;
	} else if ( pVal->Type == XVO_DT_COLL ) {
		str sRet = xrtTempMemory(48);
		sprintf(__xrt_str(sRet), "[coll:%p]", pVal->vColl);
		return sRet;
	} else if ( pVal->Type == XVO_DT_TABLE ) {
		str sRet = xrtTempMemory(48);
		sprintf(__xrt_str(sRet), "[table:%p]", pVal->vTable);
		return sRet;
	} else if ( pVal->Type == XVO_DT_CLASS ) {
		str sRet = xrtTempMemory(48);
		sprintf(__xrt_str(sRet), "[class:%p]", pVal->vStruct);
		return sRet;
	} else if ( pVal->Type == XVO_DT_CUSTOM ) {
		str sRet = xrtTempMemory(48);
		sprintf(__xrt_str(sRet), "[custom:%p]", pVal->vCustom);
		return sRet;
	// 未知类型返回空字符串
	} else {
		return xCore.sNull;
	}
}


// 获取可释放的字符串副本；这是 xvalue 的统一 toString 入口
XXAPI str xvoToString(xvalue pVal, uint32* pSize)
{
	int iType;
	const xrt_type_desc* pType;
	str sText;

	if ( pSize != NULL ) {
		*pSize = 0;
	}
	if ( (pVal == NULL) || (pVal->Type == XVO_DT_NULL) ) {
		return xCore.sNull;
	}

	iType = pVal->Type;
	if ( iType == XVO_DT_BOOL ) {
		bool bVal = xvoGetBool(pVal);
		return xrtTypeToStringValue(xrtTypeBool(), &bVal, pSize);
	}
	if ( iType == XVO_DT_INT ) {
		int64 iVal = xvoGetInt(pVal);
		return xrtTypeToStringValue(xrtTypeInt(), &iVal, pSize);
	}
	if ( iType == XVO_DT_FLOAT ) {
		double fVal = xvoGetFloat(pVal);
		return xrtTypeToStringValue(xrtTypeFloat(), &fVal, pSize);
	}
	if ( iType == XVO_DT_TEXT ) {
		sText = xvoGetText(pVal);
		return xrtTypeToStringValue(xrtTypeString(), &sText, pSize);
	}
	if ( iType == XVO_DT_TIME ) {
		xtime tVal = xvoGetTime(pVal);
		return xrtTypeToStringValue(xrtTypeTime(), &tVal, pSize);
	}
	if ( iType == XVO_DT_POINT ) {
		ptr pPoint = xvoGetPoint(pVal);
		return xrtTypeToStringValue(xrtTypePoint(), &pPoint, pSize);
	}

	pType = xvoTypeDesc(pVal);
	if ( pType != NULL && pType->Ops != NULL && pType->Ops->to_string != NULL ) {
		if ( iType == XVO_DT_CLASS ) {
			return xrtTypeToStringValue(pType, xvoGetRecordData(pVal), pSize);
		}
		if ( iType == XVO_DT_CUSTOM ) {
			ptr pHandle = xvoGetHandleData(pVal);
			return xrtTypeToStringValue(pType, &pHandle, pSize);
		}
	}

	sText = xvoGetText(pVal);
	return xrtTypeToStringValue(xrtTypeString(), &sText, pSize);
}


// xvoGetTime 相关处理
XXAPI xtime xvoGetTime(xvalue pVal)
{
	if ( pVal == NULL ) {
		return 0;
	} else if ( pVal->Type == XVO_DT_TIME ) {
		return pVal->vTime;
	} else if ( pVal->Type == XVO_DT_TEXT ) {
		return xrtStrToTime(pVal->vText, pVal->Size);
	} else {
		return 0;
	}
}


// xvoGetPoint 相关处理
XXAPI ptr xvoGetPoint(xvalue pVal)
{
	if ( pVal == NULL ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_POINT ) {
		return pVal->vPoint;
	} else {
		return NULL;
	}
}


// xvoGetFunc 相关处理
XXAPI xfunction xvoGetFunc(xvalue pVal)
{
	if ( pVal == NULL ) {
		return NULL;
	} else if ( __xvoIsCallableValue(pVal) ) {
		return (xfunction)pVal->vCallable->NativeEntry;
	} else if ( pVal->Type == XVO_DT_FUNC ) {
		return pVal->vFunc;
	} else {
		return NULL;
	}
}


// 获取 callable 函数对象
XXAPI xrt_callable* xvoGetCallable(xvalue pVal)
{
	if ( __xvoIsCallableValue(pVal) ) {
		return pVal->vCallable;
	}
	return NULL;
}


// 获取数组
// 获取 callable 函数签名
XXAPI const xrt_func_sig* xvoCallableSig(xvalue pVal)
{
	return xrtCallableSig(xvoGetCallable(pVal));
}


XXAPI xparray xvoGetArray(xvalue pVal)
{
	if ( pVal == NULL ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_ARRAY ) {
		return pVal->vArray;
	} else {
		return NULL;
	}
}


// 获取列表
XXAPI xlist xvoGetList(xvalue pVal)
{
	if ( pVal == NULL ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_LIST ) {
		return pVal->vList;
	} else {
		return NULL;
	}
}


// xvoGetColl 相关处理
XXAPI xavltree xvoGetColl(xvalue pVal)
{
	if ( pVal == NULL ) {
		return NULL;
	} else if ( __xvoIsSetValue(pVal) ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_COLL ) {
		return pVal->vColl;
	} else {
		return NULL;
	}
}


XXAPI xavltree xvoGetMutableColl(xvalue pVal)
{
	return pVal != NULL && pVal->Type == XVO_DT_COLL && !__xvoIsSetValue(pVal) &&
		__xvoEnsureContainerUnique(pVal) ? pVal->vColl : NULL;
}


XXAPI xlist xvoGetMutableList(xvalue pVal)
{
	return pVal != NULL && pVal->Type == XVO_DT_LIST && __xvoEnsureContainerUnique(pVal) ? pVal->vList : NULL;
}


XXAPI xparray xvoGetMutableArray(xvalue pVal)
{
	return pVal != NULL && pVal->Type == XVO_DT_ARRAY && __xvoEnsureContainerUnique(pVal) ? pVal->vArray : NULL;
}


// 获取基于 XRT set 的集合对象
XXAPI xset xvoGetSet(xvalue pVal)
{
	if ( __xvoIsSetValue(pVal) ) {
		return pVal->vSet;
	}
	return NULL;
}


XXAPI xset xvoGetMutableSet(xvalue pVal)
{
	return __xvoIsSetValue(pVal) && __xvoEnsureContainerUnique(pVal) ? pVal->vSet : NULL;
}


// xvoGetTable 相关处理
XXAPI xdict xvoGetTable(xvalue pVal)
{
	if ( pVal == NULL ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_TABLE ) {
		return pVal->vTable;
	} else {
		return NULL;
	}
}


// 获取分类
XXAPI ptr xvoGetClass(xvalue pVal)
{
	if ( pVal == NULL ) {
		return NULL;
	} else if ( (pVal->Type == XVO_DT_CLASS) && (pVal->vTypeDesc != NULL) ) {
		return ((xrt_record_value*)pVal->vStruct)->Data;
	} else if ( pVal->Type == XVO_DT_CLASS ) {
		return pVal->vStruct;
	} else {
		return NULL;
	}
}


XXAPI xdict xvoGetMutableTable(xvalue pVal)
{
	return pVal != NULL && pVal->Type == XVO_DT_TABLE && __xvoEnsureContainerUnique(pVal) ? pVal->vTable : NULL;
}


// 获取 record/class 数据指针
XXAPI ptr xvoGetRecordData(xvalue pVal)
{
	return xvoGetClass(pVal);
}


// 根据 record/class 数据指针获取非持有的 xvalue 外壳
XXAPI xvalue xvoGetRecordOwner(const ptr pData)
{
	xrt_record_value* pRecord;
	if ( pData == NULL ) {
		return NULL;
	}
	pRecord = (xrt_record_value*)((uint8*)pData - offsetof(xrt_record_value, Data));
	return pRecord->Data == pData ? pRecord->Owner : NULL;
}


// xvoGetCustom 相关处理
XXAPI ptr xvoGetCustom(xvalue pVal)
{
	if ( pVal == NULL ) {
		return NULL;
	} else if ( (pVal->Type == XVO_DT_CUSTOM) && (pVal->vTypeDesc != NULL) ) {
		return ((xrt_handle_value*)pVal->vCustom)->Handle;
	} else if ( pVal->Type == XVO_DT_CUSTOM ) {
		return pVal->vCustom;
	} else {
		return NULL;
	}
}


// 获取 handle/custom 句柄
XXAPI ptr xvoGetHandleData(xvalue pVal)
{
	return xvoGetCustom(pVal);
}


// 获取 typed handle 内部的稳定句柄槽，供 C 层原位更新或清空句柄。
XXAPI ptr* xvoGetHandleSlot(xvalue pVal)
{
	xrt_handle_value* pHandle;

	if ( pVal == NULL || pVal->Type != XVO_DT_CUSTOM || pVal->vTypeDesc == NULL || pVal->vCustom == NULL ) {
		return NULL;
	}
	pHandle = (xrt_handle_value*)pVal->vCustom;
	return &pHandle->Handle;
}



// Array 读数据
XXAPI xvalue xvoArrayGetValue(xvalue pArr, uint32 index)
{
	if ( pArr == NULL ) {
		return &XVO_VALUE_NULL;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return &XVO_VALUE_NULL;
	}
	xvalue pVal = xrtPtrArrayGet(pArr->vArray, index + 1);
	if ( pVal ) {
		return pVal;
	} else {
		return &XVO_VALUE_NULL;
	}
}



// Array 追加数据
XXAPI bool xvoArrayAppendValue(xvalue pArr, xvalue pVal, bool bColloc)
{
	xvalue pStored;
	bool bCopied;
	if ( (pArr == NULL) || (pVal == NULL) ) {
		return FALSE;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	if ( !__xvoEnsureContainerUnique(pArr) ) {
		return FALSE;
	}
	if ( !__xvoPrepareStoredValue(&pArr->vArray->Owner, pVal, &pStored, &bCopied) ) {
		return FALSE;
	}
	uint32 index = xrtPtrArrayAppend(pArr->vArray, pStored);
	if ( index == 0 ) {
		__xvoDiscardPreparedValue(pStored, bCopied);
		return FALSE;
	}
	__xvoCommitStoredValue(pVal, pStored, bCopied, bColloc);
	return TRUE;
}



// Array 插入操作
XXAPI bool xvoArrayInsertValue(xvalue pArr, uint32 index, xvalue pVal, bool bColloc)
{
	xvalue pStored;
	bool bCopied;
	if ( (pArr == NULL) || (pVal == NULL) ) {
		return FALSE;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	if ( !__xvoEnsureContainerUnique(pArr) ) {
		return FALSE;
	}
	if ( !__xvoPrepareStoredValue(&pArr->vArray->Owner, pVal, &pStored, &bCopied) ) {
		return FALSE;
	}
	uint32 idx = xrtPtrArrayInsert(pArr->vArray, index, pStored);
	if ( idx == 0 ) {
		__xvoDiscardPreparedValue(pStored, bCopied);
		return FALSE;
	}
	__xvoCommitStoredValue(pVal, pStored, bCopied, bColloc);
	return TRUE;
}



// Array 修改操作
XXAPI bool xvoArraySetValue(xvalue pArr, uint32 index, xvalue pVal, bool bColloc)
{
	xvalue pStored;
	bool bCopied;
	if ( (pArr == NULL) || (pVal == NULL) ) {
		return FALSE;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	if ( !__xvoEnsureContainerUnique(pArr) ) {
		return FALSE;
	}
	if ( !__xvoPrepareStoredValue(&pArr->vArray->Owner, pVal, &pStored, &bCopied) ) {
		return FALSE;
	}
	xvalue pOldVal = xrtPtrArrayGet(pArr->vArray, index + 1);
	if ( pOldVal == NULL ) {
		__xvoDiscardPreparedValue(pStored, bCopied);
		return FALSE;
	}
	xvoUnref(pOldVal);
	xrtPtrArraySet_Inline(pArr->vArray, index + 1, pStored);
	__xvoCommitStoredValue(pVal, pStored, bCopied, bColloc);
	return TRUE;
}



// Array 合并
XXAPI bool xvoArrayMerge(xvalue pArr1, xvalue pArr2)
{
	xvalue pSource;
	bool bOwnedSource;
	bool bRet;
	uint32 i;

	if ( (pArr1 == NULL) || (pArr2 == NULL) ) {
		return FALSE;
	}
	if ( pArr1->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	if ( pArr2->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	pSource = pArr2;
	bOwnedSource = pArr1 == pArr2;
	if ( bOwnedSource ) {
		pSource = xvoCopy(pArr2);
		if ( pSource == NULL ) {
			return FALSE;
		}
	}
	bRet = TRUE;
	for ( i = 1; i <= pSource->vArray->Count; i++ ) {
		xvalue pVal = xrtPtrArrayGet_Inline(pSource->vArray, i);
		if ( !xvoArrayAppendValue(pArr1, pVal, FALSE) ) {
			bRet = FALSE;
			break;
		}
	}
	if ( bOwnedSource ) {
		xvoUnref(pSource);
	}
	return bRet;
}



// Array 操作
XXAPI bool xvoArraySwap(xvalue pArr, uint32 index1, uint32 index2)
{
	if ( pArr == NULL ) {
		return FALSE;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	if ( !__xvoEnsureContainerUnique(pArr) ) {
		return FALSE;
	}
	return xrtPtrArraySwap(pArr->vArray, index1 + 1, index2 + 1);
}


// 删除数组
XXAPI bool xvoArrayRemove(xvalue pArr, uint32 index, uint32 count)
{
	if ( pArr == NULL ) {
		return FALSE;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	if ( !__xvoEnsureContainerUnique(pArr) ) {
		return FALSE;
	}
	// 先释放被删除元素的引用
	for ( uint32 i = 0; i < count; i++ ) {
		xvalue pVal = xrtPtrArrayGet(pArr->vArray, index + 1 + i);
		if ( pVal ) {
			xvoUnref(pVal);
		}
	}
	return xrtPtrArrayRemove(pArr->vArray, index + 1, count);
}


// Take one array item without unref; ownership of the stored value reference is transferred to caller.
XXAPI xvalue xvoArrayTakeValue(xvalue pArr, uint32 index)
{
	xvalue pVal;
	if ( pArr == NULL ) {
		return &XVO_VALUE_NULL;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return &XVO_VALUE_NULL;
	}
	if ( !__xvoEnsureContainerUnique(pArr) ) {
		return &XVO_VALUE_NULL;
	}
	pVal = xrtPtrArrayGet(pArr->vArray, index + 1);
	if ( pVal == NULL ) {
		return &XVO_VALUE_NULL;
	}
	if ( !xrtPtrArrayRemove(pArr->vArray, index + 1, 1) ) {
		return &XVO_VALUE_NULL;
	}
	return pVal;
}


XXAPI xvalue xvoCreateFuncEx(xfunction pFunc, xvalue pEnv)
{
	xvalue pVal = xvoCreateFunc(pFunc);
	if ( pVal ) {
		pVal->vFuncEnv = pEnv;
		if ( (pEnv != NULL) && (pEnv->IsStatic == FALSE) ) {
			xvoAddRef_Inline(pEnv);
		}
	}
	return pVal;
}


// Pop the last array item without unref; ownership of the stored value reference is transferred to caller.
XXAPI xvalue xvoArrayPopValue(xvalue pArr)
{
	if ( (pArr == NULL) || (pArr->Type != XVO_DT_ARRAY) || (pArr->vArray == NULL) || (pArr->vArray->Count == 0) ) {
		return &XVO_VALUE_NULL;
	}
	return xvoArrayTakeValue(pArr, pArr->vArray->Count - 1);
}


// xvoArrayItemCount 相关处理
XXAPI uint32 xvoArrayItemCount(xvalue pArr)
{
	if ( pArr == NULL ) {
		return 0;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return 0;
	}
	return pArr->vArray->Count;
}


// 清除数组
XXAPI bool xvoArrayClear(xvalue pArr)
{
	if ( pArr == NULL ) {
		return FALSE;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	if ( !__xvoEnsureContainerUnique(pArr) ) {
		return FALSE;
	}
	for ( uint32 i = 1; i <= pArr->vArray->Count; i++ ) {
		xvalue pVal = xrtPtrArrayGet_Inline(pArr->vArray, i);
		xvoUnref(pVal);
	}
	xrtPtrArrayClear(pArr->vArray);
	return TRUE;
}


// 分配数组
XXAPI bool xvoArrayAlloc(xvalue pArr, uint32 count)
{
	if ( pArr == NULL ) {
		return FALSE;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	if ( !__xvoEnsureContainerUnique(pArr) ) {
		return FALSE;
	}
	return xrtPtrArrayMalloc(pArr->vArray, count);
}


// 内部函数：比较数组 sort 默认值
static int __xvoArraySortDefaultCompareValue(xvalue pLeft, xvalue pRight)
{
	uintptr_t iLeftAddr;
	uintptr_t iRightAddr;

	// 相同指针视为相等
	if ( pLeft == pRight ) {
		return 0;
	}

	// null 值排在最前面
	if ( pLeft == NULL || pLeft->Type == XVO_DT_NULL ) {
		return (pRight == NULL || pRight->Type == XVO_DT_NULL) ? 0 : -1;
	}
	if ( pRight == NULL || pRight->Type == XVO_DT_NULL ) {
		return 1;
	}

	// 不同类型按类型编号排序
	if ( pLeft->Type != pRight->Type ) {
		return (pLeft->Type < pRight->Type) ? -1 : 1;
	}

	// 相同类型按具体值比较
	switch ( pLeft->Type ) {
		case XVO_DT_BOOL:
			return (pLeft->vBool > pRight->vBool) - (pLeft->vBool < pRight->vBool);
		case XVO_DT_INT:
			return (pLeft->vInt > pRight->vInt) ? 1 : ((pLeft->vInt < pRight->vInt) ? -1 : 0);
		case XVO_DT_FLOAT:
			return (pLeft->vFloat > pRight->vFloat) ? 1 : ((pLeft->vFloat < pRight->vFloat) ? -1 : 0);
		case XVO_DT_TEXT:
		{
			// 文本类型先比较公共前缀，再比较长度
			uint32 iMinSize = (pLeft->Size < pRight->Size) ? pLeft->Size : pRight->Size;
			int iCmp = xrtStrComp(pLeft->vText, pRight->vText, iMinSize, FALSE);
			if ( iCmp != 0 ) {
				return (iCmp < 0) ? -1 : 1;
			}
			return (pLeft->Size > pRight->Size) ? 1 : ((pLeft->Size < pRight->Size) ? -1 : 0);
		}
		case XVO_DT_TIME:
			return (pLeft->vTime > pRight->vTime) ? 1 : ((pLeft->vTime < pRight->vTime) ? -1 : 0);
		case XVO_DT_POINT:
			// 指针类型按地址值比较
			iLeftAddr = (uintptr_t)pLeft->vPoint;
			iRightAddr = (uintptr_t)pRight->vPoint;
			return (iLeftAddr > iRightAddr) ? 1 : ((iLeftAddr < iRightAddr) ? -1 : 0);
		case XVO_DT_FUNC:
			// 函数类型按地址值比较
			iLeftAddr = (uintptr_t)pLeft->vFunc;
			iRightAddr = (uintptr_t)pRight->vFunc;
			return (iLeftAddr > iRightAddr) ? 1 : ((iLeftAddr < iRightAddr) ? -1 : 0);
		case XVO_DT_ARRAY:
			// 数组类型按元素数量比较
			return (pLeft->vArray->Count > pRight->vArray->Count) ? 1 : ((pLeft->vArray->Count < pRight->vArray->Count) ? -1 : 0);
		case XVO_DT_LIST:
			// 列表类型按元素数量比较
			return (pLeft->vList->AVLT.Count > pRight->vList->AVLT.Count) ? 1 : ((pLeft->vList->AVLT.Count < pRight->vList->AVLT.Count) ? -1 : 0);
		case XVO_DT_COLL:
			// 集合类型按元素数量比较
			if ( __xvoIsSetValue(pLeft) || __xvoIsSetValue(pRight) ) {
				uint32 iLeftCount = __xvoIsSetValue(pLeft) ? pLeft->vSet->Count : pLeft->vColl->Count;
				uint32 iRightCount = __xvoIsSetValue(pRight) ? pRight->vSet->Count : pRight->vColl->Count;
				return (iLeftCount > iRightCount) ? 1 : ((iLeftCount < iRightCount) ? -1 : 0);
			}
			return (pLeft->vColl->Count > pRight->vColl->Count) ? 1 : ((pLeft->vColl->Count < pRight->vColl->Count) ? -1 : 0);
		case XVO_DT_TABLE:
			// 字典类型按元素数量比较
			return (pLeft->vTable->AVLT.Count > pRight->vTable->AVLT.Count) ? 1 : ((pLeft->vTable->AVLT.Count < pRight->vTable->AVLT.Count) ? -1 : 0);
		case XVO_DT_CLASS:
		case XVO_DT_CUSTOM:
			// 类和自定义类型按数据大小比较
			if ( pLeft->Size != pRight->Size ) {
				return (pLeft->Size > pRight->Size) ? 1 : -1;
			}
			break;
		default:
			break;
	}

	// 无法通过值比较时，按对象地址排序
	iLeftAddr = (uintptr_t)pLeft;
	iRightAddr = (uintptr_t)pRight;
	return (iLeftAddr > iRightAddr) ? 1 : ((iLeftAddr < iRightAddr) ? -1 : 0);
}


// 内部函数：比较数组 sort 默认进程
static int __xvoArraySortDefaultCompareProc(const void* pLeft, const void* pRight)
{
	xvalue pLeftValue = pLeft ? *(const xvalue*)pLeft : NULL;
	xvalue pRightValue = pRight ? *(const xvalue*)pRight : NULL;
	return __xvoArraySortDefaultCompareValue(pLeftValue, pRightValue);
}


// xvoArraySort 相关处理
XXAPI bool xvoArraySort(xvalue pArr, ptr proc)
{
	if ( pArr == NULL ) {
		return FALSE;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	if ( !__xvoEnsureContainerUnique(pArr) ) {
		return FALSE;
	}
	if ( proc == NULL ) {
		proc = (ptr)__xvoArraySortDefaultCompareProc;
	}
	return xrtPtrArraySort(pArr->vArray, proc);
}



// List 读数据
XXAPI xvalue xvoListGetValue(xvalue pList, int64 index)
{
	if ( pList == NULL ) {
		return &XVO_VALUE_NULL;
	}
	if ( pList->Type != XVO_DT_LIST ) {
		return &XVO_VALUE_NULL;
	}
	xvalue pVal = xrtListGetPtr(pList->vList, index);
	if ( pVal ) {
		return pVal;
	} else {
		return &XVO_VALUE_NULL;
	}
}



// List 写数据
XXAPI bool xvoListSetValue(xvalue pList, int64 index, xvalue pVal, bool bColloc)
{
	xvalue pStored;
	bool bCopied;
	if ( (pList == NULL) || (pVal == NULL) ) {
		return FALSE;
	}
	if ( pList->Type != XVO_DT_LIST ) {
		return FALSE;
	}
	if ( !__xvoEnsureContainerUnique(pList) ) {
		return FALSE;
	}
	if ( !__xvoPrepareStoredValue(&pList->vList->Owner, pVal, &pStored, &bCopied) ) {
		return FALSE;
	}
	xvalue pOldVal = NULL;
	bool bRet = xrtListSetPtr(pList->vList, index, pStored, (ptr*)&pOldVal);
	if ( bRet == FALSE ) {
		__xvoDiscardPreparedValue(pStored, bCopied);
		return FALSE;
	}
	if ( pOldVal ) {
		xvoUnref(pOldVal);
	}
	__xvoCommitStoredValue(pVal, pStored, bCopied, bColloc);
	return TRUE;
}



// List 合并
typedef struct {
	xvalue pList;
	bool bFailed;
} __xvoListMergeCtx;


// xvoListMerge_RefProc 相关处理
bool xvoListMerge_RefProc(int64 iKey, xvalue* ppVal, __xvoListMergeCtx* pCtx)
{
	if ( !xrtListExists(pCtx->pList->vList, iKey) &&
		 !xvoListSetValue(pCtx->pList, iKey, *ppVal, FALSE) ) {
		pCtx->bFailed = TRUE;
	}
	return pCtx->bFailed;
}


// xvoListMerge_RefProc_ReWrite 相关处理
bool xvoListMerge_RefProc_ReWrite(int64 iKey, xvalue* ppVal, __xvoListMergeCtx* pCtx)
{
	if ( !xvoListSetValue(pCtx->pList, iKey, *ppVal, FALSE) ) {
		pCtx->bFailed = TRUE;
	}
	return pCtx->bFailed;
}


// xvoListMerge 相关处理
XXAPI bool xvoListMerge(xvalue pList1, xvalue pList2, bool bReWrite)
{
	__xvoListMergeCtx tCtx;
	xvalue pSource;
	bool bOwnedSource;
	if ( (pList1 == NULL) || (pList2 == NULL) ) {
		return FALSE;
	}
	if ( pList1->Type != XVO_DT_LIST ) {
		return FALSE;
	}
	if ( pList2->Type != XVO_DT_LIST ) {
		return FALSE;
	}
	pSource = pList2;
	bOwnedSource = pList1 == pList2;
	if ( bOwnedSource ) {
		pSource = xvoCopy(pList2);
		if ( pSource == NULL ) {
			return FALSE;
		}
	}
	memset(&tCtx, 0, sizeof(tCtx));
	tCtx.pList = pList1;
	if ( bReWrite ) {
		xrtListWalk(pSource->vList, (ptr)xvoListMerge_RefProc_ReWrite, &tCtx);
	} else {
		xrtListWalk(pSource->vList, (ptr)xvoListMerge_RefProc, &tCtx);
	}
	if ( bOwnedSource ) {
		xvoUnref(pSource);
	}
	return tCtx.bFailed == FALSE;
}



// List 操作
XXAPI bool xvoListExists(xvalue pList, int64 index)
{
	if ( pList == NULL ) {
		return FALSE;
	}
	if ( pList->Type != XVO_DT_LIST ) {
		return FALSE;
	}
	return xrtListExists(pList->vList, index);
}


// 删除列表
XXAPI bool xvoListRemove(xvalue pList, int64 index)
{
	if ( pList == NULL ) {
		return FALSE;
	}
	if ( pList->Type != XVO_DT_LIST ) {
		return FALSE;
	}
	if ( !__xvoEnsureContainerUnique(pList) ) {
		return FALSE;
	}
	xvalue pOldVal = xrtListRemovePtr(pList->vList, index);
	if ( pOldVal ) {
		xvoUnref(pOldVal);
		return TRUE;
	} else {
		return FALSE;
	}
}


// xvoListItemCount 相关处理
XXAPI uint32 xvoListItemCount(xvalue pList)
{
	if ( pList == NULL ) {
		return 0;
	}
	if ( pList->Type != XVO_DT_LIST ) {
		return 0;
	}
	return xrtListCount(pList->vList);
}


// 清除列表
XXAPI bool xvoListClear(xvalue pList)
{
	if ( pList == NULL ) {
		return FALSE;
	}
	if ( pList->Type != XVO_DT_LIST ) {
		return FALSE;
	}
	if ( !__xvoEnsureContainerUnique(pList) ) {
		return FALSE;
	}
	xrtListWalk(pList->vList, (ptr)xvoListClear_FreeProc, pList);
	xrtListClear(pList->vList);
	return TRUE;
}


// xvoListSetParent 相关处理
XXAPI bool xvoListSetParent(xvalue pList, xvalue pParentList)
{
	if ( (pList == NULL) || (pParentList == NULL) ) {
		return FALSE;
	}
	if ( pList->Type != XVO_DT_LIST ) {
		return FALSE;
	}
	if ( pParentList->Type != XVO_DT_LIST ) {
		return FALSE;
	}
	if ( !__xvoEnsureContainerUnique(pList) ) {
		return FALSE;
	}
	pList->vList->AVLT.Parent = &pParentList->vList->AVLT;
	return TRUE;
}



// 集合功能实现 - 值对比函数
int Coll_CompProc(Coll_Key* pNode, Coll_Key* pObjKey)
{
	if ( pNode->Hash == pObjKey->Hash ) {
		if ( pNode->Value->Type == XVO_DT_NULL ) {
			return 0;
		} else if ( pNode->Value->Type == XVO_DT_BOOL ) {
			return 0;
		} else if ( pNode->Value->Type == XVO_DT_TEXT ) {
			if ( pNode->Value->Size == pObjKey->Value->Size ) {
				return strcmp(__xrt_cstr(pNode->Value->vText), __xrt_cstr(pObjKey->Value->vText));
			} else {
				if ( pNode->Value->Size > pObjKey->Value->Size ) {
					return -1;
				} else {
					return 1;
				}
			}
		} else {
			// 其他类型比较 vInt
			if ( pNode->Value->vInt > pObjKey->Value->vInt ) {
				return -1;
			} else if ( pNode->Value->vInt < pObjKey->Value->vInt ) {
				return 1;
			} else {
				return 0;  // 相等时返回 0
			}
		}
	} else if ( pNode->Hash > pObjKey->Hash ) {
		return -1;
	} else {
		return 1;
	}
}



// Coll 写数据
XXAPI bool xvoCollSetValue(xvalue pColl, xvalue pVal, bool bColloc)
{
	xvalue pStored;
	bool bCopied;
	bool bRet;
	if ( (pColl == NULL) || (pVal == NULL) ) {
		return FALSE;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return FALSE;
	}
	if ( __xvoIsSetValue(pColl) ) {
		return xvoSetAddValue(pColl, pVal, bColloc);
	}
	if ( !__xvoEnsureContainerUnique(pColl) ) {
		return FALSE;
	}
	if ( !__xvoPrepareStoredValue(&pColl->vColl->Owner, pVal, &pStored, &bCopied) ) {
		return FALSE;
	}
	Coll_Key objKey;
	MAKE_COLL_KEY(objKey, pStored);
	bRet = xvoCollSetValueWithKey(pColl->vColl, &objKey, bCopied ? TRUE : bColloc);
	if ( bCopied ) {
		if ( bRet ) {
			if ( bColloc && !pVal->IsStatic ) {
				xvoUnref(pVal);
			}
		} else {
			xvoUnref(pStored);
		}
	}
	return bRet;
}


// 基于 XRT set 的集合写入
XXAPI bool xvoSetAddValue(xvalue pSet, xvalue pVal, bool bColloc)
{
	bool bRet;
	xvalue pStored;
	bool bCopied;
	if ( (pSet == NULL) || (pVal == NULL) || !__xvoIsSetValue(pSet) ) {
		return FALSE;
	}
	if ( !__xvoEnsureContainerUnique(pSet) ) {
		return FALSE;
	}
	if ( !__xvoPrepareStoredValue(&pSet->vSet->Owner, pVal, &pStored, &bCopied) ) {
		return FALSE;
	}
	bRet = xrtSetAdd(pSet->vSet, &pStored);
	if ( bCopied ) {
		xvoUnref(pStored);
	}
	if ( bRet && bColloc && !pVal->IsStatic ) {
		xvoUnref(pVal);
	}
	return bRet;
}


// 基于 XRT set 的集合存在判断
XXAPI bool xvoSetExistsValue(xvalue pSet, xvalue pVal)
{
	if ( (pSet == NULL) || (pVal == NULL) || !__xvoIsSetValue(pSet) ) {
		return FALSE;
	}
	return xrtSetExists(pSet->vSet, &pVal);
}


// 基于 XRT set 的集合删除
XXAPI bool xvoSetRemoveValue(xvalue pSet, xvalue pVal)
{
	if ( (pSet == NULL) || (pVal == NULL) || !__xvoIsSetValue(pSet) ) {
		return FALSE;
	}
	if ( !__xvoEnsureContainerUnique(pSet) ) {
		return FALSE;
	}
	return xrtSetRemove(pSet->vSet, &pVal);
}


// 基于 XRT set 按序号读取集合值
XXAPI xvalue xvoSetGetValueAt(xvalue pSet, uint32 iIndex)
{
	xvalue* pSlot;
	if ( (pSet == NULL) || !__xvoIsSetValue(pSet) ) {
		return NULL;
	}
	pSlot = (xvalue*)xrtSetItemAt(pSet->vSet, iIndex);
	return pSlot != NULL ? *pSlot : NULL;
}


// 基于 XRT set 的集合数量
XXAPI uint32 xvoSetItemCount(xvalue pSet)
{
	if ( (pSet == NULL) || !__xvoIsSetValue(pSet) ) {
		return 0;
	}
	return xrtSetCount(pSet->vSet);
}


// 基于 XRT set 的集合清空
XXAPI bool xvoSetClear(xvalue pSet)
{
	if ( (pSet == NULL) || !__xvoIsSetValue(pSet) ) {
		return FALSE;
	}
	if ( !__xvoEnsureContainerUnique(pSet) ) {
		return FALSE;
	}
	return xrtSetClear(pSet->vSet);
}


static xvalue __xvoCreateSetFromRaw(xset pRawSet)
{
	xvalue pRet;
	if ( pRawSet == NULL ) {
		return &XVO_VALUE_NULL;
	}
	pRet = xvoCreateSet();
	if ( pRet == NULL || pRet == &XVO_VALUE_NULL ) {
		xrtSetDestroy(pRawSet);
		return &XVO_VALUE_NULL;
	}
	if ( !xrtSetMerge(pRet->vSet, pRawSet) ) {
		xvoUnref(pRet);
		xrtSetDestroy(pRawSet);
		return &XVO_VALUE_NULL;
	}
	xrtSetDestroy(pRawSet);
	return pRet;
}


static bool __xvoBothAreSetValues(xvalue pLeft, xvalue pRight)
{
	return __xvoIsSetValue(pLeft) && __xvoIsSetValue(pRight);
}



// Coll 获取差集 [ pSelf 集合相对 pColl 集合没有的元素 ]
struct CollProcParam {
	xvalue pColl;
	xvalue pRetVal;
};


// xvoCollDifference_EachProc 相关处理
bool xvoCollDifference_EachProc(Coll_Key* pKey, struct CollProcParam* param)
{
	Coll_Key* pNode = xrtAVLTreeSearch(param->pColl->vColl, pKey);
	if ( pNode == NULL ) {
		xvoCollSetValueWithKey(param->pRetVal->vColl, pKey, FALSE);
	}
	return FALSE;
}


// xvoCollDifference 相关处理
XXAPI xvalue xvoCollDifference(xvalue pSelf, xvalue pColl)
{
	if ( (pSelf == NULL) || (pColl == NULL) ) {
		return &XVO_VALUE_NULL;
	}
	if ( pSelf->Type != XVO_DT_COLL ) {
		return &XVO_VALUE_NULL;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return &XVO_VALUE_NULL;
	}
	if ( __xvoIsSetValue(pSelf) || __xvoIsSetValue(pColl) ) {
		if ( !__xvoBothAreSetValues(pSelf, pColl) ) {
			return &XVO_VALUE_NULL;
		}
		return __xvoCreateSetFromRaw(xrtSetDifference(pSelf->vSet, pColl->vSet));
	}
	xvalue pRetVal = xvoCreateColl();
	struct CollProcParam param = { pColl, pRetVal };
	xrtAVLTreeWalk(pSelf->vColl, (ptr)xvoCollDifference_EachProc, &param);
	return pRetVal;
}



// Coll 获取对称差集 [ 两个集合中不重复的元素 ]
XXAPI xvalue xvoCollSymmetricDifference(xvalue pSelf, xvalue pColl)
{
	if ( (pSelf == NULL) || (pColl == NULL) ) {
		return &XVO_VALUE_NULL;
	}
	if ( pSelf->Type != XVO_DT_COLL ) {
		return &XVO_VALUE_NULL;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return &XVO_VALUE_NULL;
	}
	if ( __xvoIsSetValue(pSelf) || __xvoIsSetValue(pColl) ) {
		if ( !__xvoBothAreSetValues(pSelf, pColl) ) {
			return &XVO_VALUE_NULL;
		}
		return __xvoCreateSetFromRaw(xrtSetSymmetricDifference(pSelf->vSet, pColl->vSet));
	}
	xvalue pRetVal = xvoCreateColl();
	struct CollProcParam param = { pColl, pRetVal };
	xrtAVLTreeWalk(pSelf->vColl, (ptr)xvoCollDifference_EachProc, &param);
	param.pColl = pSelf;
	xrtAVLTreeWalk(pColl->vColl, (ptr)xvoCollDifference_EachProc, &param);
	return pRetVal;
}



// Coll 获取交集 [ pSelf 集合相对 pColl 集合存在的元素 ]
bool xvoCollIntersection_EachProc(Coll_Key* pKey, struct CollProcParam* param)
{
	Coll_Key* pNode = xrtAVLTreeSearch(param->pColl->vColl, pKey);
	if ( pNode ) {
		xvoCollSetValueWithKey(param->pRetVal->vColl, pKey, FALSE);
	}
	return FALSE;
}


// xvoCollIntersection 相关处理
XXAPI xvalue xvoCollIntersection(xvalue pSelf, xvalue pColl)
{
	if ( (pSelf == NULL) || (pColl == NULL) ) {
		return &XVO_VALUE_NULL;
	}
	if ( pSelf->Type != XVO_DT_COLL ) {
		return &XVO_VALUE_NULL;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return &XVO_VALUE_NULL;
	}
	if ( __xvoIsSetValue(pSelf) || __xvoIsSetValue(pColl) ) {
		if ( !__xvoBothAreSetValues(pSelf, pColl) ) {
			return &XVO_VALUE_NULL;
		}
		return __xvoCreateSetFromRaw(xrtSetIntersection(pSelf->vSet, pColl->vSet));
	}
	xvalue pRetVal = xvoCreateColl();
	struct CollProcParam param = { pColl, pRetVal };
	xrtAVLTreeWalk(pSelf->vColl, (ptr)xvoCollIntersection_EachProc, &param);
	return pRetVal;
}



// Coll 获取并集 [ 合并两个集合，返回和一个新的集合 ]
bool xvoCollUnion_EachProc(Coll_Key* pKey, xavltree pColl)
{
	xvoCollSetValueWithKey(pColl, pKey, FALSE);
	return FALSE;
}


// xvoCollUnion 相关处理
XXAPI xvalue xvoCollUnion(xvalue pSelf, xvalue pColl)
{
	if ( (pSelf == NULL) || (pColl == NULL) ) {
		return &XVO_VALUE_NULL;
	}
	if ( pSelf->Type != XVO_DT_COLL ) {
		return &XVO_VALUE_NULL;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return &XVO_VALUE_NULL;
	}
	if ( __xvoIsSetValue(pSelf) || __xvoIsSetValue(pColl) ) {
		if ( !__xvoBothAreSetValues(pSelf, pColl) ) {
			return &XVO_VALUE_NULL;
		}
		return __xvoCreateSetFromRaw(xrtSetUnion(pSelf->vSet, pColl->vSet));
	}
	xvalue pRetVal = xvoCreateColl();
	xrtAVLTreeWalk(pSelf->vColl, (ptr)xvoCollUnion_EachProc, pRetVal->vColl);
	xrtAVLTreeWalk(pColl->vColl, (ptr)xvoCollUnion_EachProc, pRetVal->vColl);
	return pRetVal;
}



// Coll 合并集合 [ 将 pColl 中的元素并入 pSelf ]
XXAPI bool xvoCollMerge(xvalue pSelf, xvalue pColl)
{
	xvalue pSource;
	bool bOwnedSource;
	bool bRet;

	if ( (pSelf == NULL) || (pColl == NULL) ) {
		return FALSE;
	}
	if ( pSelf->Type != XVO_DT_COLL ) {
		return FALSE;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return FALSE;
	}
	pSource = pColl;
	bOwnedSource = pSelf == pColl;
	if ( bOwnedSource ) {
		pSource = xvoCopy(pColl);
		if ( pSource == NULL ) {
			return FALSE;
		}
	}
	if ( !__xvoEnsureContainerUnique(pSelf) ) {
		if ( bOwnedSource ) {
			xvoUnref(pSource);
		}
		return FALSE;
	}
	if ( __xvoIsSetValue(pSelf) || __xvoIsSetValue(pSource) ) {
		if ( !__xvoBothAreSetValues(pSelf, pSource) ) {
			if ( bOwnedSource ) {
				xvoUnref(pSource);
			}
			return FALSE;
		}
		bRet = xrtSetMerge(pSelf->vSet, pSource->vSet);
	} else {
		xrtAVLTreeWalk(pSource->vColl, (ptr)xvoCollUnion_EachProc, pSelf->vColl);
		bRet = TRUE;
	}
	if ( bOwnedSource ) {
		xvoUnref(pSource);
	}
	return bRet;
}



// Coll 操作
XXAPI bool xvoCollExists(xvalue pColl, xvalue pVal)
{
	if ( (pColl == NULL) || (pVal == NULL) ) {
		return FALSE;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return FALSE;
	}
	if ( __xvoIsSetValue(pColl) ) {
		return xvoSetExistsValue(pColl, pVal);
	}
	Coll_Key objKey;
	MAKE_COLL_KEY(objKey, pVal);
	Coll_Key* pNode = xrtAVLTreeSearch(pColl->vColl, &objKey);
	if ( pNode ) {
		return TRUE;
	}
	return FALSE;
}


// xvoCollRemove 相关处理
XXAPI bool xvoCollRemove(xvalue pColl, xvalue pVal)
{
	if ( (pColl == NULL) || (pVal == NULL) ) {
		return FALSE;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return FALSE;
	}
	if ( __xvoIsSetValue(pColl) ) {
		return xvoSetRemoveValue(pColl, pVal);
	}
	if ( !__xvoEnsureContainerUnique(pColl) ) {
		return FALSE;
	}
	Coll_Key objKey;
	MAKE_COLL_KEY(objKey, pVal);
	return xrtAVLTreeRemove(pColl->vColl, &objKey);
}


// xvoCollItemCount 相关处理
XXAPI uint32 xvoCollItemCount(xvalue pColl)
{
	uint32 iCount = 0;
	if ( pColl == NULL ) {
		return 0;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return 0;
	}
	if ( __xvoIsSetValue(pColl) ) {
		return xvoSetItemCount(pColl);
	}
	if ( !xrtAVLTreeLock(pColl->vColl) ) {
		return 0;
	}
	iCount = pColl->vColl->Count;
	xrtAVLTreeUnlock(pColl->vColl);
	return iCount;
}


// xvoCollClear 相关处理
XXAPI bool xvoCollClear(xvalue pColl)
{
	if ( pColl == NULL ) {
		return FALSE;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return FALSE;
	}
	if ( __xvoIsSetValue(pColl) ) {
		return xvoSetClear(pColl);
	}
	if ( !__xvoEnsureContainerUnique(pColl) ) {
		return FALSE;
	}
	xrtAVLTreeClear(pColl->vColl);
	return TRUE;
}


// xvoCollSetParent 相关处理
XXAPI bool xvoCollSetParent(xvalue pColl, xvalue pParentColl)
{
	if ( (pColl == NULL) || (pParentColl == NULL) ) {
		return FALSE;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return FALSE;
	}
	if ( pParentColl->Type != XVO_DT_COLL ) {
		return FALSE;
	}
	if ( __xvoIsSetValue(pColl) || __xvoIsSetValue(pParentColl) ) {
		return FALSE;
	}
	if ( !__xvoEnsureContainerUnique(pColl) ) {
		return FALSE;
	}
	if ( !xrtOwnerBeginMutable(&pColl->vColl->Owner, "coll belongs to another thread.") ) {
		return FALSE;
	}
	pColl->vColl->Parent = pParentColl->vColl;
	xrtOwnerEndMutable(&pColl->vColl->Owner);
	return TRUE;
}



// Table 读数据
XXAPI xvalue xvoTableGetValue(xvalue pTbl, const void* key, uint32 kl)
{
	str sKey = (str)key;
	if ( pTbl == NULL ) {
		return &XVO_VALUE_NULL;
	}
	if ( pTbl->Type != XVO_DT_TABLE ) {
		return &XVO_VALUE_NULL;
	}
	if ( sKey == NULL ) {
		sKey = xCore.sNull;
		kl = 0;
	} else if ( kl == 0 ) {
		kl = strlen((const char*)sKey);
	}
	xvalue pVal = xrtDictGetPtr(pTbl->vTable, sKey, kl);
	if ( pVal ) {
		return pVal;
	} else {
		return &XVO_VALUE_NULL;
	}
}



// Table 写数据
XXAPI bool xvoTableSetValue(xvalue pTbl, const void* key, uint32 kl, xvalue pVal, bool bColloc)
{
	str sKey = (str)key;
	bool bNewKey;
	xvalue pStored;
	bool bCopied;
	// 参数有效性检查
	if ( (pTbl == NULL) || (pVal == NULL) ) {
		return FALSE;
	}
	if ( pTbl->Type != XVO_DT_TABLE ) {
		return FALSE;
	}
	if ( !__xvoEnsureContainerUnique(pTbl) ) {
		return FALSE;
	}
	// 处理键值参数
	if ( sKey == NULL ) {
		sKey = xCore.sNull;
		kl = 0;
	} else if ( kl == 0 ) {
		kl = strlen((const char*)sKey);
	}
	// 准备写入（线程安全检查）
	if ( !__xvoPrepareStoredValue(&pTbl->vTable->Owner, pVal, &pStored, &bCopied) ) {
		return FALSE;
	}
	bNewKey = !xrtDictExists(pTbl->vTable, sKey, kl);
	if ( bNewKey && !__xvoTableOrderAppend(pTbl, sKey, kl) ) {
		__xvoDiscardPreparedValue(pStored, bCopied);
		return FALSE;
	}
	// 写入字典并获取旧值
	xvalue pOldVal = NULL;
	int iRet = xrtDictSetPtr(pTbl->vTable, sKey, kl, pStored, (ptr*)&pOldVal);
	if ( iRet == FALSE ) {
		if ( bNewKey ) {
			__xvoTableOrderRemove(pTbl, sKey, kl);
		}
		__xvoDiscardPreparedValue(pStored, bCopied);
		return FALSE;
	}
	// 释放旧值的引用
	if ( pOldVal ) {
		xvoUnref(pOldVal);
	}
	// 增加新值的引用计数
	__xvoCommitStoredValue(pVal, pStored, bCopied, bColloc);
	return TRUE;
}



// Table 合并
typedef struct {
	xvalue pTbl;
	bool bFailed;
} __xvoTableMergeCtx;


// xvoTableMerge_RefProc 相关处理
bool xvoTableMerge_RefProc(Dict_Key* pKey, xvalue* ppVal, __xvoTableMergeCtx* pCtx)
{
	if ( pKey == NULL || ppVal == NULL || pCtx == NULL || pCtx->pTbl == NULL ) {
		if ( pCtx ) {
			pCtx->bFailed = TRUE;
		}
		return TRUE;
	}
	if ( xrtDictExists(pCtx->pTbl->vTable, pKey->Key, pKey->KeyLen) ) {
		return FALSE;
	}
	if ( !xvoTableSetValue(pCtx->pTbl, pKey->Key, pKey->KeyLen, *ppVal, FALSE) ) {
		pCtx->bFailed = TRUE;
		return TRUE;
	}
	return FALSE;
}


// xvoTableMerge_RefProc_ReWrite 相关处理
bool xvoTableMerge_RefProc_ReWrite(Dict_Key* pKey, xvalue* ppVal, __xvoTableMergeCtx* pCtx)
{
	if ( pKey == NULL || ppVal == NULL || pCtx == NULL || pCtx->pTbl == NULL ) {
		if ( pCtx ) {
			pCtx->bFailed = TRUE;
		}
		return TRUE;
	}
	if ( !xvoTableSetValue(pCtx->pTbl, pKey->Key, pKey->KeyLen, *ppVal, FALSE) ) {
		pCtx->bFailed = TRUE;
		return TRUE;
	}
	return FALSE;
}


// xvoTableMerge 相关处理
XXAPI bool xvoTableMerge(xvalue pTbl1, xvalue pTbl2, bool bReWrite)
{
	__xvoTableMergeCtx tCtx;
	xvalue pSource;
	bool bOwnedSource;
	if ( (pTbl1 == NULL) || (pTbl2 == NULL) ) {
		return FALSE;
	}
	if ( pTbl1->Type != XVO_DT_TABLE ) {
		return FALSE;
	}
	if ( pTbl2->Type != XVO_DT_TABLE ) {
		return FALSE;
	}
	pSource = pTbl2;
	bOwnedSource = pTbl1 == pTbl2;
	if ( bOwnedSource ) {
		pSource = xvoCopy(pTbl2);
		if ( pSource == NULL ) {
			return FALSE;
		}
	}
	memset(&tCtx, 0, sizeof(tCtx));
	tCtx.pTbl = pTbl1;
	if ( bReWrite ) {
		if ( !xvoTableWalkOrdered(pSource, (ptr)xvoTableMerge_RefProc_ReWrite, &tCtx) ) {
			xrtDictWalk(pSource->vTable, (ptr)xvoTableMerge_RefProc_ReWrite, &tCtx);
		}
	} else {
		if ( !xvoTableWalkOrdered(pSource, (ptr)xvoTableMerge_RefProc, &tCtx) ) {
			xrtDictWalk(pSource->vTable, (ptr)xvoTableMerge_RefProc, &tCtx);
		}
	}
	if ( bOwnedSource ) {
		xvoUnref(pSource);
	}
	return tCtx.bFailed == FALSE;
}



// Table 操作
XXAPI bool xvoTableExists(xvalue pTbl, str key, uint32 kl)
{
	if ( pTbl == NULL ) {
		return FALSE;
	}
	if ( pTbl->Type != XVO_DT_TABLE ) {
		return FALSE;
	}
	if ( !__xvoEnsureContainerUnique(pTbl) ) {
		return FALSE;
	}
	if ( key == NULL ) {
		key = xCore.sNull;
		kl = 0;
	} else if ( kl == 0 ) {
		kl = strlen(__xrt_cstr(key));
	}
	return xrtDictExists(pTbl->vTable, key, kl);
}


// xvoTableRemove 相关处理
XXAPI bool xvoTableRemove(xvalue pTbl, str key, uint32 kl)
{
	if ( pTbl == NULL ) {
		return FALSE;
	}
	if ( pTbl->Type != XVO_DT_TABLE ) {
		return FALSE;
	}
	if ( key == NULL ) {
		key = xCore.sNull;
		kl = 0;
	} else if ( kl == 0 ) {
		kl = strlen(__xrt_cstr(key));
	}
	xvalue pOldVal = xrtDictRemovePtr(pTbl->vTable, key, kl);
	if ( pOldVal ) {
		__xvoTableOrderRemove(pTbl, key, kl);
		xvoUnref(pOldVal);
		return TRUE;
	} else {
		return FALSE;
	}
}


// xvoTableItemCount 相关处理
XXAPI uint32 xvoTableItemCount(xvalue pTbl)
{
	if ( pTbl == NULL ) {
		return 0;
	}
	if ( pTbl->Type != XVO_DT_TABLE ) {
		return 0;
	}
	return xrtDictCount(pTbl->vTable);
}


// xvoTableClear 相关处理
XXAPI bool xvoTableClear(xvalue pTbl)
{
	if ( pTbl == NULL ) {
		return FALSE;
	}
	if ( pTbl->Type != XVO_DT_TABLE ) {
		return FALSE;
	}
	if ( !__xvoEnsureContainerUnique(pTbl) ) {
		return FALSE;
	}
	xrtDictWalk(pTbl->vTable, (ptr)xvoTableClear_FreeProc, pTbl);
	xrtDictClear(pTbl->vTable);
	__xvoTableOrderClear(pTbl);
	return TRUE;
}


// 按插入顺序遍历表；若表没有顺序信息，则退回底层字典遍历
XXAPI bool xvoTableWalk(xvalue pTbl, Dict_EachProc procEach, ptr pArg)
{
	if ( pTbl == NULL || pTbl->Type != XVO_DT_TABLE || procEach == NULL ) {
		return FALSE;
	}
	if ( xvoTableWalkOrdered(pTbl, procEach, pArg) ) {
		return TRUE;
	}
	xrtDictWalk(pTbl->vTable, procEach, pArg);
	return TRUE;
}


// xvoTableSetParent 相关处理
// ------------------------------------ dynamic -> typed container ------------------------------------
//
// 源 xvalue 只借用，不转移所有权；返回的新容器由调用者释放。
// 任一元素无法按目标类型安全解箱时，转换整体失败，不返回半成品。

static bool __xrtTypedValueCanImport(xvalue pValue, const xrt_type_desc* pItemType)
{
	const xrt_type_desc* pValueType;

	if ( pValue == NULL || pItemType == NULL ) {
		return FALSE;
	}
	if ( pItemType == xrtTypeValue() ) {
		return TRUE;
	}
	pValueType = xvoTypeDesc(pValue);
	return xrtTypeCanConvert(
		pValueType,
		pItemType,
		XRT_TYPE_CONVERT_EXACT | XRT_TYPE_CONVERT_SAFE_WIDEN);
}

XXAPI xtarray xrtTypedArrayFromValue(xvalue pValue, const xrt_type_desc* pItemType, uint32 iMode)
{
	xtarray pResult;
	uint32 i;
	uint32 iCount;

	if ( pValue == NULL || pValue->Type != XVO_DT_ARRAY || pItemType == NULL ) {
		return NULL;
	}
	pResult = xrtTypedArrayCreate(pItemType, iMode);
	if ( pResult == NULL ) {
		return NULL;
	}
	iCount = xvoArrayItemCount(pValue);
	for ( i = 0; i < iCount; ++i ) {
		xvalue pItem = xvoArrayGetValue(pValue, i);
		if ( !__xrtTypedValueCanImport(pItem, pItemType) ||
			 !xrtTypedArrayAppendValue(pResult, pItem) ) {
			xrtTypedArrayDestroy(pResult);
			return NULL;
		}
	}
	return pResult;
}


XXAPI xvalue xrtTypedArrayToValue(xtarray pArray)
{
	xvalue pResult;
	uint32 i;
	uint32 iCount;

	if ( pArray == NULL || pArray->ItemType == NULL ) {
		return NULL;
	}
	pResult = xvoCreateArray();
	if ( pResult == NULL ) {
		return NULL;
	}
	iCount = xrtTypedArrayCount(pArray);
	for ( i = 0; i < iCount; ++i ) {
		xvalue pItem = xrtTypeBoxValue(pArray->ItemType, xrtTypedArrayGet(pArray, i + 1));
		if ( pItem == NULL || !xvoArrayAppendValue(pResult, pItem, TRUE) ) {
			if ( pItem != NULL ) {
				xvoUnref(pItem);
			}
			xvoUnref(pResult);
			return NULL;
		}
	}
	return pResult;
}


typedef struct __xrt_typed_list_from_value_ctx {
	xtlist Result;
	const xrt_type_desc* ItemType;
	bool Failed;
} __xrt_typed_list_from_value_ctx;


static bool __xrtTypedListFromValueEach(int64 iKey, xvalue* ppValue, __xrt_typed_list_from_value_ctx* pCtx)
{
	if ( pCtx == NULL || pCtx->Result == NULL || ppValue == NULL ||
		 !__xrtTypedValueCanImport(*ppValue, pCtx->ItemType) ||
		 !xrtTypedListSetValue(pCtx->Result, iKey, *ppValue) ) {
		if ( pCtx != NULL ) {
			pCtx->Failed = TRUE;
		}
		return TRUE;
	}
	return FALSE;
}


XXAPI xtlist xrtTypedListFromValue(xvalue pValue, const xrt_type_desc* pItemType, uint32 iMode)
{
	__xrt_typed_list_from_value_ctx tCtx;

	if ( pValue == NULL || pValue->Type != XVO_DT_LIST || pItemType == NULL ) {
		return NULL;
	}
	tCtx.Result = xrtTypedListCreate(pItemType, iMode);
	tCtx.ItemType = pItemType;
	tCtx.Failed = tCtx.Result == NULL;
	if ( tCtx.Failed ) {
		return NULL;
	}
	xrtListWalk(pValue->vList, (ptr)__xrtTypedListFromValueEach, &tCtx);
	if ( tCtx.Failed ) {
		xrtTypedListDestroy(tCtx.Result);
		return NULL;
	}
	return tCtx.Result;
}


XXAPI xvalue xrtTypedListToValue(xtlist pList)
{
	xvalue pResult;
	uint32 i;
	uint32 iCount;

	if ( pList == NULL || pList->ItemType == NULL ) {
		return NULL;
	}
	pResult = xvoCreateList();
	if ( pResult == NULL ) {
		return NULL;
	}
	iCount = xrtTypedListCount(pList);
	for ( i = 0; i < iCount; ++i ) {
		int64 iKey = 0;
		xvalue pItem = xrtTypeBoxValue(pList->ItemType, xrtTypedListItemAt(pList, i, &iKey));
		if ( pItem == NULL || !xvoListSetValue(pResult, iKey, pItem, TRUE) ) {
			if ( pItem != NULL ) {
				xvoUnref(pItem);
			}
			xvoUnref(pResult);
			return NULL;
		}
	}
	return pResult;
}


typedef struct __xrt_typed_set_from_value_ctx {
	xtset Result;
	const xrt_type_desc* ItemType;
	bool Failed;
} __xrt_typed_set_from_value_ctx;


static bool __xrtTypedSetFromLegacyValueEach(Coll_Key* pKey, __xrt_typed_set_from_value_ctx* pCtx)
{
	if ( pCtx == NULL || pCtx->Result == NULL || pKey == NULL || pKey->Value == NULL ||
		 !__xrtTypedValueCanImport(pKey->Value, pCtx->ItemType) ||
		 !xrtTypedSetAddValue(pCtx->Result, pKey->Value) ) {
		if ( pCtx != NULL ) {
			pCtx->Failed = TRUE;
		}
		return TRUE;
	}
	return FALSE;
}


XXAPI xtset xrtTypedSetFromValue(xvalue pValue, const xrt_type_desc* pItemType, uint32 iMode)
{
	__xrt_typed_set_from_value_ctx tCtx;
	uint32 i;
	uint32 iCount;

	if ( pValue == NULL || pValue->Type != XVO_DT_COLL || pItemType == NULL ) {
		return NULL;
	}
	tCtx.Result = xrtTypedSetCreate(pItemType, iMode);
	tCtx.ItemType = pItemType;
	tCtx.Failed = tCtx.Result == NULL;
	if ( tCtx.Failed ) {
		return NULL;
	}
	if ( __xvoIsSetValue(pValue) ) {
		iCount = xvoSetItemCount(pValue);
		for ( i = 0; i < iCount; ++i ) {
			xvalue pItem = xvoSetGetValueAt(pValue, i);
			if ( !__xrtTypedValueCanImport(pItem, pItemType) ||
				 !xrtTypedSetAddValue(tCtx.Result, pItem) ) {
				tCtx.Failed = TRUE;
				break;
			}
		}
	} else {
		xrtAVLTreeWalk(pValue->vColl, (ptr)__xrtTypedSetFromLegacyValueEach, &tCtx);
	}
	if ( tCtx.Failed ) {
		xrtTypedSetDestroy(tCtx.Result);
		return NULL;
	}
	return tCtx.Result;
}


XXAPI xvalue xrtTypedSetToValue(xtset pSet)
{
	xvalue pResult;
	uint32 i;
	uint32 iCount;

	if ( pSet == NULL || pSet->ItemType == NULL ) {
		return NULL;
	}
	pResult = xvoCreateSet();
	if ( pResult == NULL ) {
		return NULL;
	}
	iCount = xrtTypedSetCount(pSet);
	for ( i = 0; i < iCount; ++i ) {
		xvalue pItem = xrtTypeBoxValue(pSet->ItemType, xrtTypedSetItemAt(pSet, i));
		if ( pItem == NULL || !xvoCollSetValue(pResult, pItem, TRUE) ) {
			if ( pItem != NULL ) {
				xvoUnref(pItem);
			}
			xvoUnref(pResult);
			return NULL;
		}
	}
	return pResult;
}


typedef struct __xrt_typed_dict_from_value_ctx {
	xtdict Result;
	const xrt_type_desc* ItemType;
	bool Failed;
} __xrt_typed_dict_from_value_ctx;


static bool __xrtTypedDictFromValueEach(Dict_Key* pKey, xvalue* ppValue, __xrt_typed_dict_from_value_ctx* pCtx)
{
	if ( pCtx == NULL || pCtx->Result == NULL || pKey == NULL || ppValue == NULL ||
		 !__xrtTypedValueCanImport(*ppValue, pCtx->ItemType) ||
		 !xrtTypedDictSetValue(pCtx->Result, pKey->Key, pKey->KeyLen, *ppValue) ) {
		if ( pCtx != NULL ) {
			pCtx->Failed = TRUE;
		}
		return TRUE;
	}
	return FALSE;
}


XXAPI xtdict xrtTypedDictFromValue(xvalue pValue, const xrt_type_desc* pItemType, uint32 iMode)
{
	__xrt_typed_dict_from_value_ctx tCtx;

	if ( pValue == NULL || pValue->Type != XVO_DT_TABLE || pItemType == NULL ) {
		return NULL;
	}
	tCtx.Result = xrtTypedDictCreate(pItemType, iMode);
	tCtx.ItemType = pItemType;
	tCtx.Failed = tCtx.Result == NULL;
	if ( tCtx.Failed ) {
		return NULL;
	}
	xvoTableWalk(pValue, (ptr)__xrtTypedDictFromValueEach, &tCtx);
	if ( tCtx.Failed ) {
		xrtTypedDictDestroy(tCtx.Result);
		return NULL;
	}
	return tCtx.Result;
}


XXAPI xvalue xrtTypedDictToValue(xtdict pDict)
{
	xvalue pResult;
	uint32 i;
	uint32 iCount;

	if ( pDict == NULL || pDict->ItemType == NULL ) {
		return NULL;
	}
	pResult = xvoCreateTable();
	if ( pResult == NULL ) {
		return NULL;
	}
	iCount = xrtTypedDictCount(pDict);
	for ( i = 0; i < iCount; ++i ) {
		const char* sKey = NULL;
		uint32 iKeyLen = 0;
		xvalue pItem = xrtTypeBoxValue(
			pDict->ItemType,
			xrtTypedDictItemAt(pDict, i, &sKey, &iKeyLen));
		if ( pItem == NULL || !xvoTableSetValue(pResult, sKey, iKeyLen, pItem, TRUE) ) {
			if ( pItem != NULL ) {
				xvoUnref(pItem);
			}
			xvoUnref(pResult);
			return NULL;
		}
	}
	return pResult;
}


XXAPI bool xvoTableSetParent(xvalue pTbl, xvalue pParentTable)
{
	if ( (pTbl == NULL) || (pParentTable == NULL) ) {
		return FALSE;
	}
	if ( pTbl->Type != XVO_DT_TABLE ) {
		return FALSE;
	}
	if ( pParentTable->Type != XVO_DT_TABLE ) {
		return FALSE;
	}
	if ( !__xvoEnsureContainerUnique(pTbl) ) {
		return FALSE;
	}
	pTbl->vTable->AVLT.Parent = &pParentTable->vTable->AVLT;
	return TRUE;
}



// 类型操作
XXAPI bool xvoIsNull(xvalue pVal)
{
	if ( pVal == NULL ) {
		return TRUE;
	} else if ( pVal->Type == XVO_DT_NULL ) {
		return TRUE;
	} else {
		return FALSE;
	}
}


// xvoType 相关处理
XXAPI int xvoType(xvalue pVal)
{
	if ( pVal == NULL ) {
		return XVO_DT_NULL;
	} else {
		return pVal->Type;
	}
}



// 获取数据长度
static bool __xvoTypeIsNumeric(int iType)
{
	return (iType == XVO_DT_BOOL) || (iType == XVO_DT_INT) || (iType == XVO_DT_FLOAT);
}
static int __xvoCompareInt64(int64 iLeft, int64 iRight)
{
	if ( iLeft < iRight ) {
		return -1;
	} else if ( iLeft > iRight ) {
		return 1;
	} else {
		return 0;
	}
}
static int __xvoCompareDouble(double fLeft, double fRight)
{
	if ( fLeft < fRight ) {
		return -1;
	} else if ( fLeft > fRight ) {
		return 1;
	} else {
		return 0;
	}
}
XXAPI str xvoTypeName(int iType)
{
	switch ( iType ) {
		case XVO_DT_NULL: return (str)"null";
		case XVO_DT_BOOL: return (str)"bool";
		case XVO_DT_INT: return (str)"int";
		case XVO_DT_FLOAT: return (str)"float";
		case XVO_DT_TEXT: return (str)"text";
		case XVO_DT_TIME: return (str)"time";
		case XVO_DT_POINT: return (str)"point";
		case XVO_DT_FUNC: return (str)"func";
		case XVO_DT_ARRAY: return (str)"array";
		case XVO_DT_LIST: return (str)"list";
		case XVO_DT_COLL: return (str)"coll";
		case XVO_DT_TABLE: return (str)"table";
		case XVO_DT_CLASS: return (str)"class";
		case XVO_DT_CUSTOM: return (str)"custom";
		default: return (str)"unknown";
	}
}


XXAPI xvalue xvoGetFuncEnv(xvalue pVal)
{
	if ( pVal == NULL ) {
		return NULL;
	} else if ( __xvoIsCallableValue(pVal) ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_FUNC ) {
		return pVal->vFuncEnv;
	} else {
		return NULL;
	}
}


// 获取动态值的精确运行时类型
static bool __xvoCanBindTypeDesc(xvalue pVal, const xrt_type_desc* pType)
{
	if ( (pVal == NULL) || (pType == NULL) ) {
		return FALSE;
	}
	switch ( pVal->Type ) {
		case XVO_DT_ARRAY:
			return pType->Kind == XRT_TYPE_KIND_ARRAY;
		case XVO_DT_LIST:
			return pType->Kind == XRT_TYPE_KIND_LIST;
		case XVO_DT_COLL:
			return pType->Kind == XRT_TYPE_KIND_SET;
		case XVO_DT_TABLE:
			return pType->Kind == XRT_TYPE_KIND_DICT;
		case XVO_DT_CLASS:
			return pType->Kind == XRT_TYPE_KIND_RECORD
				|| pType->Kind == XRT_TYPE_KIND_CLASS;
		case XVO_DT_CUSTOM:
			return pType->Kind == XRT_TYPE_KIND_HANDLE
				|| pType->Kind == XRT_TYPE_KIND_FUTURE;
		default:
			return FALSE;
	}
}


// 设置动态值的精确运行时类型
XXAPI bool xvoSetTypeDesc(xvalue pVal, const xrt_type_desc* pType)
{
	if ( !__xvoCanBindTypeDesc(pVal, pType) ) {
		return FALSE;
	}
	if ( __xvoIsContainerValue(pVal) && !__xvoEnsureContainerUnique(pVal) ) {
		return FALSE;
	}
	return __xvoSetValueTypeDesc(pVal, pType);
}


// 获取动态值的精确运行时类型
XXAPI const xrt_type_desc* xvoTypeDesc(xvalue pVal)
{
	if ( pVal == NULL ) {
		return xrtTypeNull();
	}
	if ( pVal->Type == XVO_DT_ARRAY || pVal->Type == XVO_DT_LIST || pVal->Type == XVO_DT_COLL || pVal->Type == XVO_DT_TABLE || pVal->Type == XVO_DT_CLASS || pVal->Type == XVO_DT_CUSTOM ) {
		const xrt_type_desc* pType = __xvoValueTypeDesc(pVal);
		if ( (pType != NULL) && __xvoCanBindTypeDesc(pVal, pType) ) {
			return pType;
		}
	}
	switch ( pVal->Type ) {
		case XVO_DT_NULL: return xrtTypeNull();
		case XVO_DT_BOOL: return xrtTypeBool();
		case XVO_DT_INT: return xrtTypeInt();
		case XVO_DT_FLOAT: return xrtTypeFloat();
		case XVO_DT_TEXT: return xrtTypeString();
		case XVO_DT_TIME: return xrtTypeTime();
		case XVO_DT_POINT: return xrtTypePoint();
		case XVO_DT_FUNC: return xrtTypeFunction();
		case XVO_DT_ARRAY: return xrtTypeArray();
		case XVO_DT_LIST: return xrtTypeList();
		case XVO_DT_COLL: return xrtTypeSet();
		case XVO_DT_TABLE: return xrtTypeDict();
		case XVO_DT_CLASS: return xrtTypeRecord();
		case XVO_DT_CUSTOM: return xrtTypeHandle();
		default: return NULL;
	}
}


// 调用 function/callable 动态值
XXAPI bool xvoInvoke(xvalue pFunc, xrt_call_frame* pFrame, xrt_call_result* pResult)
{
	xfunction pLegacyFunc;
	xvalue pParam;
	xvalue pRet;
	uint32 i;

	if ( (pFunc == NULL) || (pFunc->Type != XVO_DT_FUNC) || (pResult == NULL) ) {
		return FALSE;
	}
	if ( __xvoIsCallableValue(pFunc) ) {
		return xrtCallableInvoke(pFunc->vCallable, pFrame, pResult);
	}

	pLegacyFunc = pFunc->vFunc;
	if ( pLegacyFunc == NULL ) {
		return FALSE;
	}

	// 旧函数约定只接收一个 array 参数；必要时由 call frame 收集生成。
	pParam = NULL;
	if ( pFrame ) {
		if ( pFrame->Varargs && pFrame->Varargs->Type == XVO_DT_ARRAY ) {
			pParam = pFrame->Varargs;
			xvoAddRef(pParam);
		} else if ( pFrame->Argc > 0 && pFrame->Argv != NULL ) {
			pParam = xvoCreateArray();
			if ( pParam == NULL ) {
				return FALSE;
			}
			for ( i = 0; i < pFrame->Argc; i++ ) {
				xvoArrayAppendValue(pParam, pFrame->Argv[i], FALSE);
			}
		}
	}
	if ( pParam == NULL ) {
		pParam = xvoCreateArray();
		if ( pParam == NULL ) {
			return FALSE;
		}
	}

	pRet = pLegacyFunc(pFunc->vFuncEnv, pParam);
	xvoUnref(pParam);
	if ( pRet == NULL ) {
		return TRUE;
	}
	return xrtCallResultSetValue(pResult, 0, pRet, TRUE);
}


// Take one table value without unref; ownership of the stored value reference is transferred to caller.
XXAPI xvalue xvoTableTakeValue(xvalue pTbl, str key, uint32 kl)
{
	xvalue pOldVal;
	if ( pTbl == NULL ) {
		return &XVO_VALUE_NULL;
	}
	if ( pTbl->Type != XVO_DT_TABLE ) {
		return &XVO_VALUE_NULL;
	}
	if ( !__xvoEnsureContainerUnique(pTbl) ) {
		return &XVO_VALUE_NULL;
	}
	if ( key == NULL ) {
		key = xCore.sNull;
		kl = 0;
	} else if ( kl == 0 ) {
		kl = strlen(__xrt_cstr(key));
	}
	pOldVal = xrtDictRemovePtr(pTbl->vTable, key, kl);
	if ( pOldVal ) {
		__xvoTableOrderRemove(pTbl, key, kl);
		return pOldVal;
	} else {
		return &XVO_VALUE_NULL;
	}
}


// Take one list item without unref; ownership of the stored value reference is transferred to caller.
XXAPI xvalue xvoListTakeValue(xvalue pList, int64 index)
{
	xvalue pOldVal;
	if ( pList == NULL ) {
		return &XVO_VALUE_NULL;
	}
	if ( pList->Type != XVO_DT_LIST ) {
		return &XVO_VALUE_NULL;
	}
	if ( !__xvoEnsureContainerUnique(pList) ) {
		return &XVO_VALUE_NULL;
	}
	pOldVal = xrtListRemovePtr(pList->vList, index);
	if ( pOldVal ) {
		return pOldVal;
	} else {
		return &XVO_VALUE_NULL;
	}
}
XXAPI bool xvoIsBool(xvalue pVal)
{
	return xvoType(pVal) == XVO_DT_BOOL;
}
XXAPI bool xvoIsInt(xvalue pVal)
{
	return xvoType(pVal) == XVO_DT_INT;
}
XXAPI bool xvoIsFloat(xvalue pVal)
{
	return xvoType(pVal) == XVO_DT_FLOAT;
}
XXAPI bool xvoIsText(xvalue pVal)
{
	return xvoType(pVal) == XVO_DT_TEXT;
}
XXAPI bool xvoIsTime(xvalue pVal)
{
	return xvoType(pVal) == XVO_DT_TIME;
}
XXAPI bool xvoIsPoint(xvalue pVal)
{
	return xvoType(pVal) == XVO_DT_POINT;
}
XXAPI bool xvoIsFunc(xvalue pVal)
{
	return xvoType(pVal) == XVO_DT_FUNC;
}
XXAPI bool xvoIsArray(xvalue pVal)
{
	return xvoType(pVal) == XVO_DT_ARRAY;
}
XXAPI bool xvoIsList(xvalue pVal)
{
	return xvoType(pVal) == XVO_DT_LIST;
}
XXAPI bool xvoIsColl(xvalue pVal)
{
	return xvoType(pVal) == XVO_DT_COLL;
}
XXAPI bool xvoIsTable(xvalue pVal)
{
	return xvoType(pVal) == XVO_DT_TABLE;
}
XXAPI bool xvoIsClass(xvalue pVal)
{
	return xvoType(pVal) == XVO_DT_CLASS;
}
XXAPI bool xvoIsCustom(xvalue pVal)
{
	return xvoType(pVal) == XVO_DT_CUSTOM;
}
XXAPI bool xvoIsNumber(xvalue pVal)
{
	return __xvoTypeIsNumeric(xvoType(pVal));
}
XXAPI bool xvoIsBasic(xvalue pVal)
{
	int iType = xvoType(pVal);
	return (iType == XVO_DT_NULL)
		|| (iType == XVO_DT_BOOL)
		|| (iType == XVO_DT_INT)
		|| (iType == XVO_DT_FLOAT)
		|| (iType == XVO_DT_TEXT)
		|| (iType == XVO_DT_TIME);
}
XXAPI bool xvoIsContainer(xvalue pVal)
{
	int iType = xvoType(pVal);
	return (iType == XVO_DT_ARRAY)
		|| (iType == XVO_DT_LIST)
		|| (iType == XVO_DT_COLL)
		|| (iType == XVO_DT_TABLE);
}
XXAPI bool xvoCanCompareBasic(xvalue pLeft, xvalue pRight)
{
	int iLeftType = xvoType(pLeft);
	int iRightType = xvoType(pRight);
	if ( iLeftType == XVO_DT_NULL || iRightType == XVO_DT_NULL ) {
		return iLeftType == iRightType;
	}
	if ( __xvoTypeIsNumeric(iLeftType) && __xvoTypeIsNumeric(iRightType) ) {
		return TRUE;
	}
	if ( iLeftType == XVO_DT_TEXT && iRightType == XVO_DT_TEXT ) {
		return TRUE;
	}
	if ( iLeftType == XVO_DT_TIME && iRightType == XVO_DT_TIME ) {
		return TRUE;
	}
	return FALSE;
}
XXAPI int xvoBasicCompare(xvalue pLeft, xvalue pRight)
{
	int iLeftType = xvoType(pLeft);
	int iRightType = xvoType(pRight);
	if ( !xvoCanCompareBasic(pLeft, pRight) ) {
		return 2;
	}
	if ( iLeftType == XVO_DT_NULL ) {
		return 0;
	}
	if ( __xvoTypeIsNumeric(iLeftType) && __xvoTypeIsNumeric(iRightType) ) {
		if ( iLeftType == XVO_DT_FLOAT || iRightType == XVO_DT_FLOAT ) {
			return __xvoCompareDouble(xvoGetFloat(pLeft), xvoGetFloat(pRight));
		}
		return __xvoCompareInt64(xvoGetInt(pLeft), xvoGetInt(pRight));
	}
	if ( iLeftType == XVO_DT_TEXT ) {
		int iCmp = xrtStrComp(xvoGetText(pLeft), xvoGetText(pRight), 0, FALSE);
		return (iCmp < 0) ? -1 : ((iCmp > 0) ? 1 : 0);
	}
	return __xvoCompareInt64(xvoGetTime(pLeft), xvoGetTime(pRight));
}
XXAPI bool xvoBasicEqual(xvalue pLeft, xvalue pRight)
{
	return xvoBasicCompare(pLeft, pRight) == 0;
}


// 判断 xvalue 的语言级真假值
XXAPI bool xvoValueTruthy(xvalue pVal)
{
	int iType = xvoType(pVal);
	if ( iType == XVO_DT_NULL ) { return FALSE; }
	if ( iType == XVO_DT_BOOL ) { return xvoGetBool(pVal); }
	if ( iType == XVO_DT_INT ) { return xvoGetInt(pVal) != 0; }
	if ( iType == XVO_DT_FLOAT ) { return xvoGetFloat(pVal) != 0.0; }
	if ( iType == XVO_DT_TEXT ) {
		str sText = xvoGetText(pVal);
		return sText != NULL && sText[0] != 0;
	}
	if ( iType == XVO_DT_ARRAY ) { return xvoArrayItemCount(pVal) != 0; }
	if ( iType == XVO_DT_LIST ) { return xvoListItemCount(pVal) != 0; }
	if ( iType == XVO_DT_COLL ) { return xvoCollItemCount(pVal) != 0; }
	if ( iType == XVO_DT_TABLE ) { return xvoTableItemCount(pVal) != 0; }
	return TRUE;
}


// 将数组的语言级 int64 下标转换为底层 uint32 下标。
static bool __xvoArrayResolveI64(xvalue pVal, int64 iIndex, uint32* pIndex)
{
	uint32 iCount;
	if ( pVal == NULL || pIndex == NULL || xvoType(pVal) != XVO_DT_ARRAY ) {
		return FALSE;
	}
	if ( iIndex < 0 ) {
		iCount = xvoArrayItemCount(pVal);
		if ( iIndex < -(int64)iCount ) {
			return FALSE;
		}
		iIndex += (int64)iCount;
	}
	if ( iIndex < 0 || (uint64)iIndex > 0xFFFFFFFFULL ) {
		return FALSE;
	}
	*pIndex = (uint32)iIndex;
	return TRUE;
}


// 安全按整数下标读取数组或列表
XXAPI xvalue xvoIndexGetI64(xvalue pVal, int64 iIndex)
{
	int iType;
	uint32 iArrayIndex;
	if ( pVal == NULL || xvoIsNull(pVal) ) { return xvoCreateNull(); }
	iType = xvoType(pVal);
	if ( iType == XVO_DT_ARRAY ) {
		return __xvoArrayResolveI64(pVal, iIndex, &iArrayIndex) ?
			xvoArrayGetValue(pVal, iArrayIndex) : xvoCreateNull();
	}
	if ( iType == XVO_DT_LIST ) { return xvoListGetValue(pVal, iIndex); }
	return xvoCreateNull();
}


// 为链式写入取得子项：先分离父容器，再分离子容器 backing。
XXAPI xvalue xvoIndexGetMutableI64(xvalue pVal, int64 iIndex)
{
	xvalue pItem;
	if ( !__xvoEnsureContainerUnique(pVal) ) {
		return NULL;
	}
	pItem = xvoIndexGetI64(pVal, iIndex);
	if ( __xvoIsContainerValue(pItem) && !__xvoEnsureContainerUnique(pItem) ) {
		return NULL;
	}
	return pItem;
}


// 按整数下标写入数组或列表。
XXAPI bool xvoIndexSetI64(xvalue pVal, int64 iIndex, xvalue pItem, bool bColloc)
{
	int iType;
	uint32 iArrayIndex;
	if ( pVal == NULL || pItem == NULL || xvoIsNull(pVal) ) {
		return FALSE;
	}
	iType = xvoType(pVal);
	if ( iType == XVO_DT_ARRAY ) {
		if ( !__xvoArrayResolveI64(pVal, iIndex, &iArrayIndex) ) {
			return FALSE;
		}
		return xvoArraySetValue(pVal, iArrayIndex, pItem, bColloc);
	}
	if ( iType == XVO_DT_LIST ) {
		return xvoListSetValue(pVal, iIndex, pItem, bColloc);
	}
	return FALSE;
}


// 安全按字符串键读取表
XXAPI xvalue xvoIndexGetKey(xvalue pVal, str sKey, uint32 iKeySize)
{
	if ( pVal == NULL || xvoIsNull(pVal) || xvoType(pVal) != XVO_DT_TABLE ) { return xvoCreateNull(); }
	return xvoTableGetValue(pVal, sKey, iKeySize);
}


XXAPI xvalue xvoIndexGetMutableKey(xvalue pVal, str sKey, uint32 iKeySize)
{
	xvalue pItem;
	if ( !__xvoEnsureContainerUnique(pVal) ) {
		return NULL;
	}
	pItem = xvoIndexGetKey(pVal, sKey, iKeySize);
	if ( __xvoIsContainerValue(pItem) && !__xvoEnsureContainerUnique(pItem) ) {
		return NULL;
	}
	return pItem;
}


// 安全按动态键读取容器
XXAPI xvalue xvoIndexGetValue(xvalue pVal, xvalue pKey)
{
	if ( pKey != NULL && xvoType(pKey) == XVO_DT_TEXT ) {
		return xvoIndexGetKey(pVal, xvoGetText(pKey), 0);
	}
	return xvoIndexGetI64(pVal, pKey != NULL ? xvoGetInt(pKey) : 0);
}


XXAPI xvalue xvoIndexGetMutableValue(xvalue pVal, xvalue pKey)
{
	if ( pKey != NULL && xvoType(pKey) == XVO_DT_TEXT ) {
		return xvoIndexGetMutableKey(pVal, xvoGetText(pKey), 0);
	}
	return xvoIndexGetMutableI64(pVal, pKey != NULL ? xvoGetInt(pKey) : 0);
}


typedef struct __xvo_contains_ctx {
	xvalue Item;
	bool Found;
} __xvo_contains_ctx;


static bool __xvoListContainsEach(int64 iKey, xvalue* ppVal, __xvo_contains_ctx* pCtx)
{
	(void)iKey;
	if ( pCtx != NULL && ppVal != NULL && xvoBasicEqual(*ppVal, pCtx->Item) ) {
		pCtx->Found = TRUE;
		return TRUE;
	}
	return FALSE;
}


// 判断容器或字符串是否包含指定值
XXAPI bool xvoValueContains(xvalue pValues, xvalue pItem)
{
	int iValuesType = xvoType(pValues);
	int iItemType = xvoType(pItem);
	if ( iValuesType == XVO_DT_ARRAY ) {
		uint32 i;
		uint32 iCount = xvoArrayItemCount(pValues);
		for ( i = 0; i < iCount; ++i ) {
			if ( xvoBasicEqual(xvoArrayGetValue(pValues, i), pItem) ) { return TRUE; }
		}
		return FALSE;
	}
	if ( iValuesType == XVO_DT_LIST ) {
		__xvo_contains_ctx ctx;
		ctx.Item = pItem;
		ctx.Found = FALSE;
		if ( pValues->vList != NULL ) {
			xrtListWalk(pValues->vList, (ptr)__xvoListContainsEach, &ctx);
		}
		return ctx.Found;
	}
	if ( iValuesType == XVO_DT_COLL ) { return xvoCollExists(pValues, pItem); }
	if ( iValuesType == XVO_DT_TABLE ) {
		return iItemType == XVO_DT_TEXT && xvoTableExists(pValues, xvoGetText(pItem), 0);
	}
	if ( iValuesType == XVO_DT_TEXT && iItemType == XVO_DT_TEXT ) {
		return xrtFindStr(xvoGetText(pValues), 0, xvoGetText(pItem), 0, FALSE) != NULL;
	}
	return FALSE;
}


// 使用可变参数创建数组字面量
XXAPI xvalue xvoCreateArrayArgs(uint32 iCount, ...)
{
	va_list ap;
	uint32 i;
	xvalue pRet = xvoCreateArray();
	if ( pRet == NULL ) { return xvoCreateNull(); }
	va_start(ap, iCount);
	for ( i = 0; i < iCount; ++i ) {
		xvalue pItem = va_arg(ap, xvalue);
		if ( !xvoArrayAppendValue(pRet, pItem, TRUE) && pItem != NULL ) { xvoUnref(pItem); }
	}
	va_end(ap);
	return pRet;
}


// 使用可变参数创建列表字面量，参数为 index/value 成对出现
XXAPI xvalue xvoCreateListArgs(uint32 iCount, ...)
{
	va_list ap;
	uint32 i;
	xvalue pRet = xvoCreateList();
	if ( pRet == NULL ) { return xvoCreateNull(); }
	va_start(ap, iCount);
	for ( i = 0; i < iCount; ++i ) {
		int64 iIndex = va_arg(ap, int64);
		xvalue pItem = va_arg(ap, xvalue);
		if ( !xvoListSetValue(pRet, iIndex, pItem, TRUE) && pItem != NULL ) { xvoUnref(pItem); }
	}
	va_end(ap);
	return pRet;
}


// 使用可变参数创建集合字面量
XXAPI xvalue xvoCreateSetArgs(uint32 iCount, ...)
{
	va_list ap;
	uint32 i;
	xvalue pRet = xvoCreateSet();
	if ( pRet == NULL ) { return xvoCreateNull(); }
	va_start(ap, iCount);
	for ( i = 0; i < iCount; ++i ) {
		xvalue pItem = va_arg(ap, xvalue);
		if ( !xvoCollSetValue(pRet, pItem, TRUE) && pItem != NULL ) { xvoUnref(pItem); }
	}
	va_end(ap);
	return pRet;
}


// 使用可变参数创建表字面量，参数为 key/value 成对出现
XXAPI xvalue xvoCreateTableArgs(uint32 iCount, ...)
{
	va_list ap;
	uint32 i;
	xvalue pRet = xvoCreateTable();
	if ( pRet == NULL ) { return xvoCreateNull(); }
	va_start(ap, iCount);
	for ( i = 0; i < iCount; ++i ) {
		str sKey = va_arg(ap, str);
		xvalue pItem = va_arg(ap, xvalue);
		if ( !xvoTableSetValue(pRet, sKey, 0, pItem, TRUE) && pItem != NULL ) { xvoUnref(pItem); }
	}
	va_end(ap);
	return pRet;
}


static void __xvoPrintWrite(str sText, size_t iSize)
{
	const char* pText;
	size_t iRemain;

	if ( sText == NULL ) { return; }
	if ( iSize == 0 ) { iSize = strlen(__xrt_cstr(sText)); }
	if ( iSize == 0 ) { return; }
	pText = __xrt_cstr(sText);
	iRemain = iSize;
	#if defined(_WIN32) || defined(_WIN64)
		{
			HANDLE hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
			if ( hOutput == NULL || hOutput == INVALID_HANDLE_VALUE ) { return; }
			while ( iRemain > 0 ) {
				DWORD iChunk = iRemain > 0x7fffffffu ? 0x7fffffffu : (DWORD)iRemain;
				DWORD iWritten = 0;
				if ( !WriteFile(hOutput, pText, iChunk, &iWritten, NULL) || iWritten == 0 ) { break; }
				pText += iWritten;
				iRemain -= (size_t)iWritten;
			}
		}
	#else
		while ( iRemain > 0 ) {
			ssize_t iWritten = write(STDOUT_FILENO, pText, iRemain);
			if ( iWritten < 0 && errno == EINTR ) { continue; }
			if ( iWritten <= 0 ) { break; }
			pText += (size_t)iWritten;
			iRemain -= (size_t)iWritten;
		}
	#endif
}


static void __xvoPrintOne(xvalue pVal)
{
	size_t iSize = 0;
	uint32 iSize32 = 0;
	str sText = NULL;
	int iType = xvoType(pVal);
	if ( iType == XVO_DT_NULL ) {
		__xvoPrintWrite((str)"null", 4);
		return;
	}
	if ( iType == XVO_DT_TEXT ) {
		__xvoPrintWrite(xvoGetText(pVal), 0);
		return;
	}
	if ( iType == XVO_DT_ARRAY || iType == XVO_DT_TABLE ) {
		#ifndef XRT_NO_JSON
		sText = xrtStringifyJSON(pVal, FALSE, &iSize);
		#else
		sText = xvoToString(pVal, &iSize32);
		iSize = (size_t)iSize32;
		#endif
	} else if ( iType == XVO_DT_LIST || iType == XVO_DT_COLL ) {
		#ifndef XRT_NO_XSON
		sText = xrtStringifyXSON(pVal, FALSE, 0, &iSize);
		#else
		sText = xvoToString(pVal, &iSize32);
		iSize = (size_t)iSize32;
		#endif
	} else {
		sText = xvoToString(pVal, &iSize32);
		iSize = (size_t)iSize32;
	}
	if ( sText != NULL ) {
		__xvoPrintWrite(sText, iSize);
		xrtFree(sText);
	}
}


// 按 xlang print 语义输出 xvalue；数组参数会按空格展开
XXAPI void xvoPrint(xvalue pVal, bool bNewLine)
{
	/* 先清空可能存在的 CRT 缓冲，随后按原始字节写出，保持调用顺序。 */
	fflush(stdout);
	if ( pVal != NULL && xvoType(pVal) == XVO_DT_ARRAY ) {
		uint32 i;
		uint32 iCount = xvoArrayItemCount(pVal);
		for ( i = 0; i < iCount; ++i ) {
			if ( i > 0 ) { __xvoPrintWrite((str)" ", 1); }
			__xvoPrintOne(xvoArrayGetValue(pVal, i));
		}
	} else {
		__xvoPrintOne(pVal);
	}
	if ( bNewLine ) { __xvoPrintWrite((str)"\n", 1); }
	fflush(stdout);
}


// ??????
XXAPI uint32 xvoGetSize(xvalue pVal)
{
	if ( pVal == NULL ) {
		return 0;
	} else {
		return pVal->Size;
	}
}



typedef struct {
	xvalue Target;
	bool Failed;
} xvo_container_copy_ctx;


static xvalue __xvoShallowCopyItem(xvalue pVal)
{
	if ( pVal == NULL ) {
		return NULL;
	}
	return pVal->Type >= XVO_DT_ARRAY ? xvoShare(pVal) : xvoCopy(pVal);
}


// 浅拷贝 List 元素
bool xvoCopy_ListProc(int64 iKey, xvalue* ppVal, xvo_container_copy_ctx* pCtx)
{
	xvalue pItemCopy;
	if ( ppVal == NULL || pCtx == NULL || pCtx->Target == NULL ) {
		if ( pCtx != NULL ) { pCtx->Failed = TRUE; }
		return TRUE;
	}
	pItemCopy = __xvoShallowCopyItem(ppVal[0]);
	if ( pItemCopy == NULL || !xvoListSetValue(pCtx->Target, iKey, pItemCopy, TRUE) ) {
		if ( pItemCopy != NULL ) { xvoUnref(pItemCopy); }
		pCtx->Failed = TRUE;
		return TRUE;
	}
	return FALSE;
}


// 浅拷贝旧 Coll 元素
bool xvoCopy_CollProc(Coll_Key* pKey, xvo_container_copy_ctx* pCtx)
{
	xvalue pItemCopy;
	if ( pKey == NULL || pCtx == NULL || pCtx->Target == NULL ) {
		if ( pCtx != NULL ) { pCtx->Failed = TRUE; }
		return TRUE;
	}
	pItemCopy = __xvoShallowCopyItem(pKey->Value);
	if ( pItemCopy == NULL || !xvoCollSetValue(pCtx->Target, pItemCopy, TRUE) ) {
		if ( pItemCopy != NULL ) { xvoUnref(pItemCopy); }
		pCtx->Failed = TRUE;
		return TRUE;
	}
	return FALSE;
}


// 浅拷贝 Set 元素
bool xvoCopy_SetProc(const ptr pItem, xvo_container_copy_ctx* pCtx)
{
	xvalue pSource;
	xvalue pItemCopy;
	if ( pItem == NULL || pCtx == NULL || pCtx->Target == NULL ) {
		if ( pCtx != NULL ) { pCtx->Failed = TRUE; }
		return TRUE;
	}
	pSource = *(const xvalue*)pItem;
	pItemCopy = __xvoShallowCopyItem(pSource);
	if ( pItemCopy == NULL || !xvoSetAddValue(pCtx->Target, pItemCopy, TRUE) ) {
		if ( pItemCopy != NULL ) { xvoUnref(pItemCopy); }
		pCtx->Failed = TRUE;
		return TRUE;
	}
	return FALSE;
}


/*
	创建独立 xvalue 外壳并共享容器 backing。
	标量和资源对象继续使用原有引用语义，容器在第一次写入时再分离。
*/
XXAPI xvalue xvoShare(xvalue pVal)
{
	xvalue pRet;
	xvo_container_extra* pExtra;
	uint32 iMode;

	if ( pVal == NULL || pVal->IsStatic || !__xvoIsContainerValue(pVal) ) {
		xvoAddRef_Inline(pVal);
		return pVal;
	}
	pExtra = __xvoContainerExtra(pVal);
	if ( pExtra == NULL ) {
		/* 兼容由 C 手工构造的旧式外壳：没有 backing 元数据时退化为普通浅拷贝。 */
		return xvoCopy(pVal);
	}
	pRet = __xvoAllocValue();
	if ( pRet == NULL ) {
		return NULL;
	}
	iMode = xvoIsShared_Inline(pVal) ? XRT_OBJMODE_SHARED : XRT_OBJMODE_LOCAL;
	xvoInitOwnedHeader_Inline(pRet, pVal->Type, iMode);
	pRet->Size = pVal->Size;
	pRet->vPoint = pVal->vPoint;
	pRet->vExtra = pExtra;
	__xvoContainerExtraAddRef(pExtra);
	return pRet;
}

// xvoCopy_TableProc 相关处理
bool xvoCopy_TableProc(Dict_Key* pKey, xvalue* ppVal, xvo_container_copy_ctx* pCtx)
{
	xvalue pItemCopy;
	if ( pKey == NULL || ppVal == NULL || pCtx == NULL || pCtx->Target == NULL ) {
		if ( pCtx ) {
			pCtx->Failed = TRUE;
		}
		return TRUE;
	}
	pItemCopy = __xvoShallowCopyItem(ppVal[0]);
	if ( pItemCopy == NULL || !xvoTableSetValue(pCtx->Target, pKey->Key, pKey->KeyLen, pItemCopy, TRUE) ) {
		if ( pItemCopy != NULL ) { xvoUnref(pItemCopy); }
		pCtx->Failed = TRUE;
		return TRUE;
	}
	return FALSE;
}


static xvalue __xvoShallowCopyContainerEx(xvalue pVal, uint32 iMode)
{
	xvalue pResult;
	xvo_container_copy_ctx tCtx = {0};
	const xrt_type_desc* pType;
	uint32 i;

	if ( !__xvoIsContainerValue(pVal) ) {
		return NULL;
	}
	pResult = NULL;
	if ( pVal->Type == XVO_DT_ARRAY ) {
		pResult = xvoCreateArrayEx(iMode);
	} else if ( pVal->Type == XVO_DT_LIST ) {
		pResult = xvoCreateListEx(iMode);
	} else if ( pVal->Type == XVO_DT_COLL ) {
		pResult = __xvoIsSetValue(pVal) ? xvoCreateSetEx(iMode) : xvoCreateCollEx(iMode);
	} else if ( pVal->Type == XVO_DT_TABLE ) {
		pResult = xvoCreateTableEx(iMode);
	}
	if ( pResult == NULL ) {
		return NULL;
	}
	pType = __xvoValueTypeDesc(pVal);
	if ( pType != NULL && !xvoSetTypeDesc(pResult, pType) ) {
		xvoUnref(pResult);
		return NULL;
	}

	tCtx.Target = pResult;
	if ( pVal->Type == XVO_DT_ARRAY ) {
		for ( i = 0; i < xvoArrayItemCount(pVal); ++i ) {
			xvalue pItemCopy = __xvoShallowCopyItem(xvoArrayGetValue(pVal, i));
			if ( pItemCopy == NULL || !xvoArrayAppendValue(pResult, pItemCopy, TRUE) ) {
				if ( pItemCopy != NULL ) { xvoUnref(pItemCopy); }
				tCtx.Failed = TRUE;
				break;
			}
		}
	} else if ( pVal->Type == XVO_DT_LIST ) {
		xrtListWalk(pVal->vList, (ptr)xvoCopy_ListProc, &tCtx);
	} else if ( pVal->Type == XVO_DT_COLL && __xvoIsSetValue(pVal) ) {
		xrtSetWalk(pVal->vSet, (xset_each_proc)xvoCopy_SetProc, &tCtx);
	} else if ( pVal->Type == XVO_DT_COLL ) {
		xrtAVLTreeWalk(pVal->vColl, (ptr)xvoCopy_CollProc, &tCtx);
	} else if ( !xvoTableWalkOrdered(pVal, (ptr)xvoCopy_TableProc, &tCtx) ) {
		xrtDictWalk(pVal->vTable, (ptr)xvoCopy_TableProc, &tCtx);
	}
	if ( tCtx.Failed ) {
		xvoUnref(pResult);
		return NULL;
	}
	return pResult;
}


// 复制
XXAPI xvalue xvoCopy(xvalue pVal)
{
	if ( (pVal == NULL) || (pVal->Type == XVO_DT_NULL) ) {
		return &XVO_VALUE_NULL;
	} else if ( pVal->Type == XVO_DT_BOOL ) {
		if ( pVal->vBool ) {
			return &XVO_VALUE_TRUE;
		} else {
			return &XVO_VALUE_FALSE;
		}
	} else if ( (pVal->Type == XVO_DT_FUNC) && __xvoIsCallableValue(pVal) ) {
		return xvoCreateCallable(pVal->vCallable, FALSE);
	} else if ( pVal->Type == XVO_DT_TEXT ) {
		return xvoCreateText(pVal->vText, pVal->Size, FALSE);
	} else if ( __xvoIsContainerValue(pVal) ) {
		return __xvoShallowCopyContainerEx(pVal, XRT_OBJMODE_LOCAL);
	} else if ( pVal->Type == XVO_DT_CLASS ) {
		if ( pVal->vTypeDesc != NULL ) {
			xrt_record_value* pRecord = (xrt_record_value*)pVal->vStruct;
			return xvoCreateRecord(pVal->vTypeDesc, pRecord ? pRecord->Data : NULL);
		} else {
			xvalue varRet = xvoCreateClass(pVal->Size);
			if ( varRet && pVal->vStruct ) {
				memcpy(varRet->vStruct, pVal->vStruct, pVal->Size);
			}
			return varRet;
		}
	} else if ( pVal->Type == XVO_DT_CUSTOM ) {
		if ( pVal->vTypeDesc != NULL ) {
			xrt_handle_value* pHandle = (xrt_handle_value*)pVal->vCustom;
			uint32 iFlags = pHandle ? pHandle->Flags : XRT_HANDLE_FLAG_NULLABLE;
			if ( pHandle != NULL && pVal->vTypeDesc->Ops != NULL && pVal->vTypeDesc->Ops->copy != NULL ) {
				ptr pNewHandle = NULL;
				pVal->vTypeDesc->Ops->copy(&pNewHandle, &pHandle->Handle);
				iFlags &= ~XRT_HANDLE_FLAG_BORROWED;
				iFlags |= XRT_HANDLE_FLAG_OWNED;
				return xvoCreateHandle(pVal->vTypeDesc, pNewHandle, iFlags | XRT_HANDLE_FLAG_NULLABLE);
			}
			/* typed handle 没有复制协议时不可复制；共享同一对象应使用 xvoShare。 */
			return NULL;
		}
		return xvoCreateCustom(pVal->vCustom);
	} else {
		// 其他类型直接 Copy 64 位数据
		xvalue varRet = __xvoAllocValue();
		xvoInitOwnedHeader_Inline(varRet, pVal->Type, XRT_OBJMODE_LOCAL);
		varRet->Size = pVal->Size;
		varRet->vInt = pVal->vInt;
		varRet->vFuncEnv = NULL;
		if ( pVal->Type == XVO_DT_FUNC ) {
			varRet->vFuncEnv = pVal->vFuncEnv;
			if ( (varRet->vFuncEnv != NULL) && (varRet->vFuncEnv->IsStatic == FALSE) ) {
				xvoAddRef_Inline(varRet->vFuncEnv);
			}
		}
		return varRet;
	}
}



// 深拷贝
bool xvoDeepCopy_ListProc(int64 iKey, xvalue* ppVal, xlist objList)
{
	xvalue pItemCopy = xvoDeepCopy(ppVal[0]);
	xrtListSetPtr(objList, iKey, pItemCopy, NULL);
	return FALSE;
}


// xvoDeepCopy_CollProc 相关处理
bool xvoDeepCopy_CollProc(Coll_Key* pKey, xavltree objColl)
{
	xvalue pItemCopy = xvoDeepCopy(pKey->Value);
	Coll_Key k = { pKey->Hash, pItemCopy };
	xvoCollSetValueWithKey(objColl, &k, TRUE);
	return FALSE;
}


// xvoDeepCopy_SetProc 相关处理
bool xvoDeepCopy_SetProc(const ptr pItem, xvalue pSet)
{
	xvalue pVal = pItem ? *(const xvalue*)pItem : NULL;
	if ( pVal ) {
		xvalue pCopy = xvoDeepCopy(pVal);
		xvoSetAddValue(pSet, pCopy, TRUE);
	}
	return FALSE;
}


// xvoDeepCopy_TableProc 相关处理
bool xvoDeepCopy_TableProc(Dict_Key* pKey, xvalue* ppVal, xvo_container_copy_ctx* pCtx)
{
	if ( pKey == NULL || ppVal == NULL || pCtx == NULL || pCtx->Target == NULL ) {
		if ( pCtx ) {
			pCtx->Failed = TRUE;
		}
		return TRUE;
	}
	xvalue pItemCopy = xvoDeepCopy(ppVal[0]);
	if ( !xvoTableSetValue(pCtx->Target, pKey->Key, pKey->KeyLen, pItemCopy, TRUE) ) {
		xvoUnref(pItemCopy);
		pCtx->Failed = TRUE;
		return TRUE;
	}
	return FALSE;
}


// xvoDeepCopy 相关处理
XXAPI xvalue xvoDeepCopy(xvalue pVal)
{
	if ( (pVal == NULL) || (pVal->Type == XVO_DT_NULL) ) {
		return &XVO_VALUE_NULL;
	} else if ( pVal->Type == XVO_DT_BOOL ) {
		if ( pVal->vBool ) {
			return &XVO_VALUE_TRUE;
		} else {
			return &XVO_VALUE_FALSE;
		}
	} else if ( (pVal->Type == XVO_DT_FUNC) && __xvoIsCallableValue(pVal) ) {
		return xvoCreateCallable(pVal->vCallable, FALSE);
	} else if ( pVal->Type == XVO_DT_TEXT ) {
		return xvoCreateText(pVal->vText, pVal->Size, FALSE);
	} else if ( pVal->Type == XVO_DT_ARRAY ) {
		xvalue arrRet = xvoCreateArray();
		const xrt_type_desc* pType = __xvoValueTypeDesc(pVal);
		if ( pType != NULL ) {
			xvoSetTypeDesc(arrRet, pType);
		}
		for ( uint32 i = 1; i <= pVal->vArray->Count; i++ ) {
			xvalue pItem = xrtPtrArrayGet_Inline(pVal->vArray, i);
			xvalue pItemCopy = xvoDeepCopy(pItem);
			xrtPtrArrayAppend(arrRet->vArray, pItemCopy);
		}
		return arrRet;
	} else if ( pVal->Type == XVO_DT_LIST ) {
		xvalue lstRet = xvoCreateList();
		const xrt_type_desc* pType = __xvoValueTypeDesc(pVal);
		if ( pType != NULL ) {
			xvoSetTypeDesc(lstRet, pType);
		}
		xrtListWalk(pVal->vList, (ptr)xvoDeepCopy_ListProc, lstRet->vList);
		return lstRet;
	} else if ( pVal->Type == XVO_DT_COLL ) {
		xvalue setRet = __xvoIsSetValue(pVal) ? xvoCreateSet() : xvoCreateColl();
		const xrt_type_desc* pType = __xvoValueTypeDesc(pVal);
		if ( pType != NULL ) {
			xvoSetTypeDesc(setRet, pType);
		}
		if ( __xvoIsSetValue(pVal) ) {
			xrtSetWalk(pVal->vSet, (xset_each_proc)xvoDeepCopy_SetProc, setRet);
		} else {
			xrtAVLTreeWalk(pVal->vColl, (ptr)xvoDeepCopy_CollProc, setRet->vColl);
		}
		return setRet;
	} else if ( pVal->Type == XVO_DT_TABLE ) {
		xvalue tblRet = xvoCreateTable();
		const xrt_type_desc* pType = __xvoValueTypeDesc(pVal);
		xvo_container_copy_ctx tCtx = {0};
		if ( pType != NULL ) {
			xvoSetTypeDesc(tblRet, pType);
		}
		tCtx.Target = tblRet;
		if ( !xvoTableWalkOrdered(pVal, (ptr)xvoDeepCopy_TableProc, &tCtx) ) {
			xrtDictWalk(pVal->vTable, (ptr)xvoDeepCopy_TableProc, &tCtx);
		}
		return tblRet;
	} else if ( pVal->Type == XVO_DT_CLASS ) {
		if ( pVal->vTypeDesc != NULL ) {
			xrt_record_value* pRecord = (xrt_record_value*)pVal->vStruct;
			if ( pVal->vTypeDesc->Ops != NULL && pVal->vTypeDesc->Ops->clone != NULL ) {
				return __xvoCreateRecord(pVal->vTypeDesc, pRecord ? pRecord->Data : NULL, false, true);
			}
			if ( pVal->vTypeDesc->Kind == XRT_TYPE_KIND_CLASS ) {
				return NULL;
			}
			return xvoCreateRecord(pVal->vTypeDesc, pRecord ? pRecord->Data : NULL);
		} else {
			xvalue varRet = xvoCreateClass(pVal->Size);
			if ( varRet && pVal->vStruct ) {
				memcpy(varRet->vStruct, pVal->vStruct, pVal->Size);
			}
			return varRet;
		}
	} else if ( pVal->Type == XVO_DT_CUSTOM ) {
		if ( pVal->vTypeDesc != NULL ) {
			xrt_handle_value* pHandle = (xrt_handle_value*)pVal->vCustom;
			uint32 iFlags = pHandle ? pHandle->Flags : XRT_HANDLE_FLAG_NULLABLE;
			if ( pHandle != NULL && pVal->vTypeDesc->Ops != NULL && pVal->vTypeDesc->Ops->copy != NULL ) {
				ptr pNewHandle = NULL;
				pVal->vTypeDesc->Ops->copy(&pNewHandle, &pHandle->Handle);
				iFlags &= ~XRT_HANDLE_FLAG_BORROWED;
				iFlags |= XRT_HANDLE_FLAG_OWNED;
				return xvoCreateHandle(pVal->vTypeDesc, pNewHandle, iFlags | XRT_HANDLE_FLAG_NULLABLE);
			}
			iFlags &= ~XRT_HANDLE_FLAG_OWNED;
			iFlags |= XRT_HANDLE_FLAG_BORROWED;
			return xvoCreateHandle(pVal->vTypeDesc, pHandle ? pHandle->Handle : NULL, iFlags | XRT_HANDLE_FLAG_NULLABLE);
		}
		return xvoCreateCustom(pVal->vCustom);
	} else {
		// 其他类型直接 Copy 64 位数据
		xvalue varRet = __xvoAllocValue();
		xvoInitOwnedHeader_Inline(varRet, pVal->Type, XRT_OBJMODE_LOCAL);
		varRet->Size = pVal->Size;
		varRet->vInt = pVal->vInt;
		varRet->vExtra = NULL;
		if ( pVal->Type == XVO_DT_FUNC ) {
			varRet->vFuncEnv = pVal->vFuncEnv;
			if ( (varRet->vFuncEnv != NULL) && (varRet->vFuncEnv->IsStatic == FALSE) ) {
				xvoAddRef_Inline(varRet->vFuncEnv);
			}
		}
		return varRet;
	}
}


typedef struct {
	xvalue Target;
	bool Failed;
	uint32 Depth;
} __xvoSharedCopyCtx;


static xvalue __xvoDeepCopySharedDepth(xvalue pVal, uint32 iDepth);


static bool __xvoDeepCopySharedListProc(int64 iKey, xvalue* ppVal, __xvoSharedCopyCtx* pCtx)
{
	xvalue pItem;
	if ( ppVal == NULL || pCtx == NULL || pCtx->Target == NULL ) {
		if ( pCtx ) {
			pCtx->Failed = TRUE;
		}
		return TRUE;
	}
	pItem = __xvoDeepCopySharedDepth(ppVal[0], pCtx->Depth + 1);
	if ( pItem == NULL || !xvoListSetValue(pCtx->Target, iKey, pItem, TRUE) ) {
		if ( pItem != NULL ) {
			xvoUnref(pItem);
		}
		pCtx->Failed = TRUE;
		return TRUE;
	}
	return FALSE;
}


static bool __xvoDeepCopySharedSetProc(const ptr pItem, __xvoSharedCopyCtx* pCtx)
{
	xvalue pSource;
	xvalue pCopy;
	if ( pItem == NULL || pCtx == NULL || pCtx->Target == NULL ) {
		if ( pCtx ) {
			pCtx->Failed = TRUE;
		}
		return TRUE;
	}
	pSource = *(const xvalue*)pItem;
	pCopy = __xvoDeepCopySharedDepth(pSource, pCtx->Depth + 1);
	if ( pCopy == NULL || !xvoSetAddValue(pCtx->Target, pCopy, TRUE) ) {
		if ( pCopy != NULL ) {
			xvoUnref(pCopy);
		}
		pCtx->Failed = TRUE;
		return TRUE;
	}
	return FALSE;
}


static bool __xvoDeepCopySharedCollProc(Coll_Key* pKey, __xvoSharedCopyCtx* pCtx)
{
	xvalue pCopy;
	if ( pKey == NULL || pCtx == NULL || pCtx->Target == NULL ) {
		if ( pCtx ) {
			pCtx->Failed = TRUE;
		}
		return TRUE;
	}
	pCopy = __xvoDeepCopySharedDepth(pKey->Value, pCtx->Depth + 1);
	if ( pCopy == NULL || !xvoCollSetValue(pCtx->Target, pCopy, TRUE) ) {
		if ( pCopy != NULL ) {
			xvoUnref(pCopy);
		}
		pCtx->Failed = TRUE;
		return TRUE;
	}
	return FALSE;
}


static bool __xvoDeepCopySharedTableProc(Dict_Key* pKey, xvalue* ppVal, __xvoSharedCopyCtx* pCtx)
{
	xvalue pCopy;
	if ( pKey == NULL || ppVal == NULL || pCtx == NULL || pCtx->Target == NULL ) {
		if ( pCtx ) {
			pCtx->Failed = TRUE;
		}
		return TRUE;
	}
	pCopy = __xvoDeepCopySharedDepth(ppVal[0], pCtx->Depth + 1);
	if ( pCopy == NULL || !xvoTableSetValue(pCtx->Target, pKey->Key, pKey->KeyLen, pCopy, TRUE) ) {
		if ( pCopy != NULL ) {
			xvoUnref(pCopy);
		}
		pCtx->Failed = TRUE;
		return TRUE;
	}
	return FALSE;
}


// 创建可安全发布到其他线程的完整共享值图
static xvalue __xvoDeepCopySharedDepth(xvalue pVal, uint32 iDepth)
{
	xvalue pResult;
	__xvoSharedCopyCtx tCtx;
	uint32 i;

	if ( iDepth > 256 ) {
		xrtSetError((str)"shared value graph exceeds 256 nested containers or contains a cycle.", FALSE);
		return NULL;
	}
	if ( pVal == NULL || pVal->Type == XVO_DT_NULL ) {
		return &XVO_VALUE_NULL;
	}
	if ( pVal->Type == XVO_DT_BOOL ) {
		return pVal->vBool ? &XVO_VALUE_TRUE : &XVO_VALUE_FALSE;
	}
	/*
		class 与 native-backed class 具有稳定对象身份。
		共享容器复制值图时只复制容器结构，对已经发布的对象保留同一句柄，
		否则会误入 payload 深拷贝，并破坏对象身份或 native 资源生命周期。
	*/
	if ( xvoIsShared_Inline(pVal) &&
		(pVal->Type == XVO_DT_CLASS || pVal->Type == XVO_DT_CUSTOM) ) {
		xvoAddRef_Inline(pVal);
		return pVal;
	}
	if ( pVal->Type == XVO_DT_ARRAY ) {
		const xrt_type_desc* pType = __xvoValueTypeDesc(pVal);
		pResult = xvoCreateArrayEx(XRT_OBJMODE_SHARED);
		if ( pResult == NULL ) {
			return NULL;
		}
		if ( pType != NULL ) {
			xvoSetTypeDesc(pResult, pType);
		}
		for ( i = 1; i <= pVal->vArray->Count; ++i ) {
			xvalue pItem = __xvoDeepCopySharedDepth(xrtPtrArrayGet_Inline(pVal->vArray, i), iDepth + 1);
			if ( pItem == NULL || !xvoArrayAppendValue(pResult, pItem, TRUE) ) {
				if ( pItem != NULL ) {
					xvoUnref(pItem);
				}
				xvoUnref(pResult);
				return NULL;
			}
		}
		return pResult;
	}
	if ( pVal->Type == XVO_DT_LIST ) {
		const xrt_type_desc* pType = __xvoValueTypeDesc(pVal);
		pResult = xvoCreateListEx(XRT_OBJMODE_SHARED);
		if ( pResult == NULL ) {
			return NULL;
		}
		if ( pType != NULL ) {
			xvoSetTypeDesc(pResult, pType);
		}
		tCtx.Target = pResult;
		tCtx.Failed = FALSE;
		tCtx.Depth = iDepth;
		xrtListWalk(pVal->vList, (ptr)__xvoDeepCopySharedListProc, &tCtx);
		if ( tCtx.Failed ) {
			xvoUnref(pResult);
			return NULL;
		}
		return pResult;
	}
	if ( pVal->Type == XVO_DT_COLL ) {
		const xrt_type_desc* pType = __xvoValueTypeDesc(pVal);
		pResult = __xvoIsSetValue(pVal)
			? xvoCreateSetEx(XRT_OBJMODE_SHARED)
			: xvoCreateCollEx(XRT_OBJMODE_SHARED);
		if ( pResult == NULL ) {
			return NULL;
		}
		if ( pType != NULL ) {
			xvoSetTypeDesc(pResult, pType);
		}
		tCtx.Target = pResult;
		tCtx.Failed = FALSE;
		tCtx.Depth = iDepth;
		if ( __xvoIsSetValue(pVal) ) {
			xrtSetWalk(pVal->vSet, (xset_each_proc)__xvoDeepCopySharedSetProc, &tCtx);
		} else {
			xrtAVLTreeWalk(pVal->vColl, (ptr)__xvoDeepCopySharedCollProc, &tCtx);
		}
		if ( tCtx.Failed ) {
			xvoUnref(pResult);
			return NULL;
		}
		return pResult;
	}
	if ( pVal->Type == XVO_DT_TABLE ) {
		const xrt_type_desc* pType = __xvoValueTypeDesc(pVal);
		pResult = xvoCreateTableEx(XRT_OBJMODE_SHARED);
		if ( pResult == NULL ) {
			return NULL;
		}
		if ( pType != NULL ) {
			xvoSetTypeDesc(pResult, pType);
		}
		tCtx.Target = pResult;
		tCtx.Failed = FALSE;
		tCtx.Depth = iDepth;
		if ( !xvoTableWalkOrdered(pVal, (ptr)__xvoDeepCopySharedTableProc, &tCtx) ) {
			xrtDictWalk(pVal->vTable, (ptr)__xvoDeepCopySharedTableProc, &tCtx);
		}
		if ( tCtx.Failed ) {
			xvoUnref(pResult);
			return NULL;
		}
		return pResult;
	}

	pResult = xvoDeepCopy(pVal);
	if ( pResult == NULL || !xvoMakeShared_Inline(pResult) ) {
		if ( pResult != NULL ) {
			xvoUnref(pResult);
		}
		return NULL;
	}
	return pResult;
}


static xvalue __xvoDeepCopyShared(xvalue pVal)
{
	return __xvoDeepCopySharedDepth(pVal, 0);
}


XXAPI xvalue xvoCopyEx(xvalue pVal, uint32 iMode)
{
	if ( iMode == XRT_OBJMODE_SHARED && __xvoIsContainerValue(pVal) ) {
		return __xvoShallowCopyContainerEx(pVal, XRT_OBJMODE_SHARED);
	}
	return iMode == XRT_OBJMODE_SHARED ? __xvoDeepCopyShared(pVal) : xvoCopy(pVal);
}


XXAPI xvalue xvoDeepCopyEx(xvalue pVal, uint32 iMode)
{
	return iMode == XRT_OBJMODE_SHARED ? __xvoDeepCopyShared(pVal) : xvoDeepCopy(pVal);
}


/*
	把一个值的所有权发布到共享环境。
	class 和 typed handle 都具有稳定对象身份，必须移动原外壳；复制对象或句柄后
	再释放原值会破坏身份，native handle 还可能指向已经析构的资源。
	class 的字段由生成代码在对象发布前逐项发布，其他复合值仍创建独立共享图。
*/
XXAPI xvalue xvoMoveToShared(xvalue pVal)
{
	xvalue pResult;

	if ( pVal == NULL || xvoIsShared_Inline(pVal) ) {
		return pVal;
	}
	if ( (pVal->Type == XVO_DT_CLASS || pVal->Type == XVO_DT_CUSTOM) && pVal->vTypeDesc != NULL ) {
		return xvoMakeShared_Inline(pVal) ? pVal : NULL;
	}
	pResult = xvoDeepCopyEx(pVal, XRT_OBJMODE_SHARED);
	xvoUnref(pVal);
	return pResult;
}


/*
	容器第一次写入时分离 backing。
	共享模式使用完整共享图复制，避免新根夹带本地线程对象；本地模式使用浅拷贝根。
*/
static bool __xvoEnsureContainerUnique(xvalue pVal)
{
	xvo_container_extra* pExtra;
	xvalue pCopy;
	ptr pOldRoot;
	ptr pOldExtra;
	uint32 iOldSize;
	uint32 iMode;

	if ( !__xvoIsContainerValue(pVal) ) {
		return TRUE;
	}
	pExtra = __xvoContainerExtra(pVal);
	if ( pExtra == NULL || pExtra->RefCount <= 1 ) {
		return TRUE;
	}
	iMode = xvoIsShared_Inline(pVal) ? XRT_OBJMODE_SHARED : XRT_OBJMODE_LOCAL;
	pCopy = __xvoShallowCopyContainerEx(pVal, iMode);
	if ( pCopy == NULL ) {
		return FALSE;
	}

	/* 只交换 backing，保留调用方 xvalue 外壳地址和引用计数。 */
	pOldRoot = pVal->vPoint;
	pOldExtra = pVal->vExtra;
	iOldSize = pVal->Size;
	pVal->vPoint = pCopy->vPoint;
	pVal->vExtra = pCopy->vExtra;
	pVal->Size = pCopy->Size;
	pCopy->vPoint = pOldRoot;
	pCopy->vExtra = pOldExtra;
	pCopy->Size = iOldSize;
	xvoUnref(pCopy);
	return TRUE;
}


XXAPI bool xvoEnsureUnique(xvalue pVal)
{
	return __xvoEnsureContainerUnique(pVal);
}



// 输出 value 的结构和值
bool xvoPrintValue_TableItemProc(Dict_Key* pKey, xvalue* ppVal, int iLevel)
{
	xvoPrintValue(*ppVal, iLevel, 2, 0, pKey->Key);
	return FALSE;
}


// 输出值列表 item 进程
bool xvoPrintValue_ListItemProc(int64 iKey, xvalue* ppVal, int iLevel)
{
	xvoPrintValue(*ppVal, iLevel, 1, iKey, NULL);
	return FALSE;
}


// xvoPrintValue_CollItemProc 相关处理
bool xvoPrintValue_CollItemProc(Coll_Key* pKey, int iLevel)
{
	xvoPrintValue(pKey->Value, iLevel, 0, 0, NULL);
	return FALSE;
}


// 输出值
XXAPI void xvoPrintValue(xvalue objVal, int iLevel, int iMode, int64 iKey, str sKey)
{
	for ( int i = 0; i < iLevel; i++ ) {
		printf("    ");
	}
	if ( iMode == 1 ) {
		// 输出数组元素
		if ( objVal == NULL ) {
			printf("(empty) %lld = (empty)\n", iKey);
		} else if ( objVal->Type == XVO_DT_NULL ) {
			printf("(null ) [%p] %lld = (null)\n", (void*)objVal, iKey);
		} else if ( objVal->Type == XVO_DT_BOOL ) {
			printf("(bool ) [%p] %lld = (%s)\n", (void*)objVal, iKey, xvoGetText(objVal));
		} else if ( objVal->Type == XVO_DT_INT ) {
			printf("( int ) [%p] %lld = %lld\n", (void*)objVal, iKey, xvoGetInt(objVal));
		} else if ( objVal->Type == XVO_DT_FLOAT ) {
			printf("(float) [%p] %lld = %lf\n", (void*)objVal, iKey, xvoGetFloat(objVal));
		} else if ( objVal->Type == XVO_DT_TEXT ) {
			printf("(text ) [%p] %lld = \"%s\"\n", (void*)objVal, iKey, xvoGetText(objVal));
		} else if ( objVal->Type == XVO_DT_TIME ) {
			printf("(time ) [%p] %lld = < %s >\n", (void*)objVal, iKey, xvoGetText(objVal));
		} else if ( objVal->Type == XVO_DT_POINT ) {
			printf("(point) [%p] %lld = %p\n", (void*)objVal, iKey, xvoGetPoint(objVal));
		} else if ( objVal->Type == XVO_DT_FUNC ) {
			printf("(func ) [%p] %lld = address:0x%" PRIxPTR "\n", (void*)objVal, iKey, (uintptr)xvoGetFunc(objVal));
		} else if ( objVal->Type == XVO_DT_ARRAY ) {
			printf("(array) [%p] %lld = (array), count : %d\n", (void*)objVal, iKey, xvoArrayItemCount(objVal));
		} else if ( objVal->Type == XVO_DT_LIST ) {
			printf("(list ) [%p] %lld = (list), count : %d\n", (void*)objVal, iKey, xvoListItemCount(objVal));
		} else if ( objVal->Type == XVO_DT_TABLE ) {
			printf("(table) [%p] %lld = (table), count : %d\n", (void*)objVal, iKey, xvoTableItemCount(objVal));
		} else if ( objVal->Type == XVO_DT_COLL ) {
			printf("(coll ) [%p] %lld = (coll), count : %d\n", (void*)objVal, iKey, xvoCollItemCount(objVal));
		} else if ( objVal->Type == XVO_DT_CLASS ) {
			printf("(class) [%p] %lld = (class), size : %d\n", (void*)objVal, iKey, objVal->Size);
		} else {
			printf("Unknown data type\n");
		}
	} else if ( iMode == 2 ) {
		// 输出表元素
		if ( objVal == NULL ) {
			printf("(empty) \"%s\" = (empty)\n", sKey);
		} else if ( objVal->Type == XVO_DT_NULL ) {
			printf("(null ) [%p] \"%s\" = (null)\n", (void*)objVal, sKey);
		} else if ( objVal->Type == XVO_DT_BOOL ) {
			printf("(bool ) [%p] \"%s\" = (%s)\n", (void*)objVal, sKey, xvoGetText(objVal));
		} else if ( objVal->Type == XVO_DT_INT ) {
			printf("( int ) [%p] \"%s\" = %lld\n", (void*)objVal, sKey, xvoGetInt(objVal));
		} else if ( objVal->Type == XVO_DT_FLOAT ) {
			printf("(float) [%p] \"%s\" = %lf\n", (void*)objVal, sKey, xvoGetFloat(objVal));
		} else if ( objVal->Type == XVO_DT_TEXT ) {
			printf("(text ) [%p] \"%s\" = \"%s\"\n", (void*)objVal, sKey, xvoGetText(objVal));
		} else if ( objVal->Type == XVO_DT_TIME ) {
			printf("(time ) [%p] \"%s\" = < %s >\n", (void*)objVal, sKey, xvoGetText(objVal));
		} else if ( objVal->Type == XVO_DT_POINT ) {
			printf("(point) [%p] \"%s\" = %p\n", (void*)objVal, sKey, xvoGetPoint(objVal));
		} else if ( objVal->Type == XVO_DT_FUNC ) {
			printf("(func ) [%p] \"%s\" = address:0x%" PRIxPTR "\n", (void*)objVal, sKey, (uintptr)xvoGetFunc(objVal));
		} else if ( objVal->Type == XVO_DT_ARRAY ) {
			printf("(array) [%p] \"%s\" = (array), count : %d\n", (void*)objVal, sKey, xvoArrayItemCount(objVal));
		} else if ( objVal->Type == XVO_DT_LIST ) {
			printf("(list ) [%p] \"%s\" = (list), count : %d\n", (void*)objVal, sKey, xvoListItemCount(objVal));
		} else if ( objVal->Type == XVO_DT_TABLE ) {
			printf("(table) [%p] \"%s\" = (table), count : %d\n", (void*)objVal, sKey, xvoTableItemCount(objVal));
		} else if ( objVal->Type == XVO_DT_COLL ) {
			printf("(coll ) [%p] \"%s\" = (coll), count : %d\n", (void*)objVal, sKey, xvoCollItemCount(objVal));
		} else if ( objVal->Type == XVO_DT_CLASS ) {
			printf("(class) [%p] \"%s\" = (class), size : %d\n", (void*)objVal, sKey, objVal->Size);
		} else {
			printf("Unknown data type\n");
		}
	} else {
		// 输出元素
		if ( objVal == NULL ) {
			printf("(empty)\n");
		} else if ( objVal->Type == XVO_DT_NULL ) {
			printf("(null ) [%p] (null)\n", (void*)objVal);
		} else if ( objVal->Type == XVO_DT_BOOL ) {
			printf("(bool ) [%p] (%s)\n", (void*)objVal, xvoGetText(objVal));
		} else if ( objVal->Type == XVO_DT_INT ) {
			printf("( int ) [%p] %lld\n", (void*)objVal, xvoGetInt(objVal));
		} else if ( objVal->Type == XVO_DT_FLOAT ) {
			printf("(float) [%p] %lf\n", (void*)objVal, xvoGetFloat(objVal));
		} else if ( objVal->Type == XVO_DT_TEXT ) {
			printf("(text ) [%p] \"%s\"\n", (void*)objVal, xvoGetText(objVal));
		} else if ( objVal->Type == XVO_DT_TIME ) {
			printf("(time ) [%p] < %s >\n", (void*)objVal, xvoGetText(objVal));
		} else if ( objVal->Type == XVO_DT_POINT ) {
			printf("(point) [%p] %p\n", (void*)objVal, xvoGetPoint(objVal));
		} else if ( objVal->Type == XVO_DT_FUNC ) {
			printf("(func ) [%p] address:0x%" PRIxPTR "\n", (void*)objVal, (uintptr)xvoGetFunc(objVal));
		} else if ( objVal->Type == XVO_DT_ARRAY ) {
			printf("(array) [%p] (array), count : %d\n", (void*)objVal, xvoArrayItemCount(objVal));
		} else if ( objVal->Type == XVO_DT_LIST ) {
			printf("(list ) [%p] (list), count : %d\n", (void*)objVal, xvoListItemCount(objVal));
		} else if ( objVal->Type == XVO_DT_TABLE ) {
			printf("(table) [%p] (table), count : %d\n", (void*)objVal, xvoTableItemCount(objVal));
		} else if ( objVal->Type == XVO_DT_COLL ) {
			printf("(coll ) [%p] (coll), count : %d\n", (void*)objVal, xvoCollItemCount(objVal));
		} else if ( objVal->Type == XVO_DT_CLASS ) {
			printf("(class) [%p] (class), size : %d\n", (void*)objVal, objVal->Size);
		} else {
			printf("Unknown data type : %d\n", objVal->Type);
		}
	}
	if ( objVal ) {
		if ( objVal->Type == XVO_DT_ARRAY ) {
			for ( int64 i = 0; i < objVal->vArray->Count; i++ ) {
				xvalue objItem = xvoArrayGetValue(objVal, i);
				xvoPrintValue(objItem, iLevel + 1, 1, i, NULL);
			}
		} else if ( objVal->Type == XVO_DT_LIST ) {
			xrtListWalk(objVal->vList, (ptr)xvoPrintValue_ListItemProc, (ptr)(intptr_t)(iLevel+1));
		} else if ( objVal->Type == XVO_DT_TABLE ) {
			xrtDictWalk(objVal->vTable, (ptr)xvoPrintValue_TableItemProc, (ptr)(intptr_t)(iLevel+1));
		} else if ( objVal->Type == XVO_DT_COLL ) {
			xrtAVLTreeWalk(objVal->vColl, (ptr)xvoPrintValue_CollItemProc, (ptr)(intptr_t)(iLevel+1));
		}
	}
}

