#include "../internal/xrt_time.h"

#include <errno.h>
#include <time.h>



#if defined(XRT_FEATURE_TIME_LOCAL)

/* 本地时间候选搜索覆盖前后三天，并以半小时采样所有可能偏移。 */
#define XRT_LOCAL_SEARCH_RANGE	(INT64_C(3) * XRT_TIME_DAY)
#define XRT_LOCAL_SEARCH_STEP	(INT64_C(30) * XRT_TIME_MINUTE)
#define XRT_LOCAL_OFFSET_CACHE	16



/* 把 xtime 拆成向负无穷取整的秒和非负秒内微秒。 */
static void __xrtTimeSplitSecond(xtime iTime, int64* pSeconds, int* pMicrosecond)
{
	int64 iSeconds = iTime / XRT_TIME_SECOND;
	int64 iMicrosecond = iTime % XRT_TIME_SECOND;

	if ( iMicrosecond < 0 ) {
		iSeconds--;
		iMicrosecond += XRT_TIME_SECOND;
	}
	*pSeconds = iSeconds;
	*pMicrosecond = (int)iMicrosecond;
}



/* 用已经确定的本地日期和原始 Unix 秒填写公共结构。 */
static bool __xrtTimeFillLocal(xdatetime* pDateTime, int64 iUnixSeconds,
	int64 iYear, int iMonth, int iDay, int iHour, int iMinute, int iSecond,
	int iMicrosecond, int iIsDST)
{
	int64 iDays;
	int64 iYearStart;
	int64 iLocalSeconds;
	int64 iComponentSeconds;
	int64 iOffset;
	int iWeekday;

	if ( !__xrtTimeDaysFromCivil(iYear, iMonth, iDay, &iDays) ||
		 !__xrtTimeMulChecked(iDays, INT64_C(86400), &iLocalSeconds) ||
		 !__xrtTimeMulChecked((int64)iHour, 3600, &iComponentSeconds) ||
		 !__xrtTimeAddChecked(iLocalSeconds, iComponentSeconds, &iLocalSeconds) ||
		 !__xrtTimeMulChecked((int64)iMinute, 60, &iComponentSeconds) ||
		 !__xrtTimeAddChecked(iLocalSeconds, iComponentSeconds, &iLocalSeconds) ||
		 !__xrtTimeAddChecked(iLocalSeconds, (int64)iSecond, &iLocalSeconds) ||
		 !__xrtTimeSubChecked(iLocalSeconds, iUnixSeconds, &iOffset) ) {
		return false;
	}
	if ( (iOffset <= -86400) || (iOffset >= 86400) ) {
		return false;
	}
	iWeekday = (int)((iDays + 4) % 7);
	if ( iWeekday < 0 ) {
		iWeekday += 7;
	}
	if ( !__xrtTimeDaysFromCivil(iYear, 1, 1, &iYearStart) ) {
		return false;
	}

	memset(pDateTime, 0, sizeof(*pDateTime));
	pDateTime->Year = iYear;
	pDateTime->Month = iMonth;
	pDateTime->Day = iDay;
	pDateTime->Hour = iHour;
	pDateTime->Minute = iMinute;
	pDateTime->Second = iSecond;
	pDateTime->Microsecond = iMicrosecond;
	pDateTime->Offset = (int)iOffset;
	pDateTime->Weekday = iWeekday;
	pDateTime->YearDay = (int)(iDays - iYearStart) + 1;
	pDateTime->IsDST = iIsDST;
	return true;
}



#if defined(_WIN32) || defined(_WIN64)

typedef BOOL (WINAPI* __xrt_system_to_local_ex_fn)(
	const void*,
	const SYSTEMTIME*,
	LPSYSTEMTIME
);



/* 优先使用动态 DST 转换，旧 Windows 再回退到传统时区入口。 */
static bool __xrtTimeSystemToLocal(
	const SYSTEMTIME* pUTC, SYSTEMTIME* pLocal, int* pSystemCode)
{
	static __xrt_system_to_local_ex_fn pConvert = NULL;
	static volatile LONG iState = 0;
	LONG iCurrent = InterlockedCompareExchange(&iState, 1, 0);
	BOOL bResult;

	if ( iCurrent == 0 ) {
		HMODULE hKernel = GetModuleHandleA("kernel32.dll");

		if ( hKernel != NULL ) {
			pConvert = (__xrt_system_to_local_ex_fn)(uintptr_t)
				GetProcAddress(hKernel, "SystemTimeToTzSpecificLocalTimeEx");
		}
		InterlockedExchange(&iState, 2);
	} else {
		while ( InterlockedCompareExchange(&iState, 0, 0) == 1 ) {
			Sleep(0);
		}
	}
	if ( pConvert != NULL ) {
		bResult = pConvert(NULL, pUTC, pLocal);
	} else {
		bResult = SystemTimeToTzSpecificLocalTime(
			NULL, (LPSYSTEMTIME)(uintptr_t)pUTC, pLocal);
	}
	if ( !bResult && (pSystemCode != NULL) ) {
		*pSystemCode = (int)GetLastError();
	}
	return bResult != FALSE;
}



