#include "../test_allocator.h"

#include <xrt/http_cache_status.h>



/* Cache-Status 完整预校验、参数转换和字段迭代必须零分配。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Cache-Status"), XRT_STR_INIT("Origin;hit") },
		{ XRT_STR_INIT("cache-status"), XRT_STR_INIT("Edge;fwd=stale;ttl=-1") }
	};
	xhttpcachestatusfieldcursor Cursor;
	xhttpcachestatus Status;

	testRequire(
		testInstallFailAllocator(),
		"Cache-Status failure allocator install failed"
	);
	xrtHttpCacheStatusFieldCursorInit(&Cursor);
	testRequire(
		(xrtHttpCacheStatusFieldNext(
			Fields, 2, &Cursor, &Status
		) == XHTTP_NEXT_ITEM) &&
		(xrtHttpCacheStatusFieldNext(
			Fields, 2, &Cursor, &Status
		) == XHTTP_NEXT_ITEM) &&
		(Status.Ttl == -1),
		"Cache-Status parsing allocated"
	);
	printf("[PASS] http_cache_status_noalloc\n");
	return 0;
}
