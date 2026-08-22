#include "../test.h"

#include <time.h>



/* 时钟 API 必须区分墙钟与单调时钟，并保留旧版轻量测量手感。 */
static void testClocks(void)
{
	uint64 iStart = xrtClock();
	double fStart = xrtTimer();
	xtime iNow = xrtNow();
	time_t iSystemNow = time(NULL);

	xrtSleepUs(2000);
	testRequire(xrtClock() > iStart, "monotonic clock did not advance");
	testRequire(xrtTimer() > fStart, "floating timer did not advance");
	testRequire(xrtTimeNear(iNow, (xtime)iSystemNow * XRT_TIME_SECOND,
		UINT64_C(2000000)), "wall clock is not close to the system clock");
	xrtSleepUntil(xrtClock() - 1u);
}



/* Gregorian 工具必须覆盖世纪闰年、负年份和非法月份。 */
static void testCalendarRules(void)
{
	testRequire(xrtIsLeapYear(2000), "year 2000 should be a leap year");
	testRequire(!xrtIsLeapYear(1900), "year 1900 should not be a leap year");
	testRequire(xrtIsLeapYear(2024), "year 2024 should be a leap year");
	testRequire(xrtIsLeapYear(0), "astronomical year zero should be a leap year");
	testRequire(xrtIsLeapYear(-400), "negative 400-year cycle is incorrect");
	testRequire(xrtDaysInMonth(2024, 2) == 29, "leap February length is wrong");
	testRequire(xrtDaysInMonth(2023, 2) == 28, "common February length is wrong");
	testRequire(xrtDaysInYear(2024) == 366, "leap year length is wrong");

	xrtClearError();
	testRequire(xrtDaysInMonth(2024, 13) == 0, "invalid month did not fail");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"invalid month reported the wrong error");
}



/* Epoch 前后的构造、分解和单位换算必须使用向负无穷取整语义。 */
static void testEpochAndParts(void)
{
	xtime iEpoch;
	xtime iBefore;
	xtime iRoundtrip;
	xdatetime tDateTime;

	testRequire(xrtDate(1970, 1, 1, &iEpoch) && (iEpoch == 0),
		"Unix epoch construction failed");
	testRequire(xrtDateTime(1969, 12, 31, 23, 59, 59, 999999, &iBefore) &&
		(iBefore == -1), "pre-epoch microsecond construction failed");
	testRequire(xrtTimeUnix(-1) == -1, "negative Unix second did not floor");
	testRequire(xrtTimeUnixMs(-1) == -1, "negative Unix millisecond did not floor");
	testRequire(xrtTimeFromUnix(-1, &iRoundtrip) &&
		(iRoundtrip == -XRT_TIME_SECOND), "Unix second conversion failed");
	testRequire(xrtTimeFromUnixMs(-1, &iRoundtrip) &&
		(iRoundtrip == -XRT_TIME_MILLISECOND), "Unix millisecond conversion failed");

	testRequire(xrtDateTime(2024, 2, 29, 23, 58, 57, 654321, &iRoundtrip),
		"leap date construction failed");
	testRequire(xrtTimeSplit(iRoundtrip, &tDateTime), "time split failed");
	testRequire((tDateTime.Year == 2024) && (tDateTime.Month == 2) &&
		(tDateTime.Day == 29) && (tDateTime.Hour == 23) &&
		(tDateTime.Minute == 58) && (tDateTime.Second == 57) &&
		(tDateTime.Microsecond == 654321) && (tDateTime.YearDay == 60),
		"time split returned incorrect fields");
	testRequire(xrtTimeMake(&tDateTime, &iEpoch) && (iEpoch == iRoundtrip),
		"time split/make roundtrip failed");
}



