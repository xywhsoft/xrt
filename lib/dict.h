


// 哈希值计算函数（内部函数）
#if defined(__x86_64__) || defined(_M_X64)
	#define Dict_EvalHash(obj, k, l) obj.Key = k; obj.KeyLen = l; obj.Hash = xrtHash64(k, l)
#elif defined(__i386__) || defined(_M_IX86)
	#define Dict_EvalHash(obj, k, l) obj.Key = k; obj.KeyLen = l; obj.Hash = xrtHash32(k, l)
#else
	#define Dict_EvalHash(obj, k, l) obj.Key = k; obj.KeyLen = l; obj.Hash = (sizeof(void*) >= 8 ? xrtHash64(k, l) : (uint64)xrtHash32(k, l))
#endif

// 哈希表比较函数（内部函数）
int Dict_CompProc(Dict_Key* pNode, Dict_Key* pObjKey)
{
	if ( pNode->Hash == pObjKey->Hash ) {
		if ( pNode->KeyLen == pObjKey->KeyLen ) {
			return memcmp(pNode->Key, pObjKey->Key, pObjKey->KeyLen);
		} else {
			return pNode->KeyLen - pObjKey->KeyLen;
		}
	} else if ( pNode->Hash > pObjKey->Hash ) {
		return 1;
	} else {
		return -1;
	}
}



// 创建哈希表
XXAPI xdict xrtDictCreate(uint32 iItemLength, uint32 iMode)
{
	xdict objHT = xrtMalloc(sizeof(xdict_struct));
	if ( objHT ) {
		xrtDictInit(objHT, iItemLength, iMode);
	}
	return objHT;
}

// 销毁哈希表
XXAPI void xrtDictDestroy(xdict objHT)
{
	if ( objHT ) {
		if ( !xrtOwnerCheckMutable(&objHT->Owner, "dict belongs to another thread.") ) {
			return;
		}
		xrtDictUnit(objHT);
		xrtFree(objHT);
	}
}

// 释放哈希键相关资源
void AVLHT32_FreeProc(xdict objTree, Dict_Key* pNode)
{
	if ( objTree->MP ) {
		xrtMemPoolFree(objTree->MP, pNode->Key);
	} else {
		xrtFree(pNode->Key);
	}
}


// 初始化字典
XXAPI void xrtDictInit(xdict objHT, uint32 iItemLength, uint32 iMode)
{
	xrtOwnerInitMode(&objHT->Owner, iMode);
	xrtAVLTreeInit(&objHT->AVLT, iItemLength + sizeof(Dict_Key), (ptr)Dict_CompProc, iMode);
	if ( iMode == XRT_OBJMODE_SHARED ) {
		xrtOwnerActivateShared(&objHT->Owner);
		xrtOwnerActivateShared(&objHT->AVLT.Owner);
	}
	objHT->AVLT.FreeProc = (ptr)AVLHT32_FreeProc;
	objHT->MP = NULL;
}

// 释放哈希表（对自维护结构体指针使用，和 AVLHT32_Destroy 功能类似）
static inline void __xrtDictUnit_NoLock(xdict objHT)
{
	xrtAVLTreeUnit(&objHT->AVLT);
}


// 释放字典
XXAPI void xrtDictUnit(xdict objHT)
{
	if ( !xrtOwnerBeginMutable(&objHT->Owner, "dict belongs to another thread.") ) {
		return;
	}
	__xrtDictUnit_NoLock(objHT);
	xrtOwnerEndMutable(&objHT->Owner);
}

#ifdef XRT_MEM_DEBUG
// 创建字典调试
XXAPI xdict xrtDictCreateDbg(uint32 iItemLength, uint32 iMode, const char* sFile, uint32 iLine)
{
	xrtMemDebugSiteScope tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
	xdict objHT = xrtDictCreate(iItemLength, iMode);
	__xrtMemDebugLeaveSiteScope(&tScope);
	__xrtMemDebugRegisterObject(objHT, XRT_MEMDEBUG_OBJECT_DICT, XRT_MEMDEBUG_OBJECT_ORIGIN_CREATE, sFile, iLine);
	return objHT;
}


