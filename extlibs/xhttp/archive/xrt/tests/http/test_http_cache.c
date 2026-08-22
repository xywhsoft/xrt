#include "../test.h"

#include <xrt/http_cache.h>



/* 验证已知指令名称映射和重复字段游标顺序。 */
static void testHttpCacheCursor(void)
{
	static const xhttpcachedirective Directives[] = {
		XHTTP_CACHE_MAX_AGE,
		XHTTP_CACHE_MAX_STALE,
		XHTTP_CACHE_MIN_FRESH,
		XHTTP_CACHE_NO_CACHE,
		XHTTP_CACHE_NO_STORE,
		XHTTP_CACHE_NO_TRANSFORM,
		XHTTP_CACHE_ONLY_IF_CACHED,
		XHTTP_CACHE_MUST_REVALIDATE,
		XHTTP_CACHE_MUST_UNDERSTAND,
		XHTTP_CACHE_PRIVATE,
		XHTTP_CACHE_PROXY_REVALIDATE,
		XHTTP_CACHE_PUBLIC,
		XHTTP_CACHE_S_MAXAGE
	};
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Host"),
			XRT_STR_INIT("example.test")
		},
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT("max-age=60, x-mode=\"fast\"")
		},
		{
			XRT_STR_INIT("cache-control"),
			XRT_STR_INIT("no-transform, public")
		}
	};
	static const xhttpcachedirective Expected[] = {
		XHTTP_CACHE_MAX_AGE,
		XHTTP_CACHE_UNKNOWN,
		XHTTP_CACHE_NO_TRANSFORM,
		XHTTP_CACHE_PUBLIC
	};
	xhttpcachecursor Cursor;
	xhttpcacheitem Item;
	xhttpnext Next;
	uint64 iSeconds = 0;
	size_t i = 0;

	for ( i = 0;
		i < (sizeof(Directives) / sizeof(Directives[0]));
		i++ ) {
		xstrview Name = xrtHttpCacheDirectiveName(
			Directives[i]
		);

		testRequire(
			(Name.Size != 0) &&
			(xrtHttpCacheDirectiveParse(Name) ==
			 Directives[i]),
			"Cache-Control directive round trip mismatch"
		);
	}
	testRequire(
		xrtHttpCacheDirectiveParse(
			XRT_STR_LITERAL("MAX-AGE")
		) == XHTTP_CACHE_MAX_AGE &&
		xrtHttpTokenEqual(
			xrtHttpCacheDirectiveName(
				XHTTP_CACHE_ONLY_IF_CACHED
			),
			XRT_STR_LITERAL("only-if-cached")
		) &&
		(xrtHttpCacheDirectiveParse(
			XRT_STR_LITERAL("x-mode")
		 ) == XHTTP_CACHE_UNKNOWN) &&
		(xrtHttpCacheDirectiveName(
			XHTTP_CACHE_UNKNOWN
		 ).Size == 0),
		"Cache-Control directive mapping mismatch"
	);
	i = 0;
	xrtHttpCacheCursorInit(&Cursor);
	while ( (Next = xrtHttpCacheNext(
		Fields,
		sizeof(Fields) / sizeof(Fields[0]),
		&Cursor,
		&Item
	)) == XHTTP_NEXT_ITEM ) {
		testRequire(
			(i < (sizeof(Expected) / sizeof(Expected[0]))) &&
			(Item.Directive == Expected[i]),
			"Cache-Control cursor order mismatch"
		);
		if ( i == 0 ) {
			testRequire(
				xrtHttpCacheDeltaRead(
					&Item, &iSeconds
				) &&
				(iSeconds == 60),
				"Cache-Control cursor delta mismatch"
			);
		}
		if ( i == 1 ) {
			testRequire(
				xrtHttpTokenEqual(
					Item.Name,
					XRT_STR_LITERAL("x-mode")
				) &&
				((Item.Flags &
				  XHTTP_PARAM_QUOTED) != 0),
				"Cache-Control extension was not preserved"
			);
		}
		i++;
	}
	testRequire(
		(Next == XHTTP_NEXT_END) &&
		(i == (sizeof(Expected) / sizeof(Expected[0]))),
		"Cache-Control cursor did not finish"
	);
}



