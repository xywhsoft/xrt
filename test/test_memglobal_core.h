#ifndef XRT_TEST_MEMGLOBAL_CORE_H
#define XRT_TEST_MEMGLOBAL_CORE_H

// 内部函数：__Test_MemGlobalCoreRequire
static void __Test_MemGlobalCoreRequire(bool bCond, const char* sMsg)
{
	if ( !bCond ) {
		fprintf(stderr, "[memglobal-core] FAIL: %s\n", sMsg ? sMsg : "(no message)");
		exit(1);
	}
}

typedef struct
{
	xsem pReady;
	xsem pGo;
} __Test_MemGlobalCoreThreadCtx;


// 内部函数：__Test_MemGlobalCoreRequireAllocatorError
static void __Test_MemGlobalCoreRequireAllocatorError(void)
{
	str sError = xrtGetError();

	__Test_MemGlobalCoreRequire(sError != NULL, "allocator error should not be null");
	__Test_MemGlobalCoreRequire(strcmp(sError, "memory belongs to an explicit pool allocator.") == 0, "allocator error message mismatch");
}


// 内部函数：__Test_MemGlobalCoreHoldRuntimeWorker
static uint32 __Test_MemGlobalCoreHoldRuntimeWorker(ptr pParam)
{
	__Test_MemGlobalCoreThreadCtx* pCtx = (__Test_MemGlobalCoreThreadCtx*)pParam;
	ptr pHeap;
	ptr pTemp;

	if ( pCtx == NULL ) {
		return 1;
	}
	if ( xrtThreadGetCurrent() == NULL ) {
		return 2;
	}
	if ( !xrtSemPost(pCtx->pReady) ) {
		return 3;
	}

	xrtSemWait(pCtx->pGo);

	pHeap = xrtMalloc(96);
	if ( pHeap == NULL ) {
		return 4;
	}
	memset(pHeap, 0x3C, 96);

	pTemp = xrtTempMemory(64);
	if ( pTemp == NULL ) {
		xrtFree(pHeap);
		return 5;
	}
	memset(pTemp, 0xA5, 64);

	xrtFree(pHeap);
	xrtFreeTempMemory();
	return 0;
}


