


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
XXAPI ustr xrtNowDateStr()
{
	time_t rawtime = time(NULL);
	struct tm* pstm = localtime(&rawtime);
	return xrtFormat("%04d-%02d-%02d", 1900 + pstm->tm_year, pstm->tm_mon, pstm->tm_mday);
}
XXAPI wstr xrtNowDateStrW()
{
	time_t rawtime = time(NULL);
	struct tm* pstm = localtime(&rawtime);
	return xrtFormatW(L"%04d-%02d-%02d", 1900 + pstm->tm_year, pstm->tm_mon, pstm->tm_mday);
}



// 获取字符串格式的当前时间（ 需使用 xrtFree 释放内存 ）
XXAPI ustr xrtNowTimeStr()
{
	time_t rawtime = time(NULL);
	struct tm* pstm = localtime(&rawtime);
	return xrtFormat("%02d:%02d:%02d", pstm->tm_hour, pstm->tm_min, pstm->tm_sec);
}
XXAPI wstr xrtNowTimeStrW()
{
	time_t rawtime = time(NULL);
	struct tm* pstm = localtime(&rawtime);
	return xrtFormatW(L"%02d:%02d:%02d", pstm->tm_hour, pstm->tm_min, pstm->tm_sec);
}



/*
	Now
	Time
	Date
	Year
	Month
	Day
	Hour
	Minute
	Second
	Weekday
	DateAdd
	DateDiff
*/


