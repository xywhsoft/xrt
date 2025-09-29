


// 创建内存管理器
XXAPI FSMP_Object xrtFixelSizeMemPoolCreate(unsigned int iItemLength)
{
	FSMP_Object objMP = xrtMalloc(sizeof(FSMP_Struct));
	if ( objMP == NULL ) {
		return NULL;
	}
	xrtFixelSizeMemPoolInit(objMP, iItemLength);
	return objMP;
}

// 销毁内存管理器
XXAPI void xrtFixelSizeMemPoolDestroy(FSMP_Object objMP)
{
	if ( objMP ) {
		xrtFixelSizeMemPoolUnit(objMP);
		xrtFree(objMP);
	}
}

// 初始化内存管理器（对自维护结构体指针使用）
XXAPI void xrtFixelSizeMemPoolInit(FSMP_Object objMP, unsigned int iItemLength)
{
	objMP->ItemLength = iItemLength;
	objMP->LL_Idle = NULL;
	objMP->LL_Full = NULL;
	objMP->LL_Null = NULL;
	objMP->LL_Free = NULL;
}

// 释放内存管理器（对自维护结构体指针使用）
XXAPI void xrtFixelSizeMemPoolUnit(FSMP_Object objMP)
{
	// 释放有空闲的内存管理单元
	while ( objMP->LL_Idle ) {
		MMU_Object pNext = objMP->LL_Idle->Next;
		xrtMemUnitDestroy(objMP->LL_Idle);
		objMP->LL_Idle = pNext;
	}
	// 释放满载内存链表
	while ( objMP->LL_Full ) {
		MMU_Object pNext = objMP->LL_Full->Next;
		xrtMemUnitDestroy(objMP->LL_Full);
		objMP->LL_Full = pNext;
	}
	// 释放全空内存链表
	if ( objMP->LL_Null ) {
		xrtMemUnitDestroy(objMP->LL_Null);
		objMP->LL_Null = NULL;
	}
	// 释放已释放的内存链表
	while ( objMP->LL_Free ) {
		MP_FlagLL_Node* pNext = objMP->LL_Free->Next;
		xrtFree(objMP->LL_Free);
		objMP->LL_Free = pNext;
	}
}

// 从内存管理器中申请一块内存
XXAPI void* xrtFixelSizeMemPoolAlloc(FSMP_Object objMP)
{
	MMU_Object objMMU = NULL;
	if ( objMP->LL_Idle == NULL ) {
		// 如果没有空闲的内存管理单元，优先使用备用的全空单元，或创建一个新的单元
		if ( objMP->LL_Null ) {
			// 使用备用的全空内存管理单元
			objMMU = objMP->LL_Null;
			objMP->LL_Idle = objMMU;
			objMP->LL_Null = NULL;
		} else if ( objMP->LL_Free ) {
			// 创建新的内存管理单元，使用已释放的内存管理单元位置
			objMMU = xrtMemUnitCreate(objMP->ItemLength);
			if ( objMMU == NULL ) {
				xrtSetError("memory unit create failed.", FALSE);
				return NULL;
			}
			// 恢复Flag，写入新申请的单元
			objMMU->Flag = objMP->LL_Free->Flag;
			// 从 LL_Free 中移除
			objMP->LL_Free = objMP->LL_Free->Next;
			// 添加到 LL_Idle
			objMMU->Prev = NULL;
			objMMU->Next = NULL;
			objMP->LL_Idle = objMMU;
		} else {
			// 创建新的内存管理单元，创建失败就报错处理
			objMMU = xrtMemUnitCreate(objMP->ItemLength);
			if ( objMMU == NULL ) {
				xrtSetError("memory unit create failed.", FALSE);
				return NULL;
			}
			// 添加到 LL_Idle
			objMMU->Flag = MMU_FLAG_USE | (objMP->NextMMUID << 8);
			objMMU->Prev = NULL;
			objMMU->Next = NULL;
			objMP->LL_Idle = objMMU;
			objMP->NextMMUID++;
		}
	} else {
		// 有空闲的内存管理单元，优先使用空闲的
		objMMU = objMP->LL_Idle;
		// 如果空闲的内存管理单元满了，将它转移到满载单元链表
		if ( objMMU->Count >= 255 ) {
			// 从 LL_Idle 中移除
			objMP->LL_Idle = objMMU->Next;
			if ( objMP->LL_Idle ) {
				objMP->LL_Idle->Prev = NULL;
			}
			// 添加到 LL_Full
			objMMU->Prev = NULL;
			if ( objMP->LL_Full ) {
				objMP->LL_Full->Prev = objMMU;
			}
			objMMU->Next = objMP->LL_Full;
			objMP->LL_Full = objMMU;
		}
	}
	// 从选定内存管理器单元中申请内存块
	return xrtMemUnitAlloc(objMMU);
}

