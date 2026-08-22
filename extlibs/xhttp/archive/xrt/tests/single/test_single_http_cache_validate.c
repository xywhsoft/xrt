#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>



/* 验证单头文件保留 HTTP 缓存验证与失效计划。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"asset\"")
		}
	};
	xhttpcacheentry Entry = {
		Fields,
		1,
		0,
		XHTTP_CACHE_ENTRY_NONE
	};
	xhttpcachevalidateplan Plan;
	bool bPass;

	bPass = xrtHttpCacheValidatePlan(
		&Entry, 1, false, &Plan
	) == XHTTP_CACHE_VALIDATE_CONDITIONAL;
	printf(
		"%s single-http-cache-validate\n",
		bPass ? "[PASS]" : "[FAIL]"
	);
	return bPass ? 0 : 1;
}
