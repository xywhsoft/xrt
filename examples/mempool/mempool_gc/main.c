/**
 * @file main.c
 * @brief Memory-pool GC helper example for xrtFSMemPoolGC/xrtMemPoolGC APIs.
 *
 * This example demonstrates:
 * - marking live objects before a GC pass
 * - reclaiming unmarked objects in fixed-size and general pools
 * - a typical mark/sweep usage pattern
 *
 * Build (Windows/TCC):
 *   tcc main.c -DXRT_NO_COROUTINE -lWs2_32 -lIPHLPAPI -o ../../bin/mempool_mempool_gc.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

typedef struct {
	int nID;
	bool bMarked;
} GCObject;

static void test_fsmempool_gc(void)
{
	printf("=== Test: FS MemPool GC ===\n");

	xfsmempool pPool = xrtFSMemPoolCreate(sizeof(int));
	int* pPtrs[20];

	printf("Allocating 20 integers...\n");
	for (int i = 0; i < 20; i++) {
		pPtrs[i] = (int*)xrtFSMemPoolAlloc(pPool);
		*pPtrs[i] = i;
	}

	printf("Keeping only multiples of 3 alive:\n");
	for (int i = 0; i < 20; i += 3) {
		printf("  keep %d\n", *pPtrs[i]);
		xrtFSMemPoolGC_Mark(pPtrs[i]);
	}

	printf("Calling xrtFSMemPoolGC(..., true)\n");
	xrtFSMemPoolGC(pPool, true);

	xrtFSMemPoolDestroy(pPool);
}

static void test_general_mempool_gc(void)
{
	printf("\n=== Test: General MemPool GC ===\n");

	xmempool pPool = xrtMemPoolCreate(0);
	GCObject* pObjs[10];

	printf("Allocating 10 GCObject instances...\n");
	for (int i = 0; i < 10; i++) {
		pObjs[i] = (GCObject*)xrtMemPoolAlloc(pPool, sizeof(GCObject));
		pObjs[i]->nID = i;
		pObjs[i]->bMarked = false;
	}

	printf("Marking even object ids as live:\n");
	for (int i = 0; i < 10; i += 2) {
		pObjs[i]->bMarked = true;
		printf("  mark object id=%d\n", pObjs[i]->nID);
		xrtMemPoolGC_Mark(pObjs[i]);
	}

	printf("Calling xrtMemPoolGC(..., true)\n");
	xrtMemPoolGC(pPool, true);

	xrtMemPoolDestroy(pPool);
}

static void test_gc_marking_pattern(void)
{
	printf("\n=== Test: GC Marking Pattern ===\n");

	xfsmempool pPool = xrtFSMemPoolCreate(32);
	void* pObjs[50];

	printf("Allocating 50 objects...\n");
	for (int i = 0; i < 50; i++) {
		pObjs[i] = xrtFSMemPoolAlloc(pPool);
	}

	printf("Mark every 4th object as reachable:\n");
	for (int i = 0; i < 50; i += 4) {
		printf("  mark object %d\n", i);
		xrtFSMemPoolGC_Mark(pObjs[i]);
	}

	printf("Reclaim all unmarked objects\n");
	xrtFSMemPoolGC(pPool, true);

	xrtFSMemPoolDestroy(pPool);
}

int main(void)
{
	xrtInit();

	printf("========================================\n");
	printf("  Memory Pool GC Example\n");
	printf("========================================\n");

	test_fsmempool_gc();
	test_general_mempool_gc();
	test_gc_marking_pattern();

	printf("\n=== All tests completed! ===\n");

	xrtUnit();
	return 0;
}
