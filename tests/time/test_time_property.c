#include "../test.h"



/* 固定种子生成覆盖正负极值的可重复 64 位样本。 */
static uint64 testTimeRandom(uint64* pState)
{
	uint64 iValue = *pState;

	iValue ^= iValue << 13u;
	iValue ^= iValue >> 7u;
	iValue ^= iValue << 17u;
	*pState = iValue;
	return iValue;
}



/* 验证随机绝对时间在 UTC 和固定偏移下都可以无损往返。 */
static void testTimeRoundtripProperty(void)
{
	uint64 iState = UINT64_C(0x6A09E667F3BCC909);

	for ( size_t i = 0; i < 500000u; i++ ) {
		xtime iTime = (xtime)testTimeRandom(&iState);
		int iOffset = (int)(testTimeRandom(&iState) % UINT64_C(172799)) - 86399;
		xdatetime tDateTime;
		xtime iRoundtrip = 0;

		testRequire(xrtTimeSplitAt(iTime, iOffset, &tDateTime),
			"random fixed-offset split failed");
		testRequire((tDateTime.Month >= 1) && (tDateTime.Month <= 12) &&
			(tDateTime.Day >= 1) &&
			(tDateTime.Day <= xrtDaysInMonth(tDateTime.Year, tDateTime.Month)) &&
			(tDateTime.Hour >= 0) && (tDateTime.Hour <= 23) &&
			(tDateTime.Minute >= 0) && (tDateTime.Minute <= 59) &&
			(tDateTime.Second >= 0) && (tDateTime.Second <= 59) &&
			(tDateTime.Microsecond >= 0) &&
			(tDateTime.Microsecond <= 999999) &&
			(tDateTime.Offset == iOffset),
			"random fixed-offset split returned invalid fields");
		testRequire(xrtTimeMake(&tDateTime, &iRoundtrip) &&
			(iRoundtrip == iTime),
			"random fixed-offset split/make roundtrip failed");
	}
}



/* 验证固定单位差值只由最终单位数是否可表示决定。 */
static void testTimeDifferenceProperty(void)
{
	static const struct {
		xtimeunit Unit;
		uint64 Duration;
	} arrUnits[] = {
		{ XTIME_UNIT_MICROSECOND, UINT64_C(1) },
		{ XTIME_UNIT_MILLISECOND, UINT64_C(1000) },
		{ XTIME_UNIT_SECOND, UINT64_C(1000000) },
		{ XTIME_UNIT_MINUTE, UINT64_C(60000000) },
		{ XTIME_UNIT_HOUR, UINT64_C(3600000000) },
		{ XTIME_UNIT_DAY, UINT64_C(86400000000) },
		{ XTIME_UNIT_WEEK, UINT64_C(604800000000) }
	};
	uint64 iState = UINT64_C(0xBB67AE8584CAA73B);

	for ( size_t i = 0; i < 300000u; i++ ) {
		xtime iStart = (xtime)testTimeRandom(&iState);
		xtime iEnd = (xtime)testTimeRandom(&iState);
		size_t iUnit = (size_t)(testTimeRandom(&iState) %
			(sizeof(arrUnits) / sizeof(arrUnits[0])));
		bool bNegative = iEnd < iStart;
		uint64 iMagnitude = bNegative ?
			((uint64)iStart - (uint64)iEnd) :
			((uint64)iEnd - (uint64)iStart);
		uint64 iUnits = iMagnitude / arrUnits[iUnit].Duration;
		bool bRepresentable = bNegative ?
			(iUnits <= (UINT64_C(1) << 63u)) :
			(iUnits <= (uint64)INT64_MAX);
		int64 iActual = 37;

		if ( bRepresentable ) {
			int64 iExpected;

			if ( !bNegative ) {
				iExpected = (int64)iUnits;
			} else {
				iExpected = iUnits == (UINT64_C(1) << 63u) ?
					INT64_MIN : -(int64)iUnits;
			}
			testRequire(xrtTimeDiff(
				iStart, iEnd, arrUnits[iUnit].Unit, &iActual) &&
				(iActual == iExpected),
				"random full-domain fixed difference mismatch");
		} else {
			xrtClearError();
			testRequire(!xrtTimeDiff(
				iStart, iEnd, arrUnits[iUnit].Unit, &iActual) &&
				(iActual == 37) &&
				(xrtErrorCode(xrtGetError()) == XTIME_ERROR_OVERFLOW),
				"random fixed difference overflow contract mismatch");
		}
	}
}



/* 验证可表示固定时长加法和反向差值保持一致。 */
static void testTimeAddProperty(void)
{
	uint64 iState = UINT64_C(0x3C6EF372FE94F82B);

	for ( size_t i = 0; i < 300000u; i++ ) {
		xtime iTime = (xtime)testTimeRandom(&iState);
		int64 iDelta = (int64)(testTimeRandom(&iState) % UINT64_C(2000001)) -
			INT64_C(1000000);
		xtime iResult;
		int64 iDifference;

		if ( ((iDelta > 0) && (iTime > (INT64_MAX - iDelta))) ||
			 ((iDelta < 0) && (iTime < (INT64_MIN - iDelta))) ) {
			continue;
		}
		testRequire(xrtTimeAdd(
			iTime, iDelta, XTIME_UNIT_MICROSECOND, &iResult),
			"random fixed addition failed");
		testRequire(xrtTimeDiff(
			iTime, iResult, XTIME_UNIT_MICROSECOND, &iDifference) &&
			(iDifference == iDelta),
			"random fixed add/diff invariant failed");
	}
}



/* 执行完整时间域的构造、差值和加法属性测试。 */
int main(void)
{
	testTimeRoundtripProperty();
	testTimeDifferenceProperty();
	testTimeAddProperty();
	printf("[PASS] time-property\n");
	return 0;
}
