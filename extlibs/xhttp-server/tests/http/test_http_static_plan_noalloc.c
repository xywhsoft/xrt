#include "../test_allocator.h"



/* 静态协议计划的条件与多范围热路径不得依赖堆分配。 */
int main(void)
{
	xhttpfield Fields[] = {
		{
			XRT_STR_INIT("If-Range"),
			XRT_STR_INIT("\"asset\"")
		},
		{
			XRT_STR_INIT("Range"),
			XRT_STR_INIT("bytes=50-99, 0-9, 8-19")
		}
	};
	xhttprepresentation Current;
	xhttpstaticplanconfig Config;
	xhttpbyterange Ranges[16];
	xhttpstaticplan Plan;

	memset(&Current, 0, sizeof(Current));
	Current.Exists = true;
	Current.HasETag = true;
	Current.ETag.Opaque = XRT_STR_LITERAL("asset");
	xrtHttpStaticPlanConfigInit(&Config);
	testRequire(testInstallFailAllocator(),
		"HTTP static plan failure allocator install failed");
	testRequire(xrtHttpStaticPlanBuild(
		XRT_STR_LITERAL("GET"),
		Fields,
		2,
		&Current,
		100,
		Ranges,
		16,
		&Config,
		&Plan
	) && (Plan.Status == XHTTP_STATUS_PARTIAL_CONTENT) &&
		(Plan.RangeCount == 2) &&
		(Plan.SelectedLength == 70),
		"HTTP static plan allocated memory");
	printf("[PASS] http_static_plan_noalloc\n");
	return 0;
}
