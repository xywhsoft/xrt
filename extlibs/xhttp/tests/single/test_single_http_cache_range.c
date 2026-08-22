#define XHTTP_MODULE_HTTP_CACHE_RANGE
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"

#include <stdio.h>



/* 验证单头文件保留缓存片段和组合计划。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"asset\"")
		},
		{
			XRT_STR_INIT("Content-Range"),
			XRT_STR_INIT("bytes 5-9/10")
		},
		{
			XRT_STR_INIT("Content-Length"),
			XRT_STR_INIT("5")
		}
	};
	xhttpcachefragmentinput Input;
	xhttpcachefragmentplan Plan;
	bool bPass;

	xrtHttpCacheFragmentInputInit(&Input);
	Input.Method = XRT_STR_LITERAL("GET");
	Input.Fields = Fields;
	Input.FieldCount =
		sizeof(Fields) / sizeof(Fields[0]);
	Input.Status = XHTTP_STATUS_PARTIAL_CONTENT;
	Input.BodySize = 5;
	Input.Flags =
		XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE |
		XHTTP_CACHE_FRAGMENT_BODY_COMPLETE;
	bPass = xrtHttpCacheFragmentPlan(
		&Input, &Plan
	) == XHTTP_CACHE_FRAGMENT_STORE;
	printf(
		"%s single-http-cache-range\n",
		bPass ? "[PASS]" : "[FAIL]"
	);
	return bPass ? 0 : 1;
}
