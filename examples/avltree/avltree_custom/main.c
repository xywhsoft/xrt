/**
 * @file main.c
 * @brief AVL Tree Custom Compare Example - custom comparison function
 *        AVL树自定义比较示例 - 自定义比较函数
 * 
 * This example demonstrates:
 * - Using custom comparison functions
 * - String key comparison
 * - Descending order comparison
 * 
 * 本示例演示：
 * - 使用自定义比较函数
 * - 字符串键比较
 * - 降序比较
 * 
 * Build: tcc main.c -o ../../bin/avltree_avltree_custom.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

typedef struct {
	char szKey[32];
	int nValue;
} StringItem;

int compare_string_items(ptr pA, ptr pB)
{
	StringItem* pItemA = (StringItem*)pA;
	StringItem* pItemB = (StringItem*)pB;
	return strcmp(pItemA->szKey, pItemB->szKey);
}

typedef struct {
	int nKey;
	double dValue;
} DescItem;

int compare_descending(ptr pA, ptr pB)
{
	DescItem* pItemA = (DescItem*)pA;
	DescItem* pItemB = (DescItem*)pB;
	return pItemB->nKey - pItemA->nKey;
}

int compare_int_items(ptr pA, ptr pB)
{
	int* pItemA = (int*)pA;
	int* pItemB = (int*)pB;
	return *pItemA - *pItemB;
}

void test_string_keys(void)
{
	printf("=== Test: String Key Comparison ===\n");
	printf("=== 测试：字符串键比较 ===\n");
	
	xavltree pTree = xrtAVLTreeCreate(sizeof(StringItem), compare_string_items);
	
	StringItem items[] = {
		{"banana", 1}, {"apple", 2}, {"cherry", 3}, {"date", 4}
	};
	int nCount = sizeof(items) / sizeof(items[0]);
	
	printf("Inserting items with string keys...\n");
	printf("插入带字符串键的项...\n");
	
	for (int i = 0; i < nCount; i++)
	{
		bool bNew = false;
		StringItem* pNode = (StringItem*)xrtAVLTreeInsert(pTree, &items[i], &bNew);
		(void)pNode;
	}
	
	printf("\nIterating (alphabetical order):\n");
	printf("\n遍历（字母顺序）:\n");
	
	xrtAVLTreeIterBegin(pTree);
	StringItem* pItem;
	while ((pItem = (StringItem*)xrtAVLTreeIterNext(pTree)) != NULL)
	{
		printf("  %s = %d\n", pItem->szKey, pItem->nValue);
	}
	
	xrtAVLTreeDestroy(pTree);
}

void test_case_sensitivity(void)
{
	printf("\n=== Test: Case-Sensitive Comparison ===\n");
	printf("=== 测试：区分大小写比较 ===\n");
	
	xavltree pTree = xrtAVLTreeCreate(sizeof(StringItem), compare_string_items);
	
	StringItem items[] = {
		{"Apple", 1}, {"apple", 2}, {"APPLE", 3}
	};
	int nCount = sizeof(items) / sizeof(items[0]);
	
	printf("Inserting: Apple, apple, APPLE (case-sensitive)\n");
	printf("插入: Apple, apple, APPLE（区分大小写）\n");
	
	for (int i = 0; i < nCount; i++)
	{
		bool bNew = false;
		(void)xrtAVLTreeInsert(pTree, &items[i], &bNew);
		printf("  Inserted: %s (new=%s)\n", items[i].szKey, bNew ? "yes" : "no");
	}
	
	xrtAVLTreeDestroy(pTree);
}

void test_descending_order(void)
{
	printf("\n=== Test: Descending Order Comparison ===\n");
	printf("=== 测试：降序比较 ===\n");
	
	xavltree pTree = xrtAVLTreeCreate(sizeof(DescItem), compare_descending);
	
	printf("Inserting: 10, 30, 20, 50, 40\n");
	
	int keys[] = {10, 30, 20, 50, 40};
	for (int i = 0; i < 5; i++)
	{
		DescItem item;
		item.nKey = keys[i];
		item.dValue = keys[i] * 1.5;
		bool bNew = false;
		(void)xrtAVLTreeInsert(pTree, &item, &bNew);
	}
	
	printf("\nIterating (descending order):\n");
	printf("\n遍历（降序）:\n");
	
	xrtAVLTreeIterBegin(pTree);
	DescItem* pItem;
	while ((pItem = (DescItem*)xrtAVLTreeIterNext(pTree)) != NULL)
	{
		printf("  Key=%d, Value=%.1f\n", pItem->nKey, pItem->dValue);
	}
	
	xrtAVLTreeDestroy(pTree);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  AVL Tree Custom Compare / AVL树自定义比较\n");
	printf("========================================\n");
	
	test_string_keys();
	test_case_sensitivity();
	test_descending_order();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
