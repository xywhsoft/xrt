#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#ifndef XHTTP_MODULE_HTTP_CACHE_TIME
	#define XHTTP_MODULE_HTTP_CACHE_TIME
#endif
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"

#include <stdio.h>



/* 验证单头文件保留缓存年龄与显式新鲜寿命计算。 */
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
	bool bPass;

	bPass = xrtHttpCacheControlParse(
		Fields, 3, &Control
	) && xrtHttpCacheTimeParse(
		Fields, 3, &Time
	) && (xrtHttpCacheCurrentAge(
		&Time, Time.Date, 1, 2, 3, &Age
	) == XHTTP_CACHE_CALC_READY) &&
		(xrtHttpCacheFreshness(
			&Control,
			&Time,
			Time.Date,
			false,
			&Freshness
		 ) == XHTTP_CACHE_CALC_READY) &&
		(Freshness.Lifetime ==
		 UINT64_C(60000000)) &&
		xrtHttpCacheFresh(&Age, &Freshness);
	printf(
		"%s single-http-cache-time\n",
		bPass ? "[PASS]" : "[FAIL]"
	);
	return bPass ? 0 : 1;
}
