/*
 * XRT Example - Hash32
 * XRT 范例 - Hash32
 *
 * Description / 说明:
 *   EN: Demonstrates xrtHash32 and xrtHash32_WithSeed for 32-bit hash
 *       calculations.
 *   CN: 演示 xrtHash32 和 xrtHash32_WithSeed 进行 32 位哈希计算。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/hash_hash32.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/hash_hash32              (Linux, GCC)
 *
 * Note / 注意:
 *   - Returns 32-bit unsigned integer hash value
 *   - Same input always produces same output (deterministic)
 *   - Seed allows different hash values for same input
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test basic hash
// 测试基本哈希
void TestBasicHash()
{
	printf("=== Basic Hash32 ===\n");
	printf("=== 基本 Hash32 ===\n");
	
	char* psStrings[] = {
		"Hello",
		"World",
		"Hello",  // Same as first
		"hello",  // Different case
		"",
		"a",
		"The quick brown fox jumps over the lazy dog",
	};
	
	int iNumStrings = 7;
	int i;
	for ( i = 0; i < iNumStrings; i++ ) {
		uint32 uiHash = xrtHash32(psStrings[i], strlen(psStrings[i]));
		printf("  \"%s\" -> 0x%08X (%u)\n", psStrings[i], uiHash, uiHash);
	}
	printf("\n");
}

// Test hash with seed
// 测试带种子的哈希
void TestHashWithSeed()
{
	printf("=== Hash32 with Seed ===\n");
	printf("=== 带种子的 Hash32 ===\n");
	
	str sInput = "Hello World";
	size_t iLen = strlen(sInput);
	
	printf("Input: \"%s\"\n\n", sInput);
	
	uint32 uiSeeds[] = {0, 1, 12345, 0xDEADBEEF, 0xFFFFFFFF};
	int iNumSeeds = 5;
	int i;
	for ( i = 0; i < iNumSeeds; i++ ) {
		uint32 uiHash = xrtHash32_WithSeed(sInput, iLen, uiSeeds[i]);
		printf("  Seed 0x%08X -> Hash 0x%08X\n", uiSeeds[i], uiHash);
	}
	printf("\n");
}

// Test hash collision demonstration
// 测试哈希冲突演示
void TestCollision()
{
	printf("=== Hash Distribution ===\n");
	printf("=== 哈希分布 ===\n");
	
	// Hash multiple similar strings
	// 哈希多个相似字符串
	printf("Hashing 10 similar strings:\n");
	printf("哈希 10 个相似字符串:\n");
	
	int i;
	for ( i = 0; i < 10; i++ ) {
		str sStr = xrtFormat("string_%d", i);
		uint32 uiHash = xrtHash32(sStr, strlen(sStr));
		printf("  %s -> 0x%08X\n", sStr, uiHash);
		xrtFree(sStr);
	}
	printf("\n");
}

// Test binary data hashing
// 测试二进制数据哈希
void TestBinaryHash()
{
	printf("=== Binary Data Hash ===\n");
	printf("=== 二进制数据哈希 ===\n");
	
	unsigned char arrData1[] = {0x00, 0x01, 0x02, 0x03, 0xFF, 0xFE, 0xFD};
	unsigned char arrData2[] = {0x00, 0x01, 0x02, 0x03, 0xFF, 0xFE, 0xFC};  // Last byte different
	
	uint32 uiHash1 = xrtHash32(arrData1, sizeof(arrData1));
	uint32 uiHash2 = xrtHash32(arrData2, sizeof(arrData2));
	
	printf("Data 1: 00 01 02 03 FF FE FD -> 0x%08X\n", uiHash1);
	printf("Data 2: 00 01 02 03 FF FE FC -> 0x%08X\n", uiHash2);
	printf("Different: %s\n", uiHash1 != uiHash2 ? "YES" : "NO");
	printf("不同: %s\n", uiHash1 != uiHash2 ? "是" : "否");
	printf("\n");
}

// Test practical use cases
// 测试实际用例
void TestPracticalUseCases()
{
	printf("=== Practical Use Cases ===\n");
	printf("=== 实际用例 ===\n");
	
	// Simple hash table key
	// 简单哈希表键
	printf("Hash table keys:\n");
	printf("哈希表键:\n");
	
	char* psKeys[] = {"username", "password", "email", "created_at"};
	int iNumKeys = 4;
	int i;
	for ( i = 0; i < iNumKeys; i++ ) {
		uint32 uiHash = xrtHash32(psKeys[i], strlen(psKeys[i]));
		printf("  %s -> bucket %u\n", psKeys[i], uiHash % 16);
	}
	printf("\n");
	
	// File content checksum
	// 文件内容校验和
	printf("Content checksum:\n");
	printf("内容校验和:\n");
	
	str sContent = "This is some file content to checksum.\n这是要计算校验和的文件内容。\n";
	uint32 uiChecksum = xrtHash32(sContent, strlen(sContent));
	printf("  Content hash: 0x%08X\n", uiChecksum);
	printf("  内容哈希: 0x%08X\n", uiChecksum);
	printf("\n");
	
	// Quick string comparison
	// 快速字符串比较
	printf("Quick comparison using hash:\n");
	printf("使用哈希快速比较:\n");
	
	str sA = "user_session_12345";
	str sB = "user_session_12345";
	str sC = "user_session_12346";
	
	uint32 uiHashA = xrtHash32(sA, strlen(sA));
	uint32 uiHashB = xrtHash32(sB, strlen(sB));
	uint32 uiHashC = xrtHash32(sC, strlen(sC));
	
	printf("  A vs B: hash %s (strings %s)\n", 
	       uiHashA == uiHashB ? "equal" : "different",
	       strcmp(sA, sB) == 0 ? "equal" : "different");
	printf("  A vs C: hash %s (strings %s)\n",
	       uiHashA == uiHashC ? "equal" : "different",
	       strcmp(sA, sC) == 0 ? "equal" : "different");
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Hash - Hash32 Demo\n");
	printf("XRT 哈希 - Hash32 演示\n");
	printf("======================\n\n");
	
	TestBasicHash();
	TestHashWithSeed();
	TestCollision();
	TestBinaryHash();
	TestPracticalUseCases();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
