#include "../test_allocator.h"

#include <xrt/http_proxy_status.h>



/* RFC 9532 分配型构建在 OOM 下保持长度输出不变。 */
int main(void)
{
	static const xstrview Aliases[] = {
		XRT_STR_INIT("one.example"),
		XRT_STR_INIT("two.example")
	};
	size_t iSize = 77u;

	testRequire(
		testInstallFailAllocator(),
		"proxy alias build failure allocator install failed"
	);
	testRequire(
		(xrtHttpProxyAliasesBuild(
			Aliases, 2u, &iSize
		) == NULL) && (iSize == 77u) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"proxy alias build OOM was not atomic"
	);
	printf("[PASS] http_proxy_alias_write_oom\n");
	return 0;
}
