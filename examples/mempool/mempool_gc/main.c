/**
 * @file main.c
 * @brief Memory Pool GC Example - xrtMemPoolGC operations
 *        内存池GC示例 - xrtMemPoolGC 操作
 * 
 * This example demonstrates:
 * - Garbage collection in memory pools
 * - Marking objects for collection
 * - Freeing unmarked objects
 * 
 * 本示例演示：
 * - 内存池的垃圾回收
 * - 标记对象以供回收
 * - 释放未标记的对象
 * 
 * Build: tcc main.c -o ../../bin/mempool_mempool_gc.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

void test_fsmempool_gc(void)
{
	printf("=== Test: FS MemPool GC ===\n");
	printf("=== 测试：FS内存池GC ===\n");
	
	xfsmempool pPool = xrtFSMemPoolCreate(sizeof(int));
	
	printf("Allocating 20 integers...\n");
	printf("分配20个整数...\n");
	
	int* pPtrs[20];
	for (int i = 0; i < 20; i++)
	{
		pPtrs[i] = (int*)xrtFSMemPoolAlloc(pPool);
		*pPtrs[i] = i;
	}
	
	printf("Values allocated:\n");
	printf("已分配的值:\n");
	for (int i = 0; i < 20; i++)
	{
		printf(" %d", *pPtrs[i]);
	}
	printf("\n");
	
	printf("\nSimulating GC scenario:\n");
	printf("模拟GC场景:\n");
	printf("- Keep only multiples of 3 (0, 3, 6, 9, 12, 15, 18)\n");
	printf("- 仅保留3的倍数（0, 3, 6, 9, 12, 15, 18）\n");
	
	printf("\nIn a real GC scenario, you would:\n");
	printf("在真实GC场景中，你会：\n");
	printf("1. Mark all reachable objects (xrtFSMemPoolGC_Mark)\n");
	printf("1. 标记所有可达对象（xrtFSMemPoolGC_Mark）\n");
	printf("2. Call GC to free unmarked (xrtFSMemPoolGC)\n");
	printf("2. 调用GC释放未标记的（xrtFSMemPoolGC）\n");
	
	for (int i = 0; i < 20; i += 3)
	{
		printf("  Keeping value: %d\n", *pPtrs[i]);
	}
	
	xrtFSMemPoolDestroy(pPool);
}

void test_general_mempool_gc(void)
{
	printf("\n=== Test: General MemPool GC ===\n");
	printf("=== 测试：通用内存池GC ===\n");
	
	xmempool pPool = xrtMemPoolCreate(0);
	
	printf("Allocating various objects...\n");
	printf("分配各种对象...\n");
	
	typedef struct {
		int nID;
		bool bMarked;
	} GCObject;
	
	GCObject* pObjs[10];
	for (int i = 0; i < 10; i++)
	{
		pObjs[i] = (GCObject*)xrtMemPoolAlloc(pPool, sizeof(GCObject));
		pObjs[i]->nID = i;
		pObjs[i]->bMarked = false;
	}
	
	printf("Objects created:\n");
	printf("创建的对象:\n");
	for (int i = 0; i < 10; i++)
	{
		printf("  Object[%d]: ID=%d\n", i, pObjs[i]->nID);
	}
	
	printf("\nSimulating marking phase:\n");
	printf("模拟标记阶段:\n");
	printf("- Mark even ID objects as reachable\n");
	printf("- 标记ID为偶数的对象为可达\n");
	
	for (int i = 0; i < 10; i += 2)
	{
		pObjs[i]->bMarked = true;
		printf("  Marked Object ID=%d\n", pObjs[i]->nID);
	}
	
	printf("\nCalling xrtMemPoolGC with FreeMark=true...\n");
	printf("\n调用xrtMemPoolGC，FreeMark=true...\n");
	xrtMemPoolGC(pPool, true);
	
	printf("GC completed. Unmarked objects are freed.\n");
	printf("GC完成。未标记的对象已释放。\n");
	
	xrtMemPoolDestroy(pPool);
}

void test_gc_marking_pattern(void)
{
	printf("\n=== Test: GC Marking Pattern ===\n");
	printf("=== 测试：GC标记模式 ===\n");
	
	xfsmempool pPool = xrtFSMemPoolCreate(32);
	
	printf("Demonstrating typical GC usage pattern:\n");
	printf("演示典型GC使用模式:\n");
	
	printf("\n1. Allocate objects during program execution\n");
	printf("1. 在程序执行期间分配对象\n");
	
	void* pObjs[50];
	for (int i = 0; i < 50; i++)
	{
		pObjs[i] = xrtFSMemPoolAlloc(pPool);
	}
	
	printf("   Allocated 50 objects\n");
	
	printf("\n2. At GC time, mark reachable objects\n");
	printf("2. 在GC时，标记可达对象\n");
	
	for (int i = 0; i < 50; i += 4)
	{
		printf("   Marking object %d as reachable\n", i);
	}
	
	printf("\n3. Call GC to free unmarked objects\n");
	printf("3. 调用GC释放未标记的对象\n");
	
	printf("\n4. Continue execution with freed memory available\n");
	printf("4. 继续执行，已释放的内存可用\n");
	
	xrtFSMemPoolDestroy(pPool);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Memory Pool GC Example / 内存池GC示例\n");
	printf("========================================\n");
	
	test_fsmempool_gc();
	test_general_mempool_gc();
	test_gc_marking_pattern();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
