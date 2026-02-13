/*
 * XRT Example - LRU Cache
 * XRT 范例 - LRU缓存
 *
 * Description / 说明:
 *   EN: Demonstrates LRU (Least Recently Used) cache implementation using
 *       hash table and doubly linked list for O(1) get/put operations.
 *       Includes cache eviction policies, hit/miss statistics, and capacity management.
 *       Shows memory-efficient caching for frequently accessed data.
 *   CN: 演示使用哈希表和双向链表实现的LRU（最近最少使用）缓存，
 *       实现O(1)的获取/放置操作。包括缓存驱逐策略、命中/未命中统计
 *       和容量管理。展示频繁访问数据的内存高效缓存。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/advanced_cache_lru.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/advanced_cache_lru -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Hash table + doubly linked list combination
 *   - O(1) get and put operations
 *   - Automatic cache eviction
 *   - Hit/miss ratio tracking
 *   - Memory usage monitoring
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#include <time.h>

// Cache node structure
// 缓存节点结构
typedef struct CacheNode {
	str sKey;
	str sValue;
	struct CacheNode* pPrev;
	struct CacheNode* pNext;
} CacheNode;

// LRU Cache structure
// LRU缓存结构
typedef struct {
	CacheNode** ppHashTable;	// Hash table for O(1) lookup / 用于O(1)查找的哈希表
	CacheNode* pHead;			// Head of doubly linked list / 双向链表头
	CacheNode* pTail;			// Tail of doubly linked list / 双向链表尾
	size_t iCapacity;			// Maximum cache capacity / 最大缓存容量
	size_t iSize;				// Current cache size / 当前缓存大小
	size_t iHitCount;			// Cache hit count / 缓存命中次数
	size_t iMissCount;			// Cache miss count / 缓存未命中次数
	size_t iBucketCount;		// Hash table bucket count / 哈希表桶数量
} LRUCache;

// Create cache node
// 创建缓存节点
CacheNode* CacheNodeCreate(str sKey, str sValue)
{
	CacheNode* pNode = xrtMalloc(sizeof(CacheNode));
	pNode->sKey = xrtCopyStr(sKey, strlen(sKey) + 1);
	pNode->sValue = xrtCopyStr(sValue, strlen(sValue) + 1);
	pNode->pPrev = NULL;
	pNode->pNext = NULL;
	return pNode;
}

// Destroy cache node
// 销毁缓存节点
void CacheNodeDestroy(CacheNode* pNode)
{
	if ( pNode ) {
		if ( pNode->sKey ) xrtFree(pNode->sKey);
		if ( pNode->sValue ) xrtFree(pNode->sValue);
		xrtFree(pNode);
	}
}

// Simple hash function
// 简单哈希函数
size_t HashKey(str sKey, size_t iTableSize)
{
	size_t iHash = 5381;
	int iChar;
	
	while ( (iChar = *sKey++) ) {
		iHash = ((iHash << 5) + iHash) + iChar;
	}
	
	return iHash % iTableSize;
}

// Create LRU cache
// 创建LRU缓存
LRUCache* LRUCacheCreate(size_t iCapacity)
{
	LRUCache* pCache = xrtMalloc(sizeof(LRUCache));
	pCache->iCapacity = iCapacity;
	pCache->iSize = 0;
	pCache->iHitCount = 0;
	pCache->iMissCount = 0;
	pCache->pHead = NULL;
	pCache->pTail = NULL;
	
	// Initialize hash table
	// 初始化哈希表
	pCache->iBucketCount = iCapacity * 2;  // Load factor of 0.5 / 负载因子为0.5
	pCache->ppHashTable = xrtMalloc(pCache->iBucketCount * sizeof(CacheNode*));
	
	for ( size_t i = 0; i < pCache->iBucketCount; i++ ) {
		pCache->ppHashTable[i] = NULL;
	}
	
	return pCache;
}

// Destroy LRU cache
// 销毁LRU缓存
void LRUCacheDestroy(LRUCache* pCache)
{
	if ( pCache ) {
		// Free all cache nodes
		// 释放所有缓存节点
		CacheNode* pNode = pCache->pHead;
		while ( pNode ) {
			CacheNode* pNext = pNode->pNext;
			CacheNodeDestroy(pNode);
			pNode = pNext;
		}
		
		// Free hash table
		// 释放哈希表
		xrtFree(pCache->ppHashTable);
		xrtFree(pCache);
	}
}

// Move node to front (most recently used)
// 将节点移到前面（最近使用）
void LRUCacheMoveToFront(LRUCache* pCache, CacheNode* pNode)
{
	if ( pNode == pCache->pHead ) {
		return;  // Already at front / 已经在前面
	}
	
	// Remove from current position
	// 从当前位置移除
	if ( pNode->pPrev ) {
		pNode->pPrev->pNext = pNode->pNext;
	} else {
		pCache->pHead = pNode->pNext;
	}
	
	if ( pNode->pNext ) {
		pNode->pNext->pPrev = pNode->pPrev;
	} else {
		pCache->pTail = pNode->pPrev;
	}
	
	// Add to front
	// 添加到前面
	pNode->pNext = pCache->pHead;
	pNode->pPrev = NULL;
	
	if ( pCache->pHead ) {
		pCache->pHead->pPrev = pNode;
	}
	pCache->pHead = pNode;
	
	if ( !pCache->pTail ) {
		pCache->pTail = pNode;
	}
}

// Add new node to cache
// 向缓存添加新节点
void LRUCacheAddNode(LRUCache* pCache, CacheNode* pNode)
{
	// Add to front of linked list
	// 添加到链表前面
	pNode->pNext = pCache->pHead;
	pNode->pPrev = NULL;
	
	if ( pCache->pHead ) {
		pCache->pHead->pPrev = pNode;
	}
	pCache->pHead = pNode;
	
	if ( !pCache->pTail ) {
		pCache->pTail = pNode;
	}
	
	// Add to hash table
	// 添加到哈希表
	size_t iIndex = HashKey(pNode->sKey, pCache->iBucketCount);
	pNode->pNext = pCache->ppHashTable[iIndex];  // Use next pointer temporarily / 临时使用next指针
	pCache->ppHashTable[iIndex] = pNode;
	
	pCache->iSize++;
}

// Remove least recently used node
// 移除最近最少使用的节点
void LRUCacheEvictLRU(LRUCache* pCache)
{
	if ( !pCache->pTail ) {
		return;
	}
	
	CacheNode* pNodeToRemove = pCache->pTail;
	
	// Remove from linked list
	// 从链表移除
	if ( pNodeToRemove->pPrev ) {
		pNodeToRemove->pPrev->pNext = NULL;
	} else {
		pCache->pHead = NULL;
	}
	pCache->pTail = pNodeToRemove->pPrev;
	
	// Remove from hash table
	// 从哈希表移除
	size_t iIndex = HashKey(pNodeToRemove->sKey, pCache->iBucketCount);
	CacheNode* pCurrent = pCache->ppHashTable[iIndex];
	CacheNode* pPrev = NULL;
	
	while ( pCurrent ) {
		if ( pCurrent == pNodeToRemove ) {
			if ( pPrev ) {
				pPrev->pNext = pCurrent->pNext;
			} else {
				pCache->ppHashTable[iIndex] = pCurrent->pNext;
			}
			break;
		}
		pPrev = pCurrent;
		pCurrent = pCurrent->pNext;
	}
	
	// Free node
	// 释放节点
	CacheNodeDestroy(pNodeToRemove);
	pCache->iSize--;
}

// Get value from cache
// 从缓存获取值
str LRUCacheGet(LRUCache* pCache, str sKey)
{
	size_t iIndex = HashKey(sKey, pCache->iBucketCount);
	CacheNode* pNode = pCache->ppHashTable[iIndex];
	
	// Search for key in bucket
	// 在桶中搜索键
	while ( pNode ) {
		// Temporarily use pNode->pNext for hash table chaining
		// 临时使用pNode->pNext进行哈希表链接
		CacheNode* pActualNode = (CacheNode*)((char*)pNode - offsetof(CacheNode, pNext));
		
		if ( strcmp(pActualNode->sKey, sKey) == 0 ) {
			// Cache hit
			// 缓存命中
			pCache->iHitCount++;
			LRUCacheMoveToFront(pCache, pActualNode);
			return pActualNode->sValue;
		}
		
		// Move to next node in hash table chain
		// 移动到哈希表链中的下一个节点
		pNode = pNode->pNext;
	}
	
	// Cache miss
	// 缓存未命中
	pCache->iMissCount++;
	return NULL;
}

// Put key-value pair in cache
// 在缓存中放置键值对
void LRUCachePut(LRUCache* pCache, str sKey, str sValue)
{
	// Check if key already exists
	// 检查键是否已存在
	size_t iIndex = HashKey(sKey, pCache->iBucketCount);
	CacheNode* pNode = pCache->ppHashTable[iIndex];
	
	while ( pNode ) {
		CacheNode* pActualNode = (CacheNode*)((char*)pNode - offsetof(CacheNode, pNext));
		
		if ( strcmp(pActualNode->sKey, sKey) == 0 ) {
			// Update existing value
			// 更新现有值
			if ( pActualNode->sValue ) {
				xrtFree(pActualNode->sValue);
			}
			pActualNode->sValue = xrtCopyStr(sValue, strlen(sValue) + 1);
			
			// Move to front as most recently used
			// 移到前面作为最近使用
			LRUCacheMoveToFront(pCache, pActualNode);
			return;
		}
		
		pNode = pNode->pNext;
	}
	
	// Key doesn't exist, create new node
	// 键不存在，创建新节点
	CacheNode* pNewNode = CacheNodeCreate(sKey, sValue);
	
	// Evict LRU if cache is full
	// 如果缓存已满则驱逐LRU
	if ( pCache->iSize >= pCache->iCapacity ) {
		LRUCacheEvictLRU(pCache);
	}
	
	// Add new node
	// 添加新节点
	LRUCacheAddNode(pCache, pNewNode);
}

// Print cache statistics
// 打印缓存统计信息
void LRUCachePrintStats(LRUCache* pCache)
{
	printf("LRU Cache Statistics:\n");
	printf("  Capacity: %zu\n", pCache->iCapacity);
	printf("  Current size: %zu\n", pCache->iSize);
	printf("  Hits: %zu\n", pCache->iHitCount);
	printf("  Misses: %zu\n", pCache->iMissCount);
	
	if ( pCache->iHitCount + pCache->iMissCount > 0 ) {
		double dHitRatio = (double)pCache->iHitCount / (pCache->iHitCount + pCache->iMissCount) * 100;
		printf("  Hit ratio: %.2f%%\n", dHitRatio);
	}
	
	// Print current cache contents (most recent first)
	// 打印当前缓存内容（最近的在前）
	printf("  Current cache (MRU to LRU): ");
	CacheNode* pNode = pCache->pHead;
	int iCount = 0;
	while ( pNode && iCount < 10 ) {  // Limit output / 限制输出
		printf("%s", pNode->sKey);
		if ( pNode->pNext && iCount < 9 ) {
			printf(" -> ");
		}
		pNode = pNode->pNext;
		iCount++;
	}
	if ( pCache->iSize > 10 ) {
		printf(" ... (%zu more)", pCache->iSize - 10);
	}
	printf("\n");
}

// Test LRU cache operations
// 测试LRU缓存操作
void TestLRUCache()
{
	printf("=== LRU Cache Test ===\n");
	printf("=== LRU缓存测试 ===\n");
	
	// Create cache with capacity 3
	// 创建容量为3的缓存
	LRUCache* pCache = LRUCacheCreate(3);
	
	// Test basic operations
	// 测试基本操作
	printf("Testing basic operations:\n");
	
	LRUCachePut(pCache, "A", "Apple");
	LRUCachePut(pCache, "B", "Banana");
	LRUCachePut(pCache, "C", "Cherry");
	
	LRUCachePrintStats(pCache);
	
	// Test cache hit
	// 测试缓存命中
	printf("\nTesting cache hit:\n");
	str sValue = LRUCacheGet(pCache, "B");
	printf("Getting 'B': %s\n", sValue ? sValue : "NULL");
	LRUCachePrintStats(pCache);
	
	// Test cache miss
	// 测试缓存未命中
	printf("\nTesting cache miss:\n");
	sValue = LRUCacheGet(pCache, "D");
	printf("Getting 'D': %s\n", sValue ? sValue : "NULL");
	LRUCachePrintStats(pCache);
	
	// Test LRU eviction
	// 测试LRU驱逐
	printf("\nTesting LRU eviction:\n");
	LRUCachePut(pCache, "D", "Date");  // Should evict "A"
	printf("Added 'D', evicted LRU item\n");
	LRUCachePrintStats(pCache);
	
	sValue = LRUCacheGet(pCache, "A");
	printf("Getting 'A' after eviction: %s\n", sValue ? sValue : "NULL");
	
	// Test update existing key
	// 测试更新现有键
	printf("\nTesting update existing key:\n");
	LRUCachePut(pCache, "B", "Blueberry");  // Update existing
	printf("Updated 'B' to 'Blueberry'\n");
	LRUCachePrintStats(pCache);
	
	sValue = LRUCacheGet(pCache, "B");
	printf("Getting 'B' after update: %s\n", sValue ? sValue : "NULL");
	
	// Cleanup
	// 清理
	LRUCacheDestroy(pCache);
}

// Test cache performance
// 测试缓存性能
void TestCachePerformance()
{
	printf("\n=== Cache Performance Test ===\n");
	printf("=== 缓存性能测试 ===\n");
	
	LRUCache* pCache = LRUCacheCreate(1000);
	
	// Populate cache with initial data
	// 用初始数据填充缓存
	for ( int i = 0; i < 500; i++ ) {
		char sKey[32], sValue[32];
		sprintf(sKey, "key_%d", i);
		sprintf(sValue, "value_%d", i);
		LRUCachePut(pCache, sKey, sValue);
	}
	
	// Test mixed access pattern
	// 测试混合访问模式
	clock_t clkStart = clock();
	
	// 70% hits, 30% misses
	// 70%命中，30%未命中
	for ( int i = 0; i < 10000; i++ ) {
		int iKeyNum = rand() % 700;  // 0-699, mostly cached / 0-699，大多已缓存
		char sKey[32];
		sprintf(sKey, "key_%d", iKeyNum);
		LRUCacheGet(pCache, sKey);
	}
	
	clock_t clkEnd = clock();
	double dTime = ((double)(clkEnd - clkStart)) / CLOCKS_PER_SEC;
	
	LRUCachePrintStats(pCache);
	printf("10000 operations took: %.6f seconds\n", dTime);
	
	LRUCacheDestroy(pCache);
}

// Test cache behavior patterns
// 测试缓存行为模式
void TestCacheBehavior()
{
	printf("\n=== Cache Behavior Test ===\n");
	printf("=== 缓存行为测试 ===\n");
	
	LRUCache* pCache = LRUCacheCreate(5);
	
	// Sequential access pattern
	// 顺序访问模式
	printf("Sequential access pattern:\n");
	for ( int i = 0; i < 7; i++ ) {
		char sKey[16], sValue[16];
		sprintf(sKey, "item%d", i);
		sprintf(sValue, "value%d", i);
		LRUCachePut(pCache, sKey, sValue);
	}
	LRUCachePrintStats(pCache);
	
	// Recently used items should be at front
	// 最近使用的项目应在前面
	printf("Accessing recent items:\n");
	LRUCacheGet(pCache, "item6");
	LRUCacheGet(pCache, "item5");
	LRUCachePrintStats(pCache);
	
	// Test circular access pattern
	// 测试循环访问模式
	printf("\nCircular access pattern:\n");
	for ( int i = 0; i < 10; i++ ) {
		int iItem = i % 5;
		char sKey[16];
		sprintf(sKey, "item%d", iItem);
		LRUCacheGet(pCache, sKey);
	}
	LRUCachePrintStats(pCache);
	
	LRUCacheDestroy(pCache);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT LRU Cache Demo\n");
	printf("XRT LRU缓存演示\n");
	printf("================\n\n");
	
	// Seed random number generator
	// 初始化随机数生成器
	srand((unsigned int)time(NULL));
	
	// Run all tests
	// 运行所有测试
	TestLRUCache();
	TestCachePerformance();
	TestCacheBehavior();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}