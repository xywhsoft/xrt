/*
 * 范例：time/range_tour —— 区间比较族 + 单位差值
 * ----------------------------------------------------------------
 * 演示 API：
 *   【容差比较】  xrtTimeNear（显式微秒容差）
 *   【同期判断】  xrtTimeSameDay / SameMonth / SameYear
 *   【区间判定】  xrtTimeIn（闭区间）/ xrtTimeOverlap（闭区间相交）
 *   【周期区间】  xrtMonthRange / xrtYearRange /
 *                  xrtWeekRange（可指定每周第一天）
 *   【单位差值】  xrtTimeDiff（起点到终点的完整单位数）
 *   【微秒睡眠】  xrtSleepUs（单调时钟验证实际睡眠时长）
 * 模块宏：XRT_MODULE_TIME
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/time/range_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   near: within=1 outside=0
 *   same: day=1/0 month=1/0 year=1/0
 *   in/overlap: contains=1 reversed=0 cross=1 disjoint=0
 *   ranges: month=[1709251200,1711929600) year=[1704067200,1735689600)
 *   week(sun-first)=[1710028800,1710633600) week(mon-first)=[1709510400,1710115200)
 *   diff: 10 days = 1 week + 3 days, 90 sec = 1 minute + 30
 *   sleep-us: 20000us floor reached
 *
 * 基准仍取 2024-03-10（周日），让"每周第一天"的
 *   差异恰好偏移一整天——周日开周的区间比周一开周早一天。
 */

#include <stdio.h>
#include <xrt.h>



int main(void)
{
	xtime Base;
	xtime Day10;
	xtime Day11;
	xtime Mar1;
	xtime Apr1;
	xtime Jan1;
	xtime Start;
	xtime End;
	int64 iDiff;
	uint64 iBefore;
	uint64 iAfter;

	/* 基准：2024-03-10（周日）与相邻两天、跨月/跨年锚点。 */
	if ( !xrtDate(2024, 3, 10, &Base) ||
		 !xrtDate(2024, 3, 11, &Day11) ||
		 !xrtDate(2024, 3, 1, &Mar1) ||
		 !xrtDate(2024, 4, 1, &Apr1) ||
		 !xrtDate(2024, 1, 1, &Jan1) ) {
		return 1;
	}
	Day10 = Base;

	/* Near：±1000us 容差内为真，超出为假。 */
	printf("near: within=%d outside=%d\n",
		xrtTimeNear(Base, Base + 900, 1000u) ? 1 : 0,
		xrtTimeNear(Base, Base + 1001, 1000u) ? 1 : 0);

	/* Same 族：同日/同月/同年 与跨界对照各一组。 */
	printf("same: day=%d/%d month=%d/%d year=%d/%d\n",
		xrtTimeSameDay(Base, Base + 3600 * XRT_TIME_SECOND) ? 1 : 0,
		xrtTimeSameDay(Base, Day11) ? 1 : 0,
		xrtTimeSameMonth(Base, Mar1) ? 1 : 0,
		xrtTimeSameMonth(Base, Apr1) ? 1 : 0,
		xrtTimeSameYear(Base, Jan1 + 100 * XRT_TIME_DAY) ? 1 : 0,
		xrtTimeSameYear(Base, Apr1 + 365 * XRT_TIME_DAY) ? 1 : 0);

	/* In / Overlap：闭区间语义 + 反向区间恒假。 */
	printf("in/overlap: contains=%d reversed=%d cross=%d disjoint=%d\n",
		xrtTimeIn(Day11, Base, Day11) ? 1 : 0,
		xrtTimeIn(Base, Day11, Base) ? 1 : 0,
		xrtTimeOverlap(Mar1, Base, Mar1, Apr1) ? 1 : 0,
		xrtTimeOverlap(Mar1, Base, Apr1, Apr1 + 10 *
			XRT_TIME_DAY) ? 1 : 0);

	/* Month/Year 区间：包含 Base 的半开区间（秒级打印）。 */
	if ( !xrtMonthRange(Base, &Start, &End) ) {
		return 2;
	}
	printf("ranges: month=[%lld,%lld)",
		(long long)(Start / XRT_TIME_SECOND),
		(long long)(End / XRT_TIME_SECOND));
	if ( !xrtYearRange(Base, &Start, &End) ) {
		return 3;
	}
	printf(" year=[%lld,%lld)\n",
		(long long)(Start / XRT_TIME_SECOND),
		(long long)(End / XRT_TIME_SECOND));

	/* WeekRange：周日开周 vs 周一开周（Base 是周日，差一整天）。 */
	if ( !xrtWeekRange(Base, 0, &Start, &End) ) {
		return 4;
	}
	printf("week(sun-first)=[%lld,%lld)",
		(long long)(Start / XRT_TIME_SECOND),
		(long long)(End / XRT_TIME_SECOND));
	if ( !xrtWeekRange(Base, 1, &Start, &End) ) {
		return 5;
	}
	printf(" week(mon-first)=[%lld,%lld)\n",
		(long long)(Start / XRT_TIME_SECOND),
		(long long)(End / XRT_TIME_SECOND));

	/* Diff：完整单位计数（10 天 = 1 周余 3 天；90 秒 = 1 分余 30 秒）。 */
	if ( !xrtTimeDiff(Base, Day10 + 10 * XRT_TIME_DAY,
		XTIME_UNIT_DAY, &iDiff) || (iDiff != 10) ) {
		return 6;
	}
	printf("diff: %lld days = ", (long long)iDiff);
	(void)xrtTimeDiff(Base, Day10 + 10 * XRT_TIME_DAY,
		XTIME_UNIT_WEEK, &iDiff);
	printf("%lld week + ", (long long)iDiff);
	(void)xrtTimeDiff(Base, Day10 + 10 * XRT_TIME_DAY,
		XTIME_UNIT_DAY, &iDiff);
	printf("%lld days, ", (long long)iDiff - 7);
	(void)xrtTimeDiff(Base + 90 * XRT_TIME_SECOND, Base,
		XTIME_UNIT_SECOND, &iDiff);
	if ( iDiff != -90 ) {
		return 7;
	}
	(void)xrtTimeDiff(Base, Base + 90 * XRT_TIME_SECOND,
		XTIME_UNIT_MINUTE, &iDiff);
	printf("90 sec = %lld minute + %lld\n",
		(long long)iDiff, (long long)90 - iDiff * 60);

	/* SleepUs：至少睡满 20ms（单调时钟度量，向下取整即失败）。 */
	iBefore = xrtClock();
	xrtSleepUs(20000u);
	iAfter = xrtClock();
	printf("sleep-us: 20000us floor %s\n",
		(iAfter - iBefore) >= 20000u ? "reached" : "missed");
	return (iAfter - iBefore) >= 20000u ? 0 : 8;
}
