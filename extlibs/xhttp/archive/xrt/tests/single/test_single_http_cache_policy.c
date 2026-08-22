#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>



/* 验证单头文件保留缓存存储与复用策略。 */
int main(void)
{
	xhttpcachecontrol Empty;
	xhttpcachetime Time;
	xhttpcachestoreinput Input;
	xhttpcachestoreplan Plan;
	bool bPass;

	xrtHttpCacheControlInit(&Empty);
	xrtHttpCacheTimeInit(&Time);
	bPass = xrtHttpCacheStoreInputInit(
		&Input,
		XRT_STR_LITERAL("GET"),
		200,
		false
	) && (xrtHttpCacheStorePlan(
		&Empty, &Empty, &Time,
		&Input, &Plan
	) == XHTTP_CACHE_STORE_KEEP);
	printf(
		"%s single-http-cache-policy\n",
		bPass ? "[PASS]" : "[FAIL]"
	);
	return bPass ? 0 : 1;
}
