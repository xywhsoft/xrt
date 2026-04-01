


// 创建 AVLTree
XXAPI xavltree xrtAVLTreeCreate(unsigned int iItemLength, AVLTree_CompProc procComp, uint32 iMode)
{
	xavltree objAVLT = xrtMalloc(sizeof(xavltree_struct));
	if ( objAVLT ) {
		xrtAVLTreeInit(objAVLT, iItemLength, procComp, iMode);
	}
	return objAVLT;
}

// 销毁 AVLTree
XXAPI void xrtAVLTreeDestroy(xavltree objAVLT)
{
	if ( objAVLT ) {
		if ( !xrtOwnerCheckMutable(&objAVLT->Owner, "avltree belongs to another thread.") ) {
			return;
		}
		xrtAVLTreeUnit(objAVLT);
		xrtFree(objAVLT);
	}
}

// 初始化 AVLTree（对自维护结构体指针使用，和 AVLTree_Create 功能类似）
XXAPI void xrtAVLTreeInit(xavltree objAVLT, unsigned int iItemLength, AVLTree_CompProc procComp, uint32 iMode)
{
	xrtOwnerInitMode(&objAVLT->Owner, iMode);
	xrtAVLTB_Init(objAVLT);
	objAVLT->Parent = NULL;
	objAVLT->CompProc = procComp;
	objAVLT->FreeProc = NULL;
	if ( iItemLength > (unsigned int)(UINT_MAX - sizeof(xavltnode_struct)) ) {
		xrtSetError("AVLTree item length too large.", FALSE);
		iItemLength = (unsigned int)(UINT_MAX - sizeof(xavltnode_struct));
	}
	xrtFSMemPoolInit(&objAVLT->objMM, sizeof(xavltnode_struct) + iItemLength, iMode);
	objAVLT->NodeCache = NULL;
	if ( iMode == XRT_OBJMODE_SHARED ) {
		xrtOwnerActivateShared(&objAVLT->Owner);
	}
}

// 释放 AVLTree（对自维护结构体指针使用，和 AVLTree_Destroy 功能类似）
void xrtAVLTreeUnit_FreeKeysRecuProc(xavltree objAVLT, xavltnode root)
{
	if ( root ) {
		// 递归左子树
		if ( root->left != NULL ) {
			xrtAVLTreeUnit_FreeKeysRecuProc(objAVLT, root->left);
		}
		// 释放 Key
		Dict_Key* pNode = xrtAVLTreeGetNodeData(root);
		objAVLT->FreeProc(objAVLT, pNode);
		// 递归右子树
		if ( root->right != NULL ) {
			xrtAVLTreeUnit_FreeKeysRecuProc(objAVLT, root->right);
		}
	}
}


// 内部函数：__xrtAVLTreeUnit_NoLock
static inline void __xrtAVLTreeUnit_NoLock(xavltree objAVLT)
{
	if ( objAVLT->Iterator ) {
		xrtFree(objAVLT->Iterator);
		objAVLT->Iterator = NULL;
	}
	if ( objAVLT->FreeProc ) {
		xrtAVLTreeUnit_FreeKeysRecuProc(objAVLT, objAVLT->RootNode);
	}
	xrtAVLTB_Unit(objAVLT);
	xrtFSMemPoolUnit(&objAVLT->objMM);
	objAVLT->NodeCache = NULL;
}


// 释放 AVL 树
XXAPI void xrtAVLTreeUnit(xavltree objAVLT)
{
	if ( !xrtOwnerBeginMutable(&objAVLT->Owner, "avltree belongs to another thread.") ) {
		return;
	}
	__xrtAVLTreeUnit_NoLock(objAVLT);
	xrtOwnerEndMutable(&objAVLT->Owner);
}

#ifdef XRT_MEM_DEBUG
// 创建 AVL 树调试
XXAPI xavltree xrtAVLTreeCreateDbg(unsigned int iItemLength, AVLTree_CompProc procComp, uint32 iMode, const char* sFile, uint32 iLine)
{
	xrtMemDebugSiteScope tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
	xavltree objAVLT = xrtAVLTreeCreate(iItemLength, procComp, iMode);
	__xrtMemDebugLeaveSiteScope(&tScope);
	__xrtMemDebugRegisterObject(objAVLT, XRT_MEMDEBUG_OBJECT_AVLTREE, XRT_MEMDEBUG_OBJECT_ORIGIN_CREATE, sFile, iLine);
	return objAVLT;
}


// 初始化 AVL 树调试
XXAPI void xrtAVLTreeInitDbg(xavltree objAVLT, unsigned int iItemLength, AVLTree_CompProc procComp, uint32 iMode, const char* sFile, uint32 iLine)
{
	xrtMemDebugSiteScope tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
	xrtAVLTreeInit(objAVLT, iItemLength, procComp, iMode);
	__xrtMemDebugLeaveSiteScope(&tScope);
	__xrtMemDebugRegisterObject(objAVLT, XRT_MEMDEBUG_OBJECT_AVLTREE, XRT_MEMDEBUG_OBJECT_ORIGIN_INIT, sFile, iLine);
}