/* 验证重复字段汇总、扩展保留、限定字段和冲突事实。 */
static void testHttpCacheControl(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT(
				"max-age=60, no-store, "
				"private=\"Authorization, Set-Cookie\", "
				"x-mode=\"fast\""
			)
		},
		{
			XRT_STR_INIT("cache-control"),
			XRT_STR_INIT(
				"s-maxage=\"999999999999999999999\", "
				"no-transform, max-age=120, public"
			)
		}
	};
	xhttpcachecontrol Control;

	testRequire(
		xrtHttpCacheControlParse(
			Fields,
			sizeof(Fields) / sizeof(Fields[0]),
			&Control
		) &&
		xrtHttpCacheControlValid(&Control) &&
		(Control.FieldCount == 2) &&
		(Control.DirectiveCount == 8) &&
		(Control.UnknownCount == 1) &&
		(Control.DuplicateCount == 1) &&
		(Control.InvalidCount == 0) &&
		(Control.DuplicateDirectives ==
		 XHTTP_CACHE_MAX_AGE) &&
		(Control.InvalidDirectives == 0) &&
		(Control.MaxAge == 60) &&
		(Control.SMaxAge == XHTTP_CACHE_DELTA_MAX) &&
		((Control.Flags & (
			XHTTP_CACHE_PRESENT |
			XHTTP_CACHE_NO_STORE |
			XHTTP_CACHE_PRIVATE |
			XHTTP_CACHE_PRIVATE_FIELDS |
			XHTTP_CACHE_EXTENSION |
			XHTTP_CACHE_NO_TRANSFORM |
			XHTTP_CACHE_PUBLIC |
			XHTTP_CACHE_DUPLICATE |
			XHTTP_CACHE_CONFLICT
		 )) == (
			XHTTP_CACHE_PRESENT |
			XHTTP_CACHE_NO_STORE |
			XHTTP_CACHE_PRIVATE |
			XHTTP_CACHE_PRIVATE_FIELDS |
			XHTTP_CACHE_EXTENSION |
			XHTTP_CACHE_NO_TRANSFORM |
			XHTTP_CACHE_PUBLIC |
			XHTTP_CACHE_DUPLICATE |
			XHTTP_CACHE_CONFLICT
		 )),
		"Cache-Control summary facts mismatch"
	);
}



/* 验证增量合并、非法已知参数和失败原子性。 */
static void testHttpCacheAdd(void)
{
	xhttpcachecontrol Control;
	xhttpcachecontrol Before;

	xrtHttpCacheControlInit(&Control);
	testRequire(
		xrtHttpCacheControlValid(&Control) &&
		xrtHttpCacheControlAdd(
			&Control, XRT_STR_LITERAL("")
		) &&
		(Control.FieldCount == 1) &&
		(Control.DirectiveCount == 0) &&
		((Control.Flags & XHTTP_CACHE_PRESENT) != 0),
		"empty Cache-Control field mismatch"
	);
	testRequire(
		xrtHttpCacheControlAdd(
			&Control,
			XRT_STR_LITERAL(
				"max-stale, min-fresh=\"15\", "
				"only-if-cached"
			)
		) &&
		(Control.MinFresh == 15) &&
		((Control.Flags & (
			XHTTP_CACHE_MAX_STALE |
			XHTTP_CACHE_MAX_STALE_ANY |
			XHTTP_CACHE_ONLY_IF_CACHED
		 )) == (
			XHTTP_CACHE_MAX_STALE |
			XHTTP_CACHE_MAX_STALE_ANY |
			XHTTP_CACHE_ONLY_IF_CACHED
		 )),
		"Cache-Control incremental values mismatch"
	);
	testRequire(
		xrtHttpCacheControlAdd(
			&Control,
			XRT_STR_LITERAL(
				"no-store=1, max-age=\"x\", private=\"\""
			)
		) &&
		(Control.InvalidCount == 3) &&
		(Control.InvalidDirectives == (
			XHTTP_CACHE_NO_STORE |
			XHTTP_CACHE_MAX_AGE |
			XHTTP_CACHE_PRIVATE
		 )) &&
		((Control.Flags & XHTTP_CACHE_INVALID) != 0),
		"invalid known Cache-Control arguments were hidden"
	);
	Before = Control;
	testRequire(
		!xrtHttpCacheControlAdd(
			&Control,
			XRT_STR_LITERAL("public, x=\"unterminated")
		) &&
		(memcmp(
			&Control, &Before, sizeof(Control)
		) == 0),
		"malformed Cache-Control changed prior summary"
	);
	xrtClearError();
	testRequire(
		xrtHttpCacheControlAdd(
			&Control,
			XRT_STR_LITERAL("max-stale=5, public")
		) &&
		(Control.DuplicateCount == 1) &&
		(Control.DuplicateDirectives ==
		 XHTTP_CACHE_MAX_STALE) &&
		(Control.MaxStale == 0) &&
		((Control.Flags &
		  XHTTP_CACHE_MAX_STALE_ANY) != 0),
		"duplicate Cache-Control first-value rule mismatch"
	);
}



