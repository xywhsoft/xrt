#include <xrt/http_cache_policy.h>

#include <stdio.h>
#include <string.h>



/* 展示共享缓存对带 Authorization 的新鲜响应作出存储和复用计划。 */
int main(void)
{
	static const xhttpfield ResponseFields[] = {
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT("public, max-age=60")
		}
	};
	xhttpcachecontrol Request;
	xhttpcachecontrol Response;
	xhttpcachetime Time;
	xhttpcacheage Age;
	xhttpcachefreshness Freshness;
	xhttpcachestoreinput StoreInput;
	xhttpcachestoreplan StorePlan;
	xhttpcacheuseinput UseInput;
	xhttpcacheuseplan UsePlan;

	xrtHttpCacheControlInit(&Request);
	xrtHttpCacheTimeInit(&Time);
	memset(&Age, 0, sizeof(Age));
	Freshness.Lifetime = UINT64_C(60000000);
	Freshness.Source = XHTTP_CACHE_FRESHNESS_MAX_AGE;
	if ( !xrtHttpCacheControlParse(
		ResponseFields, 1, &Response
	) || !xrtHttpCacheStoreInputInit(
		&StoreInput,
		XRT_STR_LITERAL("GET"),
		200,
		true
	) ) {
		return 1;
	}
	StoreInput.Flags |=
		XHTTP_CACHE_STORE_AUTHORIZATION;
	if ( xrtHttpCacheStorePlan(
		&Request, &Response, &Time,
		&StoreInput, &StorePlan
	) != XHTTP_CACHE_STORE_KEEP ) {
		return 1;
	}
	if ( !xrtHttpCacheUseInputInit(
		&UseInput, 200, true
	) || (xrtHttpCacheUsePlan(
		&Request, &Response,
		&Age, &Freshness,
		&UseInput, &UsePlan
	) != XHTTP_CACHE_USE_STORED) ) {
		return 1;
	}
	printf(
		"store=%d use=%d actions=0x%08x\n",
		(int)StorePlan.Decision,
		(int)UsePlan.Decision,
		(unsigned int)UsePlan.Actions
	);
	return 0;
}
