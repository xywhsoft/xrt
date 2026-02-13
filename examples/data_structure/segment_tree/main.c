/*
 * XRT Example - Segment Tree
 * XRT 范例 - 线段树
 *
 * Description / 说明:
 *   EN: Demonstrates segment tree implementation for efficient range queries
 *       and point updates. Includes sum, minimum, maximum, and range update operations.
 *       Shows O(log n) query and update performance for interval operations.
 *   CN: 演示线段树实现，用于高效的区间查询和点更新。
 *       包括求和、最小值、最大值和区间更新操作。
 *       展示区间操作的O(log n)查询和更新性能。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/data_structure_segment_tree.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/data_structure_segment_tree -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Range query and update operations
 *   - Sum, min, max aggregations
 *   - Lazy propagation for range updates
 *   - Interval tree implementation
 *   - Logarithmic time complexity
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#include <limits.h>
#include <time.h>

// Helper functions
// 辅助函数
int xrtMin(int a, int b)
{
	return (a < b) ? a : b;
}

int xrtMax(int a, int b)
{
	return (a > b) ? a : b;
}

// Segment tree node structure
// 线段树节点结构
typedef struct {
	int iSum;		// Sum of range / 区间和
	int iMin;		// Minimum value in range / 区间最小值
	int iMax;		// Maximum value in range / 区间最大值
	int iLazy;		// Lazy propagation value / 延迟传播值
	int bPending;	// Pending update flag / 待更新标志
} SegTreeNode;

// Segment tree structure
// 线段树结构
typedef struct {
	SegTreeNode* paNodes;	// Tree nodes array / 树节点数组
	int* piOriginal;		// Original array / 原始数组
	size_t iSize;			// Size of original array / 原始数组大小
	size_t iTreeSize;		// Size of segment tree / 线段树大小
} SegmentTree;

// Create segment tree
// 创建线段树
SegmentTree* SegTreeCreate(int* piArray, size_t iSize)
{
	SegmentTree* pTree = xrtMalloc(sizeof(SegmentTree));
	pTree->piOriginal = xrtMalloc(iSize * sizeof(int));
	memcpy(pTree->piOriginal, piArray, iSize * sizeof(int));
	pTree->iSize = iSize;
	
	// Calculate tree size (next power of 2)
	// 计算树大小（下一个2的幂）
	size_t iTreeSize = 1;
	while ( iTreeSize < iSize ) {
		iTreeSize <<= 1;
	}
	pTree->iTreeSize = iTreeSize * 2;  // 2 * n for segment tree / 线段树为2*n
	
	pTree->paNodes = xrtMalloc(pTree->iTreeSize * sizeof(SegTreeNode));
	memset(pTree->paNodes, 0, pTree->iTreeSize * sizeof(SegTreeNode));
	
	return pTree;
}

// Destroy segment tree
// 销毁线段树
void SegTreeDestroy(SegmentTree* pTree)
{
	if ( pTree ) {
		if ( pTree->paNodes ) {
			xrtFree(pTree->paNodes);
		}
		if ( pTree->piOriginal ) {
			xrtFree(pTree->piOriginal);
		}
		xrtFree(pTree);
	}
}

// Build segment tree recursively
// 递归构建线段树
void SegTreeBuild(SegmentTree* pTree, size_t iNode, size_t iStart, size_t iEnd)
{
	if ( iStart == iEnd ) {
		// Leaf node
		// 叶节点
		pTree->paNodes[iNode].iSum = pTree->piOriginal[iStart];
		pTree->paNodes[iNode].iMin = pTree->piOriginal[iStart];
		pTree->paNodes[iNode].iMax = pTree->piOriginal[iStart];
		pTree->paNodes[iNode].iLazy = 0;
		pTree->paNodes[iNode].bPending = 0;
	} else {
		size_t iMid = (iStart + iEnd) / 2;
		size_t iLeft = 2 * iNode + 1;
		size_t iRight = 2 * iNode + 2;
		
		// Build left and right subtrees
		// 构建左右子树
		SegTreeBuild(pTree, iLeft, iStart, iMid);
		SegTreeBuild(pTree, iRight, iMid + 1, iEnd);
		
		// Combine results
		// 合并结果
		pTree->paNodes[iNode].iSum = pTree->paNodes[iLeft].iSum + pTree->paNodes[iRight].iSum;
		pTree->paNodes[iNode].iMin = xrtMin(pTree->paNodes[iLeft].iMin, pTree->paNodes[iRight].iMin);
		pTree->paNodes[iNode].iMax = xrtMax(pTree->paNodes[iLeft].iMax, pTree->paNodes[iRight].iMax);
		pTree->paNodes[iNode].iLazy = 0;
		pTree->paNodes[iNode].bPending = 0;
	}
}

// Push lazy updates down
// 向下推送延迟更新
void SegTreePush(SegmentTree* pTree, size_t iNode, size_t iStart, size_t iEnd)
{
	if ( pTree->paNodes[iNode].bPending ) {
		// Apply pending update
		// 应用待更新
		size_t iRange = iEnd - iStart + 1;
		pTree->paNodes[iNode].iSum += pTree->paNodes[iNode].iLazy * iRange;
		pTree->paNodes[iNode].iMin += pTree->paNodes[iNode].iLazy;
		pTree->paNodes[iNode].iMax += pTree->paNodes[iNode].iLazy;
		
		// Propagate to children if not leaf
		// 如果不是叶节点则传播给子节点
		if ( iStart != iEnd ) {
			size_t iLeft = 2 * iNode + 1;
			size_t iRight = 2 * iNode + 2;
			
			pTree->paNodes[iLeft].iLazy += pTree->paNodes[iNode].iLazy;
			pTree->paNodes[iRight].iLazy += pTree->paNodes[iNode].iLazy;
			pTree->paNodes[iLeft].bPending = 1;
			pTree->paNodes[iRight].bPending = 1;
		}
		
		// Clear pending flag
		// 清除待更新标志
		pTree->paNodes[iNode].iLazy = 0;
		pTree->paNodes[iNode].bPending = 0;
	}
}

// Range update with lazy propagation
// 带延迟传播的区间更新
void SegTreeUpdateRange(SegmentTree* pTree, size_t iNode, size_t iStart, size_t iEnd, 
                       size_t iRangeStart, size_t iRangeEnd, int iDelta)
{
	// Push pending updates
	// 推送待更新
	SegTreePush(pTree, iNode, iStart, iEnd);
	
	if ( iStart > iEnd || iStart > iRangeEnd || iEnd < iRangeStart ) {
		return;  // No overlap / 无重叠
	}
	
	if ( iStart >= iRangeStart && iEnd <= iRangeEnd ) {
		// Complete overlap
		// 完全覆盖
		size_t iRange = iEnd - iStart + 1;
		pTree->paNodes[iNode].iSum += iDelta * iRange;
		pTree->paNodes[iNode].iMin += iDelta;
		pTree->paNodes[iNode].iMax += iDelta;
		
		// Propagate to children if not leaf
		// 如果不是叶节点则传播给子节点
		if ( iStart != iEnd ) {
			size_t iLeft = 2 * iNode + 1;
			size_t iRight = 2 * iNode + 2;
			
			pTree->paNodes[iLeft].iLazy += iDelta;
			pTree->paNodes[iRight].iLazy += iDelta;
			pTree->paNodes[iLeft].bPending = 1;
			pTree->paNodes[iRight].bPending = 1;
		}
	} else {
		// Partial overlap
		// 部分重叠
		size_t iMid = (iStart + iEnd) / 2;
		size_t iLeft = 2 * iNode + 1;
		size_t iRight = 2 * iNode + 2;
		
		SegTreeUpdateRange(pTree, iLeft, iStart, iMid, iRangeStart, iRangeEnd, iDelta);
		SegTreeUpdateRange(pTree, iRight, iMid + 1, iEnd, iRangeStart, iRangeEnd, iDelta);
		
		// Update current node
		// 更新当前节点
		pTree->paNodes[iNode].iSum = pTree->paNodes[iLeft].iSum + pTree->paNodes[iRight].iSum;
		pTree->paNodes[iNode].iMin = xrtMin(pTree->paNodes[iLeft].iMin, pTree->paNodes[iRight].iMin);
		pTree->paNodes[iNode].iMax = xrtMax(pTree->paNodes[iLeft].iMax, pTree->paNodes[iRight].iMax);
	}
}

// Point update
// 点更新
void SegTreeUpdatePoint(SegmentTree* pTree, size_t iNode, size_t iStart, size_t iEnd, 
                       size_t iIndex, int iValue)
{
	// Push pending updates
	// 推送待更新
	SegTreePush(pTree, iNode, iStart, iEnd);
	
	if ( iStart == iEnd ) {
		// Leaf node
		// 叶节点
		pTree->paNodes[iNode].iSum = iValue;
		pTree->paNodes[iNode].iMin = iValue;
		pTree->paNodes[iNode].iMax = iValue;
	} else {
		size_t iMid = (iStart + iEnd) / 2;
		size_t iLeft = 2 * iNode + 1;
		size_t iRight = 2 * iNode + 2;
		
		if ( iIndex <= iMid ) {
			SegTreeUpdatePoint(pTree, iLeft, iStart, iMid, iIndex, iValue);
		} else {
			SegTreeUpdatePoint(pTree, iRight, iMid + 1, iEnd, iIndex, iValue);
		}
		
		// Update current node
		// 更新当前节点
		pTree->paNodes[iNode].iSum = pTree->paNodes[iLeft].iSum + pTree->paNodes[iRight].iSum;
		pTree->paNodes[iNode].iMin = xrtMin(pTree->paNodes[iLeft].iMin, pTree->paNodes[iRight].iMin);
		pTree->paNodes[iNode].iMax = xrtMax(pTree->paNodes[iLeft].iMax, pTree->paNodes[iRight].iMax);
	}
}

// Range sum query
// 区间求和查询
int SegTreeQuerySum(SegmentTree* pTree, size_t iNode, size_t iStart, size_t iEnd,
                   size_t iRangeStart, size_t iRangeEnd)
{
	// Push pending updates
	// 推送待更新
	SegTreePush(pTree, iNode, iStart, iEnd);
	
	if ( iStart > iEnd || iStart > iRangeEnd || iEnd < iRangeStart ) {
		return 0;  // No overlap / 无重叠
	}
	
	if ( iStart >= iRangeStart && iEnd <= iRangeEnd ) {
		// Complete overlap
		// 完全覆盖
		return pTree->paNodes[iNode].iSum;
	}
	
	// Partial overlap
	// 部分重叠
	size_t iMid = (iStart + iEnd) / 2;
	size_t iLeft = 2 * iNode + 1;
	size_t iRight = 2 * iNode + 2;
	
	return SegTreeQuerySum(pTree, iLeft, iStart, iMid, iRangeStart, iRangeEnd) +
	       SegTreeQuerySum(pTree, iRight, iMid + 1, iEnd, iRangeStart, iRangeEnd);
}

// Range minimum query
// 区间最小值查询
int SegTreeQueryMin(SegmentTree* pTree, size_t iNode, size_t iStart, size_t iEnd,
                   size_t iRangeStart, size_t iRangeEnd)
{
	// Push pending updates
	// 推送待更新
	SegTreePush(pTree, iNode, iStart, iEnd);
	
	if ( iStart > iEnd || iStart > iRangeEnd || iEnd < iRangeStart ) {
		return INT_MAX;  // No overlap / 无重叠
	}
	
	if ( iStart >= iRangeStart && iEnd <= iRangeEnd ) {
		// Complete overlap
		// 完全覆盖
		return pTree->paNodes[iNode].iMin;
	}
	
	// Partial overlap
	// 部分重叠
	size_t iMid = (iStart + iEnd) / 2;
	size_t iLeft = 2 * iNode + 1;
	size_t iRight = 2 * iNode + 2;
	
	int iLeftMin = SegTreeQueryMin(pTree, iLeft, iStart, iMid, iRangeStart, iRangeEnd);
	int iRightMin = SegTreeQueryMin(pTree, iRight, iMid + 1, iEnd, iRangeStart, iRangeEnd);
	
	return xrtMin(iLeftMin, iRightMin);
}

// Range maximum query
// 区间最大值查询
int SegTreeQueryMax(SegmentTree* pTree, size_t iNode, size_t iStart, size_t iEnd,
                   size_t iRangeStart, size_t iRangeEnd)
{
	// Push pending updates
	// 推送待更新
	SegTreePush(pTree, iNode, iStart, iEnd);
	
	if ( iStart > iEnd || iStart > iRangeEnd || iEnd < iRangeStart ) {
		return INT_MIN;  // No overlap / 无重叠
	}
	
	if ( iStart >= iRangeStart && iEnd <= iRangeEnd ) {
		// Complete overlap
		// 完全覆盖
		return pTree->paNodes[iNode].iMax;
	}
	
	// Partial overlap
	// 部分重叠
	size_t iMid = (iStart + iEnd) / 2;
	size_t iLeft = 2 * iNode + 1;
	size_t iRight = 2 * iNode + 2;
	
	int iLeftMax = SegTreeQueryMax(pTree, iLeft, iStart, iMid, iRangeStart, iRangeEnd);
	int iRightMax = SegTreeQueryMax(pTree, iRight, iMid + 1, iEnd, iRangeStart, iRangeEnd);
	
	return xrtMax(iLeftMax, iRightMax);
}

// Initialize segment tree
// 初始化线段树
void SegTreeInit(SegmentTree* pTree)
{
	SegTreeBuild(pTree, 0, 0, pTree->iSize - 1);
}

// Print array
// 打印数组
void PrintArray(int* piArray, size_t iSize, str sLabel)
{
	printf("%s: [", sLabel);
	for ( size_t i = 0; i < iSize; i++ ) {
		printf("%d", piArray[i]);
		if ( i < iSize - 1 ) {
			printf(", ");
		}
	}
	printf("]\n");
}

// Test segment tree operations
// 测试线段树操作
void TestSegmentTree()
{
	printf("=== Segment Tree Test ===\n");
	printf("=== 线段树测试 ===\n");
	
	// Create test array
	// 创建测试数组
	int arrData[] = {1, 3, 5, 7, 9, 11, 13, 15};
	size_t iSize = sizeof(arrData) / sizeof(arrData[0]);
	
	PrintArray(arrData, iSize, "Original array");
	
	// Create and initialize segment tree
	// 创建并初始化线段树
	SegmentTree* pTree = SegTreeCreate(arrData, iSize);
	SegTreeInit(pTree);
	
	// Test range queries
	// 测试区间查询
	printf("\nTesting range queries:\n");
	
	int iSum = SegTreeQuerySum(pTree, 0, 0, iSize - 1, 1, 4);
	printf("Sum of range [1,4]: %d (expected: 3+5+7+9 = 24)\n", iSum);
	
	int iMin = SegTreeQueryMin(pTree, 0, 0, iSize - 1, 2, 6);
	printf("Min of range [2,6]: %d (expected: 5)\n", iMin);
	
	int iMax = SegTreeQueryMax(pTree, 0, 0, iSize - 1, 0, 3);
	printf("Max of range [0,3]: %d (expected: 7)\n", iMax);
	
	// Test point updates
	// 测试点更新
	printf("\nTesting point updates:\n");
	SegTreeUpdatePoint(pTree, 0, 0, iSize - 1, 2, 100);  // Update index 2 to 100
	printf("Updated index 2 to 100\n");
	
	iSum = SegTreeQuerySum(pTree, 0, 0, iSize - 1, 1, 4);
	printf("New sum of range [1,4]: %d (expected: 3+100+7+9 = 119)\n", iSum);
	
	iMin = SegTreeQueryMin(pTree, 0, 0, iSize - 1, 0, 4);
	printf("New min of range [0,4]: %d (expected: 1)\n", iMin);
	
	iMax = SegTreeQueryMax(pTree, 0, 0, iSize - 1, 0, 4);
	printf("New max of range [0,4]: %d (expected: 100)\n", iMax);
	
	// Test range updates with lazy propagation
	// 测试带延迟传播的区间更新
	printf("\nTesting range updates with lazy propagation:\n");
	SegTreeUpdateRange(pTree, 0, 0, iSize - 1, 1, 3, 10);  // Add 10 to range [1,3]
	printf("Added 10 to range [1,3]\n");
	
	iSum = SegTreeQuerySum(pTree, 0, 0, iSize - 1, 0, 5);
	printf("New sum of range [0,5]: %d\n", iSum);
	
	iMin = SegTreeQueryMin(pTree, 0, 0, iSize - 1, 0, 5);
	printf("New min of range [0,5]: %d\n", iMin);
	
	iMax = SegTreeQueryMax(pTree, 0, 0, iSize - 1, 0, 5);
	printf("New max of range [0,5]: %d\n", iMax);
	
	// Cleanup
	// 清理
	SegTreeDestroy(pTree);
}

// Performance comparison
// 性能比较
void TestPerformance()
{
	printf("\n=== Performance Test ===\n");
	printf("=== 性能测试 ===\n");
	
	// Large array test
	// 大数组测试
	size_t iLargeSize = 100000;
	int* piLargeArray = xrtMalloc(iLargeSize * sizeof(int));
	
	srand((unsigned int)time(NULL));
	for ( size_t i = 0; i < iLargeSize; i++ ) {
		piLargeArray[i] = rand() % 1000;
	}
	
	// Build segment tree
	// 构建线段树
	clock_t clkStart = clock();
	SegmentTree* pTree = SegTreeCreate(piLargeArray, iLargeSize);
	SegTreeInit(pTree);
	clock_t clkEnd = clock();
	
	double dBuildTime = ((double)(clkEnd - clkStart)) / CLOCKS_PER_SEC;
	printf("Building segment tree for %zu elements: %.6f seconds\n", iLargeSize, dBuildTime);
	
	// Test many queries
	// 测试多次查询
	clkStart = clock();
	
	int iTotalSum = 0;
	for ( int i = 0; i < 10000; i++ ) {
		size_t iStart = rand() % (iLargeSize / 2);
		size_t iEnd = iStart + rand() % (iLargeSize / 2);
		if ( iEnd >= iLargeSize ) iEnd = iLargeSize - 1;
		
		iTotalSum += SegTreeQuerySum(pTree, 0, 0, iLargeSize - 1, iStart, iEnd);
	}
	
	clkEnd = clock();
	double dQueryTime = ((double)(clkEnd - clkStart)) / CLOCKS_PER_SEC;
	printf("10000 range sum queries: %.6f seconds (total sum: %d)\n", dQueryTime, iTotalSum);
	
	// Test many updates
	// 测试多次更新
	clkStart = clock();
	
	for ( int i = 0; i < 10000; i++ ) {
		size_t iIndex = rand() % iLargeSize;
		int iValue = rand() % 1000;
		SegTreeUpdatePoint(pTree, 0, 0, iLargeSize - 1, iIndex, iValue);
	}
	
	clkEnd = clock();
	double dUpdateTime = ((double)(clkEnd - clkStart)) / CLOCKS_PER_SEC;
	printf("10000 point updates: %.6f seconds\n", dUpdateTime);
	
	// Cleanup
	// 清理
	SegTreeDestroy(pTree);
	xrtFree(piLargeArray);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Segment Tree Demo\n");
	printf("XRT 线段树演示\n");
	printf("===================\n\n");
	
	// Run tests
	// 运行测试
	TestSegmentTree();
	TestPerformance();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}