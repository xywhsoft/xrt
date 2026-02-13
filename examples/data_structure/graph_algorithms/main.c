/*
 * XRT Example - Graph Algorithms
 * XRT 范例 - 图算法
 *
 * Description / 说明:
 *   EN: Demonstrates graph data structure implementation and fundamental algorithms
 *       including breadth-first search (BFS), depth-first search (DFS),
 *       shortest path (Dijkstra), and minimum spanning tree (Prim's algorithm).
 *       Supports both directed and undirected graphs with adjacency list representation.
 *   CN: 演示图数据结构实现和基本算法，包括广度优先搜索(BFS)、深度优先搜索(DFS)、
 *       最短路径(Dijkstra)和最小生成树(Prim算法)。
 *       支持有向图和无向图，使用邻接表表示法。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/data_structure_graph_algorithms.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/data_structure_graph_algorithms -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Adjacency list graph representation
 *   - BFS and DFS traversal algorithms
 *   - Dijkstra's shortest path algorithm
 *   - Prim's minimum spanning tree algorithm
 *   - Weighted and unweighted graph support
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#include <limits.h>
#include <time.h>

// Graph edge structure
// 图边结构
typedef struct Edge {
	int iDest;			// Destination vertex / 目标顶点
	int iWeight;		// Edge weight / 边权重
	struct Edge* pNext;	// Next edge in adjacency list / 邻接表中的下一条边
} Edge;

// Graph structure
// 图结构
typedef struct {
	int iVertexCount;	// Number of vertices / 顶点数量
	Edge** ppAdjList;	// Adjacency list array / 邻接表数组
	int bDirected;		// 1 for directed, 0 for undirected / 1为有向，0为无向
} Graph;

// Queue structure for BFS
// BFS队列结构
typedef struct QueueNode {
	int iVertex;
	struct QueueNode* pNext;
} QueueNode;

typedef struct {
	QueueNode* pFront;
	QueueNode* pRear;
} Queue;

// Create new edge
// 创建新边
Edge* EdgeCreate(int iDest, int iWeight)
{
	Edge* pEdge = xrtMalloc(sizeof(Edge));
	pEdge->iDest = iDest;
	pEdge->iWeight = iWeight;
	pEdge->pNext = NULL;
	return pEdge;
}

// Create graph
// 创建图
Graph* GraphCreate(int iVertices, int bDirected)
{
	Graph* pGraph = xrtMalloc(sizeof(Graph));
	pGraph->iVertexCount = iVertices;
	pGraph->bDirected = bDirected;
	pGraph->ppAdjList = xrtMalloc(iVertices * sizeof(Edge*));
	
	// Initialize adjacency lists
	// 初始化邻接表
	for ( int i = 0; i < iVertices; i++ ) {
		pGraph->ppAdjList[i] = NULL;
	}
	
	return pGraph;
}

// Add edge to graph
// 向图添加边
void GraphAddEdge(Graph* pGraph, int iSrc, int iDest, int iWeight)
{
	// Add edge from src to dest
	// 添加从src到dest的边
	Edge* pEdge = EdgeCreate(iDest, iWeight);
	pEdge->pNext = pGraph->ppAdjList[iSrc];
	pGraph->ppAdjList[iSrc] = pEdge;
	
	// For undirected graph, add reverse edge
	// 对于无向图，添加反向边
	if ( !pGraph->bDirected ) {
		Edge* pReverseEdge = EdgeCreate(iSrc, iWeight);
		pReverseEdge->pNext = pGraph->ppAdjList[iDest];
		pGraph->ppAdjList[iDest] = pReverseEdge;
	}
}

// Create queue
// 创建队列
Queue* QueueCreate()
{
	Queue* pQueue = xrtMalloc(sizeof(Queue));
	pQueue->pFront = NULL;
	pQueue->pRear = NULL;
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

// Enqueue vertex
// 入队顶点
void QueueEnqueue(Queue* pQueue, int iVertex)
{
	QueueNode* pNode = xrtMalloc(sizeof(QueueNode));
	pNode->iVertex = iVertex;
	pNode->pNext = NULL;
	
	if ( pQueue->pRear == NULL ) {
		pQueue->pFront = pNode;
		pQueue->pRear = pNode;
	} else {
		pQueue->pRear->pNext = pNode;
		pQueue->pRear = pNode;
	}
}

// Dequeue vertex
// 出队顶点
int QueueDequeue(Queue* pQueue)
{
	if ( pQueue->pFront == NULL ) {
		return -1;
	}
	
	QueueNode* pNode = pQueue->pFront;
	int iVertex = pNode->iVertex;
	
	pQueue->pFront = pNode->pNext;
	if ( pQueue->pFront == NULL ) {
		pQueue->pRear = NULL;
	}
	
	xrtFree(pNode);
	return iVertex;
}

// Check if queue is empty
// 检查队列是否为空
int QueueIsEmpty(Queue* pQueue)
{
	return pQueue->pFront == NULL;
}

// Breadth-First Search
// 广度优先搜索
void BFS(Graph* pGraph, int iStartVertex)
{
	int* piVisited = xrtMalloc(pGraph->iVertexCount * sizeof(int));
	for ( int i = 0; i < pGraph->iVertexCount; i++ ) {
		piVisited[i] = 0;
	}
	
	Queue* pQueue = QueueCreate();
	QueueEnqueue(pQueue, iStartVertex);
	piVisited[iStartVertex] = 1;
	
	printf("BFS traversal starting from vertex %d: ", iStartVertex);
	
	while ( !QueueIsEmpty(pQueue) ) {
		int iCurrent = QueueDequeue(pQueue);
		printf("%d ", iCurrent);
		
		// Visit all adjacent vertices
		// 访问所有相邻顶点
		Edge* pEdge = pGraph->ppAdjList[iCurrent];
		while ( pEdge ) {
			if ( !piVisited[pEdge->iDest] ) {
				piVisited[pEdge->iDest] = 1;
				QueueEnqueue(pQueue, pEdge->iDest);
			}
			pEdge = pEdge->pNext;
		}
	}
	
	printf("\n");
	
	xrtFree(piVisited);
	QueueDestroy(pQueue);
}

// Depth-First Search (recursive helper)
// 深度优先搜索（递归辅助函数）
void DFSUtil(Graph* pGraph, int iVertex, int* piVisited)
{
	piVisited[iVertex] = 1;
	printf("%d ", iVertex);
	
	Edge* pEdge = pGraph->ppAdjList[iVertex];
	while ( pEdge ) {
		if ( !piVisited[pEdge->iDest] ) {
			DFSUtil(pGraph, pEdge->iDest, piVisited);
		}
		pEdge = pEdge->pNext;
	}
}

// Depth-First Search
// 深度优先搜索
void DFS(Graph* pGraph, int iStartVertex)
{
	int* piVisited = xrtMalloc(pGraph->iVertexCount * sizeof(int));
	for ( int i = 0; i < pGraph->iVertexCount; i++ ) {
		piVisited[i] = 0;
	}
	
	printf("DFS traversal starting from vertex %d: ", iStartVertex);
	DFSUtil(pGraph, iStartVertex, piVisited);
	printf("\n");
	
	xrtFree(piVisited);
}

// Dijkstra's shortest path algorithm
// Dijkstra最短路径算法
void Dijkstra(Graph* pGraph, int iStartVertex)
{
	int* piDist = xrtMalloc(pGraph->iVertexCount * sizeof(int));
	int* piVisited = xrtMalloc(pGraph->iVertexCount * sizeof(int));
	
	// Initialize distances
	// 初始化距离
	for ( int i = 0; i < pGraph->iVertexCount; i++ ) {
		piDist[i] = INT_MAX;
		piVisited[i] = 0;
	}
	piDist[iStartVertex] = 0;
	
	// Find shortest paths
	// 寻找最短路径
	for ( int i = 0; i < pGraph->iVertexCount - 1; i++ ) {
		// Find minimum distance vertex
		// 找到最小距离顶点
		int iMinDist = INT_MAX;
		int iMinIndex = -1;
		
		for ( int v = 0; v < pGraph->iVertexCount; v++ ) {
			if ( !piVisited[v] && piDist[v] <= iMinDist ) {
				iMinDist = piDist[v];
				iMinIndex = v;
			}
		}
		
		if ( iMinIndex == -1 ) break;
		
		piVisited[iMinIndex] = 1;
		
		// Update distances of adjacent vertices
		// 更新相邻顶点的距离
		Edge* pEdge = pGraph->ppAdjList[iMinIndex];
		while ( pEdge ) {
			if ( !piVisited[pEdge->iDest] && 
			     piDist[iMinIndex] != INT_MAX &&
			     piDist[iMinIndex] + pEdge->iWeight < piDist[pEdge->iDest] ) {
				piDist[pEdge->iDest] = piDist[iMinIndex] + pEdge->iWeight;
			}
			pEdge = pEdge->pNext;
		}
	}
	
	// Print results
	// 打印结果
	printf("Shortest distances from vertex %d:\n", iStartVertex);
	for ( int i = 0; i < pGraph->iVertexCount; i++ ) {
		if ( piDist[i] == INT_MAX ) {
			printf("  Vertex %d: INF\n", i);
		} else {
			printf("  Vertex %d: %d\n", i, piDist[i]);
		}
	}
	
	xrtFree(piDist);
	xrtFree(piVisited);
}

// Prim's minimum spanning tree algorithm
// Prim最小生成树算法
void PrimMST(Graph* pGraph)
{
	if ( pGraph->bDirected ) {
		printf("Prim's algorithm works only on undirected graphs\n");
		return;
	}
	
	int* piKey = xrtMalloc(pGraph->iVertexCount * sizeof(int));
	int* piParent = xrtMalloc(pGraph->iVertexCount * sizeof(int));
	int* piVisited = xrtMalloc(pGraph->iVertexCount * sizeof(int));
	
	// Initialize
	// 初始化
	for ( int i = 0; i < pGraph->iVertexCount; i++ ) {
		piKey[i] = INT_MAX;
		piParent[i] = -1;
		piVisited[i] = 0;
	}
	
	piKey[0] = 0;  // Start from vertex 0 / 从顶点0开始
	
	// Build MST
	// 构建MST
	for ( int i = 0; i < pGraph->iVertexCount - 1; i++ ) {
		// Find minimum key vertex
		// 找到最小键值顶点
		int iMinKey = INT_MAX;
		int iMinIndex = -1;
		
		for ( int v = 0; v < pGraph->iVertexCount; v++ ) {
			if ( !piVisited[v] && piKey[v] < iMinKey ) {
				iMinKey = piKey[v];
				iMinIndex = v;
			}
		}
		
		if ( iMinIndex == -1 ) break;
		
		piVisited[iMinIndex] = 1;
		
		// Update key values of adjacent vertices
		// 更新相邻顶点的键值
		Edge* pEdge = pGraph->ppAdjList[iMinIndex];
		while ( pEdge ) {
			if ( !piVisited[pEdge->iDest] && pEdge->iWeight < piKey[pEdge->iDest] ) {
				piParent[pEdge->iDest] = iMinIndex;
				piKey[pEdge->iDest] = pEdge->iWeight;
			}
			pEdge = pEdge->pNext;
		}
	}
	
	// Print MST edges
	// 打印MST边
	printf("Minimum Spanning Tree edges:\n");
	int iTotalWeight = 0;
	for ( int i = 1; i < pGraph->iVertexCount; i++ ) {
		printf("  %d - %d (weight: %d)\n", piParent[i], i, piKey[i]);
		iTotalWeight += piKey[i];
	}
	printf("Total MST weight: %d\n", iTotalWeight);
	
	xrtFree(piKey);
	xrtFree(piParent);
	xrtFree(piVisited);
}

// Print graph structure
// 打印图结构
void GraphPrint(Graph* pGraph)
{
	printf("Graph structure:\n");
	for ( int i = 0; i < pGraph->iVertexCount; i++ ) {
		printf("Vertex %d: ", i);
		Edge* pEdge = pGraph->ppAdjList[i];
		while ( pEdge ) {
			printf("-> %d(weight:%d) ", pEdge->iDest, pEdge->iWeight);
			pEdge = pEdge->pNext;
		}
		printf("\n");
	}
}

// Destroy graph
// 销毁图
void GraphDestroy(Graph* pGraph)
{
	for ( int i = 0; i < pGraph->iVertexCount; i++ ) {
		Edge* pEdge = pGraph->ppAdjList[i];
		while ( pEdge ) {
			Edge* pNext = pEdge->pNext;
			xrtFree(pEdge);
			pEdge = pNext;
		}
	}
	xrtFree(pGraph->ppAdjList);
	xrtFree(pGraph);
}

// Test graph algorithms
// 测试图算法
void TestGraphAlgorithms()
{
	printf("=== Graph Algorithms Test ===\n");
	printf("=== 图算法测试 ===\n");
	
	// Create sample graph
	// 创建样本图
	//     0
	//    / \
	//   1   2
	//  / \   \
	// 3   4   5
	Graph* pGraph = GraphCreate(6, 0);  // Undirected graph / 无向图
	
	GraphAddEdge(pGraph, 0, 1, 4);
	GraphAddEdge(pGraph, 0, 2, 3);
	GraphAddEdge(pGraph, 1, 3, 2);
	GraphAddEdge(pGraph, 1, 4, 1);
	GraphAddEdge(pGraph, 2, 5, 5);
	
	GraphPrint(pGraph);
	
	// Test traversals
	// 测试遍历
	printf("\nTesting traversals:\n");
	BFS(pGraph, 0);
	DFS(pGraph, 0);
	
	// Test Dijkstra on weighted graph
	// 在加权图上测试Dijkstra
	printf("\nTesting Dijkstra's algorithm:\n");
	Dijkstra(pGraph, 0);
	
	// Test Prim's MST
	// 测试Prim的MST
	printf("\nTesting Prim's MST algorithm:\n");
	PrimMST(pGraph);
	
	// Test with directed graph
	// 用有向图测试
	printf("\n=== Directed Graph Test ===\n");
	Graph* pDirected = GraphCreate(5, 1);  // Directed graph / 有向图
	
	GraphAddEdge(pDirected, 0, 1, 10);
	GraphAddEdge(pDirected, 0, 2, 3);
	GraphAddEdge(pDirected, 1, 2, 1);
	GraphAddEdge(pDirected, 1, 3, 2);
	GraphAddEdge(pDirected, 2, 1, 4);
	GraphAddEdge(pDirected, 2, 3, 8);
	GraphAddEdge(pDirected, 2, 4, 2);
	GraphAddEdge(pDirected, 3, 4, 7);
	GraphAddEdge(pDirected, 4, 3, 9);
	
	GraphPrint(pDirected);
	
	printf("\nDirected graph traversals:\n");
	BFS(pDirected, 0);
	DFS(pDirected, 0);
	
	printf("\nDijkstra from vertex 0:\n");
	Dijkstra(pDirected, 0);
	
	// Prim's won't work on directed graph
	// Prim算法在有向图上不起作用
	PrimMST(pDirected);
	
	// Cleanup
	// 清理
	GraphDestroy(pGraph);
	GraphDestroy(pDirected);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Graph Algorithms Demo\n");
	printf("XRT 图算法演示\n");
	printf("======================\n\n");
	
	// Run tests
	// 运行测试
	TestGraphAlgorithms();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}