/*
 * XRT Example - Memory Pool
 * XRT 范例 - 内存池
 *
 * Description / 说明:
 *   EN: Demonstrates XRT memory pool management including pool creation,
 *       allocation/deallocation, performance comparison with standard malloc/free,
 *       and memory fragmentation handling. Shows efficient memory management
 *       for high-frequency allocation scenarios.
 *   CN: 演示XRT内存池管理，包括池创建、分配/释放、与标准malloc/free的性能对比，
 *       以及内存碎片处理。展示高频分配场景下的高效内存管理。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/storage_mempool.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/storage_mempool -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Memory pool creation and configuration
 *   - Efficient bulk allocation/deallocation
 *   - Performance benchmarking
 *   - Memory fragmentation analysis
 *   - Custom memory management
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#include <time.h>

// Test structure for memory pool
// 用于内存池测试的结构体
typedef struct {
	int iId;
	double dValue;
	str sName;
} TestStruct;

// Performance measurement
// 性能测量
typedef struct {
	clock_t clkStart;
	clock_t clkEnd;
	double dElapsedTime;
	size_t iOperations;
} PerformanceMetrics;

// Start performance measurement
// 开始性能测量
void PerformanceStart(PerformanceMetrics* pMetrics)
{
	pMetrics->clkStart = clock();
	pMetrics->iOperations = 0;
}

// End performance measurement
// 结束性能测量
void PerformanceEnd(PerformanceMetrics* pMetrics)
{
	pMetrics->clkEnd = clock();
	pMetrics->dElapsedTime = ((double)(pMetrics->clkEnd - pMetrics->clkStart)) / CLOCKS_PER_SEC;
}

// Print performance results
// 打印性能结果
void PerformancePrint(PerformanceMetrics* pMetrics, str sOperation)
{
	printf("%s Performance:\n", sOperation);
	printf("  Time: %.6f seconds\n", pMetrics->dElapsedTime);
	printf("  Operations: %zu\n", pMetrics->iOperations);
	if ( pMetrics->dElapsedTime > 0 ) {
		double dOpsPerSec = (double)pMetrics->iOperations / pMetrics->dElapsedTime;
		printf("  Rate: %.0f operations/second\n", dOpsPerSec);
	}
	printf("\n");
}

// Test standard malloc/free performance
// 测试标准malloc/free性能
void TestStandardMalloc(PerformanceMetrics* pMetrics, size_t iIterations)
{
	PerformanceStart(pMetrics);
	
	TestStruct** ppObjects = xrtMalloc(iIterations * sizeof(TestStruct*));
	
	// Allocation test
	// 分配测试
	for ( size_t i = 0; i < iIterations; i++ ) {
		ppObjects[i] = xrtMalloc(sizeof(TestStruct));
		ppObjects[i]->iId = (int)i;
		ppObjects[i]->dValue = i * 1.5;
		ppObjects[i]->sName = xrtCopyStr("TestObject", 10);
		pMetrics->iOperations++;
	}
	
	// Deallocation test
	// 释放测试
	for ( size_t i = 0; i < iIterations; i++ ) {
		if ( ppObjects[i]->sName ) {
			xrtFree(ppObjects[i]->sName);
		}
		xrtFree(ppObjects[i]);
		pMetrics->iOperations++;
	}
	
	xrtFree(ppObjects);
	PerformanceEnd(pMetrics);
}

// Test memory pool performance
// 测试内存池性能
void TestMemoryPool(PerformanceMetrics* pMetrics, size_t iIterations)
{
	PerformanceStart(pMetrics);
	
	// Create memory pool
	// 创建内存池
	xmempool objPool = xrtMemPoolCreate(0, XRT_OBJMODE_LOCAL);  // 0 for default configuration / 0表示默认配置
	if ( !objPool ) {
		printf("Failed to create memory pool\n");
		return;
	}
	
	TestStruct** ppObjects = xrtMalloc(iIterations * sizeof(TestStruct*));
	
	// Allocation test using memory pool
	// 使用内存池进行分配测试
	for ( size_t i = 0; i < iIterations; i++ ) {
		ppObjects[i] = (TestStruct*)xrtMemPoolAlloc(objPool, sizeof(TestStruct));
		if ( ppObjects[i] ) {
			ppObjects[i]->iId = (int)i;
			ppObjects[i]->dValue = i * 1.5;
			ppObjects[i]->sName = xrtCopyStr("PoolObject", 10);
			pMetrics->iOperations++;
		}
	}
	
	// Deallocation test
	// 释放测试
	for ( size_t i = 0; i < iIterations; i++ ) {
		if ( ppObjects[i] ) {
			if ( ppObjects[i]->sName ) {
				xrtFree(ppObjects[i]->sName);
			}
			xrtMemPoolFree(objPool, ppObjects[i]);
			pMetrics->iOperations++;
		}
	}
	
	xrtFree(ppObjects);
	xrtMemPoolDestroy(objPool);
	PerformanceEnd(pMetrics);
}

// Test memory pool with different sizes
// 测试不同大小的内存池
void TestDifferentSizes()
{
	printf("=== Different Size Allocation Test ===\n");
	printf("=== 不同大小分配测试 ===\n");
	
	xmempool objPool = xrtMemPoolCreate(0, XRT_OBJMODE_LOCAL);
	if ( !objPool ) {
		printf("Failed to create memory pool\n");
		return;
	}
	
	// Test various allocation sizes
	// 测试各种分配大小
	size_t arrSizes[] = {16, 32, 64, 128, 256, 512};
	size_t iSizeCount = sizeof(arrSizes) / sizeof(arrSizes[0]);
	
	for ( size_t i = 0; i < iSizeCount; i++ ) {
		printf("Testing allocation size: %zu bytes\n", arrSizes[i]);
		
		void* pPtr = xrtMemPoolAlloc(objPool, arrSizes[i]);
		if ( pPtr ) {
			printf("  Successfully allocated %zu bytes\n", arrSizes[i]);
			xrtMemPoolFree(objPool, pPtr);
		} else {
			printf("  Failed to allocate %zu bytes\n", arrSizes[i]);
		}
	}
	
	xrtMemPoolDestroy(objPool);
}

// Test memory pool fragmentation
// 测试内存池碎片
void TestFragmentation()
{
	printf("\n=== Fragmentation Test ===\n");
	printf("=== 碎片测试 ===\n");
	
	xmempool objPool = xrtMemPoolCreate(0, XRT_OBJMODE_LOCAL);
	if ( !objPool ) {
		printf("Failed to create memory pool\n");
		return;
	}
	
	// Allocate many small objects
	// 分配许多小对象
	void** ppSmallObjects = xrtMalloc(1000 * sizeof(void*));
	size_t iSmallCount = 0;
	
	for ( size_t i = 0; i < 1000; i++ ) {
		ppSmallObjects[i] = xrtMemPoolAlloc(objPool, 32);
		if ( ppSmallObjects[i] ) {
			iSmallCount++;
		}
	}
	
	printf("Allocated %zu small objects (32 bytes each)\n", iSmallCount);
	
	// Free every other object to create fragmentation
	// 释放每隔一个对象以创建碎片
	for ( size_t i = 0; i < iSmallCount; i += 2 ) {
		xrtMemPoolFree(objPool, ppSmallObjects[i]);
		ppSmallObjects[i] = NULL;
	}
	
	printf("Freed every other object, creating fragmentation\n");
	
	// Try to allocate a large object
	// 尝试分配一个大对象
	void* pLargeObject = xrtMemPoolAlloc(objPool, 1024);
	if ( pLargeObject ) {
		printf("Successfully allocated large object (1024 bytes) despite fragmentation\n");
		xrtMemPoolFree(objPool, pLargeObject);
	} else {
		printf("Failed to allocate large object due to fragmentation\n");
	}
	
	// Cleanup
	// 清理
	for ( size_t i = 1; i < iSmallCount; i += 2 ) {
		if ( ppSmallObjects[i] ) {
			xrtMemPoolFree(objPool, ppSmallObjects[i]);
		}
	}
	
	xrtFree(ppSmallObjects);
	xrtMemPoolDestroy(objPool);
}

// Test bulk allocation
// 测试批量分配
void TestBulkAllocation()
{
	printf("\n=== Bulk Allocation Test ===\n");
	printf("=== 批量分配测试 ===\n");
	
	xmempool objPool = xrtMemPoolCreate(0, XRT_OBJMODE_LOCAL);
	if ( !objPool ) {
		printf("Failed to create memory pool\n");
		return;
	}
	
	const size_t iBatchSize = 100;
	TestStruct* pBatch = xrtMalloc(iBatchSize * sizeof(TestStruct));
	
	clock_t clkStart = clock();
	
	// Bulk allocation
	// 批量分配
	for ( size_t i = 0; i < iBatchSize; i++ ) {
		pBatch[i].iId = (int)i;
		pBatch[i].dValue = i * 2.0;
		pBatch[i].sName = xrtCopyStr("BulkObject", 10);
	}
	
	clock_t clkEnd = clock();
	double dTime = ((double)(clkEnd - clkStart)) / CLOCKS_PER_SEC;
	
	printf("Bulk allocated %zu objects in %.6f seconds\n", iBatchSize, dTime);
	
	// Cleanup
	// 清理
	for ( size_t i = 0; i < iBatchSize; i++ ) {
		if ( pBatch[i].sName ) {
			xrtFree(pBatch[i].sName);
		}
	}
	xrtFree(pBatch);
	xrtMemPoolDestroy(objPool);
}

// Main performance comparison test
// 主要性能对比测试
void TestPerformanceComparison()
{
	printf("=== Memory Pool Performance Comparison ===\n");
	printf("=== 内存池性能对比 ===\n");
	
	const size_t iIterations = 10000;
	PerformanceMetrics metricsStd, metricsPool;
	
	printf("Testing %zu iterations...\n\n", iIterations);
	
	// Test standard malloc/free
	// 测试标准malloc/free
	printf("1. Standard malloc/free:\n");
	TestStandardMalloc(&metricsStd, iIterations);
	PerformancePrint(&metricsStd, "Standard malloc/free");
	
	// Test memory pool
	// 测试内存池
	printf("2. Memory pool:\n");
	TestMemoryPool(&metricsPool, iIterations);
	PerformancePrint(&metricsPool, "Memory pool");
	
	// Compare results
	// 对比结果
	if ( metricsStd.dElapsedTime > 0 && metricsPool.dElapsedTime > 0 ) {
		double dSpeedup = metricsStd.dElapsedTime / metricsPool.dElapsedTime;
		printf("Memory pool speedup: %.2fx faster\n", dSpeedup);
	}
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Memory Pool Demo\n");
	printf("XRT 内存池演示\n");
	printf("==================\n\n");
	
	// Run all tests
	// 运行所有测试
	TestPerformanceComparison();
	TestDifferentSizes();
	TestFragmentation();
	TestBulkAllocation();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}