/* int64 两端都必须能够分解并原样重建，不能对 INT64_MIN 取绝对值。 */
static void testFullDomain(void)
{
	const xtime arrValues[] = {
		INT64_MIN, INT64_MIN + XRT_TIME_DAY, -1, 0, 1,
		INT64_MAX - XRT_TIME_DAY, INT64_MAX
	};

	for ( size_t i = 0; i < (sizeof(arrValues) / sizeof(arrValues[0])); i++ ) {
		xdatetime tDateTime;
		xtime iRoundtrip = 17;

		testRequire(xrtTimeSplit(arrValues[i], &tDateTime),
			"full-domain split failed");
		testRequire(xrtTimeMake(&tDateTime, &iRoundtrip) &&
			(iRoundtrip == arrValues[i]), "full-domain roundtrip failed");
	}
	{
		xdatetime tDateTime;
		xtime iResult = 47;

		memset(&tDateTime, 0, sizeof(tDateTime));
		tDateTime.Year = INT64_MIN;
		tDateTime.Month = 1;
		tDateTime.Day = 1;
		testRequire(!xrtTimeMake(&tDateTime, &iResult) && (iResult == 47),
			"extreme calendar year overflow modified the output");
		testRequire((xrtGetError() != NULL) &&
			(xrtErrorCode(xrtGetError()) == XTIME_ERROR_OVERFLOW),
			"extreme calendar year reported the wrong error");
	}

	xrtClearError();
	testRequire(xrtDatePart(INT64_MIN) == 0,
		"unrepresentable date start did not use the failure value");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XTIME_ERROR_OVERFLOW),
		"date-part overflow reported the wrong error");
}



/* 固定偏移必须可逆，并覆盖跨越日期和 Epoch 的情况。 */
static void testOffsets(void)
{
	xdatetime tDateTime;
	xtime iTime;
	xtime iRoundtrip;

	memset(&tDateTime, 0, sizeof(tDateTime));
	tDateTime.Year = 1970;
	tDateTime.Month = 1;
	tDateTime.Day = 1;
	tDateTime.Offset = 8 * 3600;
	testRequire(xrtTimeMake(&tDateTime, &iTime) &&
		(iTime == (-8 * XRT_TIME_HOUR)), "positive offset construction failed");
	testRequire(xrtTimeSplitAt(iTime, 8 * 3600, &tDateTime),
		"fixed offset split failed");
	testRequire((tDateTime.Year == 1970) && (tDateTime.Month == 1) &&
		(tDateTime.Day == 1) && (tDateTime.Hour == 0),
		"fixed offset split crossed the wrong date");
	testRequire(xrtTimeMake(&tDateTime, &iRoundtrip) && (iRoundtrip == iTime),
		"fixed offset roundtrip failed");

	xrtClearError();
	testRequire(!xrtTimeSplitAt(0, 86400, &tDateTime),
		"invalid UTC offset was accepted");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XTIME_ERROR_RANGE),
		"invalid UTC offset reported the wrong error");
}



/* 日历加法必须定义月末钳制，并让完整单位差在正反方向保持一致。 */
static void testArithmetic(void)
{
	xtime iJanuary31;
	xtime iFebruary;
	xtime iLeapDay;
	xtime iNextYear;
	int64 iDifference;
	xdatetime tDateTime;

	testRequire(xrtDate(2024, 1, 31, &iJanuary31), "January date failed");
	testRequire(xrtTimeAdd(iJanuary31, 1, XTIME_UNIT_MONTH, &iFebruary),
		"month addition failed");
	(void)xrtTimeSplit(iFebruary, &tDateTime);
	testRequire((tDateTime.Year == 2024) && (tDateTime.Month == 2) &&
		(tDateTime.Day == 29), "month-end clamp failed");
	testRequire(xrtTimeDiff(iJanuary31, iFebruary, XTIME_UNIT_MONTH, &iDifference) &&
		(iDifference == 1), "forward complete-month difference failed");
	testRequire(xrtTimeDiff(iFebruary, iJanuary31, XTIME_UNIT_MONTH, &iDifference) &&
		(iDifference == 0), "reverse complete-month difference failed");

	testRequire(xrtDate(2024, 2, 29, &iLeapDay), "leap day failed");
	testRequire(xrtTimeAdd(iLeapDay, 1, XTIME_UNIT_YEAR, &iNextYear),
		"year addition failed");
	(void)xrtTimeSplit(iNextYear, &tDateTime);
	testRequire((tDateTime.Year == 2025) && (tDateTime.Month == 2) &&
		(tDateTime.Day == 28), "leap-year clamp failed");

	testRequire(xrtTimeAdd(0, -1, XTIME_UNIT_MICROSECOND, &iNextYear) &&
		(iNextYear == -1), "fixed duration addition failed");
	testRequire(xrtTimeDiff(-XRT_TIME_SECOND, XRT_TIME_SECOND,
		XTIME_UNIT_MILLISECOND, &iDifference) && (iDifference == 2000),
		"fixed duration difference failed");
	testRequire(xrtTimeDiff(INT64_MIN, INT64_MAX,
		XTIME_UNIT_MILLISECOND, &iDifference) &&
		(iDifference == INT64_C(18446744073709551)),
		"full-domain positive millisecond difference failed");
	testRequire(xrtTimeDiff(INT64_MAX, INT64_MIN,
		XTIME_UNIT_MILLISECOND, &iDifference) &&
		(iDifference == -INT64_C(18446744073709551)),
		"full-domain negative millisecond difference failed");
	testRequire(xrtTimeDiff(0, INT64_MIN,
		XTIME_UNIT_MICROSECOND, &iDifference) &&
		(iDifference == INT64_MIN),
		"representable negative extreme difference failed");

	xrtClearError();
	iNextYear = 123;
	testRequire(!xrtTimeAdd(INT64_MAX, 1, XTIME_UNIT_MICROSECOND, &iNextYear) &&
		(iNextYear == 123), "overflowing addition modified the output");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XTIME_ERROR_OVERFLOW),
		"addition overflow reported the wrong error");

	xrtClearError();
	iDifference = 123;
	testRequire(!xrtTimeDiff(INT64_MIN, INT64_MAX,
		XTIME_UNIT_MICROSECOND, &iDifference) &&
		(iDifference == 123),
		"unrepresentable microsecond difference modified the output");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XTIME_ERROR_OVERFLOW),
		"difference overflow reported the wrong error");
}



