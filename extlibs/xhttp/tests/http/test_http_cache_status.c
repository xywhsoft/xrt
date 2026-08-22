#include "../test.h"

#include <xrt/http_cache_status.h>



/* 按字节比较借用视图。 */
static bool testCacheStatusViewEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 验证全部 RFC 9211 参数和成员线路顺序。 */
static void testCacheStatusKnown(void)
{
	xstrview Value = XRT_STR_LITERAL(
		"ExampleCache;hit;ttl=1100;key=\"https://example.com/\", "
		"\"Example CDN\";fwd=stale;fwd-status=304;stored;"
		"collapsed=?0;detail=MEMORY"
	);
	xhttpcachestatuscursor Cursor;
	xhttpcachestatus Status;

	testRequire(
		xrtHttpCacheStatusValid(Value),
		"valid Cache-Status was rejected"
	);
	xrtHttpCacheStatusCursorInit(&Cursor);
	testRequire(
		(xrtHttpCacheStatusNext(
			Value, &Cursor, &Status
		) == XHTTP_NEXT_ITEM) &&
		(Status.Cache.Type == XHTTP_STRUCTURED_TOKEN) &&
		testCacheStatusViewEqual(
			Status.Cache.Encoded, XRT_STR_LITERAL("ExampleCache")
		) && (Status.Hit == 1u) && (Status.Ttl == 1100) &&
		((Status.Flags &
			(XHTTP_CACHE_STATUS_HAS_HIT |
			 XHTTP_CACHE_STATUS_HAS_TTL |
			 XHTTP_CACHE_STATUS_HAS_KEY)) ==
			(XHTTP_CACHE_STATUS_HAS_HIT |
			 XHTTP_CACHE_STATUS_HAS_TTL |
			 XHTTP_CACHE_STATUS_HAS_KEY)),
		"first Cache-Status member mismatch"
	);
	testRequire(
		(xrtHttpCacheStatusNext(
			Value, &Cursor, &Status
		) == XHTTP_NEXT_ITEM) &&
		(Status.Cache.Type == XHTTP_STRUCTURED_STRING) &&
		testCacheStatusViewEqual(
			Status.Cache.Encoded, XRT_STR_LITERAL("Example CDN")
		) && testCacheStatusViewEqual(
			Status.Forward.Encoded, XRT_STR_LITERAL("stale")
		) && (Status.ForwardStatus == 304) &&
		(Status.Stored == 1u) && (Status.Collapsed == 0) &&
		(Status.Detail.Type == XHTTP_STRUCTURED_TOKEN) &&
		(Status.InvalidFlags == 0) && (Status.Issues == 0),
		"second Cache-Status member mismatch"
	);
	testRequire(
		xrtHttpCacheStatusNext(
			Value, &Cursor, &Status
		) == XHTTP_NEXT_END,
		"Cache-Status iterator did not end"
	);
}



/* 验证已知参数类型错误会被报告而不会隐藏其他成员信息。 */
static void testCacheStatusInvalidParameters(void)
{
	xstrview Value = XRT_STR_LITERAL(
		"Cache;hit=1;fwd=\"bad\";fwd-status=99;ttl=abc;"
		"stored=1;collapsed=0;key=token;detail=:YQ==:"
	);
	xhttpcachestatuscursor Cursor;
	xhttpcachestatus Status;

	xrtHttpCacheStatusCursorInit(&Cursor);
	testRequire(
		(xrtHttpCacheStatusNext(
			Value, &Cursor, &Status
		) == XHTTP_NEXT_ITEM) &&
		(Status.Flags == 0) &&
		(Status.InvalidFlags ==
			(XHTTP_CACHE_STATUS_HAS_HIT |
			 XHTTP_CACHE_STATUS_HAS_FORWARD |
			 XHTTP_CACHE_STATUS_HAS_FORWARD_STATUS |
			 XHTTP_CACHE_STATUS_HAS_TTL |
			 XHTTP_CACHE_STATUS_HAS_STORED |
			 XHTTP_CACHE_STATUS_HAS_COLLAPSED |
			 XHTTP_CACHE_STATUS_HAS_KEY |
			 XHTTP_CACHE_STATUS_HAS_DETAIL)),
		"Cache-Status invalid parameter flags mismatch"
	);
}



