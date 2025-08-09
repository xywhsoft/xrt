


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



XXAPI ustr xrtTimeNowStr()
{
	return NULL;
}