// 销毁 AVL 树调试
XXAPI void xrtAVLTreeDestroyDbg(xavltree objAVLT, const char* sFile, uint32 iLine)
{
	if ( objAVLT ) {
		xrtMemDebugSiteScope tScope;
		if ( !__xrtMemDebugObjectGuardDestroy(objAVLT, XRT_MEMDEBUG_OBJECT_AVLTREE, sFile, iLine) ) {
			return;
		}
		if ( !xrtOwnerCheckMutable(&objAVLT->Owner, "avltree belongs to another thread.") ) {
			return;
		}
		tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
		xrtAVLTreeUnit(objAVLT);
		__xrtMemDebugLeaveSiteScope(&tScope);
		__xrtMemDebugUnregisterObject(objAVLT, XRT_MEMDEBUG_OBJECT_AVLTREE, sFile, iLine);
		xrtFreeDbg(objAVLT, sFile, iLine);
	}
}


// 释放 AVL 树调试
XXAPI void xrtAVLTreeUnitDbg(xavltree objAVLT, const char* sFile, uint32 iLine)
{
	xrtMemDebugSiteScope tScope;
	if ( objAVLT == NULL ) {
		return;
	}
	if ( !__xrtMemDebugObjectGuardDestroy(objAVLT, XRT_MEMDEBUG_OBJECT_AVLTREE, sFile, iLine) ) {
		return;
	}
	if ( !xrtOwnerBeginMutable(&objAVLT->Owner, "avltree belongs to another thread.") ) {
		return;
	}
	tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
	__xrtAVLTreeUnit_NoLock(objAVLT);
	__xrtMemDebugLeaveSiteScope(&tScope);
	xrtOwnerEndMutable(&objAVLT->Owner);
	__xrtMemDebugUnregisterObject(objAVLT, XRT_MEMDEBUG_OBJECT_AVLTREE, sFile, iLine);
}
#endif

// 锁定 AVL 树
XXAPI bool xrtAVLTreeLock(xavltree objAVLT)
{
	if ( objAVLT == NULL ) {
		return FALSE;
	}
	return xrtOwnerLock(&objAVLT->Owner, "avltree belongs to another thread.");
}


// 解锁 AVL 树
XXAPI void xrtAVLTreeUnlock(xavltree objAVLT)
{
	if ( objAVLT == NULL ) {
		return;
	}
	xrtOwnerUnlock(&objAVLT->Owner);
}

// 向 AVLTree 中插入节点，返回数据段指针（如果值已经存在，则会返回已存在的数据段指针）
XXAPI ptr xrtAVLTreeInsert(xavltree objAVLT, ptr pKey, bool* bNew)
{
	ptr pRet = NULL;
	if ( !xrtOwnerBeginMutable(&objAVLT->Owner, "avltree belongs to another thread.") ) {
		return NULL;
	}
	// 创建缓存节点 [节点添加可能失败，缓存节点会留到下次添加节点时继续使用]
	if ( objAVLT->NodeCache == NULL ) {
		objAVLT->NodeCache = xrtFSMemPoolAlloc(&objAVLT->objMM);
		if ( objAVLT->NodeCache == NULL ) {
			xrtOwnerEndMutable(&objAVLT->Owner);
			return NULL;
		}
	}
	// 添加节点
	xavltnode pNewNode = xrtAVLTB_Insert((xavltbase)objAVLT, objAVLT->CompProc, pKey, objAVLT->NodeCache);
	if ( bNew ) {
		if ( pNewNode == objAVLT->NodeCache ) {
			*bNew = 1;
		} else {
			*bNew = 0;
		}
	}
	// 如果缓存节点被使用了，则将缓存节点清空
	if ( pNewNode == objAVLT->NodeCache ) {
		objAVLT->NodeCache = NULL;
	}
	// 返回节点数据
	if ( pNewNode ) {
		pRet = &pNewNode[1];
	}
	xrtOwnerEndMutable(&objAVLT->Owner);
	return pRet;
}

// 从 AVLTree 中删除节点（成功返回 TRUE、失败返回 FALSE）
XXAPI bool xrtAVLTreeRemove(xavltree objAVLT, ptr pKey)
{
	bool bRet = FALSE;
	if ( !xrtOwnerBeginMutable(&objAVLT->Owner, "avltree belongs to another thread.") ) {
		return FALSE;
	}
	xavltnode pDelNode = xrtAVLTB_Remove((xavltbase)objAVLT, objAVLT->CompProc, pKey);
	if ( pDelNode ) {
		if ( objAVLT->FreeProc ) {
			objAVLT->FreeProc(objAVLT, &pDelNode[1]);
		}
		xrtFSMemPoolFree(&objAVLT->objMM, pDelNode);
		bRet = TRUE;
	}
	xrtOwnerEndMutable(&objAVLT->Owner);
	return bRet;
}

