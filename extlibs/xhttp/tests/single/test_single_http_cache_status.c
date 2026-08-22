#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头发布必须保留重复 Cache-Status 字段解析。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Cache-Status"), XRT_STR_INIT("Origin;hit") },
		{ XRT_STR_INIT("cache-status"), XRT_STR_INIT("Edge;fwd=stale") }
	};
	xhttpcachestatusfieldcursor Cursor;
	xhttpcachestatus Status;

	xrtHttpCacheStatusFieldCursorInit(&Cursor);
	return (xrtHttpCacheStatusFieldNext(
		Fields, 2, &Cursor, &Status
	) == XHTTP_NEXT_ITEM) &&
		(xrtHttpCacheStatusFieldNext(
			Fields, 2, &Cursor, &Status
		) == XHTTP_NEXT_ITEM) &&
		((Status.Flags & XHTTP_CACHE_STATUS_HAS_FORWARD) != 0) ?
		0 : 1;
}