/* 把可表示的 Unix 微秒转换为 Windows FILETIME。 */
static bool __xrtTimeToFileTime(xtime iTime, FILETIME* pFileTime)
{
	const int64 iEpochSeconds = INT64_C(11644473600);
	int64 iSeconds;
	int iMicrosecond;
	uint64 iFileSeconds;
	uint64 iTicks;

	__xrtTimeSplitSecond(iTime, &iSeconds, &iMicrosecond);
	if ( iSeconds < -iEpochSeconds ) {
		return false;
	}
	iFileSeconds = (uint64)(iSeconds + iEpochSeconds);
	if ( iFileSeconds > (UINT64_MAX / UINT64_C(10000000)) ) {
		return false;
	}
	iTicks = (iFileSeconds * UINT64_C(10000000)) +
		((uint64)iMicrosecond * UINT64_C(10));
	pFileTime->dwLowDateTime = (DWORD)iTicks;
	pFileTime->dwHighDateTime = (DWORD)(iTicks >> 32);
	return true;
}



/* Windows 使用无共享缓冲的系统时区转换 API。 */
bool __xrtTimeLocalParts(xtime iTime, xdatetime* pDateTime, int* pSystemCode)
{
	FILETIME tFileTime;
	SYSTEMTIME tUTC;
	SYSTEMTIME tLocal;
	int64 iUnixSeconds;
	int iMicrosecond;

	if ( !__xrtTimeToFileTime(iTime, &tFileTime) ) {
		if ( pSystemCode != NULL ) {
			*pSystemCode = ERROR_ARITHMETIC_OVERFLOW;
		}
		return false;
	}
	if ( !FileTimeToSystemTime(&tFileTime, &tUTC) ) {
		if ( pSystemCode != NULL ) {
			*pSystemCode = (int)GetLastError();
		}
		return false;
	}
	if ( !__xrtTimeSystemToLocal(&tUTC, &tLocal, pSystemCode) ) {
		return false;
	}
	__xrtTimeSplitSecond(iTime, &iUnixSeconds, &iMicrosecond);
	if ( !__xrtTimeFillLocal(pDateTime, iUnixSeconds,
		(int64)tLocal.wYear, (int)tLocal.wMonth, (int)tLocal.wDay,
		(int)tLocal.wHour, (int)tLocal.wMinute, (int)tLocal.wSecond,
		iMicrosecond, -1) ) {
		if ( pSystemCode != NULL ) {
			*pSystemCode = ERROR_ARITHMETIC_OVERFLOW;
		}
		return false;
	}
	return true;
}

#else

/* POSIX 使用线程安全 localtime_r，并从返回字段推导该时刻真实偏移。 */
bool __xrtTimeLocalParts(xtime iTime, xdatetime* pDateTime, int* pSystemCode)
{
	int64 iUnixSeconds;
	int iMicrosecond;
	time_t iSystemTime;
	struct tm tLocal;

	__xrtTimeSplitSecond(iTime, &iUnixSeconds, &iMicrosecond);
	iSystemTime = (time_t)iUnixSeconds;
	if ( ((int64)iSystemTime != iUnixSeconds) ||
		 ((iUnixSeconds < 0) && (iSystemTime >= (time_t)0)) ) {
		if ( pSystemCode != NULL ) {
			*pSystemCode = EOVERFLOW;
		}
		return false;
	}
	errno = 0;
	if ( localtime_r(&iSystemTime, &tLocal) == NULL ) {
		if ( pSystemCode != NULL ) {
			*pSystemCode = errno != 0 ? errno : EOVERFLOW;
		}
		return false;
	}
	if ( !__xrtTimeFillLocal(pDateTime, iUnixSeconds,
		(int64)tLocal.tm_year + 1900, tLocal.tm_mon + 1, tLocal.tm_mday,
		tLocal.tm_hour, tLocal.tm_min, tLocal.tm_sec, iMicrosecond,
		tLocal.tm_isdst > 0 ? 1 : 0) ) {
		if ( pSystemCode != NULL ) {
			*pSystemCode = EOVERFLOW;
		}
		return false;
	}
	return true;
}

#endif



/* 使用操作系统当前时区规则分解绝对时间。 */
XRT_API bool xrtTimeLocal(xtime iTime, xdatetime* pDateTime)
{
	xdatetime tResult;
	int iSystemCode = 0;

	if ( pDateTime == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtTimeLocalParts(iTime, &tResult, &iSystemCode) ) {
		__xrtTimeSetError(XERR_UNSUPPORTED, XTIME_ERROR_LOCAL_UNSUPPORTED,
			"local", "time is outside the operating system timezone range",
			iSystemCode);
		return false;
	}
	*pDateTime = tResult;
	return true;
}



/* 判断两个分解值是否表示相同的本地墙钟字段。 */
static bool __xrtTimeLocalEqual(const xdatetime* pLeft, const xdatetime* pRight)
{
	return (pLeft->Year == pRight->Year) &&
		(pLeft->Month == pRight->Month) &&
		(pLeft->Day == pRight->Day) &&
		(pLeft->Hour == pRight->Hour) &&
		(pLeft->Minute == pRight->Minute) &&
		(pLeft->Second == pRight->Second) &&
		(pLeft->Microsecond == pRight->Microsecond);
}



