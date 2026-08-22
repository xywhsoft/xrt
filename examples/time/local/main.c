#include <stdio.h>

#include <xrt.h>



/* 展示系统本地时区分解和显式 DST fold 策略。 */
int main(void)
{
	xtime iNow = xrtNow();
	xtime iRoundtrip;
	xdatetime tLocal;

	if ( !xrtTimeLocal(iNow, &tLocal) ||
		 !xrtTimeFromLocal(&tLocal, XTIME_FOLD_EARLIER, &iRoundtrip) ) {
		return 1;
	}
	printf("local=%lld-%02d-%02d %02d:%02d:%02d.%06d offset=%+d dst=%d\n",
		(long long)tLocal.Year, tLocal.Month, tLocal.Day,
		tLocal.Hour, tLocal.Minute, tLocal.Second, tLocal.Microsecond,
		tLocal.Offset, tLocal.IsDST);
	printf("roundtrip=%s\n", iRoundtrip == iNow ? "yes" : "no");
	return iRoundtrip == iNow ? 0 : 1;
}
