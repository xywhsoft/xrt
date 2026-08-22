#include "../test_allocator.h"



/* 长转义 reg-name 与 IPvFuture 验证不得申请堆内存。 */
int main(void)
{
	static const xhttpparam Params[] = {
		{
			XRT_STR_INIT("host"),
			XRT_STR_INIT(
				"very\\-long.example.example.example.example."
				"example.example.example.example.example:443"
			),
			XHTTP_PARAM_HAS_VALUE | XHTTP_PARAM_QUOTED
		},
		{
			XRT_STR_INIT("host"),
			XRT_STR_INIT(
				"[vF.this-is-an-arbitrarily-long-ipvfuture-"
				"address-value-used-without-buffering]"
			),
			XHTTP_PARAM_HAS_VALUE | XHTTP_PARAM_QUOTED
		}
	};

	testRequire(testInstallFailAllocator(),
		"HTTP parameter Host failure allocator install failed");
	testRequire(
		xrtHttpParamHostValid(&Params[0]) &&
		xrtHttpParamHostValid(&Params[1]),
		"HTTP parameter Host allocated"
	);
	printf("[PASS] http_param_host_noalloc\n");
	return 0;
}
