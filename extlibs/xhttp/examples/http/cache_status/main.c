#include <stdio.h>

#include <xrt/http_cache_status.h>



/* 按链路顺序读取缓存命中与转发诊断。 */
int main(void)
{
	xstrview Value = XRT_STR_LITERAL(
		"OriginCache;hit;ttl=300, Edge;fwd=stale;fwd-status=304"
	);
	xhttpcachestatuscursor Cursor;
	xhttpcachestatus Status;

	xrtHttpCacheStatusCursorInit(&Cursor);
	while ( xrtHttpCacheStatusNext(
		Value, &Cursor, &Status
	) == XHTTP_NEXT_ITEM ) {
		printf(
			"cache = %.*s, hit = %s\n",
			(int)Status.Cache.Encoded.Size,
			Status.Cache.Encoded.Data,
			Status.Hit != 0 ? "true" : "false"
		);
	}
	return xrtGetError() == NULL ? 0 : 1;
}
