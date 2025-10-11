


#if defined(__x86_64__) || defined(_M_X64)
	// 64 bit
	
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
	
	// 哈希值计算函数（内部函数）
	#define Dict_EvalHash(obj, k, l) obj.Key = k; obj.KeyLen = l; obj.Hash = xrtHash64(k, l)
	
#elif defined(__i386__) || defined(_M_IX86)
	// 32 bit
	
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
	
	// 哈希值计算函数（内部函数）
	#define Dict_EvalHash(obj, k, l) obj.Key = k; obj.KeyLen = l; obj.Hash = xrtHash32(k, l)
	
#endif



// 创建哈希表
XXAPI xdict xrtDictCreate(unsigned int iItemLength)
{
	xdict objHT = xrtMalloc(sizeof(xdict_struct));
	if ( objHT ) {
		xrtDictInit(objHT, iItemLength);
	}
	return objHT;
}

// 销毁哈希表
XXAPI void xrtDictDestroy(xdict objHT)
{
	if ( objHT ) {
		xrtDictUnit(objHT);
		xrtFree(objHT);
	}
}

// 初始化哈希表（对自维护结构体指针使用，和 AVLHT32_Create 功能类似）
void AVLHT32_FreeProc(xavltree objTree, Dict_Key* pNode)
{
	#ifdef MMU_USE_MP256
		if ( ((xdict)objTree->ExtData)->MP ) {
			MP256_Free(((xdict)objTree->ExtData)->MP, pNode->Key);
		} else {
			xrtFree(pNode->Key);
		}
	#else
		xrtFree(pNode->Key);
	#endif
}
XXAPI void xrtDictInit(xdict objHT, unsigned int iItemLength)
{
	xrtAVLTreeInit(&objHT->AVLT, iItemLength + sizeof(Dict_Key), (void*)Dict_CompProc);
	objHT->AVLT.FreeProc = (void*)AVLHT32_FreeProc;
	objHT->AVLT.ExtData = objHT;
	#ifdef MMU_USE_MP256
		objHT->MP = NULL;
	#endif
}

// 释放哈希表（对自维护结构体指针使用，和 AVLHT32_Destroy 功能类似）
void AVLHT32_FreeKeysRecuProc(xdict objHT, xavltnode root)
{
	if ( root ) {
		// 递归左子树
		if ( root->left != NULL ) {
			AVLHT32_FreeKeysRecuProc(objHT, root->left);
		}
		// 释放 Key
		Dict_Key* pNode = xrtAVLTreeGetNodeData(root);
		#ifdef MMU_USE_MP256
			if ( objHT->MP ) {
				MP256_Free(objHT->MP, pNode->Key);
			} else {
				xrtFree(pNode->Key);
			}
		#else
			xrtFree(pNode->Key);
		#endif
		// 递归右子树
		if ( root->right != NULL ) {
			AVLHT32_FreeKeysRecuProc(objHT, root->right);
		}
	}
}
XXAPI void xrtDictUnit(xdict objHT)
{
	AVLHT32_FreeKeysRecuProc(objHT, objHT->AVLT.RootNode);
	xrtAVLTreeUnit(&objHT->AVLT);
}

// 设置值
XXAPI void* xrtDictSet(xdict objHT, void* sKey, unsigned int iKeyLen, int* bNewRet)
{
	Dict_Key objKey;
	Dict_EvalHash(objKey, sKey, iKeyLen);
	int bNew;
	Dict_Key* pNode = xrtAVLTreeInsert(&objHT->AVLT, &objKey, &bNew);
	if ( pNode ) {
		if ( bNewRet ) {
			*bNewRet = bNew;
		}
		if ( bNew ) {
			#ifdef MMU_USE_MP256
				if ( objHT->MP ) {
					pNode->Key = MP256_Alloc(objHT->MP, iKeyLen + 1);
				} else {
					pNode->Key = xrtMalloc(iKeyLen + 1);
				}
			#else
				pNode->Key = xrtMalloc(iKeyLen + 1);
			#endif
			pNode->KeyLen = iKeyLen;
			pNode->Hash = objKey.Hash;
			memcpy(pNode->Key, sKey, iKeyLen);
			((char*)pNode->Key)[iKeyLen] = 0;
		}
		return &pNode[1];
	}
	return NULL;
}

