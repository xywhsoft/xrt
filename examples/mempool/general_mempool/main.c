/**
 * @file main.c
 * @brief General Memory Pool Example - xrtMemPool operations
 *        通用内存池示例 - xrtMemPool 操作
 * 
 * This example demonstrates:
 * - Creating general-purpose memory pools
 * - Allocating variable-size blocks
 * - Memory pool with custom configurations
 * 
 * 本示例演示：
 * - 创建通用内存池
 * - 分配可变大小块
 * - 自定义配置的内存池
 * 
 * Build: tcc main.c -o ../../bin/mempool_general_mempool.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

void test_basic_mempool(void)
{
	printf("=== Test: Basic General Memory Pool ===\n");
	printf("=== 测试：基本通用内存池 ===\n");
	
	xmempool pPool = xrtMemPoolCreate(0);
	
	printf("Created general MemPool (custom=0, default config)\n");
	printf("创建通用MemPool（custom=0，默认配置）\n");
	
	printf("\nAllocating various sizes:\n");
	printf("\n分配各种大小:\n");
	
	char* pStr1 = (char*)xrtMemPoolAlloc(pPool, 32);
	char* pStr2 = (char*)xrtMemPoolAlloc(pPool, 64);
	char* pStr3 = (char*)xrtMemPoolAlloc(pPool, 128);
	int* pArr = (int*)xrtMemPoolAlloc(pPool, 10 * sizeof(int));
	
	strcpy(pStr1, "Hello from MemPool!");
	strcpy(pStr2, "This is a longer string stored in the pool");
	strcpy(pStr3, "Even longer string to demonstrate variable allocation");
	
	for (int i = 0; i < 10; i++)
	{
		pArr[i] = i * i;
	}
	
	printf("  pStr1 (32 bytes): \"%s\"\n", pStr1);
	printf("  pStr2 (64 bytes): \"%s\"\n", pStr2);
	printf("  pStr3 (128 bytes): \"%s\"\n", pStr3);
	printf("  pArr (10 ints):");
	for (int i = 0; i < 10; i++)
	{
		printf(" %d", pArr[i]);
	}
	printf("\n");
	
	xrtMemPoolFree(pPool, pStr1);
	xrtMemPoolFree(pPool, pStr2);
	xrtMemPoolFree(pPool, pStr3);
	xrtMemPoolFree(pPool, pArr);
	
	printf("Freed all allocations\n");
	printf("释放所有分配\n");
	
	xrtMemPoolDestroy(pPool);
}

void test_struct_allocation(void)
{
	printf("\n=== Test: Struct Allocation ===\n");
	printf("=== 测试：结构体分配 ===\n");
	
	xmempool pPool = xrtMemPoolCreate(0);
	
	typedef struct {
		int nID;
		double dValue;
		char szName[32];
	} Item;
	
	printf("Allocating 5 Items (structs)...\n");
	printf("分配5个Item（结构体）...\n");
	
	Item* pItems[5];
	for (int i = 0; i < 5; i++)
	{
		pItems[i] = (Item*)xrtMemPoolAlloc(pPool, sizeof(Item));
		pItems[i]->nID = i + 1;
		pItems[i]->dValue = (i + 1) * 1.5;
		sprintf(pItems[i]->szName, "Item_%d", i + 1);
	}
	
	for (int i = 0; i < 5; i++)
	{
		printf("  Item[%d]: ID=%d, Value=%.1f, Name=\"%s\"\n",
			i, pItems[i]->nID, pItems[i]->dValue, pItems[i]->szName);
	}
	
	for (int i = 0; i < 5; i++)
	{
		xrtMemPoolFree(pPool, pItems[i]);
	}
	
	xrtMemPoolDestroy(pPool);
}

void test_custom_config(void)
{
	printf("\n=== Test: Custom Configuration ===\n");
	printf("=== 测试：自定义配置 ===\n");
	
	#define CUSTOM_CONFIG 1
	
	xmempool pPool = xrtMemPoolCreate(CUSTOM_CONFIG);
	
	printf("Created MemPool with custom config: %d\n", CUSTOM_CONFIG);
	printf("创建MemPool，自定义配置: %d\n", CUSTOM_CONFIG);
	
	for (int i = 0; i < 10; i++)
	{
		int size = (i + 1) * 16;
		void* p = xrtMemPoolAlloc(pPool, size);
		printf("  Allocated %d bytes at %p\n", size, p);
		xrtMemPoolFree(pPool, p);
	}
	
	xrtMemPoolDestroy(pPool);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  General MemPool Example / 通用内存池示例\n");
	printf("========================================\n");
	
	test_basic_mempool();
	test_struct_allocation();
	test_custom_config();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