// 初始化字典调试
XXAPI void xrtDictInitDbg(xdict objHT, uint32 iItemLength, uint32 iMode, const char* sFile, uint32 iLine)
{
	xrtMemDebugSiteScope tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
	xrtDictInit(objHT, iItemLength, iMode);
	__xrtMemDebugLeaveSiteScope(&tScope);
	__xrtMemDebugRegisterObject(objHT, XRT_MEMDEBUG_OBJECT_DICT, XRT_MEMDEBUG_OBJECT_ORIGIN_INIT, sFile, iLine);
}


// 销毁字典调试
XXAPI void xrtDictDestroyDbg(xdict objHT, const char* sFile, uint32 iLine)
{
	if ( objHT ) {
		xrtMemDebugSiteScope tScope;
		if ( !__xrtMemDebugObjectGuardDestroy(objHT, XRT_MEMDEBUG_OBJECT_DICT, sFile, iLine) ) {
			return;
		}
		if ( !xrtOwnerCheckMutable(&objHT->Owner, "dict belongs to another thread.") ) {
			return;
		}
		tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
		xrtDictUnit(objHT);
		__xrtMemDebugLeaveSiteScope(&tScope);
		__xrtMemDebugUnregisterObject(objHT, XRT_MEMDEBUG_OBJECT_DICT, sFile, iLine);
		xrtFreeDbg(objHT, sFile, iLine);
	}
}


// 释放字典调试
XXAPI void xrtDictUnitDbg(xdict objHT, const char* sFile, uint32 iLine)
{
	xrtMemDebugSiteScope tScope;
	if ( objHT == NULL ) {
		return;
	}
	if ( !__xrtMemDebugObjectGuardDestroy(objHT, XRT_MEMDEBUG_OBJECT_DICT, sFile, iLine) ) {
		return;
	}
	if ( !xrtOwnerBeginMutable(&objHT->Owner, "dict belongs to another thread.") ) {
		return;
	}
	tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
	__xrtDictUnit_NoLock(objHT);
	__xrtMemDebugLeaveSiteScope(&tScope);
	xrtOwnerEndMutable(&objHT->Owner);
	__xrtMemDebugUnregisterObject(objHT, XRT_MEMDEBUG_OBJECT_DICT, sFile, iLine);
}


// 璁剧疆鍊艰皟璇?
XXAPI ptr xrtDictSetDbg(xdict objHT, ptr sKey, uint32 iKeyLen, bool* bNewRet, const char* sFile, uint32 iLine)
{
	xrtMemDebugSiteScope tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
	ptr pRet = xrtDictSet(objHT, sKey, iKeyLen, bNewRet);
	__xrtMemDebugLeaveSiteScope(&tScope);
	return pRet;
}


// 璁剧疆鎸囬拡鍊艰皟璇?
XXAPI bool xrtDictSetPtrDbg(xdict objHT, ptr sKey, uint32 iKeyLen, ptr pVal, ptr* ppOldVal, const char* sFile, uint32 iLine)
{
	xrtMemDebugSiteScope tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
	bool bRet = xrtDictSetPtr(objHT, sKey, iKeyLen, pVal, ppOldVal);
	__xrtMemDebugLeaveSiteScope(&tScope);
	return bRet;
}


// 鍒犻櫎鍊艰皟璇?
XXAPI bool xrtDictRemoveDbg(xdict objHT, ptr sKey, uint32 iKeyLen, const char* sFile, uint32 iLine)
{
	xrtMemDebugSiteScope tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
	bool bRet = xrtDictRemove(objHT, sKey, iKeyLen);
	__xrtMemDebugLeaveSiteScope(&tScope);
	return bRet;
}


