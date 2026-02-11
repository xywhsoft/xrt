/*
 * XRT Example - Random Number Generation
 * XRT 范例 - 随机数生成
 *
 * Description / 说明:
 *   EN: Demonstrates PCG random number generation including global rand, 
 *       independent xrand states, range generation, and thread-safe versions.
 *   CN: 演示 PCG 随机数生成，包括全局随机数、独立 xrand 状态、范围生成和线程安全版本。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/math_random.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/math_random -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - xrtRand32/64 returns uint32_t/uint64_t
 *   - xrtRandRange generates [min, max] inclusive range
 *   - Thread-safe versions use _r suffix
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test global random number generation
// 测试全局随机数生成
void TestGlobalRandom()
{
	int		iCount = 10;
	int		i;
	
	printf("=== Global Random Numbers ===\n");
	printf("=== 全局随机数 ===\n");
	
	for ( i = 1; i <= iCount; i++ )
	{
		uint32	ui32Value = xrtRand32();
		uint64	ui64Value = xrtRand64();
		
		printf("Round %d: 32-bit=%u, 64-bit=%I64u\n", 
		       i, ui32Value, ui64Value);
	}
	printf("\n");
}

// Test independent xrand state
// 测试独立 xrand 状态
void TestIndependentState()
{
	xrand	sMyRand;
	int		iCount = 8;
	int		i;
	
	printf("=== Independent xrand State ===\n");
	printf("=== 独立 xrand 状态 ===\n");
	
	// Initialize with custom seed
	// 使用自定义种子初始化
	xrtRandSeed(&sMyRand, 12345, 0);
	
	printf("Custom seed=12345:\n");
	for ( i = 1; i <= iCount; i++ )
	{
		uint32 ui32Value = xrtRand32Ex(&sMyRand);
		printf("  [%d] %u\n", i, ui32Value);
	}
	
	// Reset with same seed - should produce same sequence
	// 用相同种子重置 - 应该产生相同序列
	xrtRandSeed(&sMyRand, 12345, 0);
	printf("\nSame seed again:\n");
	for ( i = 1; i <= iCount; i++ )
	{
		uint32 ui32Value = xrtRand32Ex(&sMyRand);
		printf("  [%d] %u\n", i, ui32Value);
	}
	printf("\n");
}

// Test range generation
// 测试范围生成
void TestRangeGeneration()
{
	int		iCount = 15;
	int		iMin = 1;
	int		iMax = 100;
	int		i;
	
	printf("=== Range Generation [%d, %d] ===\n", iMin, iMax);
	printf("=== 范围生成 [%d, %d] ===\n", iMin, iMax);
	
	for ( i = 1; i <= iCount; i++ )
	{
		int iValue = xrtRandRange(iMin, iMax);
		printf("[%2d] %d\n", i, iValue);
	}
	printf("\n");
	
	// Note: xrt does not provide float random function
	// 注意：xrt 不提供浮点随机数函数
}

// Test distribution verification
// 测试分布验证
void TestDistribution()
{
	int		iBuckets[10] = {0};  // 10 buckets for 0-9
	int		iTotal = 10000;
	int		i;
	
	printf("=== Distribution Test (10000 samples) ===\n");
	printf("=== 分布测试 (10000 个样本) ===\n");
	
	// Generate samples and count distribution
	// 生成样本并统计分布
	for ( i = 1; i <= iTotal; i++ )
	{
		int iValue = xrtRandRange(0, 9);  // 0-9 range
		iBuckets[iValue]++;
	}
	
	// Display histogram
	// 显示直方图
	for ( i = 0; i < 10; i++ )
	{
		float fPercent = (float)iBuckets[i] / iTotal * 100.0f;
		int   iBarLen = (int)(fPercent / 2);  // Scale for display
		char  sBar[51] = {0};
		
		memset((void*)sBar, '=', iBarLen);
		printf("%d: %6d (%5.1f%%) [%s]\n", 
		       i, iBuckets[i], fPercent, sBar);
	}
	printf("\n");
}

// Test seeded generation
// 测试带种子生成
void TestSeededGeneration()
{
	uint32	uiSeed = 98765;
	int		iCount = 5;
	int		i;
	
	printf("=== Seeded Generation (seed=%u) ===\n", uiSeed);
	printf("=== 带种子生成 (种子=%u) ===\n", uiSeed);
	
	// Method 1: Initialize global rand with seed
	// 方法1：用种子初始化全局随机数
	xrtRandSeed(&xCore.rand32, uiSeed, 0);
	printf("Global rand with seed:\n");
	for ( i = 1; i <= iCount; i++ )
	{
		printf("  [%d] %u\n", i, xrtRand32());
	}
	
	// Method 2: Independent state with seed
	// 方法2：带种子的独立状态
	xrand sRand;
	xrtRandSeed(&sRand, uiSeed, 0);
	printf("\nIndependent state with same seed:\n");
	for ( i = 1; i <= iCount; i++ )
	{
		printf("  [%d] %u\n", i, xrtRand32Ex(&sRand));
	}
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Math Random Number Generation Demo\n");
	printf("XRT 数学随机数生成演示\n");
	printf("=====================================\n\n");
	
	// Test different random number features
	// 测试不同的随机数功能
	TestGlobalRandom();
	TestIndependentState();
	TestRangeGeneration();
	TestDistribution();
	TestSeededGeneration();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}