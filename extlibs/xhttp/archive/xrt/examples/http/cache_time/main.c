#include <xrt/http_cache_time.h>

#include <stdio.h>



/* 展示响应缓存年龄和私有缓存显式寿命计算。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT("max-age=120")
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

	if ( !xrtHttpCacheControlParse(
		Fields, 3, &Control
	) || !xrtHttpCacheTimeParse(
		Fields, 3, &Time
	) ) {
		return 1;
	}
	if ( xrtHttpCacheCurrentAge(
		&Time,
		Time.Date,
		UINT64_C(1000000),
		UINT64_C(3000000),
		UINT64_C(8000000),
		&Age
	) != XHTTP_CACHE_CALC_READY ) {
		return 1;
	}
	if ( xrtHttpCacheFreshness(
		&Control,
		&Time,
		Time.Date,
		false,
		&Freshness
	) != XHTTP_CACHE_CALC_READY ) {
		return 1;
	}
	printf(
		"age=%llu lifetime=%llu fresh=%s\n",
		(unsigned long long)Age.CurrentAgeSeconds,
		(unsigned long long)(
			Freshness.Lifetime /
			(uint64)XRT_TIME_SECOND
		),
		xrtHttpCacheFresh(&Age, &Freshness) ?
			"yes" :
			"no"
	);
	return 0;
}