/* 记录已经检查过的偏移；缓存满后继续检查，绝不因容量丢弃候选。 */
static bool __xrtTimeOffsetIsNew(int* arrOffsets, size_t* pCount, int iOffset)
{
	for ( size_t i = 0; i < *pCount; i++ ) {
		if ( arrOffsets[i] == iOffset ) {
			return false;
		}
	}
	if ( *pCount < XRT_LOCAL_OFFSET_CACHE ) {
		arrOffsets[*pCount] = iOffset;
		(*pCount)++;
	}
	return true;
}



/* 把有效候选合并为最早、最晚和是否歧义三个稳定状态。 */
static void __xrtTimeAddCandidate(xtime iCandidate, bool* pHasCandidate,
	bool* pAmbiguous, xtime* pEarlier, xtime* pLater)
{
	if ( !*pHasCandidate ) {
		*pHasCandidate = true;
		*pEarlier = iCandidate;
		*pLater = iCandidate;
		return;
	}
	if ( iCandidate < *pEarlier ) {
		*pEarlier = iCandidate;
		*pAmbiguous = true;
	} else if ( iCandidate > *pLater ) {
		*pLater = iCandidate;
		*pAmbiguous = true;
	}
}



/* 枚举附近偏移并回验本地墙钟，只保留选择 fold 所需的边界候选。 */
static void __xrtTimeCollectCandidates(xtime iNaive, const xdatetime* pExpected,
	bool* pHasCandidate, bool* pAmbiguous, xtime* pEarlier, xtime* pLater)
{
	int arrOffsets[XRT_LOCAL_OFFSET_CACHE];
	xtime iStart;
	xtime iEnd;
	xtime iProbe;
	size_t iOffsetCount = 0;

	iStart = iNaive < (INT64_MIN + XRT_LOCAL_SEARCH_RANGE) ?
		INT64_MIN : iNaive - XRT_LOCAL_SEARCH_RANGE;
	iEnd = iNaive > (INT64_MAX - XRT_LOCAL_SEARCH_RANGE) ?
		INT64_MAX : iNaive + XRT_LOCAL_SEARCH_RANGE;
	iProbe = iStart;
	for ( ;; ) {
		xdatetime tProbe;

		if ( __xrtTimeLocalParts(iProbe, &tProbe, NULL) &&
			 __xrtTimeOffsetIsNew(arrOffsets, &iOffsetCount, tProbe.Offset) ) {
			xdatetime tActual;
			xtime iCandidate;
			int64 iDelta = -((int64)tProbe.Offset * XRT_TIME_SECOND);

			if ( __xrtTimeAddChecked(iNaive, iDelta, &iCandidate) &&
				 __xrtTimeLocalParts(iCandidate, &tActual, NULL) &&
				 __xrtTimeLocalEqual(&tActual, pExpected) ) {
				__xrtTimeAddCandidate(iCandidate, pHasCandidate,
					pAmbiguous, pEarlier, pLater);
			}
		}
		if ( iProbe >= iEnd ) {
			break;
		}
		if ( (iEnd - iProbe) < XRT_LOCAL_SEARCH_STEP ) {
			iProbe = iEnd;
		} else {
			iProbe += XRT_LOCAL_SEARCH_STEP;
		}
	}
}



/* 使用候选偏移反解本地墙钟，能够识别 DST gap 和 fold。 */
XRT_API bool xrtTimeFromLocal(const xdatetime* pDateTime, xtimefold Fold, xtime* pTime)
{
	xdatetime tNaiveParts;
	xtime iNaive;
	xtime iEarlier = 0;
	xtime iLater = 0;
	bool bHasCandidate = false;
	bool bAmbiguous = false;

	if ( (pDateTime == NULL) || (pTime == NULL) ||
		 (Fold < XTIME_FOLD_REJECT) || (Fold > XTIME_FOLD_LATER) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	tNaiveParts = *pDateTime;
	tNaiveParts.Offset = 0;
	if ( !xrtTimeMake(&tNaiveParts, &iNaive) ) {
		return false;
	}

	__xrtTimeCollectCandidates(iNaive, pDateTime, &bHasCandidate,
		&bAmbiguous, &iEarlier, &iLater);

	if ( !bHasCandidate ) {
		__xrtTimeSetError(XERR_VALUE, XTIME_ERROR_LOCAL_GAP,
			"from-local", "local wall time does not exist in the system timezone", 0);
		return false;
	}
	if ( bAmbiguous && (Fold == XTIME_FOLD_REJECT) ) {
		__xrtTimeSetError(XERR_VALUE, XTIME_ERROR_LOCAL_FOLD,
			"from-local", "local wall time is ambiguous in the system timezone", 0);
		return false;
	}
	*pTime = Fold == XTIME_FOLD_LATER ? iLater : iEarlier;
	return true;
}

#endif
