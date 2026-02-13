/*
 * XRT Example - DateTime Operations
 * XRT 范例 - 日期时间操作
 *
 * Description / 说明:
 *   EN: Demonstrates comprehensive datetime operations including time measurement,
 *       date parsing and formatting, timezone handling, timestamp conversion,
 *       duration calculation, and calendar operations.
 *   CN: 演示全面的日期时间操作，包括时间测量、日期解析和格式化、
 *       时区处理、时间戳转换、持续时间计算和日历操作。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/time_datetime_operations.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/time_datetime_operations -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Cross-platform time functions
 *   - High precision timing measurements
 *   - Various date/time formats support
 *   - Timezone-aware operations
 *   - Duration and interval calculations
 *   - Calendar mathematics
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

// Time measurement structure
// 时间测量结构
typedef struct {
	clock_t clkStart;
	clock_t clkEnd;
	double dElapsedSeconds;
} TimeMeasurement;

// Calendar date structure
// 日历日期结构
typedef struct {
	int iYear;
	int iMonth;
	int iDay;
	int iHour;
	int iMinute;
	int iSecond;
} CalendarDate;

// Initialize time measurement
// 初始化时间测量
void StartTimeMeasurement(TimeMeasurement* pTimer)
{
	pTimer->clkStart = clock();
	pTimer->clkEnd = 0;
	pTimer->dElapsedSeconds = 0.0;
}

// End time measurement
// 结束时间测量
void EndTimeMeasurement(TimeMeasurement* pTimer)
{
	pTimer->clkEnd = clock();
	pTimer->dElapsedSeconds = ((double)(pTimer->clkEnd - pTimer->clkStart)) / CLOCKS_PER_SEC;
}

// Get high precision timestamp
// 获取高精度时间戳
double GetHighPrecisionTime()
{
#ifdef _WIN32
	LARGE_INTEGER frequency, counter;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&counter);
	return (double)counter.QuadPart / (double)frequency.QuadPart;
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec + tv.tv_usec / 1000000.0;
#endif
}

// Parse date string
// 解析日期字符串
CalendarDate* ParseDateString(str sDate)
{
	CalendarDate* pDate = xrtMalloc(sizeof(CalendarDate));
	
	// Try different formats
	// 尝试不同格式
	// Format: YYYY-MM-DD HH:MM:SS
	// 格式：YYYY-MM-DD HH:MM:SS
	if ( sscanf(sDate, "%d-%d-%d %d:%d:%d",
	            &pDate->iYear, &pDate->iMonth, &pDate->iDay,
	            &pDate->iHour, &pDate->iMinute, &pDate->iSecond) == 6 ) {
		return pDate;
	}
	
	// Format: YYYY/MM/DD HH:MM:SS
	// 格式：YYYY/MM/DD HH:MM:SS
	if ( sscanf(sDate, "%d/%d/%d %d:%d:%d",
	            &pDate->iYear, &pDate->iMonth, &pDate->iDay,
	            &pDate->iHour, &pDate->iMinute, &pDate->iSecond) == 6 ) {
		return pDate;
	}
	
	// Format: MM/DD/YYYY HH:MM:SS
	// 格式：MM/DD/YYYY HH:MM:SS
	int iTempMonth, iTempDay, iTempYear;
	if ( sscanf(sDate, "%d/%d/%d %d:%d:%d",
	            &iTempMonth, &iTempDay, &iTempYear,
	            &pDate->iHour, &pDate->iMinute, &pDate->iSecond) == 6 ) {
		pDate->iYear = iTempYear;
		pDate->iMonth = iTempMonth;
		pDate->iDay = iTempDay;
		return pDate;
	}
	
	// Date only formats
	// 仅日期格式
	// Format: YYYY-MM-DD
	// 格式：YYYY-MM-DD
	if ( sscanf(sDate, "%d-%d-%d", &pDate->iYear, &pDate->iMonth, &pDate->iDay) == 3 ) {
		pDate->iHour = 0;
		pDate->iMinute = 0;
		pDate->iSecond = 0;
		return pDate;
	}
	
	// Format: YYYY/MM/DD
	// 格式：YYYY/MM/DD
	if ( sscanf(sDate, "%d/%d/%d", &pDate->iYear, &pDate->iMonth, &pDate->iDay) == 3 ) {
		pDate->iHour = 0;
		pDate->iMinute = 0;
		pDate->iSecond = 0;
		return pDate;
	}
	
	xrtFree(pDate);
	return NULL;
}

// Format date to string
// 格式化日期为字符串
str FormatDate(CalendarDate* pDate, str sFormat)
{
	if ( !pDate ) return NULL;
	
	// ISO 8601 format (default)
	// ISO 8601 格式（默认）
	if ( !sFormat || strcmp(sFormat, "iso") == 0 ) {
		return xrtFormat("%04d-%02d-%02d %02d:%02d:%02d",
		                pDate->iYear, pDate->iMonth, pDate->iDay,
		                pDate->iHour, pDate->iMinute, pDate->iSecond);
	}
	
	// US format
	// 美国格式
	if ( strcmp(sFormat, "us") == 0 ) {
		return xrtFormat("%02d/%02d/%04d %02d:%02d:%02d",
		                pDate->iMonth, pDate->iDay, pDate->iYear,
		                pDate->iHour, pDate->iMinute, pDate->iSecond);
	}
	
	// European format
	// 欧洲格式
	if ( strcmp(sFormat, "eu") == 0 ) {
		return xrtFormat("%02d/%02d/%04d %02d:%02d:%02d",
		                pDate->iDay, pDate->iMonth, pDate->iYear,
		                pDate->iHour, pDate->iMinute, pDate->iSecond);
	}
	
	// Date only
	// 仅日期
	if ( strcmp(sFormat, "date") == 0 ) {
		return xrtFormat("%04d-%02d-%02d",
		                pDate->iYear, pDate->iMonth, pDate->iDay);
	}
	
	// Time only
	// 仅时间
	if ( strcmp(sFormat, "time") == 0 ) {
		return xrtFormat("%02d:%02d:%02d",
		                pDate->iHour, pDate->iMinute, pDate->iSecond);
	}
	
	return NULL;
}

// Calculate days between two dates
// 计算两个日期之间的天数
int DaysBetween(CalendarDate* pDate1, CalendarDate* pDate2)
{
	// Convert dates to time_t for calculation
	// 转换日期为 time_t 进行计算
	struct tm tm1 = {0};
	tm1.tm_year = pDate1->iYear - 1900;
	tm1.tm_mon = pDate1->iMonth - 1;
	tm1.tm_mday = pDate1->iDay;
	tm1.tm_hour = pDate1->iHour;
	tm1.tm_min = pDate1->iMinute;
	tm1.tm_sec = pDate1->iSecond;
	
	struct tm tm2 = {0};
	tm2.tm_year = pDate2->iYear - 1900;
	tm2.tm_mon = pDate2->iMonth - 1;
	tm2.tm_mday = pDate2->iDay;
	tm2.tm_hour = pDate2->iHour;
	tm2.tm_min = pDate2->iMinute;
	tm2.tm_sec = pDate2->iSecond;
	
	time_t t1 = mktime(&tm1);
	time_t t2 = mktime(&tm2);
	
	// Calculate difference in seconds, convert to days
	// 计算秒数差，转换为天数
	double dDiffSeconds = difftime(t2, t1);
	return (int)(dDiffSeconds / (24 * 60 * 60));
}

// Add days to a date
// 向日期添加天数
CalendarDate* AddDays(CalendarDate* pDate, int iDays)
{
	struct tm tm = {0};
	tm.tm_year = pDate->iYear - 1900;
	tm.tm_mon = pDate->iMonth - 1;
	tm.tm_mday = pDate->iDay + iDays;
	tm.tm_hour = pDate->iHour;
	tm.tm_min = pDate->iMinute;
	tm.tm_sec = pDate->iSecond;
	
	// Normalize the date
	// 规范化日期
	time_t t = mktime(&tm);
	
	// Convert back to CalendarDate
	// 转换回 CalendarDate
	struct tm* pResult = localtime(&t);
	
	CalendarDate* pNewDate = xrtMalloc(sizeof(CalendarDate));
	pNewDate->iYear = pResult->tm_year + 1900;
	pNewDate->iMonth = pResult->tm_mon + 1;
	pNewDate->iDay = pResult->tm_mday;
	pNewDate->iHour = pResult->tm_hour;
	pNewDate->iMinute = pResult->tm_min;
	pNewDate->iSecond = pResult->tm_sec;
	
	return pNewDate;
}

// Test time measurement
// 测试时间测量
void TestTimeMeasurement()
{
	printf("=== Time Measurement ===\n");
	printf("=== 时间测量 ===\n");
	
	TimeMeasurement timer;
	
	// Measure simple operation
	// 测量简单操作
	StartTimeMeasurement(&timer);
	
	// Simulate some work
	// 模拟一些工作
	volatile long long iSum = 0;
	for ( long long i = 0; i < 1000000; i++ ) {
		iSum += i;
	}
	
	EndTimeMeasurement(&timer);
	
	printf("Simple loop execution time: %.6f seconds\n", timer.dElapsedSeconds);
	printf("CPU cycles: %ld\n", (long)(timer.clkEnd - timer.clkStart));
	
	// High precision timing
	// 高精度计时
	double dStart = GetHighPrecisionTime();
	
	// More intensive work
	// 更密集的工作
	volatile double dCalc = 1.0;
	for ( int i = 0; i < 100000; i++ ) {
		dCalc = sqrt(dCalc * 2.0 + 1.0);
	}
	
	double dEnd = GetHighPrecisionTime();
	double dDuration = dEnd - dStart;
	
	printf("High precision calculation time: %.9f seconds\n", dDuration);
	printf("Calculated value: %.6f\n", dCalc);
}

// Test date parsing and formatting
// 测试日期解析和格式化
void TestDateParsingFormatting()
{
	printf("\n=== Date Parsing and Formatting ===\n");
	printf("=== 日期解析和格式化 ===\n");
	
	// Test various date formats
	// 测试各种日期格式
	const char* arrTestDates[] = {
		"2024-03-15 14:30:25",
		"2024/03/15 09:15:30",
		"03/15/2024 16:45:10",
		"2024-12-25",
		"2024/01/01"
	};
	
	int iNumTests = sizeof(arrTestDates) / sizeof(arrTestDates[0]);
	
	for ( int i = 0; i < iNumTests; i++ ) {
		CalendarDate* pDate = ParseDateString(arrTestDates[i]);
		if ( pDate ) {
			printf("Parsed: %s\n", arrTestDates[i]);
			printf("  Year: %d, Month: %d, Day: %d\n", 
			       pDate->iYear, pDate->iMonth, pDate->iDay);
			printf("  Hour: %d, Minute: %d, Second: %d\n",
			       pDate->iHour, pDate->iMinute, pDate->iSecond);
			
			// Test formatting
			// 测试格式化
			str sIso = FormatDate(pDate, "iso");
			str sUs = FormatDate(pDate, "us");
			str sEu = FormatDate(pDate, "eu");
			str sDateOnly = FormatDate(pDate, "date");
			str sTimeOnly = FormatDate(pDate, "time");
			
			printf("  ISO format: %s\n", sIso);
			printf("  US format: %s\n", sUs);
			printf("  EU format: %s\n", sEu);
			printf("  Date only: %s\n", sDateOnly);
			printf("  Time only: %s\n", sTimeOnly);
			
			xrtFree(sIso);
			xrtFree(sUs);
			xrtFree(sEu);
			xrtFree(sDateOnly);
			xrtFree(sTimeOnly);
			xrtFree(pDate);
			printf("\n");
		} else {
			printf("Failed to parse: %s\n\n", arrTestDates[i]);
		}
	}
}

// Test date arithmetic
// 测试日期算术
void TestDateArithmetic()
{
	printf("=== Date Arithmetic ===\n");
	printf("=== 日期算术 ===\n");
	
	// Create test dates
	// 创建测试日期
	CalendarDate date1 = {2024, 1, 1, 0, 0, 0};  // Jan 1, 2024
	CalendarDate date2 = {2024, 12, 31, 23, 59, 59};  // Dec 31, 2024
	
	printf("Date 1: %04d-%02d-%02d\n", date1.iYear, date1.iMonth, date1.iDay);
	printf("Date 2: %04d-%02d-%02d\n", date2.iYear, date2.iMonth, date2.iDay);
	
	// Calculate days between
	// 计算间隔天数
	int iDaysBetween = DaysBetween(&date1, &date2);
	printf("Days between: %d\n", iDaysBetween);
	
	// Add days to dates
	// 向日期添加天数
	CalendarDate* pPlus30 = AddDays(&date1, 30);
	CalendarDate* pPlus100 = AddDays(&date1, 100);
	CalendarDate* pMinus15 = AddDays(&date2, -15);
	
	printf("Jan 1 + 30 days: %04d-%02d-%02d\n", 
	       pPlus30->iYear, pPlus30->iMonth, pPlus30->iDay);
	printf("Jan 1 + 100 days: %04d-%02d-%02d\n",
	       pPlus100->iYear, pPlus100->iMonth, pPlus100->iDay);
	printf("Dec 31 - 15 days: %04d-%02d-%02d\n",
	       pMinus15->iYear, pMinus15->iMonth, pMinus15->iDay);
	
	xrtFree(pPlus30);
	xrtFree(pPlus100);
	xrtFree(pMinus15);
	
	// Test leap year calculations
	// 测试闰年计算
	CalendarDate leapTest = {2024, 2, 28, 0, 0, 0};
	CalendarDate* pLeapPlus2 = AddDays(&leapTest, 2);
	printf("Feb 28, 2024 + 2 days: %04d-%02d-%02d (leap year test)\n",
	       pLeapPlus2->iYear, pLeapPlus2->iMonth, pLeapPlus2->iDay);
	xrtFree(pLeapPlus2);
}

// Test current time operations
// 测试当前时间操作
void TestCurrentTime()
{
	printf("\n=== Current Time Operations ===\n");
	printf("=== 当前时间操作 ===\n");
	
	// Get current time
	// 获取当前时间
	time_t tNow = time(NULL);
	struct tm* pLocalTime = localtime(&tNow);
	struct tm* pUtcTime = gmtime(&tNow);
	
	printf("Current local time: %s", asctime(pLocalTime));
	printf("Current UTC time: %s", asctime(pUtcTime));
	
	// Format current time in different ways
	// 以不同方式格式化当前时间
	char sFormatted[100];
	
	// Custom format
	// 自定义格式
	strftime(sFormatted, sizeof(sFormatted), "%Y-%m-%d %H:%M:%S", pLocalTime);
	printf("Custom format: %s\n", sFormatted);
	
	// Date only
	// 仅日期
	strftime(sFormatted, sizeof(sFormatted), "%A, %B %d, %Y", pLocalTime);
	printf("Long format: %s\n", sFormatted);
	
	// Time only
	// 仅时间
	strftime(sFormatted, sizeof(sFormatted), "%I:%M:%S %p", pLocalTime);
	printf("12-hour format: %s\n", sFormatted);
	
	// Timestamp
	// 时间戳
	printf("Unix timestamp: %ld\n", (long)tNow);
}

// Test duration calculations
// 测试持续时间计算
void TestDurationCalculations()
{
	printf("\n=== Duration Calculations ===\n");
	printf("=== 持续时间计算 ===\n");
	
	// Simulate timed operations
	// 模拟定时操作
	printf("Timing various operations:\n");
	
	// Operation 1: String processing
	// 操作1：字符串处理
	clock_t clkStart = clock();
	str sTest = xrtMalloc(10000);
	for ( int i = 0; i < 1000; i++ ) {
		sprintf(sTest, "Test string %d", i);
	}
	clock_t clkEnd = clock();
	double dStringTime = ((double)(clkEnd - clkStart)) / CLOCKS_PER_SEC;
	printf("String processing: %.6f seconds\n", dStringTime);
	xrtFree(sTest);
	
	// Operation 2: Mathematical calculations
	// 操作2：数学计算
	clkStart = clock();
	volatile double dResult = 1.0;
	for ( int i = 0; i < 100000; i++ ) {
		dResult = sin(dResult) * cos(dResult);
	}
	clkEnd = clock();
	double dMathTime = ((double)(clkEnd - clkStart)) / CLOCKS_PER_SEC;
	printf("Mathematical operations: %.6f seconds\n", dMathTime);
	
	// Operation 3: Memory allocation
	// 操作3：内存分配
	clkStart = clock();
	str arrPtrs[1000];
	for ( int i = 0; i < 1000; i++ ) {
		arrPtrs[i] = xrtMalloc(100);
	}
	for ( int i = 0; i < 1000; i++ ) {
		xrtFree(arrPtrs[i]);
	}
	clkEnd = clock();
	double dMemTime = ((double)(clkEnd - clkStart)) / CLOCKS_PER_SEC;
	printf("Memory operations: %.6f seconds\n", dMemTime);
	
	// Compare operations
	// 比较操作
	printf("\nPerformance comparison:\n");
	printf("Fastest: ");
	if ( dStringTime <= dMathTime && dStringTime <= dMemTime ) {
		printf("String processing (%.6f s)\n", dStringTime);
	} else if ( dMathTime <= dStringTime && dMathTime <= dMemTime ) {
		printf("Mathematical operations (%.6f s)\n", dMathTime);
	} else {
		printf("Memory operations (%.6f s)\n", dMemTime);
	}
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT DateTime Operations Demo\n");
	printf("XRT 日期时间操作演示\n");
	printf("=========================\n\n");
	
	// Run all tests
	// 运行所有测试
	TestTimeMeasurement();
	TestDateParsingFormatting();
	TestDateArithmetic();
	TestCurrentTime();
	TestDurationCalculations();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}