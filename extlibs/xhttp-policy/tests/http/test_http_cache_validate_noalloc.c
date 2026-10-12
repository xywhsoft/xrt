#include "../test_allocator.h"

#include <xrt/http_cache_validate.h>



/* 验证缓存协议计划和直接目标失效写出全程不分配内存。 */
int main(void)
{
	static const xhttpfield Stored[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"asset\"")
		},
		{
			XRT_STR_INIT("Last-Modified"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:35 GMT")
		},
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:37 GMT")
		},
		{
			XRT_STR_INIT("Content-Length"),
			XRT_STR_INIT("5")
		}
	};
	static const xhttpfield NotModified[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"asset\"")
		}
	};
	static const xhttpfield Location[] = {
		{
			XRT_STR_INIT("Location"),
			XRT_STR_INIT("/next")
		}
	};
	xhttpcacheentry Entry = {
		Stored,
		sizeof(Stored) / sizeof(Stored[0]),
		0,
		XHTTP_CACHE_ENTRY_NONE
	};
	const xstrview Target =
		XRT_STR_LITERAL("https://example.test/base");
	xhttpcachevalidateplan Validate;
	xhttpcacheifrange IfRange;
	xhttpcacheinvalidatecursor Cursor;
	xhttpcacheinvalidateitem Item;
	size_t Indices[1];
	size_t iCount = 0;
	size_t iSize = 0;
	char Output[128];
	bool bPass;

	testRequire(
		testInstallFailAllocator(),
		"HTTP cache validation failure allocator install failed"
	);
	xrtHttpCacheInvalidationCursorInit(&Cursor);
	bPass =
		(xrtHttpCacheValidatePlan(
			&Entry, 1, false, &Validate
		 ) == XHTTP_CACHE_VALIDATE_CONDITIONAL) &&
		xrtHttpCacheValidateETagsWrite(
			&Entry, 1, false,
			Output, sizeof(Output), &iSize
		) &&
		(xrtHttpCacheIfRangePlan(
			&Entry, &IfRange
		 ) == XHTTP_CACHE_IF_RANGE_ETAG) &&
		(xrtHttpCache304Select(
			NotModified, 1,
			&Entry, 1,
			Indices, 1,
			&iCount
		 ) == XHTTP_CACHE_UPDATE_MATCH_STRONG) &&
		(xrtHttpCacheHeadPlan(
			200, &Entry,
			Stored,
			sizeof(Stored) / sizeof(Stored[0])
		 ) == XHTTP_CACHE_HEAD_UPDATE) &&
		(xrtHttpCacheInvalidationNext(
			XRT_STR_LITERAL("POST"),
			200,
			Target,
			Location,
			1,
			&Cursor,
			&Item
		 ) == XHTTP_NEXT_ITEM) &&
		xrtHttpCacheInvalidationWrite(
			Target, &Item,
			Output, sizeof(Output), &iSize
		);
	testRequire(
		bPass,
		"HTTP cache validation path allocated memory"
	);
	printf("[PASS] http_cache_validate_noalloc\n");
	return 0;
}
