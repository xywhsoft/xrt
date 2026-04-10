/*
 * XRT Example - Approximate Equality Comparison
 * XRT 范例 - 约等于比较
 *
 * Description / 说明:
 *   EN: Demonstrates approximate equality comparison for integers and floats
 *       using difference mode and percentage mode with configurable tolerance.
 *   CN: 演示整数和浮点数的约等于比较，支持差值模式和百分比模式，可配置容差。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/math_approx.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/math_approx -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - xrtIntApprox uses integer difference tolerance
 *   - xrtNumApprox uses percentage tolerance (0.0-1.0)
 *   - Configure tolerance via xCore.IntApprox and NumApprox fields
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test integer approximate equality
// 测试整数约等于
void TestIntApprox()
{
	int64 iA = 1000;
	int64 iB = 1005;
	int64 iC = 1015;
	int64 iD = 1100;
	
	printf("=== Integer Approximate Equality ===\n");
	printf("=== 整数约等于比较 ===\n");
	printf("Base value: %lld\n", iA);
	printf("Mode: %s\n", xCore.iApproxIntMode == XRT_APPROX_PERCENT ? "PERCENT" : "DIFFERENCE");
	printf("Tolerance: %lld\n\n", (int64)xCore.fApproxIntTol);
	
	// Test different differences
	// 测试不同差值
	printf("Comparing with tolerance=%lld (mode: %s):\n", 
	       (int64)xCore.fApproxIntTol, 
	       xCore.iApproxIntMode == XRT_APPROX_PERCENT ? "PERCENT" : "DIFFERENCE");
	printf("  %lld vs %lld -> %s\n", iA, iB, xrtIntApprox(iA, iB) ? "EQUAL" : "NOT EQUAL");
	printf("  %lld vs %lld -> %s\n", iA, iC, xrtIntApprox(iC, iA) ? "EQUAL" : "NOT EQUAL");
	printf("  %lld vs %lld -> %s\n", iA, iD, xrtIntApprox(iA, iD) ? "EQUAL" : "NOT EQUAL");
	
	// Test with different tolerances
	// 测试不同容差
	printf("\nChanging tolerance:\n");
	xCore.fApproxIntTol = 2.0;
	printf("Tolerance=2: %lld vs %lld -> %s\n", iA, iB, xrtIntApprox(iA, iB) ? "EQUAL" : "NOT EQUAL");
	
	xCore.fApproxIntTol = 20.0;
	printf("Tolerance=20: %lld vs %lld -> %s\n", iA, iC, xrtIntApprox(iA, iC) ? "EQUAL" : "NOT EQUAL");
	printf("Tolerance=20: %lld vs %lld -> %s\n", iA, iD, xrtIntApprox(iA, iD) ? "EQUAL" : "NOT EQUAL");
	
	// Reset to default
	// 重置为默认值
	xCore.fApproxIntTol = 10.0;
	printf("\n");
}

// Test float approximate equality
// 测试浮点数约等于
void TestFloatApprox()
{
	double fA = 100.0;
	double fB = 100.5;
	double fC = 101.2;
	double fD = 110.0;
	
	printf("=== Float Approximate Equality ===\n");
	printf("=== 浮点数约等于比较 ===\n");
	printf("Base value: %.1f\n", fA);
	printf("Mode: %s\n", xCore.iApproxNumMode == XRT_APPROX_PERCENT ? "PERCENT" : "DIFFERENCE");
	printf("Tolerance: %.2f\n\n", xCore.fApproxNumTol);
	
	// Test different differences
	// 测试不同差值
	printf("Comparing with tolerance=%.2f (mode: %s):\n", 
	       xCore.fApproxNumTol,
	       xCore.iApproxNumMode == XRT_APPROX_PERCENT ? "PERCENT" : "DIFFERENCE");
	printf("  %.1f vs %.1f -> %s\n", fA, fB, xrtNumApprox(fA, fB) ? "EQUAL" : "NOT EQUAL");
	printf("  %.1f vs %.1f -> %s\n", fA, fC, xrtNumApprox(fC, fA) ? "EQUAL" : "NOT EQUAL");
	printf("  %.1f vs %.1f -> %s\n", fA, fD, xrtNumApprox(fA, fD) ? "EQUAL" : "NOT EQUAL");
	
	// Test with different tolerances
	// 测试不同容差
	printf("\nChanging tolerance:\n");
	xCore.fApproxNumTol = 0.001;  // 0.1%
	printf("Tolerance=0.1%%: %.1f vs %.1f -> %s\n", fA, fB, xrtNumApprox(fA, fB) ? "EQUAL" : "NOT EQUAL");
	
	xCore.fApproxNumTol = 0.02;   // 2%
	printf("Tolerance=2%%: %.1f vs %.1f -> %s\n", fA, fC, xrtNumApprox(fA, fC) ? "EQUAL" : "NOT EQUAL");
	printf("Tolerance=2%%: %.1f vs %.1f -> %s\n", fA, fD, xrtNumApprox(fA, fD) ? "NOT EQUAL" : "NOT EQUAL");
	
	// Reset to default
	// 重置为默认值
	xCore.fApproxNumTol = 0.01;  // 1%
	printf("\n");
}

// Test edge cases
// 测试边界情况
void TestEdgeCases()
{
	printf("=== Edge Cases ===\n");
	printf("=== 边界情况 ===\n");
	
	// Zero values
	// 零值
	printf("Zero comparisons:\n");
	printf("  0 vs 0 -> %s\n", xrtIntApprox(0, 0) ? "EQUAL" : "NOT EQUAL");
	printf("  0 vs 1 -> %s (tol=%.0f)\n", xrtIntApprox(0, 1) ? "EQUAL" : "NOT EQUAL", xCore.fApproxIntTol);
	printf("  0.0 vs 0.0 -> %s\n", xrtNumApprox(0.0, 0.0) ? "EQUAL" : "NOT EQUAL");
	printf("  0.0 vs 0.01 -> %s (tol=%.2f)\n", xrtNumApprox(0.0, 0.01) ? "EQUAL" : "NOT EQUAL", xCore.fApproxNumTol);
	
	// Negative values
	// 负值
	printf("\nNegative values:\n");
	printf("  -100 vs -105 -> %s (diff=5)\n", xrtIntApprox(-100, -105) ? "EQUAL" : "NOT EQUAL");
	printf("  -100.0 vs -101.5 -> %s (diff=1.5)\n", xrtNumApprox(-100.0, -101.5) ? "EQUAL" : "NOT EQUAL");
	
	// Large values
	// 大数值
	printf("\nLarge values:\n");
	int64 iLarge1 = 1000000000LL;
	int64 iLarge2 = 1000000009LL;
	int64 iLarge3 = 1000000011LL;
	
	printf("  %lld vs %lld -> %s (diff=9)\n", iLarge1, iLarge2, xrtIntApprox(iLarge1, iLarge2) ? "EQUAL" : "NOT EQUAL");
	printf("  %lld vs %lld -> %s (diff=11)\n", iLarge1, iLarge3, xrtIntApprox(iLarge1, iLarge3) ? "EQUAL" : "NOT EQUAL");
	
	printf("\n");
}

// Test practical examples
// 测试实际应用示例
void TestPracticalExamples()
{
	printf("=== Practical Examples ===\n");
	printf("=== 实际应用示例 ===\n");
	
	// Price comparison (cents)
	// 价格比较（美分）
	printf("Price comparison ($10.00):\n");
	int64 iPrice1 = 1000;  // $10.00
	int64 iPrice2 = 1003;  // $10.03
	int64 iPrice3 = 1015;  // $10.15
	
	xCore.fApproxIntTol = 5.0;  // 5 cents tolerance
	printf("  $10.00 vs $10.03 (diff=3¢) -> %s\n", xrtIntApprox(iPrice1, iPrice2) ? "EQUAL" : "NOT EQUAL");
	printf("  $10.00 vs $10.15 (diff=15¢) -> %s\n", xrtIntApprox(iPrice1, iPrice3) ? "EQUAL" : "NOT EQUAL");
	
	// Temperature comparison (0.1°C precision)
	// 温度比较（0.1°C精度）
	printf("\nTemperature comparison (°C):\n");
	double fTemp1 = 25.0;   // 25.0°C
	double fTemp2 = 25.3;   // 25.3°C
	double fTemp3 = 26.5;   // 26.5°C
	
	xCore.fApproxNumTol = 0.02;  // 2% tolerance
	printf("  25.0°C vs 25.3°C (diff=0.3°C) -> %s\n", xrtNumApprox(fTemp1, fTemp2) ? "EQUAL" : "NOT EQUAL");
	printf("  25.0°C vs 26.5°C (diff=1.5°C) -> %s\n", xrtNumApprox(fTemp1, fTemp3) ? "EQUAL" : "NOT EQUAL");
	
	// Distance measurement (meters)
	// 距离测量（米）
	printf("\nDistance comparison (meters):\n");
	double fDist1 = 1000.0;  // 1000m
	double fDist2 = 1005.0;  // 1005m
	double fDist3 = 1050.0;  // 1050m
	
	xCore.fApproxNumTol = 0.01;  // 1% tolerance
	printf("  1000m vs 1005m (0.5%% diff) -> %s\n", xrtNumApprox(fDist1, fDist2) ? "EQUAL" : "NOT EQUAL");
	printf("  1000m vs 1050m (5%% diff) -> %s\n", xrtNumApprox(fDist1, fDist3) ? "EQUAL" : "NOT EQUAL");
	
	// Reset defaults
	// 重置默认值
	xCore.fApproxIntTol = 10.0;
	xCore.fApproxNumTol = 0.01;
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	// Show default configuration
	// 显示默认配置
	printf("XRT Math Approximate Equality Demo\n");
	printf("XRT 数学约等于比较演示\n");
	printf("==================================\n\n");
	printf("Default configuration:\n");
	printf("默认配置:\n");
	printf("  Int Mode: %s\n", xCore.iApproxIntMode == XRT_APPROX_PERCENT ? "PERCENT" : "DIFFERENCE");
	printf("  Int Tolerance: %.0f\n", xCore.fApproxIntTol);
	printf("  Num Mode: %s\n", xCore.iApproxNumMode == XRT_APPROX_PERCENT ? "PERCENT" : "DIFFERENCE");
	printf("  Num Tolerance: %.2f (%.0f%%)\n\n", xCore.fApproxNumTol, xCore.fApproxNumTol * 100);
	
	// Run all tests
	// 运行所有测试
	TestIntApprox();
	TestFloatApprox();
	TestEdgeCases();
	TestPracticalExamples();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}