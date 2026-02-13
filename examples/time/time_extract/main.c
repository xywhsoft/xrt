/*
 * XRT Example - Time Extract
 * XRT 范例 - 时间提取
 *
 * Description / 说明:
 *   EN: Demonstrates xrtYear, xrtMonth, xrtDay, xrtHour, xrtMinute, xrtSecond,
 *       xrtWeekday, xrtDayOfYear for extracting time components.
 *   CN: 演示 xrtYear、xrtMonth、xrtDay、xrtHour、xrtMinute、xrtSecond、
 *       xrtWeekday、xrtDayOfYear 提取时间组件。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/time_time_extract.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/time_time_extract              (Linux, GCC)
 *
 * Note / 注意:
 *   - Weekday: 0=Sunday, 1=Monday, ..., 6=Saturday
 *   - Month: 1-12
 *   - Day: 1-31
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Day names
// 星期名称
char* g_psWeekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
char* g_psWeekdaysCN[] = {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};

// Month names
// 月份名称
char* g_psMonths[] = {"", "January", "February", "March", "April", "May", "June",
                      "July", "August", "September", "October", "November", "December"};

// Test basic extraction
// 测试基本提取
void TestBasicExtraction()
{
	printf("=== Basic Time Component Extraction ===\n");
	printf("=== 基本时间组件提取 ===\n");
	
	xtime tNow = xrtNow();
	
	int64 iYear = xrtYear(tNow);
	int iMonth = xrtMonth(tNow);
	int iDay = xrtDay(tNow);
	int iHour = xrtHour(tNow);
	int iMinute = xrtMinute(tNow);
	int iSecond = xrtSecond(tNow);
	int iWeekday = xrtWeekday(tNow);
	int iDayOfYear = xrtDayOfYear(tNow);
	
	printf("Current time components:\n");
	printf("当前时间组件:\n");
	printf("  Year:  %lld\n", iYear);
	printf("  Year (年): %lld\n", iYear);
	printf("  Month: %d (%s)\n", iMonth, g_psMonths[iMonth]);
	printf("  Day:   %d\n", iDay);
	printf("  Hour:  %d\n", iHour);
	printf("  Minute: %d\n", iMinute);
	printf("  Second: %d\n", iSecond);
	printf("  Weekday: %d (%s / %s)\n", iWeekday, g_psWeekdays[iWeekday], g_psWeekdaysCN[iWeekday]);
	printf("  Day of Year: %d\n", iDayOfYear);
	printf("\n");
}

// Test specific dates
// 测试特定日期
void TestSpecificDates()
{
	printf("=== Extracting from Specific Dates ===\n");
	printf("=== 从特定日期提取 ===\n");
	
	// Christmas 2024
	// 2024 年圣诞节
	xtime tChristmas = xrtDateTimeSerial(2024, 12, 25, 10, 30, 0);
	
	printf("2024-12-25 10:30:00:\n");
	printf("  Year: %lld, Month: %d, Day: %d\n", 
	       xrtYear(tChristmas), xrtMonth(tChristmas), xrtDay(tChristmas));
	printf("  Hour: %d, Minute: %d, Second: %d\n",
	       xrtHour(tChristmas), xrtMinute(tChristmas), xrtSecond(tChristmas));
	printf("  Weekday: %s (%s)\n", 
	       g_psWeekdays[xrtWeekday(tChristmas)], g_psWeekdaysCN[xrtWeekday(tChristmas)]);
	printf("  Day of Year: %d\n\n", xrtDayOfYear(tChristmas));
	
	// New Year 2024
	// 2024 年元旦
	xtime tNewYear = xrtDateTimeSerial(2024, 1, 1, 0, 0, 0);
	
	printf("2024-01-01 00:00:00:\n");
	printf("  Weekday: %s (%s)\n",
	       g_psWeekdays[xrtWeekday(tNewYear)], g_psWeekdaysCN[xrtWeekday(tNewYear)]);
	printf("  Day of Year: %d\n\n", xrtDayOfYear(tNewYear));
}

// Test weekday calculations
// 测试星期计算
void TestWeekdayCalc()
{
	printf("=== Weekday Calculations ===\n");
	printf("=== 星期计算 ===\n");
	
	printf("First day of each month in 2024:\n");
	printf("2024 年每月第一天:\n");
	
	int i;
	for ( i = 1; i <= 12; i++ ) {
		xtime tDate = xrtDateSerial(2024, i, 1);
		int iWeekday = xrtWeekday(tDate);
		printf("  Month %2d: %s (%s)\n", i, g_psWeekdays[iWeekday], g_psWeekdaysCN[iWeekday]);
	}
	printf("\n");
}

// Test day of year
// 测试年日
void TestDayOfYear()
{
	printf("=== Day of Year Examples ===\n");
	printf("=== 年日示例 ===\n");
	
	// First day of year
	// 年初
	xtime tJan1 = xrtDateSerial(2024, 1, 1);
	printf("Jan 1: Day %d of year\n", xrtDayOfYear(tJan1));
	printf("1月1日: 年第 %d 天\n", xrtDayOfYear(tJan1));
	
	// Last day of year
	// 年末
	xtime tDec31 = xrtDateSerial(2024, 12, 31);
	printf("Dec 31: Day %d of year\n", xrtDayOfYear(tDec31));
	printf("12月31日: 年第 %d 天\n", xrtDayOfYear(tDec31));
	
	// Leap year check
	// 闰年检查
	xtime tFeb29 = xrtDateSerial(2024, 2, 29);
	xtime tMar1 = xrtDateSerial(2024, 3, 1);
	printf("Feb 29, 2024: Day %d (leap year)\n", xrtDayOfYear(tFeb29));
	printf("Mar 1, 2024: Day %d\n", xrtDayOfYear(tMar1));
	printf("2024年2月29日: 年第 %d 天 (闰年)\n", xrtDayOfYear(tFeb29));
	printf("2024年3月1日: 年第 %d 天\n", xrtDayOfYear(tMar1));
	printf("\n");
}

// Test practical use cases
// 测试实际用例
void TestPracticalUseCases()
{
	printf("=== Practical Use Cases ===\n");
	printf("=== 实际用例 ===\n");
	
	// Check if weekend
	// 检查是否周末
	xtime tNow = xrtNow();
	int iWeekday = xrtWeekday(tNow);
	
	if ( iWeekday == 0 || iWeekday == 6 ) {
		printf("Today is weekend! (%s)\n", g_psWeekdays[iWeekday]);
		printf("今天是周末! (%s)\n", g_psWeekdaysCN[iWeekday]);
	} else {
		printf("Today is weekday. (%s)\n", g_psWeekdays[iWeekday]);
		printf("今天是工作日。(%s)\n", g_psWeekdaysCN[iWeekday]);
	}
	
	// Progress through year
	// 年度进度
	int iDayOfYear = xrtDayOfYear(tNow);
	int iTotalDays = xrtDaysInYear((int)xrtYear(tNow));
	float fProgress = (float)iDayOfYear / iTotalDays * 100.0f;
	
	printf("\nYear progress: %d/%d days (%.1f%%)\n", iDayOfYear, iTotalDays, fProgress);
	printf("年度进度: %d/%d 天 (%.1f%%)\n", iDayOfYear, iTotalDays, fProgress);
	
	// Time of day greeting
	// 一天中的问候
	int iHour = xrtHour(tNow);
	printf("\n");
	if ( iHour < 6 ) {
		printf("Good night! / 晚安!\n");
	} else if ( iHour < 12 ) {
		printf("Good morning! / 早上好!\n");
	} else if ( iHour < 18 ) {
		printf("Good afternoon! / 下午好!\n");
	} else {
		printf("Good evening! / 晚上好!\n");
	}
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Time - Time Extract Demo\n");
	printf("XRT 时间 - 时间提取演示\n");
	printf("============================\n\n");
	
	TestBasicExtraction();
	TestSpecificDates();
	TestWeekdayCalc();
	TestDayOfYear();
	TestPracticalUseCases();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
