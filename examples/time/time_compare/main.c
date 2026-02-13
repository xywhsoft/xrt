/*
 * XRT Example - Time Compare
 * XRT 范例 - 时间比较
 *
 * Description / 说明:
 *   EN: Demonstrates xrtIsSameDay, xrtIsSameMonth, xrtIsSameYear,
 *       xrtTimeInRange, xrtTimeRangeOverlap for time comparisons.
 *   CN: 演示 xrtIsSameDay、xrtIsSameMonth、xrtIsSameYear、xrtTimeInRange、
 *       xrtTimeRangeOverlap 进行时间比较。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/time_time_compare.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/time_time_compare              (Linux, GCC)
 *
 * Note / 注意:
 *   - xrtTimeApprox uses xCore.iApproxTimeTol threshold
 *   - xrtTimeInRange checks if time is within [start, end]
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test same day/month/year comparisons
// 测试同日/同月/同年比较
void TestSamePeriod()
{
	printf("=== Same Period Comparisons ===\n");
	printf("=== 同时期比较 ===\n");
	
	// Same day, different times
	// 同一天，不同时间
	xtime t1 = xrtDateTimeSerial(2024, 12, 25, 8, 0, 0);
	xtime t2 = xrtDateTimeSerial(2024, 12, 25, 20, 0, 0);
	xtime t3 = xrtDateTimeSerial(2024, 12, 26, 8, 0, 0);
	
	printf("Time 1: 2024-12-25 08:00:00\n");
	printf("Time 2: 2024-12-25 20:00:00\n");
	printf("Time 3: 2024-12-26 08:00:00\n\n");
	
	printf("Same day (t1, t2): %s\n", xrtIsSameDay(t1, t2) ? "YES" : "NO");
	printf("Same day (t1, t3): %s\n", xrtIsSameDay(t1, t3) ? "YES" : "NO");
	printf("同日 (t1, t2): %s\n", xrtIsSameDay(t1, t2) ? "是" : "否");
	printf("同日 (t1, t3): %s\n", xrtIsSameDay(t1, t3) ? "是" : "否");
	printf("\n");
	
	// Same month, different days
	// 同一月，不同天
	xtime t4 = xrtDateTimeSerial(2024, 12, 1, 0, 0, 0);
	xtime t5 = xrtDateTimeSerial(2024, 12, 31, 23, 59, 59);
	xtime t6 = xrtDateTimeSerial(2025, 1, 1, 0, 0, 0);
	
	printf("Time 4: 2024-12-01 00:00:00\n");
	printf("Time 5: 2024-12-31 23:59:59\n");
	printf("Time 6: 2025-01-01 00:00:00\n\n");
	
	printf("Same month (t4, t5): %s\n", xrtIsSameMonth(t4, t5) ? "YES" : "NO");
	printf("Same month (t5, t6): %s\n", xrtIsSameMonth(t5, t6) ? "YES" : "NO");
	printf("同月 (t4, t5): %s\n", xrtIsSameMonth(t4, t5) ? "是" : "否");
	printf("同月 (t5, t6): %s\n", xrtIsSameMonth(t5, t6) ? "是" : "否");
	printf("\n");
	
	// Same year
	// 同年
	printf("Same year (t4, t6): %s\n", xrtIsSameYear(t4, t6) ? "YES" : "NO");
	printf("Same year (t5, t6): %s\n", xrtIsSameYear(t5, t6) ? "YES" : "NO");
	printf("同年 (t4, t6): %s\n", xrtIsSameYear(t4, t6) ? "是" : "否");
	printf("同年 (t5, t6): %s\n", xrtIsSameYear(t5, t6) ? "是" : "否");
	printf("\n");
}

// Test time in range
// 测试时间在范围内
void TestTimeInRange()
{
	printf("=== Time In Range ===\n");
	printf("=== 时间在范围内 ===\n");
	
	xtime tStart = xrtDateTimeSerial(2024, 12, 1, 0, 0, 0);
	xtime tEnd = xrtDateTimeSerial(2024, 12, 31, 23, 59, 59);
	
	xtime tInside1 = xrtDateTimeSerial(2024, 12, 15, 12, 0, 0);
	xtime tInside2 = xrtDateTimeSerial(2024, 12, 1, 0, 0, 0);  // Boundary
	xtime tOutside1 = xrtDateTimeSerial(2024, 11, 30, 23, 59, 59);
	xtime tOutside2 = xrtDateTimeSerial(2025, 1, 1, 0, 0, 0);
	
	printf("Range: 2024-12-01 to 2024-12-31\n\n");
	printf("范围: 2024-12-01 到 2024-12-31\n\n");
	
	printf("2024-12-15 (inside): %s\n", xrtTimeInRange(tInside1, tStart, tEnd) ? "IN RANGE" : "OUT");
	printf("2024-12-01 (boundary): %s\n", xrtTimeInRange(tInside2, tStart, tEnd) ? "IN RANGE" : "OUT");
	printf("2024-11-30 (before): %s\n", xrtTimeInRange(tOutside1, tStart, tEnd) ? "IN RANGE" : "OUT");
	printf("2025-01-01 (after): %s\n", xrtTimeInRange(tOutside2, tStart, tEnd) ? "IN RANGE" : "OUT");
	printf("\n");
}

// Test range overlap
// 测试范围重叠
void TestRangeOverlap()
{
	printf("=== Range Overlap ===\n");
	printf("=== 范围重叠 ===\n");
	
	// Range 1: Dec 1-15
	// 范围 1: 12月 1-15日
	xtime t1Start = xrtDateTimeSerial(2024, 12, 1, 0, 0, 0);
	xtime t1End = xrtDateTimeSerial(2024, 12, 15, 23, 59, 59);
	
	// Range 2: Dec 10-20 (overlaps)
	// 范围 2: 12月 10-20日 (重叠)
	xtime t2Start = xrtDateTimeSerial(2024, 12, 10, 0, 0, 0);
	xtime t2End = xrtDateTimeSerial(2024, 12, 20, 23, 59, 59);
	
	// Range 3: Dec 16-25 (no overlap with range 1)
	// 范围 3: 12月 16-25日 (与范围 1 无重叠)
	xtime t3Start = xrtDateTimeSerial(2024, 12, 16, 0, 0, 0);
	xtime t3End = xrtDateTimeSerial(2024, 12, 25, 23, 59, 59);
	
	// Range 4: Dec 1-15 (exact match)
	// 范围 4: 12月 1-15日 (完全匹配)
	xtime t4Start = xrtDateTimeSerial(2024, 12, 1, 0, 0, 0);
	xtime t4End = xrtDateTimeSerial(2024, 12, 15, 23, 59, 59);
	
	printf("Range 1: Dec 1-15\n");
	printf("Range 2: Dec 10-20\n");
	printf("Range 3: Dec 16-25\n");
	printf("Range 4: Dec 1-15 (same as range 1)\n\n");
	
	printf("Range 1 & 2 overlap: %s\n", 
	       xrtTimeRangeOverlap(t1Start, t1End, t2Start, t2End) ? "YES" : "NO");
	printf("Range 1 & 3 overlap: %s\n", 
	       xrtTimeRangeOverlap(t1Start, t1End, t3Start, t3End) ? "YES" : "NO");
	printf("Range 1 & 4 overlap: %s\n", 
	       xrtTimeRangeOverlap(t1Start, t1End, t4Start, t4End) ? "YES" : "NO");
	printf("\n");
}

// Test approximate time comparison
// 测试近似时间比较
void TestApproxTime()
{
	printf("=== Approximate Time Comparison ===\n");
	printf("=== 近似时间比较 ===\n");
	
	printf("Current tolerance: %lld xtime units\n\n", xCore.iApproxTimeTol);
	printf("当前容差: %lld xtime 单位\n\n", xCore.iApproxTimeTol);
	
	// Create times close together
	// 创建相近的时间
	xtime t1 = xrtDateTimeSerial(2024, 12, 25, 12, 0, 0);
	xtime t2 = xrtDateTimeSerial(2024, 12, 25, 12, 0, 5);   // 5 seconds later
	xtime t3 = xrtDateTimeSerial(2024, 12, 25, 12, 1, 0);   // 1 minute later
	
	printf("Time 1: 12:00:00\n");
	printf("Time 2: 12:00:05\n");
	printf("Time 3: 12:01:00\n\n");
	
	printf("Approx (t1, t2): %s\n", xrtTimeApprox(t1, t2) ? "YES" : "NO");
	printf("Approx (t1, t3): %s\n", xrtTimeApprox(t1, t3) ? "YES" : "NO");
	printf("近似 (t1, t2): %s\n", xrtTimeApprox(t1, t2) ? "是" : "否");
	printf("近似 (t1, t3): %s\n", xrtTimeApprox(t1, t3) ? "是" : "否");
	printf("\n");
}

// Test practical use cases
// 测试实际用例
void TestPracticalUseCases()
{
	printf("=== Practical Use Cases ===\n");
	printf("=== 实际用例 ===\n");
	
	// Check if today is weekend
	// 检查今天是否周末
	xtime tNow = xrtNow();
	int iWeekday = xrtWeekday(tNow);
	
	if ( iWeekday == 0 || iWeekday == 6 ) {
		printf("Today is weekend!\n");
		printf("今天是周末!\n");
	} else {
		printf("Today is a weekday.\n");
		printf("今天是工作日。\n");
	}
	
	// Check if current time is in business hours (9-17)
	// 检查当前时间是否在工作时间 (9-17)
	xtime tDayStart = xrtDatePart(tNow);
	xtime tBizStart = tDayStart + 9 * 3600;   // 9:00 AM
	xtime tBizEnd = tDayStart + 17 * 3600;    // 5:00 PM
	
	if ( xrtTimeInRange(tNow, tBizStart, tBizEnd) ) {
		printf("Currently in business hours (9 AM - 5 PM).\n");
		printf("当前在工作时间 (上午 9 点 - 下午 5 点)。\n");
	} else {
		printf("Outside business hours.\n");
		printf("不在工作时间。\n");
	}
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Time - Time Compare Demo\n");
	printf("XRT 时间 - 时间比较演示\n");
	printf("============================\n\n");
	
	TestSamePeriod();
	TestTimeInRange();
	TestRangeOverlap();
	TestApproxTime();
	TestPracticalUseCases();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
