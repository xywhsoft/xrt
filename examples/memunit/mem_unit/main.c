/**
 * @file main.c
 * @brief Memory Unit Example - xrtMemUnit operations
 *        内存单元示例 - xrtMemUnit 操作
 * 
 * This example demonstrates:
 * - Creating and using memory units (256 fixed-size items)
 * - Allocating and freeing by index
 * - Garbage collection with marking
 * 
 * 本示例演示：
 * - 创建和使用内存单元（256个固定大小项）
 * - 按索引分配和释放
 * - 标记式垃圾回收
 * 
 * Build: tcc main.c -o ../../bin/memunit_mem_unit.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

typedef struct {
	int nX;
	int nY;
} Point;

void test_basic_memunit(void)
{
	printf("=== Test: Basic Memory Unit ===\n");
	printf("=== 测试：基本内存单元 ===\n");
	
	xmemunit pUnit = xrtMemUnitCreate(sizeof(Point));
	
	printf("Created MemUnit with item size: %u bytes\n", (uint32)sizeof(Point));
	printf("创建MemUnit，项大小: %u 字节\n", (uint32)sizeof(Point));
	
	printf("\nAllocating 5 points...\n");
	printf("\n分配5个点...\n");
	
	Point* pPoints[5];
	for (int i = 0; i < 5; i++)
	{
		pPoints[i] = (Point*)xrtMemUnitAlloc(pUnit);
		pPoints[i]->nX = i * 10;
		pPoints[i]->nY = i * 20;
	}
	
	for (int i = 0; i < 5; i++)
	{
		printf("  Point[%d]: (%d, %d)\n", i, pPoints[i]->nX, pPoints[i]->nY);
	}
	
	xrtMemUnitFree(pUnit, pPoints[1]);
	xrtMemUnitFree(pUnit, pPoints[3]);
	
	printf("\nFreed points at index 1 and 3\n");
	printf("\n释放了索引1和3的点\n");
	
	xrtFree(pUnit);
}

void test_alloc_by_index(void)
{
	printf("\n=== Test: Allocation by Index ===\n");
	printf("=== 测试：按索引分配 ===\n");
	
	xmemunit pUnit = xrtMemUnitCreate(sizeof(int));
	
	printf("Memory unit has 256 slots (index 0-255)\n");
	printf("内存单元有256个槽位（索引0-255）\n");
	
	printf("\nAllocating and checking usage bitmap:\n");
	printf("\n分配并检查使用位图:\n");
	
	for (int i = 0; i < 10; i++)
	{
		int* p = (int*)xrtMemUnitAlloc(pUnit);
		if (p)
		{
			*p = i * 100;
			printf("  Allocated slot, value=%d\n", *p);
		}
	}
	
	printf("\nFreeing some slots...\n");
	printf("\n释放一些槽位...\n");
	
	uint8 idx1 = 2;
	uint8 idx2 = 5;
	xrtMemUnitFreeIdx(pUnit, idx1);
	xrtMemUnitFreeIdx(pUnit, idx2);
	printf("  Freed indices: %d, %d\n", idx1, idx2);
	
	xrtFree(pUnit);
}

void test_garbage_collection(void)
{
	printf("\n=== Test: Garbage Collection ===\n");
	printf("=== 测试：垃圾回收 ===\n");
	
	xmemunit pUnit = xrtMemUnitCreate(sizeof(int));
	
	printf("Allocating 10 integers...\n");
	printf("分配10个整数...\n");
	
	int* pPtrs[10];
	for (int i = 0; i < 10; i++)
	{
		pPtrs[i] = (int*)xrtMemUnitAlloc(pUnit);
		*pPtrs[i] = i;
	}
	
	printf("Marking objects 0, 2, 4, 6, 8 as used (to keep)...\n");
	printf("标记对象0, 2, 4, 6, 8为使用中（保留）...\n");
	
	for (int i = 0; i < 10; i += 2)
	{
		xrtMemUnitGC_Mark(pPtrs[i]);
		printf("  Marked object at index %d\n", i);
	}
	
	printf("\nRunning GC with FreeMark=true...\n");
	printf("\n运行GC，FreeMark=true...\n");
	int nFreed = xrtMemUnitGC(pUnit, true);
	
	printf("GC freed %d items\n", nFreed);
	printf("GC释放了%d个项\n", nFreed);
	
	xrtFree(pUnit);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Memory Unit Example / 内存单元示例\n");
	printf("========================================\n");
	
	test_basic_memunit();
	test_alloc_by_index();
	test_garbage_collection();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
