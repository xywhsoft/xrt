/*
 * XRT Example - Hash64
 * XRT 范例 - Hash64
 *
 * Description / 说明:
 *   EN: Demonstrates xrtHash64, xrtHash64_Micro, xrtHash64_Nano for
 *       64-bit hash calculations with different quality levels.
 *   CN: 演示 xrtHash64、xrtHash64_Micro、xrtHash64_Nano 进行不同质量的
 *       64 位哈希计算。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/hash_hash64.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/hash_hash64              (Linux, GCC)
 *
 * Note / 注意:
 *   - xrtHash64: standard quality
 *   - xrtHash64_Micro: fast, lower quality
 *   - xrtHash64_Nano: fastest, lowest quality
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test basic hash64
// 测试基本 hash64
void TestBasicHash64()
{
	printf("=== Basic Hash64 ===\n");
	printf("=== 基本 Hash64 ===\n");
	
	char* psStrings[] = {
		"Hello",
		"World",
		"Hello",
		"",
		"The quick brown fox jumps over the lazy dog",
	};
	
	int iNumStrings = 5;
	int i;
	for ( i = 0; i < iNumStrings; i++ ) {
		uint64 uiHash = xrtHash64(psStrings[i], strlen(psStrings[i]));
		printf("  \"%s\" -> 0x%016llX\n", psStrings[i], uiHash);
	}
	printf("\n");
}

// Test different hash quality levels
// 测试不同哈希质量级别
void TestHashQualityLevels()
{
	printf("=== Hash Quality Levels ===\n");
	printf("=== 哈希质量级别 ===\n");
	
	str sInput = "Test string for hashing";
	size_t iLen = strlen(sInput);
	
	printf("Input: \"%s\"\n\n", sInput);
	
	// Standard
	// 标准
	uint64 uiStandard = xrtHash64(sInput, iLen);
	printf("  Standard: 0x%016llX\n", uiStandard);
	
	// Micro (fast)
	// Micro (快速)
	uint64 uiMicro = xrtHash64_Micro(sInput, iLen);
	printf("  Micro:    0x%016llX\n", uiMicro);
	
	// Nano (fastest)
	// Nano (最快)
	uint64 uiNano = xrtHash64_Nano(sInput, iLen);
	printf("  Nano:     0x%016llX\n", uiNano);
	printf("\n");
}

// Test hash with seed
// 测试带种子的哈希
void TestHash64WithSeed()
{
	printf("=== Hash64 with Seed ===\n");
	printf("=== 带种子的 Hash64 ===\n");
	
	str sInput = "Hello World";
	size_t iLen = strlen(sInput);
	
	printf("Input: \"%s\"\n\n", sInput);
	
	uint64 uiSeeds[] = {0, 1, 123456789, 0xDEADBEEFCAFEBABE};
	int iNumSeeds = 4;
	int i;
	for ( i = 0; i < iNumSeeds; i++ ) {
		uint64 uiHash = xrtHash64_WithSeed(sInput, iLen, uiSeeds[i]);
		printf("  Seed 0x%016llX -> Hash 0x%016llX\n", uiSeeds[i], uiHash);
	}
	printf("\n");
}

// Test performance comparison
// 测试性能比较
void TestPerformance()
{
	printf("=== Performance Comparison ===\n");
	printf("=== 性能比较 ===\n");
	
	str sInput = "The quick brown fox jumps over the lazy dog 0123456789";
	size_t iLen = strlen(sInput);
	int iIterations = 100000;
	
	printf("Hashing %d times...\n", iIterations);
	printf("哈希 %d 次...\n", iIterations);
	
	// Standard
	// 标准
	double fStart = xrtTimer();
	int i;
	for ( i = 0; i < iIterations; i++ ) {
		xrtHash64(sInput, iLen);
	}
	double fStandard = xrtTimer() - fStart;
	
	// Micro
	// Micro
	fStart = xrtTimer();
	for ( i = 0; i < iIterations; i++ ) {
		xrtHash64_Micro(sInput, iLen);
	}
	double fMicro = xrtTimer() - fStart;
	
	// Nano
	// Nano
	fStart = xrtTimer();
	for ( i = 0; i < iIterations; i++ ) {
		xrtHash64_Nano(sInput, iLen);
	}
	double fNano = xrtTimer() - fStart;
	
	printf("  Standard: %.3f ms (%.2f M/s)\n", fStandard * 1000, iIterations / fStandard / 1000000);
	printf("  Micro:    %.3f ms (%.2f M/s)\n", fMicro * 1000, iIterations / fMicro / 1000000);
	printf("  Nano:     %.3f ms (%.2f M/s)\n", fNano * 1000, iIterations / fNano / 1000000);
	printf("\n");
}

// Test practical use cases
// 测试实际用例
void TestPracticalUseCases()
{
	printf("=== Practical Use Cases ===\n");
	printf("=== 实际用例 ===\n");
	
	// Unique ID generation
	// 唯一 ID 生成
	printf("Unique IDs:\n");
	printf("唯一 ID:\n");
	
	xtime tNow = xrtNow();
	int i;
	for ( i = 0; i < 5; i++ ) {
		str sInput = xrtFormat("id_%lld_%d", tNow, i);
		uint64 uiHash = xrtHash64(sInput, strlen(sInput));
		printf("  %s -> %016llX\n", sInput, uiHash);
		xrtFree(sInput);
	}
	printf("\n");
	
	// Content-addressed storage key
	// 内容寻址存储键
	printf("Content-addressed keys:\n");
	printf("内容寻址键:\n");
	
	char* psContents[] = {
		"File content A",
		"File content B",
		"File content A",  // Duplicate
	};
	int iNumContents = 3;
	for ( i = 0; i < iNumContents; i++ ) {
		uint64 uiHash = xrtHash64(psContents[i], strlen(psContents[i]));
		printf("  \"%s\" -> %016llX\n", psContents[i], uiHash);
	}
	printf("\n");
	
	// Cache key generation
	// 缓存键生成
	printf("Cache keys:\n");
	printf("缓存键:\n");
	
	str sURL = "https://example.com/api/users?page=1&limit=10";
	uint64 uiCacheKey = xrtHash64(sURL, strlen(sURL));
	printf("  URL: %s\n", sURL);
	printf("  Cache key: %016llX\n", uiCacheKey);
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Hash - Hash64 Demo\n");
	printf("XRT 哈希 - Hash64 演示\n");
	printf("======================\n\n");
	
	TestBasicHash64();
	TestHashQualityLevels();
	TestHash64WithSeed();
	TestPerformance();
	TestPracticalUseCases();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
