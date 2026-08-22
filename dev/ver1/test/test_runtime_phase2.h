#ifndef XRT_TEST_RUNTIME_PHASE2_H
#define XRT_TEST_RUNTIME_PHASE2_H

typedef struct {
	int Val;
} __test_phase2_tree_item;

typedef struct {
	xbsmm BSMM;
	xmemunit MemUnit;
	xfsmempool FSMemPool;
	xmempool MemPool;
	xavltree Tree;
	xdict Dict;
	xlist List;
	volatile int DeniedCount;
	volatile int ErrorCount;
} __test_phase2_owner_data;

typedef struct {
	xparray PtrArray;
	xarray StructArray;
	volatile int DeniedCount;
	volatile int ErrorCount;
} __test_phase2_array_data;

typedef struct {
	int Val;
} __test_phase2_array_item;

typedef struct {
	xdict Dict;
	xlist List;
	xparray PtrArray;
	xarray StructArray;
	xvalue ValueArray;
	xvalue ValueTable;
	volatile int SharedOkCount;
	volatile int DeniedCount;
	volatile int PendingCount;
} __test_phase2_shared_data;

typedef struct {
	xbsmm BSMM;
	xmemunit MemUnit;
	xfsmempool FSMemPool;
	xmempool MemPool;
	volatile int FailCount;
} __test_phase2_shared_alloc_data;

typedef struct {
	xavltree Tree;
	volatile int SharedOkCount;
	volatile int FailCount;
} __test_phase2_shared_tree_data;

typedef struct {
	xdict Dict;
	xlist List;
	xparray PtrArray;
	xarray StructArray;
	xavltree Tree;
} __test_phase2_shared_lock_data;

typedef struct {
	xdict Dict;
	volatile int bEntered;
} __test_phase2_shared_lock_worker_data;

typedef struct {
	xdict Dict;
	xlist List;
	xparray PtrArray;
	xarray StructArray;
	volatile int FailCount;
} __test_phase2_shared_container_data;

typedef struct {
	__test_phase2_shared_container_data* pData;
	int Base;
} __test_phase2_shared_container_worker_arg;

typedef struct {
	xvalue SharedArray;
	xvalue SharedTable;
	xvalue SharedChild;
	xvalue SharedNestedList;
	xvalue SharedColl;
	volatile int FailCount;
} __test_phase2_shared_xvalue_data;

typedef struct {
	xvalue Coll;
	volatile int FailCount;
} __test_phase2_shared_coll_data;


// 内部函数：__test_phase2_has_owner_error
static bool __test_phase2_has_owner_error(void)
{
	str sError = xrtGetError();
	return sError && strstr(sError, "another thread") != NULL;
}


// 内部函数：__test_phase2_has_no_error
static bool __test_phase2_has_no_error(void)
{
	str sError = xrtGetError();
	return sError == NULL || sError == xCore.sNull || sError[0] == '\0';
}


// 内部函数：__test_phase2_has_shared_pending_error
static bool __test_phase2_has_shared_pending_error(void)
{
	str sError = xrtGetError();
	return sError && strstr(sError, "shared mode synchronization is not implemented yet.") != NULL;
}


// 内部函数：__test_phase2_has_shared_value_error
static bool __test_phase2_has_shared_value_error(void)
{
	str sError = xrtGetError();
	return sError && strstr(sError, "real shared") != NULL;
}


// 内部函数：__test_phase2_tree_comp
static int __test_phase2_tree_comp(const __test_phase2_tree_item* pNode, ptr pKey)
{
	int iKey = (int)(intptr_t)pKey;
	if ( pNode->Val == iKey ) {
		return 0;
	} else if ( pNode->Val > iKey ) {
		return -1;
	} else {
		return 1;
	}
}


// 内部函数：__test_phase2_dict_walk_count
static bool __test_phase2_dict_walk_count(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	int* pCount = (int*)pArg;
	(void)pVal;
	if ( pKey ) {
		(*pCount)++;
	}
	return FALSE;
}


// 内部函数：__test_phase2_list_walk_count
static bool __test_phase2_list_walk_count(int64 iKey, ptr pVal, ptr pArg)
{
	int* pCount = (int*)pArg;
	(void)iKey;
	(void)pVal;
	(*pCount)++;
	return FALSE;
}


// 内部函数：__test_phase2_tree_walk_count
static bool __test_phase2_tree_walk_count(__test_phase2_tree_item* pNode, ptr pArg)
{
	int* pCount = (int*)pArg;
	if ( pNode ) {
		(*pCount)++;
	}
	return FALSE;
}


// 内部函数：__test_phase2_owner_worker
static uint32 __test_phase2_owner_worker(ptr pParam)
{
	__test_phase2_owner_data* pData = (__test_phase2_owner_data*)pParam;
	ptr pValue = NULL;

	xrtClearError();
	pValue = xrtBsmmAlloc(pData->BSMM);
	if ( pValue == NULL ) {
		pData->DeniedCount++;
		if ( __test_phase2_has_owner_error() ) {
			pData->ErrorCount++;
		}
	}

	xrtClearError();
	pValue = xrtMemUnitAlloc(pData->MemUnit);
	if ( pValue == NULL ) {
		pData->DeniedCount++;
		if ( __test_phase2_has_owner_error() ) {
			pData->ErrorCount++;
		}
	}

	xrtClearError();
	pValue = xrtFSMemPoolAlloc(pData->FSMemPool);
	if ( pValue == NULL ) {
		pData->DeniedCount++;
		if ( __test_phase2_has_owner_error() ) {
			pData->ErrorCount++;
		}
	}

	xrtClearError();
	pValue = xrtMemPoolAlloc(pData->MemPool, 24);
	if ( pValue == NULL ) {
		pData->DeniedCount++;
		if ( __test_phase2_has_owner_error() ) {
			pData->ErrorCount++;
		}
	}

	xrtClearError();
	pValue = xrtAVLTreeInsert(pData->Tree, (ptr)(intptr)22, NULL);
	if ( pValue == NULL ) {
		pData->DeniedCount++;
		if ( __test_phase2_has_owner_error() ) {
			pData->ErrorCount++;
		}
	}

	xrtClearError();
	if ( !xrtDictSetPtr(pData->Dict, "k2", 2, (ptr)2, NULL) ) {
		pData->DeniedCount++;
		if ( __test_phase2_has_owner_error() ) {
			pData->ErrorCount++;
		}
	}

	xrtClearError();
	if ( !xrtListSetPtr(pData->List, 2, (ptr)2, NULL) ) {
		pData->DeniedCount++;
		if ( __test_phase2_has_owner_error() ) {
			pData->ErrorCount++;
		}
	}

	return 91;
}


