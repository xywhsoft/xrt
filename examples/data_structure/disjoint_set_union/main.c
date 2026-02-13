/*
 * XRT Example - Disjoint Set Union (Union-Find)
 * XRT 范例 - 并查集（Union-Find）
 *
 * Description / 说明:
 *   EN: Demonstrates disjoint set union data structure with path compression
 *       and union by rank optimizations. Includes connected components detection,
 *       cycle detection in graphs, and equivalence class management.
 *       Shows near-constant time operations for union and find.
 *   CN: 演示带有路径压缩和按秩合并优化的并查集数据结构。
 *       包括连通分量检测、图中的环检测和等价类管理。
 *       展示接近常数时间的合并和查找操作。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/data_structure_disjoint_set_union.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/data_structure_disjoint_set_union -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Path compression optimization
 *   - Union by rank heuristic
 *   - Connected components detection
 *   - Cycle detection in graphs
 *   - Nearly O(1) amortized operations
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#include <time.h>

// Disjoint set structure
// 并查集结构
typedef struct {
	int* piParent;		// Parent array / 父数组
	int* piRank;		// Rank array for union by rank / 用于按秩合并的秩数组
	int* piSize;		// Size of each component / 每个分量的大小
	size_t iElementCount;	// Number of elements / 元素数量
} DisjointSet;

// Create disjoint set
// 创建并查集
DisjointSet* DSUCreate(size_t iSize)
{
	DisjointSet* pDSU = xrtMalloc(sizeof(DisjointSet));
	pDSU->iElementCount = iSize;
	pDSU->piParent = xrtMalloc(iSize * sizeof(int));
	pDSU->piRank = xrtMalloc(iSize * sizeof(int));
	pDSU->piSize = xrtMalloc(iSize * sizeof(int));
	
	// Initialize each element as its own parent
	// 将每个元素初始化为自己的父节点
	for ( size_t i = 0; i < iSize; i++ ) {
		pDSU->piParent[i] = (int)i;
		pDSU->piRank[i] = 0;
		pDSU->piSize[i] = 1;
	}
	
	return pDSU;
}

// Destroy disjoint set
// 销毁并查集
void DSUDestroy(DisjointSet* pDSU)
{
	if ( pDSU ) {
		if ( pDSU->piParent ) xrtFree(pDSU->piParent);
		if ( pDSU->piRank ) xrtFree(pDSU->piRank);
		if ( pDSU->piSize ) xrtFree(pDSU->piSize);
		xrtFree(pDSU);
	}
}

// Find root with path compression
// 带路径压缩的查找根节点
int DSUFind(DisjointSet* pDSU, int iElement)
{
	if ( pDSU->piParent[iElement] != iElement ) {
		// Path compression: make parent point directly to root
		// 路径压缩：使父节点直接指向根节点
		pDSU->piParent[iElement] = DSUFind(pDSU, pDSU->piParent[iElement]);
	}
	return pDSU->piParent[iElement];
}

// Union by rank
// 按秩合并
void DSUUnion(DisjointSet* pDSU, int iElement1, int iElement2)
{
	int iRoot1 = DSUFind(pDSU, iElement1);
	int iRoot2 = DSUFind(pDSU, iElement2);
	
	if ( iRoot1 == iRoot2 ) {
		return;  // Already in same set / 已在同一集合中
	}
	
	// Union by rank: attach smaller rank tree under root of higher rank tree
	// 按秩合并：将较小秩的树附加到较大秩的树的根节点下
	if ( pDSU->piRank[iRoot1] < pDSU->piRank[iRoot2] ) {
		pDSU->piParent[iRoot1] = iRoot2;
		pDSU->piSize[iRoot2] += pDSU->piSize[iRoot1];
	} else if ( pDSU->piRank[iRoot1] > pDSU->piRank[iRoot2] ) {
		pDSU->piParent[iRoot2] = iRoot1;
		pDSU->piSize[iRoot1] += pDSU->piSize[iRoot2];
	} else {
		// Equal ranks: arbitrarily choose one as root and increment its rank
		// 秩相等：任意选择一个作为根节点并增加其秩
		pDSU->piParent[iRoot2] = iRoot1;
		pDSU->piRank[iRoot1]++;
		pDSU->piSize[iRoot1] += pDSU->piSize[iRoot2];
	}
}

// Check if two elements are connected
// 检查两个元素是否连通
int DSUConnected(DisjointSet* pDSU, int iElement1, int iElement2)
{
	return DSUFind(pDSU, iElement1) == DSUFind(pDSU, iElement2);
}

// Get size of component containing element
// 获取包含元素的分量大小
int DSUComponentSize(DisjointSet* pDSU, int iElement)
{
	int iRoot = DSUFind(pDSU, iElement);
	return pDSU->piSize[iRoot];
}

// Count number of connected components
// 计算连通分量数量
int DSUComponentCount(DisjointSet* pDSU)
{
	int iCount = 0;
	for ( size_t i = 0; i < pDSU->iElementCount; i++ ) {
		if ( pDSU->piParent[i] == (int)i ) {  // Root node / 根节点
			iCount++;
		}
	}
	return iCount;
}

// Get representative of component
// 获取分量的代表元
int DSUFindRepresentative(DisjointSet* pDSU, int iElement)
{
	return DSUFind(pDSU, iElement);
}

// Print disjoint set structure
// 打印并查集结构
void DSUPrint(DisjointSet* pDSU)
{
	printf("Disjoint Set Structure:\n");
	
	// Group elements by their root
	// 按根节点对元素分组
	int* piVisited = xrtMalloc(pDSU->iElementCount * sizeof(int));
	memset(piVisited, 0, pDSU->iElementCount * sizeof(int));
	
	for ( size_t i = 0; i < pDSU->iElementCount; i++ ) {
		if ( !piVisited[i] ) {
			int iRoot = DSUFind(pDSU, (int)i);
			printf("Component with root %d: {", iRoot);
			
			int bFirst = 1;
			for ( size_t j = 0; j < pDSU->iElementCount; j++ ) {
				if ( DSUFind(pDSU, (int)j) == iRoot ) {
					if ( !bFirst ) printf(", ");
					printf("%zu", j);
					piVisited[j] = 1;
					bFirst = 0;
				}
			}
			printf("} (size: %d)\n", pDSU->piSize[iRoot]);
		}
	}
	
	xrtFree(piVisited);
}

// Test basic DSU operations
// 测试基本DSU操作
void TestBasicOperations()
{
	printf("=== Basic DSU Operations Test ===\n");
	printf("=== 基本DSU操作测试 ===\n");
	
	// Create DSU for 10 elements (0-9)
	// 为10个元素(0-9)创建DSU
	DisjointSet* pDSU = DSUCreate(10);
	
	// Initial state
	// 初始状态
	printf("Initial state:\n");
	DSUPrint(pDSU);
	printf("Component count: %d\n", DSUComponentCount(pDSU));
	
	// Perform unions
	// 执行合并操作
	printf("\nPerforming unions:\n");
	DSUUnion(pDSU, 0, 1);
	DSUUnion(pDSU, 1, 2);
	DSUUnion(pDSU, 3, 4);
	DSUUnion(pDSU, 4, 5);
	DSUUnion(pDSU, 0, 3);  // Connect components {0,1,2} and {3,4,5}
	
	DSUPrint(pDSU);
	printf("Component count: %d\n", DSUComponentCount(pDSU));
	
	// Test connections
	// 测试连通性
	printf("\nTesting connections:\n");
	int arrPairs[][2] = {{0, 2}, {1, 5}, {2, 4}, {0, 6}, {7, 8}};
	
	for ( int i = 0; i < 5; i++ ) {
		int iConnected = DSUConnected(pDSU, arrPairs[i][0], arrPairs[i][1]);
		printf("  %d and %d: %s\n", 
		       arrPairs[i][0], arrPairs[i][1], 
		       iConnected ? "Connected" : "Not connected");
	}
	
	// Test component sizes
	// 测试分量大小
	printf("\nComponent sizes:\n");
	for ( int i = 0; i < 10; i++ ) {
		if ( DSUFindRepresentative(pDSU, i) == i ) {  // Only print for roots
			printf("  Component rooted at %d: size %d\n", 
			       i, DSUComponentSize(pDSU, i));
		}
	}
	
	// Cleanup
	// 清理
	DSUDestroy(pDSU);
}

// Test cycle detection in graph
// 测试图中的环检测
void TestCycleDetection()
{
	printf("\n=== Cycle Detection Test ===\n");
	printf("=== 环检测测试 ===\n");
	
	// Test graph represented as edges
	// 用边表示的测试图
	// Edges: (0,1), (1,2), (2,3), (3,4), (4,1) - contains cycle
	// 边：(0,1), (1,2), (2,3), (3,4), (4,1) - 包含环
	
	typedef struct {
		int iFrom;
		int iTo;
	} Edge;
	
	Edge arrEdges[] = {
		{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 1}
	};
	
	int iVertexCount = 5;
	int iEdgeCount = sizeof(arrEdges) / sizeof(arrEdges[0]);
	
	DisjointSet* pDSU = DSUCreate(iVertexCount);
	int bHasCycle = 0;
	
	printf("Checking for cycles in graph:\n");
	for ( int i = 0; i < iEdgeCount; i++ ) {
		int iFrom = arrEdges[i].iFrom;
		int iTo = arrEdges[i].iTo;
		
		printf("  Processing edge (%d,%d): ", iFrom, iTo);
		
		if ( DSUConnected(pDSU, iFrom, iTo) ) {
			printf("CYCLE DETECTED!\n");
			bHasCycle = 1;
			break;
		} else {
			printf("No cycle, merging components\n");
			DSUUnion(pDSU, iFrom, iTo);
		}
	}
	
	if ( !bHasCycle ) {
		printf("No cycles found in graph\n");
	}
	
	DSUDestroy(pDSU);
}

// Test connected components
// 测试连通分量
void TestConnectedComponents()
{
	printf("\n=== Connected Components Test ===\n");
	printf("=== 连通分量测试 ===\n");
	
	// Graph with multiple connected components
	// 具有多个连通分量的图
	// Component 1: 0-1-2-3
	// Component 2: 4-5-6
	// Component 3: 7-8
	// Component 4: 9 (isolated)
	
	typedef struct {
		int iFrom;
		int iTo;
	} Edge;
	
	Edge arrEdges[] = {
		{0, 1}, {1, 2}, {2, 3},  // Component 1
		{4, 5}, {5, 6},          // Component 2
		{7, 8}                   // Component 3
		// 9 is isolated / 9是孤立的
	};
	
	int iVertexCount = 10;
	int iEdgeCount = sizeof(arrEdges) / sizeof(arrEdges[0]);
	
	DisjointSet* pDSU = DSUCreate(iVertexCount);
	
	// Process all edges
	// 处理所有边
	for ( int i = 0; i < iEdgeCount; i++ ) {
		DSUUnion(pDSU, arrEdges[i].iFrom, arrEdges[i].iTo);
	}
	
	// Display connected components
	// 显示连通分量
	DSUPrint(pDSU);
	printf("Total connected components: %d\n", DSUComponentCount(pDSU));
	
	// Verify specific connections
	// 验证特定连接
	printf("\nVerification:\n");
	int arrTestPairs[][2] = {{0, 3}, {4, 6}, {7, 8}, {0, 4}, {1, 9}};
	
	for ( int i = 0; i < 5; i++ ) {
		int iFrom = arrTestPairs[i][0];
		int iTo = arrTestPairs[i][1];
		int iConnected = DSUConnected(pDSU, iFrom, iTo);
		printf("  %d and %d: %s\n", iFrom, iTo, iConnected ? "Connected" : "Not connected");
	}
	
	DSUDestroy(pDSU);
}

// Performance test
// 性能测试
void TestPerformance()
{
	printf("\n=== Performance Test ===\n");
	printf("=== 性能测试 ===\n");
	
	size_t iSize = 100000;
	DisjointSet* pDSU = DSUCreate(iSize);
	
	// Random union operations
	// 随机合并操作
	srand((unsigned int)time(NULL));
	
	clock_t clkStart = clock();
	
	for ( int i = 0; i < 50000; i++ ) {
		int iElement1 = rand() % iSize;
		int iElement2 = rand() % iSize;
		DSUUnion(pDSU, iElement1, iElement2);
	}
	
	clock_t clkEnd = clock();
	double dUnionTime = ((double)(clkEnd - clkStart)) / CLOCKS_PER_SEC;
	
	// Random find operations
	// 随机查找操作
	clkStart = clock();
	
	int iFindCount = 0;
	for ( int i = 0; i < 50000; i++ ) {
		int iElement = rand() % iSize;
		DSUFind(pDSU, iElement);
		iFindCount++;
	}
	
	clkEnd = clock();
	double dFindTime = ((double)(clkEnd - clkStart)) / CLOCKS_PER_SEC;
	
	printf("Performance with %zu elements:\n", iSize);
	printf("  50000 union operations: %.6f seconds\n", dUnionTime);
	printf("  50000 find operations: %.6f seconds\n", dFindTime);
	printf("  Average time per operation: %.9f seconds\n", 
	       (dUnionTime + dFindTime) / 100000);
	printf("  Connected components: %d\n", DSUComponentCount(pDSU));
	
	DSUDestroy(pDSU);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Disjoint Set Union (Union-Find) Demo\n");
	printf("XRT 并查集（Union-Find）演示\n");
	printf("=====================================\n\n");
	
	// Run all tests
	// 运行所有测试
	TestBasicOperations();
	TestCycleDetection();
	TestConnectedComponents();
	TestPerformance();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}