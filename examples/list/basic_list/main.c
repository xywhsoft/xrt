/**
 * @file main.c
 * @brief Basic List Example - xrtList operations
 *        基本列表示例 - xrtList 操作
 * 
 * This example demonstrates:
 * - Creating and destroying lists
 * - Setting and getting values by index
 * - Checking index existence
 * - Removing items
 * 
 * 本示例演示：
 * - 创建和销毁列表
 * - 按索引设置和获取值
 * - 检查索引是否存在
 * - 删除项目
 * 
 * Build: tcc main.c -o ../../bin/list_basic_list.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

typedef struct {
	int nID;
	char szName[32];
} ListItem;

void test_basic_list(void)
{
	printf("=== Test: Basic List Operations ===\n");
	printf("=== 测试：基本列表操作 ===\n");
	
	xlist pList = xrtListCreate(sizeof(ListItem));
	
	printf("Created list with ListItem value size: %u\n", (uint32)sizeof(ListItem));
	printf("创建列表，ListItem值大小: %u\n", (uint32)sizeof(ListItem));
	
	printf("\nSetting values by index...\n");
	printf("\n按索引设置值...\n");
	
	for (int i = 0; i < 5; i++)
	{
		ListItem* pItem = (ListItem*)xrtListSet(pList, i, NULL);
		pItem->nID = i + 1;
		sprintf(pItem->szName, "Item_%d", i + 1);
		printf("  Set index %d: ID=%d, Name=%s\n", i, pItem->nID, pItem->szName);
	}
	
	printf("\nList count: %u\n", xrtListCount(pList));
	printf("列表计数: %u\n", xrtListCount(pList));
	
	printf("\nGetting values:\n");
	printf("\n获取值:\n");
	
	for (int i = 0; i < 5; i++)
	{
		ListItem* pItem = (ListItem*)xrtListGet(pList, i);
		if (pItem)
			printf("  Index %d: ID=%d, Name=%s\n", i, pItem->nID, pItem->szName);
	}
	
	printf("\nChecking index existence:\n");
	printf("\n检查索引是否存在:\n");
	printf("  Index 2 exists: %s\n", xrtListExists(pList, 2) ? "yes" : "no");
	printf("  Index 100 exists: %s\n", xrtListExists(pList, 100) ? "yes" : "no");
	
	printf("\nRemoving index 2...\n");
	printf("\n删除索引2...\n");
	xrtListRemove(pList, 2);
	printf("  Index 2 exists after removal: %s\n", xrtListExists(pList, 2) ? "yes" : "no");
	printf("  List count: %u\n", xrtListCount(pList));
	
	xrtListDestroy(pList);
}

void test_sparse_list(void)
{
	printf("\n=== Test: Sparse List (Non-contiguous indices) ===\n");
	printf("=== 测试：稀疏列表（非连续索引）===\n");
	
	xlist pList = xrtListCreate(sizeof(int));
	
	printf("Setting values at non-contiguous indices...\n");
	printf("在非连续索引设置值...\n");
	
	int64 indices[] = {0, 10, 100, 50, 25};
	int nCount = sizeof(indices) / sizeof(indices[0]);
	
	for (int i = 0; i < nCount; i++)
	{
		int* pVal = (int*)xrtListSet(pList, indices[i], NULL);
		*pVal = indices[i] * 10;
		printf("  Set index %lld = %d\n", (long long)indices[i], *pVal);
	}
	
	printf("\nList count: %u\n", xrtListCount(pList));
	printf("列表计数: %u\n", xrtListCount(pList));
	
	printf("\nGetting values:\n");
	printf("\n获取值:\n");
	
	for (int i = 0; i < nCount; i++)
	{
		int* pVal = (int*)xrtListGet(pList, indices[i]);
		if (pVal)
			printf("  Index %lld = %d\n", (long long)indices[i], *pVal);
	}
	
	xrtListDestroy(pList);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Basic List Example / 基本列表示例\n");
	printf("========================================\n");
	
	test_basic_list();
	test_sparse_list();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
