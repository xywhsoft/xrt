#include "../test.h"

#include <xrt/http_cache_validate.h>



/* 验证非法字段值只使验证器不可用，不越界或误选缓存条目。 */
static void testHttpCacheValidateMalformedFields(void)
{
	static const xhttpfield Malformed[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("W/\"bad")
		},
		{
			XRT_STR_INIT("Last-Modified"),
			XRT_STR_INIT("not-a-date")
		},
		{
			XRT_STR_INIT("Content-Length"),
			XRT_STR_INIT("1, 2")
		}
	};
	xhttpcacheentry Entry = {
		Malformed,
		sizeof(Malformed) / sizeof(Malformed[0]),
		0,
		XHTTP_CACHE_ENTRY_NONE
	};
	xhttpcachevalidateplan Plan;
	xhttpcacheifrange IfRange;
	size_t iCount = 0;

	testRequire(
		xrtHttpCacheValidatePlan(
			&Entry, 1, false, &Plan
		) == XHTTP_CACHE_VALIDATE_NONE,
		"malformed cache validators generated a request condition"
	);
	testRequire(
		xrtHttpCacheIfRangePlan(
			&Entry, &IfRange
		) == XHTTP_CACHE_IF_RANGE_NONE,
		"malformed cache validators generated If-Range"
	);
	testRequire(
		(xrtHttpCache304Select(
			Malformed,
			sizeof(Malformed) / sizeof(Malformed[0]),
			&Entry,
			1,
			NULL,
			0,
			&iCount
		 ) == XHTTP_CACHE_UPDATE_MATCH_NONE) &&
		(iCount == 0),
		"malformed 304 validators selected a cache entry"
	);
	testRequire(
		xrtHttpCacheHeadPlan(
			200,
			&Entry,
			Malformed,
			sizeof(Malformed) / sizeof(Malformed[0])
		) == XHTTP_CACHE_HEAD_STALE,
		"malformed HEAD metadata kept a cache entry fresh"
	);
}



/* 验证截断视图、非法状态和破坏后的游标都被安全拒绝。 */
static void testHttpCacheValidateArguments(void)
{
	const char BadName[] = { 'E', 'T', 'a', 'g', '\n' };
	xhttpfield Field = {
		{ BadName, sizeof(BadName) },
		XRT_STR_LITERAL("\"x\"")
	};
	xhttpcacheentry Entry = {
		&Field,
		1,
		0,
		XHTTP_CACHE_ENTRY_NONE
	};
	xhttpcacheinvalidatecursor Cursor;
	xhttpcacheinvalidateitem Item;
	xhttpcachevalidateplan Plan;

	testRequire(
		!xrtHttpCacheEntryValid(&Entry),
		"cache entry accepted an invalid field name"
	);
	testRequire(
		xrtHttpCacheValidatePlan(
			&Entry, 1, false, &Plan
		) == XHTTP_CACHE_VALIDATE_ERROR,
		"cache validation accepted invalid entry fields"
	);
	xrtClearError();
	testRequire(
		xrtHttpCacheValidateResult(
			199, false
		) == XHTTP_CACHE_VALIDATE_RESULT_ERROR,
		"cache validation accepted a non-final status"
	);
	xrtClearError();
	xrtHttpCacheInvalidationCursorInit(&Cursor);
	Cursor.Field = 2;
	testRequire(
		xrtHttpCacheInvalidationNext(
			XRT_STR_LITERAL("POST"),
			200,
			XRT_STR_LITERAL("https://example.test/"),
			NULL,
			0,
			&Cursor,
			&Item
		) == XHTTP_NEXT_ERROR,
		"cache invalidation accepted a corrupt cursor"
	);
	xrtClearError();
}



/* 执行 HTTP 缓存验证畸形输入测试。 */
int main(void)
{
	testHttpCacheValidateMalformedFields();
	testHttpCacheValidateArguments();
	printf("[PASS] http_cache_validate_mutation\n");
	return 0;
}
