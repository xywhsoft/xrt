#include "../test_allocator.h"



/* 代理对象的第一笔分配失败必须原样暴露为内存错误。 */
int main(void)
{
	xnetproxyconfig Config;

	testRequire(testInstallFailAllocator(),
		"proxy OOM allocator install failed");
	xrtNetProxyConfigInit(&Config);
	Config.Host = XRT_STR_LITERAL("proxy.example");
	Config.Port = 1080;
	testRequire(xrtNetProxyCreate(&Config) == NULL,
		"proxy create unexpectedly survived OOM");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"proxy create OOM error mismatch");
	return 0;
}
