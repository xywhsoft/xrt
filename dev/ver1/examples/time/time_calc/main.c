/*
 * XRT Example - Time Calculations
 * XRT 范例 - 时间计算
 *
 * Description / 说明:
 *   EN: Demonstrates xrtDateAdd and xrtDateDiff for date/time arithmetic.
 *   CN: 演示 xrtDateAdd 和 xrtDateDiff 进行日期时间算术运算。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/time_time_calc.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/time_time_calc              (Linux, GCC)
 *
 * Note / 注意:
 *   - Intervals: year, quarter, month, day, hour, minute, second
 *   - xrtDateAdd adds specified interval to date
 *   - xrtDateDiff returns difference in specified units
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Interval names
// 间隔名称
char* g_psIntervals[] = {"", "year", "quarter", "month", "day", "hour", "minute", "second"};
char* g_psIntervalsCN[] = {"", "年", "季度", "月", "日", "小时", "分钟", "秒"};

// Test DateAdd with different intervals
// 测试不同间隔的 DateAdd
void TestDateAdd()
{
	printf("=== xrtDateAdd (Date Addition) ===\n");
	printf("=== xrtDateAdd (日期加法) ===\n");
	
	xtime tBase = xrtDateSerial(2024, 6, 15);
	str sBase = xrtTimeToStr(tBase, 0);
	printf("Base date: %s\n\n", sBase);
	printf("基准日期: %s\n\n", sBase);
	
	// Add years
	// 加年
	xtime t1 = xrtDateAdd(XRT_TIME_INTERVAL_YEAR, 1, tBase);
	str s1 = xrtTimeToStr(t1, 0);
	printf("  +1 year:   %s\n", s1);
	printf("  +1 年: %s\n", s1);
	xrtFree(s1);
	
	// Add months
	// 加月
	xtime t2 = xrtDateAdd(XRT_TIME_INTERVAL_MONTH, 3, tBase);
	str s2 = xrtTimeToStr(t2, 0);
	printf("  +3 months: %s\n", s2);
	printf("  +3 月: %s\n", s2);
	xrtFree(s2);
	
	// Add days
	// 加日
	xtime t3 = xrtDateAdd(XRT_TIME_INTERVAL_DAY, 30, tBase);
	str s3 = xrtTimeToStr(t3, 0);
	printf("  +30 days:  %s\n", s3);
	printf("  +30 日: %s\n", s3);
	xrtFree(s3);
	
	// Add quarters
	// 加季度
	xtime t4 = xrtDateAdd(XRT_TIME_INTERVAL_QUARTER, 2, tBase);
	str s4 = xrtTimeToStr(t4, 0);
	printf("  +2 quarters: %s\n", s4);
	printf("  +2 季度: %s\n", s4);
	xrtFree(s4);
	
	xrtFree(sBase);
	printf("\n");
}

// Test DateAdd with negative values (subtraction)
// 测试负值的 DateAdd (减法)
void TestDateSubtract()
{
	printf("=== DateAdd with Negative Values ===\n");
	printf("=== 负值的 DateAdd ===\n");
	
	xtime tBase = xrtDateSerial(2024, 6, 15);
	str sBase = xrtTimeToStr(tBase, 0);
	printf("Base date: %s\n\n", sBase);
	printf("基准日期: %s\n\n", sBase);
	
	// Subtract years
	// 减年
	xtime t1 = xrtDateAdd(XRT_TIME_INTERVAL_YEAR, -1, tBase);
	str s1 = xrtTimeToStr(t1, 0);
	printf("  -1 year:   %s\n", s1);
	printf("  -1 年: %s\n", s1);
	xrtFree(s1);
	
	// Subtract months
	// 减月
	xtime t2 = xrtDateAdd(XRT_TIME_INTERVAL_MONTH, -6, tBase);
	str s2 = xrtTimeToStr(t2, 0);
	printf("  -6 months: %s\n", s2);
	printf("  -6 月: %s\n", s2);
	xrtFree(s2);
	
	// Subtract days
	// 减日
	xtime t3 = xrtDateAdd(XRT_TIME_INTERVAL_DAY, -100, tBase);
	str s3 = xrtTimeToStr(t3, 0);
	printf("  -100 days: %s\n", s3);
	printf("  -100 日: %s\n", s3);
	xrtFree(s3);
	
	xrtFree(sBase);
	printf("\n");
}

// Test DateDiff
// 测试 DateDiff
void TestDateDiff()
{
	printf("=== xrtDateDiff (Date Difference) ===\n");
	printf("=== xrtDateDiff (日期差值) ===\n");
	
	xtime tDate1 = xrtDateSerial(2024, 1, 1);
	xtime tDate2 = xrtDateSerial(2024, 12, 31);
	
	str s1 = xrtTimeToStr(tDate1, 0);
	str s2 = xrtTimeToStr(tDate2, 0);
	printf("Date1: %s\n", s1);
	printf("Date2: %s\n\n", s2);
	printf("日期1: %s\n", s1);
	printf("日期2: %s\n\n", s2);
	
	// Difference in days
	// 天数差
	int64 iDays = xrtDateDiff(XRT_TIME_INTERVAL_DAY, tDate1, tDate2);
	printf("Difference in days: %lld\n", iDays);
	printf("天数差: %lld\n", iDays);
	
	// Difference in months
	// 月数差
	int64 iMonths = xrtDateDiff(XRT_TIME_INTERVAL_MONTH, tDate1, tDate2);
	printf("Difference in months: %lld\n", iMonths);
	printf("月数差: %lld\n", iMonths);
	
	// Difference in years
	// 年数差
	int64 iYears = xrtDateDiff(XRT_TIME_INTERVAL_YEAR, tDate1, tDate2);
	printf("Difference in years: %lld\n", iYears);
	printf("年数差: %lld\n", iYears);
	
	xrtFree(s1);
	xrtFree(s2);
	printf("\n");
}

// Test time difference
// 测试时间差
void TestTimeDiff()
{
	printf("=== Time Difference (Hours/Minutes/Seconds) ===\n");
	printf("=== 时间差 (小时/分钟/秒) ===\n");
	
	xtime tTime1 = xrtDateTimeSerial(2024, 6, 15, 8, 30, 0);
	xtime tTime2 = xrtDateTimeSerial(2024, 6, 15, 17, 45, 30);
	
	str s1 = xrtTimeToStr(tTime1, 0);
	str s2 = xrtTimeToStr(tTime2, 0);
	printf("Time1: %s\n", s1);
	printf("Time2: %s\n\n", s2);
	
	// Difference in hours
	// 小时差
	int64 iHours = xrtDateDiff(XRT_TIME_INTERVAL_HOUR, tTime1, tTime2);
	printf("Difference in hours: %lld\n", iHours);
	printf("小时差: %lld\n", iHours);
	
	// Difference in minutes
	// 分钟差
	int64 iMinutes = xrtDateDiff(XRT_TIME_INTERVAL_MINUTE, tTime1, tTime2);
	printf("Difference in minutes: %lld\n", iMinutes);
	printf("分钟差: %lld\n", iMinutes);
	
	// Difference in seconds
	// 秒差
	int64 iSeconds = xrtDateDiff(XRT_TIME_INTERVAL_SECOND, tTime1, tTime2);
	printf("Difference in seconds: %lld\n", iSeconds);
	printf("秒差: %lld\n", iSeconds);
	
	xrtFree(s1);
	xrtFree(s2);
	printf("\n");
}

// Test practical use cases
// 测试实际用例
void TestPracticalUseCases()
{
	printf("=== Practical Use Cases ===\n");
	printf("=== 实际用例 ===\n");
	
	// Calculate age
	// 计算年龄
	xtime tBirth = xrtDateSerial(1990, 5, 15);
	xtime tNow = xrtNow();
	int64 iAge = xrtDateDiff(XRT_TIME_INTERVAL_YEAR, tBirth, tNow);
	printf("Birth date: 1990-05-15\n");
	printf("出生日期: 1990-05-15\n");
	printf("Age: %lld years\n", iAge);
	printf("年龄: %lld 岁\n\n", iAge);
	
	// Days until Christmas
	// 到圣诞节的天数
	xtime tChristmas = xrtDateSerial(2024, 12, 25);
	int64 iDaysToChristmas = xrtDateDiff(XRT_TIME_INTERVAL_DAY, tNow, tChristmas);
	printf("Days until Christmas 2024: %lld\n", iDaysToChristmas);
	printf("到 2024 年圣诞节的天数: %lld\n\n", iDaysToChristmas);
	
	// Add business days (5 working days)
	// 添加工作日 (5 个工作日)
	xtime tDeadline = xrtDateAdd(XRT_TIME_INTERVAL_DAY, 5, tNow);
	str sDeadline = xrtTimeToStr(tDeadline, 0);
	printf("Deadline (5 days from now): %s\n", sDeadline);
	printf("截止日期 (5 天后): %s\n", sDeadline);
	xrtFree(sDeadline);
	
	printf("\n");
}

// Test edge cases
// 测试边界情况
void TestEdgeCases()
{
	printf("=== Edge Cases ===\n");
	printf("=== 边界情况 ===\n");
	
	// Month-end roll over
	// 月末滚动
	xtime tJan31 = xrtDateSerial(2024, 1, 31);
	xtime tPlus1Month = xrtDateAdd(XRT_TIME_INTERVAL_MONTH, 1, tJan31);
	
	str sJan31 = xrtTimeToStr(tJan31, 0);
	str sPlus1Month = xrtTimeToStr(tPlus1Month, 0);
	printf("Jan 31 + 1 month: %s -> %s\n", sJan31, sPlus1Month);
	printf("1月31日 + 1月: %s -> %s\n", sJan31, sPlus1Month);
	
	// Leap year handling
	// 闰年处理
	xtime tFeb28_2023 = xrtDateSerial(2023, 2, 28);
	xtime tFeb28_2024 = xrtDateSerial(2024, 2, 28);
	xtime tPlus1Day2023 = xrtDateAdd(XRT_TIME_INTERVAL_DAY, 1, tFeb28_2023);
	xtime tPlus1Day2024 = xrtDateAdd(XRT_TIME_INTERVAL_DAY, 1, tFeb28_2024);
	
	str sPlus1Day2023 = xrtTimeToStr(tPlus1Day2023, 0);
	str sPlus1Day2024 = xrtTimeToStr(tPlus1Day2024, 0);
	printf("Feb 28, 2023 + 1 day: %s\n", sPlus1Day2023);
	printf("Feb 28, 2024 + 1 day: %s (leap year)\n", sPlus1Day2024);
	printf("2023年2月28日 + 1日: %s\n", sPlus1Day2023);
	printf("2024年2月28日 + 1日: %s (闰年)\n", sPlus1Day2024);
	
	xrtFree(sJan31);
	xrtFree(sPlus1Month);
	xrtFree(sPlus1Day2023);
	xrtFree(sPlus1Day2024);
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Time - Time Calculations Demo\n");
	printf("XRT 时间 - 时间计算演示\n");
	printf("=================================\n\n");
	
	TestDateAdd();
	TestDateSubtract();
	TestDateDiff();
	TestTimeDiff();
	TestPracticalUseCases();
	TestEdgeCases();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
