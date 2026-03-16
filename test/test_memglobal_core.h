#ifndef XRT_TEST_MEMGLOBAL_CORE_H
#define XRT_TEST_MEMGLOBAL_CORE_H

static void __Test_MemGlobalCoreRequire(bool bCond, const char* sMsg)
{
	if ( !bCond ) {
		fprintf(stderr, "[memglobal-core] FAIL: %s\n", sMsg ? sMsg : "(no message)");
		exit(1);
	}
}

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

	xrtFree(pZero);
	xrtFree(pReallocGrow);
	xrtFree(pBigShrink);

	printf("[memglobal-core] PASS\n");
}

#endif
