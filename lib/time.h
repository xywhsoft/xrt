


// 获取字符串格式的当前日期 + 时间（ 需使用 xrtFree 释放内存 ）
XXAPI ustr xrtNowStr()
{
	time_t rawtime = time(NULL);
	struct tm* pstm = localtime(&rawtime);
	return xrtFormat("%04d-%02d-%02d %02d:%02d:%02d", 1900 + pstm->tm_year, pstm->tm_mon, pstm->tm_mday, pstm->tm_hour, pstm->tm_min, pstm->tm_sec);
}
XXAPI wstr xrtNowStrW()
{
	time_t rawtime = time(NULL);
	struct tm* pstm = localtime(&rawtime);
	return xrtFormatW(L"%04d-%02d-%02d %02d:%02d:%02d", 1900 + pstm->tm_year, pstm->tm_mon, pstm->tm_mday, pstm->tm_hour, pstm->tm_min, pstm->tm_sec);
}



// 获取字符串格式的当前日期（ 需使用 xrtFree 释放内存 ）
XXAPI ustr xrtDateStr()
{
	time_t rawtime = time(NULL);
	struct tm* pstm = localtime(&rawtime);
	return xrtFormat("%04d-%02d-%02d", 1900 + pstm->tm_year, pstm->tm_mon, pstm->tm_mday);
}
XXAPI wstr xrtDateStrW()
{
	time_t rawtime = time(NULL);
	struct tm* pstm = localtime(&rawtime);
	return xrtFormatW(L"%04d-%02d-%02d", 1900 + pstm->tm_year, pstm->tm_mon, pstm->tm_mday);
}



// 获取字符串格式的当前时间（ 需使用 xrtFree 释放内存 ）
XXAPI ustr xrtTimeStr()
{
	time_t rawtime = time(NULL);
	struct tm* pstm = localtime(&rawtime);
	return xrtFormat("%02d:%02d:%02d", pstm->tm_hour, pstm->tm_min, pstm->tm_sec);
}
XXAPI wstr xrtTimeStrW()
{
	time_t rawtime = time(NULL);
	struct tm* pstm = localtime(&rawtime);
	return xrtFormatW(L"%02d:%02d:%02d", pstm->tm_hour, pstm->tm_min, pstm->tm_sec);
}



// 判断是否为闰年
XXAPI int xrtIsLeapYear(int iYear)
{
	if ( (iYear % 400) == 0 ) {
		return TRUE;
	} else if ( (iYear % 100) == 0 ) {
		return FALSE;
	} else {
		if ( (iYear & 3) == 0 ) {
			return TRUE;
		} else {
			return FALSE;
		}
	}
}



