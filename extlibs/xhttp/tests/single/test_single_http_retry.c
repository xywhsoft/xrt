#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_RETRY
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"

#include <stdio.h>



/* 验证单头发布保留 Retry-After 解析和封顶退避。 */
int main(void)
{
	xhttpretryafter Retry;
	char Text[30];
	uint64 iDelay;
	size_t iSize;
	bool bPass;

	bPass = xrtHttpRetryAfterParse(
		XRT_STR_LITERAL("2"), &Retry
	) && xrtHttpRetryAfterDelay(&Retry, 0, &iDelay) &&
		(iDelay == UINT64_C(2000000)) &&
		xrtHttpRetryAfterWrite(
			&Retry, Text, sizeof(Text), &iSize
		) && (iSize == 1u) && (Text[0] == '2') &&
		xrtHttpRetryBackoff(1000, 4000, 8, &iDelay) &&
		(iDelay == UINT64_C(4000));
	printf(
		"%s single-http-retry\n",
		bPass ? "[PASS]" : "[FAIL]"
	);
	return bPass ? 0 : 1;
}


