#include <stdio.h>

#include <xhttp.h>



/* 计算一次缓存响应的当前年龄与显式新鲜寿命。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT("max-age=60")
		},
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:37 GMT")
		},
		{
			XRT_STR_INIT("Age"),
			XRT_STR_INIT("10")
		}
	};
	xhttpcachecontrol Control;
	xhttpcachetime Time;
	xhttpcacheage Age;
	xhttpcachefreshness Freshness;

	if ( !xrtHttpCacheControlParse(Fields, 3, &Control) ||
		!xrtHttpCacheTimeParse(Fields, 3, &Time) ||
		(xrtHttpCacheCurrentAge(
			&Time, Time.Date, 1, 2, 3, &Age
		) != XHTTP_CACHE_CALC_READY) ||
		(xrtHttpCacheFreshness(
			&Control, &Time, Time.Date, false, &Freshness
		) != XHTTP_CACHE_CALC_READY) ) {
		return 1;
	}
	printf(
		"age=%llu, lifetime=%llu microseconds\n",
		(unsigned long long)Age.CurrentAge,
		(unsigned long long)Freshness.Lifetime
	);
	return 0;
}