// 获取某年某月有多少天
XXAPI int xrtDaysInMonth(int iYear, int iMonth)
{
	static const int arrDays[] = { 31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	if ( (iMonth >= 1) && (iMonth <= 12) ) {
		if ( iMonth == 2 ) {
			if ( xrtIsLeapYear(iYear) ) {
				return 29;
			} else {
				return 28;
			}
		} else {
			return arrDays[iMonth - 1];
		}
	} else {
		return 0;
	}
}



// 获取某年有多少天
XXAPI int xrtDaysInYear(int iYear)
{
	if ( xrtIsLeapYear(iYear) ) {
		return 366;
	} else {
		return 365;
	}
}



// 构建时间
XXAPI xtime xrtTimeSerial(int iHour, int iMinute, int iSecond)
{
	return (iHour * XRT_TIME_HOUR) + (iMinute * XRT_TIME_MINUTE) + iSecond;
}



// 构建日期
XXAPI xtime xrtDateSerial(int64 iYear, int iMonth, int iDay)
{
	if ( (iMonth < 1) || (iMonth > 12) ) {
		xrtSetError(xCore.ERROR_DESC.MONTHRANGE, FALSE);
		return 0;
	}
	xtime iDate = (iDay - 1) * XRT_TIME_DAY;
	for ( int i = 1; i < iMonth; i++ ) {
		iDate += xrtDaysInMonth(iYear, i) * XRT_TIME_DAY;
	}
	if ( iYear < 0 ) {
		// 公元前
		iYear = llabs(iYear);
		uint64 iYear400 = iYear / 400;
		uint64 iYearMod = iYear % 400;
		for ( int i = 0; i < iYearMod; i++ ) {
			iDate += xrtDaysInYear(i) * XRT_TIME_DAY;
		}
		iDate += iYear400 * XRT_TIME_400YEAR;
		iDate = iDate * -1;
	} else {
		// 公元后
		uint64 iYear400 = iYear / 400;
		uint64 iYearMod = iYear % 400;
		for ( int i = 0; i < iYearMod; i++ ) {
			iDate += xrtDaysInYear(i) * XRT_TIME_DAY;
		}
		iDate += iYear400 * XRT_TIME_400YEAR;
	}
	return iDate;
}



// 构建日期 + 时间
XXAPI xtime xrtDateTimeSerial(int64 iYear, int iMonth, int iDay, int iHour, int iMinute, int iSecond)
{
	xtime iTime = (iHour * XRT_TIME_HOUR) + (iMinute * XRT_TIME_MINUTE) + iSecond;
	xtime iDate = xrtDateSerial(iYear, iMonth, iDay);
	return iDate + iTime;
}



// 获取时间中的秒
XXAPI int xrtSecond(xtime iTime)
{
	return iTime % XRT_TIME_MINUTE;
}



// 获取时间中的分钟
XXAPI int xrtMinute(xtime iTime)
{
	return (iTime % XRT_TIME_HOUR) / 60;
}



// 获取时间中的小时
XXAPI int xrtHour(xtime iTime)
{
	return (iTime % XRT_TIME_DAY) / XRT_TIME_HOUR;
}



// 获取时间中的日期
XXAPI int xrtDay(xtime iTime)
{
	xtime iTimeAbs = llabs(iTime);
	uint64 iYear400 = iTimeAbs / XRT_TIME_400YEAR;
	uint64 iYearMod = iTimeAbs % XRT_TIME_400YEAR;
	uint64 iYear = 0;
	for ( int i = 0; i < 400; i++ ) {
		uint64 iSec =  xrtDaysInYear(i) * XRT_TIME_DAY;
		if ( iYearMod >= iSec ) {
			iYearMod -= iSec;
		} else {
			iYear = i;
			break;
		}
	}
	int iMonth = 1;
	for ( int i = 1; i <= 12; i++ ) {
		uint64 iSec =  xrtDaysInMonth(iYear, i) * XRT_TIME_DAY;
		if ( iYearMod >= iSec ) {
			iYearMod -= iSec;
		} else {
			iMonth = i;
			break;
		}
	}
	int iDay = 1;
	for ( int i = 1; i <= 31; i++ ) {
		if ( iYearMod >= XRT_TIME_DAY ) {
			iYearMod -= XRT_TIME_DAY;
		} else {
			iDay = i;
			break;
		}
	}
	return iDay;
}



// 获取时间中的月份
XXAPI int xrtMonth(xtime iTime)
{
	xtime iTimeAbs = llabs(iTime);
	uint64 iYear400 = iTimeAbs / XRT_TIME_400YEAR;
	uint64 iYearMod = iTimeAbs % XRT_TIME_400YEAR;
	uint64 iYear = 0;
	for ( int i = 0; i < 400; i++ ) {
		uint64 iSec =  xrtDaysInYear(i) * XRT_TIME_DAY;
		if ( iYearMod >= iSec ) {
			iYearMod -= iSec;
		} else {
			iYear = i;
			break;
		}
	}
	int iMonth = 1;
	for ( int i = 1; i <= 12; i++ ) {
		uint64 iSec =  xrtDaysInMonth(iYear, i) * XRT_TIME_DAY;
		if ( iYearMod >= iSec ) {
			iYearMod -= iSec;
		} else {
			iMonth = i;
			break;
		}
	}
	return iMonth;
}



// 获取时间中的年份
XXAPI int xrtYear(xtime iTime)
{
	xtime iTimeAbs = llabs(iTime);
	uint64 iYear400 = iTimeAbs / XRT_TIME_400YEAR;
	uint64 iYearMod = iTimeAbs % XRT_TIME_400YEAR;
	uint64 iYear = iYear400 * 400;
	for ( int i = 0; i < 400; i++ ) {
		uint64 iSec =  xrtDaysInYear(i) * XRT_TIME_DAY;
		if ( iYearMod >= iSec ) {
			iYearMod -= iSec;
			iYear++;
		} else {
			break;
		}
	}
	if ( iTime < 0 ) {
		return -iYear;
	} else {
		return iYear;
	}
}



// 获取时间中的星期
XXAPI int xrtWeekday(xtime iTime)
{
	xtime iTimeAbs = llabs(iTime);
	uint64 iDay = iTimeAbs / XRT_TIME_DAY;
	return iDay % 7;
}



// 获取时间是当年的第几天
XXAPI int xrtDayOfYear(xtime iTime)
{
	xtime iTimeAbs = llabs(iTime);
	uint64 iYear400 = iTimeAbs / XRT_TIME_400YEAR;
	uint64 iYearMod = iTimeAbs % XRT_TIME_400YEAR;
	uint64 iYear = iYear400 * 400;
	for ( int i = 0; i < 400; i++ ) {
		uint64 iSec =  xrtDaysInYear(i) * XRT_TIME_DAY;
		if ( iYearMod >= iSec ) {
			iYearMod -= iSec;
			iYear++;
		} else {
			break;
		}
	}
	return 1 + (iYearMod / XRT_TIME_DAY);
}



// 解码时间
XXAPI void xrtDecodeSerial(xtime iTime, int64* pYear, int* pMonth, int* pDay, int* pHour, int* pMinute, int* pSecond, int* pWeekday, int* pDayOfYear)
{
	xtime iTimeAbs = llabs(iTime);
	uint64 iYear400 = iTimeAbs / XRT_TIME_400YEAR;
	uint64 iYearMod = iTimeAbs % XRT_TIME_400YEAR;
	uint64 iYear = iYear400 * 400;
	for ( int i = 0; i < 400; i++ ) {
		uint64 iSec =  xrtDaysInYear(i) * XRT_TIME_DAY;
		if ( iYearMod >= iSec ) {
			iYearMod -= iSec;
			iYear++;
		} else {
			break;
		}
	}
	if ( pYear ) {
		if ( iTime < 0 ) {
			*pYear = -iYear;
		} else {
			*pYear = iYear;
		}
	}
	if ( pDayOfYear ) {
		*pDayOfYear = 1 + (iYearMod / XRT_TIME_DAY);
	}
	int iMonth = 1;
	for ( int i = 1; i <= 12; i++ ) {
		uint64 iSec =  xrtDaysInMonth(iYear, i) * XRT_TIME_DAY;
		if ( iYearMod >= iSec ) {
			iYearMod -= iSec;
		} else {
			iMonth = i;
			break;
		}
	}
	if ( pMonth ) {
		*pMonth = iMonth;
	}
	int iDay = 1;
	for ( int i = 1; i <= 31; i++ ) {
		if ( iYearMod >= XRT_TIME_DAY ) {
			iYearMod -= XRT_TIME_DAY;
		} else {
			iDay = i;
			break;
		}
	}
	if ( pDay ) {
		*pDay = iDay;
	}
	if ( pHour ) {
		*pHour = (iTimeAbs % XRT_TIME_DAY) / XRT_TIME_HOUR;
	}
	if ( pMinute ) {
		*pMinute = (iTimeAbs % XRT_TIME_HOUR) / 60;
	}
	if ( pSecond ) {
		*pSecond = iTimeAbs % XRT_TIME_MINUTE;
	}
	if ( pWeekday ) {
		uint64 iDay = iTimeAbs / XRT_TIME_DAY;
		*pWeekday = iDay % 7;
	}
}



// 获取当前日期 + 时间
XXAPI xtime xrtNow()
{
	time_t rawtime = time(NULL);
	struct tm* pstm = localtime(&rawtime);
	return xrtDateTimeSerial(1900 + pstm->tm_year, pstm->tm_mon, pstm->tm_mday, pstm->tm_hour, pstm->tm_min, pstm->tm_sec);
}



// 获取当前日期
XXAPI xtime xrtDate()
{
	time_t rawtime = time(NULL);
	struct tm* pstm = localtime(&rawtime);
	return xrtDateSerial(1900 + pstm->tm_year, pstm->tm_mon, pstm->tm_mday);
}



// 获取当前时间
XXAPI xtime xrtTime()
{
	time_t rawtime = time(NULL);
	struct tm* pstm = localtime(&rawtime);
	return xrtTimeSerial(pstm->tm_hour, pstm->tm_min, pstm->tm_sec);
}



/*
	DateAdd
	DateDiff
*/


