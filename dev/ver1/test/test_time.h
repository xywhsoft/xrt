


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
	
	// ===== 新增函数测试 =====
	printf("\n----- 新增函数测试 -----\n");
	
	// 季度测试
	printf("xrtQuarter (2012-03-15) : %d\n", xrtQuarter(iTime));  // 应为 1
	printf("xrtQuarter (2012-06-15) : %d\n", xrtQuarter(xrtDateSerial(2012, 6, 15)));  // 应为 2
	printf("xrtQuarter (2012-09-15) : %d\n", xrtQuarter(xrtDateSerial(2012, 9, 15)));  // 应为 3
	printf("xrtQuarter (2012-12-15) : %d\n", xrtQuarter(xrtDateSerial(2012, 12, 15))); // 应为 4
	
	// 日期/时间部分
	printf("xrtDatePart (2012-03-15 12:20:40) : %s\n", xrtTimeToStr(xrtDatePart(iTime), XRT_TIME_FORMAT_DATE));
	printf("xrtTimePart (2012-03-15 12:20:40) : %s\n", xrtTimeToStr(xrtTimePart(iTime), XRT_TIME_FORMAT_TIME));
	
	// 同一天/月/年判断
	xtime iTime2 = xrtDateTimeSerial(2012, 3, 15, 18, 30, 00);
	xtime iTime3 = xrtDateTimeSerial(2012, 3, 20, 12, 20, 40);
	xtime iTime4 = xrtDateTimeSerial(2012, 5, 15, 12, 20, 40);
	xtime iTime5 = xrtDateTimeSerial(2013, 3, 15, 12, 20, 40);
	printf("xrtIsSameDay (03-15 vs 03-15) : %d\n", xrtIsSameDay(iTime, iTime2));   // 应为 1
	printf("xrtIsSameDay (03-15 vs 03-20) : %d\n", xrtIsSameDay(iTime, iTime3));   // 应为 0
	printf("xrtIsSameMonth (03 vs 03) : %d\n", xrtIsSameMonth(iTime, iTime3));     // 应为 1
	printf("xrtIsSameMonth (03 vs 05) : %d\n", xrtIsSameMonth(iTime, iTime4));     // 应为 0
	printf("xrtIsSameYear (2012 vs 2012) : %d\n", xrtIsSameYear(iTime, iTime4));   // 应为 1
	printf("xrtIsSameYear (2012 vs 2013) : %d\n", xrtIsSameYear(iTime, iTime5));   // 应为 0
	
	// 时间区间判断
	xtime iStart = xrtDateSerial(2012, 3, 1);
	xtime iEnd = xrtDateSerial(2012, 3, 31);
	printf("xrtTimeInRange (03-15 in 03-01~03-31) : %d\n", xrtTimeInRange(xrtDatePart(iTime), iStart, iEnd)); // 应为 1
	printf("xrtTimeInRange (04-15 in 03-01~03-31) : %d\n", xrtTimeInRange(xrtDateSerial(2012, 4, 15), iStart, iEnd)); // 应为 0
	
	// 时间区间重叠
	xtime iStart2 = xrtDateSerial(2012, 3, 15);
	xtime iEnd2 = xrtDateSerial(2012, 4, 15);
	printf("xrtTimeRangeOverlap (03-01~03-31 vs 03-15~04-15) : %d\n", xrtTimeRangeOverlap(iStart, iEnd, iStart2, iEnd2)); // 应为 1
	xtime iStart3 = xrtDateSerial(2012, 4, 1);
	xtime iEnd3 = xrtDateSerial(2012, 4, 30);
	printf("xrtTimeRangeOverlap (03-01~03-31 vs 04-01~04-30) : %d\n", xrtTimeRangeOverlap(iStart, iEnd, iStart3, iEnd3)); // 应为 0
	
	// Unix时间戳转换
	xtime iUnix1970 = xrtDateSerial(1970, 1, 1);
	printf("xrtToUnixTime (1970-01-01) : %lld\n", xrtToUnixTime(iUnix1970));  // 应为 0
	printf("xrtToUnixTime (2012-03-15 12:20:40) : %lld\n", xrtToUnixTime(iTime));
	printf("xrtFromUnixTime (0) : %s\n", xrtTimeToStr(xrtFromUnixTime(0), XRT_TIME_FORMAT_DATETIME));  // 应为 1970-01-01
	printf("xrtFromUnixTime (1331814040) : %s\n", xrtTimeToStr(xrtFromUnixTime(1331814040), XRT_TIME_FORMAT_DATETIME));
	
	// 月份边界
	printf("xrtFirstDayOfMonth (2012-03-15) : %s\n", xrtTimeToStr(xrtFirstDayOfMonth(iTime), XRT_TIME_FORMAT_DATE));  // 2012-03-01
	printf("xrtLastDayOfMonth (2012-03-15) : %s\n", xrtTimeToStr(xrtLastDayOfMonth(iTime), XRT_TIME_FORMAT_DATE));   // 2012-03-31
	printf("xrtLastDayOfMonth (2012-02-15) : %s\n", xrtTimeToStr(xrtLastDayOfMonth(xrtDateSerial(2012, 2, 15)), XRT_TIME_FORMAT_DATE)); // 2012-02-29 (闰年)
	
	// 年份边界
	printf("xrtFirstDayOfYear (2012-03-15) : %s\n", xrtTimeToStr(xrtFirstDayOfYear(iTime), XRT_TIME_FORMAT_DATE));   // 2012-01-01
	printf("xrtLastDayOfYear (2012-03-15) : %s\n", xrtTimeToStr(xrtLastDayOfYear(iTime), XRT_TIME_FORMAT_DATE));    // 2012-12-31
	
	// 周边界 (周一为一周开始)
	printf("xrtFirstDayOfWeek (2012-03-15, Mon) : %s\n", xrtTimeToStr(xrtFirstDayOfWeek(iTime, 1), XRT_TIME_FORMAT_DATE)); // 2012-03-12
	printf("xrtLastDayOfWeek (2012-03-15, Mon) : %s\n", xrtTimeToStr(xrtLastDayOfWeek(iTime, 1), XRT_TIME_FORMAT_DATE));  // 2012-03-18
	printf("xrtFirstDayOfWeek (2012-03-15, Sun) : %s\n", xrtTimeToStr(xrtFirstDayOfWeek(iTime, 0), XRT_TIME_FORMAT_DATE)); // 2012-03-11
	printf("xrtLastDayOfWeek (2012-03-15, Sun) : %s\n", xrtTimeToStr(xrtLastDayOfWeek(iTime, 0), XRT_TIME_FORMAT_DATE));  // 2012-03-17
	
	// 周数
	printf("xrtWeekOfYear (2012-03-15) : %d\n", xrtWeekOfYear(iTime));
	printf("xrtWeekOfMonth (2012-03-15) : %d\n", xrtWeekOfMonth(iTime));
	printf("xrtWeekOfYear (2012-01-01) : %d\n", xrtWeekOfYear(xrtDateSerial(2012, 1, 1)));
	printf("xrtWeekOfMonth (2012-01-01) : %d\n", xrtWeekOfMonth(xrtDateSerial(2012, 1, 1)));
	
	// UTC时间与时区
	printf("xrtNowUTC : %s\n", xrtTimeToStr(xrtNowUTC(), XRT_TIME_FORMAT_DATETIME));
	printf("xrtTimezoneOffset : %d 秒 (%d 小时)\n", xrtTimezoneOffset(), xrtTimezoneOffset() / 3600);
	xtime iLocalNow = xrtNow();
	xtime iUTCNow = xrtNowUTC();
	printf("xrtUTCToLocal (UTC now) : %s\n", xrtTimeToStr(xrtUTCToLocal(iUTCNow), XRT_TIME_FORMAT_DATETIME));
	printf("xrtLocalToUTC (Local now) : %s\n", xrtTimeToStr(xrtLocalToUTC(iLocalNow), XRT_TIME_FORMAT_DATETIME));
	
	// 相对时间描述
	xtime iBase = xrtNow();
	str sRel1 = xrtRelativeTime(xrtDateAdd(XRT_TIME_INTERVAL_SECOND, -30, iBase), iBase);
	str sRel2 = xrtRelativeTime(xrtDateAdd(XRT_TIME_INTERVAL_MINUTE, -5, iBase), iBase);
	str sRel3 = xrtRelativeTime(xrtDateAdd(XRT_TIME_INTERVAL_HOUR, -3, iBase), iBase);
	str sRel4 = xrtRelativeTime(xrtDateAdd(XRT_TIME_INTERVAL_DAY, -7, iBase), iBase);
	str sRel5 = xrtRelativeTime(xrtDateAdd(XRT_TIME_INTERVAL_DAY, 2, iBase), iBase);
	str sRel6 = xrtRelativeTime(xrtDateAdd(XRT_TIME_INTERVAL_MONTH, 3, iBase), iBase);
	printf("xrtRelativeTime (-30秒) : %s\n", sRel1);
	printf("xrtRelativeTime (-5分钟) : %s\n", sRel2);
	printf("xrtRelativeTime (-3小时) : %s\n", sRel3);
	printf("xrtRelativeTime (-7天) : %s\n", sRel4);
	printf("xrtRelativeTime (+2天) : %s\n", sRel5);
	printf("xrtRelativeTime (+3个月) : %s\n", sRel6);
	xrtFree(sRel1); xrtFree(sRel2); xrtFree(sRel3); xrtFree(sRel4); xrtFree(sRel5); xrtFree(sRel6);
	
	// 字符串转时间测试
	printf("\n----- xrtStrToTime 测试 -----\n");
	printf("xrtStrToTime (2012-03-15 12:20:40) : %s\n", xrtTimeToStr(xrtStrToTime("2012-03-15 12:20:40", 0), XRT_TIME_FORMAT_DATETIME));
	printf("xrtStrToTime (2012/03/15 12:20:40) : %s\n", xrtTimeToStr(xrtStrToTime("2012/03/15 12:20:40", 0), XRT_TIME_FORMAT_DATETIME));
	printf("xrtStrToTime (2012.03.15 12:20:40) : %s\n", xrtTimeToStr(xrtStrToTime("2012.03.15 12:20:40", 0), XRT_TIME_FORMAT_DATETIME));
	printf("xrtStrToTime (20120315122040) : %s\n", xrtTimeToStr(xrtStrToTime("20120315122040", 0), XRT_TIME_FORMAT_DATETIME));
	printf("xrtStrToTime (20120315) : %s\n", xrtTimeToStr(xrtStrToTime("20120315", 0), XRT_TIME_FORMAT_DATE));
	printf("xrtStrToTime (2012-03-15) : %s\n", xrtTimeToStr(xrtStrToTime("2012-03-15", 0), XRT_TIME_FORMAT_DATE));
	printf("xrtStrToTime (12:20:40) : %s\n", xrtTimeToStr(xrtStrToTime("12:20:40", 0), XRT_TIME_FORMAT_TIME));
	printf("xrtStrToTime (20120315 122040) : %s\n", xrtTimeToStr(xrtStrToTime("20120315 122040", 0), XRT_TIME_FORMAT_DATETIME));
	printf("xrtStrToTime (2012年3月15日) : %s\n", xrtTimeToStr(xrtStrToTime("2012年3月15日", 0), XRT_TIME_FORMAT_DATE));
	
	// ===== xrtTimeFormat 和 xrtTimeParse 测试 =====
	printf("\n----- xrtTimeFormat 测试 -----\n");
	
	// 基本格式化测试
	str sFmt1 = xrtTimeFormat(iTime, "yyyy-mm-dd hh:mm:ss");
	printf("xrtTimeFormat (yyyy-mm-dd hh:mm:ss) : %s\n", sFmt1);
	xrtFree(sFmt1);
	
	str sFmt2 = xrtTimeFormat(iTime, "yyyy年mm月dd日");
	printf("xrtTimeFormat (yyyy年mm月dd日) : %s\n", sFmt2);
	xrtFree(sFmt2);
	
	str sFmt3 = xrtTimeFormat(iTime, "yy/m/d h:n:s");
	printf("xrtTimeFormat (yy/m/d h:n:s) : %s\n", sFmt3);
	xrtFree(sFmt3);
	
	// 英文月份和星期
	str sFmt4 = xrtTimeFormat(iTime, "mmm d, yyyy");
	printf("xrtTimeFormat (mmm d, yyyy) : %s\n", sFmt4);
	xrtFree(sFmt4);
	
	str sFmt5 = xrtTimeFormat(iTime, "mmmm d, yyyy");
	printf("xrtTimeFormat (mmmm d, yyyy) : %s\n", sFmt5);
	xrtFree(sFmt5);
	
	str sFmt6 = xrtTimeFormat(iTime, "ww, mmmm d, yyyy");
	printf("xrtTimeFormat (ww, mmmm d, yyyy) : %s\n", sFmt6);
	xrtFree(sFmt6);
	
	str sFmt7 = xrtTimeFormat(iTime, "www, mmmm d, yyyy");
	printf("xrtTimeFormat (www, mmmm d, yyyy) : %s\n", sFmt7);
	xrtFree(sFmt7);
	
	// 12小时制和AM/PM
	str sFmt8 = xrtTimeFormat(iTime, "HH:nn ap");
	printf("xrtTimeFormat (HH:nn ap) 12:20 -> : %s\n", sFmt8);
	xrtFree(sFmt8);
	
	xtime iTimeAM = xrtDateTimeSerial(2012, 3, 15, 9, 30, 0);
	str sFmt9 = xrtTimeFormat(iTimeAM, "H:nn AP");
	printf("xrtTimeFormat (H:nn AP) 09:30 -> : %s\n", sFmt9);
	xrtFree(sFmt9);
	
	// 季度和星期数字
	str sFmt10 = xrtTimeFormat(iTime, "yyyy-Qq w");
	printf("xrtTimeFormat (yyyy-Qq w) : %s\n", sFmt10);
	xrtFree(sFmt10);
	
	printf("\n----- xrtTimeParse 测试 -----\n");
	
	// 基本解析测试
	xtime iParsed1 = xrtTimeParse("2012-03-15 12:20:40", "yyyy-mm-dd hh:mm:ss");
	printf("xrtTimeParse (2012-03-15 12:20:40) : %s\n", xrtTimeToStr(iParsed1, XRT_TIME_FORMAT_DATETIME));
	
	xtime iParsed2 = xrtTimeParse("2012年03月15日", "yyyy年mm月dd日");
	printf("xrtTimeParse (2012年03月15日) : %s\n", xrtTimeToStr(iParsed2, XRT_TIME_FORMAT_DATE));
	
	// 英文月份解析
	xtime iParsed3 = xrtTimeParse("Mar 15, 2012", "mmm d, yyyy");
	printf("xrtTimeParse (Mar 15, 2012) : %s\n", xrtTimeToStr(iParsed3, XRT_TIME_FORMAT_DATE));
	
	xtime iParsed4 = xrtTimeParse("March 15, 2012", "mmmm d, yyyy");
	printf("xrtTimeParse (March 15, 2012) : %s\n", xrtTimeToStr(iParsed4, XRT_TIME_FORMAT_DATE));
	
	// 12小时制解析
	xtime iParsed5 = xrtTimeParse("12:20 pm", "HH:nn ap");
	printf("xrtTimeParse (12:20 pm) : %s\n", xrtTimeToStr(iParsed5, XRT_TIME_FORMAT_TIME));
	
	xtime iParsed6 = xrtTimeParse("9:30 AM", "H:nn AP");
	printf("xrtTimeParse (9:30 AM) : %s\n", xrtTimeToStr(iParsed6, XRT_TIME_FORMAT_TIME));
	
	// 冗余前缀测试（自动跳过）
	printf("\n----- 冗余前缀测试 -----\n");
	
	xtime iParsed7 = xrtTimeParse("当前时间为：2018年9月14日 8:30:12", "yyyy年m月d日 h:mm:ss");
	printf("xrtTimeParse (当前时间为：2018年9月14日 8:30:12) : %s\n", xrtTimeToStr(iParsed7, XRT_TIME_FORMAT_DATETIME));
	
	xtime iParsed8 = xrtTimeParse("中国北京时间 UTC+8  22:17:36", "UTC+8  hh:mm:ss");
	printf("xrtTimeParse (中国北京时间 UTC+8  22:17:36) : %s\n", xrtTimeToStr(iParsed8, XRT_TIME_FORMAT_TIME));
	
	xtime iParsed9 = xrtTimeParse("[2012-03-15 12:20:40] ERROR: connection failed", "[yyyy-mm-dd hh:mm:ss]");
	printf("xrtTimeParse (日志格式) : %s\n", xrtTimeToStr(iParsed9, XRT_TIME_FORMAT_DATETIME));
	
	xtime iParsed10 = xrtTimeParse("Log: Mar 15, 2012 - System started", "mmm d, yyyy");
	printf("xrtTimeParse (Log: Mar 15, 2012) : %s\n", xrtTimeToStr(iParsed10, XRT_TIME_FORMAT_DATE));
}


