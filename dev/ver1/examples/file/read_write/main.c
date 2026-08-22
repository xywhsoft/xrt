/*
 * XRT Example - File Read Write
 * XRT 范例 - 文件读写
 *
 * Description / 说明:
 *   EN: Demonstrates xrtOpen, xrtClose, xrtRead, xrtWrite for basic
 *       file I/O operations.
 *   CN: 演示 xrtOpen、xrtClose、xrtRead、xrtWrite 基本文件 I/O 操作。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/file_read_write.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/file_read_write              (Linux, GCC)
 *
 * Note / 注意:
 *   - xrtOpen returns xfile handle, must close with xrtClose
 *   - bReadOnly=TRUE for read, FALSE for write
 *   - iCharset specifies encoding (0=UTF-8)
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test file writing
// 测试文件写入
void TestFileWrite()
{
	printf("=== File Writing ===\n");
	printf("=== 文件写入 ===\n");
	
	// Create test file
	// 创建测试文件
	str sPath = "examples/bin/test_output.txt";
	
	// Open for writing
	// 打开以写入
	xfile f = xrtOpen(sPath, FALSE, 0);
	if ( f == NULL ) {
		printf("Failed to open file for writing.\n");
		printf("打开文件写入失败。\n");
		return;
	}
	
	// Write content
	// 写入内容
	str sContent = "Hello, XRT File I/O!\n这是中文测试。\nLine 3 here.\n";
	size_t iLen = strlen(sContent);
	size_t iWritten = xrtWrite(f, sContent, iLen);
	
	printf("Wrote %zu bytes to %s\n", iWritten, sPath);
	printf("写入 %zu 字节到 %s\n", iWritten, sPath);
	
	xrtClose(f);
	printf("\n");
}

// Test file reading
// 测试文件读取
void TestFileRead()
{
	printf("=== File Reading ===\n");
	printf("=== 文件读取 ===\n");
	
	str sPath = "examples/bin/test_output.txt";
	
	// Open for reading
	// 打开以读取
	xfile f = xrtOpen(sPath, TRUE, 0);
	if ( f == NULL ) {
		printf("Failed to open file for reading.\n");
		printf("打开文件读取失败。\n");
		return;
	}
	
	// Read content
	// 读取内容
	size_t iReadSize;
	str sContent = xrtRead(f, 1024, &iReadSize);
	
	if ( sContent ) {
		printf("Read %zu bytes:\n", iReadSize);
		printf("读取 %zu 字节:\n");
		printf("--- Content ---\n");
		printf("%s", sContent);
		printf("--- End ---\n");
		xrtFree(sContent);
	}
	
	xrtClose(f);
	printf("\n");
}

// Test read and write in sequence
// 测试顺序读写
void TestReadWriteSequence()
{
	printf("=== Read/Write Sequence ===\n");
	printf("=== 读写序列 ===\n");
	
	str sPath = "examples/bin/sequence_test.txt";
	
	// Write multiple lines
	// 写入多行
	xfile f = xrtOpen(sPath, FALSE, 0);
	if ( f ) {
		int i;
		for ( i = 1; i <= 5; i++ ) {
			str sLine = xrtFormat("Line %d: Data value %d\n", i, i * 100);
			xrtWrite(f, sLine, strlen(sLine));
			xrtFree(sLine);
		}
		xrtClose(f);
		printf("Wrote 5 lines.\n");
		printf("写入 5 行。\n");
	}
	
	// Read back
	// 读回
	f = xrtOpen(sPath, TRUE, 0);
	if ( f ) {
		size_t iSize;
		str sContent = xrtRead(f, 4096, &iSize);
		if ( sContent ) {
			printf("Content:\n%s", sContent);
			xrtFree(sContent);
		}
		xrtClose(f);
	}
	printf("\n");
}

// Test binary data
// 测试二进制数据
void TestBinaryData()
{
	printf("=== Binary Data ===\n");
	printf("=== 二进制数据 ===\n");
	
	str sPath = "examples/bin/binary_test.bin";
	
	// Write binary data
	// 写入二进制数据
	xfile f = xrtOpen(sPath, FALSE, 0);
	if ( f ) {
		// Write some binary data
		// 写入一些二进制数据
		unsigned char arrData[] = {0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD, 0x7F, 0x80};
		xrtWrite(f, (str)arrData, sizeof(arrData));
		xrtClose(f);
		printf("Wrote %zu bytes of binary data.\n", sizeof(arrData));
		printf("写入 %zu 字节二进制数据。\n", sizeof(arrData));
	}
	
	// Read back
	// 读回
	f = xrtOpen(sPath, TRUE, 0);
	if ( f ) {
		size_t iSize;
		str sData = xrtRead(f, 1024, &iSize);
		if ( sData ) {
			printf("Read %zu bytes: ", iSize);
			printf("读取 %zu 字节: ", iSize);
			size_t i;
			for ( i = 0; i < iSize; i++ ) {
				printf("%02X ", (unsigned char)sData[i]);
			}
			printf("\n");
			xrtFree(sData);
		}
		xrtClose(f);
	}
	printf("\n");
}

// Test practical use cases
// 测试实际用例
void TestPracticalUseCases()
{
	printf("=== Practical Use Cases ===\n");
	printf("=== 实际用例 ===\n");
	
	// Log file
	// 日志文件
	str sLogPath = "examples/bin/app.log";
	xfile f = xrtOpen(sLogPath, FALSE, 0);
	if ( f ) {
		xtime tNow = xrtNow();
		str sTimestamp = xrtTimeFormat(tNow, "yyyy-MM-dd HH:mm:ss");
		str sLogEntry = xrtFormat("[%s] Application started\n", sTimestamp);
		xrtWrite(f, sLogEntry, strlen(sLogEntry));
		xrtFree(sTimestamp);
		xrtFree(sLogEntry);
		xrtClose(f);
		printf("Log entry written to %s\n", sLogPath);
		printf("日志条目写入 %s\n", sLogPath);
	}
	
	// Config file
	// 配置文件
	str sConfigPath = "examples/bin/config.ini";
	f = xrtOpen(sConfigPath, FALSE, 0);
	if ( f ) {
		str sConfig = "[settings]\nname=MyApp\nversion=1.0\ndebug=true\n";
		xrtWrite(f, sConfig, strlen(sConfig));
		xrtClose(f);
		printf("Config file created.\n");
		printf("配置文件已创建。\n");
	}
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT File - Read Write Demo\n");
	printf("XRT 文件 - 读写演示\n");
	printf("==========================\n\n");
	
	TestFileWrite();
	TestFileRead();
	TestReadWriteSequence();
	TestBinaryData();
	TestPracticalUseCases();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
