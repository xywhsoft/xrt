#ifndef XRT_TEST_MEMPOOL_CORE_H
#define XRT_TEST_MEMPOOL_CORE_H

static void __Test_MemPoolCoreRequire(bool bCond, const char* sMsg)
{
	if ( !bCond ) {
		fprintf(stderr, "[mempool-core] FAIL: %s\n", sMsg ? sMsg : "(no message)");
		exit(1);
	}
}

static void Test_MemPoolCore(void)
{
	xmempool objDefault;
	xmempool objCustom;
	ptr pSmall;
	ptr pEdge;
	ptr pBig;
	ptr pCustomPool;
	ptr pCustomBig;

	printf("[mempool-core] start\n");

	objDefault = xrtMemPoolCreate(0);
	__Test_MemPoolCoreRequire(objDefault != NULL, "default pool create failed");
	__Test_MemPoolCoreRequire(objDefault->iBucketStep == XRT_MEMPOOL_STEP_SIZE, "default step mismatch");
	__Test_MemPoolCoreRequire(objDefault->iFallbackCutoff == XRT_MEMPOOL_CUTOFF_DEFAULT, "default cutoff mismatch");
	__Test_MemPoolCoreRequire(objDefault->iBucketCount == XRT_MEMPOOL_CLASS_COUNT_DEFAULT, "default bucket count mismatch");
	__Test_MemPoolCoreRequire(objDefault->FSB_Lut != NULL, "default LUT missing");
	__Test_MemPoolCoreRequire(objDefault->FSB_Memory != NULL, "default bucket array missing");
	__Test_MemPoolCoreRequire(objDefault->FSB_Lut[1] == 0, "size 1 should map to bucket 0");
	__Test_MemPoolCoreRequire(objDefault->FSB_Lut[16] == 0, "size 16 should map to bucket 0");
	__Test_MemPoolCoreRequire(objDefault->FSB_Lut[17] == 1, "size 17 should map to bucket 1");
	__Test_MemPoolCoreRequire(objDefault->FSB_Lut[1024] == 63, "size 1024 should map to last default bucket");

	pSmall = xrtMemPoolAlloc(objDefault, 24);
	pEdge = xrtMemPoolAlloc(objDefault, 1024);
	pBig = xrtMemPoolAlloc(objDefault, 1025);
	__Test_MemPoolCoreRequire(pSmall != NULL, "small pooled alloc failed");
	__Test_MemPoolCoreRequire(pEdge != NULL, "edge pooled alloc failed");
	__Test_MemPoolCoreRequire(pBig != NULL, "fallback alloc failed");
	__Test_MemPoolCoreRequire(objDefault->arrMMU.Count == 2, "default pool should have two MMU buckets after 24/1024 alloc");
	__Test_MemPoolCoreRequire(objDefault->BigMM.Count == 1, "default pool should have one big alloc entry after 1025 alloc");
	xrtMemPoolFree(objDefault, pSmall);
	xrtMemPoolFree(objDefault, pEdge);
	xrtMemPoolFree(objDefault, pBig);
	xrtMemPoolDestroy(objDefault);

	objCustom = xrtMemPoolCreate(100);
	__Test_MemPoolCoreRequire(objCustom != NULL, "custom pool create failed");
	__Test_MemPoolCoreRequire(objCustom->iFallbackCutoff == 100, "custom cutoff mismatch");
	__Test_MemPoolCoreRequire(objCustom->iBucketCount == 7, "custom bucket count mismatch");
	__Test_MemPoolCoreRequire(objCustom->FSB_Lut[100] == 6, "custom size 100 should map to last bucket");
	pCustomPool = xrtMemPoolAlloc(objCustom, 100);
	pCustomBig = xrtMemPoolAlloc(objCustom, 101);
	__Test_MemPoolCoreRequire(pCustomPool != NULL, "custom pooled alloc failed");
	__Test_MemPoolCoreRequire(pCustomBig != NULL, "custom fallback alloc failed");
	__Test_MemPoolCoreRequire(objCustom->arrMMU.Count == 1, "custom pool should have one pooled MMU");
	__Test_MemPoolCoreRequire(objCustom->BigMM.Count == 1, "custom pool should have one big alloc entry");
	xrtMemPoolFree(objCustom, pCustomPool);
	xrtMemPoolFree(objCustom, pCustomBig);
	xrtMemPoolDestroy(objCustom);

	printf("[mempool-core] PASS\n");
}

#endif
