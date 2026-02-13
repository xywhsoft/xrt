/*
 * XRT Example - Hash Table
 * XRT 范例 - 哈希表
 *
 * Description / 说明:
 *   EN: Demonstrates hash table implementation with collision resolution,
 *       dynamic resizing, various hash functions, and performance analysis.
 *       Includes insertion, deletion, search operations and load factor management.
 *   CN: 演示带有冲突解决、动态调整大小、各种哈希函数和性能分析的哈希表实现。
 *       包括插入、删除、搜索操作和负载因子管理。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/data_structure_hash_table.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/data_structure_hash_table -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Chaining collision resolution
 *   - Dynamic resizing based on load factor
 *   - Multiple hash functions
 *   - Performance benchmarking
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#include <time.h>

// Hash table entry structure
// 哈希表条目结构
typedef struct HashEntry {
	str sKey;
	int iValue;
	struct HashEntry* pNext;  // For chaining / 用于链接
} HashEntry;

// Hash table structure
// 哈希表结构
typedef struct {
	HashEntry** ppBuckets;    // Array of bucket pointers / 桶指针数组
	size_t iBucketCount;      // Number of buckets / 桶数量
	size_t iEntryCount;       // Number of entries / 条目数量
	float fLoadFactor;        // Current load factor / 当前负载因子
} HashTable;

// Hash function types
// 哈希函数类型
typedef enum {
	HASH_SIMPLE = 1,
	HASH_DJB2,
	HASH_SDBM,
	HASH_ROTATING
} HashFunction;

// Create new hash entry
// 创建新哈希条目
HashEntry* HashEntryCreate(str sKey, int iValue)
{
	HashEntry* pEntry = xrtMalloc(sizeof(HashEntry));
	pEntry->sKey = xrtCopyStr(sKey, strlen(sKey) + 1);
	pEntry->iValue = iValue;
	pEntry->pNext = NULL;
	return pEntry;
}

// Destroy hash entry
// 销毁哈希条目
void HashEntryDestroy(HashEntry* pEntry)
{
	if ( pEntry ) {
		if ( pEntry->sKey ) {
			xrtFree(pEntry->sKey);
		}
		xrtFree(pEntry);
	}
}

// Create hash table
// 创建哈希表
HashTable* HashTableCreate(size_t iInitialSize)
{
	HashTable* pTable = xrtMalloc(sizeof(HashTable));
	pTable->iBucketCount = iInitialSize;
	pTable->iEntryCount = 0;
	pTable->fLoadFactor = 0.0f;
	pTable->ppBuckets = xrtMalloc(iInitialSize * sizeof(HashEntry*));
	
	// Initialize all buckets to NULL
	// 初始化所有桶为NULL
	for ( size_t i = 0; i < iInitialSize; i++ ) {
		pTable->ppBuckets[i] = NULL;
	}
	
	return pTable;
}

// Destroy hash table
// 销毁哈希表
void HashTableDestroy(HashTable* pTable)
{
	if ( pTable ) {
		// Free all entries
		// 释放所有条目
		for ( size_t i = 0; i < pTable->iBucketCount; i++ ) {
			HashEntry* pEntry = pTable->ppBuckets[i];
			while ( pEntry ) {
				HashEntry* pNext = pEntry->pNext;
				HashEntryDestroy(pEntry);
				pEntry = pNext;
			}
		}
		
		// Free bucket array
		// 释放桶数组
		xrtFree(pTable->ppBuckets);
		xrtFree(pTable);
	}
}

// Simple hash function
// 简单哈希函数
size_t HashSimple(str sKey, size_t iTableSize)
{
	size_t iHash = 0;
	while ( *sKey ) {
		iHash += *sKey;
		sKey++;
	}
	return iHash % iTableSize;
}

// DJB2 hash function
// DJB2哈希函数
size_t HashDJB2(str sKey, size_t iTableSize)
{
	size_t iHash = 5381;
	int iChar;
	
	while ( (iChar = *sKey++) ) {
		iHash = ((iHash << 5) + iHash) + iChar;  // hash * 33 + c
	}
	
	return iHash % iTableSize;
}

// SDBM hash function
// SDBM哈希函数
size_t HashSDBM(str sKey, size_t iTableSize)
{
	size_t iHash = 0;
	int iChar;
	
	while ( (iChar = *sKey++) ) {
		iHash = iChar + (iHash << 6) + (iHash << 16) - iHash;
	}
	
	return iHash % iTableSize;
}

// Rotating hash function
// 旋转哈希函数
size_t HashRotating(str sKey, size_t iTableSize)
{
	size_t iHash = 0;
	int iChar;
	
	while ( (iChar = *sKey++) ) {
		iHash = (iHash << 4) ^ (iHash >> 28) ^ iChar;
	}
	
	return iHash % iTableSize;
}

// Get hash value using specified function
// 使用指定函数获取哈希值
size_t GetHash(str sKey, size_t iTableSize, HashFunction eFunc)
{
	switch ( eFunc ) {
		case HASH_SIMPLE:
			return HashSimple(sKey, iTableSize);
		case HASH_DJB2:
			return HashDJB2(sKey, iTableSize);
		case HASH_SDBM:
			return HashSDBM(sKey, iTableSize);
		case HASH_ROTATING:
			return HashRotating(sKey, iTableSize);
		default:
			return HashDJB2(sKey, iTableSize);
	}
}

// Calculate load factor
// 计算负载因子
float HashTableLoadFactor(HashTable* pTable)
{
	return (float)pTable->iEntryCount / pTable->iBucketCount;
}

// Resize hash table
// 调整哈希表大小
void HashTableResize(HashTable* pTable, size_t iNewSize)
{
	// Create new bucket array
	// 创建新桶数组
	HashEntry** ppNewBuckets = xrtMalloc(iNewSize * sizeof(HashEntry*));
	for ( size_t i = 0; i < iNewSize; i++ ) {
		ppNewBuckets[i] = NULL;
	}
	
	// Rehash all entries
	// 重新哈希所有条目
	for ( size_t i = 0; i < pTable->iBucketCount; i++ ) {
		HashEntry* pEntry = pTable->ppBuckets[i];
		while ( pEntry ) {
			HashEntry* pNext = pEntry->pNext;
			
			// Calculate new bucket index
			// 计算新桶索引
			size_t iNewIndex = HashDJB2(pEntry->sKey, iNewSize);
			
			// Add to new bucket
			// 添加到新桶
			pEntry->pNext = ppNewBuckets[iNewIndex];
			ppNewBuckets[iNewIndex] = pEntry;
			
			pEntry = pNext;
		}
	}
	
	// Replace old bucket array
	// 替换旧桶数组
	xrtFree(pTable->ppBuckets);
	pTable->ppBuckets = ppNewBuckets;
	pTable->iBucketCount = iNewSize;
	pTable->fLoadFactor = HashTableLoadFactor(pTable);
}

// Insert key-value pair
// 插入键值对
void HashTablePut(HashTable* pTable, str sKey, int iValue)
{
	// Check if resize is needed
	// 检查是否需要调整大小
	if ( HashTableLoadFactor(pTable) > 0.75f ) {
		HashTableResize(pTable, pTable->iBucketCount * 2);
	}
	
	// Get bucket index
	// 获取桶索引
	size_t iIndex = HashDJB2(sKey, pTable->iBucketCount);
	
	// Check if key already exists
	// 检查键是否已存在
	HashEntry* pEntry = pTable->ppBuckets[iIndex];
	while ( pEntry ) {
		if ( strcmp(pEntry->sKey, sKey) == 0 ) {
			// Update existing value
			// 更新现有值
			pEntry->iValue = iValue;
			return;
		}
		pEntry = pEntry->pNext;
	}
	
	// Create new entry
	// 创建新条目
	HashEntry* pNewEntry = HashEntryCreate(sKey, iValue);
	pNewEntry->pNext = pTable->ppBuckets[iIndex];
	pTable->ppBuckets[iIndex] = pNewEntry;
	pTable->iEntryCount++;
	pTable->fLoadFactor = HashTableLoadFactor(pTable);
}

// Get value by key
// 通过键获取值
int HashTableGet(HashTable* pTable, str sKey, int* piFound)
{
	size_t iIndex = HashDJB2(sKey, pTable->iBucketCount);
	
	HashEntry* pEntry = pTable->ppBuckets[iIndex];
	while ( pEntry ) {
		if ( strcmp(pEntry->sKey, sKey) == 0 ) {
			*piFound = 1;
			return pEntry->iValue;
		}
		pEntry = pEntry->pNext;
	}
	
	*piFound = 0;
	return 0;
}

// Remove key-value pair
// 移除键值对
int HashTableRemove(HashTable* pTable, str sKey)
{
	size_t iIndex = HashDJB2(sKey, pTable->iBucketCount);
	
	HashEntry* pPrev = NULL;
	HashEntry* pEntry = pTable->ppBuckets[iIndex];
	
	while ( pEntry ) {
		if ( strcmp(pEntry->sKey, sKey) == 0 ) {
			// Found entry to remove
			// 找到要移除的条目
			if ( pPrev ) {
				pPrev->pNext = pEntry->pNext;
			} else {
				pTable->ppBuckets[iIndex] = pEntry->pNext;
			}
			
			int iValue = pEntry->iValue;
			HashEntryDestroy(pEntry);
			pTable->iEntryCount--;
			pTable->fLoadFactor = HashTableLoadFactor(pTable);
			
			return iValue;
		}
		
		pPrev = pEntry;
		pEntry = pEntry->pNext;
	}
	
	return 0;  // Key not found / 未找到键
}

// Print hash table statistics
// 打印哈希表统计信息
void HashTablePrintStats(HashTable* pTable)
{
	printf("Hash Table Statistics:\n");
	printf("  Bucket count: %zu\n", pTable->iBucketCount);
	printf("  Entry count: %zu\n", pTable->iEntryCount);
	printf("  Load factor: %.2f\n", pTable->fLoadFactor);
	
	// Count collisions
	// 计算冲突
	size_t iCollisions = 0;
	size_t iMaxChainLength = 0;
	
	for ( size_t i = 0; i < pTable->iBucketCount; i++ ) {
		HashEntry* pEntry = pTable->ppBuckets[i];
		if ( pEntry ) {
			size_t iChainLength = 0;
			while ( pEntry ) {
				iChainLength++;
				pEntry = pEntry->pNext;
			}
			
			if ( iChainLength > 1 ) {
				iCollisions += iChainLength - 1;
			}
			
			if ( iChainLength > iMaxChainLength ) {
				iMaxChainLength = iChainLength;
			}
		}
	}
	
	printf("  Total collisions: %zu\n", iCollisions);
	printf("  Max chain length: %zu\n", iMaxChainLength);
}

// Test hash table operations
// 测试哈希表操作
void TestHashTable()
{
	printf("=== Hash Table Test ===\n");
	printf("=== 哈希表测试 ===\n");
	
	// Create hash table
	// 创建哈希表
	HashTable* pTable = HashTableCreate(16);
	
	// Test insertions
	// 测试插入
	printf("Testing insertions:\n");
	HashTablePut(pTable, "apple", 100);
	HashTablePut(pTable, "banana", 200);
	HashTablePut(pTable, "cherry", 300);
	HashTablePut(pTable, "date", 400);
	HashTablePut(pTable, "elderberry", 500);
	
	HashTablePrintStats(pTable);
	
	// Test searches
	// 测试搜索
	printf("\nTesting searches:\n");
	int iFound;
	int iValue;
	
	iValue = HashTableGet(pTable, "apple", &iFound);
	printf("apple: %s (value: %d)\n", iFound ? "found" : "not found", iValue);
	
	iValue = HashTableGet(pTable, "cherry", &iFound);
	printf("cherry: %s (value: %d)\n", iFound ? "found" : "not found", iValue);
	
	iValue = HashTableGet(pTable, "grape", &iFound);
	printf("grape: %s (value: %d)\n", iFound ? "found" : "not found", iValue);
	
	// Test updates
	// 测试更新
	printf("\nTesting updates:\n");
	HashTablePut(pTable, "apple", 999);
	iValue = HashTableGet(pTable, "apple", &iFound);
	printf("apple after update: %d\n", iValue);
	
	// Test deletions
	// 测试删除
	printf("\nTesting deletions:\n");
	int iRemoved = HashTableRemove(pTable, "banana");
	printf("Removed banana: %d\n", iRemoved);
	
	iValue = HashTableGet(pTable, "banana", &iFound);
	printf("banana after removal: %s\n", iFound ? "found" : "not found");
	
	HashTablePrintStats(pTable);
	
	// Test dynamic resizing
	// 测试动态调整大小
	printf("\nTesting dynamic resizing:\n");
	for ( int i = 0; i < 50; i++ ) {
		char sKey[32];
		sprintf(sKey, "key_%d", i);
		HashTablePut(pTable, sKey, i * 10);
	}
	
	HashTablePrintStats(pTable);
	
	// Cleanup
	// 清理
	HashTableDestroy(pTable);
}

// Test hash functions
// 测试哈希函数
void TestHashFunctions()
{
	printf("\n=== Hash Functions Test ===\n");
	printf("=== 哈希函数测试 ===\n");
	
	str arrKeys[] = {"hello", "world", "test", "hash", "function", "collision"};
	size_t iKeyCount = sizeof(arrKeys) / sizeof(arrKeys[0]);
	size_t iTableSize = 16;
	
	printf("Hash values for table size %zu:\n", iTableSize);
	printf("%-12s %-8s %-6s %-6s %-10s\n", "Key", "Simple", "DJB2", "SDBM", "Rotating");
	printf("----------------------------------------\n");
	
	for ( size_t i = 0; i < iKeyCount; i++ ) {
		size_t iSimple = HashSimple(arrKeys[i], iTableSize);
		size_t iDJB2 = HashDJB2(arrKeys[i], iTableSize);
		size_t iSDBM = HashSDBM(arrKeys[i], iTableSize);
		size_t iRotating = HashRotating(arrKeys[i], iTableSize);
		
		printf("%-12s %-8zu %-6zu %-6zu %-10zu\n", 
		       arrKeys[i], iSimple, iDJB2, iSDBM, iRotating);
	}
}

// Test performance
// 测试性能
void TestPerformance()
{
	printf("\n=== Performance Test ===\n");
	printf("=== 性能测试 ===\n");
	
	HashTable* pTable = HashTableCreate(1000);
	
	// Insert performance test
	// 插入性能测试
	clock_t clkStart = clock();
	
	for ( int i = 0; i < 10000; i++ ) {
		char sKey[32];
		sprintf(sKey, "perf_key_%d", i);
		HashTablePut(pTable, sKey, i);
	}
	
	clock_t clkEnd = clock();
	double dInsertTime = ((double)(clkEnd - clkStart)) / CLOCKS_PER_SEC;
	
	printf("Insert 10000 entries: %.6f seconds\n", dInsertTime);
	
	// Search performance test
	// 搜索性能测试
	clkStart = clock();
	
	int iFoundCount = 0;
	for ( int i = 0; i < 10000; i++ ) {
		char sKey[32];
		sprintf(sKey, "perf_key_%d", i);
		int iFound;
		HashTableGet(pTable, sKey, &iFound);
		if ( iFound ) iFoundCount++;
	}
	
	clkEnd = clock();
	double dSearchTime = ((double)(clkEnd - clkStart)) / CLOCKS_PER_SEC;
	
	printf("Search 10000 entries: %.6f seconds (%d found)\n", dSearchTime, iFoundCount);
	
	// Cleanup
	// 清理
	HashTableDestroy(pTable);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Hash Table Demo\n");
	printf("XRT 哈希表演示\n");
	printf("==================\n\n");
	
	// Run all tests
	// 运行所有测试
	TestHashTable();
	TestHashFunctions();
	TestPerformance();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}