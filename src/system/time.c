#include "../internal/xrt_time.h"
#include <math.h>

#include <errno.h>
#include <time.h>



#if defined(XRT_FEATURE_TIME)

/* Unix Epoch 与 Windows FILETIME Epoch 之间相差的 100 纳秒计数。 */
#define XRT_FILETIME_EPOCH_TICKS UINT64_C(116444736000000000)



/* 设置带操作名和系统代码的时间模块错误。 */
void __xrtTimeSetError(xerrkind Kind, xtimeerror Code,
	cstr sOperation, cstr sMessage, int iSystemCode)
{
	xerrordesc tDesc;
	xerror* pError;

	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.Kind = Kind;
	tDesc.Domain = "xrt.time";
	tDesc.Code = (int32)Code;
	tDesc.SystemCode = (int32)iSystemCode;
	tDesc.Operation = sOperation;
	tDesc.Message = sMessage;
	pError = xrtErrorBuild(&tDesc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 检查 int64 加法，失败时不修改输出。 */
bool __xrtTimeAddChecked(int64 iLeft, int64 iRight, int64* pResult)
{
	if ( ((iRight > 0) && (iLeft > (INT64_MAX - iRight))) ||
		 ((iRight < 0) && (iLeft < (INT64_MIN - iRight))) ) {
		return false;
	}
	*pResult = iLeft + iRight;
	return true;
}



/* 检查 int64 减法，不先对 INT64_MIN 取负。 */
bool __xrtTimeSubChecked(int64 iLeft, int64 iRight, int64* pResult)
{
	if ( ((iRight > 0) && (iLeft < (INT64_MIN + iRight))) ||
		 ((iRight < 0) && (iLeft > (INT64_MAX + iRight))) ) {
		return false;
	}
	*pResult = iLeft - iRight;
	return true;
}



/* 检查 int64 乘法，覆盖 INT64_MIN 与 -1 的特殊边界。 */
bool __xrtTimeMulChecked(int64 iLeft, int64 iRight, int64* pResult)
{
	if ( (iLeft == 0) || (iRight == 0) ) {
		*pResult = 0;
		return true;
	}
	if ( ((iLeft == -1) && (iRight == INT64_MIN)) ||
		 ((iRight == -1) && (iLeft == INT64_MIN)) ) {
		return false;
	}
	if ( iLeft > 0 ) {
		if ( ((iRight > 0) && (iLeft > (INT64_MAX / iRight))) ||
			 ((iRight < 0) && (iRight < (INT64_MIN / iLeft))) ) {
			return false;
		}
	} else {
		if ( ((iRight > 0) && (iLeft < (INT64_MIN / iRight))) ||
			 ((iRight < 0) && (iLeft < (INT64_MAX / iRight))) ) {
			return false;
		}
	}
	*pResult = iLeft * iRight;
	return true;
}



/* 设置统一的时间范围溢出错误。 */
static void __xrtTimeSetOverflow(cstr sOperation)
{
	__xrtTimeSetError(XERR_RANGE, XTIME_ERROR_OVERFLOW, sOperation,
		"time value is outside the representable range", 0);
}



/* 执行向负无穷取整的有符号除法。 */
int64 __xrtTimeFloorDiv(int64 iValue, int64 iDivisor)
{
	int64 iQuotient = iValue / iDivisor;
	int64 iRemainder = iValue % iDivisor;

	if ( iRemainder < 0 ) {
		iQuotient--;
	}
	return iQuotient;
}



/* 把 公元毫秒拆成天数和非负当日毫秒。 */
void __xrtTimeSplitDay(xtime iTime, int64* pDays, int64* pDayTime)
{
	int64 iDays = iTime / XRT_TIME_DAY;
	int64 iDayTime = iTime % XRT_TIME_DAY;

	if ( iDayTime < 0 ) {
		iDays--;
		iDayTime += XRT_TIME_DAY;
	}
	*pDays = iDays;
	*pDayTime = iDayTime;
}



/* 判断 Gregorian 年份是否为闰年。 */
XRT_API bool xrtIsLeapYear(int64 iYear)
{
    if ( iYear == 0 ) {
        __xrtTimeSetError(XERR_RANGE, XTIME_ERROR_RANGE, "leap-year", "the civil calendar has no year zero", 0);
        return false;
    }
    if ( iYear < 0 ) { iYear++; }
    return ((iYear % 4) == 0) && (((iYear % 100) != 0) || ((iYear % 400) == 0));
}



/* 返回指定月份的天数。 */
XRT_API int xrtDaysInMonth(int64 iYear, int iMonth)
{
	static const unsigned char arrDays[12] = {
		31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
	};

	if ( iYear == 0 || (iMonth < 1) || (iMonth > 12) ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	if ( (iMonth == 2) && xrtIsLeapYear(iYear) ) {
		return 29;
	}
	return (int)arrDays[iMonth - 1];
}



/* 返回指定年份的天数。 */
XRT_API int xrtDaysInYear(int64 iYear)
{
	if ( iYear == 0 ) { (void)xrtIsLeapYear(iYear); return 0; }
	return xrtIsLeapYear(iYear) ? 366 : 365;
}



/* 把 Gregorian 日期转换为公元天数。 */
bool __xrtTimeDaysFromCivil(int64 iYear, int iMonth, int iDay, int64* pDays)
{
	int64 iEra;
	int64 iEraDays;
	int64 iYearOfEra;
	int64 iDayOfYear;
	int64 iDayOfEra;
	int iMarchMonth;
	int iMonthDays;

	if ( (pDays == NULL) || (iYear == 0) ) {
		return false;
	}
	if ( (iMonth < 1) || (iMonth > 12) ) {
		return false;
	}
	iMonthDays = (iMonth == 2) ? (xrtIsLeapYear(iYear) ? 29 : 28) :
		(int)((const unsigned char[12]){ 31, 28, 31, 30, 31, 30,
			31, 31, 30, 31, 30, 31 }[iMonth - 1]);
	if ( (iDay < 1) || (iDay > iMonthDays) ) {
		return false;
	}

	iYear = iYear < 0 ? iYear + 1 : iYear;
	if ( iMonth <= 2 ) {
		if ( iYear == INT64_MIN ) {
			return false;
		}
		iYear--;
	}
	iEra = __xrtTimeFloorDiv(iYear, 400);
	iYearOfEra = iYear % 400;
	if ( iYearOfEra < 0 ) { iYearOfEra += 400; }
	iMarchMonth = iMonth + (iMonth > 2 ? -3 : 9);
	iDayOfYear = ((153 * iMarchMonth) + 2) / 5 + iDay - 1;
	iDayOfEra = (iYearOfEra * 365) + (iYearOfEra / 4) -
		(iYearOfEra / 100) + iDayOfYear;
	if ( !__xrtTimeMulChecked(iEra, 146097, &iEraDays) ||
		 !__xrtTimeAddChecked(iEraDays, iDayOfEra - 306, pDays) ) {
		return false;
	}
	return true;
}



/* 把 公元天数常数时间转换为 Gregorian 日期。 */
void __xrtTimeCivilFromDays(int64 iDays, int64* pYear, int* pMonth, int* pDay)
{
	int64 iShifted = iDays + 306;
	int64 iEra = __xrtTimeFloorDiv(iShifted, 146097);
	int64 iDayOfEra = iShifted - (iEra * 146097);
	int64 iYearOfEra = (iDayOfEra - (iDayOfEra / 1460) +
		(iDayOfEra / 36524) - (iDayOfEra / 146096)) / 365;
	int64 iYear = iYearOfEra + (iEra * 400);
	int64 iDayOfYear = iDayOfEra - ((365 * iYearOfEra) +
		(iYearOfEra / 4) - (iYearOfEra / 100));
	int iMarchMonth = (int)(((5 * iDayOfYear) + 2) / 153);
	int iDay = (int)(iDayOfYear - (((153 * iMarchMonth) + 2) / 5) + 1);
	int iMonth = iMarchMonth + (iMarchMonth < 10 ? 3 : -9);

	iYear += iMonth <= 2 ? 1 : 0;
	*pYear = iYear <= 0 ? iYear - 1 : iYear;
	*pMonth = iMonth;
	*pDay = iDay;
}



/* 校验固定偏移，避免把时区和任意日期算术混为一谈。 */
static bool __xrtTimeOffsetValid(int iOffset)
{
	return (iOffset > -86400) && (iOffset < 86400);
}



/* 从规范化的天数和当日毫秒构造值，负极值不要求日期零点可表示。 */
static bool __xrtTimeComposeDay(int64 iDays, int64 iDayTime, xtime* pTime)
{
	int64 iDate;
	int64 iTail;

	if ( iDays >= 0 ) {
		return __xrtTimeMulChecked(iDays, XRT_TIME_DAY, &iDate) &&
			__xrtTimeAddChecked(iDate, iDayTime, pTime);
	}
	if ( !__xrtTimeMulChecked(iDays + 1, XRT_TIME_DAY, &iDate) ) {
		return false;
	}
	iTail = iDayTime - XRT_TIME_DAY;
	return __xrtTimeAddChecked(iDate, iTail, pTime);
}



/* 无错误副作用地按显式 UTC 偏移构造绝对时间。 */
__xrt_time_make_status __xrtTimeMakeValue(
	const xdatetime* pDateTime, xtime* pTime)
{
	int64 iDays;
	int64 iDayTime;
	int64 iOffset;
	int64 iAdjusted;
	int64 iCarry;
	int iMonthDays;

	if ( pDateTime->Year == 0 ) { return __XRT_TIME_MAKE_COMPONENT; }
	if ( !__xrtTimeOffsetValid(pDateTime->Offset) ) {
		return __XRT_TIME_MAKE_OFFSET;
	}
	if ( (pDateTime->Month < 1) || (pDateTime->Month > 12) ) {
		return __XRT_TIME_MAKE_COMPONENT;
	}
	iMonthDays = xrtDaysInMonth(pDateTime->Year, pDateTime->Month);
	if ( (pDateTime->Day < 1) || (pDateTime->Day > iMonthDays) ||
		 (pDateTime->Hour < 0) || (pDateTime->Hour > 23) ||
		 (pDateTime->Minute < 0) || (pDateTime->Minute > 59) ||
		 (pDateTime->Second < 0) || (pDateTime->Second > 59) ||
		 (pDateTime->Millisecond < 0) || (pDateTime->Millisecond > 999) ) {
		return __XRT_TIME_MAKE_COMPONENT;
	}
	if ( !__xrtTimeDaysFromCivil(pDateTime->Year, pDateTime->Month,
		pDateTime->Day, &iDays) ) {
		return __XRT_TIME_MAKE_OVERFLOW;
	}

	iDayTime = ((int64)pDateTime->Hour * XRT_TIME_HOUR) +
		((int64)pDateTime->Minute * XRT_TIME_MINUTE) +
		((int64)pDateTime->Second * XRT_TIME_SECOND) + pDateTime->Millisecond;
	iOffset = (int64)pDateTime->Offset * XRT_TIME_SECOND;
	iAdjusted = iDayTime - iOffset;
	iCarry = __xrtTimeFloorDiv(iAdjusted, XRT_TIME_DAY);
	if ( !__xrtTimeAddChecked(iDays, iCarry, &iDays) ) {
		return __XRT_TIME_MAKE_OVERFLOW;
	}
	iDayTime = iAdjusted - (iCarry * XRT_TIME_DAY);
	if ( !__xrtTimeComposeDay(iDays, iDayTime, pTime) ) {
		return __XRT_TIME_MAKE_OVERFLOW;
	}
	return __XRT_TIME_MAKE_OK;
}



/* 按显式 UTC 偏移构造绝对时间。 */
XRT_API bool xrtTimeMake(const xdatetime* pDateTime, xtime* pTime)
{
	__xrt_time_make_status Status;

	if ( (pDateTime == NULL) || (pTime == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Status = __xrtTimeMakeValue(pDateTime, pTime);
	if ( Status == __XRT_TIME_MAKE_OK ) {
		return true;
	}
	if ( Status == __XRT_TIME_MAKE_OFFSET ) {
		__xrtTimeSetError(XERR_RANGE, XTIME_ERROR_RANGE, "make",
			"UTC offset is outside the supported range", 0);
	} else if ( Status == __XRT_TIME_MAKE_COMPONENT ) {
		__xrtTimeSetError(XERR_RANGE, XTIME_ERROR_RANGE, "make",
			"date or time component is outside its valid range", 0);
	} else {
		__xrtTimeSetOverflow("make");
	}
	return false;
}



/* 构造 UTC 零点日期。 */
XRT_API bool xrtDate(int64 iYear, int iMonth, int iDay, xtime* pTime)
{
	return xrtDateTime(iYear, iMonth, iDay, 0, 0, 0, 0, pTime);
}



/* 构造 UTC 日期时间。 */
XRT_API bool xrtDateTime(int64 iYear, int iMonth, int iDay,
	int iHour, int iMinute, int iSecond, int iMillisecond, xtime* pTime)
{
	xdatetime tDateTime;

	memset(&tDateTime, 0, sizeof(tDateTime));
	tDateTime.Year = iYear;
	tDateTime.Month = iMonth;
	tDateTime.Day = iDay;
	tDateTime.Hour = iHour;
	tDateTime.Minute = iMinute;
	tDateTime.Second = iSecond;
	tDateTime.Millisecond = iMillisecond;
	return xrtTimeMake(&tDateTime, pTime);
}



/* 按固定偏移分解绝对时间，避免在极值处先执行可能溢出的整体加法。 */
XRT_API bool xrtTimeSplitAt(xtime iTime, int iOffset, xdatetime* pDateTime)
{
	int64 iDays;
	int64 iDayTime;
	int64 iAdjusted;
	int64 iCarry;
	int64 iYearStart;
	int64 iSecondOfDay;

	if ( pDateTime == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtTimeOffsetValid(iOffset) ) {
		__xrtTimeSetError(XERR_RANGE, XTIME_ERROR_RANGE, "split",
			"UTC offset is outside the supported range", 0);
		return false;
	}

	__xrtTimeSplitDay(iTime, &iDays, &iDayTime);
	iAdjusted = iDayTime + ((int64)iOffset * XRT_TIME_SECOND);
	iCarry = __xrtTimeFloorDiv(iAdjusted, XRT_TIME_DAY);
	iDays += iCarry;
	iDayTime = iAdjusted - (iCarry * XRT_TIME_DAY);

	memset(pDateTime, 0, sizeof(*pDateTime));
	__xrtTimeCivilFromDays(iDays, &pDateTime->Year,
		&pDateTime->Month, &pDateTime->Day);
	iSecondOfDay = iDayTime / XRT_TIME_SECOND;
	pDateTime->Hour = (int)(iSecondOfDay / 3600);
	pDateTime->Minute = (int)((iSecondOfDay % 3600) / 60);
	pDateTime->Second = (int)(iSecondOfDay % 60);
	pDateTime->Millisecond = (int)(iDayTime % XRT_TIME_SECOND);
	pDateTime->Offset = iOffset;
	pDateTime->Weekday = (int)((iDays + 1) % 7);
	if ( pDateTime->Weekday < 0 ) {
		pDateTime->Weekday += 7;
	}
	if ( !__xrtTimeDaysFromCivil(
		pDateTime->Year,
		1,
		1,
		&iYearStart
	) ) {
		__xrtTimeSetOverflow("split");
		return false;
	}
	pDateTime->YearDay = (int)(iDays - iYearStart) + 1;
	pDateTime->IsDST = -1;
	return true;
}



/* 按 UTC 分解绝对时间。 */
XRT_API bool xrtTimeSplit(xtime iTime, xdatetime* pDateTime)
{
	return xrtTimeSplitAt(iTime, 0, pDateTime);
}



/* 从 Unix 秒安全构造 xtime。 */
bool __xrtTimeFromUnixParts(int64 iSeconds, int iMillisecond, xtime* pTime)
{
    int64 iCivilSeconds;
    int64 iBase;
    int64 iResult;
    if ( pTime == NULL || iMillisecond < 0 || iMillisecond > 999 ) { return false; }
    if ( !__xrtTimeAddChecked(iSeconds, INT64_C(62135596800), &iCivilSeconds) ) { return false; }
    if ( iCivilSeconds >= 0 ) {
        if ( !__xrtTimeMulChecked(iCivilSeconds, 1000, &iBase) ||
             !__xrtTimeAddChecked(iBase, iMillisecond, &iResult) ) { return false; }
    } else {
        if ( !__xrtTimeMulChecked(iCivilSeconds + 1, 1000, &iBase) ||
             !__xrtTimeAddChecked(iBase, iMillisecond - 1000, &iResult) ) { return false; }
    }
    *pTime = iResult;
    return true;
}

XRT_API bool xrtTimeFromUnix(int64 iSeconds, xtime* pTime)
{
    if ( pTime == NULL ) { __xrtErrorSetInvalidArgument(); return false; }
    if ( !__xrtTimeFromUnixParts(iSeconds, 0, pTime) ) { __xrtTimeSetOverflow("from-unix"); return false; }
    return true;
}



/* 从 Unix 毫秒安全构造 xtime。 */
XRT_API bool xrtTimeFromUnixMs(int64 iMilliseconds, xtime* pTime)
{
    if ( pTime == NULL ) { __xrtErrorSetInvalidArgument(); return false; }
    if ( !__xrtTimeAddChecked(iMilliseconds, XRT_TIME_UNIX_EPOCH, pTime) ) {
        __xrtTimeSetOverflow("from-unix-ms"); return false;
    }
    return true;
}



/* 返回向负无穷取整的 Unix 秒。 */
XRT_API int64 xrtTimeUnix(xtime iTime)
{
    return __xrtTimeFloorDiv(iTime, 1000) - INT64_C(62135596800);
}



/* 返回向负无穷取整的 Unix 毫秒。 */
XRT_API bool xrtTimeToUnixMs(xtime iTime, int64* pMilliseconds)
{
    if ( pMilliseconds == NULL ) { __xrtErrorSetInvalidArgument(); return false; }
    if ( !__xrtTimeSubChecked(iTime, XRT_TIME_UNIX_EPOCH, pMilliseconds) ) {
        __xrtTimeSetOverflow("to-unix-ms"); return false;
    }
    return true;
}

XRT_API bool xrtTimeFromUnix32(int32 iSeconds, xtime* pTime)
{
    return xrtTimeFromUnix((int64)iSeconds, pTime);
}

XRT_API bool xrtTimeToUnix32(xtime iTime, int32* pSeconds)
{
    int64 iSeconds;
    if ( pSeconds == NULL ) { __xrtErrorSetInvalidArgument(); return false; }
    iSeconds = xrtTimeUnix(iTime);
    if ( iSeconds < INT32_MIN || iSeconds > INT32_MAX ) {
        __xrtTimeSetOverflow("to-unix32"); return false;
    }
    *pSeconds = (int32)iSeconds;
    return true;
}



#if defined(_WIN32) || defined(_WIN64)

typedef VOID (WINAPI* __xrt_precise_filetime_fn)(LPFILETIME);

/* 动态解析精确墙钟，旧 Windows 自动回退到 GetSystemTimeAsFileTime。 */
static void __xrtTimeSystemFileTime(FILETIME* pFileTime)
{
	static __xrt_precise_filetime_fn pPrecise = NULL;
	static volatile LONG iState = 0;
	LONG iCurrent = InterlockedCompareExchange(&iState, 1, 0);

	if ( iCurrent == 0 ) {
		HMODULE hKernel = GetModuleHandleA("kernel32.dll");

		if ( hKernel != NULL ) {
			pPrecise = (__xrt_precise_filetime_fn)(uintptr_t)
				GetProcAddress(hKernel, "GetSystemTimePreciseAsFileTime");
		}
		InterlockedExchange(&iState, 2);
	} else {
		while ( InterlockedCompareExchange(&iState, 0, 0) == 1 ) {
			Sleep(0);
		}
	}
	if ( pPrecise != NULL ) {
		pPrecise(pFileTime);
	} else {
		GetSystemTimeAsFileTime(pFileTime);
	}
}



/* 缓存 QPC 频率，避免每次读取单调时钟都查询固定系统参数。 */
static uint64 __xrtTimeQpcFrequency(void)
{
	static LARGE_INTEGER tFrequency;
	static volatile LONG iState = 0;
	LONG iCurrent = InterlockedCompareExchange(&iState, 1, 0);

	if ( iCurrent == 0 ) {
		if ( !QueryPerformanceFrequency(&tFrequency) || (tFrequency.QuadPart <= 0) ) {
			tFrequency.QuadPart = 0;
		}
		InterlockedExchange(&iState, 2);
	} else {
		while ( InterlockedCompareExchange(&iState, 0, 0) == 1 ) {
			Sleep(0);
		}
	}
	return (uint64)tFrequency.QuadPart;
}

#endif







/* 返回单调时钟浮点秒数。 */
XRT_API double xrtTimer(void)
{
#if defined(_WIN32) || defined(_WIN64)
    LARGE_INTEGER Counter;
    uint64 Frequency = __xrtTimeQpcFrequency();
    if ( Frequency == 0 || !QueryPerformanceCounter(&Counter) ) {
        __xrtTimeSetError(XERR_IO, XTIME_ERROR_LOCAL_UNSUPPORTED, "timer", "monotonic timer is unavailable", (int)GetLastError());
        return NAN;
    }
    return (double)(Counter.QuadPart / (int64)Frequency) +
        (double)(Counter.QuadPart % (int64)Frequency) / (double)Frequency;
#else
    struct timespec Now;
    if ( clock_gettime(CLOCK_MONOTONIC, &Now) != 0 ) {
        __xrtTimeSetError(XERR_IO, XTIME_ERROR_LOCAL_UNSUPPORTED, "timer", "monotonic timer is unavailable", errno);
        return NAN;
    }
    return (double)Now.tv_sec + (double)Now.tv_nsec * 1e-9;
#endif
}



/* 返回当前公元 UTC 毫秒。 */
XRT_API xtime xrtNow(void)
{
#if defined(_WIN32) || defined(_WIN64)
    FILETIME FileTime;
    uint64 Ticks;
    __xrtTimeSystemFileTime(&FileTime);
    Ticks = ((uint64)FileTime.dwHighDateTime << 32) | FileTime.dwLowDateTime;
    /* FILETIME begins in 1601; this division is nonnegative and exact to ms. */
    return (xtime)(Ticks / UINT64_C(10000)) + INT64_C(50491123200000);
#else
    struct timespec Now;
    xtime Result;
    if ( clock_gettime(CLOCK_REALTIME, &Now) != 0 ) {
        __xrtTimeSetError(XERR_IO, XTIME_ERROR_LOCAL_UNSUPPORTED, "now", "system clock is unavailable", errno);
        return 0;
    }
    if ( !__xrtTimeFromUnixParts((int64)Now.tv_sec, (int)(Now.tv_nsec / 1000000), &Result) ) {
        __xrtTimeSetOverflow("now"); return 0;
    }
    return Result;
#endif
}



/* 至少睡眠指定毫秒，并在 POSIX 信号中断后继续剩余时长。 */




/* 至少睡眠指定毫秒。 */
XRT_API void xrtSleep(int64 iMilliseconds)
{
    if ( iMilliseconds < 0 ) { __xrtErrorSetInvalidArgument(); return; }
#if defined(_WIN32) || defined(_WIN64)
    while ( iMilliseconds >= UINT32_MAX ) {
        Sleep(UINT32_MAX - 1u);
        iMilliseconds -= UINT32_MAX - 1u;
    }
    Sleep((DWORD)iMilliseconds);
#else
    if ( iMilliseconds == 0 ) { sched_yield(); return; }
    while ( iMilliseconds != 0 ) {
        int64 Chunk = iMilliseconds > INT64_C(86400000) ? INT64_C(86400000) : iMilliseconds;
        struct timespec Request;
        Request.tv_sec = (time_t)(Chunk / 1000);
        Request.tv_nsec = (long)((Chunk % 1000) * 1000000);
        while ( nanosleep(&Request, &Request) != 0 ) {
            if ( errno != EINTR ) {
                __xrtTimeSetError(XERR_IO, XTIME_ERROR_LOCAL_UNSUPPORTED, "sleep", "sleep failed", errno);
                return;
            }
        }
        iMilliseconds -= Chunk;
    }
#endif
}



/* 睡眠到单调截止点，使用无符号差值并避免过期后回绕。 */




/* 提取 UTC 年份。 */
XRT_API int64 xrtYear(xtime iTime)
{
	xdatetime tDateTime;

	(void)xrtTimeSplit(iTime, &tDateTime);
	return tDateTime.Year;
}



/* 提取 UTC 月份。 */
XRT_API int xrtMonth(xtime iTime)
{
	xdatetime tDateTime;

	(void)xrtTimeSplit(iTime, &tDateTime);
	return tDateTime.Month;
}



/* 提取 UTC 月内日期。 */
XRT_API int xrtDay(xtime iTime)
{
	xdatetime tDateTime;

	(void)xrtTimeSplit(iTime, &tDateTime);
	return tDateTime.Day;
}



/* 提取 UTC 小时。 */
XRT_API int xrtHour(xtime iTime)
{
	xdatetime tDateTime;

	(void)xrtTimeSplit(iTime, &tDateTime);
	return tDateTime.Hour;
}



/* 提取 UTC 分钟。 */
XRT_API int xrtMinute(xtime iTime)
{
	xdatetime tDateTime;

	(void)xrtTimeSplit(iTime, &tDateTime);
	return tDateTime.Minute;
}



/* 提取 UTC 秒。 */
XRT_API int xrtSecond(xtime iTime)
{
	xdatetime tDateTime;

	(void)xrtTimeSplit(iTime, &tDateTime);
	return tDateTime.Second;
}



/* 提取秒内毫秒。 */
XRT_API int xrtMillisecond(xtime iTime)
{
	xdatetime tDateTime;

	(void)xrtTimeSplit(iTime, &tDateTime);
	return tDateTime.Millisecond;
}



/* 提取星期。 */
XRT_API int xrtWeekday(xtime iTime)
{
	xdatetime tDateTime;

	(void)xrtTimeSplit(iTime, &tDateTime);
	return tDateTime.Weekday;
}



/* 提取年内日期。 */
XRT_API int xrtDayOfYear(xtime iTime)
{
	xdatetime tDateTime;

	(void)xrtTimeSplit(iTime, &tDateTime);
	return tDateTime.YearDay;
}



/* 提取季度。 */
XRT_API int xrtQuarter(xtime iTime)
{
	return ((xrtMonth(iTime) - 1) / 3) + 1;
}



/* 返回 UTC 当日零点，极端负值所在日期无法表示零点时报告溢出。 */
XRT_API xtime xrtDatePart(xtime iTime)
{
	int64 iDays;
	int64 iDayTime;
	int64 iResult;

	__xrtTimeSplitDay(iTime, &iDays, &iDayTime);
	if ( !__xrtTimeMulChecked(iDays, XRT_TIME_DAY, &iResult) ) {
		__xrtTimeSetOverflow("date-part");
		return 0;
	}
	return iResult;
}



/* 返回非负当日毫秒。 */
XRT_API xtime xrtTimePart(xtime iTime)
{
	int64 iDays;
	int64 iDayTime;

	__xrtTimeSplitDay(iTime, &iDays, &iDayTime);
	return iDayTime;
}



/* 使用位模式无符号减法取得完整 int64 域差值。 */
XRT_API bool xrtTimeNear(xtime iLeft, xtime iRight, uint64 iTolerance)
{
	uint64 iDifference = iLeft >= iRight ?
		((uint64)iLeft - (uint64)iRight) : ((uint64)iRight - (uint64)iLeft);

	return iDifference <= iTolerance;
}



/* 比较两个 UTC 时间所在的 Gregorian 日期。 */
XRT_API bool xrtTimeSameDay(xtime iLeft, xtime iRight)
{
	int64 iLeftDays;
	int64 iRightDays;
	int64 iDayTime;

	__xrtTimeSplitDay(iLeft, &iLeftDays, &iDayTime);
	__xrtTimeSplitDay(iRight, &iRightDays, &iDayTime);
	return iLeftDays == iRightDays;
}



/* 比较两个 UTC 时间所在的 Gregorian 月份。 */
XRT_API bool xrtTimeSameMonth(xtime iLeft, xtime iRight)
{
	xdatetime tLeft;
	xdatetime tRight;

	(void)xrtTimeSplit(iLeft, &tLeft);
	(void)xrtTimeSplit(iRight, &tRight);
	return (tLeft.Year == tRight.Year) && (tLeft.Month == tRight.Month);
}



/* 比较两个 UTC 时间所在的 Gregorian 年份。 */
XRT_API bool xrtTimeSameYear(xtime iLeft, xtime iRight)
{
	return xrtYear(iLeft) == xrtYear(iRight);
}



/* 判断时间是否位于合法闭区间。 */
XRT_API bool xrtTimeIn(xtime iTime, xtime iStart, xtime iEnd)
{
	return (iStart <= iEnd) && (iTime >= iStart) && (iTime <= iEnd);
}



/* 判断两个合法闭区间是否重叠。 */
XRT_API bool xrtTimeOverlap(xtime iStart1, xtime iEnd1,
	xtime iStart2, xtime iEnd2)
{
	return (iStart1 <= iEnd1) && (iStart2 <= iEnd2) &&
		(iStart1 <= iEnd2) && (iEnd1 >= iStart2);
}



/* 返回固定时长单位的毫秒数。 */
static bool __xrtTimeUnitDuration(xtimeunit Unit, int64* pDuration)
{
	switch ( Unit ) {
		case XTIME_UNIT_MILLISECOND: *pDuration = XRT_TIME_MILLISECOND; return true;
		case XTIME_UNIT_SECOND: *pDuration = XRT_TIME_SECOND; return true;
		case XTIME_UNIT_MINUTE: *pDuration = XRT_TIME_MINUTE; return true;
		case XTIME_UNIT_HOUR: *pDuration = XRT_TIME_HOUR; return true;
		case XTIME_UNIT_DAY: *pDuration = XRT_TIME_DAY; return true;
		case XTIME_UNIT_WEEK: *pDuration = XRT_TIME_WEEK; return true;
		default: return false;
	}
}



/* 按月增加日期，所有月末日期统一钳制到目标月末。 */
static bool __xrtTimeAddMonths(xtime iTime, int64 iMonths, xtime* pResult)
{
	xdatetime tDateTime;
	int64 iMonthIndex;
	int64 iTarget;
	int64 iTargetYear;
	int iTargetMonth;
	int iTargetDays;

	(void)xrtTimeSplit(iTime, &tDateTime);
	if ( !__xrtTimeMulChecked(tDateTime.Year < 0 ? tDateTime.Year + 1 : tDateTime.Year, 12, &iMonthIndex) ||
		 !__xrtTimeAddChecked(iMonthIndex, tDateTime.Month - 1, &iMonthIndex) ||
		 !__xrtTimeAddChecked(iMonthIndex, iMonths, &iTarget) ) {
		__xrtTimeSetOverflow("add");
		return false;
	}
	iTargetYear = __xrtTimeFloorDiv(iTarget, 12);
	if ( iTargetYear <= 0 ) { iTargetYear--; }
	/* INT64_MIN 的向下整除商乘以 12 会越界；直接规范化余数。 */
	iTargetMonth = (int)(iTarget % 12);
	if ( iTargetMonth < 0 ) {
		iTargetMonth += 12;
	}
	iTargetMonth++;
	iTargetDays = xrtDaysInMonth(iTargetYear, iTargetMonth);
	if ( tDateTime.Day > iTargetDays ) {
		tDateTime.Day = iTargetDays;
	}
	tDateTime.Year = iTargetYear;
	tDateTime.Month = iTargetMonth;
	tDateTime.Offset = 0;
	return xrtTimeMake(&tDateTime, pResult);
}



static bool __xrtTimeAddScaled(xtime Time, int64 Value, int64 Scale, xtime* Result)
{
    bool Negative = Value < 0;
    uint64 Magnitude = Negative ? UINT64_C(0) - (uint64)Value : (uint64)Value;
    uint64 Capacity = Negative ? (uint64)Time - (uint64)INT64_MIN :
        (uint64)INT64_MAX - (uint64)Time;
    uint64 Bits;
    if ( Magnitude > Capacity / (uint64)Scale ) { return false; }
    Magnitude *= (uint64)Scale;
    Bits = Negative ? (uint64)Time - Magnitude : (uint64)Time + Magnitude;
    *Result = Bits <= (uint64)INT64_MAX ? (int64)Bits :
        -INT64_C(1) - (int64)(UINT64_MAX - Bits);
    return true;
}

/* 增加固定时长或日历单位。 */
XRT_API bool xrtTimeAdd(xtime iTime, int64 iValue, xtimeunit Unit, xtime* pResult)
{
	int64 iDuration;
	int64 iMonths;

	if ( pResult == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtTimeUnitDuration(Unit, &iDuration) ) {
		if ( !__xrtTimeAddScaled(iTime, iValue, iDuration, pResult) ) {
			__xrtTimeSetOverflow("add");
			return false;
		}
		return true;
	}
	if ( Unit == XTIME_UNIT_MONTH ) {
		iMonths = iValue;
	} else if ( Unit == XTIME_UNIT_QUARTER ) {
		if ( !__xrtTimeMulChecked(iValue, 3, &iMonths) ) {
			__xrtTimeSetOverflow("add");
			return false;
		}
	} else if ( Unit == XTIME_UNIT_YEAR ) {
		if ( !__xrtTimeMulChecked(iValue, 12, &iMonths) ) {
			__xrtTimeSetOverflow("add");
			return false;
		}
	} else {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	/* The helper diagnoses arithmetic overflow and xrtTimeMake diagnoses the
	 * final date. Propagate its error once: rebuilding here can mask an OOM
	 * while constructing the first diagnostic and needlessly allocate twice. */
	return __xrtTimeAddMonths(iTime, iMonths, pResult);
}



/* 计算两个日期字段之间的粗略月份差。 */
static bool __xrtTimeMonthDifference(xtime iStart, xtime iEnd, int64* pMonths)
{
	xdatetime tStart;
	xdatetime tEnd;
	int64 iYears;
	int64 iMonths;

	(void)xrtTimeSplit(iStart, &tStart);
	(void)xrtTimeSplit(iEnd, &tEnd);
	if ( !__xrtTimeSubChecked(tEnd.Year < 0 ? tEnd.Year + 1 : tEnd.Year,
		tStart.Year < 0 ? tStart.Year + 1 : tStart.Year, &iYears) ||
		 !__xrtTimeMulChecked(iYears, 12, &iMonths) ||
		 !__xrtTimeAddChecked(iMonths, tEnd.Month - tStart.Month, pMonths) ) {
		return false;
	}
	return true;
}



/* 在完整 int64 时间域上计算固定单位差，不要求原始毫秒差可由 int64 表示。 */
static bool __xrtTimeFixedDifference(
	xtime iStart,
	xtime iEnd,
	int64 iDuration,
	int64* pResult
)
{
	bool bNegative = iEnd < iStart;
	uint64 iMagnitude = bNegative ?
		((uint64)iStart - (uint64)iEnd) :
		((uint64)iEnd - (uint64)iStart);
	uint64 iUnits = iMagnitude / (uint64)iDuration;

	if ( !bNegative ) {
		if ( iUnits > (uint64)INT64_MAX ) {
			return false;
		}
		*pResult = (int64)iUnits;
		return true;
	}
	if ( iUnits > (UINT64_C(1) << 63u) ) {
		return false;
	}
	*pResult = iUnits == (UINT64_C(1) << 63u) ?
		INT64_MIN : -(int64)iUnits;
	return true;
}



/* 计算从起点到终点经过的完整单位数量。 */
XRT_API bool xrtDateDiff(xtime iStart, xtime iEnd, xtimeunit Unit, int64* pResult)
{
    int64 Duration;
    int64 Months;
    xdatetime Start;
    xdatetime End;
    if ( pResult == NULL ) { __xrtErrorSetInvalidArgument(); return false; }
    if ( __xrtTimeUnitDuration(Unit, &Duration) ) {
        if ( !__xrtTimeFixedDifference(iStart, iEnd, Duration, pResult) ) {
            __xrtTimeSetOverflow("date-diff"); return false;
        }
        return true;
    }
    if ( Unit == XTIME_UNIT_YEAR ) {
        (void)xrtTimeSplit(iStart, &Start);
        (void)xrtTimeSplit(iEnd, &End);
        *pResult = (End.Year < 0 ? End.Year + 1 : End.Year) -
                   (Start.Year < 0 ? Start.Year + 1 : Start.Year);
        return true;
    }
    if ( Unit != XTIME_UNIT_MONTH && Unit != XTIME_UNIT_QUARTER ) {
        __xrtErrorSetInvalidArgument(); return false;
    }
    if ( !__xrtTimeMonthDifference(iStart, iEnd, &Months) ) {
        __xrtTimeSetOverflow("date-diff"); return false;
    }
    *pResult = Unit == XTIME_UNIT_MONTH ? Months : Months / 3;
    return true;
}



/* 返回包含给定时间的半开月份区间。 */
XRT_API bool xrtMonthRange(xtime iTime, xtime* pStart, xtime* pEnd)
{
	xdatetime tDateTime;
	xtime iStart;
	xtime iEnd;

	if ( (pStart == NULL) && (pEnd == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	(void)xrtTimeSplit(iTime, &tDateTime);
	if ( !xrtDate(tDateTime.Year, tDateTime.Month, 1, &iStart) ||
		 !xrtTimeAdd(iStart, 1, XTIME_UNIT_MONTH, &iEnd) ) {
		return false;
	}
	if ( pStart != NULL ) {
		*pStart = iStart;
	}
	if ( pEnd != NULL ) {
		*pEnd = iEnd;
	}
	return true;
}



/* 返回包含给定时间的半开年份区间。 */
XRT_API bool xrtYearRange(xtime iTime, xtime* pStart, xtime* pEnd)
{
	int64 iYear = xrtYear(iTime);
	xtime iStart;
	xtime iEnd;

	if ( (pStart == NULL) && (pEnd == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtDate(iYear, 1, 1, &iStart) ||
		 !xrtTimeAdd(iStart, 1, XTIME_UNIT_YEAR, &iEnd) ) {
		return false;
	}
	if ( pStart != NULL ) {
		*pStart = iStart;
	}
	if ( pEnd != NULL ) {
		*pEnd = iEnd;
	}
	return true;
}



/* 返回包含给定时间的半开星期区间。 */
XRT_API bool xrtWeekRange(xtime iTime, int iFirstWeekday, xtime* pStart, xtime* pEnd)
{
	int64 iDays;
	int64 iDayTime;
	int64 iStartDays;
	xtime iStart;
	xtime iEnd;
	int iWeekday;
	int iDifference;

	if ( ((pStart == NULL) && (pEnd == NULL)) ||
		 (iFirstWeekday < XTIME_SUNDAY) || (iFirstWeekday > XTIME_SATURDAY) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	__xrtTimeSplitDay(iTime, &iDays, &iDayTime);
	iWeekday = (int)((iDays + 1) % 7);
	if ( iWeekday < 0 ) {
		iWeekday += 7;
	}
	iDifference = iWeekday - iFirstWeekday;
	if ( iDifference < 0 ) {
		iDifference += 7;
	}
	iStartDays = iDays - iDifference;
	if ( !__xrtTimeMulChecked(iStartDays, XRT_TIME_DAY, &iStart) ||
		 !__xrtTimeAddChecked(iStart, XRT_TIME_WEEK, &iEnd) ) {
		__xrtTimeSetOverflow("week-range");
		return false;
	}
	if ( pStart != NULL ) {
		*pStart = iStart;
	}
	if ( pEnd != NULL ) {
		*pEnd = iEnd;
	}
	return true;
}



/* 计算符合 ISO 8601 的周年、周数和星期值。 */
XRT_API bool xrtISOWeek(xtime iTime, int64* pWeekYear, int* pWeek, int* pWeekday)
{
	int64 iDays;
	int64 iDayTime;
	int64 iThursday;
	int64 iWeekYear;
	int64 iJanuary4;
	int64 iWeek1Monday;
	int iSundayWeekday;
	int iISOWeekday;
	int iJanuary4Weekday;

	if ( (pWeekYear == NULL) && (pWeek == NULL) && (pWeekday == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	__xrtTimeSplitDay(iTime, &iDays, &iDayTime);
	iSundayWeekday = (int)((iDays + 1) % 7);
	if ( iSundayWeekday < 0 ) {
		iSundayWeekday += 7;
	}
	iISOWeekday = ((iSundayWeekday + 6) % 7) + 1;
	iThursday = iDays + (4 - iISOWeekday);
	__xrtTimeCivilFromDays(iThursday, &iWeekYear,
		&iJanuary4Weekday, &iSundayWeekday);
	if ( !__xrtTimeDaysFromCivil(iWeekYear, 1, 4, &iJanuary4) ) {
		__xrtTimeSetOverflow("iso-week");
		return false;
	}
	iJanuary4Weekday = (int)((iJanuary4 + 1) % 7);
	if ( iJanuary4Weekday < 0 ) {
		iJanuary4Weekday += 7;
	}
	iJanuary4Weekday = ((iJanuary4Weekday + 6) % 7) + 1;
	iWeek1Monday = iJanuary4 - (iJanuary4Weekday - 1);
	if ( pWeekYear != NULL ) {
		*pWeekYear = iWeekYear;
	}
	if ( pWeek != NULL ) {
		*pWeek = (int)((iDays - iWeek1Monday) / 7) + 1;
	}
	if ( pWeekday != NULL ) {
		*pWeekday = iISOWeekday;
	}
	return true;
}

#endif
