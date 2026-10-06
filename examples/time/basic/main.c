/*
 * 范例：time/basic —— xtime 主线：当前时刻、分解、偏移与日历运算
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtNow          当前 公元 UTC 毫秒（xtime 即 int64 毫秒，全库统一）
 *   xrtTimeSplit    按 UTC 分解为日历字段（年月日时分秒毫秒）
 *   xrtTimeSplitAt  按固定秒偏移分解（+8×3600 即东八区，无 DST）
 *   xrtTimeAdd      日历加法（月/日/时等单位，处理月末进位）
 * 模块宏：XRT_MODULE_TIME
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/time/basic/main.c -lws2_32 -liphlpapi
 * 预期输出（时间随运行时刻变化，格式如下）：
 *   utc_ms=63924170493834
 *   utc=2026-09-05 02:01:33.834
 *   utc+8=2026-09-05 10:01:33
 *   next_month=63926762493834
 *
 * xtime 设计要点：
 *   整数毫秒（非 double 秒）——比较/差值零浮点误差，
 *   等待接口用相对毫秒；耗时测量使用 xrtTimer 的单调 double 秒。
 *   日历加法按"日历语义"进位：8 月 31 日 +1 月 = 9 月 30 日，
 *   而不是简单加 30×86400 秒。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	xtime iNow = xrtNow();
	xtime iNextMonth;
	xdatetime tUTC;
	xdatetime tLocalOffset;

	/*
	 * 三种常用换算一步到位：
	 *   Split    UTC 字段（零偏移基准）；
	 *   SplitAt  固定偏移字段（协议场景常直接给偏移秒，
	 *            不经系统时区数据库，结果可复现）；
	 *   Add      日历单位加法（跨月/闰年自动进位）。
	 */
	if ( !xrtTimeSplit(iNow, &tUTC) ||
		 !xrtTimeSplitAt(iNow, 8 * 3600, &tLocalOffset) ||
		 !xrtTimeAdd(iNow, 1, XTIME_UNIT_MONTH, &iNextMonth) ) {
		return 1;
	}
	printf("utc_ms=%lld\n", (long long)iNow);
	printf("utc=%lld-%02d-%02d %02d:%02d:%02d.%03d\n",
		(long long)tUTC.Year, tUTC.Month, tUTC.Day,
		tUTC.Hour, tUTC.Minute, tUTC.Second, tUTC.Millisecond);
	printf("utc+8=%lld-%02d-%02d %02d:%02d:%02d\n",
		(long long)tLocalOffset.Year, tLocalOffset.Month, tLocalOffset.Day,
		tLocalOffset.Hour, tLocalOffset.Minute, tLocalOffset.Second);
	printf("next_month=%lld\n", (long long)iNextMonth);
	return 0;
}