// 内部函数：__test_phase2_owner_allocator_container
static bool __test_phase2_owner_allocator_container(void)
{
	__test_phase2_owner_data tData;
	__test_phase2_tree_item* pTreeNode = NULL;
	xthread pThread = NULL;
	ptr pValue = NULL;
	bool bOk = TRUE;

	memset(&tData, 0, sizeof(tData));
	tData.BSMM = xrtBsmmCreate(sizeof(int), XRT_OBJMODE_LOCAL);
	tData.MemUnit = xrtMemUnitCreate(sizeof(int), XRT_OBJMODE_LOCAL);
	tData.FSMemPool = xrtFSMemPoolCreate(sizeof(int), XRT_OBJMODE_LOCAL);
	tData.MemPool = xrtMemPoolCreate(0, XRT_OBJMODE_LOCAL);
	tData.Tree = xrtAVLTreeCreate(sizeof(__test_phase2_tree_item), (ptr)__test_phase2_tree_comp, XRT_OBJMODE_LOCAL);
	tData.Dict = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_LOCAL);
	tData.List = xrtListCreate(sizeof(ptr), XRT_OBJMODE_LOCAL);
	if ( !tData.BSMM || !tData.MemUnit || !tData.FSMemPool || !tData.MemPool || !tData.Tree || !tData.Dict || !tData.List ) {
		bOk = FALSE;
		goto cleanup;
	}

	pValue = xrtBsmmAlloc(tData.BSMM);
	if ( pValue == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	*(int*)pValue = 7;
	xrtBsmmFree(tData.BSMM, pValue);

	pValue = xrtMemUnitAlloc(tData.MemUnit);
	if ( pValue == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	*(int*)pValue = 8;
	if ( !xrtMemUnitFree(tData.MemUnit, pValue) ) {
		bOk = FALSE;
		goto cleanup;
	}

	pValue = xrtFSMemPoolAlloc(tData.FSMemPool);
	if ( pValue == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	*(int*)pValue = 9;
	xrtFSMemPoolFree(tData.FSMemPool, pValue);

	pValue = xrtMemPoolAlloc(tData.MemPool, 24);
	if ( pValue == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	memset(pValue, 0x5A, 24);
	xrtMemPoolFree(tData.MemPool, pValue);

	pTreeNode = xrtAVLTreeInsert(tData.Tree, (ptr)(intptr)11, NULL);
	if ( pTreeNode == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	pTreeNode->Val = 11;
	if ( xrtAVLTreeSearch(tData.Tree, (ptr)(intptr)11) == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}

	if ( !xrtDictSetPtr(tData.Dict, "k1", 2, (ptr)1, NULL) ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtDictGetPtr(tData.Dict, "k1", 2) != (ptr)1 ) {
		bOk = FALSE;
		goto cleanup;
	}

	if ( !xrtListSetPtr(tData.List, 1, (ptr)1, NULL) ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtListGetPtr(tData.List, 1) != (ptr)1 ) {
		bOk = FALSE;
		goto cleanup;
	}

	pThread = xrtThreadCreate((ptr)__test_phase2_owner_worker, &tData, 0);
	if ( pThread == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	xrtThreadWait(pThread);
	if ( xrtThreadGetExitCode(pThread) != 91 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( tData.DeniedCount != 7 || tData.ErrorCount != 7 ) {
		bOk = FALSE;
		goto cleanup;
	}

cleanup:
	if ( pThread ) {
		xrtThreadDestroy(pThread);
	}
	if ( tData.List ) {
		xrtListDestroy(tData.List);
	}
	if ( tData.Dict ) {
		xrtDictDestroy(tData.Dict);
	}
	if ( tData.Tree ) {
		xrtAVLTreeDestroy(tData.Tree);
	}
	if ( tData.MemPool ) {
		xrtMemPoolDestroy(tData.MemPool);
	}
	if ( tData.FSMemPool ) {
		xrtFSMemPoolDestroy(tData.FSMemPool);
	}
	if ( tData.MemUnit ) {
		xrtMemUnitDestroy(tData.MemUnit);
	}
	if ( tData.BSMM ) {
		xrtBsmmDestroy(tData.BSMM);
	}
	xrtClearError();
	return bOk;
}


// 内部函数：__test_phase2_array_worker
static uint32 __test_phase2_array_worker(ptr pParam)
{
	__test_phase2_array_data* pData = (__test_phase2_array_data*)pParam;

	xrtClearError();
	if ( xrtPtrArrayAppend(pData->PtrArray, (ptr)22) == 0 ) {
		pData->DeniedCount++;
		if ( __test_phase2_has_owner_error() ) {
			pData->ErrorCount++;
		}
	}

	xrtClearError();
	xrtPtrArraySet_Inline(pData->PtrArray, 1, (ptr)33);
	if ( __test_phase2_has_owner_error() ) {
		pData->DeniedCount++;
		pData->ErrorCount++;
	}

	xrtClearError();
	if ( xrtArrayAppend(pData->StructArray, 1) == 0 ) {
		pData->DeniedCount++;
		if ( __test_phase2_has_owner_error() ) {
			pData->ErrorCount++;
		}
	}

	xrtClearError();
	if ( !xrtArrayRemove(pData->StructArray, 1, 1) ) {
		pData->DeniedCount++;
		if ( __test_phase2_has_owner_error() ) {
			pData->ErrorCount++;
		}
	}

	return 92;
}


// 内部函数：__test_phase2_owner_arrays
static bool __test_phase2_owner_arrays(void)
{
	__test_phase2_array_data tData;
	__test_phase2_array_item* pItem = NULL;
	xthread pThread = NULL;
	bool bOk = TRUE;

	memset(&tData, 0, sizeof(tData));
	tData.PtrArray = xrtPtrArrayCreate(XRT_OBJMODE_LOCAL);
	tData.StructArray = xrtArrayCreate(sizeof(__test_phase2_array_item), XRT_OBJMODE_LOCAL);
	if ( !tData.PtrArray || !tData.StructArray ) {
		bOk = FALSE;
		goto cleanup;
	}

	if ( xrtPtrArrayAppend(tData.PtrArray, (ptr)11) != 1 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtPtrArrayGet(tData.PtrArray, 1) != (ptr)11 ) {
		bOk = FALSE;
		goto cleanup;
	}
	xrtPtrArraySet_Inline(tData.PtrArray, 1, (ptr)12);
	if ( xrtPtrArrayGet(tData.PtrArray, 1) != (ptr)12 ) {
		bOk = FALSE;
		goto cleanup;
	}

	if ( xrtArrayAppend(tData.StructArray, 1) != 1 ) {
		bOk = FALSE;
		goto cleanup;
	}
	pItem = xrtArrayGet(tData.StructArray, 1);
	if ( pItem == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	pItem->Val = 21;
	pItem = xrtArrayGet(tData.StructArray, 1);
	if ( pItem == NULL || pItem->Val != 21 ) {
		bOk = FALSE;
		goto cleanup;
	}

	pThread = xrtThreadCreate((ptr)__test_phase2_array_worker, &tData, 0);
	if ( pThread == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	xrtThreadWait(pThread);
	if ( xrtThreadGetExitCode(pThread) != 92 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( tData.DeniedCount != 4 || tData.ErrorCount != 4 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtPtrArrayGet(tData.PtrArray, 1) != (ptr)12 ) {
		bOk = FALSE;
		goto cleanup;
	}
	pItem = xrtArrayGet(tData.StructArray, 1);
	if ( pItem == NULL || pItem->Val != 21 ) {
		bOk = FALSE;
		goto cleanup;
	}

cleanup:
	if ( pThread ) {
		xrtThreadDestroy(pThread);
	}
	if ( tData.StructArray ) {
		xrtArrayDestroy(tData.StructArray);
	}
	if ( tData.PtrArray ) {
		xrtPtrArrayDestroy(tData.PtrArray);
	}
	xrtClearError();
	return bOk;
}


// 内部函数：__test_phase2_shared_worker
static uint32 __test_phase2_shared_worker(ptr pParam)
{
	__test_phase2_shared_data* pData = (__test_phase2_shared_data*)pParam;
	xvalue pVal = NULL;

	xrtClearError();
	if ( xrtDictSetPtr(pData->Dict, "k2", 2, (ptr)2, NULL) ) {
		if ( __test_phase2_has_no_error() ) {
			pData->SharedOkCount++;
		}
	} else {
		pData->DeniedCount++;
	}

	xrtClearError();
	if ( xrtListSetPtr(pData->List, 2, (ptr)2, NULL) ) {
		if ( __test_phase2_has_no_error() ) {
			pData->SharedOkCount++;
		}
	} else {
		pData->DeniedCount++;
	}

	xrtClearError();
	if ( xrtPtrArrayAppend(pData->PtrArray, (ptr)22) != 0 ) {
		if ( __test_phase2_has_no_error() ) {
			pData->SharedOkCount++;
		}
	} else {
		pData->DeniedCount++;
	}

	xrtClearError();
	if ( xrtArrayAppend(pData->StructArray, 1) != 0 ) {
		if ( __test_phase2_has_no_error() ) {
			pData->SharedOkCount++;
		}
	} else {
		pData->DeniedCount++;
	}

	xrtClearError();
	pVal = xvoCreateInt(8);
	if ( xvoArrayAppendValue(pData->ValueArray, pVal, TRUE) ) {
		if ( __test_phase2_has_no_error() ) {
			pData->SharedOkCount++;
		}
	} else {
		pData->DeniedCount++;
		xvoUnref(pVal);
	}

	xrtClearError();
	pVal = xvoCreateInt(10);
	if ( xvoTableSetValue(pData->ValueTable, "k2", 2, pVal, TRUE) ) {
		if ( __test_phase2_has_no_error() ) {
			pData->SharedOkCount++;
		}
	} else {
		pData->DeniedCount++;
		xvoUnref(pVal);
	}

	return 93;
}


// 内部函数：__test_phase2_shared_entry_points
static bool __test_phase2_shared_entry_points(void)
{
	__test_phase2_shared_data tData;
	__test_phase2_array_item* pItem = NULL;
	xthread pThread = NULL;
	bool bOk = TRUE;

	memset(&tData, 0, sizeof(tData));
	tData.Dict = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	tData.List = xrtListCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	tData.PtrArray = xrtPtrArrayCreate(XRT_OBJMODE_SHARED);
	tData.StructArray = xrtArrayCreate(sizeof(__test_phase2_array_item), XRT_OBJMODE_SHARED);
	tData.ValueArray = xvoCreateArrayEx(XRT_OBJMODE_SHARED);
	tData.ValueTable = xvoCreateTableEx(XRT_OBJMODE_SHARED);
	if ( !tData.Dict || !tData.List || !tData.PtrArray || !tData.StructArray || !tData.ValueArray || !tData.ValueTable ) {
		bOk = FALSE;
		goto cleanup;
	}

	if ( xrtOwnerGetMode(&tData.Dict->Owner) != XRT_OBJMODE_SHARED ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( (tData.Dict->Owner.iFlags & XRT_OBJFLAG_SHARED_PENDING) != 0 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtOwnerGetMode(&tData.Dict->AVLT.Owner) != XRT_OBJMODE_SHARED ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( (tData.Dict->AVLT.Owner.iFlags & XRT_OBJFLAG_SHARED_PENDING) != 0 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtOwnerGetMode(&tData.Dict->AVLT.objMM.Owner) != XRT_OBJMODE_SHARED ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtOwnerGetMode(&tData.Dict->AVLT.objMM.arrMMU.Owner) != XRT_OBJMODE_SHARED ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtOwnerGetMode(&tData.Dict->AVLT.objMM.arrMMU.PageMMU.Owner) != XRT_OBJMODE_SHARED ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtOwnerGetMode(&tData.PtrArray->Owner) != XRT_OBJMODE_SHARED ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( (tData.PtrArray->Owner.iFlags & XRT_OBJFLAG_SHARED_PENDING) != 0 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtOwnerGetMode(&tData.StructArray->Owner) != XRT_OBJMODE_SHARED ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( (tData.StructArray->Owner.iFlags & XRT_OBJFLAG_SHARED_PENDING) != 0 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtOwnerGetMode(&tData.List->Owner) != XRT_OBJMODE_SHARED ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( (tData.List->Owner.iFlags & XRT_OBJFLAG_SHARED_PENDING) != 0 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtOwnerGetMode(&tData.List->AVLT.Owner) != XRT_OBJMODE_SHARED ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( (tData.List->AVLT.Owner.iFlags & XRT_OBJFLAG_SHARED_PENDING) != 0 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtOwnerGetMode(&xvoGetArray(tData.ValueArray)->Owner) != XRT_OBJMODE_SHARED ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( (xvoGetArray(tData.ValueArray)->Owner.iFlags & XRT_OBJFLAG_SHARED_PENDING) != 0 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( !xvoIsShared_Inline(tData.ValueArray) ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtOwnerGetMode(&xvoGetTable(tData.ValueTable)->Owner) != XRT_OBJMODE_SHARED ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( (xvoGetTable(tData.ValueTable)->Owner.iFlags & XRT_OBJFLAG_SHARED_PENDING) != 0 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( !xvoIsShared_Inline(tData.ValueTable) ) {
		bOk = FALSE;
		goto cleanup;
	}

	if ( !xrtDictSetPtr(tData.Dict, "k1", 2, (ptr)1, NULL) ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtDictGetPtr(tData.Dict, "k1", 2) != (ptr)1 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( !xrtListSetPtr(tData.List, 1, (ptr)1, NULL) ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtListGetPtr(tData.List, 1) != (ptr)1 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtPtrArrayAppend(tData.PtrArray, (ptr)41) != 1 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtArrayAppend(tData.StructArray, 1) != 1 ) {
		bOk = FALSE;
		goto cleanup;
	}
	pItem = xrtArrayGet(tData.StructArray, 1);
	if ( pItem == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	pItem->Val = 55;
	if ( !xvoArrayAppendInt(tData.ValueArray, 7) ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xvoArrayItemCount(tData.ValueArray) != 1 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( !xvoTableSetInt(tData.ValueTable, "k1", 2, 9) ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( !xvoTableExists(tData.ValueTable, "k1", 2) ) {
		bOk = FALSE;
		goto cleanup;
	}

	pThread = xrtThreadCreate((ptr)__test_phase2_shared_worker, &tData, 0);
	if ( pThread == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	xrtThreadWait(pThread);
	if ( xrtThreadGetExitCode(pThread) != 93 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( tData.SharedOkCount != 6 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( tData.DeniedCount != 0 || tData.PendingCount != 0 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtDictGetPtr(tData.Dict, "k2", 2) != (ptr)2 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtListGetPtr(tData.List, 2) != (ptr)2 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtPtrArrayGet(tData.PtrArray, 2) != (ptr)22 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( tData.StructArray->Count != 2 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xvoArrayItemCount(tData.ValueArray) != 2 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( !xvoTableExists(tData.ValueTable, "k2", 2) ) {
		bOk = FALSE;
		goto cleanup;
	}

cleanup:
	if ( pThread ) {
		xrtThreadDestroy(pThread);
	}
	if ( tData.ValueTable ) {
		xvoUnref(tData.ValueTable);
	}
	if ( tData.ValueArray ) {
		xvoUnref(tData.ValueArray);
	}
	if ( tData.StructArray ) {
		xrtArrayDestroy(tData.StructArray);
	}
	if ( tData.PtrArray ) {
		xrtPtrArrayDestroy(tData.PtrArray);
	}
	if ( tData.List ) {
		xrtListDestroy(tData.List);
	}
	if ( tData.Dict ) {
		xrtDictDestroy(tData.Dict);
	}
	xrtClearError();
	return bOk;
}


// 内部函数：__test_phase2_shared_xvalue_worker
static uint32 __test_phase2_shared_xvalue_worker(ptr pParam)
{
	__test_phase2_shared_xvalue_data* pData = (__test_phase2_shared_xvalue_data*)pParam;
	for ( int i = 0; i < 20000; ++i ) {
		xvoAddRef(pData->SharedArray);
		xvoUnref(pData->SharedArray);
		xvoAddRef(pData->SharedChild);
		xvoUnref(pData->SharedChild);
		xvoAddRef(pData->SharedColl);
		xvoUnref(pData->SharedColl);
		if ( (i & 0xFF) == 0 ) {
			xrtThreadYield();
		}
	}
	return 98;
}


// 内部函数：__test_phase2_shared_xvalue_semantics
static bool __test_phase2_shared_xvalue_semantics(void)
{
	__test_phase2_shared_xvalue_data tData;
	xthread pThread1 = NULL;
	xthread pThread2 = NULL;
	xvalue pLocalList = NULL;
	xvalue pLocalColl = NULL;
	xvalue pStored = NULL;
	bool bOk = TRUE;

	memset(&tData, 0, sizeof(tData));
	tData.SharedArray = xvoCreateArrayEx(XRT_OBJMODE_SHARED);
	tData.SharedTable = xvoCreateTableEx(XRT_OBJMODE_SHARED);
	tData.SharedChild = xvoCreateInt(123);
	tData.SharedNestedList = xvoCreateListEx(XRT_OBJMODE_SHARED);
	tData.SharedColl = xvoCreateCollEx(XRT_OBJMODE_SHARED);
	if ( !tData.SharedArray || !tData.SharedTable || !tData.SharedChild || !tData.SharedNestedList || !tData.SharedColl ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( !xvoIsShared_Inline(tData.SharedArray) || !xvoIsShared_Inline(tData.SharedTable) || !xvoIsShared_Inline(tData.SharedNestedList) || !xvoIsShared_Inline(tData.SharedColl) ) {
		bOk = FALSE;
		goto cleanup;
	}

	xrtClearError();
	if ( !xvoArrayAppendValue(tData.SharedArray, tData.SharedChild, FALSE) ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( !xvoIsShared_Inline(tData.SharedChild) || !__test_phase2_has_no_error() ) {
		bOk = FALSE;
		goto cleanup;
	}

	xrtClearError();
	if ( !xvoTableSetValue(tData.SharedTable, "nested", 6, tData.SharedNestedList, FALSE) ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( !__test_phase2_has_no_error() ) {
		bOk = FALSE;
		goto cleanup;
	}

	xrtClearError();
	if ( !xvoCollSetInt(tData.SharedColl, 456) || !__test_phase2_has_no_error() ) {
		bOk = FALSE;
		goto cleanup;
	}

	xrtClearError();
	if ( !xvoTableSetValue(tData.SharedTable, "coll", 4, tData.SharedColl, FALSE) ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( !__test_phase2_has_no_error() ) {
		bOk = FALSE;
		goto cleanup;
	}

	pLocalList = xvoCreateList();
	if ( pLocalList == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( !xvoListSetInt(pLocalList, 3, 321) ) {
		bOk = FALSE;
		goto cleanup;
	}
	xrtClearError();
	if ( !xvoArrayAppendValue(tData.SharedArray, pLocalList, FALSE) ) {
		bOk = FALSE;
		goto cleanup;
	}
	pStored = xvoIndexGetI64(tData.SharedArray, 1);
	if ( pStored == NULL || pStored == pLocalList || !xvoIsShared_Inline(pStored) ||
		 xvoListItemCount(pStored) != 1 || xvoGetInt(xvoListGetValue(pStored, 3)) != 321 ||
		 !__test_phase2_has_no_error() ) {
		bOk = FALSE;
		goto cleanup;
	}
	xvoUnref(pLocalList);
	pLocalList = NULL;

	pLocalColl = xvoCreateColl();
	if ( pLocalColl == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( !xvoCollSetInt(pLocalColl, 789) ) {
		bOk = FALSE;
		goto cleanup;
	}
	xrtClearError();
	if ( !xvoTableSetValue(tData.SharedTable, "local_coll", 10, pLocalColl, FALSE) ) {
		bOk = FALSE;
		goto cleanup;
	}
	pStored = xvoTableGetValue(tData.SharedTable, "local_coll", 10);
	if ( pStored == NULL || pStored == pLocalColl || !xvoIsShared_Inline(pStored) ||
		 xvoCollItemCount(pStored) != 1 || !__test_phase2_has_no_error() ) {
		bOk = FALSE;
		goto cleanup;
	}
	xvoUnref(pLocalColl);
	pLocalColl = NULL;

	pThread1 = xrtThreadCreate((ptr)__test_phase2_shared_xvalue_worker, &tData, 0);
	pThread2 = xrtThreadCreate((ptr)__test_phase2_shared_xvalue_worker, &tData, 0);
	if ( pThread1 == NULL || pThread2 == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	xrtThreadWait(pThread1);
	xrtThreadWait(pThread2);
	if ( xrtThreadGetExitCode(pThread1) != 98 || xrtThreadGetExitCode(pThread2) != 98 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xvoArrayItemCount(tData.SharedArray) != 2 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( !xvoTableExists(tData.SharedTable, "nested", 6) ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( !xvoTableExists(tData.SharedTable, "coll", 4) ||
		 !xvoTableExists(tData.SharedTable, "local_coll", 10) ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xvoCollItemCount(tData.SharedColl) != 1 ) {
		bOk = FALSE;
		goto cleanup;
	}

cleanup:
	if ( pThread1 ) {
		xrtThreadDestroy(pThread1);
	}
	if ( pThread2 ) {
		xrtThreadDestroy(pThread2);
	}
	if ( pLocalList ) {
		xvoUnref(pLocalList);
	}
	if ( pLocalColl ) {
		xvoUnref(pLocalColl);
	}
	if ( tData.SharedTable ) {
		xvoUnref(tData.SharedTable);
	}
	if ( tData.SharedColl ) {
		xvoUnref(tData.SharedColl);
	}
	if ( tData.SharedNestedList ) {
		xvoUnref(tData.SharedNestedList);
	}
	if ( tData.SharedArray ) {
		xvoUnref(tData.SharedArray);
	}
	if ( tData.SharedChild ) {
		xvoUnref(tData.SharedChild);
	}
	xrtClearError();
	return bOk;
}


// 内部函数：__test_phase2_shared_coll_worker
static uint32 __test_phase2_shared_coll_worker(ptr pParam)
{
	__test_phase2_shared_coll_data* pData = (__test_phase2_shared_coll_data*)pParam;
	xvalue pCheck = NULL;

	xrtClearError();
	if ( !xvoCollSetInt(pData->Coll, 22) || !__test_phase2_has_no_error() ) {
		pData->FailCount++;
		return 112;
	}

	pCheck = xvoCreateInt(22);
	if ( pCheck == NULL ) {
		pData->FailCount++;
		return 112;
	}
	xrtClearError();
	if ( !xvoCollExists(pData->Coll, pCheck) || xvoCollItemCount(pData->Coll) != 2 || !__test_phase2_has_no_error() ) {
		xvoUnref(pCheck);
		pData->FailCount++;
		return 112;
	}
	xvoUnref(pCheck);
	return 111;
}


// 内部函数：__test_phase2_shared_coll_root
static bool __test_phase2_shared_coll_root(void)
{
	__test_phase2_shared_coll_data tData;
	xthread pThread = NULL;
	xvalue pParentColl = NULL;
	xvalue pCheck = NULL;
	bool bOk = TRUE;

	memset(&tData, 0, sizeof(tData));
	tData.Coll = xvoCreateCollEx(XRT_OBJMODE_SHARED);
	pParentColl = xvoCreateCollEx(XRT_OBJMODE_SHARED);
	if ( tData.Coll == NULL || pParentColl == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( !xvoIsShared_Inline(tData.Coll) ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtOwnerGetMode(&xvoGetColl(tData.Coll)->Owner) != XRT_OBJMODE_SHARED ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( (xvoGetColl(tData.Coll)->Owner.iFlags & XRT_OBJFLAG_SHARED_PENDING) != 0 ) {
		bOk = FALSE;
		goto cleanup;
	}

	xrtClearError();
	if ( !xvoCollSetInt(tData.Coll, 11) || xvoCollItemCount(tData.Coll) != 1 || !__test_phase2_has_no_error() ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( !xvoCollSetParent(tData.Coll, pParentColl) ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xvoGetColl(tData.Coll)->Parent != xvoGetColl(pParentColl) ) {
		bOk = FALSE;
		goto cleanup;
	}

	pThread = xrtThreadCreate((ptr)__test_phase2_shared_coll_worker, &tData, 0);
	if ( pThread == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	xrtThreadWait(pThread);
	if ( xrtThreadGetExitCode(pThread) != 111 || tData.FailCount != 0 ) {
		bOk = FALSE;
		goto cleanup;
	}

	pCheck = xvoCreateInt(11);
	if ( pCheck == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	xrtClearError();
	if ( !xvoCollRemove(tData.Coll, pCheck) || !__test_phase2_has_no_error() ) {
		bOk = FALSE;
		goto cleanup;
	}
	xvoUnref(pCheck);
	pCheck = NULL;
	if ( xvoCollItemCount(tData.Coll) != 1 ) {
		bOk = FALSE;
		goto cleanup;
	}

	xrtClearError();
	if ( !xvoCollClear(tData.Coll) || xvoCollItemCount(tData.Coll) != 0 || !__test_phase2_has_no_error() ) {
		bOk = FALSE;
		goto cleanup;
	}

cleanup:
	if ( pThread ) {
		xrtThreadDestroy(pThread);
	}
	if ( pCheck ) {
		xvoUnref(pCheck);
	}
	if ( pParentColl ) {
		xvoUnref(pParentColl);
	}
	if ( tData.Coll ) {
		xvoUnref(tData.Coll);
	}
	xrtClearError();
	return bOk;
}


// 内部函数：__test_phase2_shared_allocator_step
static bool __test_phase2_shared_allocator_step(__test_phase2_shared_alloc_data* pData, int iTag)
{
	int* pInt = NULL;
	char* pBuf = NULL;

	xrtClearError();
	pInt = xrtBsmmAlloc(pData->BSMM);
	if ( pInt == NULL || !__test_phase2_has_no_error() ) {
		return FALSE;
	}
	*pInt = iTag;
	xrtBsmmFree(pData->BSMM, pInt);
	if ( !__test_phase2_has_no_error() ) {
		return FALSE;
	}

	xrtClearError();
	pInt = xrtMemUnitAlloc(pData->MemUnit);
	if ( pInt == NULL || !__test_phase2_has_no_error() ) {
		return FALSE;
	}
	*pInt = iTag + 1;
	if ( !xrtMemUnitFree(pData->MemUnit, pInt) || !__test_phase2_has_no_error() ) {
		return FALSE;
	}

	xrtClearError();
	pInt = xrtFSMemPoolAlloc(pData->FSMemPool);
	if ( pInt == NULL || !__test_phase2_has_no_error() ) {
		return FALSE;
	}
	*pInt = iTag + 2;
	xrtFSMemPoolFree(pData->FSMemPool, pInt);
	if ( !__test_phase2_has_no_error() ) {
		return FALSE;
	}

	xrtClearError();
	pBuf = xrtMemPoolAlloc(pData->MemPool, 24);
	if ( pBuf == NULL || !__test_phase2_has_no_error() ) {
		return FALSE;
	}
	memset(pBuf, iTag & 0xFF, 24);
	xrtMemPoolFree(pData->MemPool, pBuf);
	return __test_phase2_has_no_error();
}


// 内部函数：__test_phase2_shared_allocator_worker
static uint32 __test_phase2_shared_allocator_worker(ptr pParam)
{
	__test_phase2_shared_alloc_data* pData = (__test_phase2_shared_alloc_data*)pParam;
	for ( int i = 0; i < 64; ++i ) {
		if ( !__test_phase2_shared_allocator_step(pData, i + 100) ) {
			pData->FailCount++;
			return 95;
		}
		xrtThreadYield();
	}
	return 94;
}


// 内部函数：__test_phase2_shared_allocator_roots
static bool __test_phase2_shared_allocator_roots(void)
{
	__test_phase2_shared_alloc_data tData;
	xthread pThread1 = NULL;
	xthread pThread2 = NULL;
	bool bOk = TRUE;

	memset(&tData, 0, sizeof(tData));
	tData.BSMM = xrtBsmmCreate(sizeof(int), XRT_OBJMODE_SHARED);
	tData.MemUnit = xrtMemUnitCreate(sizeof(int), XRT_OBJMODE_SHARED);
	tData.FSMemPool = xrtFSMemPoolCreate(sizeof(int), XRT_OBJMODE_SHARED);
	tData.MemPool = xrtMemPoolCreate(0, XRT_OBJMODE_SHARED);
	if ( !tData.BSMM || !tData.MemUnit || !tData.FSMemPool || !tData.MemPool ) {
		bOk = FALSE;
		goto cleanup;
	}

	if ( xrtOwnerGetMode(&tData.BSMM->Owner) != XRT_OBJMODE_SHARED || (tData.BSMM->Owner.iFlags & XRT_OBJFLAG_SHARED_PENDING) != 0 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtOwnerGetMode(&tData.MemUnit->Owner) != XRT_OBJMODE_SHARED || (tData.MemUnit->Owner.iFlags & XRT_OBJFLAG_SHARED_PENDING) != 0 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtOwnerGetMode(&tData.FSMemPool->Owner) != XRT_OBJMODE_SHARED || (tData.FSMemPool->Owner.iFlags & XRT_OBJFLAG_SHARED_PENDING) != 0 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtOwnerGetMode(&tData.MemPool->Owner) != XRT_OBJMODE_SHARED || (tData.MemPool->Owner.iFlags & XRT_OBJFLAG_SHARED_PENDING) != 0 ) {
		bOk = FALSE;
		goto cleanup;
	}

	pThread1 = xrtThreadCreate((ptr)__test_phase2_shared_allocator_worker, &tData, 0);
	pThread2 = xrtThreadCreate((ptr)__test_phase2_shared_allocator_worker, &tData, 0);
	if ( pThread1 == NULL || pThread2 == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}

	for ( int i = 0; i < 64; ++i ) {
		if ( !__test_phase2_shared_allocator_step(&tData, i + 1) ) {
			tData.FailCount++;
			bOk = FALSE;
			goto cleanup;
		}
		xrtThreadYield();
	}

	xrtThreadWait(pThread1);
	xrtThreadWait(pThread2);
	if ( xrtThreadGetExitCode(pThread1) != 94 || xrtThreadGetExitCode(pThread2) != 94 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( tData.FailCount != 0 ) {
		bOk = FALSE;
		goto cleanup;
	}

cleanup:
	if ( pThread1 ) {
		xrtThreadDestroy(pThread1);
	}
	if ( pThread2 ) {
		xrtThreadDestroy(pThread2);
	}
	if ( tData.MemPool ) {
		xrtMemPoolDestroy(tData.MemPool);
	}
	if ( tData.FSMemPool ) {
		xrtFSMemPoolDestroy(tData.FSMemPool);
	}
	if ( tData.MemUnit ) {
		xrtMemUnitDestroy(tData.MemUnit);
	}
	if ( tData.BSMM ) {
		xrtBsmmDestroy(tData.BSMM);
	}
	xrtClearError();
	return bOk;
}


// 内部函数：__test_phase2_shared_tree_worker
static uint32 __test_phase2_shared_tree_worker(ptr pParam)
{
	__test_phase2_shared_tree_data* pData = (__test_phase2_shared_tree_data*)pParam;
	__test_phase2_tree_item* pNode = NULL;
	int iWalkCount = 0;

	xrtClearError();
	pNode = xrtAVLTreeInsert(pData->Tree, (ptr)(intptr)22, NULL);
	if ( pNode == NULL || !__test_phase2_has_no_error() ) {
		pData->FailCount++;
		return 98;
	}
	pNode->Val = 22;

	xrtClearError();
	pNode = xrtAVLTreeSearch(pData->Tree, (ptr)(intptr)11);
	if ( pNode == NULL || pNode->Val != 11 || !__test_phase2_has_no_error() ) {
		pData->FailCount++;
		return 98;
	}

	xrtClearError();
	iWalkCount = 0;
	xrtAVLTreeWalk(pData->Tree, (ptr)__test_phase2_tree_walk_count, &iWalkCount);
	if ( iWalkCount != 2 || !__test_phase2_has_no_error() ) {
		pData->FailCount++;
		return 98;
	}

	xrtClearError();
	xrtAVLTreeIterBegin(pData->Tree);
	pNode = xrtAVLTreeIterNext(pData->Tree);
	if ( pNode == NULL ) {
		pData->FailCount++;
		xrtAVLTreeIterEnd(pData->Tree);
		return 98;
	}
	xrtAVLTreeIterEnd(pData->Tree);
	if ( !__test_phase2_has_no_error() ) {
		pData->FailCount++;
		return 98;
	}

	pData->SharedOkCount++;
	return 99;
}


// 内部函数：__test_phase2_shared_tree_real
static bool __test_phase2_shared_tree_real(void)
{
	__test_phase2_shared_tree_data tData;
	__test_phase2_tree_item* pNode = NULL;
	xthread pThread = NULL;
	bool bOk = TRUE;
	int iWalkCount = 0;

	memset(&tData, 0, sizeof(tData));
	tData.Tree = xrtAVLTreeCreate(sizeof(__test_phase2_tree_item), (ptr)__test_phase2_tree_comp, XRT_OBJMODE_SHARED);
	if ( tData.Tree == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}

	if ( xrtOwnerGetMode(&tData.Tree->Owner) != XRT_OBJMODE_SHARED ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( (tData.Tree->Owner.iFlags & XRT_OBJFLAG_SHARED_PENDING) != 0 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtOwnerGetMode(&tData.Tree->objMM.Owner) != XRT_OBJMODE_SHARED ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( (tData.Tree->objMM.Owner.iFlags & XRT_OBJFLAG_SHARED_PENDING) != 0 ) {
		bOk = FALSE;
		goto cleanup;
	}

	pNode = xrtAVLTreeInsert(tData.Tree, (ptr)(intptr)11, NULL);
	if ( pNode == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	pNode->Val = 11;

	xrtClearError();
	iWalkCount = 0;
	xrtAVLTreeWalk(tData.Tree, (ptr)__test_phase2_tree_walk_count, &iWalkCount);
	if ( iWalkCount != 1 || !__test_phase2_has_no_error() ) {
		bOk = FALSE;
		goto cleanup;
	}

	pThread = xrtThreadCreate((ptr)__test_phase2_shared_tree_worker, &tData, 0);
	if ( pThread == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	xrtThreadWait(pThread);
	if ( xrtThreadGetExitCode(pThread) != 99 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( tData.SharedOkCount != 1 || tData.FailCount != 0 ) {
		bOk = FALSE;
		goto cleanup;
	}

	xrtClearError();
	pNode = xrtAVLTreeSearch(tData.Tree, (ptr)(intptr)22);
	if ( pNode == NULL || pNode->Val != 22 || !__test_phase2_has_no_error() ) {
		bOk = FALSE;
		goto cleanup;
	}

	xrtClearError();
	iWalkCount = 0;
	xrtAVLTreeWalk(tData.Tree, (ptr)__test_phase2_tree_walk_count, &iWalkCount);
	if ( iWalkCount != 2 || !__test_phase2_has_no_error() ) {
		bOk = FALSE;
		goto cleanup;
	}

	xrtClearError();
	if ( !xrtAVLTreeRemove(tData.Tree, (ptr)(intptr)11) || !__test_phase2_has_no_error() ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtAVLTreeSearch(tData.Tree, (ptr)(intptr)11) != NULL ) {
		bOk = FALSE;
		goto cleanup;
	}

cleanup:
	if ( pThread ) {
		xrtThreadDestroy(pThread);
	}
	if ( tData.Tree ) {
		xrtAVLTreeDestroy(tData.Tree);
	}
	xrtClearError();
	return bOk;
}


// 内部函数：__test_phase2_shared_lock_worker
static uint32 __test_phase2_shared_lock_worker(ptr pParam)
{
	__test_phase2_shared_lock_worker_data* pData = (__test_phase2_shared_lock_worker_data*)pParam;
	pData->bEntered = TRUE;
	if ( !xrtDictSetPtr(pData->Dict, "k2", 2, (ptr)2, NULL) ) {
		return 101;
	}
	return 100;
}


// 内部函数：__test_phase2_shared_explicit_lock_api
static bool __test_phase2_shared_explicit_lock_api(void)
{
	__test_phase2_shared_lock_data tData;
	__test_phase2_shared_lock_worker_data tWorker;
	__test_phase2_tree_item* pTreeItem = NULL;
	__test_phase2_array_item* pArrayItem = NULL;
	xthread pThread = NULL;
	bool bDictLocked = FALSE;
	bool bListLocked = FALSE;
	bool bPtrLocked = FALSE;
	bool bArrayLocked = FALSE;
	bool bTreeLocked = FALSE;
	bool bOk = TRUE;
	int iWalkCount = 0;
	Dict_Key* pDictNode = NULL;
	int64* pListNode = NULL;
	__test_phase2_tree_item* pTreeNode = NULL;

	memset(&tData, 0, sizeof(tData));
	memset(&tWorker, 0, sizeof(tWorker));

	tData.Dict = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	tData.List = xrtListCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	tData.PtrArray = xrtPtrArrayCreate(XRT_OBJMODE_SHARED);
	tData.StructArray = xrtArrayCreate(sizeof(__test_phase2_array_item), XRT_OBJMODE_SHARED);
	tData.Tree = xrtAVLTreeCreate(sizeof(__test_phase2_tree_item), (ptr)__test_phase2_tree_comp, XRT_OBJMODE_SHARED);
	if ( !tData.Dict || !tData.List || !tData.PtrArray || !tData.StructArray || !tData.Tree ) {
		bOk = FALSE;
		goto cleanup;
	}

	if ( !xrtDictSetPtr(tData.Dict, "k1", 2, (ptr)1, NULL) ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( !xrtListSetPtr(tData.List, 1, (ptr)1, NULL) ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtPtrArrayAppend(tData.PtrArray, (ptr)11) != 1 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtArrayAppend(tData.StructArray, 1) != 1 ) {
		bOk = FALSE;
		goto cleanup;
	}
	pArrayItem = xrtArrayGet(tData.StructArray, 1);
	if ( pArrayItem == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	pArrayItem->Val = 21;
	pTreeItem = xrtAVLTreeInsert(tData.Tree, (ptr)(intptr)11, NULL);
	if ( pTreeItem == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	pTreeItem->Val = 11;

	tWorker.Dict = tData.Dict;
	if ( !xrtDictLock(tData.Dict) ) {
		bOk = FALSE;
		goto cleanup;
	}
	bDictLocked = TRUE;
	if ( xrtDictGetPtr(tData.Dict, "k1", 2) != (ptr)1 ) {
		bOk = FALSE;
		goto cleanup;
	}
	iWalkCount = 0;
	xrtDictWalk(tData.Dict, (ptr)__test_phase2_dict_walk_count, &iWalkCount);
	if ( iWalkCount != 1 ) {
		bOk = FALSE;
		goto cleanup;
	}
	xrtDictIterBegin(tData.Dict);
	pDictNode = xrtDictIterNext(tData.Dict);
	if ( pDictNode == NULL || pDictNode->Key == NULL || pDictNode->KeyLen != 2 || memcmp(pDictNode->Key, "k1", 2) != 0 ) {
		xrtDictIterEnd(tData.Dict);
		bOk = FALSE;
		goto cleanup;
	}
	xrtDictIterEnd(tData.Dict);
	pThread = xrtThreadCreate((ptr)__test_phase2_shared_lock_worker, &tWorker, 0);
	if ( pThread == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtThreadWaitTimeout(pThread, 50) != XRT_WAIT_TIMEOUT ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtDictGetPtr(tData.Dict, "k2", 2) != NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	xrtDictUnlock(tData.Dict);
	bDictLocked = FALSE;
	xrtThreadWait(pThread);
	if ( xrtThreadGetExitCode(pThread) != 100 || !tWorker.bEntered ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtDictGetPtr(tData.Dict, "k2", 2) != (ptr)2 ) {
		bOk = FALSE;
		goto cleanup;
	}

	if ( !xrtListLock(tData.List) ) {
		bOk = FALSE;
		goto cleanup;
	}
	bListLocked = TRUE;
	if ( xrtListGetPtr(tData.List, 1) != (ptr)1 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( !xrtListSetPtr(tData.List, 2, (ptr)2, NULL) ) {
		bOk = FALSE;
		goto cleanup;
	}
	iWalkCount = 0;
	xrtListWalk(tData.List, (ptr)__test_phase2_list_walk_count, &iWalkCount);
	if ( iWalkCount != 2 ) {
		bOk = FALSE;
		goto cleanup;
	}
	xrtListIterBegin(tData.List);
	pListNode = xrtListIterNext(tData.List);
	if ( pListNode == NULL ) {
		xrtListIterEnd(tData.List);
		bOk = FALSE;
		goto cleanup;
	}
	xrtListIterEnd(tData.List);
	xrtListUnlock(tData.List);
	bListLocked = FALSE;

	if ( !xrtPtrArrayLock(tData.PtrArray) ) {
		bOk = FALSE;
		goto cleanup;
	}
	bPtrLocked = TRUE;
	if ( xrtPtrArrayAppend(tData.PtrArray, (ptr)12) != 2 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtPtrArrayGet(tData.PtrArray, 1) != (ptr)11 || xrtPtrArrayGet(tData.PtrArray, 2) != (ptr)12 ) {
		bOk = FALSE;
		goto cleanup;
	}
	xrtPtrArrayUnlock(tData.PtrArray);
	bPtrLocked = FALSE;

	if ( !xrtArrayLock(tData.StructArray) ) {
		bOk = FALSE;
		goto cleanup;
	}
	bArrayLocked = TRUE;
	if ( xrtArrayAppend(tData.StructArray, 1) != 2 ) {
		bOk = FALSE;
		goto cleanup;
	}
	pArrayItem = xrtArrayGet(tData.StructArray, 2);
	if ( pArrayItem == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	pArrayItem->Val = 22;
	xrtArrayUnlock(tData.StructArray);
	bArrayLocked = FALSE;

	if ( !xrtAVLTreeLock(tData.Tree) ) {
		bOk = FALSE;
		goto cleanup;
	}
	bTreeLocked = TRUE;
	pTreeItem = xrtAVLTreeSearch(tData.Tree, (ptr)(intptr)11);
	if ( pTreeItem == NULL || pTreeItem->Val != 11 ) {
		bOk = FALSE;
		goto cleanup;
	}
	pTreeItem = xrtAVLTreeInsert(tData.Tree, (ptr)(intptr)12, NULL);
	if ( pTreeItem == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	pTreeItem->Val = 12;
	iWalkCount = 0;
	xrtAVLTreeWalk(tData.Tree, (ptr)__test_phase2_tree_walk_count, &iWalkCount);
	if ( iWalkCount != 2 ) {
		bOk = FALSE;
		goto cleanup;
	}
	xrtAVLTreeIterBegin(tData.Tree);
	pTreeNode = xrtAVLTreeIterNext(tData.Tree);
	if ( pTreeNode == NULL ) {
		xrtAVLTreeIterEnd(tData.Tree);
		bOk = FALSE;
		goto cleanup;
	}
	xrtAVLTreeIterEnd(tData.Tree);
	xrtAVLTreeUnlock(tData.Tree);
	bTreeLocked = FALSE;

cleanup:
	if ( bTreeLocked ) {
		xrtAVLTreeUnlock(tData.Tree);
	}
	if ( bArrayLocked ) {
		xrtArrayUnlock(tData.StructArray);
	}
	if ( bPtrLocked ) {
		xrtPtrArrayUnlock(tData.PtrArray);
	}
	if ( bListLocked ) {
		xrtListUnlock(tData.List);
	}
	if ( bDictLocked ) {
		xrtDictUnlock(tData.Dict);
	}
	if ( pThread ) {
		xrtThreadWait(pThread);
		xrtThreadDestroy(pThread);
	}
	if ( tData.Tree ) {
		xrtAVLTreeDestroy(tData.Tree);
	}
	if ( tData.StructArray ) {
		xrtArrayDestroy(tData.StructArray);
	}
	if ( tData.PtrArray ) {
		xrtPtrArrayDestroy(tData.PtrArray);
	}
	if ( tData.List ) {
		xrtListDestroy(tData.List);
	}
	if ( tData.Dict ) {
		xrtDictDestroy(tData.Dict);
	}
	xrtClearError();
	return bOk;
}


// 内部函数：__test_phase2_shared_container_step
static bool __test_phase2_shared_container_step(__test_phase2_shared_container_data* pData, int iKeyBase)
{
	char sKey[32];
	int iKeyLen = snprintf(sKey, sizeof(sKey), "k%d", iKeyBase);
	ptr pValue = (ptr)(intptr_t)(iKeyBase + 1000);

	xrtClearError();
	if ( !xrtDictSetPtr(pData->Dict, sKey, iKeyLen, pValue, NULL) || !__test_phase2_has_no_error() ) {
		return FALSE;
	}
	if ( xrtDictGetPtr(pData->Dict, sKey, iKeyLen) != pValue || !__test_phase2_has_no_error() ) {
		return FALSE;
	}

	xrtClearError();
	if ( !xrtListSetPtr(pData->List, iKeyBase, pValue, NULL) || !__test_phase2_has_no_error() ) {
		return FALSE;
	}
	if ( xrtListGetPtr(pData->List, iKeyBase) != pValue || !__test_phase2_has_no_error() ) {
		return FALSE;
	}

	xrtClearError();
	if ( xrtPtrArrayAppend(pData->PtrArray, pValue) == 0 || !__test_phase2_has_no_error() ) {
		return FALSE;
	}

	xrtClearError();
	if ( xrtArrayAppend(pData->StructArray, 1) == 0 || !__test_phase2_has_no_error() ) {
		return FALSE;
	}

	return TRUE;
}


// 内部函数：__test_phase2_shared_container_worker
static uint32 __test_phase2_shared_container_worker(ptr pParam)
{
	__test_phase2_shared_container_worker_arg* pArg = (__test_phase2_shared_container_worker_arg*)pParam;
	for ( int i = 0; i < 64; ++i ) {
		if ( !__test_phase2_shared_container_step(pArg->pData, pArg->Base + i) ) {
			pArg->pData->FailCount++;
			return 97;
		}
		xrtThreadYield();
	}
	return 96;
}


// 内部函数：__test_phase2_shared_container_roots
static bool __test_phase2_shared_container_roots(void)
{
	__test_phase2_shared_container_data tData;
	__test_phase2_shared_container_worker_arg tArg1;
	__test_phase2_shared_container_worker_arg tArg2;
	xthread pThread1 = NULL;
	xthread pThread2 = NULL;
	bool bOk = TRUE;

	memset(&tData, 0, sizeof(tData));
	memset(&tArg1, 0, sizeof(tArg1));
	memset(&tArg2, 0, sizeof(tArg2));
	tData.Dict = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	tData.List = xrtListCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	tData.PtrArray = xrtPtrArrayCreate(XRT_OBJMODE_SHARED);
	tData.StructArray = xrtArrayCreate(sizeof(__test_phase2_array_item), XRT_OBJMODE_SHARED);
	if ( !tData.Dict || !tData.List || !tData.PtrArray || !tData.StructArray ) {
		bOk = FALSE;
		goto cleanup;
	}

	tArg1.pData = &tData;
	tArg1.Base = 1000;
	tArg2.pData = &tData;
	tArg2.Base = 2000;
	pThread1 = xrtThreadCreate((ptr)__test_phase2_shared_container_worker, &tArg1, 0);
	pThread2 = xrtThreadCreate((ptr)__test_phase2_shared_container_worker, &tArg2, 0);
	if ( pThread1 == NULL || pThread2 == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}

	for ( int i = 0; i < 64; ++i ) {
		if ( !__test_phase2_shared_container_step(&tData, i + 1) ) {
			tData.FailCount++;
			bOk = FALSE;
			goto cleanup;
		}
		xrtThreadYield();
	}

	xrtThreadWait(pThread1);
	xrtThreadWait(pThread2);
	if ( xrtThreadGetExitCode(pThread1) != 96 || xrtThreadGetExitCode(pThread2) != 96 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( tData.FailCount != 0 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtDictCount(tData.Dict) != 192 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtListCount(tData.List) != 192 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( tData.PtrArray->Count != 192 ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( tData.StructArray->Count != 192 ) {
		bOk = FALSE;
		goto cleanup;
	}

cleanup:
	if ( pThread1 ) {
		xrtThreadDestroy(pThread1);
	}
	if ( pThread2 ) {
		xrtThreadDestroy(pThread2);
	}
	if ( tData.StructArray ) {
		xrtArrayDestroy(tData.StructArray);
	}
	if ( tData.PtrArray ) {
		xrtPtrArrayDestroy(tData.PtrArray);
	}
	if ( tData.List ) {
		xrtListDestroy(tData.List);
	}
	if ( tData.Dict ) {
		xrtDictDestroy(tData.Dict);
	}
	xrtClearError();
	return bOk;
}


// 运行时PHASE2测试
int Test_Runtime_Phase2(xrtGlobalData* xCore)
{
	bool bOwnerChain;
	bool bArrayChain;
	bool bSharedEntry;
	bool bSharedLock;
	bool bSharedTree;
	bool bSharedContainer;
	bool bSharedAlloc;
	bool bSharedXValue;
	bool bSharedColl;
	(void)xCore;

	printf("\n\n\n------------------------------------\n\n Runtime Phase-2 测试 :\n\n");

	printf("[Test 1] owner-thread allocator / container 保护:\n");
	bOwnerChain = __test_phase2_owner_allocator_container();
	printf("  allocator/container owner 保护: %s\n", bOwnerChain ? "成功" : "失败");

	printf("\n[Test 2] owner-thread array / parray 保护:\n");
	bArrayChain = __test_phase2_owner_arrays();
	printf("  array/parray owner 保护: %s\n", bArrayChain ? "成功" : "失败");

	printf("\n[Test 3] shared mode constructor staged 入口:\n");
	bSharedEntry = __test_phase2_shared_entry_points();
	printf("  shared container/value 边界: %s\n", bSharedEntry ? "成功" : "失败");

	printf("\n[Test 4] shared explicit lock API:\n");
	bSharedLock = __test_phase2_shared_explicit_lock_api();
	printf("  shared lock / view API: %s\n", bSharedLock ? "成功" : "失败");

	printf("\n[Test 5] shared generic avltree real 语义:\n");
	bSharedTree = __test_phase2_shared_tree_real();
	printf("  shared avltree real 语义: %s\n", bSharedTree ? "成功" : "失败");

	printf("\n[Test 6] shared container roots:\n");
	bSharedContainer = __test_phase2_shared_container_roots();
	printf("  shared container roots: %s\n", bSharedContainer ? "成功" : "失败");

	printf("\n[Test 7] shared low-level allocator roots:\n");
	bSharedAlloc = __test_phase2_shared_allocator_roots();
	printf("  shared allocator roots: %s\n", bSharedAlloc ? "成功" : "失败");

	printf("\n[Test 8] shared xvalue 语义:\n");
	bSharedXValue = __test_phase2_shared_xvalue_semantics();
	printf("  shared xvalue refcount / store: %s\n", bSharedXValue ? "成功" : "失败");

	printf("\n[Test 9] shared coll root 语义:\n");
	bSharedColl = __test_phase2_shared_coll_root();
	printf("  shared coll root: %s\n", bSharedColl ? "成功" : "失败");

	printf("\n Runtime Phase-2 测试完成\n");
	return bOwnerChain && bArrayChain && bSharedEntry && bSharedLock && bSharedTree &&
		bSharedContainer && bSharedAlloc && bSharedXValue && bSharedColl ? 0 : 1;
}

#endif
