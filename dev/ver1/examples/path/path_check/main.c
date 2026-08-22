/*
 * XRT Example - Path Check
 * XRT 范例 - 路径检查
 *
 * Description / 说明:
 *   EN: Demonstrates xrtPathIsAbs, xrtPathRandom, xrtPathExists for
 *       path validation and generation.
 *   CN: 演示 xrtPathIsAbs、xrtPathRandom、xrtPathExists 进行路径验证和生成。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/path_path_check.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/path_path_check              (Linux, GCC)
 *
 * Note / 注意:
 *   - xrtPathIsAbs checks if path is absolute
 *   - xrtPathRandom generates unique temporary path
 *   - xrtPathExists checks if file/directory exists
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test absolute path detection
// 测试绝对路径检测
void TestIsAbsolute()
{
	printf("=== Absolute Path Detection (xrtPathIsAbs) ===\n");
	printf("=== 绝对路径检测 (xrtPathIsAbs) ===\n");
	
	char* psPaths[] = {
		"/home/user/file.txt",
		"relative/path/file.txt",
		"C:\\Windows\\System32",
		"folder\\file.txt",
		"/",
		".",
		"../parent/file.txt",
		"\\\\server\\share\\file.txt",
	};
	
	char* psDesc[] = {
		"Unix absolute",
		"Unix relative",
		"Windows absolute",
		"Windows relative",
		"Root",
		"Current directory",
		"Parent directory",
		"UNC path",
	};
	
	int iNumPaths = 8;
	int i;
	for ( i = 0; i < iNumPaths; i++ ) {
		bool bIsAbs = xrtPathIsAbs(psPaths[i], 0);
		printf("  %-30s (%-20s): %s\n", 
		       psPaths[i], psDesc[i], 
		       bIsAbs ? "ABSOLUTE" : "relative");
	}
	printf("\n");
}

// Test random path generation
// 测试随机路径生成
void TestRandomPath()
{
	printf("=== Random Path Generation (xrtPathRandom) ===\n");
	printf("=== 随机路径生成 (xrtPathRandom) ===\n");
	
	// Generate temp paths with different formats
	// 生成不同格式的临时路径
	printf("Temp paths (prefix + random + suffix):\n");
	printf("临时路径 (前缀 + 随机 + 后缀):\n");
	
	int i;
	for ( i = 0; i < 5; i++ ) {
		str sPath = xrtPathRandom("temp_", 0, ".txt", 0, 8);
		printf("  %s\n", sPath);
		xrtFree(sPath);
	}
	printf("\n");
	
	// With head and foot
	// 带头和尾
	printf("With prefix and suffix:\n");
	printf("带前缀和后缀:\n");
	for ( i = 0; i < 5; i++ ) {
		str sPath = xrtPathRandom("/tmp/session_", 0, ".dat", 0, 12);
		printf("  %s\n", sPath);
		xrtFree(sPath);
	}
	printf("\n");
	
	// No head/foot
	// 无头/尾
	printf("Random only:\n");
	printf("仅随机部分:\n");
	for ( i = 0; i < 5; i++ ) {
		str sPath = xrtPathRandom("", 0, "", 0, 16);
		printf("  %s\n", sPath);
		xrtFree(sPath);
	}
	printf("\n");
}

// Test path existence
// 测试路径存在性
void TestPathExists()
{
	printf("=== Path Existence Check (xrtPathExists) ===\n");
	printf("=== 路径存在性检查 (xrtPathExists) ===\n");
	
	// Common paths that likely exist
	// 可能存在的常见路径
	char* psPaths[] = {
		".",
		"..",
		"/",
		"/tmp",
		"/etc",
		"C:\\Windows",
		"C:\\Program Files",
		"/nonexistent/path/that/does/not/exist",
	};
	
	char* psDesc[] = {
		"Current directory",
		"Parent directory",
		"Root",
		"/tmp",
		"/etc",
		"Windows folder",
		"Program Files",
		"Nonexistent path",
	};
	
	char* psDescCN[] = {
		"当前目录",
		"父目录",
		"根目录",
		"/tmp",
		"/etc",
		"Windows 文件夹",
		"Program Files",
		"不存在的路径",
	};
	
	int iNumPaths = 8;
	int i;
	for ( i = 0; i < iNumPaths; i++ ) {
		bool bExists = xrtPathExists(psPaths[i]);
		printf("  %-40s (%s): %s\n", 
		       psPaths[i], psDescCN[i],
		       bExists ? "EXISTS" : "not found");
	}
	printf("\n");
}

// Test practical use case - safe file creation
// 测试实际用例 - 安全文件创建
void TestSafeFileCreation()
{
	printf("=== Practical: Safe File Creation ===\n");
	printf("=== 实际: 安全文件创建 ===\n");
	
	// Generate unique filename until we find one that doesn't exist
	// 生成唯一文件名直到找到不存在的
	printf("Finding unique filename...\n");
	printf("查找唯一文件名...\n");
	
	int iAttempts = 0;
	str sPath;
	
	do {
		sPath = xrtPathRandom("test_file_", 0, ".tmp", 0, 6);
		iAttempts++;
		if ( iAttempts > 10 ) {
			printf("  Could not find unique name after 10 attempts\n");
			printf("  10 次尝试后仍无法找到唯一名称\n");
			break;
		}
	} while ( xrtPathExists(sPath) );
	
	if ( iAttempts <= 10 ) {
		printf("  Found unique path: %s (attempt %d)\n", sPath, iAttempts);
		printf("  找到唯一路径: %s (第 %d 次尝试)\n", sPath, iAttempts);
	}
	
	xrtFree(sPath);
	printf("\n");
}

// Test path validation workflow
// 测试路径验证工作流
void TestPathValidation()
{
	printf("=== Path Validation Workflow ===\n");
	printf("=== 路径验证工作流 ===\n");
	
	char* psPaths[] = {
		"/etc/passwd",
		"config/local.json",
		"C:\\boot.ini",
		"nonexistent.dat",
	};
	
	int iNumPaths = 4;
	int i;
	for ( i = 0; i < iNumPaths; i++ ) {
		printf("Path: %s\n", psPaths[i]);
		printf("路径: %s\n", psPaths[i]);
		
		// Check if absolute
		// 检查是否绝对
		bool bIsAbs = xrtPathIsAbs(psPaths[i], 0);
		printf("  Absolute: %s\n", bIsAbs ? "Yes" : "No");
		printf("  绝对路径: %s\n", bIsAbs ? "是" : "否");
		
		// Check if exists
		// 检查是否存在
		bool bExists = xrtPathExists(psPaths[i]);
		printf("  Exists: %s\n", bExists ? "Yes" : "No");
		printf("  存在: %s\n", bExists ? "是" : "否");
		
		printf("\n");
	}
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Path - Path Check Demo\n");
	printf("XRT 路径 - 路径检查演示\n");
	printf("==========================\n\n");
	
	TestIsAbsolute();
	TestRandomPath();
	TestPathExists();
	TestSafeFileCreation();
	TestPathValidation();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
