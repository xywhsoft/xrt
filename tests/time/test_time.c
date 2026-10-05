#include "../test.h"

#include <time.h>
#include <math.h>

/* Fail exactly one allocator request, then delegate normally. A permanently
 * failing allocator cannot detect a second diagnostic overwriting MemoryError. */
static xallocator tOriginalAllocator;
static bool bFailNextAllocation;
static unsigned iDiagnosticRequests;

static ptr testCalendarAlloc(ptr pContext, size_t iSize)
{
	(void)pContext;
	if ( bFailNextAllocation ) {
		bFailNextAllocation = false;
		iDiagnosticRequests++;
		return NULL;
	}
	return tOriginalAllocator.Alloc(tOriginalAllocator.Context, iSize);
}

static ptr testCalendarRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	(void)pContext;
	if ( bFailNextAllocation ) {
		bFailNextAllocation = false;
		iDiagnosticRequests++;
		return NULL;
	}
	return tOriginalAllocator.Realloc(tOriginalAllocator.Context, pMemory, iSize);
}

static void testCalendarFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	tOriginalAllocator.Free(tOriginalAllocator.Context, pMemory);
}

static void testInstallCalendarAllocator(void)
{
	xallocator tAllocator;
	xrtGetAllocator(&tOriginalAllocator);
	tAllocator.Context = NULL;
	tAllocator.Alloc = testCalendarAlloc;
	tAllocator.Realloc = testCalendarRealloc;
	tAllocator.Free = testCalendarFree;
	testRequire(xrtSetAllocator(&tAllocator), "calendar test allocator install failed");
}



/* 时钟 API 必须区分墙钟与单调时钟，并保留旧版轻量测量手感。 */
static void testClocks(void)
{
    double Start = xrtTimer();
    xtime Now = xrtNow();
    xtime SystemNow;
    testRequire(isfinite(Start), "timer is not finite");
    testRequire(xrtTimeFromUnix((int64)time(NULL), &SystemNow), "system wall clock conversion failed");
    testRequire(xrtTimeNear(Now, SystemNow, 2000), "wall clock disagrees with the system clock");
    xrtSleep(2);
    testRequire(xrtTimer() >= Start + 0.001, "timer seconds or millisecond sleep are wrong");
    xrtSleep(0);
    xrtClearError();
    xrtSleep(-1);
    testRequire(xrtGetError() != NULL, "negative sleep accepted");
    xrtClearError();
}



