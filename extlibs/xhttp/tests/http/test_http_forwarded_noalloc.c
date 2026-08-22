#include "../test_allocator.h"

#include <xrt/http_forwarded.h>



/* 转义 Host、IPv6 和重复字段解析必须保持零堆分配。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Forwarded"),
			XRT_STR_INIT(
				"for=\"\\[2001:db8::1]:443\";"
				"host=\"exa\\mple.com:443\";proto=https"
			)
		},
		{
			XRT_STR_INIT("forwarded"),
			XRT_STR_INIT("for=_hidden")
		},
		{
			XRT_STR_INIT("Forwarded"),
			XRT_STR_INIT(
				"for=\"_node:_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
				"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
				"aaaaaaaaaaaaaaaa\""
			)
		}
	};
	xhttpforwardedfieldcursor Cursor;
	xhttpforwarded Forwarded;
	size_t iCount;

	testRequire(testInstallFailAllocator(),
		"Forwarded failure allocator install failed");
	xrtHttpForwardedFieldCursorInit(&Cursor);
	testRequire(
		xrtHttpForwardedFieldCount(
			Fields, 3u, &iCount
		) && (iCount == 3u) &&
		(xrtHttpForwardedFieldNext(
			Fields, 3u, &Cursor, &Forwarded
		) == XHTTP_NEXT_ITEM) &&
		(xrtHttpForwardedFieldNext(
			Fields, 3u, &Cursor, &Forwarded
		) == XHTTP_NEXT_ITEM) &&
		(xrtHttpForwardedFieldNext(
			Fields, 3u, &Cursor, &Forwarded
		) == XHTTP_NEXT_ITEM) &&
		(xrtHttpForwardedFieldNext(
			Fields, 3u, &Cursor, &Forwarded
		) == XHTTP_NEXT_END),
		"Forwarded parser allocated"
	);
	printf("[PASS] http_forwarded_noalloc\n");
	return 0;
}
