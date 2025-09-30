/*
	Dynamic Array [动态数组]
		成员编号规则（0为不存在的成员编号）：
			┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬────┐
			│01│02│03│04│05│06│07│08│09│10│11│12│ .. │
			└──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴────┘
*/



// 创建数据块结构内存管理器
XXAPI BSMM_Object BSMM_Create(unsigned int iItemLength)
{
	BSMM_Object objBSMM = mmu_malloc(sizeof(BSMM_Struct));
	if ( objBSMM ) {
		BSMM_Init(objBSMM, iItemLength);
	}
	return objBSMM;
}

// 销毁数据块结构内存管理器
XXAPI void BSMM_Destroy(BSMM_Object objBSMM)
{
	if ( objBSMM ) {
		BSMM_Unit(objBSMM);
		mmu_free(objBSMM);
	}
}

// 初始化数据块结构内存管理器（对自维护结构体指针使用，和 BSMM_Create 功能类似）
XXAPI void BSMM_Init(BSMM_Object objBSMM, unsigned int iItemLength)
{
	objBSMM->ItemLength = iItemLength;
	objBSMM->Count = 0;
	PAMM_Init(&objBSMM->PageMMU);
	objBSMM->LL_Free = NULL;
}

// 释放数据块结构内存管理器（对自维护结构体指针使用，和 BSMM_Destroy 功能类似）
XXAPI void BSMM_Unit(BSMM_Object objBSMM)
{
	objBSMM->Count = 0;
	// 循环释放 PageMMU 中的内存页
	for ( int i = 0; i < objBSMM->PageMMU.Count; i++ ) {
		mmu_free(objBSMM->PageMMU.Memory[i]);
	}
	PAMM_Unit(&objBSMM->PageMMU);
	// 循环释放空闲内存块链表
	MemPtr_LLNode* pNode = objBSMM->LL_Free;
	while ( pNode ) {
		MemPtr_LLNode* pNext = pNode->Next;
		mmu_free(pNode);
		pNode = pNext;
	}
	objBSMM->LL_Free = NULL;
}

// 申请结构体内存
XXAPI void* BSMM_Alloc(BSMM_Object objBSMM)
{
	if ( objBSMM->LL_Free ) {
		// 有空闲内存块先用空闲的
		void* Ptr = objBSMM->LL_Free->Ptr;
		MemPtr_LLNode* pNext = objBSMM->LL_Free->Next;
		mmu_free(objBSMM->LL_Free);
		objBSMM->LL_Free = pNext;
		return Ptr;
	} else {
		// 需要申请新的内存块
		if ( objBSMM->Count >= (objBSMM->PageMMU.Count * 256) ) {
			char* pBlock = mmu_malloc(objBSMM->ItemLength * 256);
			if ( pBlock == NULL ) {
				return NULL;
			}
			int iIdx = PAMM_Append(&objBSMM->PageMMU, pBlock);
			if ( iIdx == 0 ) {
				mmu_free(pBlock);
				return NULL;
			}
		}
		// 从内存块中分配值
		unsigned int iBlock = objBSMM->Count >> 8;
		unsigned int iPos = objBSMM->Count & 0xFF;
		char* pBlock = PAMM_GetVal_Inline(&objBSMM->PageMMU, iBlock + 1);
		objBSMM->Count++;
		return &pBlock[iPos * objBSMM->ItemLength];
	}
}

// 释放结构体内存
XXAPI void BSMM_Free(BSMM_Object objBSMM, void* Ptr)
{
	MemPtr_LLNode* pNode = mmu_malloc(sizeof(MemPtr_LLNode));
	pNode->Ptr = Ptr;
	pNode->Next = objBSMM->LL_Free;
	objBSMM->LL_Free = pNode;
}


