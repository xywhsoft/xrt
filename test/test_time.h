


// Time 库测试
void Test_Time(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n Time 库测试 :\n\n");
	for ( int i = 1; i <= 12; i++ ) {
		printf("xrtDateSerial (0-%02d-01 00:00:00) : %d\n", i, xrtDateSerial(0, i, 1));
	}
	printf("xrtDateSerial (-5-01-01 00:00:00) : %d\n", xrtDateSerial(-5, 1, 1));
	printf("xrtDateSerial (-2-01-01 00:00:00) : %d\n", xrtDateSerial(-2, 1, 1));
	printf("xrtDateSerial (-1-01-01 00:00:00) : %d\n", xrtDateSerial(-1, 1, 1));
	printf("xrtDateSerial (0-01-01 00:00:00) : %d\n", xrtDateSerial(0, 1, 1));
	printf("xrtDateSerial (1-01-01 00:00:00) : %d\n", xrtDateSerial(1, 1, 1));
	printf("xrtDateSerial (2-01-01 00:00:00) : %d\n", xrtDateSerial(2, 1, 1));
	printf("xrtDateSerial (5-01-01 00:00:00) : %d\n", xrtDateSerial(5, 1, 1));
	printf("xrtDateSerial (1970-01-01 00:00:00) : %lld\n", xrtDateSerial(1970, 1, 1));
	printf("xrtTimeSerial (12:00:00) : %d\n", xrtTimeSerial(12, 0, 0));
	xtime iTime = xrtDateTimeSerial(2012, 3, 15, 12, 20, 40);
	printf("xrtDateTimeSerial (2012-03-15 12:20:40) : %lld\n", iTime);
	printf("xrtYear (2012-03-15 12:20:40) : %d\n", xrtYear(iTime));
	printf("xrtMonth (2012-03-15 12:20:40) : %d\n", xrtMonth(iTime));
	printf("xrtDay (2012-03-15 12:20:40) : %d\n", xrtDay(iTime));
	printf("xrtHour (2012-03-15 12:20:40) : %d\n", xrtHour(iTime));
	printf("xrtMinute (2012-03-15 12:20:40) : %d\n", xrtMinute(iTime));
	printf("xrtSecond (2012-03-15 12:20:40) : %d\n", xrtSecond(iTime));
	printf("xrtWeekday (2012-03-15 12:20:40) : %d\n", xrtWeekday(iTime));
	printf("xrtDayOfYear (2012-03-15 12:20:40) : %d\n", xrtDayOfYear(iTime));
	int64 iYear;
	int iMonth, iDay, iHour, iMinute, iSecond, iWeekday, iDayOfYear;
	xrtDecodeSerial(iTime, &iYear, &iMonth, &iDay, &iHour, &iMinute, &iSecond, &iWeekday, &iDayOfYear);
	printf("xrtDecodeSerial : %d-%d-%d %d:%d:%d ( %d - %d )\n", iYear, iMonth, iDay, iHour, iMinute, iSecond, iWeekday, iDayOfYear);
	printf("xrtTimeToStr : %s\n", xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME));
	printf("xrtTimeToStr : %s\n", xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATE));
	printf("xrtTimeToStr : %s\n", xrtTimeToStr(iTime, XRT_TIME_FORMAT_TIME));
	
	xtime tRet = xrtDateAdd(XRT_TIME_INTERVAL_SECOND, 30, iTime);
	int64 iDiff = xrtDateDiff(XRT_TIME_INTERVAL_SECOND, iTime, tRet);
	printf("xrtDateAdd (2012-03-15 12:20:40) 30 sec : %s\n", xrtTimeToStr(tRet, XRT_TIME_FORMAT_DATETIME));
	printf("xrtDateDiff 30 sec : %d\n", iDiff);
	tRet = xrtDateAdd(XRT_TIME_INTERVAL_MINUTE, 30, iTime);
	iDiff = xrtDateDiff(XRT_TIME_INTERVAL_MINUTE, iTime, tRet);
	printf("xrtDateAdd (2012-03-15 12:20:40) 30 min : %s\n", xrtTimeToStr(tRet, XRT_TIME_FORMAT_DATETIME));
	printf("xrtDateDiff 30 min : %d\n", iDiff);
	tRet = xrtDateAdd(XRT_TIME_INTERVAL_HOUR, 30, iTime);
	iDiff = xrtDateDiff(XRT_TIME_INTERVAL_HOUR, iTime, tRet);
	printf("xrtDateAdd (2012-03-15 12:20:40) 30 hour : %s\n", xrtTimeToStr(tRet, XRT_TIME_FORMAT_DATETIME));
	printf("xrtDateDiff 30 hour : %d\n", iDiff);
	tRet = xrtDateAdd(XRT_TIME_INTERVAL_DAY, 30, iTime);
	iDiff = xrtDateDiff(XRT_TIME_INTERVAL_DAY, iTime, tRet);
	printf("xrtDateAdd (2012-03-15 12:20:40) 30 day : %s\n", xrtTimeToStr(tRet, XRT_TIME_FORMAT_DATETIME));
	printf("xrtDateDiff 30 day : %d\n", iDiff);
	tRet = xrtDateAdd(XRT_TIME_INTERVAL_MONTH, 30, iTime);
	iDiff = xrtDateDiff(XRT_TIME_INTERVAL_MONTH, iTime, tRet);
	printf("xrtDateAdd (2012-03-15 12:20:40) 30 mon : %s\n", xrtTimeToStr(tRet, XRT_TIME_FORMAT_DATETIME));
	printf("xrtDateDiff 30 mon : %d\n", iDiff);
	tRet = xrtDateAdd(XRT_TIME_INTERVAL_YEAR, 30, iTime);
	iDiff = xrtDateDiff(XRT_TIME_INTERVAL_YEAR, iTime, tRet);
	printf("xrtDateAdd (2012-03-15 12:20:40) 30 year : %s\n", xrtTimeToStr(tRet, XRT_TIME_FORMAT_DATETIME));
	printf("xrtDateDiff 30 year : %d\n", iDiff);
	tRet = xrtDateAdd(XRT_TIME_INTERVAL_WEEKDAY, 30, iTime);
	iDiff = xrtDateDiff(XRT_TIME_INTERVAL_DAY, iTime, tRet);
	printf("xrtDateAdd (2012-03-15 12:20:40) 30 week : %s\n", xrtTimeToStr(tRet, XRT_TIME_FORMAT_DATETIME));
	printf("xrtDateDiff 30 week : %d (day)\n", iDiff);
	tRet = xrtDateAdd(XRT_TIME_INTERVAL_QUARTER, 30, iTime);
	iDiff = xrtDateDiff(XRT_TIME_INTERVAL_QUARTER, iTime, tRet);
	printf("xrtDateAdd (2012-03-15 12:20:40) 30 quar : %s\n", xrtTimeToStr(tRet, XRT_TIME_FORMAT_DATETIME));
	printf("xrtDateDiff 30 quar : %d\n", iDiff);
	
	double st = xrtTimer();
	printf("xrtTimer Start : %f\n", st);
	xrtSleep(1000);
	double et = xrtTimer();
	printf("xrtTimer Stop : %f\n", et);
	printf("time diff (s) : %f\n", et - st);
	
	printf("xrtNow : %s\n", xrtTimeToStr(xrtNow(), XRT_TIME_FORMAT_DATETIME));
	printf("xrtNow : %s\n", xrtNowStr());
}


