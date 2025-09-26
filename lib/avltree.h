


// 创建 AVLTree
XXAPI AVLTree_Object AVLTree_Create(unsigned int iItemLength, AVLTree_CompProc procComp)
{
	AVLTree_Object objAVLT = mmu_malloc(sizeof(AVLTree_Struct));
	if ( objAVLT ) {
		AVLTree_Init(objAVLT, iItemLength, procComp);
	}
	return objAVLT;
}

// 销毁 AVLTree
XXAPI void AVLTree_Destroy(AVLTree_Object objAVLT)
{
	if ( objAVLT ) {
		AVLTree_Unit(objAVLT);
		mmu_free(objAVLT);
	}
}

// 初始化 AVLTree（对自维护结构体指针使用，和 AVLTree_Create 功能类似）
XXAPI void AVLTree_Init(AVLTree_Object objAVLT, unsigned int iItemLength, AVLTree_CompProc procComp)
{
	AVLTB_Init(objAVLT);
	objAVLT->CompProc = procComp;
	objAVLT->FreeProc = NULL;
	MM256_Init(&objAVLT->objMM, sizeof(AVLTree_NodeBase) + iItemLength);
	objAVLT->ExtData = NULL;
	objAVLT->NodeCache = NULL;
}

// 释放 AVLTree（对自维护结构体指针使用，和 AVLTree_Destroy 功能类似）
XXAPI void AVLTree_Unit(AVLTree_Object objAVLT)
{
	AVLTB_Unit(objAVLT);
	if (objAVLT->FreeProc ) {
		
	}
	MM256_Unit(&objAVLT->objMM);
	objAVLT->ExtData = NULL;
	objAVLT->NodeCache = NULL;
}

// 向 AVLTree 中插入节点，返回数据段指针（如果值已经存在，则会返回已存在的数据段指针）
XXAPI void* AVLTree_Insert(AVLTree_Object objAVLT, void* pKey, int* bNew)
{
	// 创建缓存节点 [节点添加可能失败，缓存节点会留到下次添加节点时继续使用]
	if ( objAVLT->NodeCache == NULL ) {
		objAVLT->NodeCache = MM256_Alloc(&objAVLT->objMM);
		if ( objAVLT->NodeCache == NULL ) {
			return NULL;
		}
	}
	// 添加节点
	AVLTree_NodeBase* pNewNode = AVLTB_Insert((AVLTree_BaseObject)objAVLT, objAVLT->CompProc, pKey, objAVLT->NodeCache);
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
XXAPI int AVLTree_Remove(AVLTree_Object objAVLT, void* pKey)
{
	AVLTree_NodeBase* pDelNode = AVLTB_Remove((AVLTree_BaseObject)objAVLT, objAVLT->CompProc, pKey);
	if ( pDelNode ) {
		if ( objAVLT->FreeProc ) {
			objAVLT->FreeProc(objAVLT, &pDelNode[1]);
		}
		MM256_Free(&objAVLT->objMM, pDelNode);
		return -1;
	} else {
		return 0;
	}
}

// 从 AVLTree 中查找节点（返回 AVLTree 节点对象）
XXAPI void* AVLTree_Search(AVLTree_Object objAVLT, void* pKey)
{
	AVLTree_NodeBase* pNode = AVLTB_Search((AVLTree_BaseObject)objAVLT, objAVLT->CompProc, pKey);
	if ( pNode ) {
		return &pNode[1];
	} else {
		return NULL;
	}
}