/* Gregorian 工具必须覆盖世纪闰年、负年份和非法月份。 */
static void testCalendarRules(void)
{
	testRequire(xrtIsLeapYear(2000), "year 2000 should be a leap year");
	testRequire(!xrtIsLeapYear(1900), "year 1900 should not be a leap year");
	testRequire(xrtIsLeapYear(2024), "year 2024 should be a leap year");
	testRequire(!xrtIsLeapYear(0), "civil year zero must be rejected");
	testRequire(xrtIsLeapYear(-401), "negative 400-year cycle is incorrect");
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
    xtime Epoch, Before, Roundtrip;
    int64 UnixMilliseconds;
    int32 Unix32;
    xdatetime Parts;
    testRequire(xrtDate(1, 1, 1, &Epoch) && Epoch == 0, "civil epoch is wrong");
    testRequire(xrtDateTime(-1, 12, 31, 23, 59, 59, 999, &Before) && Before == -1, "BCE/CE boundary is discontinuous");
    testRequire(xrtTimeSplit(-1, &Parts) && Parts.Year == -1 && Parts.Millisecond == 999, "negative millisecond split is wrong");
    testRequire(xrtDate(1970, 1, 1, &Epoch) && Epoch == XRT_TIME_UNIX_EPOCH, "Unix epoch bias is wrong");
    testRequire(xrtTimeUnix(Epoch - 1) == -1, "negative Unix seconds must floor");
    testRequire(xrtTimeToUnixMs(Epoch - 1, &UnixMilliseconds) && UnixMilliseconds == -1, "negative Unix milliseconds are wrong");
    testRequire(xrtTimeFromUnix(-1, &Roundtrip) && Roundtrip == Epoch - 1000, "Unix seconds conversion failed");
    testRequire(xrtTimeFromUnixMs(-1, &Roundtrip) && Roundtrip == Epoch - 1, "Unix ms conversion failed");
    testRequire(xrtTimeFromUnix32(INT32_MIN, &Roundtrip) && xrtTimeToUnix32(Roundtrip, &Unix32) && Unix32 == INT32_MIN, "Unix32 lower boundary failed");
    testRequire(xrtTimeFromUnix32(INT32_MAX, &Roundtrip) && xrtTimeToUnix32(Roundtrip + 999, &Unix32) && Unix32 == INT32_MAX, "Unix32 upper boundary failed");
    Unix32 = 17;
    testRequire(!xrtTimeToUnix32(Roundtrip + 1000, &Unix32) && Unix32 == 17, "Unix32 overflow modified output");
    Roundtrip = 17;
    testRequire(!xrtTimeFromUnix(INT64_MAX, &Roundtrip) && Roundtrip == 17, "Unix64 overflow modified output");
    testRequire(xrtTimeFromUnixMs(INT64_MIN, &Roundtrip) && Roundtrip == INT64_MIN + XRT_TIME_UNIX_EPOCH, "Unix ms lower input failed");
    UnixMilliseconds = 17;
    testRequire(!xrtTimeToUnixMs(INT64_MIN, &UnixMilliseconds) && UnixMilliseconds == 17, "Unix ms lower output overflow changed output");
    testRequire(xrtTimeFromUnixMs(INT64_MAX - XRT_TIME_UNIX_EPOCH, &Roundtrip) && Roundtrip == INT64_MAX, "Unix ms upper input failed");
    testRequire(xrtTimeFromUnix(xrtTimeUnix(INT64_MIN) + 1, &Roundtrip) && Roundtrip == INT64_MIN + 808, "valid Unix seconds rejected due to intermediate overflow");
    Roundtrip = 17;
    testRequire(!xrtDate(0, 1, 1, &Roundtrip) && Roundtrip == 17, "year zero accepted");
    testRequire(xrtDateTime(2024, 2, 29, 23, 58, 57, 654, &Roundtrip), "leap date failed");
    testRequire(xrtTimeSplit(Roundtrip, &Parts) && Parts.Year == 2024 && Parts.Month == 2 && Parts.Day == 29 && Parts.Millisecond == 654 && Parts.YearDay == 60, "millisecond parts wrong");
    testRequire(xrtTimeMake(&Parts, &Epoch) && Epoch == Roundtrip, "date roundtrip failed");
    testRequire(xrtDateDiff(-1, 0, XTIME_UNIT_YEAR, &UnixMilliseconds) && UnixMilliseconds == 1, "year difference counted a nonexistent year");
    testRequire(xrtDateDiff(-1, 0, XTIME_UNIT_MONTH, &UnixMilliseconds) && UnixMilliseconds == 1, "month difference at era boundary failed");
    testRequire(xrtTimeAdd(-1, 1, XTIME_UNIT_MONTH, &Roundtrip) && xrtYear(Roundtrip) == 1 && xrtMonth(Roundtrip) == 1, "month addition did not skip year zero");
    testRequire(xrtTimeAdd(INT64_MIN, INT64_C(9223372036854776), XTIME_UNIT_SECOND, &Roundtrip) && Roundtrip == 192, "scaled addition rejected a representable result");
    testRequire(xrtTimeAdd(INT64_MAX, -INT64_C(9223372036854776), XTIME_UNIT_SECOND, &Roundtrip) && Roundtrip == -193, "negative scaled addition rejected a representable result");
    testRequire(xrtDate(2024, 1, 31, &Before) && xrtDate(2024, 2, 1, &Epoch) && xrtDateDiff(Before, Epoch, XTIME_UNIT_MONTH, &UnixMilliseconds) && UnixMilliseconds == 1, "DateDiff must use calendar month indices");
    xrtClearError();
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
		(iTime == (XRT_TIME_UNIX_EPOCH - 8 * XRT_TIME_HOUR)), "positive offset construction failed");
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
	testRequire(xrtDateDiff(iJanuary31, iFebruary, XTIME_UNIT_MONTH, &iDifference) &&
		(iDifference == 1), "forward complete-month difference failed");
	testRequire(xrtDateDiff(iFebruary, iJanuary31, XTIME_UNIT_MONTH, &iDifference) &&
		(iDifference == -1), "reverse calendar-month difference failed");

	testRequire(xrtDate(2024, 2, 29, &iLeapDay), "leap day failed");
	testRequire(xrtTimeAdd(iLeapDay, 1, XTIME_UNIT_YEAR, &iNextYear),
		"year addition failed");
	(void)xrtTimeSplit(iNextYear, &tDateTime);
	testRequire((tDateTime.Year == 2025) && (tDateTime.Month == 2) &&
		(tDateTime.Day == 28), "leap-year clamp failed");

	testRequire(xrtTimeAdd(0, -1, XTIME_UNIT_MILLISECOND, &iNextYear) &&
		(iNextYear == -1), "fixed duration addition failed");
	testRequire(xrtDateDiff(-XRT_TIME_SECOND, XRT_TIME_SECOND,
		XTIME_UNIT_MILLISECOND, &iDifference) && (iDifference == 2000),
		"fixed duration difference failed");
	testRequire(xrtDateDiff(INT64_MIN, INT64_MAX,
		XTIME_UNIT_SECOND, &iDifference) &&
		(iDifference == INT64_C(18446744073709551)),
		"full-domain positive millisecond difference failed");
	testRequire(xrtDateDiff(INT64_MAX, INT64_MIN,
		XTIME_UNIT_SECOND, &iDifference) &&
		(iDifference == -INT64_C(18446744073709551)),
		"full-domain negative millisecond difference failed");
	testRequire(xrtDateDiff(0, INT64_MIN,
		XTIME_UNIT_MILLISECOND, &iDifference) &&
		(iDifference == INT64_MIN),
		"representable negative extreme difference failed");

	xrtClearError();
	iNextYear = 123;
	testRequire(!xrtTimeAdd(INT64_MAX, 1, XTIME_UNIT_MILLISECOND, &iNextYear) &&
		(iNextYear == 123), "overflowing addition modified the output");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XTIME_ERROR_OVERFLOW),
		"addition overflow reported the wrong error");

	xrtClearError();
	iDifference = 123;
	testRequire(!xrtDateDiff(INT64_MIN, INT64_MAX,
		XTIME_UNIT_MILLISECOND, &iDifference) &&
		(iDifference == 123),
		"unrepresentable millisecond difference modified the output");
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

	testRequire(xrtDateTime(2024, 2, 15, 23, 59, 59, 999, &iSame),
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



/* Calendar overflow must preserve a failed diagnostic allocation, including
 * when propagated through half-open range constructors. */
static void testCalendarDiagnosticOOM(void)
{
	xtime iStart = 123;
	xtime iEnd = 456;
	/* Initialize the optional TLS heap cache using a different size class.
	 * Cache allocation failure alone may validly fall back to the global heap;
	 * the injected failure must hit the diagnostic's actual backing span. */
	ptr pWarmup = xrtMalloc(1);
	testRequire(pWarmup != NULL, "calendar allocator warmup failed");
	xrtFree(pWarmup);
	bFailNextAllocation = true;
	testRequire(!xrtYearRange(INT64_MAX, &iStart, &iEnd),
		"year range overflow unexpectedly succeeded");
	testRequire(!bFailNextAllocation && (iDiagnosticRequests == 1),
		"calendar diagnostic fail-once injection missed");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"calendar propagation replaced the original diagnostic error");
	testRequire((iStart == 123) && (iEnd == 456), "calendar OOM changed output");
	xrtClearError();
	for ( unsigned iPath = 0; iPath < 6; iPath++ ) {
		bool bSuccess;
		iStart = 123;
		iEnd = 456;
		xrtClearError();
		switch ( iPath ) {
		case 0: bSuccess = xrtTimeAdd(INT64_MAX, 1, XTIME_UNIT_MONTH, &iStart); break;
		case 1: bSuccess = xrtTimeAdd(INT64_MIN, -1, XTIME_UNIT_YEAR, &iStart); break;
		case 2: bSuccess = xrtTimeAdd(0, INT64_MAX, XTIME_UNIT_MONTH, &iStart); break;
		case 3: bSuccess = xrtTimeAdd(0, INT64_MIN, XTIME_UNIT_MONTH, &iStart); break;
		case 4: bSuccess = xrtMonthRange(INT64_MAX, &iStart, &iEnd); break;
		default: bSuccess = xrtYearRange(INT64_MAX, &iStart, &iEnd); break;
		}
		testRequire(!bSuccess && (iStart == 123) && (iEnd == 456),
			"calendar overflow changed output");
		testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE,
			"calendar propagation replaced the original diagnostic error");
		testRequire(xrtErrorCode(xrtGetError()) == XTIME_ERROR_OVERFLOW,
			"calendar overflow code changed");
		xrtClearError();
	}
}

/* 执行时钟、Gregorian、全域、偏移、算术、区间与 ISO 周测试。 */
int main(void)
{
	testInstallCalendarAllocator();
	testCalendarDiagnosticOOM();
	testClocks();
	testCalendarRules();
	testEpochAndParts();
	testFullDomain();
	testOffsets();
	testArithmetic();
	testRangesAndISOWeek();
	return 0;
}
