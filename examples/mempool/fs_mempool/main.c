/**
 * @file main.c
 * @brief FS Memory Pool Example - xrtFSMemPool operations
 *        FS内存池示例 - xrtFSMemPool 操作
 * 
 * This example demonstrates:
 * - Creating fixed-size memory pools
 * - Allocating and freeing fixed-size blocks
 * - Memory pool based on memunit (256 items max per unit)
 * 
 * 本示例演示：
 * - 创建固定大小内存池
 * - 分配和释放固定大小块
 * - 基于memunit的内存池（每单元最多256项）
 * 
 * Build: tcc main.c -o ../../bin/mempool_fs_mempool.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

typedef struct {
	int nID;
	char szData[64];
} Record;

void test_basic_fsmempool(void)
{
	printf("=== Test: Basic FS Memory Pool ===\n");
	printf("=== 测试：基本FS内存池 ===\n");
	
	xfsmempool pPool = xrtFSMemPoolCreate(sizeof(Record));
	
	printf("Created FS MemPool with item size: %u bytes\n", (uint32)sizeof(Record));
	printf("创建FS MemPool，项大小: %u 字节\n", (uint32)sizeof(Record));
	
	printf("\nAllocating 5 records...\n");
	printf("\n分配5条记录...\n");
	
	Record* pRecords[5];
	for (int i = 0; i < 5; i++)
	{
		pRecords[i] = (Record*)xrtFSMemPoolAlloc(pPool);
		pRecords[i]->nID = 1000 + i;
		sprintf(pRecords[i]->szData, "Record data #%d", i + 1);
	}
	
	for (int i = 0; i < 5; i++)
	{
		printf("  Record[%d]: ID=%d, Data=\"%s\"\n", 
			i, pRecords[i]->nID, pRecords[i]->szData);
	}
	
	xrtFSMemPoolFree(pPool, pRecords[1]);
	xrtFSMemPoolFree(pPool, pRecords[3]);
	
	printf("\nFreed records 1 and 3\n");
	printf("\n释放了记录1和3\n");
	
	xrtFSMemPoolDestroy(pPool);
}

void test_large_allocation(void)
{
	printf("\n=== Test: Large Number of Allocations ===\n");
	printf("=== 测试：大量分配 ===\n");
	
	xfsmempool pPool = xrtFSMemPoolCreate(sizeof(int));
	
	#define ALLOC_COUNT 300
	
	printf("Allocating %d integers...\n", ALLOC_COUNT);
	printf("分配%d个整数...\n", ALLOC_COUNT);
	
	int* pPtrs[ALLOC_COUNT];
	for (int i = 0; i < ALLOC_COUNT; i++)
	{
		pPtrs[i] = (int*)xrtFSMemPoolAlloc(pPool);
		if (pPtrs[i])
		{
			*pPtrs[i] = i;
		}
	}
	
	printf("Verifying first 5 and last 5 values:\n");
	printf("验证前5个和后5个值:\n");
	
	for (int i = 0; i < 5; i++)
	{
		printf("  pPtrs[%d] = %d\n", i, *pPtrs[i]);
	}
	printf("  ...\n");
	for (int i = ALLOC_COUNT - 5; i < ALLOC_COUNT; i++)
	{
		printf("  pPtrs[%d] = %d\n", i, *pPtrs[i]);
	}
	
	printf("\nFreeing all %d integers...\n", ALLOC_COUNT);
	printf("\n释放所有%d个整数...\n", ALLOC_COUNT);
	
	for (int i = 0; i < ALLOC_COUNT; i++)
	{
		xrtFSMemPoolFree(pPool, pPtrs[i]);
	}
	
	xrtFSMemPoolDestroy(pPool);
}

void test_gc(void)
{
	printf("\n=== Test: Garbage Collection ===\n");
	printf("=== 测试：垃圾回收 ===\n");
	
	xfsmempool pPool = xrtFSMemPoolCreate(sizeof(int));
	
	printf("Allocating 10 integers...\n");
	printf("分配10个整数...\n");
	
	int* pPtrs[10];
	for (int i = 0; i < 10; i++)
	{
		pPtrs[i] = (int*)xrtFSMemPoolAlloc(pPool);
		*pPtrs[i] = i;
	}
	
	printf("Only marking even indices as used (0, 2, 4, 6, 8)...\n");
	printf("仅标记偶数索引为使用中（0, 2, 4, 6, 8）...\n");
	
	for (int i = 0; i < 10; i += 2)
	{
		xrtFSMemPoolGC_Mark(pPtrs[i]);
	}
	
	printf("\nRunning GC with FreeMark=true...\n");
	printf("\n运行GC，FreeMark=true...\n");
	xrtFSMemPoolGC(pPool, true);
	
	xrtFSMemPoolDestroy(pPool);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  FS Memory Pool Example / FS内存池示例\n");
	printf("========================================\n");
	
	test_basic_fsmempool();
	test_large_allocation();
	test_gc();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
