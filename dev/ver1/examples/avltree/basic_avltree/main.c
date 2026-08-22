/**
 * @file main.c
 * @brief Basic AVL Tree Example - xrtAVLTree operations
 *        基本AVL树示例 - xrtAVLTree 操作
 * 
 * This example demonstrates:
 * - Creating and destroying AVL trees
 * - Inserting nodes with keys
 * - Searching for nodes
 * - Removing nodes
 * 
 * 本示例演示：
 * - 创建和销毁AVL树
 * - 插入带键的节点
 * - 搜索节点
 * - 删除节点
 * 
 * Build: tcc main.c -o ../../bin/avltree_basic_avltree.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

typedef struct {
	int nKey;
	char szData[32];
} TreeNode;

int compare_int_keys(ptr pA, ptr pB)
{
	TreeNode* pNodeA = (TreeNode*)pA;
	TreeNode* pNodeB = (TreeNode*)pB;
	return pNodeA->nKey - pNodeB->nKey;
}

void test_basic_avltree(void)
{
	printf("=== Test: Basic AVL Tree Operations ===\n");
	printf("=== 测试：基本AVL树操作 ===\n");
	
	xavltree pTree = xrtAVLTreeCreate(sizeof(TreeNode), compare_int_keys, XRT_OBJMODE_LOCAL);
	
	printf("Created AVL tree with TreeNode item size: %u\n", (uint32)sizeof(TreeNode));
	printf("创建AVL树，TreeNode项大小: %u\n", (uint32)sizeof(TreeNode));
	
	printf("\nInserting nodes...\n");
	printf("\n插入节点...\n");
	
	int keys[] = {50, 30, 70, 20, 40, 60, 80};
	char* names[] = {"Alpha", "Beta", "Gamma", "Delta", "Epsilon", "Zeta", "Eta"};
	int nCount = sizeof(keys) / sizeof(keys[0]);
	
	for (int i = 0; i < nCount; i++)
	{
		TreeNode keyData;
		keyData.nKey = keys[i];
		strcpy(keyData.szData, names[i]);
		
		bool bNew = false;
		TreeNode* pNode = (TreeNode*)xrtAVLTreeInsert(pTree, &keyData, &bNew);
		printf("  Inserted key=%d, data=%s (new=%s)\n", keys[i], names[i], bNew ? "yes" : "no");
	}
	
	printf("\nSearching for keys...\n");
	printf("\n搜索键...\n");
	
	TreeNode searchKey;
	searchKey.nKey = 40;
	TreeNode* pFound = (TreeNode*)xrtAVLTreeSearch(pTree, &searchKey);
	if (pFound)
	{
		printf("  Found key=%d, data=%s\n", pFound->nKey, pFound->szData);
	}
	
	searchKey.nKey = 99;
	pFound = (TreeNode*)xrtAVLTreeSearch(pTree, &searchKey);
	if (!pFound)
	{
		printf("  Key 99 not found (expected)\n");
		printf("  键99未找到（预期）\n");
	}
	
	printf("\nRemoving key=30...\n");
	printf("\n删除键=30...\n");
	searchKey.nKey = 30;
	bool bRemoved = xrtAVLTreeRemove(pTree, &searchKey);
	printf("  Remove result: %s\n", bRemoved ? "success" : "failed");
	
	searchKey.nKey = 30;
	pFound = (TreeNode*)xrtAVLTreeSearch(pTree, &searchKey);
	if (!pFound)
	{
		printf("  Key 30 not found after removal (expected)\n");
		printf("  键30删除后未找到（预期）\n");
	}
	
	xrtAVLTreeDestroy(pTree);
}

void test_balanced_insertions(void)
{
	printf("\n=== Test: Balanced Insertions ===\n");
	printf("=== 测试：平衡插入 ===\n");
	
	xavltree pTree = xrtAVLTreeCreate(sizeof(int), compare_int_keys, XRT_OBJMODE_LOCAL);
	
	printf("Inserting keys in ascending order (tests AVL balancing):\n");
	printf("按升序插入键（测试AVL平衡）:\n");
	
	for (int i = 1; i <= 10; i++)
	{
		int keyData = i * 10;
		bool bNew = false;
		int* pNode = (int*)xrtAVLTreeInsert(pTree, &keyData, &bNew);
		printf("  Inserted: %d\n", *pNode);
	}
	
	printf("\nTree remains balanced due to AVL rotations\n");
	printf("由于AVL旋转，树保持平衡\n");
	
	xrtAVLTreeDestroy(pTree);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  Basic AVL Tree Example / 基本AVL树示例\n");
	printf("========================================\n");
	
	test_basic_avltree();
	test_balanced_insertions();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
