


// 哈希表比较函数（内部函数）
int List_CompProc(int64* pNode, int64* pObjKey)
{
	if ( *pNode == *pObjKey ) {
		return 0;
	} else if ( *pNode > *pObjKey ) {
		return -1;
	} else {
		return 1;
	}
}



// 创建列表
XXAPI xlist xrtListCreate(uint32 iItemLength, uint32 iMode)
{
	xlist objList = xrtMalloc(sizeof(xlist_struct));
	if ( objList ) {
		xrtListInit(objList, iItemLength, iMode);
	}
	return objList;
}

// 销毁列表
XXAPI void xrtListDestroy(xlist objList)
{
	if ( objList ) {
		if ( !xrtOwnerCheckMutable(&objList->Owner, "list belongs to another thread.") ) {
			return;
		}
			(xrtListUnit)(objList);
		xrtFree(objList);
	}
}

// 初始化列表（对自维护结构体指针使用）
XXAPI void xrtListInit(xlist objList, uint32 iItemLength, uint32 iMode)
{
	xrtOwnerInitMode(&objList->Owner, iMode);
	xrtAVLTreeInit(&objList->AVLT, iItemLength + sizeof(int64), (ptr)List_CompProc, iMode);
	if ( iMode == XRT_OBJMODE_SHARED ) {
		xrtOwnerActivateShared(&objList->Owner);
		xrtOwnerActivateShared(&objList->AVLT.Owner);
	}
}

// 释放列表（对自维护结构体指针使用）
static inline void __xrtListUnit_NoLock(xlist objList)
{
	(xrtAVLTreeUnit)(&objList->AVLT);
}


// 释放列表
XXAPI void xrtListUnit(xlist objList)
{
	if ( !xrtOwnerBeginMutable(&objList->Owner, "list belongs to another thread.") ) {
		return;
	}
	__xrtListUnit_NoLock(objList);
	xrtOwnerEndMutable(&objList->Owner);
}

#ifdef XRT_MEM_DEBUG
// 创建列表调试
XXAPI xlist xrtListCreateDbg(uint32 iItemLength, uint32 iMode, const char* sFile, uint32 iLine)
{
	xrtMemDebugSiteScope tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
	xlist objList = xrtListCreate(iItemLength, iMode);
	__xrtMemDebugLeaveSiteScope(&tScope);
	__xrtMemDebugRegisterObject(objList, XRT_MEMDEBUG_OBJECT_LIST, XRT_MEMDEBUG_OBJECT_ORIGIN_CREATE, sFile, iLine);
	return objList;
}


// 初始化列表调试
XXAPI void xrtListInitDbg(xlist objList, uint32 iItemLength, uint32 iMode, const char* sFile, uint32 iLine)
{
	xrtMemDebugSiteScope tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
	xrtListInit(objList, iItemLength, iMode);
	__xrtMemDebugLeaveSiteScope(&tScope);
	__xrtMemDebugRegisterObject(objList, XRT_MEMDEBUG_OBJECT_LIST, XRT_MEMDEBUG_OBJECT_ORIGIN_INIT, sFile, iLine);
}


// 销毁列表调试
XXAPI void xrtListDestroyDbg(xlist objList, const char* sFile, uint32 iLine)
{
	if ( objList ) {
		xrtMemDebugSiteScope tScope;
		if ( !__xrtMemDebugObjectGuardDestroy(objList, XRT_MEMDEBUG_OBJECT_LIST, sFile, iLine) ) {
			return;
		}
		if ( !xrtOwnerCheckMutable(&objList->Owner, "list belongs to another thread.") ) {
			return;
		}
		tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
		(xrtListUnit)(objList);
		__xrtMemDebugLeaveSiteScope(&tScope);
		__xrtMemDebugUnregisterObject(objList, XRT_MEMDEBUG_OBJECT_LIST, sFile, iLine);
		xrtFreeDbg(objList, sFile, iLine);
	}
}


// 释放列表调试
XXAPI void xrtListUnitDbg(xlist objList, const char* sFile, uint32 iLine)
{
	xrtMemDebugSiteScope tScope;
	if ( objList == NULL ) {
		return;
	}
	if ( !__xrtMemDebugObjectGuardDestroy(objList, XRT_MEMDEBUG_OBJECT_LIST, sFile, iLine) ) {
		return;
	}
	if ( !xrtOwnerBeginMutable(&objList->Owner, "list belongs to another thread.") ) {
		return;
	}
	tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
	__xrtListUnit_NoLock(objList);
	__xrtMemDebugLeaveSiteScope(&tScope);
	xrtOwnerEndMutable(&objList->Owner);
	__xrtMemDebugUnregisterObject(objList, XRT_MEMDEBUG_OBJECT_LIST, sFile, iLine);
}


