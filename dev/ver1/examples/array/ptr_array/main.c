/**
 * @file main.c
 * @brief Pointer Array Example - xparray operations
 *        指针数组示例 - xparray 操作
 * 
 * This example demonstrates:
 * - Creating and destroying pointer arrays
 * - Appending and inserting pointers
 * - Getting and setting pointer values
 * - Using AddAlt for alternate append
 * 
 * 本示例演示：
 * - 创建和销毁指针数组
 * - 追加和插入指针
 * - 获取和设置指针值
 * - 使用 AddAlt 进行交替追加
 * 
 * Build: tcc main.c -o ../../bin/array_ptr_array.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

void test_basic_operations(void)
{
	printf("=== Test: Basic Pointer Array Operations ===\n");
	printf("=== 测试：基本指针数组操作 ===\n");
	
	xparray pArr = xrtPtrArrayCreate(XRT_OBJMODE_LOCAL);
	
	int a = 10, b = 20, c = 30, d = 40;
	
	xrtPtrArrayAppend(pArr, &a);
	xrtPtrArrayAppend(pArr, &b);
	xrtPtrArrayAppend(pArr, &c);
	
	printf("Appended 3 pointers\n");
	printf("追加了3个指针\n");
	
	for (uint32 i = 1; i <= pArr->Count; i++)
	{
		int* pVal = (int*)xrtPtrArrayGet(pArr, i);
		printf("  [%d] = %d\n", i, *pVal);
	}
	
	xrtPtrArrayDestroy(pArr);
}

void test_insert_and_set(void)
{
	printf("\n=== Test: Insert and Set Operations ===\n");
	printf("=== 测试：插入和设置操作 ===\n");
	
	xparray pArr = xrtPtrArrayCreate(XRT_OBJMODE_LOCAL);
	
	int a = 100, b = 200, c = 300, d = 400;
	
	xrtPtrArrayAppend(pArr, &a);
	xrtPtrArrayAppend(pArr, &b);
	xrtPtrArrayAppend(pArr, &c);
	
	printf("Before insert:\n");
	printf("插入前:\n");
	for (uint32 i = 1; i <= pArr->Count; i++)
	{
		int* pVal = (int*)xrtPtrArrayGet(pArr, i);
		printf("  [%d] = %d\n", i, *pVal);
	}
	
	xrtPtrArrayInsert(pArr, 2, &d);
	
	printf("After inserting d at position 2:\n");
	printf("在位置2插入d后:\n");
	for (uint32 i = 1; i <= pArr->Count; i++)
	{
		int* pVal = (int*)xrtPtrArrayGet(pArr, i);
		printf("  [%d] = %d\n", i, *pVal);
	}
	
	int e = 999;
	xrtPtrArraySet(pArr, 1, &e);
	
	printf("After setting position 1 to 999:\n");
	printf("将位置1设置为999后:\n");
	for (uint32 i = 1; i <= pArr->Count; i++)
	{
		int* pVal = (int*)xrtPtrArrayGet(pArr, i);
		printf("  [%d] = %d\n", i, *pVal);
	}
	
	xrtPtrArrayDestroy(pArr);
}

void test_add_alt(void)
{
	printf("\n=== Test: AddAlt (Alternate Append) ===\n");
	printf("=== 测试：AddAlt（交替追加）===\n");
	
	xparray pArr = xrtPtrArrayCreate(XRT_OBJMODE_LOCAL);
	
	int a = 1, b = 2, c = 3, d = 4, e = 5;
	
	uint32 pos1 = xrtPtrArrayAddAlt(pArr, &a);
	uint32 pos2 = xrtPtrArrayAddAlt(pArr, &b);
	uint32 pos3 = xrtPtrArrayAddAlt(pArr, &c);
	uint32 pos4 = xrtPtrArrayAddAlt(pArr, &d);
	uint32 pos5 = xrtPtrArrayAddAlt(pArr, &e);
	
	printf("AddAlt returned positions: %u, %u, %u, %u, %u\n", pos1, pos2, pos3, pos4, pos5);
	printf("AddAlt 返回的位置: %u, %u, %u, %u, %u\n", pos1, pos2, pos3, pos4, pos5);
	
	for (uint32 i = 1; i <= pArr->Count; i++)
	{
		int* pVal = (int*)xrtPtrArrayGet(pArr, i);
		printf("  [%u] = %d\n", i, *pVal);
	}
	
	xrtPtrArrayDestroy(pArr);
}

void test_remove_and_swap(void)
{
	printf("\n=== Test: Remove and Swap Operations ===\n");
	printf("=== 测试：移除和交换操作 ===\n");
	
	xparray pArr = xrtPtrArrayCreate(XRT_OBJMODE_LOCAL);
	
	int vals[] = {10, 20, 30, 40, 50};
	for (int i = 0; i < 5; i++)
	{
		xrtPtrArrayAppend(pArr, &vals[i]);
	}
	
	printf("Original array:\n");
	printf("原始数组:\n");
	for (uint32 i = 1; i <= pArr->Count; i++)
	{
		int* pVal = (int*)xrtPtrArrayGet(pArr, i);
		printf("  [%u] = %d\n", i, *pVal);
	}
	
	xrtPtrArraySwap(pArr, 1, 5);
	printf("After swapping positions 1 and 5:\n");
	printf("交换位置1和5后:\n");
	for (uint32 i = 1; i <= pArr->Count; i++)
	{
		int* pVal = (int*)xrtPtrArrayGet(pArr, i);
		printf("  [%u] = %d\n", i, *pVal);
	}
	
	xrtPtrArrayRemove(pArr, 2, 1);
	printf("After removing element at position 2:\n");
	printf("移除位置2的元素后:\n");
	for (uint32 i = 1; i <= pArr->Count; i++)
	{
		int* pVal = (int*)xrtPtrArrayGet(pArr, i);
		printf("  [%u] = %d\n", i, *pVal);
	}
	
	xrtPtrArrayDestroy(pArr);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Pointer Array Example / 指针数组示例\n");
	printf("========================================\n");
	
	test_basic_operations();
	test_insert_and_set();
	test_add_alt();
	test_remove_and_swap();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