/* 验证重复参数最后值以及非致命组合诊断。 */
static void testCacheStatusDuplicatesAndIssues(void)
{
	xhttpcachestatuscursor Cursor;
	xhttpcachestatus Status;

	xrtHttpCacheStatusCursorInit(&Cursor);
	testRequire(
		(xrtHttpCacheStatusNext(
			XRT_STR_LITERAL("Cache;hit=1;hit;ttl=abc;ttl=-5"),
			&Cursor, &Status
		) == XHTTP_NEXT_ITEM) && (Status.Hit == 1u) &&
		(Status.Ttl == -5) && (Status.InvalidFlags == 0),
		"Cache-Status duplicate final value mismatch"
	);
	xrtHttpCacheStatusCursorInit(&Cursor);
	testRequire(
		(xrtHttpCacheStatusNext(
			XRT_STR_LITERAL("Cache;hit;fwd=miss;stored"),
			&Cursor, &Status
		) == XHTTP_NEXT_ITEM) &&
		((Status.Issues &
			XHTTP_CACHE_STATUS_ISSUE_HIT_AND_FORWARD) != 0) &&
		((Status.Issues &
			XHTTP_CACHE_STATUS_ISSUE_FORWARD_REQUIRED) == 0),
		"Cache-Status hit/fwd issue mismatch"
	);
	xrtHttpCacheStatusCursorInit(&Cursor);
	testRequire(
		(xrtHttpCacheStatusNext(
			XRT_STR_LITERAL("Cache;stored"),
			&Cursor, &Status
		) == XHTTP_NEXT_ITEM) &&
		((Status.Issues &
			XHTTP_CACHE_STATUS_ISSUE_FORWARD_REQUIRED) != 0),
		"Cache-Status fwd dependency issue mismatch"
	);
}



/* 验证重复字段行保持从源站到用户侧的成员顺序。 */
static void testCacheStatusFields(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Cache-Status"), XRT_STR_INIT("Origin;hit") },
		{ XRT_STR_INIT("Other"), XRT_STR_INIT("ignored") },
		{ XRT_STR_INIT("cache-status"), XRT_STR_INIT("Edge;fwd=uri-miss") }
	};
	xhttpcachestatusfieldcursor Cursor;
	xhttpcachestatus Status;

	xrtHttpCacheStatusFieldCursorInit(&Cursor);
	testRequire(
		(xrtHttpCacheStatusFieldNext(
			Fields, 3, &Cursor, &Status
		) == XHTTP_NEXT_ITEM) &&
		testCacheStatusViewEqual(
			Status.Cache.Encoded, XRT_STR_LITERAL("Origin")
		),
		"first repeated Cache-Status field mismatch"
	);
	testRequire(
		(xrtHttpCacheStatusFieldNext(
			Fields, 3, &Cursor, &Status
		) == XHTTP_NEXT_ITEM) &&
		testCacheStatusViewEqual(
			Status.Cache.Encoded, XRT_STR_LITERAL("Edge")
		) && (xrtHttpCacheStatusFieldNext(
			Fields, 3, &Cursor, &Status
		) == XHTTP_NEXT_END),
		"repeated Cache-Status field order mismatch"
	);
}



/* 验证完整预校验、游标原子性和未对齐输出。 */
static void testCacheStatusFailureAndMemory(void)
{
	static const xhttpfield InvalidFields[] = {
		{ XRT_STR_INIT("Cache-Status"), XRT_STR_INIT("Good") },
		{ XRT_STR_INIT("cache-status"), XRT_STR_INIT("(bad member)") }
	};
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xhttpcachestatus) + 1u];
	} Storage;
	xhttpcachestatus* pStatus =
		(xhttpcachestatus*)(Storage.Bytes + 1u);
	xhttpcachestatusfieldcursor FieldCursor;
	xhttpcachestatuscursor Cursor;
	xhttpcachestatuscursor Saved;
	xhttpcachestatus Loaded;

	xrtHttpCacheStatusCursorInit(&Cursor);
	Saved = Cursor;
	memset(pStatus, 0xA5, sizeof(*pStatus));
	testRequire(
		xrtHttpCacheStatusNext(
			XRT_STR_LITERAL("Good, (bad member)"),
			&Cursor, pStatus
		) == XHTTP_NEXT_ERROR,
		"Cache-Status accepted invalid later member"
	);
	memcpy(&Loaded, pStatus, sizeof(Loaded));
	testRequire(
		(memcmp(&Cursor, &Saved, sizeof(Saved)) == 0) &&
		((uint8)Loaded.Cache.Type == 0xA5u),
		"Cache-Status failure was not atomic"
	);
	xrtClearError();
	xrtHttpCacheStatusFieldCursorInit(&FieldCursor);
	testRequire(
		xrtHttpCacheStatusFieldNext(
			InvalidFields, 2, &FieldCursor, pStatus
		) == XHTTP_NEXT_ERROR,
		"repeated Cache-Status accepted invalid later member"
	);
	xrtClearError();
	xrtHttpCacheStatusCursorInit(&Cursor);
	testRequire(
		xrtHttpCacheStatusNext(
			XRT_STR_LITERAL("Good;hit"), &Cursor, pStatus
		) == XHTTP_NEXT_ITEM,
		"Cache-Status rejected unaligned output"
	);
	memcpy(&Loaded, pStatus, sizeof(Loaded));
	testRequire(
		(Loaded.Hit == 1u) &&
		((Loaded.Flags & XHTTP_CACHE_STATUS_HAS_HIT) != 0),
		"Cache-Status unaligned output mismatch"
	);
}



