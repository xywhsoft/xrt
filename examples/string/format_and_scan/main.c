/*
 * XRT Example - String Format and Scan
 * XRT 范例 - 字符串格式化与扫描
 *
 * Description / 说明:
 *   EN: Demonstrates string formatting functions (xrtFormat/xrtFormatV)
 *       and scanning/parsing functions for converting between strings and values.
 *   CN: 演示字符串格式化函数（xrtFormat/xrtFormatV）和扫描/解析函数，
 *       用于在字符串和数值之间转换。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/string_format_and_scan.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/string_format_and_scan -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - xrtFormat: format string with printf-style syntax
 *   - xrtFormatV: format string with va_list parameters
 *   - Manual parsing functions for number conversion
 *   - Supports integers, floats, hex, octal formats
 *   - Format strings use % specifiers like printf
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test basic string formatting
// 测试基本字符串格式化
void TestBasicFormatting()
{
	printf("=== Basic String Formatting ===\n");
	printf("=== 基本字符串格式化 ===\n");
	
	// Integer formatting
	// 整数格式化
	int iValue = 42;
	unsigned int uiValue = 255;
	long lValue = 1234567890L;
	
	str sFormatted1 = xrtFormat("Integer: %d", iValue);
	str sFormatted2 = xrtFormat("Unsigned: %u", uiValue);
	str sFormatted3 = xrtFormat("Long: %ld", lValue);
	
	printf("Integer formatting:\n");
	printf("  %%d: %s\n", sFormatted1);
	printf("  %%u: %s\n", sFormatted2);
	printf("  %%ld: %s\n\n", sFormatted3);
	
	// Hexadecimal and octal
	// 十六进制和八进制
	str sFormatted4 = xrtFormat("Hex lowercase: %x", uiValue);
	str sFormatted5 = xrtFormat("Hex uppercase: %X", uiValue);
	str sFormatted6 = xrtFormat("Octal: %o", uiValue);
	
	printf("Number base formatting:\n");
	printf("  %%x: %s\n", sFormatted4);
	printf("  %%X: %s\n", sFormatted5);
	printf("  %%o: %s\n\n", sFormatted6);
	
	// Float formatting
	// 浮点数格式化
	float fValue1 = 3.14159f;
	double dValue1 = 2.718281828459045;
	
	str sFormatted7 = xrtFormat("Float: %.2f", fValue1);
	str sFormatted8 = xrtFormat("Double: %.6f", dValue1);
	str sFormatted9 = xrtFormat("Scientific: %.2e", dValue1);
	
	printf("Float formatting:\n");
	printf("  %%.2f: %s\n", sFormatted7);
	printf("  %%.6f: %s\n", sFormatted8);
	printf("  %%.2e: %s\n\n", sFormatted9);
	
	// String and character formatting
	// 字符串和字符格式化
	str sText = "Hello World";
	char cChar = 'A';
	
	str sFormatted10 = xrtFormat("String: %s", sText);
	str sFormatted11 = xrtFormat("Character: %c", cChar);
	str sFormatted12 = xrtFormat("Percent sign: %%");
	
	printf("String/character formatting:\n");
	printf("  %%s: %s\n", sFormatted10);
	printf("  %%c: %s\n", sFormatted11);
	printf("  %%: %s\n\n", sFormatted12);
	
	// Field width and alignment
	// 字段宽度和对齐
	str sFormatted13 = xrtFormat("Right aligned: %10d", iValue);
	str sFormatted14 = xrtFormat("Left aligned: %-10d", iValue);
	str sFormatted15 = xrtFormat("Zero padded: %010d", iValue);
	
	printf("Alignment formatting:\n");
	printf("  %%10d: '%s'\n", sFormatted13);
	printf("  %%-10d: '%s'\n", sFormatted14);
	printf("  %%010d: '%s'\n\n", sFormatted15);
	
	// Precision formatting
	// 精度格式化
	str sFormatted16 = xrtFormat("Precision float: %.3f", 3.14159);
	str sFormatted17 = xrtFormat("Precision string: %.5s", "Hello World");
	
	printf("Precision formatting:\n");
	printf("  %%.3f: %s\n", sFormatted16);
	printf("  %%.5s: %s\n\n", sFormatted17);
	
	// Cleanup
	// 清理
	xrtFree(sFormatted1);
	xrtFree(sFormatted2);
	xrtFree(sFormatted3);
	xrtFree(sFormatted4);
	xrtFree(sFormatted5);
	xrtFree(sFormatted6);
	xrtFree(sFormatted7);
	xrtFree(sFormatted8);
	xrtFree(sFormatted9);
	xrtFree(sFormatted10);
	xrtFree(sFormatted11);
	xrtFree(sFormatted12);
	xrtFree(sFormatted13);
	xrtFree(sFormatted14);
	xrtFree(sFormatted15);
	xrtFree(sFormatted16);
	xrtFree(sFormatted17);
}

// Test complex formatting scenarios
// 测试复杂格式化场景
void TestComplexFormatting()
{
	printf("=== Complex Formatting Scenarios ===\n");
	printf("=== 复杂格式化场景 ===\n");
	
	// Multiple values in one format
	// 一个格式中的多个值
	int iAge = 25;
	str sName = "Alice";
	float fScore = 95.5f;
	
	str sFormatted1 = xrtFormat("Student: %s, Age: %d, Score: %.1f%%", sName, iAge, fScore);
	printf("Multiple values: %s\n\n", sFormatted1);
	
	// Date/time formatting simulation
	// 日期/时间格式化模拟
	int iYear = 2024;
	int iMonth = 12;
	int iDay = 25;
	int iHour = 14;
	int iMinute = 30;
	int iSecond = 45;
	
	str sFormatted2 = xrtFormat("%04d-%02d-%02d %02d:%02d:%02d", 
	                            iYear, iMonth, iDay, iHour, iMinute, iSecond);
	printf("Date/Time: %s\n\n", sFormatted2);
	
	// Memory address formatting
	// 内存地址格式化
	void* pAddress = (void*)0x1234ABCD;
	str sFormatted3 = xrtFormat("Address: %p", pAddress);
	printf("Pointer: %s\n\n", sFormatted3);
	
	// Table-like formatting
	// 表格格式化
	str sHeader = xrtFormat("%-15s %8s %10s", "Name", "Age", "Salary");
	str sRow1 = xrtFormat("%-15s %8d %10.2f", "John Smith", 30, 50000.0);
	str sRow2 = xrtFormat("%-15s %8d %10.2f", "Jane Doe", 25, 45000.0);
	str sRow3 = xrtFormat("%-15s %8d %10.2f", "Bob Johnson", 35, 55000.0);
	
	printf("Table formatting:\n");
	printf("%s\n", sHeader);
	printf("----------------------------------------\n");
	printf("%s\n", sRow1);
	printf("%s\n", sRow2);
	printf("%s\n", sRow3);
	printf("\n");
	
	// Hex dump simulation
	// 十六进制转储模拟
	unsigned char arrBytes[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
	size_t iByteCount = sizeof(arrBytes);
	
	str sHexDump = xrtMalloc(iByteCount * 3 + 1);  // 2 hex chars + space per byte
	sHexDump[0] = '\0';
	for ( size_t i = 0; i < iByteCount; i++ ) {
		str sTemp = xrtFormat("%02X ", arrBytes[i]);
		strcat(sHexDump, sTemp);
		xrtFree(sTemp);
	}
	
	printf("Hex dump: %s\n\n", sHexDump);
	
	// Cleanup
	// 清理
	xrtFree(sFormatted1);
	xrtFree(sFormatted2);
	xrtFree(sFormatted3);
	xrtFree(sHeader);
	xrtFree(sRow1);
	xrtFree(sRow2);
	xrtFree(sRow3);
	xrtFree(sHexDump);
}

// Test string to number conversion (scanning)
// 测试字符串到数字转换（扫描）
void TestStringToNumberConversion()
{
	printf("=== String to Number Conversion ===\n");
	printf("=== 字符串到数字转换 ===\n");
	
	// Integer parsing
	// 整数解析
	str sInt1 = "123";
	str sInt2 = "-456";
	str sInt3 = "0xFF";  // Hexadecimal
	str sInt4 = "0755";  // Octal
	
	// Manual parsing using standard library functions
	// 使用标准库函数手动解析
	int iParsed1 = atoi(sInt1);
	int iParsed2 = atoi(sInt2);
	
	// For hex and octal, we'd need strtol with base parameter
	// 对于十六进制和八进制，需要使用带基数的 strtol
	long lParsed3 = strtol(sInt3, NULL, 16);
	long lParsed4 = strtol(sInt4, NULL, 8);
	
	printf("Integer parsing:\n");
	printf("  \"%s\" -> %d\n", sInt1, iParsed1);
	printf("  \"%s\" -> %d\n", sInt2, iParsed2);
	printf("  \"%s\" -> %ld (hex)\n", sInt3, lParsed3);
	printf("  \"%s\" -> %ld (octal)\n\n", sInt4, lParsed4);
	
	// Float parsing
	// 浮点数解析
	str sFloat1 = "3.14159";
	str sFloat2 = "-2.718";
	str sFloat3 = "1.23e-4";
	
	float fParsed1 = atof(sFloat1);
	float fParsed2 = atof(sFloat2);
	double dParsed3 = atof(sFloat3);
	
	printf("Float parsing:\n");
	printf("  \"%s\" -> %.5f\n", sFloat1, fParsed1);
	printf("  \"%s\" -> %.3f\n", sFloat2, fParsed2);
	printf("  \"%s\" -> %.2e\n\n", sFloat3, dParsed3);
	
	// Validation and error handling
	// 验证和错误处理
	str sInvalid1 = "abc123";
	str sInvalid2 = "12.34.56";
	str sInvalid3 = "";
	
	int iInvalid1 = atoi(sInvalid1);
	float fInvalid2 = atof(sInvalid2);
	int iInvalid3 = atoi(sInvalid3);
	
	printf("Error handling:\n");
	printf("  \"%s\" -> %d (partial conversion)\n", sInvalid1, iInvalid1);
	printf("  \"%s\" -> %.2f (stops at second decimal)\n", sInvalid2, fInvalid2);
	printf("  \"%s\" -> %d (empty string)\n\n", sInvalid3, iInvalid3);
	
	// Boundary values
	// 边界值
	str sMaxInt = "2147483647";   // INT_MAX
	str sMinInt = "-2147483648";  // INT_MIN
	
	int iMaxParsed = atoi(sMaxInt);
	int iMinParsed = atoi(sMinInt);
	
	printf("Boundary values:\n");
	printf("  Max int: \"%s\" -> %d\n", sMaxInt, iMaxParsed);
	printf("  Min int: \"%s\" -> %d\n\n", sMinInt, iMinParsed);
}

// Test number to string conversion
// 测试数字到字符串转换
void TestNumberToStringConversion()
{
	printf("=== Number to String Conversion ===\n");
	printf("=== 数字到字符串转换 ===\n");
	
	// Integer to string
	// 整数到字符串
	int iValue1 = 42;
	int iValue2 = -123;
	unsigned int uiValue1 = 255;
	
	// Using xrtFormat for conversion
	// 使用 xrtFormat 进行转换
	str sConverted1 = xrtFormat("%d", iValue1);
	str sConverted2 = xrtFormat("%d", iValue2);
	str sConverted3 = xrtFormat("%u", uiValue1);
	str sConverted4 = xrtFormat("%x", uiValue1);  // Hex
	
	printf("Integer to string:\n");
	printf("  %d -> \"%s\"\n", iValue1, sConverted1);
	printf("  %d -> \"%s\"\n", iValue2, sConverted2);
	printf("  %u -> \"%s\"\n", uiValue1, sConverted3);
	printf("  0x%x -> \"%s\"\n\n", uiValue1, sConverted4);
	
	// Float to string
	// 浮点数到字符串
	float fValue1 = 3.14159f;
	double dValue1 = 2.718281828459045;
	
	str sConverted5 = xrtFormat("%.2f", fValue1);
	str sConverted6 = xrtFormat("%.6f", dValue1);
	str sConverted7 = xrtFormat("%e", dValue1);
	
	printf("Float to string:\n");
	printf("  %.5f -> \"%s\"\n", fValue1, sConverted5);
	printf("  %.15f -> \"%s\"\n", dValue1, sConverted6);
	printf("  %.15f -> \"%s\" (scientific)\n\n", dValue1, sConverted7);
	
	// Padding and formatting
	// 填充和格式化
	int iSmall = 7;
	
	str sConverted8 = xrtFormat("%03d", iSmall);   // Zero padding
	str sConverted9 = xrtFormat("%5d", iSmall);    // Right align
	str sConverted10 = xrtFormat("%-5d", iSmall);  // Left align
	
	printf("Padding and alignment:\n");
	printf("  %d with %%03d -> \"%s\"\n", iSmall, sConverted8);
	printf("  %d with %%5d -> \"%s\"\n", iSmall, sConverted9);
	printf("  %d with %%-5d -> \"%s\"\n\n", iSmall, sConverted10);
	
	// Hexadecimal representations
	// 十六进制表示
	unsigned int uiColors[] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00};
	
	printf("Color codes (hex):\n");
	for ( int i = 0; i < 4; i++ ) {
		str sHex = xrtFormat("#%06X", uiColors[i]);
		printf("  Color %d: %s\n", i+1, sHex);
		xrtFree(sHex);
	}
	printf("\n");
	
	// Cleanup
	// 清理
	xrtFree(sConverted1);
	xrtFree(sConverted2);
	xrtFree(sConverted3);
	xrtFree(sConverted4);
	xrtFree(sConverted5);
	xrtFree(sConverted6);
	xrtFree(sConverted7);
	xrtFree(sConverted8);
	xrtFree(sConverted9);
	xrtFree(sConverted10);
}

// Test practical examples
// 测试实际应用示例
void TestPracticalExamples()
{
	printf("=== Practical Examples ===\n");
	printf("=== 实际应用示例 ===\n");
	
	// Configuration file generation
	// 配置文件生成
	str sAppName = "MyApplication";
	int iVersionMajor = 1;
	int iVersionMinor = 2;
	int iVersionPatch = 3;
	bool bDebug = TRUE;
	int iPort = 8080;
	
	str sConfig = xrtFormat(
		"# Application Configuration\n"
		"app.name=%s\n"
		"app.version=%d.%d.%d\n"
		"app.debug=%s\n"
		"server.port=%d\n"
		"server.host=localhost\n",
		sAppName,
		iVersionMajor, iVersionMinor, iVersionPatch,
		bDebug ? "true" : "false",
		iPort
	);
	
	printf("Configuration file:\n%s\n", sConfig);
	
	// Log message formatting
	// 日志消息格式化
	int iLogLevel = 2;  // INFO
	str sLogLevelNames[] = {"ERROR", "WARN", "INFO", "DEBUG"};
	time_t tNow = time(NULL);
	str sTimestamp = ctime(&tNow);
	sTimestamp[strlen(sTimestamp)-1] = '\0';  // Remove newline
	
	str sLogMessage = xrtFormat("[%s] [%s] User 'admin' logged in from IP 192.168.1.100",
	                            sTimestamp, sLogLevelNames[iLogLevel]);
	
	printf("Log message:\n%s\n\n", sLogMessage);
	
	// Data serialization
	// 数据序列化
	str sUserName = "john_doe";
	int iUserID = 12345;
	float fBalance = 1250.75f;
	
	str sUserData = xrtFormat("{\"user\":\"%s\",\"id\":%d,\"balance\":%.2f}",
	                          sUserName, iUserID, fBalance);
	
	printf("Serialized user data:\n%s\n\n", sUserData);
	
	// Progress indicator
	// 进度指示器
	int iProgress = 75;
	int iTotal = 100;
	
	str sProgressBar = xrtMalloc(51);  // 50 chars + null
	memset(sProgressBar, '.', 50);
	memset(sProgressBar, '=', iProgress / 2);
	sProgressBar[50] = '\0';
	
	str sProgress = xrtFormat("Progress: [%s] %d%% (%d/%d)",
	                          sProgressBar, iProgress, iProgress, iTotal);
	
	printf("Progress indicator:\n%s\n\n", sProgress);
	
	// Cleanup
	// 清理
	xrtFree(sConfig);
	xrtFree(sLogMessage);
	xrtFree(sUserData);
	xrtFree(sProgressBar);
	xrtFree(sProgress);
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT String Format and Scan Demo\n");
	printf("XRT 字符串格式化与扫描演示\n");
	printf("==============================\n\n");
	
	// Run all tests
	// 运行所有测试
	TestBasicFormatting();
	TestComplexFormatting();
	TestStringToNumberConversion();
	TestNumberToStringConversion();
	TestPracticalExamples();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}