// 璁剧疆鍊艰皟璇?
XXAPI ptr xrtListSetDbg(xlist objList, int64 iKey, bool* bNewRet, const char* sFile, uint32 iLine)
{
	xrtMemDebugSiteScope tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
	ptr pRet = xrtListSet(objList, iKey, bNewRet);
	__xrtMemDebugLeaveSiteScope(&tScope);
	return pRet;
}


// 璁剧疆鎸囬拡鍊艰皟璇?
XXAPI bool xrtListSetPtrDbg(xlist objList, int64 iKey, ptr pVal, ptr* ppOldVal, const char* sFile, uint32 iLine)
{
	xrtMemDebugSiteScope tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
	bool bRet = xrtListSetPtr(objList, iKey, pVal, ppOldVal);
	__xrtMemDebugLeaveSiteScope(&tScope);
	return bRet;
}


// 鍒犻櫎鍊艰皟璇?
XXAPI bool xrtListRemoveDbg(xlist objList, int64 iKey, const char* sFile, uint32 iLine)
{
	xrtMemDebugSiteScope tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
	bool bRet = xrtListRemove(objList, iKey);
	__xrtMemDebugLeaveSiteScope(&tScope);
	return bRet;
}


// 鍒犻櫎鎸囬拡鍊艰皟璇?
XXAPI ptr xrtListRemovePtrDbg(xlist objList, int64 iKey, const char* sFile, uint32 iLine)
{
	xrtMemDebugSiteScope tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
	ptr pRet = xrtListRemovePtr(objList, iKey);
	__xrtMemDebugLeaveSiteScope(&tScope);
	return pRet;
}
#endif

// 锁定列表
XXAPI bool xrtListLock(xlist objList)
{
	if ( objList == NULL ) {
		return FALSE;
	}
	return xrtOwnerLock(&objList->Owner, "list belongs to another thread.");
}


// 解锁列表
XXAPI void xrtListUnlock(xlist objList)
{
	if ( objList == NULL ) {
		return;
	}
	xrtOwnerUnlock(&objList->Owner);
}

// 设置值
XXAPI ptr xrtListSet(xlist objList, int64 iKey, bool* bNewRet)
{
	ptr pRet = NULL;
	if ( !xrtOwnerBeginMutable(&objList->Owner, "list belongs to another thread.") ) {
		return NULL;
	}
	bool bNew;
	int64* pNode = xrtAVLTreeInsert(&objList->AVLT, &iKey, &bNew);
	if ( pNode == NULL ) {
		xrtOwnerEndMutable(&objList->Owner);
		return NULL;
	}
	if ( bNewRet ) {
		*bNewRet = bNew;
	}
	if ( bNew ) {
		*pNode = iKey;
	}
	pRet = &pNode[1];
	xrtOwnerEndMutable(&objList->Owner);
	return pRet;
}

// 设置值 - 当值为 ptr 时直接修改指针内容
XXAPI bool xrtListSetPtr(xlist objList, int64 iKey, ptr pVal, ptr* ppOldVal)
{
	if ( !xrtOwnerBeginMutable(&objList->Owner, "list belongs to another thread.") ) {
		if ( ppOldVal ) {
			*ppOldVal = NULL;
		}
		return FALSE;
	}
	bool bNew;
	int64* pNode = xrtAVLTreeInsert(&objList->AVLT, &iKey, &bNew);
	if ( pNode == NULL ) {
		xrtOwnerEndMutable(&objList->Owner);
		return FALSE;
	}
	if ( bNew ) {
		*pNode = iKey;
	}
	// 获取单指针数据结构
	struct {
		ptr val;
	} *pData = (ptr)&pNode[1];
	// 传回旧值
	if ( ppOldVal ) {
		if ( bNew ) {
			*ppOldVal = NULL;
		} else {
			*ppOldVal = pData->val;
		}
	}
	// 修改为新值
	pData->val = pVal;
	xrtOwnerEndMutable(&objList->Owner);
	return TRUE;
}

