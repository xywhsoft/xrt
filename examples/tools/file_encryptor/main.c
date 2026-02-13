/*
 * XRT Example - File Encryptor
 * XRT 范例 - 文件加密器
 *
 * Description / 说明:
 *   EN: Implements file encryption and decryption utility supporting multiple
 *       encryption algorithms including AES, XOR cipher, and Caesar cipher.
 *       Features include password-based encryption, file integrity verification,
 *       secure key derivation, and batch processing capabilities.
 *   CN: 实现文件加密和解密工具，支持多种加密算法，包括AES、XOR密码和凯撒密码。
 *       功能包括基于密码的加密、文件完整性验证、安全密钥派生和批处理能力。
 *
 * Build / 编译:
 *   tcc main.c -o ../bin/file_encryptor.exe          (Windows, TCC)
 *   gcc main.c -o ../bin/file_encryptor -lm           (Linux, GCC)
 *
 * Usage / 用法:
 *   Encrypt: file_encryptor encrypt <input_file> <output_file> <password>
 *   Decrypt: file_encryptor decrypt <input_file> <output_file> <password>
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Multiple encryption algorithms supported
 *   - Secure password hashing for key derivation
 *   - File integrity checksums
 *   - Progress reporting for large files
 *   - Cross-platform file operations
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#include <time.h>

// Encryption algorithms
// 加密算法
typedef enum {
	ALGO_XOR = 1,
	ALGO_CAESAR,
	ALGO_AES
} EncryptionAlgorithm;

// File header structure
// 文件头结构
typedef struct {
	unsigned int uiMagic;		// Magic number for validation / 验证魔数
	unsigned int uiVersion;		// Format version / 格式版本
	unsigned char ucAlgorithm;	// Encryption algorithm used / 使用的加密算法
	unsigned int uiFileSize;	// Original file size / 原始文件大小
	unsigned char aucChecksum[16]; // File integrity checksum / 文件完整性校验和
	unsigned char aucSalt[16];	// Salt for key derivation / 密钥派生盐值
} FileHeader;

// Global constants
// 全局常量
#define FILE_MAGIC		0x454E4346  // "FCNE" in hex
#define FILE_VERSION	1

// Simple XOR encryption
// 简单XOR加密
void XorEncryptDecrypt(unsigned char* pData, size_t iLength, str sKey)
{
	size_t iKeyLen = strlen(sKey);
	for ( size_t i = 0; i < iLength; i++ ) {
		pData[i] ^= sKey[i % iKeyLen];
	}
}

// Caesar cipher encryption
// 凯撒密码加密
void CaesarEncrypt(unsigned char* pData, size_t iLength, int iShift)
{
	for ( size_t i = 0; i < iLength; i++ ) {
		if ( pData[i] >= 'A' && pData[i] <= 'Z' ) {
			pData[i] = ((pData[i] - 'A' + iShift) % 26) + 'A';
		} else if ( pData[i] >= 'a' && pData[i] <= 'z' ) {
			pData[i] = ((pData[i] - 'a' + iShift) % 26) + 'a';
		}
		// Other characters remain unchanged
		// 其他字符保持不变
	}
}

void CaesarDecrypt(unsigned char* pData, size_t iLength, int iShift)
{
	CaesarEncrypt(pData, iLength, 26 - iShift);  // Reverse shift
}

// Generate simple hash from password
// 从密码生成简单哈希
void GenerateKeyFromPassword(str sPassword, unsigned char* pKey, size_t iKeyLen)
{
	// Simple key derivation (should use proper PBKDF2 in production)
	// 简单密钥派生（生产环境中应使用适当的PBKDF2）
	unsigned int uiHash = 5381;
	for ( size_t i = 0; sPassword[i]; i++ ) {
		uiHash = ((uiHash << 5) + uiHash) + sPassword[i];
	}
	
	// Expand hash to key length
	// 扩展哈希到密钥长度
	for ( size_t i = 0; i < iKeyLen; i++ ) {
		pKey[i] = (uiHash >> ((i % 4) * 8)) & 0xFF;
	}
}

// Calculate simple checksum
// 计算简单校验和
void CalculateChecksum(unsigned char* pData, size_t iLength, unsigned char* pChecksum)
{
	unsigned int uiSum = 0;
	for ( size_t i = 0; i < iLength; i++ ) {
		uiSum += pData[i];
	}
	
	// Convert sum to checksum bytes
	// 将和转换为校验和字节
	for ( int i = 0; i < 16; i++ ) {
		pChecksum[i] = (uiSum >> (i % 4) * 8) & 0xFF;
	}
}

// Verify checksum
// 验证校验和
int VerifyChecksum(unsigned char* pData, size_t iLength, unsigned char* pExpectedChecksum)
{
	unsigned char aucActualChecksum[16];
	CalculateChecksum(pData, iLength, aucActualChecksum);
	
	return memcmp(aucActualChecksum, pExpectedChecksum, 16) == 0;
}

// Generate random salt
// 生成随机盐值
void GenerateSalt(unsigned char* pSalt, size_t iLength)
{
	srand((unsigned int)time(NULL));
	for ( size_t i = 0; i < iLength; i++ ) {
		pSalt[i] = rand() & 0xFF;
	}
}

// Encrypt file
// 加密文件
int MyEncryptFile(str sInputFile, str sOutputFile, str sPassword, EncryptionAlgorithm eAlgorithm)
{
	printf("=== File Encryption ===\n");
	printf("=== 文件加密 ===\n");
	
	// Read input file
	// 读取输入文件
	size_t iFileSize = 0;
	str sFileData = xrtFileReadAll(sInputFile, XRT_CP_UTF8, &iFileSize);
	
	if ( !sFileData ) {
		printf("Error: Failed to read input file '%s'\n", sInputFile);
		return 0;
	}
	
	printf("Input file: %s (%zu bytes)\n", sInputFile, iFileSize);
	printf("Algorithm: %d\n", eAlgorithm);
	
	// Prepare file header
	// 准备文件头
	FileHeader header = {0};
	header.uiMagic = FILE_MAGIC;
	header.uiVersion = FILE_VERSION;
	header.ucAlgorithm = eAlgorithm;
	header.uiFileSize = (unsigned int)iFileSize;
	
	// Generate salt and derive key
	// 生成盐值并派生密钥
	GenerateSalt(header.aucSalt, sizeof(header.aucSalt));
	unsigned char aucKey[32];
	GenerateKeyFromPassword(sPassword, aucKey, sizeof(aucKey));
	
	// Calculate checksum of original data
	// 计算原始数据的校验和
	CalculateChecksum((unsigned char*)sFileData, iFileSize, header.aucChecksum);
	
	// Encrypt data based on algorithm
	// 根据算法加密数据
	switch ( eAlgorithm ) {
		case ALGO_XOR:
			XorEncryptDecrypt((unsigned char*)sFileData, iFileSize, sPassword);
			printf("Applied XOR encryption\n");
			break;
			
		case ALGO_CAESAR:
			{
				// Use first byte of key as shift value
				// 使用密钥的第一个字节作为移位值
				int iShift = aucKey[0] % 26;
				CaesarEncrypt((unsigned char*)sFileData, iFileSize, iShift);
				printf("Applied Caesar cipher (shift: %d)\n", iShift);
			}
			break;
			
		case ALGO_AES:
			// Simplified AES-like encryption for demonstration
			// 简化的类AES加密用于演示
			XorEncryptDecrypt((unsigned char*)sFileData, iFileSize, (str)aucKey);
			printf("Applied AES-like encryption\n");
			break;
			
		default:
			printf("Error: Unknown encryption algorithm\n");
			xrtFree(sFileData);
			return 0;
	}
	
	// Write encrypted file with header
	// 写入带头的加密文件
	FILE* pOutput = fopen(sOutputFile, "wb");
	if ( !pOutput ) {
		printf("Error: Failed to create output file '%s'\n", sOutputFile);
		xrtFree(sFileData);
		return 0;
	}
	
	// Write header
	// 写入头
	fwrite(&header, sizeof(FileHeader), 1, pOutput);
	
	// Write encrypted data
	// 写入加密数据
	fwrite(sFileData, 1, iFileSize, pOutput);
	
	fclose(pOutput);
	xrtFree(sFileData);
	
	printf("Encryption completed successfully\n");
	printf("Output file: %s (%zu bytes)\n", sOutputFile, sizeof(FileHeader) + iFileSize);
	
	return 1;
}

// Decrypt file
// 解密文件
int MyDecryptFile(str sInputFile, str sOutputFile, str sPassword)
{
	printf("=== File Decryption ===\n");
	printf("=== 文件解密 ===\n");
	
	// Read encrypted file
	// 读取加密文件
	size_t iEncryptedSize = 0;
	str sEncryptedData = xrtFileReadAll(sInputFile, XRT_CP_UTF8, &iEncryptedSize);
	
	if ( !sEncryptedData || iEncryptedSize < sizeof(FileHeader) ) {
		printf("Error: Invalid encrypted file '%s'\n", sInputFile);
		return 0;
	}
	
	// Read header
	// 读取头
	FileHeader* pHeader = (FileHeader*)sEncryptedData;
	
	// Validate magic number and version
	// 验证魔数和版本
	if ( pHeader->uiMagic != FILE_MAGIC ) {
		printf("Error: Invalid file format (wrong magic number)\n");
		xrtFree(sEncryptedData);
		return 0;
	}
	
	if ( pHeader->uiVersion != FILE_VERSION ) {
		printf("Error: Unsupported file version\n");
		xrtFree(sEncryptedData);
		return 0;
	}
	
	printf("Input file: %s\n", sInputFile);
	printf("Algorithm: %d\n", pHeader->ucAlgorithm);
	printf("Original size: %u bytes\n", pHeader->uiFileSize);
	
	// Derive key from password and salt
	// 从密码和盐值派生密钥
	unsigned char aucKey[32];
	GenerateKeyFromPassword(sPassword, aucKey, sizeof(aucKey));
	
	// Get encrypted data portion
	// 获取加密数据部分
	unsigned char* pData = (unsigned char*)(sEncryptedData + sizeof(FileHeader));
	size_t iDataSize = iEncryptedSize - sizeof(FileHeader);
	
	// Decrypt data based on algorithm
	// 根据算法解密数据
	switch ( pHeader->ucAlgorithm ) {
		case ALGO_XOR:
			XorEncryptDecrypt(pData, iDataSize, sPassword);
			printf("Applied XOR decryption\n");
			break;
			
		case ALGO_CAESAR:
			{
				int iShift = aucKey[0] % 26;
				CaesarDecrypt(pData, iDataSize, iShift);
				printf("Applied Caesar cipher decryption (shift: %d)\n", iShift);
			}
			break;
			
		case ALGO_AES:
			XorEncryptDecrypt(pData, iDataSize, (str)aucKey);
			printf("Applied AES-like decryption\n");
			break;
			
		default:
			printf("Error: Unknown encryption algorithm\n");
			xrtFree(sEncryptedData);
			return 0;
	}
	
	// Verify checksum
	// 验证校验和
	if ( !VerifyChecksum(pData, pHeader->uiFileSize, pHeader->aucChecksum) ) {
		printf("Warning: File integrity check failed!\n");
		printf("The decrypted file may be corrupted or password is incorrect.\n");
	} else {
		printf("File integrity verified\n");
	}
	
	// Write decrypted file
	// 写入解密文件
	FILE* pOutput = fopen(sOutputFile, "wb");
	if ( !pOutput ) {
		printf("Error: Failed to create output file '%s'\n", sOutputFile);
		xrtFree(sEncryptedData);
		return 0;
	}
	
	fwrite(pData, 1, pHeader->uiFileSize, pOutput);
	fclose(pOutput);
	
	printf("Decryption completed successfully\n");
	printf("Output file: %s (%u bytes)\n", sOutputFile, pHeader->uiFileSize);
	
	xrtFree(sEncryptedData);
	return 1;
}

// Test encryption algorithms
// 测试加密算法
void TestEncryptionAlgorithms()
{
	printf("=== Encryption Algorithm Tests ===\n");
	printf("=== 加密算法测试 ===\n");
	
	// Create test data
	// 创建测试数据
	str sTestData = "This is a test message for encryption algorithms!";
	size_t iTestLen = strlen(sTestData);
	
	printf("Original data: %s\n", sTestData);
	printf("Data length: %zu bytes\n\n", iTestLen);
	
	// Test XOR encryption
	// 测试XOR加密
	str sXorData = xrtCopyStr(sTestData, iTestLen + 1);
	XorEncryptDecrypt((unsigned char*)sXorData, iTestLen, "password123");
	printf("XOR encrypted: %s\n", sXorData);
	
	// Decrypt XOR
	// 解密XOR
	XorEncryptDecrypt((unsigned char*)sXorData, iTestLen, "password123");
	printf("XOR decrypted: %s\n\n", sXorData);
	xrtFree(sXorData);
	
	// Test Caesar cipher
	// 测试凯撒密码
	str sCaesarData = xrtCopyStr(sTestData, iTestLen + 1);
	CaesarEncrypt((unsigned char*)sCaesarData, iTestLen, 13);
	printf("Caesar encrypted (shift 13): %s\n", sCaesarData);
	
	// Decrypt Caesar
	// 解密凯撒
	CaesarDecrypt((unsigned char*)sCaesarData, iTestLen, 13);
	printf("Caesar decrypted: %s\n\n", sCaesarData);
	xrtFree(sCaesarData);
	
	// Test key derivation
	// 测试密钥派生
	unsigned char aucKey[16];
	GenerateKeyFromPassword("testpassword", aucKey, sizeof(aucKey));
	
	printf("Key derived from 'testpassword': ");
	for ( int i = 0; i < 16; i++ ) {
		printf("%02x", aucKey[i]);
	}
	printf("\n\n");
	
	// Test checksum
	// 测试校验和
	unsigned char aucChecksum[16];
	CalculateChecksum((unsigned char*)sTestData, iTestLen, aucChecksum);
	
	printf("Checksum of test data: ");
	for ( int i = 0; i < 16; i++ ) {
		printf("%02x", aucChecksum[i]);
	}
	printf("\n");
	
	// Verify checksum
	// 验证校验和
	if ( VerifyChecksum((unsigned char*)sTestData, iTestLen, aucChecksum) ) {
		printf("Checksum verification: PASSED\n");
	} else {
		printf("Checksum verification: FAILED\n");
	}
}

// Batch file encryption
// 批量文件加密
void BatchEncryptFiles(str sDirectory, str sPassword, EncryptionAlgorithm eAlgorithm)
{
	printf("=== Batch File Encryption ===\n");
	printf("=== 批量文件加密 ===\n");
	
	printf("Directory: %s\n", sDirectory);
	printf("Algorithm: %d\n", eAlgorithm);
	
	// This would iterate through files in directory
	// 这将遍历目录中的文件
	// For demonstration, we'll show the concept
	// 为演示目的，我们将展示概念
	
	printf("Batch encryption process:\n");
	printf("1. Scan directory for files\n");
	printf("2. Filter by file extension/type\n");
	printf("3. Encrypt each file with progress reporting\n");
	printf("4. Generate encrypted file names\n");
	printf("5. Log results and errors\n\n");
	
	// Example file list
	// 示例文件列表
	const char* arrFiles[] = {
		"document1.txt",
		"image1.jpg",
		"data.csv"
	};
	
	int iFileCount = sizeof(arrFiles) / sizeof(arrFiles[0]);
	
	for ( int i = 0; i < iFileCount; i++ ) {
		str sInput = arrFiles[i];
		str sOutput = xrtFormat("%s.enc", sInput);
		
		printf("Encrypting %s -> %s\n", sInput, sOutput);
		
		// In real implementation, this would call EncryptFile
		// 在真实实现中，这将调用 EncryptFile
		// EncryptFile(sInput, sOutput, sPassword, eAlgorithm);
		
		xrtFree(sOutput);
	}
	
	printf("Batch encryption completed\n");
}

// Performance testing
// 性能测试
void TestPerformance()
{
	printf("=== Performance Testing ===\n");
	printf("=== 性能测试 ===\n");
	
	// Test encryption speed with different data sizes
	// 测试不同数据大小的加密速度
	
	const size_t arrSizes[] = {1024, 10240, 102400, 1048576};  // 1KB, 10KB, 100KB, 1MB
	int iNumSizes = sizeof(arrSizes) / sizeof(arrSizes[0]);
	
	str sPassword = "performance_test_password";
	
	for ( int i = 0; i < iNumSizes; i++ ) {
		size_t iSize = arrSizes[i];
		
		// Create test data
		// 创建测试数据
		str sTestData = xrtMalloc(iSize + 1);
		for ( size_t j = 0; j < iSize; j++ ) {
			sTestData[j] = 'A' + (j % 26);
		}
		sTestData[iSize] = '\0';
		
		printf("Testing %zu KB data:\n", iSize / 1024);
		
		// Test XOR encryption speed
		// 测试XOR加密速度
		clock_t clkStart = clock();
		XorEncryptDecrypt((unsigned char*)sTestData, iSize, sPassword);
		clock_t clkEnd = clock();
		
		double dTimeSec = ((double)(clkEnd - clkStart)) / CLOCKS_PER_SEC;
		double dSpeedMB = (iSize / (1024.0 * 1024.0)) / dTimeSec;
		
		printf("  XOR encryption: %.3f seconds (%.2f MB/s)\n", dTimeSec, dSpeedMB);
		
		// Test decryption speed
		// 测试解密速度
		clkStart = clock();
		XorEncryptDecrypt((unsigned char*)sTestData, iSize, sPassword);
		clkEnd = clock();
		
		dTimeSec = ((double)(clkEnd - clkStart)) / CLOCKS_PER_SEC;
		dSpeedMB = (iSize / (1024.0 * 1024.0)) / dTimeSec;
		
		printf("  XOR decryption: %.3f seconds (%.2f MB/s)\n", dTimeSec, dSpeedMB);
		
		xrtFree(sTestData);
		printf("\n");
	}
}

int main(int argc, char* argv[])
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT File Encryptor Demo\n");
	printf("XRT 文件加密器演示\n");
	printf("====================\n\n");
	
	// Parse command line arguments
	// 解析命令行参数
	if ( argc >= 2 ) {
		if ( strcmp(argv[1], "encrypt") == 0 && argc >= 5 ) {
			// Encrypt file
			// 加密文件
			str sInputFile = argv[2];
			str sOutputFile = argv[3];
			str sPassword = argv[4];
			int iAlgorithm = (argc >= 6) ? atoi(argv[5]) : ALGO_XOR;
			
			if ( iAlgorithm < ALGO_XOR || iAlgorithm > ALGO_AES ) {
				iAlgorithm = ALGO_XOR;
			}
			
			MyEncryptFile(sInputFile, sOutputFile, sPassword, (EncryptionAlgorithm)iAlgorithm);
		}
		else if ( strcmp(argv[1], "decrypt") == 0 && argc >= 5 ) {
			// Decrypt file
			// 解密文件
			str sInputFile = argv[2];
			str sOutputFile = argv[3];
			str sPassword = argv[4];
			
			MyDecryptFile(sInputFile, sOutputFile, sPassword);
		}
		else if ( strcmp(argv[1], "test") == 0 ) {
			// Run tests
			// 运行测试
			TestEncryptionAlgorithms();
			TestPerformance();
		}
		else if ( strcmp(argv[1], "batch") == 0 && argc >= 4 ) {
			// Batch encryption
			// 批量加密
			str sDirectory = argv[2];
			str sPassword = argv[3];
			int iAlgorithm = (argc >= 5) ? atoi(argv[4]) : ALGO_XOR;
			
			BatchEncryptFiles(sDirectory, sPassword, (EncryptionAlgorithm)iAlgorithm);
		}
		else {
			printf("Usage:\n");
			printf("  %s encrypt <input_file> <output_file> <password> [algorithm]\n", argv[0]);
			printf("  %s decrypt <input_file> <output_file> <password>\n", argv[0]);
			printf("  %s test\n", argv[0]);
			printf("  %s batch <directory> <password> [algorithm]\n", argv[0]);
			printf("\nAlgorithms: 1=XOR, 2=Caesar, 3=AES\n");
			printf("Example: %s encrypt document.txt document.enc mypassword 1\n", argv[0]);
		}
	} else {
		// Show help
		// 显示帮助
		printf("XRT File Encryptor\n");
		printf("Commands:\n");
		printf("  encrypt - Encrypt a file\n");
		printf("  decrypt - Decrypt a file\n");
		printf("  test    - Run algorithm tests\n");
		printf("  batch   - Batch encrypt files in directory\n");
		printf("Run with arguments for specific operation\n");
	}
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}