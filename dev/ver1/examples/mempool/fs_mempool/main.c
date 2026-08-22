/**
 * @file main.c
 * @brief Fixed-size memory-pool example for xrtFSMemPool APIs.
 *
 * This example demonstrates:
 * - creating a fixed-size memory pool
 * - allocating and freeing fixed-size records
 * - scaling across more than one internal memunit page
 * - using the optional mark/sweep helpers
 *
 * Build (Windows/TCC):
 *   tcc main.c -DXRT_NO_COROUTINE -lWs2_32 -lIPHLPAPI -o ../../bin/mempool_fs_mempool.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

typedef struct {
	int nID;
	char szData[64];
} Record;

static void test_basic_fsmempool(void)
{
	printf("=== Test: Basic FS Memory Pool ===\n");

	xfsmempool pPool = xrtFSMemPoolCreate(sizeof(Record), XRT_OBJMODE_LOCAL);
	Record* pRecords[5];

	printf("Created FS MemPool with item size: %u bytes\n", (uint32)sizeof(Record));
	printf("\nAllocating 5 records...\n");

	for (int i = 0; i < 5; i++) {
		pRecords[i] = (Record*)xrtFSMemPoolAlloc(pPool);
		pRecords[i]->nID = 1000 + i;
		sprintf(pRecords[i]->szData, "Record data #%d", i + 1);
	}

	for (int i = 0; i < 5; i++) {
		printf("  Record[%d]: ID=%d, Data=\"%s\"\n", i, pRecords[i]->nID, pRecords[i]->szData);
	}

	xrtFSMemPoolFree(pPool, pRecords[1]);
	xrtFSMemPoolFree(pPool, pRecords[3]);

	printf("\nFreed records 1 and 3\n");

	xrtFSMemPoolDestroy(pPool);
}

static void test_large_allocation(void)
{
	enum { ALLOC_COUNT = 300 };

	printf("\n=== Test: Large Number of Allocations ===\n");

	xfsmempool pPool = xrtFSMemPoolCreate(sizeof(int), XRT_OBJMODE_LOCAL);
	int* pPtrs[ALLOC_COUNT];

	printf("Allocating %d integers...\n", ALLOC_COUNT);

	for (int i = 0; i < ALLOC_COUNT; i++) {
		pPtrs[i] = (int*)xrtFSMemPoolAlloc(pPool);
		if (pPtrs[i]) {
			*pPtrs[i] = i;
		}
	}

	printf("Verifying first 5 and last 5 values:\n");
	for (int i = 0; i < 5; i++) {
		printf("  pPtrs[%d] = %d\n", i, *pPtrs[i]);
	}
	printf("  ...\n");
	for (int i = ALLOC_COUNT - 5; i < ALLOC_COUNT; i++) {
		printf("  pPtrs[%d] = %d\n", i, *pPtrs[i]);
	}

	printf("\nFreeing all %d integers...\n", ALLOC_COUNT);
	for (int i = 0; i < ALLOC_COUNT; i++) {
		xrtFSMemPoolFree(pPool, pPtrs[i]);
	}

	xrtFSMemPoolDestroy(pPool);
}

static void test_gc_helpers(void)
{
	printf("\n=== Test: Garbage Collection Helpers ===\n");

	xfsmempool pPool = xrtFSMemPoolCreate(sizeof(int), XRT_OBJMODE_LOCAL);
	int* pPtrs[10];

	printf("Allocating 10 integers...\n");

	for (int i = 0; i < 10; i++) {
		pPtrs[i] = (int*)xrtFSMemPoolAlloc(pPool);
		*pPtrs[i] = i;
	}

	printf("Marking even indices as live: 0, 2, 4, 6, 8\n");
	for (int i = 0; i < 10; i += 2) {
		xrtFSMemPoolGC_Mark(pPtrs[i]);
	}

	printf("Running xrtFSMemPoolGC(..., true)\n");
	xrtFSMemPoolGC(pPool, true);

	xrtFSMemPoolDestroy(pPool);
}

int main(void)
{
	xrtInit();

	printf("========================================\n");
	printf("  Fixed-Size Memory Pool Example\n");
	printf("========================================\n");

	test_basic_fsmempool();
	test_large_allocation();
	test_gc_helpers();

	printf("\n=== All tests completed! ===\n");

	xrtUnit();
	return 0;
}