// 获取值
XXAPI ptr xrtListGet(xlist objList, int64 iKey)
{
	ptr pRet = NULL;
	if ( !xrtOwnerBeginMutable(&objList->Owner, "list belongs to another thread.") ) {
		return NULL;
	}
	int64* pNode = xrtAVLTreeSearch(&objList->AVLT, &iKey);
	if ( pNode ) {
		pRet = &pNode[1];
	}
	xrtOwnerEndMutable(&objList->Owner);
	return pRet;
}

// 获取值 - 当值为 ptr 时直接获取指针内容
XXAPI ptr xrtListGetPtr(xlist objList, int64 iKey)
{
	ptr result = NULL;
	if ( !xrtOwnerBeginMutable(&objList->Owner, "list belongs to another thread.") ) {
		return NULL;
	}
	int64* pNode = xrtAVLTreeSearch(&objList->AVLT, &iKey);
	if ( pNode ) {
		// 指针大小可能为 4 或 8 字节，使用 memcpy 确保跨平台兼容性
		memcpy(&result, &pNode[1], sizeof(ptr));
	}
	xrtOwnerEndMutable(&objList->Owner);
	return result;
}

// 删除值
XXAPI bool xrtListRemove(xlist objList, int64 iKey)
{
	bool bRet;
	if ( !xrtOwnerBeginMutable(&objList->Owner, "list belongs to another thread.") ) {
		return FALSE;
	}
	bRet = xrtAVLTreeRemove(&objList->AVLT, &iKey);
	xrtOwnerEndMutable(&objList->Owner);
	return bRet;
}

// 删除值，当值为 ptr 时返回 ptr
XXAPI ptr xrtListRemovePtr(xlist objList, int64 iKey)
{
	ptr result = NULL;
	if ( !xrtOwnerBeginMutable(&objList->Owner, "list belongs to another thread.") ) {
		return NULL;
	}
	xavltnode pDelNode = xrtAVLTB_Remove((xavltbase)&objList->AVLT, objList->AVLT.CompProc, &iKey);
	if ( pDelNode ) {
		int64* pKeyPtr = (int64*)xrtAVLTreeGetNodeData(pDelNode);  // List 使用 int64 作为键
		ptr* pData = (ptr*)&pKeyPtr[1];
		result = pData[0];  // 先保存返回值
		if ( objList->AVLT.FreeProc ) {
			objList->AVLT.FreeProc(&objList->AVLT, &pDelNode[1]);
		}
		xrtFSMemPoolFree(&objList->AVLT.objMM, pDelNode);
	}
	xrtOwnerEndMutable(&objList->Owner);
	return result;  // 返回保存的值
}

// 判断值是否存在
XXAPI bool xrtListExists(xlist objList, int64 iKey)
{
	bool bRet = FALSE;
	if ( !xrtOwnerBeginMutable(&objList->Owner, "list belongs to another thread.") ) {
		return FALSE;
	}
	int64* pNode = xrtAVLTreeSearch(&objList->AVLT, &iKey);
	if ( pNode ) {
		bRet = TRUE;
	}
	xrtOwnerEndMutable(&objList->Owner);
	return bRet;
}

// 获取表内元素数量
XXAPI uint32 xrtListCount(xlist objList)
{
	uint32 iCount = 0;
	if ( !xrtOwnerBeginMutable(&objList->Owner, "list belongs to another thread.") ) {
		return 0;
	}
	iCount = objList->AVLT.Count;
	xrtOwnerEndMutable(&objList->Owner);
	return iCount;
}

// 遍历表元素
int List_WalkRecuProc(xavltnode root, List_EachProc procEach, ptr pArg)
{
	if ( root ) {
		// 递归左子树
		if ( root->left != NULL ) {
			if ( List_WalkRecuProc(root->left, procEach, pArg) ) {
				return -1;
			}
		}
		// 调用回调函数
		if ( procEach ) {
			if ( procEach(((int64*)&root[1])[0], (ptr)((uint8*)root + sizeof(xavltnode_struct) + sizeof(int64)), pArg) ) {
				return -1;
			}
		}
		// 递归右子树
		if ( root->right != NULL ) {
			if ( List_WalkRecuProc(root->right, procEach, pArg) ) {
				return -1;
			}
		}
	}
	return 0;
}


// 遍历列表
XXAPI void xrtListWalk(xlist objList, List_EachProc procEach, ptr pArg)
{
	if ( !xrtOwnerBeginMutable(&objList->Owner, "list belongs to another thread.") ) {
		return;
	}
	List_WalkRecuProc(objList->AVLT.RootNode, procEach, pArg);
	xrtOwnerEndMutable(&objList->Owner);
}


