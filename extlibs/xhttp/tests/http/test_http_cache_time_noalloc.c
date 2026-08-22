#include "../test_allocator.h"

#include <xrt/http_cache_time.h>



/* 验证时间提取、年龄和显式寿命计算全程不分配内存。 */
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

	testRequire(
		testInstallFailAllocator(),
		"HTTP cache time failure allocator install failed"
	);
	bPass = xrtHttpCacheControlParse(
		Fields, 3, &Control
	) && xrtHttpCacheTimeParse(
		Fields, 3, &Time
	) && (xrtHttpCacheCurrentAge(
		&Time,
		Time.Date,
		UINT64_C(1000000),
		UINT64_C(2000000),
		UINT64_C(3000000),
		&Age
	) == XHTTP_CACHE_CALC_READY) &&
		(xrtHttpCacheFreshness(
			&Control,
			&Time,
			Time.Date,
			false,
			&Freshness
		 ) == XHTTP_CACHE_CALC_READY) &&
		xrtHttpCacheFresh(&Age, &Freshness);
	testRequire(
		bPass,
		"HTTP cache time calculation allocated memory"
	);
	printf("[PASS] http_cache_time_noalloc\n");
	return 0;
}
