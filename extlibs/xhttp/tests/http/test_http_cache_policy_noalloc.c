#include "../test_allocator.h"

#include <xrt/http_cache_policy.h>



/* 验证存储与复用计划在失败分配器下仍可完整执行。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT("max-age=60")
		}
	};
	xhttpcachecontrol Empty;
	xhttpcachecontrol Response;
	xhttpcachetime Time;
	xhttpcacheage Age;
	xhttpcachefreshness Freshness;
	xhttpcachestoreinput StoreInput;
	xhttpcachestoreplan StorePlan;
	xhttpcacheuseinput UseInput;
	xhttpcacheuseplan UsePlan;
	bool bPass;

	testRequire(
		testInstallFailAllocator(),
		"HTTP cache policy failure allocator install failed"
	);
	xrtHttpCacheControlInit(&Empty);
	xrtHttpCacheTimeInit(&Time);
	memset(&Age, 0, sizeof(Age));
	Freshness.Lifetime = UINT64_C(60000000);
	Freshness.Source = XHTTP_CACHE_FRESHNESS_MAX_AGE;
	bPass = xrtHttpCacheControlParse(
		Fields, 1, &Response
	) && xrtHttpCacheStoreInputInit(
		&StoreInput,
		XRT_STR_LITERAL("GET"),
		200,
		false
	) && (xrtHttpCacheStorePlan(
		&Empty, &Response, &Time,
		&StoreInput, &StorePlan
	) == XHTTP_CACHE_STORE_KEEP) &&
		xrtHttpCacheUseInputInit(
			&UseInput, 200, false
		) && (xrtHttpCacheUsePlan(
			&Empty, &Response,
			&Age, &Freshness,
			&UseInput, &UsePlan
		) == XHTTP_CACHE_USE_STORED);
	testRequire(
		bPass,
		"HTTP cache policy allocated memory"
	);
	printf("[PASS] http_cache_policy_noalloc\n");
	return 0;
}