/* 验证游标不能跨字段值或字段数组复用。 */
static void testCacheStatusCursorBinding(void)
{
	static const xhttpfield FirstFields[] = {
		{ XRT_STR_INIT("Cache-Status"), XRT_STR_INIT("One") },
		{ XRT_STR_INIT("Cache-Status"), XRT_STR_INIT("Two") }
	};
	static const xhttpfield OtherFields[] = {
		{ XRT_STR_INIT("Cache-Status"), XRT_STR_INIT("Six") },
		{ XRT_STR_INIT("Cache-Status"), XRT_STR_INIT("Ten") }
	};
	char arrFirst[] = "One, Two";
	char arrOther[] = "Six, Ten";
	xhttpcachestatusfieldcursor FieldCursor;
	xhttpcachestatusfieldcursor SavedFieldCursor;
	xhttpcachestatuscursor Cursor;
	xhttpcachestatuscursor SavedCursor;
	xhttpcachestatus Status;
	xhttpcachestatus SavedStatus;

	xrtHttpCacheStatusCursorInit(&Cursor);
	testRequire(
		xrtHttpCacheStatusNext(
			(xstrview){ arrFirst, sizeof(arrFirst) - 1u },
			&Cursor, &Status
		) == XHTTP_NEXT_ITEM,
		"Cache-Status source binding setup failed"
	);
	SavedCursor = Cursor;
	SavedStatus = Status;
	testRequire(
		xrtHttpCacheStatusNext(
			(xstrview){ arrOther, sizeof(arrOther) - 1u },
			&Cursor, &Status
		) == XHTTP_NEXT_ERROR,
		"Cache-Status cursor accepted another equal-size value"
	);
	testRequire(
		(memcmp(&Cursor, &SavedCursor, sizeof(Cursor)) == 0) &&
		(memcmp(&Status, &SavedStatus, sizeof(Status)) == 0),
		"Cache-Status source mismatch was not atomic"
	);
	xrtClearError();
	testRequire(
		(xrtHttpCacheStatusNext(
			(xstrview){ arrFirst, sizeof(arrFirst) - 1u },
			&Cursor, &Status
		) == XHTTP_NEXT_ITEM) &&
		testCacheStatusViewEqual(
			Status.Cache.Encoded, XRT_STR_LITERAL("Two")
		),
		"Cache-Status cursor could not resume its source"
	);

	xrtHttpCacheStatusFieldCursorInit(&FieldCursor);
	testRequire(
		xrtHttpCacheStatusFieldNext(
			FirstFields, 2u, &FieldCursor, &Status
		) == XHTTP_NEXT_ITEM,
		"Cache-Status field binding setup failed"
	);
	SavedFieldCursor = FieldCursor;
	SavedStatus = Status;
	testRequire(
		xrtHttpCacheStatusFieldNext(
			OtherFields, 2u, &FieldCursor, &Status
		) == XHTTP_NEXT_ERROR,
		"Cache-Status field cursor accepted another array"
	);
	testRequire(
		(memcmp(
			&FieldCursor, &SavedFieldCursor, sizeof(FieldCursor)
		) == 0) &&
		(memcmp(&Status, &SavedStatus, sizeof(Status)) == 0),
		"Cache-Status field source mismatch was not atomic"
	);
}



/* 运行 RFC 9211 Cache-Status 解析测试。 */
int main(void)
{
	testCacheStatusKnown();
	testCacheStatusInvalidParameters();
	testCacheStatusDuplicatesAndIssues();
	testCacheStatusFields();
	testCacheStatusFailureAndMemory();
	testCacheStatusCursorBinding();
	printf("[PASS] http_cache_status\n");
	return 0;
}
