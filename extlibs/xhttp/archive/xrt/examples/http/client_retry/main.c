#include <xrt/http_client_runtime.h>

#include <stdio.h>



/* 展示默认关闭和显式启用的客户端重试配置。 */
int main(void)
{
	xhttpclientconfig Config;

	xrtHttpClientConfigInit(&Config);
	Config.Retry.MaxRetries = 3;
	Config.Retry.BaseDelay = UINT64_C(250000);
	Config.Retry.MaxDelay = UINT64_C(5000000);
	printf(
		"retries=%u base=%llu max=%llu\n",
		(unsigned int)Config.Retry.MaxRetries,
		(unsigned long long)Config.Retry.BaseDelay,
		(unsigned long long)Config.Retry.MaxDelay
	);
	return 0;
}
