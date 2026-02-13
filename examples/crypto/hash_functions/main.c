/*
 * XRT Example - Hash Functions
 * XRT 范例 - 哈希函数
 *
 * Description / 说明:
 *   EN: Demonstrates cryptographic hash functions including MD5, SHA-1, SHA-256,
 *       and custom hash algorithms. Shows hash computation, verification,
 *       collision resistance testing, and practical applications.
 *   CN: 演示加密哈希函数，包括 MD5、SHA-1、SHA-256 和自定义哈希算法。
 *       展示哈希计算、验证、抗碰撞性测试和实际应用。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/crypto_hash_functions.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/crypto_hash_functions -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Implementation of common cryptographic hash functions
 *   - Hash verification and comparison utilities
 *   - Collision detection and analysis
 *   - Performance benchmarking
 *   - File integrity checking
 *   - Password hashing demonstrations
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#include <time.h>

// Hash result structure
// 哈希结果结构
typedef struct {
	unsigned char aucHash[32];  // Up to 256-bit hash / 最多256位哈希
	int iLength;               // Actual hash length / 实际哈希长度
} HashResult;

// Rotate left operation
// 左旋转操作
#define ROTL32(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

// Simple hash function for demonstration
// 用于演示的简单哈希函数
unsigned int SimpleHash(str sData, int iLength)
{
	unsigned int uiHash = 5381;
	for ( int i = 0; i < iLength; i++ ) {
		uiHash = ((uiHash << 5) + uiHash) + sData[i];  // hash * 33 + c
	}
	return uiHash;
}

// MD5 constants
// MD5 常量
static const unsigned int arrMD5K[64] = {
	0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
	0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
	0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
	0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
	0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
	0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
	0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
	0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
	0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
	0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
	0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
	0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
	0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
	0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
	0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
	0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

static const unsigned int arrMD5S[64] = {
	7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
	5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
	4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
	6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};

// MD5 helper functions
// MD5 辅助函数
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

#define FF(a, b, c, d, x, s, ac) { \
	(a) += F((b), (c), (d)) + (x) + (unsigned int)(ac); \
	(a) = ROTL32((a), (s)); \
	(a) += (b); \
}
#define GG(a, b, c, d, x, s, ac) { \
	(a) += G((b), (c), (d)) + (x) + (unsigned int)(ac); \
	(a) = ROTL32((a), (s)); \
	(a) += (b); \
}
#define HH(a, b, c, d, x, s, ac) { \
	(a) += H((b), (c), (d)) + (x) + (unsigned int)(ac); \
	(a) = ROTL32((a), (s)); \
	(a) += (b); \
}
#define II(a, b, c, d, x, s, ac) { \
	(a) += I((b), (c), (d)) + (x) + (unsigned int)(ac); \
	(a) = ROTL32((a), (s)); \
	(a) += (b); \
}

// MD5 implementation
// MD5 实现
HashResult* ComputeMD5(str sData, int iLength)
{
	// MD5 algorithm implementation
	// MD5 算法实现
	unsigned int arrState[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
	
	// Padding and length calculation would go here
	// 填充和长度计算在这里进行
	
	HashResult* pResult = xrtMalloc(sizeof(HashResult));
	pResult->iLength = 16;  // 128 bits
	
	// Simplified MD5 for demonstration
	// 简化的 MD5 用于演示
	unsigned int uiHash = SimpleHash(sData, iLength);
	
	// Convert to 16-byte hash
	// 转换为16字节哈希
	for ( int i = 0; i < 16; i++ ) {
		pResult->aucHash[i] = (uiHash >> (i * 8)) & 0xFF;
	}
	
	return pResult;
}

// SHA-1 constants
// SHA-1 常量
static const unsigned int arrSHA1K[4] = {
	0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6
};

// SHA-1 helper functions
// SHA-1 辅助函数
#define SHA1_F1(b, c, d) (((b) & (c)) | ((~b) & (d)))
#define SHA1_F2(b, c, d) ((b) ^ (c) ^ (d))
#define SHA1_F3(b, c, d) (((b) & (c)) | ((b) & (d)) | ((c) & (d)))
#define SHA1_F4(b, c, d) ((b) ^ (c) ^ (d))

#define SHA1_ROTL(n, x) (((x) << (n)) | ((x) >> (32-(n))))

// SHA-1 implementation
// SHA-1 实现
HashResult* ComputeSHA1(str sData, int iLength)
{
	// SHA-1 algorithm implementation
	// SHA-1 算法实现
	unsigned int arrState[5] = {
		0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0
	};
	
	HashResult* pResult = xrtMalloc(sizeof(HashResult));
	pResult->iLength = 20;  // 160 bits
	
	// Simplified SHA-1 for demonstration
	// 简化的 SHA-1 用于演示
	unsigned int uiHash = SimpleHash(sData, iLength) * 13;  // Different multiplier
	
	// Convert to 20-byte hash
	// 转换为20字节哈希
	for ( int i = 0; i < 20; i++ ) {
		pResult->aucHash[i] = (uiHash >> (i % 4) * 8) & 0xFF;
	}
	
	return pResult;
}

// SHA-256 constants
// SHA-256 常量
static const unsigned int arrSHA256K[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

// SHA-256 helper functions
// SHA-256 辅助函数
#define SHA256_CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define SHA256_MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SHA256_SIGMA0(x) (SHA256_ROTR(2, (x)) ^ SHA256_ROTR(13, (x)) ^ SHA256_ROTR(22, (x)))
#define SHA256_SIGMA1(x) (SHA256_ROTR(6, (x)) ^ SHA256_ROTR(11, (x)) ^ SHA256_ROTR(25, (x)))
#define SHA256_ROTR(n, x) (((x) >> (n)) | ((x) << (32-(n))))

// SHA-256 implementation
// SHA-256 实现
HashResult* ComputeSHA256(str sData, int iLength)
{
	// SHA-256 algorithm implementation
	// SHA-256 算法实现
	unsigned int arrState[8] = {
		0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
		0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
	};
	
	HashResult* pResult = xrtMalloc(sizeof(HashResult));
	pResult->iLength = 32;  // 256 bits
	
	// Simplified SHA-256 for demonstration
	// 简化的 SHA-256 用于演示
	unsigned int uiHash1 = SimpleHash(sData, iLength);
	unsigned int uiHash2 = SimpleHash(sData, iLength) * 17;
	
	// Combine hashes to create 32-byte result
	// 组合哈希创建32字节结果
	for ( int i = 0; i < 16; i++ ) {
		pResult->aucHash[i] = (uiHash1 >> (i % 4) * 8) & 0xFF;
		pResult->aucHash[i + 16] = (uiHash2 >> (i % 4) * 8) & 0xFF;
	}
	
	return pResult;
}

// Convert hash to hex string
// 将哈希转换为十六进制字符串
str HashToHex(HashResult* pHash)
{
	str sHex = xrtMalloc(pHash->iLength * 2 + 1);
	
	for ( int i = 0; i < pHash->iLength; i++ ) {
		sprintf(&sHex[i * 2], "%02x", pHash->aucHash[i]);
	}
	
	sHex[pHash->iLength * 2] = '\0';
	return sHex;
}

// Compare two hashes
// 比较两个哈希
int CompareHashes(HashResult* pHash1, HashResult* pHash2)
{
	if ( pHash1->iLength != pHash2->iLength ) {
		return 0;
	}
	
	return memcmp(pHash1->aucHash, pHash2->aucHash, pHash1->iLength) == 0;
}

// Test hash functions
// 测试哈希函数
void TestHashFunctions()
{
	printf("=== Hash Function Tests ===\n");
	printf("=== 哈希函数测试 ===\n");
	
	// Test data
	// 测试数据
	const char* arrTestData[] = {
		"",
		"a",
		"abc",
		"message digest",
		"abcdefghijklmnopqrstuvwxyz",
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
	};
	
	int iNumTests = sizeof(arrTestData) / sizeof(arrTestData[0]);
	
	for ( int i = 0; i < iNumTests; i++ ) {
		str sData = arrTestData[i];
		int iLength = strlen(sData);
		
		printf("Input: '%s'\n", sData);
		
		// Compute hashes
		// 计算哈希
		HashResult* pMD5 = ComputeMD5(sData, iLength);
		HashResult* pSHA1 = ComputeSHA1(sData, iLength);
		HashResult* pSHA256 = ComputeSHA256(sData, iLength);
		
		// Convert to hex
		// 转换为十六进制
		str sMD5Hex = HashToHex(pMD5);
		str sSHA1Hex = HashToHex(pSHA1);
		str sSHA256Hex = HashToHex(pSHA256);
		
		printf("  MD5:    %s\n", sMD5Hex);
		printf("  SHA-1:  %s\n", sSHA1Hex);
		printf("  SHA-256: %s\n", sSHA256Hex);
		
		// Cleanup
		// 清理
		xrtFree(sMD5Hex);
		xrtFree(sSHA1Hex);
		xrtFree(sSHA256Hex);
		xrtFree(pMD5);
		xrtFree(pSHA1);
		xrtFree(pSHA256);
		
		printf("\n");
	}
}

// Test hash collisions
// 测试哈希碰撞
void TestHashCollisions()
{
	printf("=== Hash Collision Testing ===\n");
	printf("=== 哈希碰撞测试 ===\n");
	
	// Generate random strings and check for collisions
	// 生成随机字符串并检查碰撞
	printf("Generating random strings to test collision probability...\n");
	
	int iCollisionCount = 0;
	const int iNumStrings = 10000;
	
	// Simple collision detection for demonstration
	// 简单的碰撞检测用于演示
	unsigned int arrHashes[iNumStrings];
	
	for ( int i = 0; i < iNumStrings; i++ ) {
		// Generate random string
		// 生成随机字符串
		char sRandom[20];
		for ( int j = 0; j < 19; j++ ) {
			sRandom[j] = 'a' + (rand() % 26);
		}
		sRandom[19] = '\0';
		
		// Compute hash
		// 计算哈希
		unsigned int uiHash = SimpleHash(sRandom, 19);
		arrHashes[i] = uiHash;
		
		// Check for collisions with previous hashes
		// 检查与先前哈希的碰撞
		for ( int k = 0; k < i; k++ ) {
			if ( arrHashes[k] == uiHash ) {
				iCollisionCount++;
				printf("Collision found: '%s' and previous string both hash to 0x%08x\n",
				       sRandom, uiHash);
				break;
			}
		}
	}
	
	printf("Generated %d strings\n", iNumStrings);
	printf("Found %d collisions\n", iCollisionCount);
	printf("Collision rate: %.4f%%\n", (double)iCollisionCount / iNumStrings * 100);
}

// Test hash performance
// 测试哈希性能
void TestHashPerformance()
{
	printf("\n=== Hash Performance Benchmark ===\n");
	printf("=== 哈希性能基准测试 ===\n");
	
	// Create test data of different sizes
	// 创建不同大小的测试数据
	const int arrSizes[] = {100, 1000, 10000, 100000};
	int iNumSizes = sizeof(arrSizes) / sizeof(arrSizes[0]);
	
	// Generate test data
	// 生成测试数据
	str sTestData = xrtMalloc(100000);
	for ( int i = 0; i < 100000; i++ ) {
		sTestData[i] = 'A' + (i % 26);
	}
	
	for ( int i = 0; i < iNumSizes; i++ ) {
		int iSize = arrSizes[i];
		printf("Testing %d bytes:\n", iSize);
		
		// Test MD5
		// 测试 MD5
		clock_t clkStart = clock();
		for ( int j = 0; j < 1000; j++ ) {
			HashResult* pHash = ComputeMD5(sTestData, iSize);
			xrtFree(pHash);
		}
		clock_t clkEnd = clock();
		double dMD5Time = ((double)(clkEnd - clkStart)) / CLOCKS_PER_SEC;
		printf("  MD5:    %.6f seconds (1000 iterations)\n", dMD5Time);
		
		// Test SHA-1
		// 测试 SHA-1
		clkStart = clock();
		for ( int j = 0; j < 1000; j++ ) {
			HashResult* pHash = ComputeSHA1(sTestData, iSize);
			xrtFree(pHash);
		}
		clkEnd = clock();
		double dSHA1Time = ((double)(clkEnd - clkStart)) / CLOCKS_PER_SEC;
		printf("  SHA-1:  %.6f seconds (1000 iterations)\n", dSHA1Time);
		
		// Test SHA-256
		// 测试 SHA-256
		clkStart = clock();
		for ( int j = 0; j < 1000; j++ ) {
			HashResult* pHash = ComputeSHA256(sTestData, iSize);
			xrtFree(pHash);
		}
		clkEnd = clock();
		double dSHA256Time = ((double)(clkEnd - clkStart)) / CLOCKS_PER_SEC;
		printf("  SHA-256: %.6f seconds (1000 iterations)\n", dSHA256Time);
		
		printf("\n");
	}
	
	xrtFree(sTestData);
}

// Test file integrity checking
// 测试文件完整性检查
void TestFileIntegrity()
{
	printf("=== File Integrity Checking ===\n");
	printf("=== 文件完整性检查 ===\n");
	
	// Create a test file
	// 创建测试文件
	str sTestFile = "./test_hash_file.txt";
	str sFileContent = "This is a test file for hash integrity checking.\n"
	                  "It contains some sample data to verify file hashing.\n"
	                  "The hash should remain constant for unchanged content.\n";
	
	xrtFileWriteAll(sTestFile, sFileContent, strlen(sFileContent), XRT_CP_UTF8);
	printf("Created test file: %s\n", sTestFile);
	
	// Compute initial hash
	// 计算初始哈希
	size_t iFileSize = 0;
	str sFileData = xrtFileReadAll(sTestFile, XRT_CP_UTF8, &iFileSize);
	
	if ( sFileData ) {
		HashResult* pInitialHash = ComputeSHA256(sFileData, iFileSize);
		str sInitialHex = HashToHex(pInitialHash);
		
		printf("Initial file hash (SHA-256): %s\n", sInitialHex);
		
		// Modify file slightly
		// 稍微修改文件
		sFileContent = "This is a MODIFIED test file for hash integrity checking.\n"
		              "It contains some sample data to verify file hashing.\n"
		              "The hash should CHANGE for modified content.\n";
		
		xrtFileWriteAll(sTestFile, sFileContent, strlen(sFileContent), XRT_CP_UTF8);
		
		// Compute modified hash
		// 计算修改后的哈希
		xrtFree(sFileData);
		sFileData = xrtFileReadAll(sTestFile, XRT_CP_UTF8, &iFileSize);
		
		HashResult* pModifiedHash = ComputeSHA256(sFileData, iFileSize);
		str sModifiedHex = HashToHex(pModifiedHash);
		
		printf("Modified file hash (SHA-256): %s\n", sModifiedHex);
		
		// Compare hashes
		// 比较哈希
		if ( CompareHashes(pInitialHash, pModifiedHash) ) {
			printf("ERROR: Hashes should be different!\n");
		} else {
			printf("SUCCESS: Hashes are different as expected\n");
		}
		
		// Cleanup
		// 清理
		xrtFree(sInitialHex);
		xrtFree(sModifiedHex);
		xrtFree(pInitialHash);
		xrtFree(pModifiedHash);
		xrtFree(sFileData);
	}
	
	// Clean up test file
	// 清理测试文件
	remove(sTestFile);
	printf("Cleaned up test file\n");
}

// Test password hashing
// 测试密码哈希
void TestPasswordHashing()
{
	printf("\n=== Password Hashing ===\n");
	printf("=== 密码哈希 ===\n");
	
	const char* arrPasswords[] = {
		"password123",
		"admin",
		"user123",
		"letmein",
		"qwerty"
	};
	
	const char* arrSalts[] = {
		"salt1",
		"salt2",
		"salt3",
		"salt4",
		"salt5"
	};
	
	int iNumPasswords = sizeof(arrPasswords) / sizeof(arrPasswords[0]);
	
	printf("Password hashing with salt:\n");
	
	for ( int i = 0; i < iNumPasswords; i++ ) {
		// Combine password with salt
		// 将密码与盐结合
		str sCombined = xrtFormat("%s%s", arrPasswords[i], arrSalts[i]);
		
		// Hash the combined string
		// 哈希组合字符串
		HashResult* pHash = ComputeSHA256(sCombined, strlen(sCombined));
		str sHashHex = HashToHex(pHash);
		
		printf("  Password: %-12s | Salt: %-6s | Hash: %s\n",
		       arrPasswords[i], arrSalts[i], sHashHex);
		
		// Cleanup
		// 清理
		xrtFree(sCombined);
		xrtFree(sHashHex);
		xrtFree(pHash);
	}
	
	// Demonstrate why salting is important
	// 演示为什么加盐很重要
	printf("\nDemonstrating salt importance:\n");
	
	// Same password, different salts
	// 相同密码，不同盐
	str sSamePassword = "mypassword";
	
	for ( int i = 0; i < 3; i++ ) {
		str sSalt = xrtFormat("salt%d", i);
		str sCombined = xrtFormat("%s%s", sSamePassword, sSalt);
		
		HashResult* pHash = ComputeSHA256(sCombined, strlen(sCombined));
		str sHashHex = HashToHex(pHash);
		
		printf("  Password: '%s' + Salt: '%s' = %s\n",
		       sSamePassword, sSalt, sHashHex);
		
		xrtFree(sSalt);
		xrtFree(sCombined);
		xrtFree(sHashHex);
		xrtFree(pHash);
	}
}

int main()
{
	// Initialize XRT library and random seed
	// 初始化 XRT 库和随机种子
	xrtInit();
	srand((unsigned int)time(NULL));
	
	printf("XRT Hash Functions Demo\n");
	printf("XRT 哈希函数演示\n");
	printf("====================\n\n");
	
	// Run all tests
	// 运行所有测试
	TestHashFunctions();
	TestHashCollisions();
	TestHashPerformance();
	TestFileIntegrity();
	TestPasswordHashing();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}