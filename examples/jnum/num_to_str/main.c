/*
 * XRT Example - Number to String
 * XRT 范例 - 数字转字符串
 *
 * Description / 说明:
 *   EN: Demonstrates xrtI32ToStr, xrtI64ToStr, xrtNumToStr for converting
 *       numbers to string representation.
 *   CN: 演示 xrtI32ToStr、xrtI64ToStr、xrtNumToStr 将数字转换为字符串表示。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/jnum_num_to_str.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/jnum_num_to_str              (Linux, GCC)
 *
 * Note / 注意:
 *   - Functions write to provided buffer and return length
 *   - Buffer should be large enough (32 bytes recommended)
 *   - Returns number of characters written (excluding null)
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test int32 to string conversion
// 测试 int32 到字符串转换
void TestI32ToStr()
{
	printf("=== xrtI32ToStr (int32 to string) ===\n");
	printf("=== xrtI32ToStr (int32 到字符串) ===\n");
	
	int32_t iValues[] = {0, 1, -1, 42, -42, 2147483647, -2147483648};
	char* psDesc[] = {"Zero", "One", "Negative one", "42", "-42", "INT_MAX", "INT_MIN"};
	
	char sBuffer[32];
	int iNumValues = 7;
	int i;
	
	for ( i = 0; i < iNumValues; i++ ) {
		int iLen = xrtI32ToStr(iValues[i], sBuffer);
		printf("  %12d -> \"%s\" (len=%d) [%s]\n", 
		       iValues[i], sBuffer, iLen, psDesc[i]);
	}
	printf("\n");
}

// Test int64 to string conversion
// 测试 int64 到字符串转换
void TestI64ToStr()
{
	printf("=== xrtI64ToStr (int64 to string) ===\n");
	printf("=== xrtI64ToStr (int64 到字符串) ===\n");
	
	int64_t iValues[] = {
		0,
		1,
		-1,
		9223372036854775807LL,   // LLONG_MAX
		-9223372036854775807LL,  // Near LLONG_MIN
		12345678901234LL,
		-98765432109876LL,
	};
	char* psDesc[] = {
		"Zero",
		"One",
		"Negative one",
		"LLONG_MAX",
		"Near LLONG_MIN",
		"Large positive",
		"Large negative",
	};
	
	char sBuffer[32];
	int iNumValues = 7;
	int i;
	
	for ( i = 0; i < iNumValues; i++ ) {
		int iLen = xrtI64ToStr(iValues[i], sBuffer);
		printf("  %22lld -> \"%s\" (len=%d) [%s]\n", 
		       iValues[i], sBuffer, iLen, psDesc[i]);
	}
	printf("\n");
}

// Test double to string conversion
// 测试 double 到字符串转换
void TestNumToStr()
{
	printf("=== xrtNumToStr (double to string) ===\n");
	printf("=== xrtNumToStr (double 到字符串) ===\n");
	
	double fValues[] = {
		0.0,
		1.0,
		-1.0,
		3.14159265358979,
		-2.71828182845904,
		1.23456789e10,
		1.23456789e-10,
		123456.789,
		0.00000123456,
	};
	char* psDesc[] = {
		"Zero",
		"One",
		"Negative one",
		"Pi approximation",
		"e approximation",
		"Large scientific",
		"Small scientific",
		"Medium decimal",
		"Small decimal",
	};
	
	char sBuffer[64];
	int iNumValues = 9;
	int i;
	
	for ( i = 0; i < iNumValues; i++ ) {
		int iLen = xrtNumToStr(fValues[i], sBuffer);
		printf("  %20.15g -> \"%s\" (len=%d) [%s]\n", 
		       fValues[i], sBuffer, iLen, psDesc[i]);
	}
	printf("\n");
}

// Test special values
// 测试特殊值
void TestSpecialValues()
{
	printf("=== Special Values ===\n");
	printf("=== 特殊值 ===\n");
	
	char sBuffer[64];
	
	// Note: Behavior may vary with special floating-point values
	// 注意: 特殊浮点值的行为可能不同
	printf("Testing edge cases:\n");
	printf("测试边界情况:\n");
	
	// Very small numbers
	// 非常小的数字
	int iLen = xrtNumToStr(1e-15, sBuffer);
	printf("  1e-15 -> \"%s\" (len=%d)\n", sBuffer, iLen);
	
	// Very large numbers
	// 非常大的数字
	iLen = xrtNumToStr(1e15, sBuffer);
	printf("  1e15 -> \"%s\" (len=%d)\n", sBuffer, iLen);
	
	// Powers of 10
	// 10 的幂
	iLen = xrtNumToStr(1000000.0, sBuffer);
	printf("  1000000 -> \"%s\" (len=%d)\n", sBuffer, iLen);
	
	// Binary round numbers
	// 二进制圆数
	iLen = xrtNumToStr(1024.0, sBuffer);
	printf("  1024 -> \"%s\" (len=%d)\n", sBuffer, iLen);
	
	printf("\n");
}

// Test practical use cases
// 测试实际用例
void TestPracticalUseCases()
{
	printf("=== Practical Use Cases ===\n");
	printf("=== 实际用例 ===\n");
	
	char sBuffer[64];
	
	// Building CSV row
	// 构建 CSV 行
	printf("CSV data:\n");
	printf("CSV 数据:\n");
	
	int iID = 12345;
	int64_t iTimestamp = 1703520000;
	double fAmount = 1234.56;
	
	xrtI32ToStr(iID, sBuffer);
	printf("  ID: %s\n", sBuffer);
	
	xrtI64ToStr(iTimestamp, sBuffer);
	printf("  Timestamp: %s\n", sBuffer);
	
	xrtNumToStr(fAmount, sBuffer);
	printf("  Amount: %s\n", sBuffer);
	printf("\n");
	
	// Building file names with counters
	// 构建带计数器的文件名
	printf("Generated filenames:\n");
	printf("生成的文件名:\n");
	
	int i;
	for ( i = 0; i < 5; i++ ) {
		xrtI32ToStr(i, sBuffer);
		printf("  output_%s.txt\n", sBuffer);
	}
	printf("\n");
	
	// Display measurements
	// 显示测量值
	printf("Measurements:\n");
	printf("测量值:\n");
	
	double fTemps[] = {36.5, 37.2, 36.8, 38.1};
	int iNumTemps = 4;
	for ( i = 0; i < iNumTemps; i++ ) {
		xrtNumToStr(fTemps[i], sBuffer);
		printf("  Temperature %d: %s C\n", i+1, sBuffer);
	}
	printf("\n");
}

// Test performance characteristics
// 测试性能特征
void TestPerformance()
{
	printf("=== Performance Test ===\n");
	printf("=== 性能测试 ===\n");
	
	char sBuffer[64];
	int64_t iValue = 1234567890123LL;
	int iIterations = 10000;
	
	printf("Converting %lld %d times...\n", iValue, iIterations);
	printf("将 %lld 转换 %d 次...\n", iValue, iIterations);
	
	clock_t start = clock();
	int i;
	for ( i = 0; i < iIterations; i++ ) {
		xrtI64ToStr(iValue, sBuffer);
	}
	clock_t end = clock();
	
	double fElapsed = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;
	printf("Time: %.2f ms (%.2f us per conversion)\n", 
	       fElapsed, fElapsed * 1000.0 / iIterations);
	printf("时间: %.2f 毫秒 (每次转换 %.2f 微秒)\n", 
	       fElapsed, fElapsed * 1000.0 / iIterations);
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT JNUM - Number to String Demo\n");
	printf("XRT JNUM - 数字转字符串演示\n");
	printf("================================\n\n");
	
	TestI32ToStr();
	TestI64ToStr();
	TestNumToStr();
	TestSpecialValues();
	TestPracticalUseCases();
	TestPerformance();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
