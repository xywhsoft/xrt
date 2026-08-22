#include <stdio.h>

#include <xrt.h>



/* 展示绝对时间、UTC 分解、固定偏移和日历加法的常用路径。 */
int main(void)
{
	xtime iNow = xrtNow();
	xtime iNextMonth;
	xdatetime tUTC;
	xdatetime tLocalOffset;

	if ( !xrtTimeSplit(iNow, &tUTC) ||
		 !xrtTimeSplitAt(iNow, 8 * 3600, &tLocalOffset) ||
		 !xrtTimeAdd(iNow, 1, XTIME_UNIT_MONTH, &iNextMonth) ) {
		return 1;
	}
	printf("unix_us=%lld\n", (long long)iNow);
	printf("utc=%lld-%02d-%02d %02d:%02d:%02d.%06d\n",
		(long long)tUTC.Year, tUTC.Month, tUTC.Day,
		tUTC.Hour, tUTC.Minute, tUTC.Second, tUTC.Microsecond);
	printf("utc+8=%lld-%02d-%02d %02d:%02d:%02d\n",
		(long long)tLocalOffset.Year, tLocalOffset.Month, tLocalOffset.Day,
		tLocalOffset.Hour, tLocalOffset.Minute, tLocalOffset.Second);
	printf("next_month=%lld\n", (long long)iNextMonth);
	return 0;
}
