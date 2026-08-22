/*
 * XRT Example - Basic Date Time
 * XRT 范例 - 基本日期时间
 *
 * Description / 说明:
 *   EN: Demonstrates xrtNow, xrtDate, xrtTime, xrtDateSerial, xrtTimeSerial,
 *       xrtDateTimeSerial for basic date/time operations.
 *   CN: 演示 xrtNow、xrtDate、xrtTime、xrtDateSerial、xrtTimeSerial、
 *       xrtDateTimeSerial 基本日期时间操作。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/time_basic_datetime.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/time_basic_datetime              (Linux, GCC)
 *
 * Note / 注意:
 *   - xtime is int64 type representing combined date/time
 *   - xrtNow returns current local date/time
 *   - Serial functions create xtime from components
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test current time functions
// 测试当前时间函数
void TestCurrentTime()
{
	printf("=== Current Time Functions ===\n");
	printf("=== 当前时间函数 ===\n");
	
	// Get current date/time
	// 获取当前日期时间
	xtime tNow = xrtNow();
	xtime tDate = xrtDate();
	xtime tTime = xrtTime();
	
	printf("xrtNow():  %lld (full datetime)\n", tNow);
	printf("xrtDate(): %lld (date only)\n", tDate);
	printf("xrtTime(): %lld (time only)\n", tTime);
	printf("\n");
	
	// String representations
	// 字符串表示
	str sNowStr = xrtNowStr();
	str sDateStr = xrtDateStr();
	str sTimeStr = xrtTimeStr();
	
	printf("xrtNowStr():  %s\n", sNowStr);
	printf("xrtDateStr(): %s\n", sDateStr);
	printf("xrtTimeStr(): %s\n", sTimeStr);
	printf("\n当前时间字符串: %s\n", sNowStr);
	printf("当前日期字符串: %s\n", sDateStr);
	printf("当前时间字符串: %s\n", sTimeStr);
	
	xrtFree(sNowStr);
	xrtFree(sDateStr);
	xrtFree(sTimeStr);
	printf("\n");
}

// Test DateSerial
// 测试 DateSerial
void TestDateSerial()
{
	printf("=== xrtDateSerial ===\n");
	printf("=== xrtDateSerial ===\n");
	
	// Create specific dates
	// 创建特定日期
	xtime tDate1 = xrtDateSerial(2024, 12, 25);  // Christmas 2024
	xtime tDate2 = xrtDateSerial(2000, 1, 1);    // Y2K
	xtime tDate3 = xrtDateSerial(1970, 1, 1);    // Unix epoch
	
	printf("2024-12-25: %lld\n", tDate1);
	printf("2000-01-01: %lld\n", tDate2);
	printf("1970-01-01: %lld\n", tDate3);
	printf("\n");
	
	// Display as strings
	// 显示为字符串
	str sDate1 = xrtTimeToStr(tDate1, 0);
	str sDate2 = xrtTimeToStr(tDate2, 0);
	str sDate3 = xrtTimeToStr(tDate3, 0);
	
	printf("Formatted:\n");
	printf("格式化:\n");
	printf("  2024-12-25: %s\n", sDate1);
	printf("  2000-01-01: %s\n", sDate2);
	printf("  1970-01-01: %s\n", sDate3);
	
	xrtFree(sDate1);
	xrtFree(sDate2);
	xrtFree(sDate3);
	printf("\n");
}

// Test TimeSerial
// 测试 TimeSerial
void TestTimeSerial()
{
	printf("=== xrtTimeSerial ===\n");
	printf("=== xrtTimeSerial ===\n");
	
	// Create specific times
	// 创建特定时间
	xtime tTime1 = xrtTimeSerial(0, 0, 0);     // Midnight
	xtime tTime2 = xrtTimeSerial(12, 0, 0);    // Noon
	xtime tTime3 = xrtTimeSerial(23, 59, 59);  // End of day
	
	printf("00:00:00: %lld\n", tTime1);
	printf("12:00:00: %lld\n", tTime2);
	printf("23:59:59: %lld\n", tTime3);
	printf("\n");
	
	// Display as strings
	// 显示为字符串
	str sTime1 = xrtTimeToStr(tTime1, 0);
	str sTime2 = xrtTimeToStr(tTime2, 0);
	str sTime3 = xrtTimeToStr(tTime3, 0);
	
	printf("Formatted:\n");
	printf("格式化:\n");
	printf("  Midnight: %s\n", sTime1);
	printf("  Noon: %s\n", sTime2);
	printf("  End of day: %s\n", sTime3);
	
	xrtFree(sTime1);
	xrtFree(sTime2);
	xrtFree(sTime3);
	printf("\n");
}

// Test DateTimeSerial
// 测试 DateTimeSerial
void TestDateTimeSerial()
{
	printf("=== xrtDateTimeSerial ===\n");
	printf("=== xrtDateTimeSerial ===\n");
	
	// Create full date/time values
	// 创建完整日期时间值
	xtime t1 = xrtDateTimeSerial(2024, 12, 25, 8, 30, 0);   // Christmas morning
	xtime t2 = xrtDateTimeSerial(2024, 1, 1, 0, 0, 0);     // New Year
	xtime t3 = xrtDateTimeSerial(2024, 7, 4, 12, 0, 0);    // July 4th noon
	
	printf("2024-12-25 08:30:00: %lld\n", t1);
	printf("2024-01-01 00:00:00: %lld\n", t2);
	printf("2024-07-04 12:00:00: %lld\n", t3);
	printf("\n");
	
	// Display formatted
	// 格式化显示
	str s1 = xrtTimeToStr(t1, 0);
	str s2 = xrtTimeToStr(t2, 0);
	str s3 = xrtTimeToStr(t3, 0);
	
	printf("Formatted:\n");
	printf("格式化:\n");
	printf("  %s\n", s1);
	printf("  %s\n", s2);
	printf("  %s\n", s3);
	
	xrtFree(s1);
	xrtFree(s2);
	xrtFree(s3);
	printf("\n");
}

// Test date/time part extraction
// 测试日期时间部分提取
void TestPartExtraction()
{
	printf("=== Date/Time Part Extraction ===\n");
	printf("=== 日期时间部分提取 ===\n");
	
	xtime tNow = xrtNow();
	xtime tDate = xrtDatePart(tNow);
	xtime tTime = xrtTimePart(tNow);
	
	printf("Full datetime: %lld\n", tNow);
	printf("Date part:     %lld\n", tDate);
	printf("Time part:     %lld\n", tTime);
	printf("\n完整日期时间: %lld\n", tNow);
	printf("日期部分: %lld\n", tDate);
	printf("时间部分: %lld\n", tTime);
	printf("\n");
}

// Test days in month/year
// 测试月/年中的天数
void TestDaysCount()
{
	printf("=== Days in Month/Year ===\n");
	printf("=== 月/年中的天数 ===\n");
	
	// Days in each month of 2024 (leap year)
	// 2024 年每个月的天数 (闰年)
	printf("Days in each month (2024 - leap year):\n");
	printf("每月天数 (2024 - 闰年):\n");
	
	int i;
	for ( i = 1; i <= 12; i++ ) {
		int iDays = xrtDaysInMonth(2024, i);
		printf("  Month %2d: %d days\n", i, iDays);
	}
	printf("\n");
	
	// Compare leap years
	// 比较闰年
	printf("Days in year:\n");
	printf("年天数:\n");
	printf("  2020 (leap): %d days\n", xrtDaysInYear(2020));
	printf("  2021: %d days\n", xrtDaysInYear(2021));
	printf("  2024 (leap): %d days\n", xrtDaysInYear(2024));
	printf("  2100: %d days\n", xrtDaysInYear(2100));
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Time - Basic Date Time Demo\n");
	printf("XRT 时间 - 基本日期时间演示\n");
	printf("===============================\n\n");
	
	TestCurrentTime();
	TestDateSerial();
	TestTimeSerial();
	TestDateTimeSerial();
	TestPartExtraction();
	TestDaysCount();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
