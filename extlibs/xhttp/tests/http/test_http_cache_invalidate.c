#include "../test.h"

#include <xrt/http_cache_validate.h>



/* 验证 unsafe 成功响应只返回目标和同源位置候选。 */
static void testHttpCacheInvalidationCandidates(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Location"),
			XRT_STR_INIT("/next?q=1#part")
		},
		{
			XRT_STR_INIT("Content-Location"),
			XRT_STR_INIT(
				"https://outside.test/content"
			)
		}
	};
	const xstrview Target =
		XRT_STR_LITERAL("https://example.test/base/item");
	xhttpcacheinvalidatecursor Cursor;
	xhttpcacheinvalidateitem Item;
	xhttpnext Next;
	char Output[128];
	size_t iSize = 0;

	xrtHttpCacheInvalidationCursorInit(&Cursor);
	Next = xrtHttpCacheInvalidationNext(
		XRT_STR_LITERAL("POST"),
		204,
		Target,
		Fields,
		2,
		&Cursor,
		&Item
	);
	testRequire(
		(Next == XHTTP_NEXT_ITEM) &&
		(Item.Kind == XHTTP_CACHE_INVALIDATE_TARGET) &&
		xrtHttpCacheInvalidationWrite(
			Target, &Item,
			Output, sizeof(Output), &iSize
		) &&
		(iSize == Target.Size) &&
		(memcmp(Output, Target.Data, iSize) == 0),
		"HTTP cache target invalidation mismatch"
	);
	Next = xrtHttpCacheInvalidationNext(
		XRT_STR_LITERAL("POST"),
		204,
		Target,
		Fields,
		2,
		&Cursor,
		&Item
	);
	testRequire(
		(Next == XHTTP_NEXT_ITEM) &&
		(Item.Kind == XHTTP_CACHE_INVALIDATE_LOCATION) &&
		xrtHttpCacheInvalidationWrite(
			Target, &Item,
			Output, sizeof(Output), &iSize
		) &&
		(iSize == strlen(
			"https://example.test/next?q=1"
		)) &&
		(memcmp(
			Output,
			"https://example.test/next?q=1",
			iSize
		) == 0),
		"HTTP cache relative Location invalidation mismatch"
	);
	testRequire(
		xrtHttpCacheInvalidationNext(
			XRT_STR_LITERAL("POST"),
			204,
			Target,
			Fields,
			2,
			&Cursor,
			&Item
		) == XHTTP_NEXT_END,
		"HTTP cache exposed cross-origin invalidation"
	);
}



/* 验证安全方法、错误响应、未知方法和重复字段边界。 */
static void testHttpCacheInvalidationPolicy(void)
{
	static const xhttpfield Duplicate[] = {
		{
			XRT_STR_INIT("Location"),
			XRT_STR_INIT("/one")
		},
		{
			XRT_STR_INIT("Location"),
			XRT_STR_INIT("/two")
		}
	};
	static const xhttpfield SameOrigin[] = {
		{
			XRT_STR_INIT("Content-Location"),
			XRT_STR_INIT(
				"//EXAMPLE.test:443/content#f"
			)
		}
	};
	static const xhttpfield EmptyPortOrigin[] = {
		{
			XRT_STR_INIT("Location"),
			XRT_STR_INIT(
				"https://EXAMPLE.test:/empty-port"
			)
		}
	};
	const xstrview Target =
		XRT_STR_LITERAL("https://example.test/base");
	xhttpcacheinvalidatecursor Cursor;
	xhttpcacheinvalidateitem Item;
	xhttpnext Next;
	char Output[128];
	size_t iSize = 0;

	xrtHttpCacheInvalidationCursorInit(&Cursor);
	testRequire(
		xrtHttpCacheInvalidationNext(
			XRT_STR_LITERAL("GET"),
			200,
			Target,
			NULL,
			0,
			&Cursor,
			&Item
		) == XHTTP_NEXT_END,
		"HTTP cache invalidated a safe method"
	);
	xrtHttpCacheInvalidationCursorInit(&Cursor);
	testRequire(
		xrtHttpCacheInvalidationNext(
			XRT_STR_LITERAL("DELETE"),
			404,
			Target,
			NULL,
			0,
			&Cursor,
			&Item
		) == XHTTP_NEXT_END,
		"HTTP cache invalidated an error response"
	);

	/* 未知方法按规范视为不安全。 */
	xrtHttpCacheInvalidationCursorInit(&Cursor);
	testRequire(
		(xrtHttpCacheInvalidationNext(
			XRT_STR_LITERAL("PATCH"),
			200,
			Target,
			NULL,
			0,
			&Cursor,
			&Item
		 ) == XHTTP_NEXT_ITEM) &&
		(Item.Kind == XHTTP_CACHE_INVALIDATE_TARGET),
		"HTTP cache did not invalidate an unknown unsafe method"
	);

	/* 重复 singleton Location 被忽略，但目标仍然返回。 */
	xrtHttpCacheInvalidationCursorInit(&Cursor);
	testRequire(
		xrtHttpCacheInvalidationNext(
			XRT_STR_LITERAL("POST"),
			200,
			Target,
			Duplicate,
			2,
			&Cursor,
			&Item
		) == XHTTP_NEXT_ITEM,
		"HTTP cache duplicate fixture lost target"
	);
	testRequire(
		xrtHttpCacheInvalidationNext(
			XRT_STR_LITERAL("POST"),
			200,
			Target,
			Duplicate,
			2,
			&Cursor,
			&Item
		) == XHTTP_NEXT_END,
		"HTTP cache accepted duplicate Location"
	);

	/* network-path 引用按继承 scheme 后的默认端口比较 origin。 */
	xrtHttpCacheInvalidationCursorInit(&Cursor);
	(void)xrtHttpCacheInvalidationNext(
		XRT_STR_LITERAL("POST"),
		302,
		Target,
		SameOrigin,
		1,
		&Cursor,
		&Item
	);
	Next = xrtHttpCacheInvalidationNext(
		XRT_STR_LITERAL("POST"),
		302,
		Target,
		SameOrigin,
		1,
		&Cursor,
		&Item
	);
	testRequire(
		(Next == XHTTP_NEXT_ITEM) &&
		(Item.Kind ==
		 XHTTP_CACHE_INVALIDATE_CONTENT_LOCATION),
		"HTTP cache rejected same-origin authority reference"
	);
	testRequire(
		xrtHttpCacheInvalidationWrite(
			Target, &Item,
			NULL, 0, &iSize
		) && (iSize == strlen(
			"https://EXAMPLE.test:443/content"
		)),
		"HTTP cache same-origin reference length mismatch"
	);
	testRequire(
		xrtHttpCacheInvalidationWrite(
			Target, &Item,
			Output, sizeof(Output), &iSize
		) &&
		(iSize == strlen(
			"https://EXAMPLE.test:443/content"
		)) &&
		(memcmp(
			Output,
			"https://EXAMPLE.test:443/content",
			iSize
		) == 0),
		"HTTP cache same-origin authority reference mismatch"
	);

	/* 显式空端口与省略端口都使用 HTTPS 默认端口。 */
	xrtHttpCacheInvalidationCursorInit(&Cursor);
	(void)xrtHttpCacheInvalidationNext(
		XRT_STR_LITERAL("POST"),
		200,
		XRT_STR_LITERAL("https://example.test:/base"),
		EmptyPortOrigin,
		1,
		&Cursor,
		&Item
	);
	testRequire(
		(xrtHttpCacheInvalidationNext(
			XRT_STR_LITERAL("POST"),
			200,
			XRT_STR_LITERAL("https://example.test:/base"),
			EmptyPortOrigin,
			1,
			&Cursor,
			&Item
		) == XHTTP_NEXT_ITEM) &&
		(Item.Kind == XHTTP_CACHE_INVALIDATE_LOCATION),
		"HTTP cache rejected equivalent empty default ports"
	);
}



