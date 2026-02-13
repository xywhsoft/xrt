/**
 * @file main.c
 * @brief AVL Tree Iterator Example - xrtAVLTreeIterBegin/Next
 *        AVL树迭代器示例 - xrtAVLTreeIterBegin/Next
 * 
 * This example demonstrates:
 * - Iterating through AVL tree nodes
 * - Using iterator pattern for traversal
 * 
 * 本示例演示：
 * - 遍历AVL树节点
 * - 使用迭代器模式进行遍历
 * 
 * Build: tcc main.c -o ../../bin/avltree_avltree_iterator.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

typedef struct {
	int nKey;
	char szName[24];
} Item;

int compare_items(ptr pA, ptr pB)
{
	Item* pItemA = (Item*)pA;
	Item* pItemB = (Item*)pB;
	return pItemA->nKey - pItemB->nKey;
}

void test_iterator(void)
{
	printf("=== Test: AVL Tree Iterator ===\n");
	printf("=== 测试：AVL树迭代器 ===\n");
	
	xavltree pTree = xrtAVLTreeCreate(sizeof(Item), compare_items);
	
	Item items[] = {
		{50, "Fifty"}, {25, "Twenty-five"}, {75, "Seventy-five"},
		{10, "Ten"}, {30, "Thirty"}, {60, "Sixty"}, {90, "Ninety"}
	};
	int nCount = sizeof(items) / sizeof(items[0]);
	
	printf("Inserting %d items...\n", nCount);
	printf("插入%d个项...\n", nCount);
	
	for (int i = 0; i < nCount; i++)
	{
		bool bNew = false;
		Item* pNode = (Item*)xrtAVLTreeInsert(pTree, &items[i], &bNew);
		(void)pNode;
	}
	
	printf("\nIterating through tree (sorted order):\n");
	printf("\n遍历树（排序顺序）:\n");
	
	xrtAVLTreeIterBegin(pTree);
	Item* pItem;
	while ((pItem = (Item*)xrtAVLTreeIterNext(pTree)) != NULL)
	{
		printf("  Key=%d, Name=%s\n", pItem->nKey, pItem->szName);
	}
	
	xrtAVLTreeDestroy(pTree);
}

void test_empty_tree(void)
{
	printf("\n=== Test: Empty Tree Iterator ===\n");
	printf("=== 测试：空树迭代器 ===\n");
	
	xavltree pTree = xrtAVLTreeCreate(sizeof(int), compare_items);
	
	printf("Iterating empty tree:\n");
	printf("遍历空树:\n");
	
	xrtAVLTreeIterBegin(pTree);
	int nCount = 0;
	int* pItem;
	while ((pItem = (int*)xrtAVLTreeIterNext(pTree)) != NULL)
	{
		nCount++;
	}
	
	printf("  Items found: %d (expected: 0)\n", nCount);
	printf("  找到项数: %d（预期: 0）\n", nCount);
	
	xrtAVLTreeDestroy(pTree);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  AVL Tree Iterator Example / AVL树迭代器示例\n");
	printf("========================================\n");
	
	test_iterator();
	test_empty_tree();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
