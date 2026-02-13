/*
 * XRT Example - Initialization and Configuration
 * XRT 范例 - 初始化与配置
 *
 * Description / 说明:
 *   EN: Demonstrates xrtInit/xrtUnit lifecycle, global data access,
 *       reference counting, and library configuration.
 *   CN: 演示 xrtInit/xrtUnit 生命周期、全局数据访问、引用计数和库配置。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/base_init_and_config.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/base_init_and_config              (Linux, GCC)
 *
 * Note / 注意:
 *   - xrtInit returns pointer to xCore global data
 *   - Multiple calls to xrtInit use reference counting
 *   - xrtUnit only cleans up when reference count reaches 0
 *   - xCore.sNull is a safe null string pointer
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Custom error handler
// 自定义错误处理器
void MyErrorHandler(str sError)
{
	printf("[ERROR] %s\n", sError);
	printf("[错误] %s\n", sError);
}

// Test basic initialization
// 测试基本初始化
void TestBasicInit()
{
	printf("=== Basic Initialization ===\n");
	printf("=== 基本初始化 ===\n");
	
	// Check if library is initialized
	// 检查库是否已初始化
	if ( xCore.bInit ) {
		printf("Library is initialized.\n");
		printf("库已初始化。\n");
	} else {
		printf("Library is NOT initialized.\n");
		printf("库未初始化。\n");
	}
	printf("\n");
}

// Test global data access
// 测试全局数据访问
void TestGlobalData()
{
	printf("=== Global Data Access ===\n");
	printf("=== 全局数据访问 ===\n");
	
	// Access application file path
	// 访问应用程序文件路径
	printf("AppFile: %s\n", xCore.AppFile);
	printf("应用文件: %s\n", xCore.AppFile);
	
	// Access application directory
	// 访问应用程序目录
	printf("AppPath: %s\n", xCore.AppPath);
	printf("应用路径: %s\n", xCore.AppPath);
	
	// Access safe null string
	// 访问安全的空字符串
	printf("sNull: %p (safe null pointer)\n", (void*)xCore.sNull);
	printf("sNull: %p (安全空指针)\n", (void*)xCore.sNull);
	printf("\n");
}

// Test reference counting
// 测试引用计数
void TestRefCount()
{
	printf("=== Reference Counting ===\n");
	printf("=== 引用计数 ===\n");
	
	// Call xrtInit multiple times
	// 多次调用 xrtInit
	xrtGlobalData* pCore1 = xrtInit();
	xrtGlobalData* pCore2 = xrtInit();
	xrtGlobalData* pCore3 = xrtInit();
	
	// All calls return same pointer
	// 所有调用返回相同指针
	printf("Call 1: %p\n", (void*)pCore1);
	printf("Call 2: %p\n", (void*)pCore2);
	printf("Call 3: %p\n", (void*)pCore3);
	
	if ( pCore1 == pCore2 && pCore2 == pCore3 ) {
		printf("All pointers are identical (singleton pattern).\n");
		printf("所有指针相同 (单例模式)。\n");
	}
	printf("\n");
	
	// Must call xrtUnit same number of times
	// 必须调用 xrtUnit 相同次数
	xrtUnit();
	xrtUnit();
	xrtUnit();
}

// Test error handling
// 测试错误处理
void TestErrorHandling()
{
	printf("=== Error Handling ===\n");
	printf("=== 错误处理 ===\n");
	
	// Set custom error handler
	// 设置自定义错误处理器
	xCore.OnError = MyErrorHandler;
	printf("Custom error handler set.\n");
	printf("已设置自定义错误处理器。\n");
	
	// Clear any existing error
	// 清除任何现有错误
	xrtClearError();
	printf("LastError cleared.\n");
	printf("LastError 已清除。\n");
	
	// Trigger an error (will call handler)
	// 触发错误 (将调用处理器)
	xrtSetError("Test error message", FALSE);
	
	// Check last error
	// 检查最后错误
	printf("LastError: %s\n", xCore.LastError);
	printf("最后错误: %s\n", xCore.LastError);
	
	// Clear error
	// 清除错误
	xrtClearError();
	printf("Error cleared. LastError now: %s\n", xCore.LastError);
	printf("错误已清除。LastError 现在: %s\n", xCore.LastError);
	
	// Reset error handler
	// 重置错误处理器
	xCore.OnError = NULL;
	printf("\n");
}

// Test approx configuration
// 测试约等于配置
void TestApproxConfig()
{
	printf("=== Approx Configuration ===\n");
	printf("=== 约等于配置 ===\n");
	
	printf("Integer tolerance mode: %d (0=diff, 1=percent)\n", xCore.iApproxIntMode);
	printf("Integer tolerance: %.4f\n", xCore.fApproxIntTol);
	printf("Float tolerance mode: %d (0=diff, 1=percent)\n", xCore.iApproxNumMode);
	printf("Float tolerance: %.4f\n", xCore.fApproxNumTol);
	printf("Time tolerance (xtime units): %lld\n", xCore.iApproxTimeTol);
	printf("String approx mode: %d (0=wildcard, 1=similarity)\n", xCore.iApproxStrMode);
	printf("String similarity threshold: %.2f\n", xCore.fApproxStrTol);
	printf("Case insensitive: %d\n", xCore.bApproxStrCase);
	printf("\n");
	
	printf("整数容差模式: %d (0=差值, 1=百分比)\n", xCore.iApproxIntMode);
	printf("整数容差: %.4f\n", xCore.fApproxIntTol);
	printf("浮点容差模式: %d (0=差值, 1=百分比)\n", xCore.iApproxNumMode);
	printf("浮点容差: %.4f\n", xCore.fApproxNumTol);
	printf("时间容差 (xtime单位): %lld\n", xCore.iApproxTimeTol);
	printf("字符串近似模式: %d (0=通配符, 1=相似度)\n", xCore.iApproxStrMode);
	printf("字符串相似度阈值: %.2f\n", xCore.fApproxStrTol);
	printf("忽略大小写: %d\n", xCore.bApproxStrCase);
	printf("\n");
}

// Test memory functions
// 测试内存函数
void TestMemoryFunctions()
{
	printf("=== Memory Functions ===\n");
	printf("=== 内存函数 ===\n");
	
	// xCore holds standard memory functions
	// xCore 保存标准内存函数
	printf("malloc: %p\n", (void*)xCore.malloc);
	printf("calloc: %p\n", (void*)xCore.calloc);
	printf("realloc: %p\n", (void*)xCore.realloc);
	printf("free: %p\n", (void*)xCore.free);
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Base - Initialization and Configuration Demo\n");
	printf("XRT 基础 - 初始化与配置演示\n");
	printf("=================================================\n\n");
	
	// Run tests
	// 运行测试
	TestBasicInit();
	TestGlobalData();
	TestRefCount();
	TestErrorHandling();
	TestApproxConfig();
	TestMemoryFunctions();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	printf("Library cleanup complete.\n");
	printf("库清理完成。\n");
	return 0;
}
