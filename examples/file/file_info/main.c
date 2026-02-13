/*
 * XRT Example - File Info
 * XRT 范例 - 文件信息
 *
 * Description / 说明:
 *   EN: Demonstrates xrtFileGetSize, xrtFileGetAttr, xrtPathExists for
 *       getting file metadata.
 *   CN: 演示 xrtFileGetSize、xrtFileGetAttr、xrtPathExists 获取文件元数据。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/file_file_info.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/file_file_info              (Linux, GCC)
 *
 * Note / 注意:
 *   - xrtFileGetSize returns size in bytes
 *   - xrtPathExists checks file/directory existence
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test file size
// 测试文件大小
void TestFileSize()
{
	printf("=== File Size ===\n");
	printf("=== 文件大小 ===\n");
	
	// Create test file first
	// 先创建测试文件
	str sPath = "examples/bin/size_test.txt";
	str sContent = "This is a test file with some content.\n这是带内容的测试文件。\n";
	xrtFileWriteAll(sPath, sContent, strlen(sContent), 0);
	
	// Get size
	// 获取大小
	size_t iSize = xrtFileGetSize(sPath);
	
	printf("File: %s\n", sPath);
	printf("Size: %zu bytes\n", iSize);
	printf("文件: %s\n", sPath);
	printf("大小: %zu 字节\n", iSize);
	printf("\n");
}

// Test path existence
// 测试路径存在性
void TestPathExists()
{
	printf("=== Path Exists ===\n");
	printf("=== 路径存在 ===\n");
	
	char* psPaths[] = {
		"examples/bin",
		"examples/bin/size_test.txt",
		"examples/bin/nonexistent.txt",
		".",
		"..",
	};
	
	char* psDesc[] = {
		"Directory (bin)",
		"Existing file",
		"Non-existent file",
		"Current directory",
		"Parent directory",
	};
	
	int iNumPaths = 5;
	int i;
	for ( i = 0; i < iNumPaths; i++ ) {
		bool bExists = xrtPathExists(psPaths[i]);
		printf("  %-30s: %s\n", psDesc[i], bExists ? "EXISTS" : "NOT FOUND");
	}
	printf("\n");
}

// Test multiple file sizes
// 测试多个文件大小
void TestMultipleSizes()
{
	printf("=== Multiple File Sizes ===\n");
	printf("=== 多文件大小 ===\n");
	
	// Create files of different sizes
	// 创建不同大小的文件
	struct {
		str sName;
		int iSize;
	} sFiles[] = {
		{"examples/bin/small.txt", 10},
		{"examples/bin/medium.txt", 1000},
		{"examples/bin/large.txt", 10000},
	};
	
	int iNumFiles = 3;
	int i;
	for ( i = 0; i < iNumFiles; i++ ) {
		// Create file with specified size
		// 创建指定大小的文件
		str sContent = (str)xrtMalloc(sFiles[i].iSize + 1);
		memset(sContent, 'X', sFiles[i].iSize);
		sContent[sFiles[i].iSize] = '\0';
		xrtFileWriteAll(sFiles[i].sName, sContent, sFiles[i].iSize, 0);
		xrtFree(sContent);
		
		// Get actual size
		// 获取实际大小
		size_t iSize = xrtFileGetSize(sFiles[i].sName);
		printf("  %s: %zu bytes\n", sFiles[i].sName, iSize);
	}
	printf("\n");
}

// Test practical use cases
// 测试实际用例
void TestPracticalUseCases()
{
	printf("=== Practical Use Cases ===\n");
	printf("=== 实际用例 ===\n");
	
	// Check if config file exists before reading
	// 读取前检查配置文件是否存在
	str sConfigPath = "examples/bin/config.json";
	
	if ( xrtPathExists(sConfigPath) ) {
		size_t iSize = xrtFileGetSize(sConfigPath);
		printf("Config exists: %s (%zu bytes)\n", sConfigPath, iSize);
		printf("配置存在: %s (%zu 字节)\n", sConfigPath, iSize);
		
		str sContent = xrtFileReadAll(sConfigPath, 0, NULL);
		if ( sContent ) {
			printf("Content: %s\n", sContent);
			xrtFree(sContent);
		}
	} else {
		printf("Config file not found, using defaults.\n");
		printf("未找到配置文件，使用默认值。\n");
		
		// Create default config
		// 创建默认配置
		str sDefault = "{\"debug\": false, \"port\": 8080}\n";
		xrtFileWriteAll(sConfigPath, sDefault, strlen(sDefault), 0);
		printf("Created default config.\n");
		printf("已创建默认配置。\n");
	}
	printf("\n");
	
	// Log rotation check
	// 日志轮换检查
	str sLogPath = "examples/bin/app.log";
	if ( xrtPathExists(sLogPath) ) {
		size_t iLogSize = xrtFileGetSize(sLogPath);
		printf("Log file size: %zu bytes\n", iLogSize);
		printf("日志文件大小: %zu 字节\n", iLogSize);
		
		if ( iLogSize > 1024 * 1024 ) {  // > 1MB
			printf("Log file exceeds 1MB, consider rotation.\n");
			printf("日志文件超过 1MB，考虑轮换。\n");
		} else {
			printf("Log size OK.\n");
			printf("日志大小正常。\n");
		}
	}
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT File - File Info Demo\n");
	printf("XRT 文件 - 文件信息演示\n");
	printf("=========================\n\n");
	
	TestFileSize();
	TestPathExists();
	TestMultipleSizes();
	TestPracticalUseCases();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