/* 验证数值容错、quoted-pair 解码和错误输出原子性。 */
static void testHttpCacheDelta(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT(
				"max-age=99999999999999999999, "
				"min-fresh=\"21\\4\", max-stale, "
				"s-maxage=\"-1\""
			)
		}
	};
	xhttpcachecursor Cursor;
	xhttpcacheitem Item;
	uint64 iSeconds = 77;

	testRequire(
		xrtHttpCacheDeltaParse(
			XRT_STR_LITERAL(" \t42\t"), &iSeconds
		) &&
		(iSeconds == 42),
		"Cache-Control strict delta mismatch"
	);
	xrtHttpCacheCursorInit(&Cursor);
	testRequire(
		xrtHttpCacheNext(
			Fields, 1, &Cursor, &Item
		) == XHTTP_NEXT_ITEM &&
		xrtHttpCacheDeltaRead(&Item, &iSeconds) &&
		(iSeconds == XHTTP_CACHE_DELTA_MAX),
		"Cache-Control delta saturation mismatch"
	);
	testRequire(
		xrtHttpCacheNext(
			Fields, 1, &Cursor, &Item
		) == XHTTP_NEXT_ITEM &&
		xrtHttpCacheDeltaRead(&Item, &iSeconds) &&
		(iSeconds == 214),
		"Cache-Control quoted delta mismatch"
	);
	testRequire(
		xrtHttpCacheNext(
			Fields, 1, &Cursor, &Item
		) == XHTTP_NEXT_ITEM,
		"Cache-Control valueless delta setup failed"
	);
	iSeconds = 77;
	testRequire(
		!xrtHttpCacheDeltaRead(&Item, &iSeconds) &&
		(iSeconds == 77),
		"Cache-Control valueless delta changed output"
	);
	xrtClearError();
	testRequire(
		xrtHttpCacheNext(
			Fields, 1, &Cursor, &Item
		) == XHTTP_NEXT_ITEM,
		"Cache-Control invalid delta setup failed"
	);
	iSeconds = 77;
	testRequire(
		!xrtHttpCacheDeltaRead(&Item, &iSeconds) &&
		(iSeconds == 77),
		"Cache-Control invalid delta changed output"
	);
	xrtClearError();
}



/* 验证字段名限定值和公开结构边界。 */
static void testHttpCacheEdges(void)
{
	static const xhttpfield Valid[] = {
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT(
				"no-cache=Authorization, "
				"private=\"ETag,, Last-Modified\""
			)
		}
	};
	static const xhttpfield Invalid[] = {
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT(
				"no-cache=\" \", private=\"foo bar\""
			)
		}
	};
	static const xhttpfield Malformed[] = {
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT("max-age=\"unterminated")
		}
	};
	xhttpcachecontrol Control;
	xhttpcachecontrol Before;
	xhttpcachecursor Cursor;

	testRequire(
		xrtHttpCacheControlParse(
			Valid, 1, &Control
		) &&
		(Control.InvalidCount == 0) &&
		((Control.Flags & (
			XHTTP_CACHE_NO_CACHE_FIELDS |
			XHTTP_CACHE_PRIVATE_FIELDS
		 )) == (
			XHTTP_CACHE_NO_CACHE_FIELDS |
			XHTTP_CACHE_PRIVATE_FIELDS
		 )),
		"qualified Cache-Control field names mismatch"
	);
	testRequire(
		xrtHttpCacheControlParse(
			Invalid, 1, &Control
		) &&
		(Control.InvalidCount == 2) &&
		(Control.InvalidDirectives == (
			XHTTP_CACHE_NO_CACHE |
			XHTTP_CACHE_PRIVATE
		 )) &&
		((Control.Flags & XHTTP_CACHE_INVALID) != 0),
		"invalid qualified Cache-Control was accepted"
	);
	Before = Control;
	testRequire(
		!xrtHttpCacheControlParse(
			Malformed, 1, &Control
		) &&
		(memcmp(
			&Control, &Before, sizeof(Control)
		) == 0),
		"malformed Cache-Control parse changed output"
	);
	xrtClearError();
	Control.Flags = UINT32_MAX;
	testRequire(
		!xrtHttpCacheControlValid(&Control),
		"Cache-Control validator accepted unknown flags"
	);
	xrtHttpCacheControlInit(&Control);
	Control.FieldCount = 1;
	Control.Flags = XHTTP_CACHE_PRESENT |
		XHTTP_CACHE_MAX_AGE;
	testRequire(
		!xrtHttpCacheControlValid(&Control),
		"Cache-Control validator accepted inconsistent counts"
	);
	xrtHttpCacheControlInit(&Control);
	Control.FieldCount = 1;
	Control.DirectiveCount = 1;
	Control.MaxAge = XHTTP_CACHE_DELTA_MAX + 1;
	Control.Flags = XHTTP_CACHE_PRESENT |
		XHTTP_CACHE_MAX_AGE;
	testRequire(
		!xrtHttpCacheControlValid(&Control),
		"Cache-Control validator accepted oversized delta"
	);
	xrtHttpCacheCursorInit(&Cursor);
	Cursor.Field = 2;
	testRequire(
		xrtHttpCacheNext(
			Valid, 1, &Cursor,
			(xhttpcacheitem*)&Cursor
		) == XHTTP_NEXT_ERROR,
		"Cache-Control cursor accepted invalid aliases"
	);
	xrtClearError();
}



/* 运行 Cache-Control 协议事实和边界测试。 */
int main(void)
{
	testHttpCacheCursor();
	testHttpCacheControl();
	testHttpCacheAdd();
	testHttpCacheDelta();
	testHttpCacheEdges();
	printf("[PASS] http_cache\n");
	return 0;
}
