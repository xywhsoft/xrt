#include "../test.h"



/* 从调用方缓冲构造不依赖 string 模块的视图。 */
static xstrview testMailDateView(const char* sText, size_t iSize)
{
	xstrview Text;

	Text.Data = sText;
	Text.Size = iSize;
	return Text;
}



/* 构造带固定时区的日期测试值。 */
static xtime testMailDateValue(void)
{
	xdatetime DateTime;
	xtime iTime = 0;

	memset(&DateTime, 0, sizeof(DateTime));
	DateTime.Year = 2024;
	DateTime.Month = 1;
	DateTime.Day = 2;
	DateTime.Hour = 3;
	DateTime.Minute = 4;
	DateTime.Second = 5;
	DateTime.Offset = 8 * 60 * 60;
	testRequire(xrtTimeMake(&DateTime, &iTime), "mail date setup failed");
	return iTime;
}



/* 验证规范写入、可选星期/秒解析和偏移保留。 */
static void testMailDateRoundtrip(void)
{
	char arrOutput[64];
	xtime iTime = testMailDateValue();
	xtime iParsed = 0;
	int iOffset = 0;
	size_t iSize = 0;

	testRequire(xrtMailDateWrite(
		iTime,
		8 * 60 * 60,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, "Tue, 02 Jan 2024 03:04:05 +0800") == 0),
		"mail date canonical output mismatch");
	testRequire(xrtMailDateParse(
		testMailDateView(arrOutput, iSize),
		&iParsed,
		&iOffset
	) && (iParsed == iTime) && (iOffset == (8 * 60 * 60)),
		"mail date roundtrip mismatch");
	testRequire(xrtMailDateParse(
		XRT_STR_LITERAL("2 Jan 2024 03:04 +0800"),
		&iParsed,
		&iOffset
	) && (iOffset == (8 * 60 * 60)),
		"mail date optional weekday or seconds rejected");
}



/* 验证范围、星期一致性和事务式短缓冲。 */
static void testMailDateErrors(void)
{
	char arrOutput[16] = "keep";
	xtime iTime = testMailDateValue();
	xtime iParsed = 47;
	int iOffset = 59;
	size_t iSize = 0;

	testRequire(!xrtMailDateWrite(
		iTime,
		1,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	), "mail date accepted a sub-minute offset");
	memcpy(arrOutput, "keep", 5u);
	testRequire(!xrtMailDateWrite(
		iTime,
		0,
		arrOutput,
		8u,
		&iSize
	) && (memcmp(arrOutput, "keep", 5u) == 0) && (iSize > 8u),
		"mail date short buffer published partial output");
	testRequire(!xrtMailDateParse(
		XRT_STR_LITERAL("Mon, 02 Jan 2024 03:04:05 +0800"),
		&iParsed,
		&iOffset
	) && (iParsed == 47) && (iOffset == 59),
		"mail date accepted a mismatched weekday or modified outputs");
}



/* 运行邮件日期全部契约测试。 */
int main(void)
{
	testMailDateRoundtrip();
	testMailDateErrors();
	return 0;
}
