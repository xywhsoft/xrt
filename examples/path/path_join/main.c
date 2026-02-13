/*
 * XRT Example - Path Join
 * XRT 范例 - 路径拼接
 *
 * Description / 说明:
 *   EN: Demonstrates xrtPathJoin for joining multiple path segments
 *       with proper separator handling.
 *   CN: 演示 xrtPathJoin 正确处理分隔符并拼接多个路径段。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/path_path_join.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/path_path_join              (Linux, GCC)
 *
 * Note / 注意:
 *   - Handles both forward and backslash separators
 *   - Eliminates duplicate separators
 *   - Returns newly allocated string, must free with xrtFree
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test basic path joining
// 测试基本路径拼接
void TestBasicJoin()
{
	printf("=== Basic Path Joining ===\n");
	printf("=== 基本路径拼接 ===\n");
	
	// Two components
	// 两个组件
	str sPath1 = xrtPathJoin(2, "home", "user");
	printf("Join(2, \"home\", \"user\") = \"%s\"\n", sPath1);
	xrtFree(sPath1);
	
	// Three components
	// 三个组件
	str sPath2 = xrtPathJoin(3, "home", "user", "documents");
	printf("Join(3, \"home\", \"user\", \"documents\") = \"%s\"\n", sPath2);
	xrtFree(sPath2);
	
	// Four components
	// 四个组件
	str sPath3 = xrtPathJoin(4, "var", "log", "app", "error.log");
	printf("Join(4, \"var\", \"log\", \"app\", \"error.log\") = \"%s\"\n", sPath3);
	xrtFree(sPath3);
	
	printf("\n");
}

// Test with existing separators
// 测试带现有分隔符
void TestWithSeparators()
{
	printf("=== Handling Existing Separators ===\n");
	printf("=== 处理现有分隔符 ===\n");
	
	// Components with trailing/leading separators
	// 带尾部/头部分隔符的组件
	str sPath1 = xrtPathJoin(2, "home/", "/user");
	printf("Join(\"home/\", \"/user\") = \"%s\"\n", sPath1);
	xrtFree(sPath1);
	
	// Multiple separators
	// 多个分隔符
	str sPath2 = xrtPathJoin(2, "home//", "user");
	printf("Join(\"home//\", \"user\") = \"%s\"\n", sPath2);
	xrtFree(sPath2);
	
	// Mixed separators
	// 混合分隔符
	str sPath3 = xrtPathJoin(3, "home\\user", "documents", "file.txt");
	printf("Join(\"home\\user\", \"documents\", \"file.txt\") = \"%s\"\n", sPath3);
	xrtFree(sPath3);
	
	printf("\n");
}

// Test absolute and relative paths
// 测试绝对和相对路径
void TestAbsRelPaths()
{
	printf("=== Absolute and Relative Paths ===\n");
	printf("=== 绝对和相对路径 ===\n");
	
	// Absolute Unix path
	// Unix 绝对路径
	str sPath1 = xrtPathJoin(3, "/", "home", "user");
	printf("Absolute Unix: \"%s\"\n", sPath1);
	printf("Unix 绝对路径: \"%s\"\n", sPath1);
	xrtFree(sPath1);
	
	// Relative path
	// 相对路径
	str sPath2 = xrtPathJoin(3, "..", "parent", "file.txt");
	printf("Relative: \"%s\"\n", sPath2);
	printf("相对路径: \"%s\"\n", sPath2);
	xrtFree(sPath2);
	
	// Current directory
	// 当前目录
	str sPath3 = xrtPathJoin(3, ".", "subdir", "file.txt");
	printf("Current dir: \"%s\"\n", sPath3);
	printf("当前目录: \"%s\"\n", sPath3);
	xrtFree(sPath3);
	
	printf("\n");
}

// Test Windows-style paths
// 测试 Windows 风格路径
void TestWindowsPaths()
{
	printf("=== Windows-Style Paths ===\n");
	printf("=== Windows 风格路径 ===\n");
	
	// Drive letter
	// 驱动器号
	str sPath1 = xrtPathJoin(3, "C:", "Windows", "System32");
	printf("Drive letter: \"%s\"\n", sPath1);
	printf("驱动器号: \"%s\"\n", sPath1);
	xrtFree(sPath1);
	
	// UNC path
	// UNC 路径
	str sPath2 = xrtPathJoin(3, "\\\\server", "share", "file.txt");
	printf("UNC path: \"%s\"\n", sPath2);
	printf("UNC 路径: \"%s\"\n", sPath2);
	xrtFree(sPath2);
	
	printf("\n");
}

// Test practical use cases
// 测试实际用例
void TestPracticalUseCases()
{
	printf("=== Practical Use Cases ===\n");
	printf("=== 实际用例 ===\n");
	
	// Build config file path
	// 构建配置文件路径
	str sConfigPath = xrtPathJoin(4, "etc", "myapp", "config", "settings.json");
	printf("Config file: %s\n", sConfigPath);
	printf("配置文件: %s\n", sConfigPath);
	xrtFree(sConfigPath);
	
	// Build log file path with date
	// 构建带日期的日志文件路径
	str sLogPath = xrtPathJoin(4, "var", "log", "myapp", "app-2024-12-25.log");
	printf("Log file: %s\n", sLogPath);
	printf("日志文件: %s\n", sLogPath);
	xrtFree(sLogPath);
	
	// Build cache path
	// 构建缓存路径
	str sCachePath = xrtPathJoin(5, "var", "cache", "myapp", "thumbnails", "img123.png");
	printf("Cache file: %s\n", sCachePath);
	printf("缓存文件: %s\n", sCachePath);
	xrtFree(sCachePath);
	
	// Build temp file path
	// 构建临时文件路径
	str sTempPath = xrtPathJoin(3, "tmp", "myapp", "temp_12345.dat");
	printf("Temp file: %s\n", sTempPath);
	printf("临时文件: %s\n", sTempPath);
	xrtFree(sTempPath);
	
	printf("\n");
}

// Test empty components
// 测试空组件
void TestEmptyComponents()
{
	printf("=== Empty Components ===\n");
	printf("=== 空组件 ===\n");
	
	// Empty string in middle
	// 中间的空字符串
	str sPath1 = xrtPathJoin(3, "home", "", "user");
	printf("Join(\"home\", \"\", \"user\") = \"%s\"\n", sPath1);
	xrtFree(sPath1);
	
	// Single empty string
	// 单个空字符串
	str sPath2 = xrtPathJoin(1, "single");
	printf("Join(1, \"single\") = \"%s\"\n", sPath2);
	xrtFree(sPath2);
	
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Path - Path Join Demo\n");
	printf("XRT 路径 - 路径拼接演示\n");
	printf("=========================\n\n");
	
	TestBasicJoin();
	TestWithSeparators();
	TestAbsRelPaths();
	TestWindowsPaths();
	TestPracticalUseCases();
	TestEmptyComponents();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
