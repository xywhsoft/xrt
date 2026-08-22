/*
 * XRT Example - Timer and Sleep
 * XRT 范例 - 计时器和休眠
 *
 * Description / 说明:
 *   EN: Demonstrates xrtTimer for high-precision timing and xrtSleep
 *       for pausing execution.
 *   CN: 演示 xrtTimer 高精度计时和 xrtSleep 暂停执行。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/time_timer.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/time_timer              (Linux, GCC)
 *
 * Note / 注意:
 *   - xrtTimer returns seconds as double (high precision)
 *   - xrtSleep pauses for specified milliseconds
 *   - Timer uses platform-specific high-resolution counters
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test basic timer
// 测试基本计时器
void TestBasicTimer()
{
	printf("=== Basic Timer ===\n");
	printf("=== 基本计时器 ===\n");
	
	double fStart = xrtTimer();
	printf("Start time: %.6f seconds\n", fStart);
	printf("开始时间: %.6f 秒\n", fStart);
	
	// Do some work
	// 做一些工作
	int i;
	volatile int iSum = 0;
	for ( i = 0; i < 1000000; i++ ) {
		iSum += i;
	}
	
	double fEnd = xrtTimer();
	double fElapsed = fEnd - fStart;
	
	printf("End time: %.6f seconds\n", fEnd);
	printf("结束时间: %.6f 秒\n", fEnd);
	printf("Elapsed: %.6f seconds\n", fElapsed);
	printf("耗时: %.6f 秒\n", fElapsed);
	printf("\n");
}

// Test xrtSleep
// 测试 xrtSleep
void TestSleep()
{
	printf("=== xrtSleep Test ===\n");
	printf("=== xrtSleep 测试 ===\n");
	
	printf("Sleeping for 100ms...\n");
	printf("休眠 100 毫秒...\n");
	
	double fStart = xrtTimer();
	xrtSleep(100);
	double fEnd = xrtTimer();
	
	printf("Actual sleep: %.3f ms\n", (fEnd - fStart) * 1000.0);
	printf("实际休眠: %.3f 毫秒\n\n", (fEnd - fStart) * 1000.0);
	
	printf("Sleeping for 500ms...\n");
	printf("休眠 500 毫秒...\n");
	
	fStart = xrtTimer();
	xrtSleep(500);
	fEnd = xrtTimer();
	
	printf("Actual sleep: %.3f ms\n", (fEnd - fStart) * 1000.0);
	printf("实际休眠: %.3f 毫秒\n", (fEnd - fStart) * 1000.0);
	printf("\n");
}

// Test multiple timer calls
// 测试多次计时器调用
void TestMultipleCalls()
{
	printf("=== Multiple Timer Calls ===\n");
	printf("=== 多次计时器调用 ===\n");
	
	printf("Calling xrtTimer 5 times:\n");
	printf("调用 xrtTimer 5 次:\n");
	
	int i;
	for ( i = 0; i < 5; i++ ) {
		double fTime = xrtTimer();
		printf("  Call %d: %.6f\n", i + 1, fTime);
	}
	printf("\n");
}

// Test timing precision
// 测试计时精度
void TestTimingPrecision()
{
	printf("=== Timing Precision ===\n");
	printf("=== 计时精度 ===\n");
	
	printf("Measuring minimal measurable time:\n");
	printf("测量最小可测时间:\n");
	
	int i;
	double fMinElapsed = 1.0;
	
	for ( i = 0; i < 10; i++ ) {
		double fStart = xrtTimer();
		double fEnd = xrtTimer();
		double fElapsed = fEnd - fStart;
		if ( fElapsed < fMinElapsed && fElapsed > 0 ) {
			fMinElapsed = fElapsed;
		}
	}
	
	printf("Minimum measurable: %.9f seconds\n", fMinElapsed);
	printf("最小可测: %.9f 秒\n", fMinElapsed);
	printf("Approximately %.3f microseconds\n", fMinElapsed * 1000000.0);
	printf("约 %.3f 微秒\n\n", fMinElapsed * 1000000.0);
}

// Test benchmarking pattern
// 测试基准测试模式
void TestBenchmarking()
{
	printf("=== Benchmarking Pattern ===\n");
	printf("=== 基准测试模式 ===\n");
	
	printf("Benchmark: Sum of 10 million integers\n");
	printf("基准测试: 1000 万整数求和\n\n");
	
	int iIterations = 10;
	double fTotalTime = 0;
	int i;
	
	for ( i = 0; i < iIterations; i++ ) {
		double fStart = xrtTimer();
		
		volatile int64 iSum = 0;
		int j;
		for ( j = 0; j < 10000000; j++ ) {
			iSum += j;
		}
		
		double fElapsed = xrtTimer() - fStart;
		fTotalTime += fElapsed;
		printf("  Iteration %d: %.4f ms\n", i + 1, fElapsed * 1000.0);
	}
	
	printf("\nAverage: %.4f ms\n", (fTotalTime / iIterations) * 1000.0);
	printf("平均: %.4f 毫秒\n\n", (fTotalTime / iIterations) * 1000.0);
}

// Test practical use cases
// 测试实际用例
void TestPracticalUseCases()
{
	printf("=== Practical Use Cases ===\n");
	printf("=== 实际用例 ===\n");
	
	// Rate limiting
	// 速率限制
	printf("Rate limiting example:\n");
	printf("速率限制示例:\n");
	printf("Running 5 operations with 100ms delay each...\n");
	printf("运行 5 个操作，每个延迟 100 毫秒...\n");
	
	double fStart = xrtTimer();
	int i;
	for ( i = 0; i < 5; i++ ) {
		printf("  Operation %d at %.3f s\n", i + 1, xrtTimer() - fStart);
		xrtSleep(100);
	}
	double fTotal = xrtTimer() - fStart;
	printf("Total time: %.3f s\n\n", fTotal);
	
	// Performance measurement
	// 性能测量
	printf("Performance measurement:\n");
	printf("性能测量:\n");
	
	fStart = xrtTimer();
	
	// Simulate work
	// 模拟工作
	xrtSleep(50);
	
	double fElapsed = xrtTimer() - fStart;
	printf("Task completed in %.3f ms\n", fElapsed * 1000.0);
	printf("任务完成于 %.3f 毫秒\n", fElapsed * 1000.0);
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Time - Timer Demo\n");
	printf("XRT 时间 - 计时器演示\n");
	printf("=====================\n\n");
	
	TestBasicTimer();
	TestSleep();
	TestMultipleCalls();
	TestTimingPrecision();
	TestBenchmarking();
	TestPracticalUseCases();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
