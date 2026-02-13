/*
 * XRT Example - AVL Tree
 * XRT 范例 - AVL树
 *
 * Description / 说明:
 *   EN: Demonstrates AVL tree implementation with automatic balancing,
 *       including insertion, deletion, rotation operations, and balance factor calculation.
 *       Shows how AVL trees maintain O(log n) height through rotations.
 *   CN: 演示带有自动平衡的AVL树实现，包括插入、删除、旋转操作和平衡因子计算。
 *       展示AVL树如何通过旋转保持O(log n)高度。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/data_structure_avl_tree.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/data_structure_avl_tree -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Self-balancing binary search tree
 *   - LL, RR, LR, RL rotations
 *   - Height and balance factor tracking
 *   - AVL tree properties validation
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#include <time.h>

// AVL tree node structure
// AVL树节点结构
typedef struct AVLNode {
	int iData;
	int iHeight;
	struct AVLNode* pLeft;
	struct AVLNode* pRight;
} AVLNode;

// AVL tree structure
// AVL树结构
typedef struct {
	AVLNode* pRoot;
} AVLTree;

// Create new AVL node
// 创建新AVL节点
AVLNode* AVLNodeCreate(int iData)
{
	AVLNode* pNode = xrtMalloc(sizeof(AVLNode));
	pNode->iData = iData;
	pNode->iHeight = 1;
	pNode->pLeft = NULL;
	pNode->pRight = NULL;
	return pNode;
}

// Create AVL tree
// 创建AVL树
AVLTree* AVLTreeCreate()
{
	AVLTree* pTree = xrtMalloc(sizeof(AVLTree));
	pTree->pRoot = NULL;
	return pTree;
}

// Destroy AVL tree
// 销毁AVL树
void AVLNodeDestroy(AVLNode* pNode)
{
	if ( pNode ) {
		AVLNodeDestroy(pNode->pLeft);
		AVLNodeDestroy(pNode->pRight);
		xrtFree(pNode);
	}
}

void AVLTreeDestroy(AVLTree* pTree)
{
	if ( pTree ) {
		AVLNodeDestroy(pTree->pRoot);
		xrtFree(pTree);
	}
}

// Get height of node
// 获取节点高度
int AVLGetHeight(AVLNode* pNode)
{
	return pNode ? pNode->iHeight : 0;
}

// Get balance factor
// 获取平衡因子
int AVLGetBalance(AVLNode* pNode)
{
	return pNode ? AVLGetHeight(pNode->pLeft) - AVLGetHeight(pNode->pRight) : 0;
}

// Update height of node
// 更新节点高度
void AVLUpdateHeight(AVLNode* pNode)
{
	if ( pNode ) {
		int iLeftHeight = AVLGetHeight(pNode->pLeft);
		int iRightHeight = AVLGetHeight(pNode->pRight);
		pNode->iHeight = 1 + (iLeftHeight > iRightHeight ? iLeftHeight : iRightHeight);
	}
}

// Right rotation (LL case)
// 右旋转（LL情况）
AVLNode* AVLRightRotate(AVLNode* pNode)
{
	AVLNode* pNewRoot = pNode->pLeft;
	AVLNode* pTemp = pNewRoot->pRight;
	
	// Perform rotation
	// 执行旋转
	pNewRoot->pRight = pNode;
	pNode->pLeft = pTemp;
	
	// Update heights
	// 更新高度
	AVLUpdateHeight(pNode);
	AVLUpdateHeight(pNewRoot);
	
	return pNewRoot;
}

// Left rotation (RR case)
// 左旋转（RR情况）
AVLNode* AVLLeftRotate(AVLNode* pNode)
{
	AVLNode* pNewRoot = pNode->pRight;
	AVLNode* pTemp = pNewRoot->pLeft;
	
	// Perform rotation
	// 执行旋转
	pNewRoot->pLeft = pNode;
	pNode->pRight = pTemp;
	
	// Update heights
	// 更新高度
	AVLUpdateHeight(pNode);
	AVLUpdateHeight(pNewRoot);
	
	return pNewRoot;
}

// Insert node into AVL tree
// 向AVL树插入节点
AVLNode* AVLInsert(AVLNode* pNode, int iData)
{
	// Standard BST insertion
	// 标准BST插入
	if ( pNode == NULL ) {
		return AVLNodeCreate(iData);
	}
	
	if ( iData < pNode->iData ) {
		pNode->pLeft = AVLInsert(pNode->pLeft, iData);
	} else if ( iData > pNode->iData ) {
		pNode->pRight = AVLInsert(pNode->pRight, iData);
	} else {
		// Duplicate keys not allowed
		// 不允许重复键
		return pNode;
	}
	
	// Update height of this ancestor node
	// 更新此祖先节点的高度
	AVLUpdateHeight(pNode);
	
	// Get balance factor
	// 获取平衡因子
	int iBalance = AVLGetBalance(pNode);
	
	// Left Left Case
	// 左左情况
	if ( iBalance > 1 && iData < pNode->pLeft->iData ) {
		return AVLRightRotate(pNode);
	}
	
	// Right Right Case
	// 右右情况
	if ( iBalance < -1 && iData > pNode->pRight->iData ) {
		return AVLLeftRotate(pNode);
	}
	
	// Left Right Case
	// 左右情况
	if ( iBalance > 1 && iData > pNode->pLeft->iData ) {
		pNode->pLeft = AVLLeftRotate(pNode->pLeft);
		return AVLRightRotate(pNode);
	}
	
	// Right Left Case
	// 右左情况
	if ( iBalance < -1 && iData < pNode->pRight->iData ) {
		pNode->pRight = AVLRightRotate(pNode->pRight);
		return AVLLeftRotate(pNode);
	}
	
	// Return unchanged node pointer
	// 返回未改变的节点指针
	return pNode;
}

// Find minimum value node
// 查找最小值节点
AVLNode* AVLFindMin(AVLNode* pNode)
{
	while ( pNode && pNode->pLeft ) {
		pNode = pNode->pLeft;
	}
	return pNode;
}

// Delete node from AVL tree
// 从AVL树删除节点
AVLNode* AVLDelete(AVLNode* pNode, int iData)
{
	// Standard BST deletion
	// 标准BST删除
	if ( pNode == NULL ) {
		return pNode;
	}
	
	if ( iData < pNode->iData ) {
		pNode->pLeft = AVLDelete(pNode->pLeft, iData);
	} else if ( iData > pNode->iData ) {
		pNode->pRight = AVLDelete(pNode->pRight, iData);
	} else {
		// Node to be deleted found
		// 找到要删除的节点
		
		// Node with only one child or no child
		// 只有一个子节点或无子节点的节点
		if ( pNode->pLeft == NULL || pNode->pRight == NULL ) {
			AVLNode* pTemp = pNode->pLeft ? pNode->pLeft : pNode->pRight;
			
			// No child case
			// 无子节点情况
			if ( pTemp == NULL ) {
				pTemp = pNode;
				pNode = NULL;
			} else {
				// One child case
				// 一个子节点情况
				*pNode = *pTemp;  // Copy contents / 复制内容
			}
			xrtFree(pTemp);
		} else {
			// Node with two children
			// 有两个子节点的节点
			AVLNode* pTemp = AVLFindMin(pNode->pRight);
			pNode->iData = pTemp->iData;
			pNode->pRight = AVLDelete(pNode->pRight, pTemp->iData);
		}
	}
	
	// If tree had only one node
	// 如果树只有一个节点
	if ( pNode == NULL ) {
		return pNode;
	}
	
	// Update height of current node
	// 更新当前节点的高度
	AVLUpdateHeight(pNode);
	
	// Get balance factor
	// 获取平衡因子
	int iBalance = AVLGetBalance(pNode);
	
	// Left Left Case
	// 左左情况
	if ( iBalance > 1 && AVLGetBalance(pNode->pLeft) >= 0 ) {
		return AVLRightRotate(pNode);
	}
	
	// Left Right Case
	// 左右情况
	if ( iBalance > 1 && AVLGetBalance(pNode->pLeft) < 0 ) {
		pNode->pLeft = AVLLeftRotate(pNode->pLeft);
		return AVLRightRotate(pNode);
	}
	
	// Right Right Case
	// 右右情况
	if ( iBalance < -1 && AVLGetBalance(pNode->pRight) <= 0 ) {
		return AVLLeftRotate(pNode);
	}
	
	// Right Left Case
	// 右左情况
	if ( iBalance < -1 && AVLGetBalance(pNode->pRight) > 0 ) {
		pNode->pRight = AVLRightRotate(pNode->pRight);
		return AVLLeftRotate(pNode);
	}
	
	return pNode;
}

// Search in AVL tree
// 在AVL树中搜索
AVLNode* AVLSearch(AVLNode* pNode, int iData)
{
	if ( pNode == NULL || pNode->iData == iData ) {
		return pNode;
	}
	
	if ( iData < pNode->iData ) {
		return AVLSearch(pNode->pLeft, iData);
	} else {
		return AVLSearch(pNode->pRight, iData);
	}
}

// Inorder traversal
// 中序遍历
void AVLInorder(AVLNode* pNode)
{
	if ( pNode ) {
		AVLInorder(pNode->pLeft);
		printf("%d ", pNode->iData);
		AVLInorder(pNode->pRight);
	}
}

// Preorder traversal
// 前序遍历
void AVLPreorder(AVLNode* pNode)
{
	if ( pNode ) {
		printf("%d ", pNode->iData);
		AVLPreorder(pNode->pLeft);
		AVLPreorder(pNode->pRight);
	}
}

// Check if tree is balanced
// 检查树是否平衡
int AVLIsBalanced(AVLNode* pNode)
{
	if ( pNode == NULL ) {
		return 1;
	}
	
	int iBalance = AVLGetBalance(pNode);
	if ( iBalance > 1 || iBalance < -1 ) {
		return 0;
	}
	
	return AVLIsBalanced(pNode->pLeft) && AVLIsBalanced(pNode->pRight);
}

// Calculate tree height
// 计算树高度
int AVLTreeHeight(AVLNode* pNode)
{
	return pNode ? pNode->iHeight : 0;
}

// Print tree structure (simplified)
// 打印树结构（简化）
void AVLPrintStructure(AVLNode* pNode, int iLevel)
{
	if ( pNode ) {
		AVLPrintStructure(pNode->pRight, iLevel + 1);
		
		for ( int i = 0; i < iLevel; i++ ) {
			printf("    ");
		}
		printf("%d(H:%d,B:%d)\n", pNode->iData, pNode->iHeight, AVLGetBalance(pNode));
		
		AVLPrintStructure(pNode->pLeft, iLevel + 1);
	}
}

// Test AVL tree operations
// 测试AVL树操作
void TestAVLOperations()
{
	printf("=== AVL Tree Test ===\n");
	printf("=== AVL树测试 ===\n");
	
	AVLTree* pTree = AVLTreeCreate();
	
	// Test insertions that trigger rotations
	// 测试触发旋转的插入
	printf("Testing insertions with rotations:\n");
	
	// Insert sequence that causes LL rotation
	// 插入序列导致LL旋转
	// 30 -> 20 -> 10 (should trigger LL rotation)
	pTree->pRoot = AVLInsert(pTree->pRoot, 30);
	pTree->pRoot = AVLInsert(pTree->pRoot, 20);
	pTree->pRoot = AVLInsert(pTree->pRoot, 10);
	
	printf("After LL rotation (insert 10):\n");
	AVLPrintStructure(pTree->pRoot, 0);
	printf("Is balanced: %s\n", AVLIsBalanced(pTree->pRoot) ? "Yes" : "No");
	printf("Height: %d\n", AVLTreeHeight(pTree->pRoot));
	
	// Continue inserting to trigger other rotations
	// 继续插入以触发其他旋转
	pTree->pRoot = AVLInsert(pTree->pRoot, 25);
	pTree->pRoot = AVLInsert(pTree->pRoot, 35);
	pTree->pRoot = AVLInsert(pTree->pRoot, 40);  // Should trigger RR rotation
	
	printf("\nAfter RR rotation (insert 40):\n");
	AVLPrintStructure(pTree->pRoot, 0);
	printf("Is balanced: %s\n", AVLIsBalanced(pTree->pRoot) ? "Yes" : "No");
	
	// Test LR rotation
	// 测试LR旋转
	pTree->pRoot = AVLInsert(pTree->pRoot, 5);
	pTree->pRoot = AVLInsert(pTree->pRoot, 15);  // Should trigger LR rotation
	
	printf("\nAfter LR rotation (insert 15):\n");
	AVLPrintStructure(pTree->pRoot, 0);
	printf("Is balanced: %s\n", AVLIsBalanced(pTree->pRoot) ? "Yes" : "No");
	
	// Test traversals
	// 测试遍历
	printf("\nTraversals:\n");
	printf("Inorder: ");
	AVLInorder(pTree->pRoot);
	printf("\nPreorder: ");
	AVLPreorder(pTree->pRoot);
	printf("\n");
	
	// Test search
	// 测试搜索
	printf("\nTesting search:\n");
	int arrSearch[] = {15, 25, 99};
	for ( int i = 0; i < 3; i++ ) {
		AVLNode* pNode = AVLSearch(pTree->pRoot, arrSearch[i]);
		if ( pNode ) {
			printf("Found %d in tree\n", arrSearch[i]);
		} else {
			printf("%d not found in tree\n", arrSearch[i]);
		}
	}
	
	// Test deletions
	// 测试删除
	printf("\nTesting deletions:\n");
	pTree->pRoot = AVLDelete(pTree->pRoot, 15);
	printf("After deleting 15:\n");
	AVLPrintStructure(pTree->pRoot, 0);
	printf("Is balanced: %s\n", AVLIsBalanced(pTree->pRoot) ? "Yes" : "No");
	
	pTree->pRoot = AVLDelete(pTree->pRoot, 30);
	printf("After deleting 30 (root):\n");
	AVLPrintStructure(pTree->pRoot, 0);
	printf("Is balanced: %s\n", AVLIsBalanced(pTree->pRoot) ? "Yes" : "No");
	
	// Cleanup
	// 清理
	AVLTreeDestroy(pTree);
}

// Test AVL properties
// 测试AVL性质
void TestAVLProperties()
{
	printf("\n=== AVL Properties Test ===\n");
	printf("=== AVL性质测试 ===\n");
	
	AVLTree* pTree = AVLTreeCreate();
	
	// Insert many elements and verify balance
	// 插入许多元素并验证平衡
	printf("Inserting 1000 random elements:\n");
	srand((unsigned int)time(NULL));
	
	for ( int i = 0; i < 1000; i++ ) {
		int iValue = rand() % 10000;
		pTree->pRoot = AVLInsert(pTree->pRoot, iValue);
	}
	
	printf("Tree height: %d\n", AVLTreeHeight(pTree->pRoot));
	printf("Is balanced: %s\n", AVLIsBalanced(pTree->pRoot) ? "Yes" : "No");
	
	// For comparison, maximum height of balanced tree with 1000 nodes
	// 用于比较，1000个节点的平衡树的最大高度
	// Should be approximately log2(1000) ≈ 10
	// 应该大约是log2(1000) ≈ 10
	double dMaxHeight = log(1000) / log(2);
	printf("Expected max height: ~%.1f\n", dMaxHeight);
	
	// Cleanup
	// 清理
	AVLTreeDestroy(pTree);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT AVL Tree Demo\n");
	printf("XRT AVL树演示\n");
	printf("================\n\n");
	
	// Run tests
	// 运行测试
	TestAVLOperations();
	TestAVLProperties();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}