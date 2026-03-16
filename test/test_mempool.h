static void Test_MemPool(xrtGlobalData* xCore)
{
	(void)xCore;

	printf("\n\n\n------------------------------------\n\n MemPool Test:\n\n");

	printf("MemPool subject 1 : create default pool\n\n");
	xmempool objMP = xrtMemPoolCreate(0, XRT_OBJMODE_LOCAL);
	printf("  object                : %p\n", objMP);
	printf("  fallback cutoff       : %u (expected 1024)\n", objMP->iFallbackCutoff);
	printf("  bucket step           : %u (expected 16)\n", objMP->iBucketStep);
	printf("  bucket count          : %u (expected 64)\n", objMP->iBucketCount);
	printf("  bucket LUT            : %p\n", objMP->FSB_Lut);
	printf("  bucket[1] range       : %u..%u\n", objMP->FSB_Memory[0].MinLength, objMP->FSB_Memory[0].MaxLength);
	printf("  bucket[last] range    : %u..%u\n", objMP->FSB_Memory[objMP->iBucketCount - 1].MinLength, objMP->FSB_Memory[objMP->iBucketCount - 1].MaxLength);
	xrtMemPoolDestroy(objMP);

	printf("\nMemPool subject 2 : create custom-cutoff pool\n\n");
	objMP = xrtMemPoolCreate(256, XRT_OBJMODE_LOCAL);
	printf("  object                : %p\n", objMP);
	printf("  fallback cutoff       : %u (expected 256)\n", objMP->iFallbackCutoff);
	printf("  bucket count          : %u (expected 16)\n", objMP->iBucketCount);
	printf("  bucket[1] range       : %u..%u\n", objMP->FSB_Memory[0].MinLength, objMP->FSB_Memory[0].MaxLength);
	printf("  bucket[last] range    : %u..%u\n", objMP->FSB_Memory[objMP->iBucketCount - 1].MinLength, objMP->FSB_Memory[objMP->iBucketCount - 1].MaxLength);
	xrtMemPoolDestroy(objMP);

	printf("\nMemPool subject 3 : pooled and fallback allocations\n\n");
	objMP = xrtMemPoolCreate(256, XRT_OBJMODE_LOCAL);
	void* pSmall = xrtMemPoolAlloc(objMP, 32);
	void* pMedium = xrtMemPoolAlloc(objMP, 240);
	void* pFallback = xrtMemPoolAlloc(objMP, 257);
	printf("  alloc 32              : %p\n", pSmall);
	printf("  alloc 240             : %p\n", pMedium);
	printf("  alloc 257 (fallback)  : %p\n", pFallback);
	printf("  arrMMU.Count          : %d\n", objMP->arrMMU.Count);
	printf("  BigMM.Count           : %d\n", objMP->BigMM.Count);
	xrtMemPoolFree(objMP, pSmall);
	xrtMemPoolFree(objMP, pMedium);
	xrtMemPoolFree(objMP, pFallback);
	xrtMemPoolDestroy(objMP);

	printf("\nMemPool subject 4 : reusable struct allocations\n\n");
	typedef struct {
		int nID;
		char szName[32];
	} MP_TestRecord;

	objMP = xrtMemPoolCreate(0, XRT_OBJMODE_LOCAL);
	MP_TestRecord* pRecords[4];
	for (int i = 0; i < 4; i++) {
		pRecords[i] = (MP_TestRecord*)xrtMemPoolAlloc(objMP, sizeof(MP_TestRecord));
		pRecords[i]->nID = i + 1;
		sprintf(pRecords[i]->szName, "record_%d", i + 1);
		printf("  record[%d]            : %p id=%d name=%s\n", i, pRecords[i], pRecords[i]->nID, pRecords[i]->szName);
	}
	for (int i = 0; i < 4; i++) {
		xrtMemPoolFree(objMP, pRecords[i]);
	}
	xrtMemPoolDestroy(objMP);
}
