


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
XXAPI xlist xrtListCreate(unsigned int iItemLength)
{
	xlist objList = xrtMalloc(sizeof(xlist_struct));
	if ( objList ) {
		xrtListInit(objList, iItemLength);
	}
	return objList;
}

// 销毁列表
XXAPI void xrtListDestroy(xlist objList)
{
	if ( objList ) {
		xrtListUnit(objList);
		xrtFree(objList);
	}
}

// 初始化列表（对自维护结构体指针使用）
XXAPI void xrtListInit(xlist objList, unsigned int iItemLength)
{
	xrtAVLTreeInit(&objList->AVLT, iItemLength + sizeof(int64), (void*)List_CompProc);
	objList->Parent = NULL;
}

// 释放列表（对自维护结构体指针使用）
XXAPI void xrtListUnit(xlist objList)
{
	xrtAVLTreeUnit(&objList->AVLT);
}

// 设置值
XXAPI ptr xrtListSet(xlist objList, int64 iKey, int* bNewRet)
{
	int bNew;
	int64* pNode = xrtAVLTreeInsert(&objList->AVLT, &iKey, &bNew);
	if ( pNode == NULL ) {
		return NULL;
	}
	if ( bNewRet ) {
		*bNewRet = bNew;
	}
	if ( bNew ) {
		*pNode = iKey;
	}
	return &pNode[1];
}

// 设置值 - 当值为 void* 时直接修改指针内容
XXAPI int xrtListSetPtr(xlist objList, int64 iKey, ptr pVal, ptr* ppOldVal)
{
	int bNew;
	int64* pNode = xrtAVLTreeInsert(&objList->AVLT, &iKey, &bNew);
	if ( pNode == NULL ) {
		return 0;
	}
	if ( bNew ) {
		*pNode = iKey;
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

// 获取值
XXAPI ptr xrtListGet(xlist objList, int64 iKey)
{
	int64* pNode = xrtAVLTreeSearch(&objList->AVLT, &iKey);
	if ( pNode ) {
		return &pNode[1];
	}
	if ( objList->Parent ) {
		return xrtListGet(objList->Parent, iKey);
	} else {
		return NULL;
	}
}

// 获取值 - 当值为 void* 时直接获取指针内容
XXAPI ptr xrtListGetPtr(xlist objList, int64 iKey)
{
	int64* pNode = xrtAVLTreeSearch(&objList->AVLT, &iKey);
	if ( pNode ) {
		#if defined(__x86_64__) || defined(_M_X64)
			return (ptr)pNode[1];
		#elif defined(__i386__) || defined(_M_IX86)
			return ((ptr*)&pNode[1])[0];
		#endif
	}
	if ( objList->Parent ) {
		return xrtListGetPtr(objList->Parent, iKey);
	} else {
		return NULL;
	}
}

// 删除值
XXAPI int xrtListRemove(xlist objList, int64 iKey)
{
	return xrtAVLTreeRemove(&objList->AVLT, &iKey);
}

// 判断值是否存在
XXAPI int xrtListExists(xlist objList, int64 iKey)
{
	int64* pNode = xrtAVLTreeSearch(&objList->AVLT, &iKey);
	if ( pNode ) {
		return -1;
	}
	return 0;
}

// 获取表内元素数量
XXAPI unsigned int xrtListCount(xlist objList)
{
	return objList->AVLT.Count;
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
			if ( procEach(((int64*)&root[1])[0], ((ptr)root) + sizeof(xavltnode_struct) + sizeof(int64), pArg) ) {
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
XXAPI void xrtListWalk(xlist objList, List_EachProc procEach, void* pArg)
{
	List_WalkRecuProc(objList->AVLT.RootNode, procEach, pArg);
}