// 鍒犻櫎鎸囬拡鍊艰皟璇?
XXAPI ptr xrtDictRemovePtrDbg(xdict objHT, ptr sKey, uint32 iKeyLen, const char* sFile, uint32 iLine)
{
	xrtMemDebugSiteScope tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
	ptr pRet = xrtDictRemovePtr(objHT, sKey, iKeyLen);
	__xrtMemDebugLeaveSiteScope(&tScope);
	return pRet;
}
#endif

// 锁定字典
XXAPI bool xrtDictLock(xdict objHT)
{
	if ( objHT == NULL ) {
		return FALSE;
	}
	return xrtOwnerLock(&objHT->Owner, "dict belongs to another thread.");
}


// 解锁字典
XXAPI void xrtDictUnlock(xdict objHT)
{
	if ( objHT == NULL ) {
		return;
	}
	xrtOwnerUnlock(&objHT->Owner);
}

// 设置值
XXAPI ptr xrtDictSet(xdict objHT, ptr sKey, uint32 iKeyLen, bool* bNewRet)
{
	ptr pRet;
	Dict_Key objKey;
	Dict_EvalHash(objKey, sKey, iKeyLen);
	if ( !xrtOwnerBeginMutable(&objHT->Owner, "dict belongs to another thread.") ) {
		return NULL;
	}
	pRet = xrtDictSetWithKey(objHT, &objKey, bNewRet);
	xrtOwnerEndMutable(&objHT->Owner);
	return pRet;
}

// 设置值 - 当值为 ptr 时直接修改指针内容
XXAPI bool xrtDictSetPtr(xdict objHT, ptr sKey, uint32 iKeyLen, ptr pVal, ptr* ppOldVal)
{
	if ( !xrtOwnerBeginMutable(&objHT->Owner, "dict belongs to another thread.") ) {
		if ( ppOldVal ) {
			*ppOldVal = NULL;
		}
		return FALSE;
	}
	Dict_Key objKey;
	Dict_EvalHash(objKey, sKey, iKeyLen);
	bool bNew;
	ptr* ppVal = xrtDictSetWithKey(objHT, &objKey, &bNew);
	if ( ppVal ) {
		// 传回旧值
		if ( ppOldVal ) {
			if ( bNew ) {
				*ppOldVal = NULL;
			} else {
				*ppOldVal = ppVal[0];
			}
		}
		// 修改为新值
		ppVal[0] = pVal;
		xrtOwnerEndMutable(&objHT->Owner);
		return TRUE;
	} else {
		if ( ppOldVal ) {
			*ppOldVal = NULL;
		}
		xrtOwnerEndMutable(&objHT->Owner);
		return FALSE;
	}
}

// 获取值
XXAPI ptr xrtDictGet(xdict objHT, ptr sKey, uint32 iKeyLen)
{
	ptr pRet;
	Dict_Key objKey;
	Dict_EvalHash(objKey, sKey, iKeyLen);
	if ( !xrtOwnerBeginMutable(&objHT->Owner, "dict belongs to another thread.") ) {
		return NULL;
	}
	pRet = xrtDictGetWithKey(objHT, &objKey);
	xrtOwnerEndMutable(&objHT->Owner);
	return pRet;
}

// 获取值 - 当值为 ptr 时直接获取指针内容
XXAPI ptr xrtDictGetPtr(xdict objHT, ptr sKey, uint32 iKeyLen)
{
	ptr pRet = NULL;
	Dict_Key objKey;
	Dict_EvalHash(objKey, sKey, iKeyLen);
	if ( !xrtOwnerBeginMutable(&objHT->Owner, "dict belongs to another thread.") ) {
		return NULL;
	}
	struct {
		ptr val;
	} *pData = xrtDictGetWithKey(objHT, &objKey);
	if ( pData ) {
		pRet = pData->val;
	}
	xrtOwnerEndMutable(&objHT->Owner);
	return pRet;
}

