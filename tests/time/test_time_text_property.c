#include "../test.h"



/* 固定种子生成可重复时间文本样本。 */
static uint64 testTimeTextRandom(uint64* pState)
{
	uint64 iValue = *pState;

	iValue ^= iValue << 13u;
	iValue ^= iValue >> 7u;
	iValue ^= iValue << 17u;
	*pState = iValue;
	return iValue;
}



/* 自定义完整格式必须覆盖 xtime 全域和全部分钟级固定偏移。 */
static void testTimeTextRoundtripProperty(void)
{
	const xstrview Format = XRT_STR_LITERAL("%Y-%m-%dT%H:%M:%S.%f%:z");
	uint64 iState = UINT64_C(0xA54FF53A5F1D36F1);
	char sText[96];

	for ( size_t i = 0; i < 250000u; i++ ) {
		xtime iTime = (xtime)testTimeTextRandom(&iState);
		int iOffsetMinutes =
			(int)(testTimeTextRandom(&iState) % UINT64_C(2879)) - 1439;
		int iOffset = iOffsetMinutes * 60;
		size_t iSize = xrtTimeWrite(
			sText, sizeof(sText), iTime, iOffset, Format);
		xtime iRoundtrip = 0;

		testRequire((iSize != XRT_NPOS) && (iSize < sizeof(sText)),
			"random custom time format failed");
		testRequire(xrtTimeParse(
			(xstrview){ sText, iSize }, Format, &iRoundtrip) &&
			(iRoundtrip == iTime),
			"random custom time format/parse roundtrip failed");
	}
}



/* RFC 3339 必须无损往返任意合法字段、微秒和分钟级偏移。 */
static void testTimeRFC3339Property(void)
{
	uint64 iState = UINT64_C(0x510E527FADE682D1);
	char sText[64];

	for ( size_t i = 0; i < 200000u; i++ ) {
		xdatetime tDateTime;
		xtime iTime;
		xtime iRoundtrip = 0;
		int iOffsetMinutes =
			(int)(testTimeTextRandom(&iState) % UINT64_C(2879)) - 1439;
		size_t iSize;

		memset(&tDateTime, 0, sizeof(tDateTime));
		tDateTime.Year = (int64)(testTimeTextRandom(&iState) %
			UINT64_C(10000));
		tDateTime.Month = (int)(testTimeTextRandom(&iState) % 12u) + 1;
		tDateTime.Day = (int)(testTimeTextRandom(&iState) %
			(uint64)xrtDaysInMonth(tDateTime.Year, tDateTime.Month)) + 1;
		tDateTime.Hour = (int)(testTimeTextRandom(&iState) % 24u);
		tDateTime.Minute = (int)(testTimeTextRandom(&iState) % 60u);
		tDateTime.Second = (int)(testTimeTextRandom(&iState) % 60u);
		tDateTime.Microsecond =
			(int)(testTimeTextRandom(&iState) % UINT64_C(1000000));
		tDateTime.Offset = iOffsetMinutes * 60;
		testRequire(xrtTimeMake(&tDateTime, &iTime),
			"random RFC 3339 source construction failed");
		iSize = xrtTimeWriteRFC3339(
			sText, sizeof(sText), iTime, tDateTime.Offset);
		testRequire((iSize != XRT_NPOS) && (iSize < sizeof(sText)),
			"random RFC 3339 format failed");
		testRequire(xrtTimeParseRFC3339(
			(xstrview){ sText, iSize }, &iRoundtrip) &&
			(iRoundtrip == iTime),
			"random RFC 3339 roundtrip failed");
	}
}



/* 有界写入必须始终返回完整长度、保留正确前缀并可靠终止。 */
static void testTimeBoundedWriteProperty(void)
{
	const xstrview Format = XRT_STR_LITERAL("%F %T.%f %:z");
	xtime iTime;
	char sFull[64];
	char sShort[64];
	size_t iSize;

	testRequire(xrtDateTime(2024, 2, 29, 23, 58, 57, 654321, &iTime),
		"bounded-write source construction failed");
	iSize = xrtTimeWrite(
		sFull, sizeof(sFull), iTime, 8 * 3600, Format);
	testRequire((iSize != XRT_NPOS) && (iSize < sizeof(sFull)),
		"bounded-write full format failed");
	for ( size_t iCapacity = 1; iCapacity <= (iSize + 1u); iCapacity++ ) {
		memset(sShort, 0xA5, sizeof(sShort));
		testRequire(xrtTimeWrite(
			sShort, iCapacity, iTime, 8 * 3600, Format) == iSize,
			"bounded-write size changed with capacity");
		testRequire(sShort[iCapacity - 1u] == '\0',
			"bounded-write result is not terminated");
		testRequire(memcmp(
			sShort, sFull, iCapacity - 1u) == 0,
			"bounded-write prefix mismatch");
	}
}



/* HTTP-date 必须按秒精度往返全部四位 Gregorian 年份。 */
static void testTimeHTTPDateProperty(void)
{
	uint64 iState = UINT64_C(0x1F83D9ABFB41BD6B);
	char sText[32];

	for ( size_t i = 0; i < 100000u; i++ ) {
		xdatetime tDateTime;
		xtime iTime;
		xtime iRoundtrip = 0;
		size_t iSize;

		memset(&tDateTime, 0, sizeof(tDateTime));
		tDateTime.Year = (int64)(testTimeTextRandom(&iState) %
			UINT64_C(10000));
		tDateTime.Month = (int)(testTimeTextRandom(&iState) % 12u) + 1;
		tDateTime.Day = (int)(testTimeTextRandom(&iState) %
			(uint64)xrtDaysInMonth(tDateTime.Year, tDateTime.Month)) + 1;
		tDateTime.Hour = (int)(testTimeTextRandom(&iState) % 24u);
		tDateTime.Minute = (int)(testTimeTextRandom(&iState) % 60u);
		tDateTime.Second = (int)(testTimeTextRandom(&iState) % 60u);
		testRequire(xrtTimeMake(&tDateTime, &iTime),
			"random HTTP-date source construction failed");
		iSize = xrtTimeWriteHTTPDate(
			sText, sizeof(sText), iTime);
		testRequire((iSize == 29u) &&
			xrtTimeParseHTTPDate(
				(xstrview){ sText, iSize }, &iRoundtrip) &&
			(iRoundtrip == iTime),
			"random HTTP-date roundtrip failed");
	}
}



/* 执行自定义格式、协议格式和有界写入属性测试。 */
int main(void)
{
	testTimeTextRoundtripProperty();
	testTimeRFC3339Property();
	testTimeBoundedWriteProperty();
	testTimeHTTPDateProperty();
	printf("[PASS] time-text-property\n");
	return 0;
}
