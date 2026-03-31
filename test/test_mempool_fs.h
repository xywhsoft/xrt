typedef struct {
	int iVal;
	char sVal[32];
} MM256_Test_Struct, *MM256_Test_Object;


// MEMPOOLFS测试
void Test_MemPoolFS(xrtGlobalData* xCore)
{
	(void)xCore;

	printf("\n\n\n------------------------------------\n\n Fixed-Size Memory Pool Test:\n\n");

	printf("FSMemPool subject 1 : create object\n\n");
	xfsmempool objMM = xrtFSMemPoolCreate(sizeof(MM256_Test_Struct), XRT_OBJMODE_LOCAL);
	printf("  object                : %p\n", objMM);
	printf("  item length           : %d (expected %d)\n", objMM->ItemLength, (int)sizeof(MM256_Test_Struct));
	printf("  page alloc step       : %d (expected 64)\n", objMM->arrMMU.PageMMU.AllocStep);
	printf("  MMU count             : %d\n", objMM->arrMMU.Count);
	printf("  page count            : %d\n", objMM->arrMMU.PageMMU.Count);

	printf("\nFSMemPool subject 2 : allocate and inspect values\n\n");
	MM256_Test_Struct* ptr[12];
	for (int i = 0; i < 12; i++) {
		ptr[i] = xrtFSMemPoolAlloc(objMM);
		ptr[i]->iVal = i * 10;
		sprintf(ptr[i]->sVal, "String Field idx = %d", i);
		printf("  alloc[%d]             : %p value=%d text=%s\n", i, ptr[i], ptr[i]->iVal, ptr[i]->sVal);
	}
	printf("  MMU count             : %d\n", objMM->arrMMU.Count);
	printf("  page count            : %d\n", objMM->arrMMU.PageMMU.Count);
	printf("  live alloc count      : %d\n", objMM->arrMMU.PageMMU.AllocCount);

	printf("\nFSMemPool subject 3 : free and reuse\n\n");
	xrtFSMemPoolFree(objMM, ptr[3]);
	xrtFSMemPoolFree(objMM, ptr[5]);
	xrtFSMemPoolFree(objMM, ptr[7]);
	printf("  freed slots           : %p %p %p\n", ptr[3], ptr[5], ptr[7]);
	for (int i = 0; i < 3; i++) {
		MM256_Test_Struct* pReuse = xrtFSMemPoolAlloc(objMM);
		pReuse->iVal = 100 + i;
		sprintf(pReuse->sVal, "Reused Slot %d", i);
		printf("  reuse[%d]             : %p value=%d text=%s\n", i, pReuse, pReuse->iVal, pReuse->sVal);
	}

	printf("\nFSMemPool subject 4 : GC helpers\n\n");
	xfsmempool pGcPool = xrtFSMemPoolCreate(sizeof(int), XRT_OBJMODE_LOCAL);
	int* pValues[10];
	for (int i = 0; i < 10; i++) {
		pValues[i] = (int*)xrtFSMemPoolAlloc(pGcPool);
		*pValues[i] = i;
	}
	for (int i = 0; i < 10; i += 2) {
		xrtFSMemPoolGC_Mark(pValues[i]);
	}
	xrtFSMemPoolGC(pGcPool, TRUE);
	printf("  GC completed on marked-even pattern\n");
	xrtFSMemPoolDestroy(pGcPool);

	printf("\nFSMemPool subject 5 : destroy\n\n");
	xrtFSMemPoolDestroy(objMM);
	printf("  destroy completed\n");
}