// 将内存管理器申请的内存释放掉
static inline void MM256_LLNode_ClearCheck(FSMP_Object objMP, MMU256_LLNode* pNode, int bLL_Full)
{
	// 如果这个内存管理单元已经清空
	if ( pNode->objMMU->Count == 0 ) {
		if ( objMP->LL_Null ) {
			// 有备用单元时，直接释放掉这个单元
			MMU256_Destroy(pNode->objMMU);
			pNode->objMMU = NULL;
			// 从 LL_Idle 或 LL_Full 中移除
			if ( pNode->Prev ) {
				pNode->Prev->Next = pNode->Next;
			} else {
				if ( bLL_Full ) {
					objMP->LL_Full = pNode->Next;
				} else {
					objMP->LL_Idle = pNode->Next;
				}
			}
			if ( pNode->Next ) {
				pNode->Next->Prev = pNode->Prev;
			}
			// 添加到 LL_Free
			pNode->Prev = NULL;
			pNode->Next = objMP->LL_Free;
			if ( objMP->LL_Free ) {
				objMP->LL_Free->Prev = pNode;
			}
			objMP->LL_Free = pNode;
		} else {
			// 没有备用单元时，让这个单元备用，避免临界状态反复申请和释放内存管理单元，造成性能损失
			// 从 LL_Idle 或 LL_Full 中移除
			if ( pNode->Prev ) {
				pNode->Prev->Next = pNode->Next;
			} else {
				if ( bLL_Full ) {
					objMP->LL_Full = pNode->Next;
				} else {
					objMP->LL_Idle = pNode->Next;
				}
			}
			if ( pNode->Next ) {
				pNode->Next->Prev = pNode->Prev;
			}
			// 添加到 LL_Null
			objMP->LL_Null = pNode;
			pNode->Prev = NULL;
			pNode->Next = NULL;
		}
	}
}
static inline void MM256_LLNode_IdleCheck(FSMP_Object objMM, MMU256_LLNode* pNode)
{
	if ( pNode->objMMU->Count < 256 ) {
		// 从 LL_Full 中移除
		if ( pNode->Prev ) {
			pNode->Prev->Next = pNode->Next;
		} else {
			objMM->LL_Full = pNode->Next;
		}
		if ( pNode->Next ) {
			pNode->Next->Prev = pNode->Prev;
		}
		// 添加到 LL_Idle
		pNode->Prev = NULL;
		pNode->Next = objMM->LL_Idle;
		if ( objMM->LL_Idle ) {
			objMM->LL_Idle->Prev = pNode;
		}
		objMM->LL_Idle = pNode;
	}
}
XXAPI void xrtFixelSizeMemPoolFree(FSMP_Object objMM, void* ptr)
{
	MMU_ValuePtr v = ptr - sizeof(MMU_Value);
	if ( v->ItemFlag & MMU_FLAG_USE ) {
		int iMMU = (v->ItemFlag & MMU_FLAG_MASK) >> 8;
		unsigned char idx = v->ItemFlag & 0xFF;
		// 获取对应的内存管理器单元链表结构
		MMU256_LLNode* pNode = BSMM_GetPtr_Inline(&objMM->arrMMU, iMMU);
		if ( (pNode->objMMU == NULL) && objMM->OnError ) {
			objMM->OnError(objMM, MM_ERROR_NULLMMU);
			return;
		}
		// 调用对应 MMU 的释放函数
		MMU256_FreeIdx_Inline(pNode->objMMU, idx);
		v->ItemFlag = 0;
		// 如果是一个满载的内存管理器单元，将它放入空闲单元列表
		if ( pNode->objMMU->Count >= 255 ) {
			MM256_LLNode_IdleCheck(objMM, pNode);
		}
		// 如果这个内存管理单元已经清空，将他释放或变为备用单元
		MM256_LLNode_ClearCheck(objMM, pNode, 0);
	}
}

// 进行一轮GC，将未标记为使用中的内存全部回收
XXAPI void xrtFixelSizeMemPoolGC(FSMP_Object objMM, int bFreeMark)
{
	// 遍历所有 空闲的 和 满载的 内存管理单元，进行标记回收
	MMU256_LLNode* pNode = objMM->LL_Idle;
	while ( pNode ) {
		MMU256_GC(pNode->objMMU, bFreeMark);
		pNode = pNode->Next;
	}
	pNode = objMM->LL_Full;
	while ( pNode ) {
		MMU256_GC(pNode->objMMU, bFreeMark);
		pNode = pNode->Next;
	}
	// 再次遍历所有 空闲的 和 满载的 内存管理单元，将他们归类到正确的分组
	pNode = objMM->LL_Idle;
	while ( pNode ) {
		MMU256_LLNode* pNext = pNode->Next;
		MM256_LLNode_ClearCheck(objMM, pNode, 0);
		pNode = pNext;
	}
	pNode = objMM->LL_Full;
	while ( pNode ) {
		MMU256_LLNode* pNext = pNode->Next;
		if ( pNode->objMMU->Count == 0 ) {
			MM256_LLNode_ClearCheck(objMM, pNode, -1);
		} else {
			MM256_LLNode_IdleCheck(objMM, pNode);
		}
		pNode = pNext;
	}
}


