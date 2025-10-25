


// 创建 AVLTree
XXAPI xavltree xrtAVLTreeCreate(unsigned int iItemLength, AVLTree_CompProc procComp)
{
	xavltree objAVLT = xrtMalloc(sizeof(xavltree_struct));
	if ( objAVLT ) {
		xrtAVLTreeInit(objAVLT, iItemLength, procComp);
	}
	return objAVLT;
}

// 销毁 AVLTree
XXAPI void xrtAVLTreeDestroy(xavltree objAVLT)
{
	if ( objAVLT ) {
		xrtAVLTreeUnit(objAVLT);
		xrtFree(objAVLT);
	}
}

// 初始化 AVLTree（对自维护结构体指针使用，和 AVLTree_Create 功能类似）
XXAPI void xrtAVLTreeInit(xavltree objAVLT, unsigned int iItemLength, AVLTree_CompProc procComp)
{
	xrtAVLTB_Init(objAVLT);
	objAVLT->CompProc = procComp;
	objAVLT->FreeProc = NULL;
	xrtFSMemPoolInit(&objAVLT->objMM, sizeof(xavltnode_struct) + iItemLength);
	objAVLT->NodeCache = NULL;
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
XXAPI void xrtAVLTreeUnit(xavltree objAVLT)
{
	if (objAVLT->FreeProc ) {
		xrtAVLTreeUnit_FreeKeysRecuProc(objAVLT, objAVLT->RootNode);
	}
	xrtAVLTB_Unit(objAVLT);
	xrtFSMemPoolUnit(&objAVLT->objMM);
	objAVLT->NodeCache = NULL;
}

// 向 AVLTree 中插入节点，返回数据段指针（如果值已经存在，则会返回已存在的数据段指针）
XXAPI void* xrtAVLTreeInsert(xavltree objAVLT, void* pKey, int* bNew)
{
	// 创建缓存节点 [节点添加可能失败，缓存节点会留到下次添加节点时继续使用]
	if ( objAVLT->NodeCache == NULL ) {
		objAVLT->NodeCache = xrtFSMemPoolAlloc(&objAVLT->objMM);
		if ( objAVLT->NodeCache == NULL ) {
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
		return &pNewNode[1];
	} else {
		return NULL;
	}
}

// 从 AVLTree 中删除节点（成功返回 TRUE、失败返回 FALSE）
XXAPI int xrtAVLTreeRemove(xavltree objAVLT, void* pKey)
{
	xavltnode pDelNode = xrtAVLTB_Remove((xavltbase)objAVLT, objAVLT->CompProc, pKey);
	if ( pDelNode ) {
		if ( objAVLT->FreeProc ) {
			objAVLT->FreeProc(objAVLT, &pDelNode[1]);
		}
		xrtFSMemPoolFree(&objAVLT->objMM, pDelNode);
		return -1;
	} else {
		return 0;
	}
}

// 从 AVLTree 中查找节点（返回 AVLTree 节点对象）
XXAPI void* xrtAVLTreeSearch(xavltree objAVLT, void* pKey)
{
	xavltnode pNode = xrtAVLTB_Search((xavltbase)objAVLT, objAVLT->CompProc, pKey);
	if ( pNode ) {
		return &pNode[1];
	} else {
		return NULL;
	}
}


