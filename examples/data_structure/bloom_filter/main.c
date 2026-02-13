/*
 * XRT Example - Bloom Filter
 * XRT 范例 - 布隆过滤器
 *
 * Description / 说明:
 *   EN: Demonstrates Bloom filter implementation for probabilistic set membership testing.
 *       Includes multiple hash functions, false positive rate calculation,
 *       optimal parameter selection, and performance comparison with traditional sets.
 *       Shows space-efficient approximate membership queries.
 *   CN: 演示布隆过滤器实现用于概率性集合成员资格测试。
 *       包括多个哈希函数、误报率计算、最优参数选择，
 *       以及与传统集合的性能比较。展示空间高效的大致成员资格查询。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/data_structure_bloom_filter.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/data_structure_bloom_filter -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Bit array implementation
 *   - Multiple hash functions
 *   - False positive rate analysis
 *   - Space efficiency comparison
 *   - Probabilistic data structure
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#include <time.h>
#include <math.h>

// Bloom filter structure
// 布隆过滤器结构
typedef struct {
	unsigned char* pucBitArray;	// Bit array / 位数组
	size_t iBitCount;			// Number of bits / 位数
	size_t iHashCount;			// Number of hash functions / 哈希函数数量
	size_t iElementCount;		// Number of inserted elements / 插入元素数量
} BloomFilter;

// Simple hash functions
// 简单哈希函数

// Hash function 1 - DJB2 variant
// 哈希函数1 - DJB2变体
size_t Hash1(str sKey, size_t iModulus)
{
	size_t iHash = 5381;
	int iChar;
	
	while ( (iChar = *sKey++) ) {
		iHash = ((iHash << 5) + iHash) + iChar;
	}
	
	return iHash % iModulus;
}

// Hash function 2 - SDBM variant
// 哈希函数2 - SDBM变体
size_t Hash2(str sKey, size_t iModulus)
{
	size_t iHash = 0;
	int iChar;
	
	while ( (iChar = *sKey++) ) {
		iHash = iChar + (iHash << 6) + (iHash << 16) - iHash;
	}
	
	return iHash % iModulus;
}

// Hash function 3 - Custom polynomial rolling hash
// 哈希函数3 - 自定义多项式滚动哈希
size_t Hash3(str sKey, size_t iModulus)
{
	size_t iHash = 0;
	size_t iPower = 1;
	const size_t iBase = 31;
	
	while ( *sKey ) {
		iHash = (iHash + (*sKey) * iPower) % iModulus;
		iPower = (iPower * iBase) % iModulus;
		sKey++;
	}
	
	return iHash;
}

// Hash function 4 - Simple additive hash
// 哈希函数4 - 简单加法哈希
size_t Hash4(str sKey, size_t iModulus)
{
	size_t iHash = 0;
	
	while ( *sKey ) {
		iHash += *sKey;
		iHash %= iModulus;
		sKey++;
	}
	
	return iHash;
}

// Create Bloom filter
// 创建布隆过滤器
BloomFilter* BloomCreate(size_t iExpectedElements, double dFalsePositiveRate)
{
	BloomFilter* pBF = xrtMalloc(sizeof(BloomFilter));
	
	// Calculate optimal bit array size
	// 计算最优位数组大小
	// m = -(n * ln(p)) / (ln(2)^2)
	// 其中 n=预期元素数, p=误报率
	double dLn2 = log(2.0);
	size_t iBits = (size_t)(-(double)iExpectedElements * log(dFalsePositiveRate) / (dLn2 * dLn2));
	
	// Calculate optimal hash function count
	// 计算最优哈希函数数量
	// k = (m/n) * ln(2)
	size_t iHashes = (size_t)((double)iBits / iExpectedElements * dLn2);
	
	// Ensure reasonable bounds
	// 确保合理边界
	if ( iHashes < 1 ) iHashes = 1;
	if ( iHashes > 20 ) iHashes = 20;  // Practical limit / 实用限制
	if ( iBits < 8 ) iBits = 8;
	
	pBF->iBitCount = iBits;
	pBF->iHashCount = iHashes;
	pBF->iElementCount = 0;
	
	// Allocate bit array (bytes)
	// 分配位数组（字节）
	size_t iByteCount = (iBits + 7) / 8;
	pBF->pucBitArray = xrtMalloc(iByteCount);
	memset(pBF->pucBitArray, 0, iByteCount);
	
	return pBF;
}

// Destroy Bloom filter
// 销毁布隆过滤器
void BloomDestroy(BloomFilter* pBF)
{
	if ( pBF ) {
		if ( pBF->pucBitArray ) {
			xrtFree(pBF->pucBitArray);
		}
		xrtFree(pBF);
	}
}

// Set bit at position
// 设置指定位
void BloomSetBit(BloomFilter* pBF, size_t iBitIndex)
{
	size_t iByteIndex = iBitIndex / 8;
	size_t iBitOffset = iBitIndex % 8;
	pBF->pucBitArray[iByteIndex] |= (1 << iBitOffset);
}

// Get bit at position
// 获取指定位
int BloomGetBit(BloomFilter* pBF, size_t iBitIndex)
{
	size_t iByteIndex = iBitIndex / 8;
	size_t iBitOffset = iBitIndex % 8;
	return (pBF->pucBitArray[iByteIndex] >> iBitOffset) & 1;
}

// Add element to Bloom filter
// 向布隆过滤器添加元素
void BloomAdd(BloomFilter* pBF, str sElement)
{
	size_t iBitIndices[20];  // Support up to 20 hash functions / 支持最多20个哈希函数
	
	// Calculate hash values
	// 计算哈希值
	iBitIndices[0] = Hash1(sElement, pBF->iBitCount);
	iBitIndices[1] = Hash2(sElement, pBF->iBitCount);
	iBitIndices[2] = Hash3(sElement, pBF->iBitCount);
	iBitIndices[3] = Hash4(sElement, pBF->iBitCount);
	
	// Generate additional hash values using linear combinations
	// 使用线性组合生成额外的哈希值
	for ( size_t i = 4; i < pBF->iHashCount; i++ ) {
		iBitIndices[i] = (iBitIndices[0] + i * iBitIndices[1]) % pBF->iBitCount;
	}
	
	// Set corresponding bits
	// 设置对应位
	for ( size_t i = 0; i < pBF->iHashCount; i++ ) {
		BloomSetBit(pBF, iBitIndices[i]);
	}
	
	pBF->iElementCount++;
}

// Check if element might be in set
// 检查元素是否可能在集合中
int BloomContains(BloomFilter* pBF, str sElement)
{
	size_t iBitIndices[20];
	
	// Calculate hash values
	// 计算哈希值
	iBitIndices[0] = Hash1(sElement, pBF->iBitCount);
	iBitIndices[1] = Hash2(sElement, pBF->iBitCount);
	iBitIndices[2] = Hash3(sElement, pBF->iBitCount);
	iBitIndices[3] = Hash4(sElement, pBF->iBitCount);
	
	// Generate additional hash values
	// 生成额外的哈希值
	for ( size_t i = 4; i < pBF->iHashCount; i++ ) {
		iBitIndices[i] = (iBitIndices[0] + i * iBitIndices[1]) % pBF->iBitCount;
	}
	
	// Check if all bits are set
	// 检查所有位是否都被设置
	for ( size_t i = 0; i < pBF->iHashCount; i++ ) {
		if ( !BloomGetBit(pBF, iBitIndices[i]) ) {
			return 0;  // Definitely not present / 肯定不存在
		}
	}
	
	return 1;  // Might be present (false positive possible) / 可能存在（可能出现误报）
}

// Calculate theoretical false positive rate
// 计算理论误报率
double BloomFalsePositiveRate(BloomFilter* pBF)
{
	// p = (1 - e^(-kn/m))^k
	// 其中 k=哈希函数数, n=元素数, m=位数
	double dK = (double)pBF->iHashCount;
	double dN = (double)pBF->iElementCount;
	double dM = (double)pBF->iBitCount;
	
	double dExp = exp(-dK * dN / dM);
	return pow(1.0 - dExp, dK);
}

// Get filter statistics
// 获取过滤器统计信息
void BloomPrintStats(BloomFilter* pBF)
{
	double dFalsePosRate = BloomFalsePositiveRate(pBF);
	size_t iByteCount = (pBF->iBitCount + 7) / 8;
	
	printf("Bloom Filter Statistics:\n");
	printf("  Bit array size: %zu bits (%zu bytes)\n", pBF->iBitCount, iByteCount);
	printf("  Hash functions: %zu\n", pBF->iHashCount);
	printf("  Elements inserted: %zu\n", pBF->iElementCount);
	printf("  Theoretical false positive rate: %.4f%%\n", dFalsePosRate * 100);
	
	// Count set bits
	// 计算设置的位数
	size_t iSetBits = 0;
	for ( size_t i = 0; i < pBF->iBitCount; i++ ) {
		if ( BloomGetBit(pBF, i) ) {
			iSetBits++;
		}
	}
	
	double dFillRatio = (double)iSetBits / pBF->iBitCount;
	printf("  Bits set: %zu/%zu (%.2f%%)\n", iSetBits, pBF->iBitCount, dFillRatio * 100);
}

// Test Bloom filter operations
// 测试布隆过滤器操作
void TestBloomFilter()
{
	printf("=== Bloom Filter Test ===\n");
	printf("=== 布隆过滤器测试 ===\n");
	
	// Create Bloom filter for 1000 elements with 1% false positive rate
	// 创建用于1000个元素且误报率为1%的布隆过滤器
	BloomFilter* pBF = BloomCreate(1000, 0.01);
	
	// Add some elements
	// 添加一些元素
	str arrElements[] = {"apple", "banana", "cherry", "date", "elderberry", 
	                    "fig", "grape", "honeydew", "kiwi", "lemon"};
	size_t iElementCount = sizeof(arrElements) / sizeof(arrElements[0]);
	
	printf("Adding elements:\n");
	for ( size_t i = 0; i < iElementCount; i++ ) {
		BloomAdd(pBF, arrElements[i]);
		printf("  Added: %s\n", arrElements[i]);
	}
	
	BloomPrintStats(pBF);
	
	// Test membership queries
	// 测试成员资格查询
	printf("\nTesting membership queries:\n");
	
	// Test elements that were added
	// 测试已添加的元素
	printf("Elements that were added:\n");
	for ( size_t i = 0; i < iElementCount; i++ ) {
		int iResult = BloomContains(pBF, arrElements[i]);
		printf("  %s: %s\n", arrElements[i], iResult ? "Present" : "Not Present");
	}
	
	// Test elements that were not added
	// 测试未添加的元素
	str arrNonElements[] = {"orange", "pear", "peach", "plum", "watermelon"};
	size_t iNonElementCount = sizeof(arrNonElements) / sizeof(arrNonElements[0]);
	
	printf("\nElements that were NOT added:\n");
	int iFalsePositives = 0;
	for ( size_t i = 0; i < iNonElementCount; i++ ) {
		int iResult = BloomContains(pBF, arrNonElements[i]);
		if ( iResult ) iFalsePositives++;
		printf("  %s: %s\n", arrNonElements[i], iResult ? "Present (FALSE POSITIVE)" : "Not Present");
	}
	
	printf("\nFalse positives: %d/%zu (%.2f%%)\n", 
	       iFalsePositives, iNonElementCount, 
	       (double)iFalsePositives / iNonElementCount * 100);
	
	// Cleanup
	// 清理
	BloomDestroy(pBF);
}

// Performance comparison with traditional set
// 与传统集合的性能比较
void TestPerformanceComparison()
{
	printf("\n=== Performance Comparison ===\n");
	printf("=== 性能比较 ===\n");
	
	// Test with larger dataset
	// 用更大的数据集测试
	size_t iTestSize = 5000;
	double dTargetFalsePosRate = 0.05;  // 5%
	
	BloomFilter* pBF = BloomCreate(iTestSize, dTargetFalsePosRate);
	
	// Generate test data
	// 生成测试数据
	str* psTestData = xrtMalloc(iTestSize * sizeof(str));
	
	srand((unsigned int)time(NULL));
	for ( size_t i = 0; i < iTestSize; i++ ) {
		// Generate random strings
		// 生成随机字符串
		psTestData[i] = xrtMalloc(20);
		sprintf(psTestData[i], "item_%zu_%d", i, rand() % 1000);
		BloomAdd(pBF, psTestData[i]);
	}
	
	BloomPrintStats(pBF);
	
	// Test lookup performance
	// 测试查找性能
	clock_t clkStart, clkEnd;
	double dTimeBF, dTimeLinear;
	
	// Bloom filter lookup performance
	// 布隆过滤器查找性能
	clkStart = clock();
	
	int iBFHits = 0;
	for ( size_t i = 0; i < iTestSize; i++ ) {
		if ( BloomContains(pBF, psTestData[i]) ) {
			iBFHits++;
		}
	}
	
	clkEnd = clock();
	dTimeBF = ((double)(clkEnd - clkStart)) / CLOCKS_PER_SEC;
	
	// Linear search performance (simulated)
	// 线性搜索性能（模拟）
	clkStart = clock();
	
	int iLinearHits = 0;
	for ( size_t i = 0; i < iTestSize; i++ ) {
		// Simulate checking against all elements
		// 模拟检查所有元素
		for ( size_t j = 0; j < iTestSize; j++ ) {
			if ( strcmp(psTestData[i], psTestData[j]) == 0 ) {
				iLinearHits++;
				break;
			}
		}
	}
	
	clkEnd = clock();
	dTimeLinear = ((double)(clkEnd - clkStart)) / CLOCKS_PER_SEC;
	
	printf("\nPerformance comparison (%zu elements):\n", iTestSize);
	printf("Bloom Filter: %.6f seconds (%d hits)\n", dTimeBF, iBFHits);
	printf("Linear Search: %.6f seconds (%d hits)\n", dTimeLinear, iLinearHits);
	printf("Speed improvement: %.2fx\n", dTimeLinear / dTimeBF);
	
	// Memory usage comparison
	// 内存使用比较
	size_t iBFMemory = (pBF->iBitCount + 7) / 8;
	size_t iLinearMemory = iTestSize * sizeof(str) + iTestSize * 20;  // Approximate
	
	printf("\nMemory usage comparison:\n");
	printf("Bloom Filter: %zu bytes\n", iBFMemory);
	printf("Linear array: %zu bytes\n", iLinearMemory);
	printf("Space savings: %.2fx\n", (double)iLinearMemory / iBFMemory);
	
	// Cleanup
	// 清理
	BloomDestroy(pBF);
	for ( size_t i = 0; i < iTestSize; i++ ) {
		xrtFree(psTestData[i]);
	}
	xrtFree(psTestData);
}

// Test false positive rate accuracy
// 测试误报率准确性
void TestFalsePositiveAccuracy()
{
	printf("\n=== False Positive Rate Test ===\n");
	printf("=== 误报率测试 ===\n");
	
	// Create filter with known parameters
	// 创建具有已知参数的过滤器
	size_t iElements = 1000;
	BloomFilter* pBF = BloomCreate(iElements, 0.01);  // 1% target
	
	// Add elements
	// 添加元素
	for ( size_t i = 0; i < iElements; i++ ) {
		char sKey[32];
		sprintf(sKey, "key_%zu", i);
		BloomAdd(pBF, sKey);
	}
	
	// Test many non-existent elements
	// 测试许多不存在的元素
	size_t iTestCount = 10000;
	int iFalsePositives = 0;
	
	for ( size_t i = iElements; i < iElements + iTestCount; i++ ) {
		char sKey[32];
		sprintf(sKey, "key_%zu", i);
		if ( BloomContains(pBF, sKey) ) {
			iFalsePositives++;
		}
	}
	
	double dActualRate = (double)iFalsePositives / iTestCount;
	double dTheoreticalRate = BloomFalsePositiveRate(pBF);
	
	printf("False positive rate test:\n");
	printf("  Tested %zu non-existent elements\n", iTestCount);
	printf("  Actual false positives: %d\n", iFalsePositives);
	printf("  Actual rate: %.4f%%\n", dActualRate * 100);
	printf("  Theoretical rate: %.4f%%\n", dTheoreticalRate * 100);
	printf("  Difference: %.4f%%\n", fabs(dActualRate - dTheoreticalRate) * 100);
	
	BloomDestroy(pBF);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Bloom Filter Demo\n");
	printf("XRT 布隆过滤器演示\n");
	printf("===================\n\n");
	
	// Run all tests
	// 运行所有测试
	TestBloomFilter();
	TestPerformanceComparison();
	TestFalsePositiveAccuracy();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}