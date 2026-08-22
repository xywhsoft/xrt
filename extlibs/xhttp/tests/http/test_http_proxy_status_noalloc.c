#include "../test_allocator.h"

#include <xrt/http_proxy_status.h>



/* Proxy-Status 完整预校验和重复字段迭代必须零分配。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Proxy-Status"), XRT_STR_INIT("OriginProxy") },
		{ XRT_STR_INIT("proxy-status"), XRT_STR_INIT("EdgeProxy;error=dns_timeout") }
	};
	xhttpproxystatusfieldcursor Cursor;
	xhttpproxystatus Status;

	testRequire(
		testInstallFailAllocator(),
		"Proxy-Status failure allocator install failed"
	);
	xrtHttpProxyStatusFieldCursorInit(&Cursor);
	testRequire(
		(xrtHttpProxyStatusFieldNext(
			Fields, 2, &Cursor, &Status
		) == XHTTP_NEXT_ITEM) &&
		(xrtHttpProxyStatusFieldNext(
			Fields, 2, &Cursor, &Status
		) == XHTTP_NEXT_ITEM) &&
		((Status.Flags & XHTTP_PROXY_STATUS_HAS_ERROR) != 0),
		"Proxy-Status parsing allocated"
	);
	printf("[PASS] http_proxy_status_noalloc\n");
	return 0;
}
