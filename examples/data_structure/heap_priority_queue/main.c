/*
 * XRT Example - Heap Priority Queue
 * XRT 范例 - 堆优先队列
 *
 * Description / 说明:
 *   EN: Demonstrates heap data structure implementation as a priority queue,
 *       including min-heap and max-heap operations, heapify algorithms,
 *       and applications like heap sort and top-K selection.
 *       Supports efficient insertion, deletion, and priority-based extraction.
 *   CN: 演示堆数据结构作为优先队列的实现，包括最小堆和最大堆操作、
 *       堆化算法，以及堆排序和Top-K选择等应用。
 *       支持高效的插入、删除和基于优先级的提取。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/data_structure_heap_priority_queue.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/data_structure_heap_priority_queue -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Min-heap and max-heap implementations
 *   - Heapify up and down operations
 *   - Priority queue functionality
 *   - Heap sort algorithm
 *   - Top-K selection algorithms
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#include <time.h>

// Heap structure
// 堆结构
typedef struct {
	int* piData;		// Heap array / 堆数组
	size_t iSize;		// Current size / 当前大小
	size_t iCapacity;	// Maximum capacity / 最大容量
	int bIsMinHeap;		// 1 for min-heap, 0 for max-heap / 1为最小堆，0为最大堆
} PriorityQueue;

// Create priority queue
// 创建优先队列
PriorityQueue* PQCreate(size_t iCapacity, int bIsMinHeap)
{
	PriorityQueue* pPQ = xrtMalloc(sizeof(PriorityQueue));
	pPQ->piData = xrtMalloc(iCapacity * sizeof(int));
	pPQ->iSize = 0;
	pPQ->iCapacity = iCapacity;
	pPQ->bIsMinHeap = bIsMinHeap;
	return pPQ;
}

// Destroy priority queue
// 销毁优先队列
void PQDestroy(PriorityQueue* pPQ)
{
	if ( pPQ ) {
		if ( pPQ->piData ) {
			xrtFree(pPQ->piData);
		}
		xrtFree(pPQ);
	}
}

// Helper functions for heap operations
// 堆操作的辅助函数

// Compare two elements based on heap type
// 根据堆类型比较两个元素
int PQCompare(PriorityQueue* pPQ, int iParent, int iChild)
{
	if ( pPQ->bIsMinHeap ) {
		return pPQ->piData[iParent] <= pPQ->piData[iChild];
	} else {
		return pPQ->piData[iParent] >= pPQ->piData[iChild];
	}
}

// Swap two elements
// 交换两个元素
void PQSwap(PriorityQueue* pPQ, size_t i, size_t j)
{
	int iTemp = pPQ->piData[i];
	pPQ->piData[i] = pPQ->piData[j];
	pPQ->piData[j] = iTemp;
}

// Heapify down operation
// 向下堆化操作
void PQHeapifyDown(PriorityQueue* pPQ, size_t iIndex)
{
	size_t iSmallest = iIndex;
	size_t iLeft = 2 * iIndex + 1;
	size_t iRight = 2 * iIndex + 2;
	
	// Find smallest/largest among parent and children
	// 在父节点和子节点中找到最小/最大值
	if ( iLeft < pPQ->iSize && !PQCompare(pPQ, iSmallest, iLeft) ) {
		iSmallest = iLeft;
	}
	
	if ( iRight < pPQ->iSize && !PQCompare(pPQ, iSmallest, iRight) ) {
		iSmallest = iRight;
	}
	
	// If smallest is not parent, swap and continue heapifying
	// 如果最小值不是父节点，则交换并继续堆化
	if ( iSmallest != iIndex ) {
		PQSwap(pPQ, iIndex, iSmallest);
		PQHeapifyDown(pPQ, iSmallest);
	}
}

// Heapify up operation
// 向上堆化操作
void PQHeapifyUp(PriorityQueue* pPQ, size_t iIndex)
{
	if ( iIndex == 0 ) return;
	
	size_t iParent = (iIndex - 1) / 2;
	
	if ( !PQCompare(pPQ, iParent, iIndex) ) {
		PQSwap(pPQ, iIndex, iParent);
		PQHeapifyUp(pPQ, iParent);
	}
}

// Insert element into priority queue
// 向优先队列中插入元素
void PQInsert(PriorityQueue* pPQ, int iValue)
{
	if ( pPQ->iSize >= pPQ->iCapacity ) {
		// Resize priority queue
		// 调整优先队列大小
		pPQ->iCapacity *= 2;
		pPQ->piData = xrtRealloc(pPQ->piData, pPQ->iCapacity * sizeof(int));
	}
	
	// Add element at end
	// 在末尾添加元素
	pPQ->piData[pPQ->iSize] = iValue;
	pPQ->iSize++;
	
	// Heapify up
	// 向上堆化
	PQHeapifyUp(pPQ, pPQ->iSize - 1);
}

// Extract root element
// 提取根元素
int PQExtract(PriorityQueue* pPQ)
{
	if ( pPQ->iSize == 0 ) {
		return 0;  // Priority queue is empty / 优先队列为空
	}
	
	int iRoot = pPQ->piData[0];
	
	// Move last element to root
	// 将最后一个元素移到根部
	pPQ->piData[0] = pPQ->piData[pPQ->iSize - 1];
	pPQ->iSize--;
	
	// Heapify down
	// 向下堆化
	PQHeapifyDown(pPQ, 0);
	
	return iRoot;
}

// Peek at root element
// 查看根元素
int PQPeek(PriorityQueue* pPQ)
{
	if ( pPQ->iSize == 0 ) {
		return 0;
	}
	return pPQ->piData[0];
}

// Build heap from array
// 从数组构建堆
void PQBuild(PriorityQueue* pPQ, int* piArray, size_t iSize)
{
	// Copy array to heap
	// 将数组复制到堆中
	for ( size_t i = 0; i < iSize && i < pPQ->iCapacity; i++ ) {
		pPQ->piData[i] = piArray[i];
	}
	pPQ->iSize = iSize;
	
	// Heapify from bottom up
	// 从底部向上堆化
	for ( int i = (iSize / 2) - 1; i >= 0; i-- ) {
		PQHeapifyDown(pPQ, i);
	}
}

// Heap sort implementation
// 堆排序实现
void HeapSort(int* piArray, size_t iSize, int bAscending)
{
	// Create max-heap for ascending sort, min-heap for descending
	// 创建最大堆用于升序排序，最小堆用于降序排序
	PriorityQueue* pPQ = PQCreate(iSize, !bAscending);
	PQBuild(pPQ, piArray, iSize);
	
	// Extract elements one by one
	// 逐个提取元素
	for ( size_t i = 0; i < iSize; i++ ) {
		piArray[i] = PQExtract(pPQ);
	}
	
	PQDestroy(pPQ);
}

// Find top K elements
// 查找前K个元素
void FindTopK(int* piArray, size_t iSize, int iK, int* piResult, int bLargest)
{
	if ( iK > (int)iSize ) iK = iSize;
	
	// Use min-heap for largest K, max-heap for smallest K
	// 对于最大的K使用最小堆，对于最小的K使用最大堆
	PriorityQueue* pPQ = PQCreate(iK, bLargest);
	
	// Insert first K elements
	// 插入前K个元素
	for ( int i = 0; i < iK; i++ ) {
		PQInsert(pPQ, piArray[i]);
	}
	
	// Process remaining elements
	// 处理剩余元素
	for ( size_t i = iK; i < iSize; i++ ) {
		if ( bLargest ) {
			// For largest K, replace if current element is larger than heap root
			// 对于最大的K，如果当前元素大于堆根则替换
			if ( piArray[i] > PQPeek(pPQ) ) {
				PQExtract(pPQ);  // Remove smallest
				PQInsert(pPQ, piArray[i]);
			}
		} else {
			// For smallest K, replace if current element is smaller than heap root
			// 对于最小的K，如果当前元素小于堆根则替换
			if ( piArray[i] < PQPeek(pPQ) ) {
				PQExtract(pPQ);  // Remove largest
				PQInsert(pPQ, piArray[i]);
			}
		}
	}
	
	// Extract results
	// 提取结果
	for ( int i = iK - 1; i >= 0; i-- ) {
		piResult[i] = PQExtract(pPQ);
	}
	
	PQDestroy(pPQ);
}

// Print priority queue structure
// 打印优先队列结构
void PQPrint(PriorityQueue* pPQ, str sLabel)
{
	printf("%s: [", sLabel);
	for ( size_t i = 0; i < pPQ->iSize; i++ ) {
		printf("%d", pPQ->piData[i]);
		if ( i < pPQ->iSize - 1 ) {
			printf(", ");
		}
	}
	printf("] (size: %zu)\n", pPQ->iSize);
}

// Test priority queue operations
// 测试优先队列操作
void TestPQOperations()
{
	printf("=== Priority Queue Operations Test ===\n");
	printf("=== 优先队列操作测试 ===\n");
	
	// Test min-heap
	// 测试最小堆
	printf("\nTesting Min-Heap:\n");
	PriorityQueue* pMinPQ = PQCreate(16, 1);
	
	int arrData[] = {50, 30, 70, 20, 40, 60, 10};
	size_t iDataSize = sizeof(arrData) / sizeof(arrData[0]);
	
	for ( size_t i = 0; i < iDataSize; i++ ) {
		PQInsert(pMinPQ, arrData[i]);
	}
	
	PQPrint(pMinPQ, "Min-heap after insertions");
	
	printf("Extracting elements in ascending order:\n");
	while ( pMinPQ->iSize > 0 ) {
		printf("%d ", PQExtract(pMinPQ));
	}
	printf("\n");
	
	// Test max-heap
	// 测试最大堆
	printf("\nTesting Max-Heap:\n");
	PriorityQueue* pMaxPQ = PQCreate(16, 0);
	
	for ( size_t i = 0; i < iDataSize; i++ ) {
		PQInsert(pMaxPQ, arrData[i]);
	}
	
	PQPrint(pMaxPQ, "Max-heap after insertions");
	
	printf("Extracting elements in descending order:\n");
	while ( pMaxPQ->iSize > 0 ) {
		printf("%d ", PQExtract(pMaxPQ));
	}
	printf("\n");
	
	// Cleanup
	// 清理
	PQDestroy(pMinPQ);
	PQDestroy(pMaxPQ);
}

// Test heap sort
// 测试堆排序
void TestHeapSort()
{
	printf("\n=== Heap Sort Test ===\n");
	printf("=== 堆排序测试 ===\n");
	
	int arrUnsorted[] = {64, 34, 25, 12, 22, 11, 90, 5};
	size_t iSize = sizeof(arrUnsorted) / sizeof(arrUnsorted[0]);
	
	printf("Original array: ");
	for ( size_t i = 0; i < iSize; i++ ) {
		printf("%d ", arrUnsorted[i]);
	}
	printf("\n");
	
	// Ascending sort
	// 升序排序
	int* piAsc = xrtMalloc(iSize * sizeof(int));
	memcpy(piAsc, arrUnsorted, iSize * sizeof(int));
	HeapSort(piAsc, iSize, 1);
	
	printf("Ascending sort: ");
	for ( size_t i = 0; i < iSize; i++ ) {
		printf("%d ", piAsc[i]);
	}
	printf("\n");
	
	// Descending sort
	// 降序排序
	int* piDesc = xrtMalloc(iSize * sizeof(int));
	memcpy(piDesc, arrUnsorted, iSize * sizeof(int));
	HeapSort(piDesc, iSize, 0);
	
	printf("Descending sort: ");
	for ( size_t i = 0; i < iSize; i++ ) {
		printf("%d ", piDesc[i]);
	}
	printf("\n");
	
	xrtFree(piAsc);
	xrtFree(piDesc);
}

// Test top-K algorithms
// 测试Top-K算法
void TestTopK()
{
	printf("\n=== Top-K Algorithms Test ===\n");
	printf("=== Top-K算法测试 ===\n");
	
	int arrData[] = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5};
	size_t iSize = sizeof(arrData) / sizeof(arrData[0]);
	int iK = 4;
	
	printf("Array: ");
	for ( size_t i = 0; i < iSize; i++ ) {
		printf("%d ", arrData[i]);
	}
	printf("\n");
	
	// Find largest K elements
	// 查找最大的K个元素
	int* piLargest = xrtMalloc(iK * sizeof(int));
	FindTopK(arrData, iSize, iK, piLargest, 1);
	
	printf("Largest %d elements: ", iK);
	for ( int i = 0; i < iK; i++ ) {
		printf("%d ", piLargest[i]);
	}
	printf("\n");
	
	// Find smallest K elements
	// 查找最小的K个元素
	int* piSmallest = xrtMalloc(iK * sizeof(int));
	FindTopK(arrData, iSize, iK, piSmallest, 0);
	
	printf("Smallest %d elements: ", iK);
	for ( int i = 0; i < iK; i++ ) {
		printf("%d ", piSmallest[i]);
	}
	printf("\n");
	
	xrtFree(piLargest);
	xrtFree(piSmallest);
}

// Test priority queue simulation
// 测试优先队列模拟
void TestPriorityQueueSimulation()
{
	printf("\n=== Priority Queue Simulation ===\n");
	printf("=== 优先队列模拟 ===\n");
	
	// Simulate task scheduling with priorities
	// 模拟带优先级的任务调度
	PriorityQueue* pPQ = PQCreate(16, 1);  // Min-heap for lowest priority first / 最小堆优先处理最低优先级
	
	// Insert tasks with priorities
	// 插入带优先级的任务
	PQInsert(pPQ, 3);  // Low priority task / 低优先级任务
	PQInsert(pPQ, 1);  // High priority task / 高优先级任务
	PQInsert(pPQ, 4);  // Low priority task / 低优先级任务
	PQInsert(pPQ, 2);  // Medium priority task / 中等优先级任务
	
	printf("Processing tasks in priority order:\n");
	while ( pPQ->iSize > 0 ) {
		int iPriority = PQExtract(pPQ);
		printf("Processing task with priority %d\n", iPriority);
	}
	
	PQDestroy(pPQ);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Heap Priority Queue Demo\n");
	printf("XRT 堆优先队列演示\n");
	printf("==========================\n\n");
	
	// Run all tests
	// 运行所有测试
	TestPQOperations();
	TestHeapSort();
	TestTopK();
	TestPriorityQueueSimulation();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}