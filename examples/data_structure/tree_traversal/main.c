/*
 * XRT Example - Tree Traversal
 * XRT 范例 - 树遍历
 *
 * Description / 说明:
 *   EN: Demonstrates binary tree implementation and various traversal algorithms
 *       including inorder, preorder, postorder, and level-order traversal.
 *       Also includes tree construction, searching, insertion, and deletion operations.
 *   CN: 演示二叉树实现和各种遍历算法，包括中序、前序、后序和层序遍历。
 *       还包括树的构建、搜索、插入和删除操作。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/data_structure_tree_traversal.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/data_structure_tree_traversal -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Binary tree implementation
 *   - Recursive and iterative traversal methods
 *   - Tree construction and manipulation
 *   - Level-order traversal with queue
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#include <time.h>

// Binary tree node structure
// 二叉树节点结构
typedef struct TreeNode {
	int iData;
	struct TreeNode* pLeft;
	struct TreeNode* pRight;
} TreeNode;

// Queue node for level-order traversal
// 用于层序遍历的队列节点
typedef struct QueueNode {
	TreeNode* pTreeNode;
	struct QueueNode* pNext;
} QueueNode;

// Queue structure
// 队列结构
typedef struct {
	QueueNode* pFront;
	QueueNode* pRear;
	size_t iSize;
} Queue;

// Create new tree node
// 创建新树节点
TreeNode* TreeNodeCreate(int iData)
{
	TreeNode* pNode = xrtMalloc(sizeof(TreeNode));
	pNode->iData = iData;
	pNode->pLeft = NULL;
	pNode->pRight = NULL;
	return pNode;
}

// Create queue
// 创建队列
Queue* QueueCreate()
{
	Queue* pQueue = xrtMalloc(sizeof(Queue));
	pQueue->pFront = NULL;
	pQueue->pRear = NULL;
	pQueue->iSize = 0;
	return pQueue;
}

// Destroy queue
// 销毁队列
void QueueDestroy(Queue* pQueue)
{
	QueueNode* pNode = pQueue->pFront;
	while ( pNode ) {
		QueueNode* pNext = pNode->pNext;
		xrtFree(pNode);
		pNode = pNext;
	}
	xrtFree(pQueue);
}

// Enqueue tree node
// 入队树节点
void QueueEnqueue(Queue* pQueue, TreeNode* pTreeNode)
{
	QueueNode* pNode = xrtMalloc(sizeof(QueueNode));
	pNode->pTreeNode = pTreeNode;
	pNode->pNext = NULL;
	
	if ( pQueue->pRear == NULL ) {
		pQueue->pFront = pNode;
		pQueue->pRear = pNode;
	} else {
		pQueue->pRear->pNext = pNode;
		pQueue->pRear = pNode;
	}
	
	pQueue->iSize++;
}

// Dequeue tree node
// 出队树节点
TreeNode* QueueDequeue(Queue* pQueue)
{
	if ( pQueue->pFront == NULL ) {
		return NULL;
	}
	
	QueueNode* pNode = pQueue->pFront;
	TreeNode* pTreeNode = pNode->pTreeNode;
	
	pQueue->pFront = pNode->pNext;
	if ( pQueue->pFront == NULL ) {
		pQueue->pRear = NULL;
	}
	
	xrtFree(pNode);
	pQueue->iSize--;
	
	return pTreeNode;
}

// Insert node into binary search tree
// 向二叉搜索树插入节点
TreeNode* BSTInsert(TreeNode* pRoot, int iData)
{
	if ( pRoot == NULL ) {
		return TreeNodeCreate(iData);
	}
	
	if ( iData < pRoot->iData ) {
		pRoot->pLeft = BSTInsert(pRoot->pLeft, iData);
	} else if ( iData > pRoot->iData ) {
		pRoot->pRight = BSTInsert(pRoot->pRight, iData);
	}
	// If equal, do nothing (no duplicates)
	// 如果相等，则不操作（无重复）
	
	return pRoot;
}

// Search in binary search tree
// 在二叉搜索树中搜索
TreeNode* BSTSearch(TreeNode* pRoot, int iData)
{
	if ( pRoot == NULL || pRoot->iData == iData ) {
		return pRoot;
	}
	
	if ( iData < pRoot->iData ) {
		return BSTSearch(pRoot->pLeft, iData);
	} else {
		return BSTSearch(pRoot->pRight, iData);
	}
}

// Find minimum value node
// 查找最小值节点
TreeNode* BSTFindMin(TreeNode* pRoot)
{
	while ( pRoot && pRoot->pLeft ) {
		pRoot = pRoot->pLeft;
	}
	return pRoot;
}

// Delete node from binary search tree
// 从二叉搜索树删除节点
TreeNode* BSTDelete(TreeNode* pRoot, int iData)
{
	if ( pRoot == NULL ) {
		return pRoot;
	}
	
	if ( iData < pRoot->iData ) {
		pRoot->pLeft = BSTDelete(pRoot->pLeft, iData);
	} else if ( iData > pRoot->iData ) {
		pRoot->pRight = BSTDelete(pRoot->pRight, iData);
	} else {
		// Node to delete found
		// 找到要删除的节点
		
		// Case 1: No children
		// 情况1：无子节点
		if ( pRoot->pLeft == NULL && pRoot->pRight == NULL ) {
			xrtFree(pRoot);
			return NULL;
		}
		// Case 2: One child
		// 情况2：一个子节点
		else if ( pRoot->pLeft == NULL ) {
			TreeNode* pTemp = pRoot->pRight;
			xrtFree(pRoot);
			return pTemp;
		} else if ( pRoot->pRight == NULL ) {
			TreeNode* pTemp = pRoot->pLeft;
			xrtFree(pRoot);
			return pTemp;
		}
		// Case 3: Two children
		// 情况3：两个子节点
		else {
			TreeNode* pSuccessor = BSTFindMin(pRoot->pRight);
			pRoot->iData = pSuccessor->iData;
			pRoot->pRight = BSTDelete(pRoot->pRight, pSuccessor->iData);
		}
	}
	
	return pRoot;
}

// Inorder traversal (Left -> Root -> Right)
// 中序遍历（左->根->右）
void InorderTraversal(TreeNode* pRoot)
{
	if ( pRoot ) {
		InorderTraversal(pRoot->pLeft);
		printf("%d ", pRoot->iData);
		InorderTraversal(pRoot->pRight);
	}
}

// Preorder traversal (Root -> Left -> Right)
// 前序遍历（根->左->右）
void PreorderTraversal(TreeNode* pRoot)
{
	if ( pRoot ) {
		printf("%d ", pRoot->iData);
		PreorderTraversal(pRoot->pLeft);
		PreorderTraversal(pRoot->pRight);
	}
}

// Postorder traversal (Left -> Right -> Root)
// 后序遍历（左->右->根）
void PostorderTraversal(TreeNode* pRoot)
{
	if ( pRoot ) {
		PostorderTraversal(pRoot->pLeft);
		PostorderTraversal(pRoot->pRight);
		printf("%d ", pRoot->iData);
	}
}

// Level-order traversal (Breadth-first)
// 层序遍历（广度优先）
void LevelOrderTraversal(TreeNode* pRoot)
{
	if ( pRoot == NULL ) {
		return;
	}
	
	Queue* pQueue = QueueCreate();
	QueueEnqueue(pQueue, pRoot);
	
	while ( pQueue->pFront != NULL ) {
		TreeNode* pNode = QueueDequeue(pQueue);
		printf("%d ", pNode->iData);
		
		if ( pNode->pLeft ) {
			QueueEnqueue(pQueue, pNode->pLeft);
		}
		if ( pNode->pRight ) {
			QueueEnqueue(pQueue, pNode->pRight);
		}
	}
	
	QueueDestroy(pQueue);
}

// Calculate tree height
// 计算树的高度
int TreeHeight(TreeNode* pRoot)
{
	if ( pRoot == NULL ) {
		return 0;
	}
	
	int iLeftHeight = TreeHeight(pRoot->pLeft);
	int iRightHeight = TreeHeight(pRoot->pRight);
	
	return 1 + (iLeftHeight > iRightHeight ? iLeftHeight : iRightHeight);
}

// Count total nodes
// 计算总节点数
int CountNodes(TreeNode* pRoot)
{
	if ( pRoot == NULL ) {
		return 0;
	}
	return 1 + CountNodes(pRoot->pLeft) + CountNodes(pRoot->pRight);
}

// Destroy entire tree
// 销毁整个树
void TreeDestroy(TreeNode* pRoot)
{
	if ( pRoot ) {
		TreeDestroy(pRoot->pLeft);
		TreeDestroy(pRoot->pRight);
		xrtFree(pRoot);
	}
}

// Print tree structure (simplified visualization)
// 打印树结构（简化可视化）
void PrintTreeStructure(TreeNode* pRoot, int iLevel)
{
	if ( pRoot ) {
		PrintTreeStructure(pRoot->pRight, iLevel + 1);
		
		for ( int i = 0; i < iLevel; i++ ) {
			printf("    ");
		}
		printf("%d\n", pRoot->iData);
		
		PrintTreeStructure(pRoot->pLeft, iLevel + 1);
	}
}

// Test tree operations
// 测试树操作
void TestTreeOperations()
{
	printf("=== Binary Search Tree Test ===\n");
	printf("=== 二叉搜索树测试 ===\n");
	
	// Create tree with sample data
	// 使用样本数据创建树
	TreeNode* pRoot = NULL;
	int arrData[] = {50, 30, 70, 20, 40, 60, 80, 10, 25, 35, 45};
	size_t iSize = sizeof(arrData) / sizeof(arrData[0]);
	
	printf("Inserting data: ");
	for ( size_t i = 0; i < iSize; i++ ) {
		printf("%d ", arrData[i]);
		pRoot = BSTInsert(pRoot, arrData[i]);
	}
	printf("\n\n");
	
	// Print tree structure
	// 打印树结构
	printf("Tree structure:\n");
	PrintTreeStructure(pRoot, 0);
	
	// Test traversals
	// 测试遍历
	printf("\nTree traversals:\n");
	printf("Inorder (sorted): ");
	InorderTraversal(pRoot);
	printf("\n");
	
	printf("Preorder: ");
	PreorderTraversal(pRoot);
	printf("\n");
	
	printf("Postorder: ");
	PostorderTraversal(pRoot);
	printf("\n");
	
	printf("Level-order: ");
	LevelOrderTraversal(pRoot);
	printf("\n");
	
	// Test tree properties
	// 测试树属性
	printf("\nTree properties:\n");
	printf("Height: %d\n", TreeHeight(pRoot));
	printf("Total nodes: %d\n", CountNodes(pRoot));
	
	// Test search
	// 测试搜索
	printf("\nTesting search:\n");
	int iSearchValues[] = {40, 99, 25, 70};
	for ( int i = 0; i < 4; i++ ) {
		TreeNode* pNode = BSTSearch(pRoot, iSearchValues[i]);
		if ( pNode ) {
			printf("Found %d in tree\n", iSearchValues[i]);
		} else {
			printf("%d not found in tree\n", iSearchValues[i]);
		}
	}
	
	// Test deletion
	// 测试删除
	printf("\nTesting deletion:\n");
	printf("Deleting 30...\n");
	pRoot = BSTDelete(pRoot, 30);
	printf("Inorder after deletion: ");
	InorderTraversal(pRoot);
	printf("\n");
	
	printf("Deleting 50 (root)...\n");
	pRoot = BSTDelete(pRoot, 50);
	printf("Inorder after deletion: ");
	InorderTraversal(pRoot);
	printf("\n");
	
	// Cleanup
	// 清理
	TreeDestroy(pRoot);
}

// Test edge cases
// 测试边界情况
void TestEdgeCases()
{
	printf("\n=== Edge Cases Test ===\n");
	printf("=== 边界情况测试 ===\n");
	
	// Test empty tree
	// 测试空树
	TreeNode* pEmpty = NULL;
	printf("Empty tree traversals:\n");
	printf("Inorder: ");
	InorderTraversal(pEmpty);
	printf("\nPreorder: ");
	PreorderTraversal(pEmpty);
	printf("\nPostorder: ");
	PostorderTraversal(pEmpty);
	printf("\nLevel-order: ");
	LevelOrderTraversal(pEmpty);
	printf("\n");
	
	printf("Empty tree height: %d\n", TreeHeight(pEmpty));
	printf("Empty tree nodes: %d\n", CountNodes(pEmpty));
	
	// Test single node tree
	// 测试单节点树
	TreeNode* pSingle = TreeNodeCreate(42);
	printf("\nSingle node tree:\n");
	printf("Inorder: ");
	InorderTraversal(pSingle);
	printf("\nHeight: %d\n", TreeHeight(pSingle));
	printf("Nodes: %d\n", CountNodes(pSingle));
	
	TreeDestroy(pSingle);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Tree Traversal Demo\n");
	printf("XRT 树遍历演示\n");
	printf("=====================\n\n");
	
	// Run all tests
	// 运行所有测试
	TestTreeOperations();
	TestEdgeCases();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}