/* 验证失效 URI 的短缓冲和手工候选安全检查。 */
static void testHttpCacheInvalidationOutput(void)
{
	char TargetText[] = "http://example.test/base";
	xstrview Target = {
		TargetText,
		sizeof(TargetText) - 1u
	};
	xhttpcacheinvalidateitem Item;
	xhttpcacheinvalidateitem Saved;
	union {
		size_t Values[8];
		char Bytes[64];
	} Aliased;
	char Output[64];
	size_t iSize = 0;

	Item.Reference = XRT_STR_LITERAL("../next#fragment");
	Item.Field = 0;
	Item.Kind = XHTTP_CACHE_INVALIDATE_LOCATION;
	memset(Output, 'z', sizeof(Output));
	testRequire(
		xrtHttpCacheInvalidationWrite(
			Target, &Item, NULL, 0, &iSize
		) &&
		(iSize == strlen("http://example.test/next")),
		"HTTP cache invalidation size query mismatch"
	);
	testRequire(
		!xrtHttpCacheInvalidationWrite(
			Target, &Item,
			Output, iSize - 1u, &iSize
		) && (Output[0] == 'z'),
		"HTTP cache invalidation short buffer changed output"
	);
	xrtClearError();
	Item.Reference = XRT_STR_LITERAL(
		"https://outside.test/next"
	);
	testRequire(
		!xrtHttpCacheInvalidationWrite(
			Target, &Item,
			Output, sizeof(Output), &iSize
		),
		"HTTP cache invalidation writer accepted cross-origin item"
	);
	xrtClearError();

	/* 输出、数量和借用输入之间的重叠必须在任何写入前拒绝。 */
	Item.Reference = XRT_STR_LITERAL("../next#fragment");
	Item.Field = 0;
	Item.Kind = XHTTP_CACHE_INVALIDATE_LOCATION;
	Saved = Item;
	testRequire(
		!xrtHttpCacheInvalidationWrite(
			Target, &Item,
			&Item, sizeof(Item), &iSize
		) && (memcmp(
			&Item, &Saved, sizeof(Item)
		) == 0),
		"HTTP cache invalidation output changed its item"
	);
	xrtClearError();
	testRequire(
		!xrtHttpCacheInvalidationWrite(
			Target, &Item,
			TargetText, sizeof(TargetText), &iSize
		) && (memcmp(
			TargetText,
			"http://example.test/base",
			sizeof(TargetText)
		) == 0),
		"HTTP cache invalidation output changed its target"
	);
	xrtClearError();

	memcpy(
		Aliased.Bytes,
		"http://example.test/base",
		sizeof(TargetText)
	);
	Target.Data = Aliased.Bytes;
	Target.Size = sizeof(TargetText) - 1u;
	testRequire(
		!xrtHttpCacheInvalidationWrite(
			Target, &Item,
			Output, sizeof(Output),
			&Aliased.Values[0]
		) && (memcmp(
			Aliased.Bytes,
			"http://example.test/base",
			sizeof(TargetText)
		) == 0),
		"HTTP cache invalidation size changed its target"
	);
	xrtClearError();
}



/* 执行 HTTP 缓存 unsafe 方法失效测试。 */
int main(void)
{
	testHttpCacheInvalidationCandidates();
	testHttpCacheInvalidationPolicy();
	testHttpCacheInvalidationOutput();
	printf("[PASS] http_cache_invalidate\n");
	return 0;
}
