/*
 * XRT Example - Number Formatting
 * XRT 范例 - 数字格式化
 *
 * Description / 说明:
 *   EN: Demonstrates xrtIntFormat and xrtNumFormat for locale-aware
 *       number formatting with thousands separators.
 *   CN: 演示 xrtIntFormat 和 xrtNumFormat 实现带千位分隔符的本地化数字格式化。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/string_number_format.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/string_number_format -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Format strings support grouping with comma/period
 *   - xrtIntFormat for integers, xrtNumFormat for floats
 *   - Returns newly allocated string, must be freed with xrtFree
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test xrtIntFormat basic usage
// 测试 xrtIntFormat 基本用法
void TestIntFormatBasic()
{
	printf("=== xrtIntFormat Basic ===\n");
	printf("=== xrtIntFormat 基本 ===\n");
	
	int64 iValues[] = {0, 42, 1234, 1234567, 1234567890, -1234567, 9223372036854775807LL};
	int iNumValues = 7;
	int i;
	
	printf("Integer values without grouping:\n");
	printf("无分组的整数值:\n");
	for ( i = 0; i < iNumValues; i++ ) {
		str sFormatted = xrtIntFormat(iValues[i], "d");
		printf("  %22lld -> %s\n", iValues[i], sFormatted);
		xrtFree(sFormatted);
	}
	printf("\n");
}

// Test xrtIntFormat with grouping
// 测试带分组的 xrtIntFormat
void TestIntFormatGrouping()
{
	printf("=== xrtIntFormat with Grouping ===\n");
	printf("=== 带分组的 xrtIntFormat ===\n");
	
	int64 iValues[] = {0, 42, 1234, 1234567, 1234567890, -1234567, 999999999999};
	int iNumValues = 7;
	int i;
	
	printf("Integer values with comma grouping:\n");
	printf("带逗号分组的整数值:\n");
	for ( i = 0; i < iNumValues; i++ ) {
		str sFormatted = xrtIntFormat(iValues[i], ",d");
		printf("  %22lld -> %s\n", iValues[i], sFormatted);
		xrtFree(sFormatted);
	}
	printf("\n");
	
	printf("Integer values with period grouping:\n");
	printf("带句点分组的整数值:\n");
	for ( i = 0; i < iNumValues; i++ ) {
		str sFormatted = xrtIntFormat(iValues[i], ".d");
		printf("  %22lld -> %s\n", iValues[i], sFormatted);
		xrtFree(sFormatted);
	}
	printf("\n");
}

// Test xrtIntFormat hex and other bases
// 测试 xrtIntFormat 十六进制和其他进制
void TestIntFormatBases()
{
	printf("=== xrtIntFormat Different Bases ===\n");
	printf("=== xrtIntFormat 不同进制 ===\n");
	
	int64 iValue = 255;
	
	printf("Value: %lld (255 decimal)\n\n", iValue);
	printf("值: %lld (255 十进制)\n\n", iValue);
	
	// Decimal
	// 十进制
	str sDec = xrtIntFormat(iValue, "d");
	printf("Decimal (d): %s\n", sDec);
	printf("十进制: %s\n", sDec);
	xrtFree(sDec);
	
	// Hex lowercase
	// 十六进制小写
	str sHexLower = xrtIntFormat(iValue, "x");
	printf("Hex lowercase (x): %s\n", sHexLower);
	printf("十六进制小写 (x): %s\n", sHexLower);
	xrtFree(sHexLower);
	
	// Hex uppercase
	// 十六进制大写
	str sHexUpper = xrtIntFormat(iValue, "X");
	printf("Hex uppercase (X): %s\n", sHexUpper);
	printf("十六进制大写 (X): %s\n", sHexUpper);
	xrtFree(sHexUpper);
	
	// Octal
	// 八进制
	str sOct = xrtIntFormat(iValue, "o");
	printf("Octal (o): %s\n", sOct);
	printf("八进制: %s\n", sOct);
	xrtFree(sOct);
	
	// Binary
	// 二进制
	str sBin = xrtIntFormat(iValue, "b");
	printf("Binary (b): %s\n", sBin);
	printf("二进制: %s\n", sBin);
	xrtFree(sBin);
	
	printf("\n");
}

// Test xrtNumFormat basic usage
// 测试 xrtNumFormat 基本用法
void TestNumFormatBasic()
{
	printf("=== xrtNumFormat Basic ===\n");
	printf("=== xrtNumFormat 基本 ===\n");
	
	double fValues[] = {0.0, 3.14159, 1234.5678, 1234567.89, -9876.54321, 0.000123456};
	int iNumValues = 6;
	int i;
	
	printf("Float values:\n");
	printf("浮点值:\n");
	for ( i = 0; i < iNumValues; i++ ) {
		str sFormatted = xrtNumFormat(fValues[i], ".2f");
		printf("  %20.6f -> %s\n", fValues[i], sFormatted);
		xrtFree(sFormatted);
	}
	printf("\n");
}

// Test xrtNumFormat with grouping
// 测试带分组的 xrtNumFormat
void TestNumFormatGrouping()
{
	printf("=== xrtNumFormat with Grouping ===\n");
	printf("=== 带分组的 xrtNumFormat ===\n");
	
	double fValues[] = {1234.56, 1234567.89, 9876543.21, -1234567.89};
	int iNumValues = 4;
	int i;
	
	printf("Float values with comma grouping:\n");
	printf("带逗号分组的浮点值:\n");
	for ( i = 0; i < iNumValues; i++ ) {
		str sFormatted = xrtNumFormat(fValues[i], ",.2f");
		printf("  %15.2f -> %s\n", fValues[i], sFormatted);
		xrtFree(sFormatted);
	}
	printf("\n");
	
	printf("Float values with period grouping:\n");
	printf("带句点分组的浮点值:\n");
	for ( i = 0; i < iNumValues; i++ ) {
		str sFormatted = xrtNumFormat(fValues[i], "..2f");
		printf("  %15.2f -> %s\n", fValues[i], sFormatted);
		xrtFree(sFormatted);
	}
	printf("\n");
}

// Test xrtNumFormat precision
// 测试 xrtNumFormat 精度
void TestNumFormatPrecision()
{
	printf("=== xrtNumFormat Precision ===\n");
	printf("=== xrtNumFormat 精度 ===\n");
	
	double fValue = 3.141592653589793;
	
	printf("Value: %.15f\n\n", fValue);
	printf("值: %.15f\n\n", fValue);
	
	char* psFormats[] = {".0f", ".1f", ".2f", ".4f", ".6f", ".8f", ".10f"};
	char* psDesc[] = {"0 decimals", "1 decimal", "2 decimals", "4 decimals", 
	                  "6 decimals", "8 decimals", "10 decimals"};
	int iNumFormats = 7;
	int i;
	
	for ( i = 0; i < iNumFormats; i++ ) {
		str sFormatted = xrtNumFormat(fValue, psFormats[i]);
		printf("  %s (%s): %s\n", psDesc[i], psFormats[i], sFormatted);
		xrtFree(sFormatted);
	}
	printf("\n");
}

// Test practical use cases
// 测试实际用例
void TestPracticalUseCases()
{
	printf("=== Practical Use Cases ===\n");
	printf("=== 实际用例 ===\n");
	
	// Currency formatting
	// 货币格式化
	printf("Currency formatting:\n");
	printf("货币格式化:\n");
	double fPrices[] = {9.99, 1234.56, 999999.99, 12345678.90};
	int i;
	for ( i = 0; i < 4; i++ ) {
		str sPrice = xrtNumFormat(fPrices[i], ",.2f");
		printf("  $%s\n", sPrice);
		xrtFree(sPrice);
	}
	printf("\n");
	
	// Population counts
	// 人口计数
	printf("Population counts:\n");
	printf("人口计数:\n");
	int64 iPopulations[] = {8100000000LL, 1400000000LL, 330000000LL, 67000000LL};
	char* psCountries[] = {"World", "China", "USA", "UK"};
	char* psCountriesCN[] = {"世界", "中国", "美国", "英国"};
	for ( i = 0; i < 4; i++ ) {
		str sPop = xrtIntFormat(iPopulations[i], ",d");
		printf("  %s (%s): %s\n", psCountries[i], psCountriesCN[i], sPop);
		xrtFree(sPop);
	}
	printf("\n");
	
	// Scientific notation
	// 科学计数法
	printf("Scientific values:\n");
	printf("科学数值:\n");
	double fScientific[] = {0.000000001, 299792458.0, 6.022e23};
	char* psSciDesc[] = {"Speed of light (m/s)", "Planck constant", "Avogadro number"};
	char* psSciDescCN[] = {"光速 (米/秒)", "普朗克常数", "阿伏伽德罗数"};
	for ( i = 0; i < 3; i++ ) {
		str sSci = xrtNumFormat(fScientific[i], ",.3e");
		printf("  %s (%s): %s\n", psSciDesc[i], psSciDescCN[i], sSci);
		xrtFree(sSci);
	}
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT String - Number Formatting Demo\n");
	printf("XRT 字符串 - 数字格式化演示\n");
	printf("===================================\n\n");
	
	TestIntFormatBasic();
	TestIntFormatGrouping();
	TestIntFormatBases();
	TestNumFormatBasic();
	TestNumFormatGrouping();
	TestNumFormatPrecision();
	TestPracticalUseCases();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
