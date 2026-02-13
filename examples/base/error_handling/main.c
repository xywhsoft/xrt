/*
 * XRT Example - Error Handling
 * XRT 范例 - 错误处理
 *
 * Description / 说明:
 *   EN: Demonstrates xrtSetError, xrtClearError, and custom OnError
 *       callback for centralized error management.
 *   CN: 演示 xrtSetError、xrtClearError 和自定义 OnError 回调实现集中式错误管理。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/base_error_handling.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/base_error_handling              (Linux, GCC)
 *
 * Note / 注意:
 *   - Error system is NOT thread-safe
 *   - OnError callback is called when xrtSetError is invoked
 *   - xrtClearError frees dynamically allocated error messages
 *   - Set bFree=TRUE when passing dynamically allocated strings
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Global error counter for demonstration
// 用于演示的全局错误计数器
int giErrorCount = 0;

// Custom error handler - logs all errors
// 自定义错误处理器 - 记录所有错误
void LogErrorHandler(str sError)
{
	giErrorCount++;
	printf("[Error #%d] %s\n", giErrorCount, sError);
	printf("[错误 #%d] %s\n", giErrorCount, sError);
}

// Custom error handler with prefix
// 带前缀的自定义错误处理器
void PrefixedErrorHandler(str sError)
{
	printf("!!! CRITICAL: %s !!!\n", sError);
	printf("!!! 严重: %s !!!\n", sError);
}

// Function that simulates an operation that might fail
// 模拟可能失败操作的函数
int Divide(int iA, int iB, int* piResult)
{
	if ( iB == 0 ) {
		xrtSetError("Division by zero", FALSE);
		return FALSE;
	}
	
	*piResult = iA / iB;
	return TRUE;
}

// Function that allocates and sets error
// 分配并设置错误的函数
void CheckArrayBounds(int iIndex, int iMax)
{
	if ( iIndex < 0 || iIndex >= iMax ) {
		// Create dynamic error message
		// 创建动态错误消息
		char* psMsg = (char*)xrtMalloc(128);
		if ( psMsg ) {
			sprintf(psMsg, "Index %d out of bounds [0, %d)", iIndex, iMax);
			// TRUE means xrtClearError will free this string
			// TRUE 表示 xrtClearError 将释放此字符串
			xrtSetError(psMsg, TRUE);
		}
	} else {
		printf("Index %d is valid.\n", iIndex);
		printf("索引 %d 有效。\n", iIndex);
	}
}

// Test basic error setting and getting
// 测试基本错误设置和获取
void TestBasicError()
{
	printf("=== Basic Error Setting ===\n");
	printf("=== 基本错误设置 ===\n");
	
	// Set error handler
	// 设置错误处理器
	xCore.OnError = LogErrorHandler;
	giErrorCount = 0;
	
	// Clear any existing error
	// 清除任何现有错误
	xrtClearError();
	printf("LastError after clear: \"%s\"\n", xCore.LastError);
	printf("清除后 LastError: \"%s\"\n", xCore.LastError);
	printf("\n");
	
	// Set a static error string
	// 设置静态错误字符串
	xrtSetError("Something went wrong", FALSE);
	printf("LastError is now: \"%s\"\n", xCore.LastError);
	printf("LastError 现在: \"%s\"\n", xCore.LastError);
	printf("\n");
	
	// Set another error (overwrites previous)
	// 设置另一个错误 (覆盖前一个)
	xrtSetError("Another error occurred", FALSE);
	printf("LastError is now: \"%s\"\n", xCore.LastError);
	printf("LastError 现在: \"%s\"\n", xCore.LastError);
	printf("\n");
	
	xrtClearError();
}

// Test error in operations
// 测试操作中的错误
void TestOperationError()
{
	printf("=== Error in Operations ===\n");
	printf("=== 操作中的错误 ===\n");
	
	xCore.OnError = LogErrorHandler;
	xrtClearError();
	
	int iResult;
	
	// Successful operation
	// 成功操作
	if ( Divide(10, 2, &iResult) ) {
		printf("10 / 2 = %d\n", iResult);
		printf("LastError: \"%s\"\n", xCore.LastError);
		printf("\n");
	}
	
	// Failing operation
	// 失败操作
	if ( !Divide(10, 0, &iResult) ) {
		printf("Division failed as expected.\n");
		printf("除法按预期失败。\n");
		printf("LastError: \"%s\"\n", xCore.LastError);
		printf("LastError: \"%s\"\n", xCore.LastError);
		printf("\n");
	}
	
	xrtClearError();
}

// Test dynamic error messages
// 测试动态错误消息
void TestDynamicError()
{
	printf("=== Dynamic Error Messages ===\n");
	printf("=== 动态错误消息 ===\n");
	
	xCore.OnError = LogErrorHandler;
	xrtClearError();
	
	// Valid index - no error
	// 有效索引 - 无错误
	CheckArrayBounds(5, 10);
	printf("LastError: \"%s\"\n", xCore.LastError);
	printf("\n");
	
	// Invalid index - triggers error with dynamic message
	// 无效索引 - 触发带动态消息的错误
	CheckArrayBounds(15, 10);
	printf("LastError: \"%s\"\n", xCore.LastError);
	printf("\n");
	
	// Clear error (frees dynamic message)
	// 清除错误 (释放动态消息)
	xrtClearError();
	printf("Error cleared. LastError: \"%s\"\n", xCore.LastError);
	printf("错误已清除。LastError: \"%s\"\n\n", xCore.LastError);
}

// Test switching error handlers
// 测试切换错误处理器
void TestErrorHandlerSwitch()
{
	printf("=== Switching Error Handlers ===\n");
	printf("=== 切换错误处理器 ===\n");
	
	// No handler - silent
	// 无处理器 - 静默
	xCore.OnError = NULL;
	printf("No handler set:\n");
	printf("未设置处理器:\n");
	xrtSetError("Silent error 1", FALSE);
	printf("LastError: \"%s\"\n\n", xCore.LastError);
	
	// First handler
	// 第一个处理器
	xCore.OnError = LogErrorHandler;
	printf("LogErrorHandler set:\n");
	printf("已设置 LogErrorHandler:\n");
	xrtSetError("Logged error 1", FALSE);
	printf("\n");
	
	// Second handler
	// 第二个处理器
	xCore.OnError = PrefixedErrorHandler;
	printf("PrefixedErrorHandler set:\n");
	printf("已设置 PrefixedErrorHandler:\n");
	xrtSetError("Critical error!", FALSE);
	printf("\n");
	
	xrtClearError();
	xCore.OnError = NULL;
}

// Test error callback behavior
// 测试错误回调行为
void TestCallbackBehavior()
{
	printf("=== Callback Behavior Test ===\n");
	printf("=== 回调行为测试 ===\n");
	
	int iLocalCount = 0;
	
	// Simple inline handler using a global-like approach
	// 使用类全局方法的简单内联处理器
	xCore.OnError = LogErrorHandler;
	
	printf("Setting multiple errors:\n");
	printf("设置多个错误:\n");
	
	xrtSetError("First callback test", FALSE);
	xrtSetError("Second callback test", FALSE);
	xrtSetError("Third callback test", FALSE);
	
	printf("\nTotal errors logged: %d\n", giErrorCount);
	printf("记录的错误总数: %d\n\n", giErrorCount);
	
	xrtClearError();
	giErrorCount = 0;
	xCore.OnError = NULL;
}

// Demonstrate error propagation pattern
// 演示错误传播模式
int DoStep1()
{
	// Simulate success
	// 模拟成功
	printf("Step 1 completed.\n");
	printf("步骤 1 完成。\n");
	return TRUE;
}

int DoStep2()
{
	// Simulate failure
	// 模拟失败
	xrtSetError("Step 2 failed: resource unavailable", FALSE);
	printf("Step 2 failed.\n");
	printf("步骤 2 失败。\n");
	return FALSE;
}

int DoStep3()
{
	printf("Step 3 completed.\n");
	printf("步骤 3 完成。\n");
	return TRUE;
}

void TestErrorPropagation()
{
	printf("=== Error Propagation Pattern ===\n");
	printf("=== 错误传播模式 ===\n");
	
	xCore.OnError = LogErrorHandler;
	xrtClearError();
	
	printf("Executing multi-step operation:\n");
	printf("执行多步操作:\n");
	
	if ( !DoStep1() ) {
		printf("Aborted at step 1.\n");
		printf("在步骤 1 中止。\n");
		goto cleanup;
	}
	
	if ( !DoStep2() ) {
		printf("Aborted at step 2.\n");
		printf("在步骤 2 中止。\n");
		printf("Reason: %s\n", xCore.LastError);
		printf("原因: %s\n", xCore.LastError);
		goto cleanup;
	}
	
	if ( !DoStep3() ) {
		printf("Aborted at step 3.\n");
		printf("在步骤 3 中止。\n");
		goto cleanup;
	}
	
	printf("All steps completed successfully!\n");
	printf("所有步骤成功完成！\n");
	
cleanup:
	xrtClearError();
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Base - Error Handling Demo\n");
	printf("XRT 基础 - 错误处理演示\n");
	printf("===============================\n\n");
	
	TestBasicError();
	TestOperationError();
	TestDynamicError();
	TestErrorHandlerSwitch();
	TestCallbackBehavior();
	TestErrorPropagation();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	printf("Error handling demo complete.\n");
	printf("错误处理演示完成。\n");
	return 0;
}