// 设置值 - 当值为 void* 时直接修改指针内容
XXAPI int xrtDictSetPtr(xdict objHT, void* sKey, unsigned int iKeyLen, void* pVal, void** ppOldVal)
{
	Dict_Key objKey;
	Dict_EvalHash(objKey, sKey, iKeyLen);
	int bNew;
	Dict_Key* pNode = xrtAVLTreeInsert(&objHT->AVLT, &objKey, &bNew);
	if ( pNode ) {
		if ( bNew ) {
			#ifdef MMU_USE_MP256
				if ( objHT->MP ) {
					pNode->Key = MP256_Alloc(objHT->MP, iKeyLen + 1);
				} else {
					pNode->Key = xrtMalloc(iKeyLen + 1);
				}
			#else
				pNode->Key = xrtMalloc(iKeyLen + 1);
			#endif
			pNode->KeyLen = iKeyLen;
			pNode->Hash = objKey.Hash;
			memcpy(pNode->Key, sKey, iKeyLen);
			((char*)pNode->Key)[iKeyLen] = 0;
		}
		// 获取单指针数据结构
		struct {
			void* val;
		} *pData = (void*)&pNode[1];
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
		return 1;
	}
	return 0;
}

// 获取值
XXAPI void* xrtDictGet(xdict objHT, void* sKey, unsigned int iKeyLen)
{
	Dict_Key objKey;
	Dict_EvalHash(objKey, sKey, iKeyLen);
	Dict_Key* pNode = xrtAVLTreeSearch(&objHT->AVLT, &objKey);
	if ( pNode ) {
		return &pNode[1];
	}
	return NULL;
}

// 获取值 - 当值为 void* 时直接获取指针内容
XXAPI void* xrtDictGetPtr(xdict objHT, void* sKey, unsigned int iKeyLen)
{
	Dict_Key objKey;
	Dict_EvalHash(objKey, sKey, iKeyLen);
	Dict_Key* pNode = xrtAVLTreeSearch(&objHT->AVLT, &objKey);
	if ( pNode ) {
		struct {
			void* val;
		} *pData = (void*)&pNode[1];
		return pData->val;
	}
	return NULL;
}

// 删除值
XXAPI int xrtDictRemove(xdict objHT, void* sKey, unsigned int iKeyLen)
{
	Dict_Key objKey;
	Dict_EvalHash(objKey, sKey, iKeyLen);
	return xrtAVLTreeRemove(&objHT->AVLT, &objKey);
}

// 判断值是否存在
XXAPI int xrtDictExists(xdict objHT, void* sKey, unsigned int iKeyLen)
{
	Dict_Key objKey;
	Dict_EvalHash(objKey, sKey, iKeyLen);
	Dict_Key* pNode = xrtAVLTreeSearch(&objHT->AVLT, &objKey);
	if ( pNode ) {
		return -1;
	}
	return 0;
}

// 获取表内元素数量
XXAPI unsigned int xrtDictCount(xdict objHT)
{
	return objHT->AVLT.Count;
}

// 遍历表元素
int AVLHT32_WalkRecuProc(xavltnode root, Dict_EachProc procEach, void* pArg)
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
			if ( procEach(xrtAVLTreeGetNodeData(root), ((void*)root) + sizeof(xavltnode_struct) + sizeof(Dict_Key), pArg) ) {
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
XXAPI void xrtDictWalk(xdict objHT, Dict_EachProc procEach, void* pArg)
{
	AVLHT32_WalkRecuProc(objHT->AVLT.RootNode, procEach, pArg);
}


