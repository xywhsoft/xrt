#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_CLIENT_RETRY
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"

#include <stdio.h>



/* 验证单头发布保留客户端重试配置和纯策略依赖。 */
int main(void)
{
	xhttpclientconfig Config;
	uint64 iDelay;
	bool bPass;

	xrtHttpClientConfigInit(&Config);
	bPass = (Config.Retry.MaxRetries == 0) &&
		xrtHttpRetryBackoff(1000, 4000, 3, &iDelay) &&
		(iDelay == 4000);
	printf(
		"%s single-http-client-retry\n",
		bPass ? "[PASS]" : "[FAIL]"
	);
	return bPass ? 0 : 1;
}