// MEMGLOBAL核心测试
static void Test_MemGlobalCore(void)
{
	xrtThreadData* pThreadData;
	xrtMemThreadCache* pCache;
	ptr pSmall;
	ptr pZero;
	ptr pReallocSame;
	ptr pReallocGrow;
	ptr pBig;
	ptr pBigGrow;
	ptr pBigShrink;
	ptr pNullGrow;
	xfsmempool objFSMemPool;
	ptr pFSMem;
	xmempool objMemPool;
	ptr pMemPool;
	__Test_MemGlobalCoreThreadCtx tThreadCtx;
	xthread pThread;
	xrtGlobalData* pCore;
	xrtMemBlockHeader* pHeader;

	printf("[memglobal-core] start\n");

	__Test_MemGlobalCoreRequire(xCore.MemGlobal.iClassCount == XRT_MEMPOOL_CLASS_COUNT_DEFAULT, "global class count mismatch");
	__Test_MemGlobalCoreRequire(xCore.MemGlobal.iClassStep == XRT_MEMPOOL_STEP_SIZE, "global class step mismatch");
	__Test_MemGlobalCoreRequire(xCore.MemGlobal.iCutoff == XRT_MEMPOOL_CUTOFF_DEFAULT, "global cutoff mismatch");
	__Test_MemGlobalCoreRequire(xCore.MemGlobal.arrClassDesc[0].iBlockSize == 16, "class[0] block size mismatch");
	__Test_MemGlobalCoreRequire(xCore.MemGlobal.arrClassDesc[63].iBlockSize == 1024, "last class block size mismatch");
	__Test_MemGlobalCoreRequire(xCore.MemGlobal.arrSizeClassLut[1] == 0, "lut[1] mismatch");
	__Test_MemGlobalCoreRequire(xCore.MemGlobal.arrSizeClassLut[16] == 0, "lut[16] mismatch");
	__Test_MemGlobalCoreRequire(xCore.MemGlobal.arrSizeClassLut[17] == 1, "lut[17] mismatch");
	__Test_MemGlobalCoreRequire(xCore.MemGlobal.arrSizeClassLut[1024] == 63, "lut[1024] mismatch");

	pThreadData = xrtThreadGetCurrent();
	__Test_MemGlobalCoreRequire(pThreadData != NULL, "current thread must be attached");
	__Test_MemGlobalCoreRequire(pThreadData->tMem.pLocalAlloc != NULL, "thread local mem cache missing");

	pCache = (xrtMemThreadCache*)pThreadData->tMem.pLocalAlloc;
	__Test_MemGlobalCoreRequire(pCache->iClassCount == XRT_MEMPOOL_CLASS_COUNT_DEFAULT, "thread cache class count mismatch");
	__Test_MemGlobalCoreRequire(pCache->iCacheLimit == XRT_MEMGLOBAL_CACHE_LIMIT, "thread cache limit mismatch");

	pSmall = xrtMalloc(24);
	__Test_MemGlobalCoreRequire(pSmall != NULL, "pooled malloc failed");
	pHeader = (xrtMemBlockHeader*)((char*)pSmall - sizeof(xrtMemBlockHeader));
	__Test_MemGlobalCoreRequire(pHeader->iMagic == XRT_MEMBLOCK_MAGIC, "pooled header magic mismatch");
	__Test_MemGlobalCoreRequire((pHeader->iFlags & XRT_MEMBLOCK_FLAG_POOLED) != 0, "pooled malloc flag mismatch");
	__Test_MemGlobalCoreRequire(pHeader->iClassIndex == 1, "pooled malloc class mismatch");
	__Test_MemGlobalCoreRequire(pHeader->iRequestSize == 24, "pooled malloc request size mismatch");

	pZero = xrtCalloc(4, 8);
	__Test_MemGlobalCoreRequire(pZero != NULL, "pooled calloc failed");
	__Test_MemGlobalCoreRequire(memcmp(pZero, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16) == 0, "calloc should zero memory");

	memset(pSmall, 0x5A, 24);
	pReallocSame = xrtRealloc(pSmall, 32);
	__Test_MemGlobalCoreRequire(pReallocSame == pSmall, "same-class realloc should stay in place");
	pHeader = (xrtMemBlockHeader*)((char*)pReallocSame - sizeof(xrtMemBlockHeader));
	__Test_MemGlobalCoreRequire(pHeader->iRequestSize == 32, "same-class realloc request size mismatch");

	pReallocGrow = xrtRealloc(pReallocSame, 48);
	__Test_MemGlobalCoreRequire(pReallocGrow != NULL, "cross-class realloc failed");
	__Test_MemGlobalCoreRequire(memcmp(pReallocGrow, "\x5A\x5A\x5A\x5A", 4) == 0, "cross-class realloc should preserve payload");
	pHeader = (xrtMemBlockHeader*)((char*)pReallocGrow - sizeof(xrtMemBlockHeader));
	__Test_MemGlobalCoreRequire((pHeader->iFlags & XRT_MEMBLOCK_FLAG_POOLED) != 0, "cross-class realloc should stay pooled");
	__Test_MemGlobalCoreRequire(pHeader->iClassIndex == 2, "cross-class realloc target class mismatch");

	pBig = xrtMalloc(2048);
	__Test_MemGlobalCoreRequire(pBig != NULL, "backing malloc failed");
	pHeader = (xrtMemBlockHeader*)((char*)pBig - sizeof(xrtMemBlockHeader));
	__Test_MemGlobalCoreRequire((pHeader->iFlags & XRT_MEMBLOCK_FLAG_BACKING) != 0, "backing malloc flag mismatch");
	__Test_MemGlobalCoreRequire(pHeader->iRequestSize == 2048, "backing malloc request size mismatch");

	pBigGrow = xrtRealloc(pBig, 3072);
	__Test_MemGlobalCoreRequire(pBigGrow != NULL, "backing realloc grow failed");
	pHeader = (xrtMemBlockHeader*)((char*)pBigGrow - sizeof(xrtMemBlockHeader));
	__Test_MemGlobalCoreRequire((pHeader->iFlags & XRT_MEMBLOCK_FLAG_BACKING) != 0, "backing realloc grow should stay backing");
	__Test_MemGlobalCoreRequire(pHeader->iRequestSize == 3072, "backing realloc grow size mismatch");

	pBigShrink = xrtRealloc(pBigGrow, 128);
	__Test_MemGlobalCoreRequire(pBigShrink != NULL, "backing realloc shrink-to-pooled failed");
	pHeader = (xrtMemBlockHeader*)((char*)pBigShrink - sizeof(xrtMemBlockHeader));
	__Test_MemGlobalCoreRequire((pHeader->iFlags & XRT_MEMBLOCK_FLAG_POOLED) != 0, "backing realloc shrink should become pooled");
	__Test_MemGlobalCoreRequire(pHeader->iRequestSize == 128, "backing realloc shrink size mismatch");

	pNullGrow = xrtRealloc(NULL, 64);
	__Test_MemGlobalCoreRequire(pNullGrow != NULL, "realloc(NULL, size) should allocate");
	__Test_MemGlobalCoreRequire(xrtRealloc(pNullGrow, 0) == NULL, "realloc(ptr, 0) should return NULL");

	objFSMemPool = xrtFSMemPoolCreate(64, 0);
	__Test_MemGlobalCoreRequire(objFSMemPool != NULL, "fixed-size memory pool create failed");
	pFSMem = xrtFSMemPoolAlloc(objFSMemPool);
	__Test_MemGlobalCoreRequire(pFSMem != NULL, "fixed-size memory pool alloc failed");

	xrtClearError();
	xrtFree(pFSMem);
	__Test_MemGlobalCoreRequireAllocatorError();

	xrtClearError();
	__Test_MemGlobalCoreRequire(xrtRealloc(pFSMem, 80) == NULL, "fixed-size pool realloc should fail");
	__Test_MemGlobalCoreRequireAllocatorError();

	xrtFSMemPoolFree(objFSMemPool, pFSMem);
	xrtFSMemPoolDestroy(objFSMemPool);

	objMemPool = xrtMemPoolCreate(0, 0);
	__Test_MemGlobalCoreRequire(objMemPool != NULL, "memory pool create failed");
	pMemPool = xrtMemPoolAlloc(objMemPool, 48);
	__Test_MemGlobalCoreRequire(pMemPool != NULL, "memory pool alloc failed");

	xrtClearError();
	xrtFree(pMemPool);
	__Test_MemGlobalCoreRequireAllocatorError();

	xrtClearError();
	__Test_MemGlobalCoreRequire(xrtRealloc(pMemPool, 96) == NULL, "memory pool realloc should fail");
	__Test_MemGlobalCoreRequireAllocatorError();

	xrtMemPoolFree(objMemPool, pMemPool);
	xrtMemPoolDestroy(objMemPool);

	xrtFree(pZero);
	xrtFree(pReallocGrow);
	xrtFree(pBigShrink);

	memset(&tThreadCtx, 0, sizeof(tThreadCtx));
	tThreadCtx.pReady = xrtSemCreate(0, 1);
	tThreadCtx.pGo = xrtSemCreate(0, 1);
	__Test_MemGlobalCoreRequire(tThreadCtx.pReady != NULL, "ready semaphore create failed");
	__Test_MemGlobalCoreRequire(tThreadCtx.pGo != NULL, "go semaphore create failed");

	pThread = xrtThreadCreate(__Test_MemGlobalCoreHoldRuntimeWorker, &tThreadCtx, 0);
	__Test_MemGlobalCoreRequire(pThread != NULL, "worker thread create failed");
	__Test_MemGlobalCoreRequire(xrtSemWaitTimeout(tThreadCtx.pReady, 5000) == XRT_WAIT_OK, "worker ready wait timeout");

	xrtUnit();
	__Test_MemGlobalCoreRequire(xCore.bInit == TRUE, "runtime should stay initialized while worker is attached");

	__Test_MemGlobalCoreRequire(xrtSemPost(tThreadCtx.pGo) == TRUE, "worker go post failed");
	__Test_MemGlobalCoreRequire(xrtThreadWaitTimeout(pThread, 5000) == XRT_WAIT_OK, "worker join timeout");
	__Test_MemGlobalCoreRequire(xrtThreadGetExitCode(pThread) == 0, "worker exit code mismatch");

	xrtThreadDestroy(pThread);
	xrtSemDestroy(tThreadCtx.pReady);
	xrtSemDestroy(tThreadCtx.pGo);
	__Test_MemGlobalCoreRequire(xCore.bInit == FALSE, "runtime should finalize after last worker detach");

	pCore = xrtInit();
	__Test_MemGlobalCoreRequire(pCore != NULL, "runtime reinit failed");
	__Test_MemGlobalCoreRequire(xrtThreadGetCurrent() != NULL, "main thread should reattach after reinit");

	printf("[memglobal-core] PASS\n");
}

#endif
