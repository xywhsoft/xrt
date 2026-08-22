#include "../test_allocator.h"

#include <xrt/http_link.h>



/* Link writer 的长度查询和调用方缓冲写入不得分配。 */
int main(void)
{
	static const xhttplinkparamvalue Params[] = {
		{
			XRT_STR_INIT("title"),
			XRT_STR_INIT("no allocation"),
			XHTTP_PARAM_HAS_VALUE | XHTTP_PARAM_QUOTED
		}
	};
	static const xhttplinkvalue Link = {
		XRT_STR_INIT("/next"),
		XRT_STR_INIT("next alternate"),
		Params,
		1u
	};
	char sOutput[96];
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"Link writer failure allocator install failed");
	testRequire(
		xrtHttpLinkElementWrite(
			&Link, NULL, 0, &iSize
		) && xrtHttpLinkElementWrite(
			&Link, sOutput, sizeof(sOutput), &iSize
		) && xrtHttpLinkValid(
			(xstrview){ sOutput, iSize }
		),
		"Link writer allocated"
	);
	printf("[PASS] http_link_write_noalloc\n");
	return 0;
}
