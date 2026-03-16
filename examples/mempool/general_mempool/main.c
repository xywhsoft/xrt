/**
 * @file main.c
 * @brief General memory-pool example for xrtMemPool APIs.
 *
 * This example demonstrates:
 * - creating a general-purpose memory pool
 * - allocating variable-size blocks
 * - using the default and custom fallback-cutoff configurations
 *
 * Build (Windows/TCC):
 *   tcc main.c -DXRT_NO_COROUTINE -lWs2_32 -lIPHLPAPI -o ../../bin/mempool_general_mempool.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

static void test_basic_mempool(void)
{
	printf("=== Test: Basic General Memory Pool ===\n");

	xmempool pPool = xrtMemPoolCreate(0);

	printf("Created general MemPool (cutoff=0 => default 16..1024 profile)\n");
	printf("\nAllocating various sizes:\n");

	char* pStr1 = (char*)xrtMemPoolAlloc(pPool, 32);
	char* pStr2 = (char*)xrtMemPoolAlloc(pPool, 64);
	char* pStr3 = (char*)xrtMemPoolAlloc(pPool, 128);
	int* pArr = (int*)xrtMemPoolAlloc(pPool, 10 * sizeof(int));

	strcpy(pStr1, "Hello from MemPool!");
	strcpy(pStr2, "This is a longer string stored in the pool");
	strcpy(pStr3, "Even longer string to demonstrate variable allocation");

	for (int i = 0; i < 10; i++) {
		pArr[i] = i * i;
	}

	printf("  pStr1 (32 bytes): \"%s\"\n", pStr1);
	printf("  pStr2 (64 bytes): \"%s\"\n", pStr2);
	printf("  pStr3 (128 bytes): \"%s\"\n", pStr3);
	printf("  pArr (10 ints):");
	for (int i = 0; i < 10; i++) {
		printf(" %d", pArr[i]);
	}
	printf("\n");

	xrtMemPoolFree(pPool, pStr1);
	xrtMemPoolFree(pPool, pStr2);
	xrtMemPoolFree(pPool, pStr3);
	xrtMemPoolFree(pPool, pArr);

	printf("Freed all allocations\n");

	xrtMemPoolDestroy(pPool);
}

static void test_struct_allocation(void)
{
	typedef struct {
		int nID;
		double dValue;
		char szName[32];
	} Item;

	printf("\n=== Test: Struct Allocation ===\n");

	xmempool pPool = xrtMemPoolCreate(0);
	Item* pItems[5];

	printf("Allocating 5 Items (structs)...\n");

	for (int i = 0; i < 5; i++) {
		pItems[i] = (Item*)xrtMemPoolAlloc(pPool, sizeof(Item));
		pItems[i]->nID = i + 1;
		pItems[i]->dValue = (i + 1) * 1.5;
		sprintf(pItems[i]->szName, "Item_%d", i + 1);
	}

	for (int i = 0; i < 5; i++) {
		printf(
			"  Item[%d]: ID=%d, Value=%.1f, Name=\"%s\"\n",
			i,
			pItems[i]->nID,
			pItems[i]->dValue,
			pItems[i]->szName
		);
	}

	for (int i = 0; i < 5; i++) {
		xrtMemPoolFree(pPool, pItems[i]);
	}

	xrtMemPoolDestroy(pPool);
}

static void test_custom_config(void)
{
	enum { CUSTOM_CUTOFF = 256 };

	printf("\n=== Test: Custom Configuration ===\n");

	xmempool pPool = xrtMemPoolCreate(CUSTOM_CUTOFF);

	printf("Created MemPool with custom fallback cutoff: %d\n", CUSTOM_CUTOFF);

	for (int i = 0; i < 10; i++) {
		int iSize = (i + 1) * 16;
		void* pData = xrtMemPoolAlloc(pPool, iSize);
		printf("  Allocated %d bytes at %p\n", iSize, pData);
		xrtMemPoolFree(pPool, pData);
	}

	xrtMemPoolDestroy(pPool);
}

int main(void)
{
	xrtInit();

	printf("========================================\n");
	printf("  General MemPool Example\n");
	printf("========================================\n");

	test_basic_mempool();
	test_struct_allocation();
	test_custom_config();

	printf("\n=== All tests completed! ===\n");

	xrtUnit();
	return 0;
}