/* 半开日历区间和 ISO 周必须在跨年边界给出规范结果。 */
static void testRangesAndISOWeek(void)
{
	xtime iTime;
	xtime iSame;
	xtime iOther;
	xtime iStart;
	xtime iEnd;
	int64 iWeekYear;
	int iWeek;
	int iWeekday;

	testRequire(xrtDate(2016, 1, 1, &iTime), "ISO test date failed");
	testRequire(xrtISOWeek(iTime, &iWeekYear, &iWeek, &iWeekday) &&
		(iWeekYear == 2015) && (iWeek == 53) && (iWeekday == 5),
		"ISO cross-year week is wrong");
	testRequire(xrtDate(2016, 1, 4, &iTime), "ISO week-one date failed");
	testRequire(xrtISOWeek(iTime, &iWeekYear, &iWeek, &iWeekday) &&
		(iWeekYear == 2016) && (iWeek == 1) && (iWeekday == 1),
		"ISO week one is wrong");

	testRequire(xrtDateTime(2024, 2, 15, 12, 0, 0, 0, &iTime),
		"range test date failed");
	testRequire(xrtMonthRange(iTime, &iStart, &iEnd) &&
		(xrtDay(iStart) == 1) && (xrtMonth(iStart) == 2) &&
		(xrtMonth(iEnd) == 3) && xrtTimeIn(iTime, iStart, iEnd - 1),
		"month range failed");
	testRequire(xrtYearRange(iTime, &iStart, &iEnd) &&
		(xrtMonth(iStart) == 1) && (xrtDay(iStart) == 1) &&
		(xrtYear(iEnd) == 2025), "year range failed");
	testRequire(xrtWeekRange(iTime, XTIME_MONDAY, &iStart, &iEnd) &&
		(xrtWeekday(iStart) == XTIME_MONDAY) &&
		((iEnd - iStart) == XRT_TIME_WEEK), "week range failed");

	testRequire(xrtDateTime(2024, 2, 15, 23, 59, 59, 999999, &iSame),
		"same-period first source construction failed");
	testRequire(xrtDateTime(2024, 2, 16, 0, 0, 0, 0, &iOther),
		"same-period second source construction failed");
	testRequire(xrtTimeSameDay(iTime, iSame) &&
		!xrtTimeSameDay(iTime, iOther), "same-day comparison failed");
	testRequire(xrtTimeSameMonth(iTime, iOther) &&
		xrtTimeSameYear(iTime, iOther), "same month/year comparison failed");
	testRequire(xrtTimeSameDay(INT64_MIN, INT64_MIN + 1),
		"same-day comparison failed at the negative extreme");

	testRequire(xrtTimeIn(5, 1, 5), "closed range rejected its boundary");
	testRequire(!xrtTimeIn(5, 6, 1), "reversed range was accepted");
	testRequire(xrtTimeOverlap(1, 5, 5, 9),
		"touching closed ranges did not overlap");
	testRequire(!xrtTimeOverlap(5, 1, 0, 9),
		"invalid range participated in overlap");
}



/* 执行时钟、Gregorian、全域、偏移、算术、区间与 ISO 周测试。 */
int main(void)
{
	testClocks();
	testCalendarRules();
	testEpochAndParts();
	testFullDomain();
	testOffsets();
	testArithmetic();
	testRangesAndISOWeek();
	return 0;
}
