#include "../test_allocator.h"

#include <xrt/http_route.h>



/* 验证长路径与参数捕获在失败分配器下仍不分配内存或消耗调用栈。 */
int main(void)
{
	static char Path[65536];
	xhttprouteparam Params[2];
	size_t iCount;
	size_t i;

	Path[0] = '/';
	for ( i = 1u; i < (sizeof(Path) - 1u); i++ ) {
		Path[i] = (i % 97u) == 0 ? '/' : 'a';
	}
	Path[sizeof(Path) - 1u] = 'z';
	testRequire(
		testInstallFailAllocator(),
		"HTTP route failure allocator install failed"
	);
	testRequire(
		xrtHttpRouteMatch(
			XRT_STR_LITERAL("/{first}/{rest...}"),
			(xstrview){ Path, sizeof(Path) },
			Params, 2, &iCount
		) == XHTTP_ROUTE_MATCH && (iCount == 2) &&
		(Params[0].Value.Size == 96u) &&
		(Params[1].Value.Size == (sizeof(Path) - 98u)),
		"HTTP route long no-allocation match failed"
	);
	puts("[PASS] http_route_noalloc");
	return 0;
}