// 从 AVLTree 中查找节点（返回 AVLTree 节点对象）
XXAPI ptr xrtAVLTreeSearch(xavltree objAVLT, ptr pKey)
{
	ptr pRet = NULL;
	xavltree pParent = NULL;
	if ( !xrtOwnerBeginMutable(&objAVLT->Owner, "avltree belongs to another thread.") ) {
		return NULL;
	}
	xavltnode pNode = xrtAVLTB_Search((xavltbase)objAVLT, objAVLT->CompProc, pKey);
	if ( pNode ) {
		pRet = &pNode[1];
	} else {
		pParent = objAVLT->Parent;
	}
	xrtOwnerEndMutable(&objAVLT->Owner);
	if ( pRet || pParent == NULL ) {
		return pRet;
	}
	if ( !xrtOwnerBeginMutable(&pParent->Owner, "avltree belongs to another thread.") ) {
		return NULL;
	}
	pNode = xrtAVLTB_Search((xavltbase)pParent, pParent->CompProc, pKey);
	if ( pNode ) {
		pRet = &pNode[1];
	}
	xrtOwnerEndMutable(&pParent->Owner);
	return pRet;
}


// 遍历 AVL 树
XXAPI bool xrtAVLTreeWalk(xavltree objAVLT, AVLTree_EachProc procEach, ptr pArg)
{
	bool bRet = FALSE;
	if ( objAVLT == NULL ) {
		return FALSE;
	}
	if ( !xrtOwnerBeginMutable(&objAVLT->Owner, "avltree belongs to another thread.") ) {
		return FALSE;
	}
	bRet = xrtAVLTB_WalkRecuProc(objAVLT->RootNode, procEach, pArg);
	xrtOwnerEndMutable(&objAVLT->Owner);
	return bRet;
}


// 遍历 AVL 树扩展
XXAPI bool xrtAVLTreeWalkEx(xavltree objAVLT, AVLTree_EachProc procPre, AVLTree_EachProc procIn, AVLTree_EachProc procPost, ptr pArg)
{
	bool bRet = FALSE;
	if ( objAVLT == NULL ) {
		return FALSE;
	}
	if ( !xrtOwnerBeginMutable(&objAVLT->Owner, "avltree belongs to another thread.") ) {
		return FALSE;
	}
	bRet = xrtAVLTB_WalkExRecuProc(objAVLT->RootNode, procPre, procIn, procPost, pArg);
	xrtOwnerEndMutable(&objAVLT->Owner);
	return bRet;
}


// 开始 AVL 树 iter
XXAPI void xrtAVLTreeIterBegin(xavltree objAVLT)
{
	if ( objAVLT == NULL ) {
		return;
	}
	if ( !xrtOwnerBeginMutable(&objAVLT->Owner, "avltree belongs to another thread.") ) {
		return;
	}
	if ( objAVLT->Iterator && (objAVLT->Iterator->Flags & XRT_AVLITER_FLAG_HOLD_ROOT_LOCK) ) {
		xrtAVLTB_IterEnd((xavltbase)objAVLT);
		xrtOwnerEndMutable(&objAVLT->Owner);
	}
	xrtAVLTB_IterBegin((xavltbase)objAVLT);
	if ( objAVLT->Iterator ) {
		objAVLT->Iterator->Flags |= XRT_AVLITER_FLAG_HOLD_ROOT_LOCK;
	} else {
		xrtOwnerEndMutable(&objAVLT->Owner);
	}
}


// 获取下一个 AVL 树 iter
XXAPI ptr xrtAVLTreeIterNext(xavltree objAVLT)
{
	ptr pRet;
	bool bOwnsLock;
	if ( objAVLT == NULL || objAVLT->Iterator == NULL ) {
		return NULL;
	}
	bOwnsLock = (objAVLT->Iterator->Flags & XRT_AVLITER_FLAG_HOLD_ROOT_LOCK) != 0;
	pRet = xrtAVLTB_IterNext((xavltbase)objAVLT);
	if ( pRet == NULL && bOwnsLock ) {
		xrtOwnerEndMutable(&objAVLT->Owner);
	}
	return pRet;
}


// 结束 AVL 树 iter
XXAPI void xrtAVLTreeIterEnd(xavltree objAVLT)
{
	bool bOwnsLock = FALSE;
	if ( objAVLT == NULL ) {
		return;
	}
	if ( objAVLT->Iterator ) {
		bOwnsLock = (objAVLT->Iterator->Flags & XRT_AVLITER_FLAG_HOLD_ROOT_LOCK) != 0;
	}
	xrtAVLTB_IterEnd((xavltbase)objAVLT);
	if ( bOwnsLock ) {
		xrtOwnerEndMutable(&objAVLT->Owner);
	}
}


