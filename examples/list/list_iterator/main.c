/**
 * @file main.c
 * @brief List Iterator Example - xrtListIterBegin/Next
 *        列表迭代器示例 - xrtListIterBegin/Next
 * 
 * This example demonstrates:
 * - Iterating through list items
 * - Using iterator pattern for traversal
 * 
 * 本示例演示：
 * - 遍历列表项目
 * - 使用迭代器模式进行遍历
 * 
 * Build: tcc main.c -o ../../bin/list_list_iterator.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

typedef struct {
	int64 nIndex;
	double dValue;
} DataItem;

void test_list_iterator(void)
{
	printf("=== Test: List Iterator ===\n");
	printf("=== 测试：列表迭代器 ===\n");
	
	xlist pList = xrtListCreate(sizeof(DataItem), XRT_OBJMODE_LOCAL);
	
	printf("Setting values at various indices...\n");
	printf("在各种索引设置值...\n");
	
	int64 indices[] = {0, 5, 10, 15, 20, 25, 30};
	int nCount = sizeof(indices) / sizeof(indices[0]);
	
	for (int i = 0; i < nCount; i++)
	{
		DataItem* pItem = (DataItem*)xrtListSet(pList, indices[i], NULL);
		pItem->nIndex = indices[i];
		pItem->dValue = indices[i] * 1.5;
	}
	
	printf("List count: %u\n", xrtListCount(pList));
	printf("列表计数: %u\n", xrtListCount(pList));
	
	printf("\nIterating through list:\n");
	printf("\n遍历列表:\n");
	
	xrtListIterBegin(pList);
	DataItem* pItem;
	while ((pItem = (DataItem*)xrtListIterNext(pList)) != NULL)
	{
		printf("  Index=%lld, Value=%.1f\n", (long long)pItem->nIndex, pItem->dValue);
	}
	
	xrtListDestroy(pList);
}

void test_walk_function(void)
{
	printf("\n=== Test: List Walk Function ===\n");
	printf("=== 测试：列表遍历函数 ===\n");
	
	xlist pList = xrtListCreate(sizeof(int), XRT_OBJMODE_LOCAL);
	
	printf("Setting values...\n");
	printf("设置值...\n");
	
	for (int i = 0; i < 10; i++)
	{
		int* pVal = (int*)xrtListSet(pList, i * 3, NULL);
		*pVal = i * 100;
	}
	
	printf("List count: %u\n", xrtListCount(pList));
	printf("列表计数: %u\n", xrtListCount(pList));
	
	printf("\nUsing xrtListIterBegin/Next to traverse:\n");
	printf("\n使用xrtListIterBegin/Next遍历:\n");
	
	xrtListIterBegin(pList);
	int* pVal;
	int nCount = 0;
	while ((pVal = (int*)xrtListIterNext(pList)) != NULL)
	{
		printf("  Value: %d\n", *pVal);
		nCount++;
	}
	printf("  Total items iterated: %d\n", nCount);
	
	xrtListDestroy(pList);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  List Iterator Example / 列表迭代器示例\n");
	printf("========================================\n");
	
	test_list_iterator();
	test_walk_function();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
