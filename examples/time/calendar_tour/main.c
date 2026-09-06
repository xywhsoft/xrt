/*
 * 范例：time/calendar_tour —— 日历族全接口（字段/构造/换算）
 * ----------------------------------------------------------------
 * 演示 API：
 *   【历法查询】  xrtIsLeapYear / xrtDaysInMonth / xrtDaysInYear
 *   【构造】      xrtDate（UTC 零点）/ xrtTimeMake（按分解结构）
 *   【字段提取】  xrtYear / Month / Day / Hour / Minute / Second /
 *                  Microsecond / Weekday / DayOfYear / Quarter /
 *                  xrtDatePart（当日零点）/ xrtTimePart（日内微秒）
 *   【ISO 周历】  xrtISOWeek（周日为 7，跨年归上一周年）
 *   【Unix 换算】 xrtTimeFromUnix / xrtTimeUnix /
 *                  xrtTimeFromUnixMs / xrtTimeUnixMs
 * 模块宏：XRT_MODULE_TIME
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/time/calendar_tour/main.c
 * 预期输出：
 *   calendar: leap(2000=1 1900=0 2024=1) feb(2024=29 2023=28) year=366
 *   fields: 2024-03-10 Sun 12:34:56.789012 yday=70 q=1
 *   parts: date-part=1710028800 time-part=45296789012
 *   iso-week: 2024-W10-7, 2023-01-01 -> 2022-W52-7
 *   unix: 1710074096 / 1710074096789 roundtrip=ok
 *
 * 基准时刻取 2024-03-10T12:34:56.789012Z（周日、闰年、
 *   第 70 天、Q1、ISO 第 10 周）——一个把所有字段都
 *   撑出非平凡值的时间点，断言全部对齐预先算好的常数。
 */

#include <stdio.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

/* 基准时刻 2024-03-10T12:34:56.789012Z 的预先计算常数。 */
#define EXAMPLE_EPOCH_US	INT64_C(1710074096789012)
#define EXAMPLE_EPOCH_S	INT64_C(1710074096)
#define EXAMPLE_EPOCH_MS	INT64_C(1710074096789)
#define EXAMPLE_MIDNIGHT	INT64_C(1710028800000000)
#define EXAMPLE_TIME_PART	INT64_C(45296789012)



int main(void)
{
	xtime Moment;
	xtime Made;
	xtime Midnight;
	xtime FromS;
	xtime FromMs;
	int64 iWeekYear;
	int iWeek;
	int iWeekday;

	/* 历法查询：整除规则 1900 不闰、2000/2024 闰；闰年二月 29 天。 */
	printf("calendar: leap(2000=%d 1900=%d 2024=%d)",
		xrtIsLeapYear(2000) ? 1 : 0,
		xrtIsLeapYear(1900) ? 1 : 0,
		xrtIsLeapYear(2024) ? 1 : 0);
	printf(" feb(2024=%d 2023=%d)",
		xrtDaysInMonth(2024, 2), xrtDaysInMonth(2023, 2));
	printf(" year=%d\n", xrtDaysInYear(2024));

	/* 全微秒精度基准：直接按字段构造（FromUnix 只有秒精度）。 */
	if ( !xrtDateTime(2024, 3, 10, 12, 34, 56, 789012, &Moment) ) {
		return 1;
	}

	/* 字段提取：年月日时分秒微秒 + 星期 + 年内日 + 季度。 */
	printf("fields: %lld-%02d-%02d Sun %02d:%02d:%02d.%06d",
		(long long)xrtYear(Moment),
		xrtMonth(Moment), xrtDay(Moment),
		xrtHour(Moment), xrtMinute(Moment),
		xrtSecond(Moment), xrtMicrosecond(Moment));
	printf(" yday=%d q=%d\n", xrtDayOfYear(Moment), xrtQuarter(Moment));
	if ( (xrtYear(Moment) != 2024) || (xrtMonth(Moment) != 3) ||
		 (xrtDay(Moment) != 10) || (xrtWeekday(Moment) != 0) ||
		 (xrtHour(Moment) != 12) || (xrtMinute(Moment) != 34) ||
		 (xrtSecond(Moment) != 56) ||
		 (xrtMicrosecond(Moment) != 789012) ||
		 (xrtDayOfYear(Moment) != 70) ||
		 (xrtQuarter(Moment) != 1) ) {
		return 2;
	}

	/* DatePart / TimePart：零点与日内偏移拼回原时刻。 */
	Midnight = xrtDatePart(Moment);
	printf("parts: date-part=%lld",
		(long long)xrtTimeUnixMs(Midnight) / 1000);
	printf(" time-part=%lld\n", (long long)xrtTimePart(Moment));
	if ( (Midnight != EXAMPLE_MIDNIGHT) ||
		 (xrtTimePart(Moment) != EXAMPLE_TIME_PART) ) {
		return 3;
	}

	/* ISO 周历：周日编号 7；年初的周日归上一周年。 */
	if ( !xrtISOWeek(Moment, &iWeekYear, &iWeek, &iWeekday) ||
		 (iWeekYear != 2024) || (iWeek != 10) || (iWeekday != 7) ) {
		return 4;
	}
	printf("iso-week: %lld-W%d-%d", (long long)iWeekYear, iWeek,
		iWeekday);
	{
		xtime NewYear2023;

		if ( !xrtDate(2023, 1, 1, &NewYear2023) ||
			 !xrtISOWeek(NewYear2023, &iWeekYear, &iWeek,
				&iWeekday) ||
			 (iWeekYear != 2022) || (iWeek != 52) ||
			 (iWeekday != 7) ) {
			return 5;
		}
		printf(", 2023-01-01 -> %lld-W%d-%d\n",
			(long long)iWeekYear, iWeek, iWeekday);
	}

	/* TimeMake：按分解结构（含显式偏移）重建同一时刻。 */
	{
		xdatetime Parts;

		Parts.Year = 2024;
		Parts.Month = 3;
		Parts.Day = 10;
		Parts.Hour = 12;
		Parts.Minute = 34;
		Parts.Second = 56;
		Parts.Microsecond = 789012;
		Parts.Offset = 0;  /* UTC */
		if ( !xrtTimeMake(&Parts, &Made) || (Made != Moment) ) {
			return 6;
		}
	}

	/* Unix 秒/毫秒双精度换算：毫秒精度只能还原毫秒（.789012 → .789000）。 */
	if ( !xrtTimeFromUnixMs(EXAMPLE_EPOCH_MS, &FromMs) ||
		 (xrtTimeUnixMs(FromMs) != EXAMPLE_EPOCH_MS) ||
		 (xrtTimeUnixMs(Moment) != EXAMPLE_EPOCH_MS) ) {
		return 7;
	}
	FromS = Moment;
	(void)xrtTimeFromUnix(xrtTimeUnix(Moment), &FromS);
	printf("unix: %lld / %lld roundtrip=%s\n",
		(long long)xrtTimeUnix(Moment),
		(long long)xrtTimeUnixMs(Moment),
		(xrtTimeUnix(FromS) == EXAMPLE_EPOCH_S &&
		 xrtTimeUnixMs(FromMs) == EXAMPLE_EPOCH_MS) ? "ok" : "fail");
	return (xrtTimeUnix(FromS) == EXAMPLE_EPOCH_S &&
		xrtTimeUnixMs(FromMs) == EXAMPLE_EPOCH_MS) ? 0 : 8;
}
