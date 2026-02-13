/*
 * XRT Example - Linked List Operations
 * XRT 范例 - 链表操作
 *
 * Description / 说明:
 *   EN: Demonstrates comprehensive linked list operations including node management,
 *       insertion/deletion at various positions, traversal techniques, sorting algorithms,
 *       and common linked list patterns. Covers singly and doubly linked lists.
 *   CN: 演示全面的链表操作，包括节点管理、各位置的插入/删除、遍历技术、
 *       排序算法和常见链表模式。涵盖单向和双向链表。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/data_structure_linked_list_operations.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/data_structure_linked_list_operations -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Singly linked list implementation
 *   - Doubly linked list implementation
 *   - Various insertion/deletion methods
 *   - Traversal and search operations
 *   - Sorting and reversing algorithms
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#include <time.h>

// Node structure for singly linked list
// 单向链表的节点结构
typedef struct SNode {
	int iData;
	struct SNode* pNext;
} SNode;

// Node structure for doubly linked list
// 双向链表的节点结构
typedef struct DNode {
	int iData;
	struct DNode* pNext;
	struct DNode* pPrev;
} DNode;

// Singly linked list structure
// 单向链表结构
typedef struct {
	SNode* pHead;
	SNode* pTail;
	size_t iSize;
} SLinkedList;

// Doubly linked list structure
// 双向链表结构
typedef struct {
	DNode* pHead;
	DNode* pTail;
	size_t iSize;
} DLinkedList;

// Create new node for singly linked list
// 为单向链表创建新节点
SNode* SNodeCreate(int iData)
{
	SNode* pNode = xrtMalloc(sizeof(SNode));
	pNode->iData = iData;
	pNode->pNext = NULL;
	return pNode;
}

// Create new node for doubly linked list
// 为双向链表创建新节点
DNode* DNodeCreate(int iData)
{
	DNode* pNode = xrtMalloc(sizeof(DNode));
	pNode->iData = iData;
	pNode->pNext = NULL;
	pNode->pPrev = NULL;
	return pNode;
}

// Initialize singly linked list
// 初始化单向链表
SLinkedList* SListCreate()
{
	SLinkedList* pList = xrtMalloc(sizeof(SLinkedList));
	pList->pHead = NULL;
	pList->pTail = NULL;
	pList->iSize = 0;
	return pList;
}

// Initialize doubly linked list
// 初始化双向链表
DLinkedList* DListCreate()
{
	DLinkedList* pList = xrtMalloc(sizeof(DLinkedList));
	pList->pHead = NULL;
	pList->pTail = NULL;
	pList->iSize = 0;
	return pList;
}

// Destroy singly linked list
// 销毁单向链表
void SListDestroy(SLinkedList* pList)
{
	SNode* pNode = pList->pHead;
	while ( pNode ) {
		SNode* pNext = pNode->pNext;
		xrtFree(pNode);
		pNode = pNext;
	}
	xrtFree(pList);
}

// Destroy doubly linked list
// 销毁双向链表
void DListDestroy(DLinkedList* pList)
{
	DNode* pNode = pList->pHead;
	while ( pNode ) {
		DNode* pNext = pNode->pNext;
		xrtFree(pNode);
		pNode = pNext;
	}
	xrtFree(pList);
}

// Insert at beginning of singly linked list
// 在单向链表开头插入
void SListInsertFront(SLinkedList* pList, int iData)
{
	SNode* pNode = SNodeCreate(iData);
	
	if ( pList->pHead == NULL ) {
		// Empty list
		// 空列表
		pList->pHead = pNode;
		pList->pTail = pNode;
	} else {
		// Insert at front
		// 在前面插入
		pNode->pNext = pList->pHead;
		pList->pHead = pNode;
	}
	
	pList->iSize++;
}

// Insert at end of singly linked list
// 在单向链表末尾插入
void SListInsertBack(SLinkedList* pList, int iData)
{
	SNode* pNode = SNodeCreate(iData);
	
	if ( pList->pHead == NULL ) {
		// Empty list
		// 空列表
		pList->pHead = pNode;
		pList->pTail = pNode;
	} else {
		// Insert at back
		// 在后面插入
		pList->pTail->pNext = pNode;
		pList->pTail = pNode;
	}
	
	pList->iSize++;
}

// Insert at specific position in singly linked list
// 在单向链表的指定位置插入
void SListInsertAt(SLinkedList* pList, size_t iIndex, int iData)
{
	if ( iIndex == 0 ) {
		SListInsertFront(pList, iData);
		return;
	}
	
	if ( iIndex >= pList->iSize ) {
		SListInsertBack(pList, iData);
		return;
	}
	
	// Find node before insertion point
	// 找到插入点之前的节点
	SNode* pNode = pList->pHead;
	for ( size_t i = 0; i < iIndex - 1; i++ ) {
		pNode = pNode->pNext;
	}
	
	// Insert new node
	// 插入新节点
	SNode* pNewNode = SNodeCreate(iData);
	pNewNode->pNext = pNode->pNext;
	pNode->pNext = pNewNode;
	pList->iSize++;
}

// Delete from beginning of singly linked list
// 从单向链表开头删除
int SListDeleteFront(SLinkedList* pList)
{
	if ( pList->pHead == NULL ) {
		return 0;  // Empty list / 空列表
	}
	
	SNode* pNode = pList->pHead;
	int iData = pNode->iData;
	
	pList->pHead = pNode->pNext;
	if ( pList->pHead == NULL ) {
		pList->pTail = NULL;  // List becomes empty / 列表变为空
	}
	
	xrtFree(pNode);
	pList->iSize--;
	
	return iData;
}

// Delete from end of singly linked list
// 从单向链表末尾删除
int SListDeleteBack(SLinkedList* pList)
{
	if ( pList->pHead == NULL ) {
		return 0;  // Empty list / 空列表
	}
	
	if ( pList->pHead == pList->pTail ) {
		// Only one node
		// 只有一个节点
		int iData = pList->pHead->iData;
		xrtFree(pList->pHead);
		pList->pHead = NULL;
		pList->pTail = NULL;
		pList->iSize = 0;
		return iData;
	}
	
	// Find second-to-last node
	// 找到倒数第二个节点
	SNode* pNode = pList->pHead;
	while ( pNode->pNext != pList->pTail ) {
		pNode = pNode->pNext;
	}
	
	int iData = pList->pTail->iData;
	xrtFree(pList->pTail);
	pList->pTail = pNode;
	pList->pTail->pNext = NULL;
	pList->iSize--;
	
	return iData;
}

// Delete node with specific value
// 删除具有特定值的节点
int SListDeleteValue(SLinkedList* pList, int iValue)
{
	if ( pList->pHead == NULL ) {
		return 0;  // Empty list / 空列表
	}
	
	// Check if head node contains value
	// 检查头节点是否包含值
	if ( pList->pHead->iData == iValue ) {
		return SListDeleteFront(pList);
	}
	
	// Search for node
	// 搜索节点
	SNode* pNode = pList->pHead;
	while ( pNode->pNext && pNode->pNext->iData != iValue ) {
		pNode = pNode->pNext;
	}
	
	if ( pNode->pNext == NULL ) {
		return 0;  // Value not found / 未找到值
	}
	
	// Delete node
	// 删除节点
	SNode* pNodeToDelete = pNode->pNext;
	int iData = pNodeToDelete->iData;
	
	pNode->pNext = pNodeToDelete->pNext;
	if ( pNodeToDelete == pList->pTail ) {
		pList->pTail = pNode;  // Update tail if needed / 如需要则更新尾部
	}
	
	xrtFree(pNodeToDelete);
	pList->iSize--;
	
	return iData;
}

// Search for value in singly linked list
// 在单向链表中搜索值
SNode* SListSearch(SLinkedList* pList, int iValue)
{
	SNode* pNode = pList->pHead;
	while ( pNode ) {
		if ( pNode->iData == iValue ) {
			return pNode;
		}
		pNode = pNode->pNext;
	}
	return NULL;  // Not found / 未找到
}

// Print singly linked list
// 打印单向链表
void SListPrint(SLinkedList* pList, str sLabel)
{
	printf("%s: ", sLabel);
	
	if ( pList->pHead == NULL ) {
		printf("(empty)\n");
		return;
	}
	
	SNode* pNode = pList->pHead;
	printf("[");
	while ( pNode ) {
		printf("%d", pNode->iData);
		if ( pNode->pNext ) {
			printf(" -> ");
		}
		pNode = pNode->pNext;
	}
	printf("] (size: %zu)\n", pList->iSize);
}

// Reverse singly linked list
// 反转单向链表
void SListReverse(SLinkedList* pList)
{
	if ( pList->pHead == NULL || pList->pHead->pNext == NULL ) {
		return;  // Empty or single node / 空或单节点
	}
	
	SNode* pPrev = NULL;
	SNode* pCurrent = pList->pHead;
	SNode* pNext = NULL;
	
	while ( pCurrent ) {
		pNext = pCurrent->pNext;  // Store next node / 存储下一个节点
		pCurrent->pNext = pPrev;  // Reverse link / 反转链接
		pPrev = pCurrent;         // Move prev forward / 向前移动prev
		pCurrent = pNext;         // Move current forward / 向前移动current
	}
	
	// Swap head and tail
	// 交换头部和尾部
	SNode* pTemp = pList->pHead;
	pList->pHead = pList->pTail;
	pList->pTail = pTemp;
}

// Insert at beginning of doubly linked list
// 在双向链表开头插入
void DListInsertFront(DLinkedList* pList, int iData)
{
	DNode* pNode = DNodeCreate(iData);
	
	if ( pList->pHead == NULL ) {
		// Empty list
		// 空列表
		pList->pHead = pNode;
		pList->pTail = pNode;
	} else {
		// Insert at front
		// 在前面插入
		pNode->pNext = pList->pHead;
		pList->pHead->pPrev = pNode;
		pList->pHead = pNode;
	}
	
	pList->iSize++;
}

// Insert at end of doubly linked list
// 在双向链表末尾插入
void DListInsertBack(DLinkedList* pList, int iData)
{
	DNode* pNode = DNodeCreate(iData);
	
	if ( pList->pHead == NULL ) {
		// Empty list
		// 空列表
		pList->pHead = pNode;
		pList->pTail = pNode;
	} else {
		// Insert at back
		// 在后面插入
		pNode->pPrev = pList->pTail;
		pList->pTail->pNext = pNode;
		pList->pTail = pNode;
	}
	
	pList->iSize++;
}

// Delete from beginning of doubly linked list
// 从双向链表开头删除
int DListDeleteFront(DLinkedList* pList)
{
	if ( pList->pHead == NULL ) {
		return 0;  // Empty list / 空列表
	}
	
	DNode* pNode = pList->pHead;
	int iData = pNode->iData;
	
	pList->pHead = pNode->pNext;
	if ( pList->pHead == NULL ) {
		pList->pTail = NULL;  // List becomes empty / 列表变为空
	} else {
		pList->pHead->pPrev = NULL;
	}
	
	xrtFree(pNode);
	pList->iSize--;
	
	return iData;
}

// Delete from end of doubly linked list
// 从双向链表末尾删除
int DListDeleteBack(DLinkedList* pList)
{
	if ( pList->pHead == NULL ) {
		return 0;  // Empty list / 空列表
	}
	
	if ( pList->pHead == pList->pTail ) {
		// Only one node
		// 只有一个节点
		int iData = pList->pHead->iData;
		xrtFree(pList->pHead);
		pList->pHead = NULL;
		pList->pTail = NULL;
		pList->iSize = 0;
		return iData;
	}
	
	int iData = pList->pTail->iData;
	DNode* pNode = pList->pTail;
	
	pList->pTail = pNode->pPrev;
	pList->pTail->pNext = NULL;
	
	xrtFree(pNode);
	pList->iSize--;
	
	return iData;
}

// Print doubly linked list forward
// 正向打印双向链表
void DListPrintForward(DLinkedList* pList, str sLabel)
{
	printf("%s (forward): ", sLabel);
	
	if ( pList->pHead == NULL ) {
		printf("(empty)\n");
		return;
	}
	
	DNode* pNode = pList->pHead;
	printf("[");
	while ( pNode ) {
		printf("%d", pNode->iData);
		if ( pNode->pNext ) {
			printf(" <-> ");
		}
		pNode = pNode->pNext;
	}
	printf("] (size: %zu)\n", pList->iSize);
}

// Print doubly linked list backward
// 反向打印双向链表
void DListPrintBackward(DLinkedList* pList, str sLabel)
{
	printf("%s (backward): ", sLabel);
	
	if ( pList->pTail == NULL ) {
		printf("(empty)\n");
		return;
	}
	
	DNode* pNode = pList->pTail;
	printf("[");
	while ( pNode ) {
		printf("%d", pNode->iData);
		if ( pNode->pPrev ) {
			printf(" <-> ");
		}
		pNode = pNode->pPrev;
	}
	printf("] (size: %zu)\n", pList->iSize);
}

// Test singly linked list operations
// 测试单向链表操作
void TestSinglyLinkedList()
{
	printf("=== Singly Linked List Test ===\n");
	printf("=== 单向链表测试 ===\n");
	
	SLinkedList* pList = SListCreate();
	
	// Test insertions
	// 测试插入
	printf("Testing insertions:\n");
	SListInsertBack(pList, 10);
	SListInsertBack(pList, 20);
	SListInsertBack(pList, 30);
	SListPrint(pList, "After back insertions");
	
	SListInsertFront(pList, 5);
	SListPrint(pList, "After front insertion");
	
	SListInsertAt(pList, 2, 15);
	SListPrint(pList, "After insertion at index 2");
	
	// Test deletions
	// 测试删除
	printf("\nTesting deletions:\n");
	int iDeleted = SListDeleteFront(pList);
	printf("Deleted from front: %d\n", iDeleted);
	SListPrint(pList, "After front deletion");
	
	iDeleted = SListDeleteBack(pList);
	printf("Deleted from back: %d\n", iDeleted);
	SListPrint(pList, "After back deletion");
	
	iDeleted = SListDeleteValue(pList, 15);
	printf("Deleted value 15: %d\n", iDeleted);
	SListPrint(pList, "After value deletion");
	
	// Test search
	// 测试搜索
	printf("\nTesting search:\n");
	SNode* pNode = SListSearch(pList, 20);
	if ( pNode ) {
		printf("Found node with value 20\n");
	} else {
		printf("Node with value 20 not found\n");
	}
	
	pNode = SListSearch(pList, 99);
	if ( pNode ) {
		printf("Found node with value 99\n");
	} else {
		printf("Node with value 99 not found\n");
	}
	
	// Test reverse
	// 测试反转
	printf("\nTesting reverse:\n");
	SListPrint(pList, "Before reverse");
	SListReverse(pList);
	SListPrint(pList, "After reverse");
	
	// Cleanup
	// 清理
	SListDestroy(pList);
}

// Test doubly linked list operations
// 测试双向链表操作
void TestDoublyLinkedList()
{
	printf("\n=== Doubly Linked List Test ===\n");
	printf("=== 双向链表测试 ===\n");
	
	DLinkedList* pList = DListCreate();
	
	// Test insertions
	// 测试插入
	printf("Testing insertions:\n");
	DListInsertBack(pList, 100);
	DListInsertBack(pList, 200);
	DListInsertBack(pList, 300);
	DListPrintForward(pList, "After back insertions");
	DListPrintBackward(pList, "After back insertions");
	
	DListInsertFront(pList, 50);
	DListPrintForward(pList, "After front insertion");
	
	// Test deletions
	// 测试删除
	printf("\nTesting deletions:\n");
	int iDeleted = DListDeleteFront(pList);
	printf("Deleted from front: %d\n", iDeleted);
	DListPrintForward(pList, "After front deletion");
	
	iDeleted = DListDeleteBack(pList);
	printf("Deleted from back: %d\n", iDeleted);
	DListPrintForward(pList, "After back deletion");
	DListPrintBackward(pList, "After back deletion");
	
	// Cleanup
	// 清理
	DListDestroy(pList);
}

// Test edge cases
// 测试边界情况
void TestEdgeCases()
{
	printf("\n=== Edge Cases Test ===\n");
	printf("=== 边界情况测试 ===\n");
	
	// Test empty list operations
	// 测试空列表操作
	SLinkedList* pEmpty = SListCreate();
	
	printf("Empty list operations:\n");
	SListPrint(pEmpty, "Empty list");
	
	int iResult = SListDeleteFront(pEmpty);
	printf("Delete from empty list: %d\n", iResult);
	
	iResult = SListDeleteBack(pEmpty);
	printf("Delete from empty list: %d\n", iResult);
	
	SNode* pNode = SListSearch(pEmpty, 42);
	printf("Search in empty list: %s\n", pNode ? "Found" : "Not found");
	
	SListDestroy(pEmpty);
	
	// Test single node operations
	// 测试单节点操作
	SLinkedList* pSingle = SListCreate();
	SListInsertBack(pSingle, 999);
	SListPrint(pSingle, "Single node list");
	
	SListReverse(pSingle);
	SListPrint(pSingle, "After reverse single node");
	
	iResult = SListDeleteValue(pSingle, 999);
	printf("Delete single node: %d\n", iResult);
	SListPrint(pSingle, "After deleting single node");
	
	SListDestroy(pSingle);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Linked List Operations Demo\n");
	printf("XRT 链表操作演示\n");
	printf("=============================\n\n");
	
	// Run all tests
	// 运行所有测试
	TestSinglyLinkedList();
	TestDoublyLinkedList();
	TestEdgeCases();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}