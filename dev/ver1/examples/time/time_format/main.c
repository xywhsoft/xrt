/*
 * XRT Example - Time Format and Parse
 * XRT 范例 - 时间格式化与解析
 *
 * Description / 说明:
 *   EN: Demonstrates xrtTimeFormat and xrtTimeParse for formatting and
 *       parsing date/time strings with custom patterns.
 *   CN: 演示 xrtTimeFormat 和 xrtTimeParse 使用自定义模式格式化和解析日期时间字符串。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/time_time_format.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/time_time_format              (Linux, GCC)
 *
 * Note / 注意:
 *   - Format: yyyy=year, MM=month, dd=day, HH=hour, mm=minute, ss=second
 *   - Also supports: MMM/Month, ddd/day, yy/short year, etc.
 *   - xrtTimeToStr has predefined formats
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test basic formatting
// 测试基本格式化
void TestBasicFormatting()
{
	printf("=== Basic Time Formatting ===\n");
	printf("=== 基本时间格式化 ===\n");
	
	xtime tNow = xrtNow();
	
	// Predefined formats
	// 预定义格式
	printf("Predefined formats:\n");
	printf("预定义格式:\n");
	
	str sFormat0 = xrtTimeToStr(tNow, 0);  // Default
	str sFormat1 = xrtTimeToStr(tNow, 1);  // Date only
	str sFormat2 = xrtTimeToStr(tNow, 2);  // Time only
	
	printf("  Format 0 (default): %s\n", sFormat0);
	printf("  Format 1 (date):    %s\n", sFormat1);
	printf("  Format 2 (time):    %s\n", sFormat2);
	printf("\n");
	
	xrtFree(sFormat0);
	xrtFree(sFormat1);
	xrtFree(sFormat2);
}

// Test custom format patterns
// 测试自定义格式模式
void TestCustomFormats()
{
	printf("=== Custom Format Patterns ===\n");
	printf("=== 自定义格式模式 ===\n");
	
	xtime tTime = xrtDateTimeSerial(2024, 12, 25, 14, 30, 45);
	
	char* psFormats[] = {
		"yyyy-MM-dd",
		"dd/MM/yyyy",
		"MM/dd/yyyy",
		"yyyy-MM-dd HH:mm:ss",
		"HH:mm:ss",
		"yyyy年MM月dd日",
		"ddd, MMM dd, yyyy",
		"yyyyMMdd_HHmmss",
	};
	
	char* psDesc[] = {
		"ISO date",
		"European date",
		"US date",
		"Full datetime",
		"Time only",
		"Chinese format",
		"Long format",
		"Compact format",
	};
	
	int iNumFormats = 8;
	int i;
	for ( i = 0; i < iNumFormats; i++ ) {
		str sResult = xrtTimeFormat(tTime, psFormats[i]);
		printf("  %s: %s\n", psDesc[i], sResult);
		xrtFree(sResult);
	}
	printf("\n");
}

// Test time parsing
// 测试时间解析
void TestTimeParsing()
{
	printf("=== Time Parsing ===\n");
	printf("=== 时间解析 ===\n");
	
	// Parse various formats
	// 解析各种格式
	struct {
		char* sTime;
		char* sFormat;
		char* sDesc;
	} sTests[] = {
		{"2024-12-25", "yyyy-MM-dd", "ISO date"},
		{"25/12/2024", "dd/MM/yyyy", "European date"},
		{"12/25/2024", "MM/dd/yyyy", "US date"},
		{"2024-12-25 14:30:00", "yyyy-MM-dd HH:mm:ss", "Full datetime"},
		{"14:30:00", "HH:mm:ss", "Time only"},
	};
	
	int iNumTests = 5;
	int i;
	for ( i = 0; i < iNumTests; i++ ) {
		xtime tParsed = xrtTimeParse(sTests[i].sTime, sTests[i].sFormat);
		str sFormatted = xrtTimeToStr(tParsed, 0);
		
		printf("  %s: \"%s\" -> %s\n", 
		       sTests[i].sDesc, sTests[i].sTime, sFormatted);
		xrtFree(sFormatted);
	}
	printf("\n");
}

// Test format and parse round-trip
// 测试格式化和解析往返
void TestRoundTrip()
{
	printf("=== Format and Parse Round-Trip ===\n");
	printf("=== 格式化和解析往返 ===\n");
	
	xtime tOriginal = xrtDateTimeSerial(2024, 6, 15, 9, 30, 0);
	char* sFormat = "yyyy-MM-dd HH:mm:ss";
	
	printf("Original: ");
	str sOriginal = xrtTimeToStr(tOriginal, 0);
	printf("%s\n", sOriginal);
	printf("原始: %s\n", sOriginal);
	
	// Format
	// 格式化
	str sFormatted = xrtTimeFormat(tOriginal, sFormat);
	printf("Formatted (%s): %s\n", sFormat, sFormatted);
	printf("格式化 (%s): %s\n", sFormat, sFormatted);
	
	// Parse back
	// 解析回来
	xtime tParsed = xrtTimeParse(sFormatted, sFormat);
	str sParsed = xrtTimeToStr(tParsed, 0);
	printf("Parsed back: %s\n", sParsed);
	printf("解析回来: %s\n", sParsed);
	
	// Compare
	// 比较
	printf("Match: %s\n", tOriginal == tParsed ? "YES" : "NO");
	printf("匹配: %s\n", tOriginal == tParsed ? "是" : "否");
	
	xrtFree(sOriginal);
	xrtFree(sFormatted);
	xrtFree(sParsed);
	printf("\n");
}

// Test practical use cases
// 测试实际用例
void TestPracticalUseCases()
{
	printf("=== Practical Use Cases ===\n");
	printf("=== 实际用例 ===\n");
	
	xtime tNow = xrtNow();
	
	// Log timestamp
	// 日志时间戳
	str sLogTime = xrtTimeFormat(tNow, "yyyy-MM-dd HH:mm:ss");
	printf("Log timestamp: [%s]\n", sLogTime);
	printf("日志时间戳: [%s]\n", sLogTime);
	xrtFree(sLogTime);
	
	// Filename safe
	// 文件名安全
	str sFileTime = xrtTimeFormat(tNow, "yyyyMMdd_HHmmss");
	printf("Filename: backup_%s.dat\n", sFileTime);
	printf("文件名: backup_%s.dat\n", sFileTime);
	xrtFree(sFileTime);
	
	// Display format
	// 显示格式
	str sDisplay = xrtTimeFormat(tNow, "dddd, MMMM dd, yyyy");
	printf("Display: %s\n", sDisplay);
	printf("显示: %s\n", sDisplay);
	xrtFree(sDisplay);
	
	// ISO 8601
	// ISO 8601
	str sISO = xrtTimeFormat(tNow, "yyyy-MM-dd'T'HH:mm:ss");
	printf("ISO 8601: %s\n", sISO);
	printf("ISO 8601: %s\n", sISO);
	xrtFree(sISO);
	
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Time - Time Format Demo\n");
	printf("XRT 时间 - 时间格式化演示\n");
	printf("===========================\n\n");
	
	TestBasicFormatting();
	TestCustomFormats();
	TestTimeParsing();
	TestRoundTrip();
	TestPracticalUseCases();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