// 删除值
XXAPI bool xrtDictRemove(xdict objHT, ptr sKey, uint32 iKeyLen)
{
	bool bRet;
	if ( !xrtOwnerBeginMutable(&objHT->Owner, "dict belongs to another thread.") ) {
		return FALSE;
	}
	Dict_Key objKey;
	Dict_EvalHash(objKey, sKey, iKeyLen);
	bRet = xrtAVLTreeRemove(&objHT->AVLT, &objKey);
	xrtOwnerEndMutable(&objHT->Owner);
	return bRet;
}

// 删除值，当值为 ptr 时返回 ptr
XXAPI ptr xrtDictRemovePtr(xdict objHT, ptr sKey, uint32 iKeyLen)
{
	ptr result = NULL;
	if ( !xrtOwnerBeginMutable(&objHT->Owner, "dict belongs to another thread.") ) {
		return NULL;
	}
	Dict_Key objKey;
	Dict_EvalHash(objKey, sKey, iKeyLen);
	xavltnode pDelNode = xrtAVLTB_Remove((xavltbase)&objHT->AVLT, objHT->AVLT.CompProc, &objKey);
	if ( pDelNode ) {
		Dict_Key* pKeyPtr = xrtAVLTreeGetNodeData(pDelNode);
		ptr* pData = (ptr*)&pKeyPtr[1];
		result = pData[0];  // 先保存返回值
		if ( objHT->AVLT.FreeProc ) {
			objHT->AVLT.FreeProc(&objHT->AVLT, &pDelNode[1]);
		}
		xrtFSMemPoolFree(&objHT->AVLT.objMM, pDelNode);
	}
	xrtOwnerEndMutable(&objHT->Owner);
	return result;  // 返回保存的值
}

// 判断值是否存在
XXAPI bool xrtDictExists(xdict objHT, ptr sKey, uint32 iKeyLen)
{
	bool bRet = FALSE;
	Dict_Key objKey;
	Dict_EvalHash(objKey, sKey, iKeyLen);
	if ( !xrtOwnerBeginMutable(&objHT->Owner, "dict belongs to another thread.") ) {
		return FALSE;
	}
	Dict_Key* pNode = xrtAVLTreeSearch(&objHT->AVLT, &objKey);
	if ( pNode ) {
		bRet = TRUE;
	}
	xrtOwnerEndMutable(&objHT->Owner);
	return bRet;
}

// 获取表内元素数量
XXAPI uint32 xrtDictCount(xdict objHT)
{
	uint32 iCount = 0;
	if ( !xrtOwnerBeginMutable(&objHT->Owner, "dict belongs to another thread.") ) {
		return 0;
	}
	iCount = objHT->AVLT.Count;
	xrtOwnerEndMutable(&objHT->Owner);
	return iCount;
}

// 遍历表元素
int AVLHT32_WalkRecuProc(xavltnode root, Dict_EachProc procEach, ptr pArg)
{
	if ( root ) {
		// 递归左子树
		if ( root->left != NULL ) {
			if ( AVLHT32_WalkRecuProc(root->left, procEach, pArg) ) {
				return -1;
			}
		}
		// 调用回调函数
		if ( procEach ) {
			if ( procEach(xrtAVLTreeGetNodeData(root), (ptr)((uint8*)root + sizeof(xavltnode_struct) + sizeof(Dict_Key)), pArg) ) {
				return -1;
			}
		}
		// 递归右子树
		if ( root->right != NULL ) {
			if ( AVLHT32_WalkRecuProc(root->right, procEach, pArg) ) {
				return -1;
			}
		}
	}
	return 0;
}


// 遍历字典
XXAPI void xrtDictWalk(xdict objHT, Dict_EachProc procEach, ptr pArg)
{
	if ( !xrtOwnerBeginMutable(&objHT->Owner, "dict belongs to another thread.") ) {
		return;
	}
	AVLHT32_WalkRecuProc(objHT->AVLT.RootNode, procEach, pArg);
	xrtOwnerEndMutable(&objHT->Owner);
}


