/*
 * XRT Example - Timezone
 * XRT 范例 - 时区
 *
 * Description / 说明:
 *   EN: Demonstrates xrtNowUTC, xrtTimezoneOffset, xrtTimeToUTC,
 *       xrtTimeToLocal for timezone conversions.
 *   CN: 演示 xrtNowUTC、xrtTimezoneOffset、xrtTimeToUTC、xrtTimeToLocal 进行时区转换。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/time_timezone.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/time_timezone              (Linux, GCC)
 *
 * Note / 注意:
 *   - xrtTimezoneOffset returns offset in seconds
 *   - Positive offset = east of UTC
 *   - Functions convert between local and UTC time
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

// Test timezone offset
// 测试时区偏移
void TestTimezoneOffset()
{
	printf("=== Timezone Offset ===\n");
	printf("=== 时区偏移 ===\n");
	
	int iOffset = xrtTimezoneOffset();
	int iHours = iOffset / 3600;
	int iMinutes = (iOffset % 3600) / 60;
	
	printf("Timezone offset: %d seconds\n", iOffset);
	printf("时区偏移: %d 秒\n", iOffset);
	printf("UTC%+03d:%02d\n", iHours, abs(iMinutes));
	printf("\n");
}

// Test UTC vs Local time
// 测试 UTC 与本地时间
void TestUTCvsLocal()
{
	printf("=== UTC vs Local Time ===\n");
	printf("=== UTC 与本地时间 ===\n");
	
	xtime tLocal = xrtNow();
	xtime tUTC = xrtNowUTC();
	
	str sLocal = xrtTimeToStr(tLocal, 0);
	str sUTC = xrtTimeToStr(tUTC, 0);
	
	printf("Local time: %s\n", sLocal);
	printf("UTC time:   %s\n", sUTC);
	printf("本地时间: %s\n", sLocal);
	printf("UTC 时间: %s\n", sUTC);
	
	// Calculate difference
	// 计算差异
	int64 iDiff = tLocal - tUTC;
	int iDiffHours = (int)(iDiff / 3600);
	printf("\nDifference: %d hours\n", iDiffHours);
	printf("差异: %d 小时\n", iDiffHours);
	
	xrtFree(sLocal);
	xrtFree(sUTC);
	printf("\n");
}

// Test UTC conversion
// 测试 UTC 转换
void TestUTCConversion()
{
	printf("=== UTC Conversion ===\n");
	printf("=== UTC 转换 ===\n");
	
	// Create a specific local time
	// 创建特定的本地时间
	xtime tLocal = xrtDateTimeSerial(2024, 12, 25, 12, 0, 0);
	str sLocal = xrtTimeToStr(tLocal, 0);
	printf("Local time: %s\n", sLocal);
	printf("本地时间: %s\n", sLocal);
	
	// Convert to UTC
	// 转换为 UTC
	xtime tUTC = xrtLocalToUTC(tLocal);
	str sUTC = xrtTimeToStr(tUTC, 0);
	printf("As UTC:     %s\n", sUTC);
	printf("作为 UTC: %s\n", sUTC);
	
	// Convert back to local
	// 转回本地
	xtime tBack = xrtUTCToLocal(tUTC);
	str sBack = xrtTimeToStr(tBack, 0);
	printf("Back to local: %s\n", sBack);
	printf("转回本地: %s\n", sBack);
	
	// Verify
	// 验证
	printf("\nRound-trip match: %s\n", tLocal == tBack ? "YES" : "NO");
	printf("往返匹配: %s\n", tLocal == tBack ? "是" : "否");
	
	xrtFree(sLocal);
	xrtFree(sUTC);
	xrtFree(sBack);
	printf("\n");
}

// Test timezone scenarios
// 测试时区场景
void TestTimezoneScenarios()
{
	printf("=== Timezone Scenarios ===\n");
	printf("=== 时区场景 ===\n");
	
	// Meeting across timezones
	// 跨时区会议
	printf("Scenario: Schedule meeting at 3 PM local time\n");
	printf("场景: 在本地时间下午 3 点安排会议\n\n");
	
	xtime tMeeting = xrtDateTimeSerial(2024, 12, 25, 15, 0, 0);
	xtime tMeetingUTC = xrtLocalToUTC(tMeeting);
	
	str sLocal = xrtTimeToStr(tMeeting, 0);
	str sUTC = xrtTimeToStr(tMeetingUTC, 0);
	
	printf("Meeting time (local): %s\n", sLocal);
	printf("Meeting time (UTC):   %s\n", sUTC);
	printf("会议时间 (本地): %s\n", sLocal);
	printf("会议时间 (UTC): %s\n", sUTC);
	
	printf("\nShare UTC time with participants in other timezones.\n");
	printf("与其他时区的参与者分享 UTC 时间。\n");
	
	xrtFree(sLocal);
	xrtFree(sUTC);
	printf("\n");
}

// Test practical use cases
// 测试实际用例
void TestPracticalUseCases()
{
	printf("=== Practical Use Cases ===\n");
	printf("=== 实际用例 ===\n");
	
	// Log with UTC timestamp
	// 带 UTC 时间戳的日志
	xtime tNow = xrtNow();
	xtime tNowUTC = xrtNowUTC();
	
	str sLocalStr = xrtTimeFormat(tNow, "yyyy-MM-dd HH:mm:ss");
	str sUTCStr = xrtTimeFormat(tNowUTC, "yyyy-MM-dd HH:mm:ss");
	
	printf("Log entry:\n");
	printf("  Local: [%s] Event occurred\n", sLocalStr);
	printf("  UTC:   [%sZ] Event occurred\n", sUTCStr);
	printf("日志条目:\n");
	printf("  本地: [%s] 事件发生\n", sLocalStr);
	printf("  UTC: [%sZ] 事件发生\n", sUTCStr);
	
	xrtFree(sLocalStr);
	xrtFree(sUTCStr);
	printf("\n");
	
	// Display timezone info
	// 显示时区信息
	int iOffset = xrtTimezoneOffset();
	int iHours = iOffset / 3600;
	
	printf("System timezone: UTC%+03d:00\n", iHours);
	printf("系统时区: UTC%+03d:00\n", iHours);
	printf("\n");
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Time - Timezone Demo\n");
	printf("XRT 时间 - 时区演示\n");
	printf("========================\n\n");
	
	TestTimezoneOffset();
	TestUTCvsLocal();
	TestUTCConversion();
	TestTimezoneScenarios();
	TestPracticalUseCases();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}
