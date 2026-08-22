#include "../test.h"

#include <xrt/http_cache_validate.h>



/* 把 IMF-fixdate 测试文本解析为内部时间。 */
static xtime testHttpCacheValidateDate(cstr sText)
{
	xtime iTime = 0;

	testRequire(
		xrtTimeParseHTTPDate(
			(xstrview){ sText, strlen(sText) },
			&iTime
		),
		"HTTP cache validation date setup failed"
	);
	return iTime;
}



/* 构造借用响应字段的完整缓存条目。 */
static xhttpcacheentry testHttpCacheValidateEntry(
	const xhttpfield* pFields,
	size_t iCount,
	xtime iResponseTime,
	uint32 iFlags
)
{
	xhttpcacheentry Entry;

	Entry.Fields = pFields;
	Entry.FieldCount = iCount;
	Entry.ResponseTime = iResponseTime;
	Entry.Flags = iFlags;
	return Entry;
}



/* 验证多条 ETag 去重、日期回退和 Range 部分条目过滤。 */
static void testHttpCacheValidateRequest(void)
{
	static const xhttpfield OneFields[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"one\"")
		},
		{
			XRT_STR_INIT("Last-Modified"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:35 GMT")
		},
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:37 GMT")
		}
	};
	static const xhttpfield TwoFields[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("W/\"two\"")
		}
	};
	static const xhttpfield DuplicateFields[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("W/\"one\"")
		}
	};
	static const xhttpfield EmptyFields[] = {
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:37 GMT")
		}
	};
	xhttpcacheentry Entries[3];
	xhttpcachevalidateplan Plan;
	char Output[64];
	size_t iSize = 0;

	Entries[0] = testHttpCacheValidateEntry(
		OneFields,
		sizeof(OneFields) / sizeof(OneFields[0]),
		0,
		XHTTP_CACHE_ENTRY_NONE
	);
	Entries[1] = testHttpCacheValidateEntry(
		TwoFields,
		sizeof(TwoFields) / sizeof(TwoFields[0]),
		0,
		XHTTP_CACHE_ENTRY_NONE
	);
	Entries[2] = testHttpCacheValidateEntry(
		DuplicateFields,
		sizeof(DuplicateFields) /
			sizeof(DuplicateFields[0]),
		0,
		XHTTP_CACHE_ENTRY_NONE
	);
	testRequire(
		(xrtHttpCacheValidatePlan(
			Entries, 3, false, &Plan
		 ) == XHTTP_CACHE_VALIDATE_CONDITIONAL) &&
		(Plan.EligibleCount == 3) &&
		(Plan.ETagCount == 2) &&
		(Plan.ETagSize == 14) &&
		(Plan.Actions ==
		 XHTTP_CACHE_VALIDATE_IF_NONE_MATCH),
		"HTTP cache validation multi-entry plan mismatch"
	);
	testRequire(
		xrtHttpCacheValidateETagsWrite(
			Entries, 3, false,
			NULL, 0, &iSize
		) && (iSize == 14) &&
		xrtHttpCacheValidateETagsWrite(
			Entries, 3, false,
			Output, sizeof(Output), &iSize
		) && (iSize == 14) &&
		(memcmp(
			Output, "\"one\", W/\"two\"", 14
		) == 0),
		"HTTP cache validation ETag union mismatch"
	);

	/* 单条完整表示同时发送 ETag 和 Last-Modified。 */
	testRequire(
		(xrtHttpCacheValidatePlan(
			Entries, 1, false, &Plan
		 ) == XHTTP_CACHE_VALIDATE_CONDITIONAL) &&
		((Plan.Actions & (
			XHTTP_CACHE_VALIDATE_IF_NONE_MATCH |
			XHTTP_CACHE_VALIDATE_IF_MODIFIED_SINCE
		 )) == (
			XHTTP_CACHE_VALIDATE_IF_NONE_MATCH |
			XHTTP_CACHE_VALIDATE_IF_MODIFIED_SINCE
		 )) &&
		(Plan.LastModified ==
		 testHttpCacheValidateDate(
			"Sun, 06 Nov 1994 08:49:35 GMT"
		 )),
		"HTTP cache validation single-entry validators mismatch"
	);

	/* 未覆盖当前 Range 的部分条目不能提名其 ETag。 */
	Entries[0].Flags = XHTTP_CACHE_ENTRY_PARTIAL;
	Entries[1].Flags = XHTTP_CACHE_ENTRY_PARTIAL |
		XHTTP_CACHE_ENTRY_RANGE_COVERED;
	testRequire(
		(xrtHttpCacheValidatePlan(
			Entries, 2, true, &Plan
		 ) == XHTTP_CACHE_VALIDATE_CONDITIONAL) &&
		(Plan.EligibleCount == 1) &&
		(Plan.ETagCount == 1) &&
		((Plan.Actions &
		  XHTTP_CACHE_VALIDATE_IF_MODIFIED_SINCE) == 0),
		"HTTP cache validation Range eligibility mismatch"
	);
	Entries[0] = testHttpCacheValidateEntry(
		EmptyFields,
		sizeof(EmptyFields) / sizeof(EmptyFields[0]),
		0,
		XHTTP_CACHE_ENTRY_NONE
	);
	testRequire(
		xrtHttpCacheValidatePlan(
			Entries, 1, false, &Plan
		) == XHTTP_CACHE_VALIDATE_NONE,
		"HTTP cache validation invented a validator"
	);
}



/* 验证 If-Range 只使用强 ETag 或已经证明为强的修改日期。 */
static void testHttpCacheValidateIfRange(void)
{
	static const xhttpfield StrongFields[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"strong\"")
		}
	};
	static const xhttpfield WeakFields[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("W/\"weak\"")
		},
		{
			XRT_STR_INIT("Last-Modified"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:35 GMT")
		},
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:37 GMT")
		}
	};
	static const xhttpfield DateFields[] = {
		{
			XRT_STR_INIT("Last-Modified"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:35 GMT")
		},
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:36 GMT")
		}
	};
	static const xhttpfield WeakDateFields[] = {
		{
			XRT_STR_INIT("Last-Modified"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:35 GMT")
		},
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:35 GMT")
		}
	};
	xhttpcacheentry Entry;
	xhttpcacheifrange Plan;

	Entry = testHttpCacheValidateEntry(
		StrongFields,
		sizeof(StrongFields) / sizeof(StrongFields[0]),
		0,
		XHTTP_CACHE_ENTRY_NONE
	);
	testRequire(
		(xrtHttpCacheIfRangePlan(
			&Entry, &Plan
		 ) == XHTTP_CACHE_IF_RANGE_ETAG) &&
		!Plan.ETag.Weak,
		"HTTP cache If-Range strong ETag mismatch"
	);
	Entry = testHttpCacheValidateEntry(
		WeakFields,
		sizeof(WeakFields) / sizeof(WeakFields[0]),
		0,
		XHTTP_CACHE_ENTRY_NONE
	);
	testRequire(
		xrtHttpCacheIfRangePlan(
			&Entry, &Plan
		) == XHTTP_CACHE_IF_RANGE_NONE,
		"HTTP cache If-Range fell back past a weak ETag"
	);
	Entry = testHttpCacheValidateEntry(
		DateFields,
		sizeof(DateFields) / sizeof(DateFields[0]),
		0,
		XHTTP_CACHE_ENTRY_NONE
	);
	testRequire(
		xrtHttpCacheIfRangePlan(
			&Entry, &Plan
		) == XHTTP_CACHE_IF_RANGE_DATE,
		"HTTP cache If-Range strong date was not selected"
	);
	Entry = testHttpCacheValidateEntry(
		WeakDateFields,
		sizeof(WeakDateFields) /
			sizeof(WeakDateFields[0]),
		0,
		XHTTP_CACHE_ENTRY_NONE
	);
	testRequire(
		xrtHttpCacheIfRangePlan(
			&Entry, &Plan
		) == XHTTP_CACHE_IF_RANGE_NONE,
		"HTTP cache If-Range accepted a weak date"
	);
}



/* 验证缓存只评估适用于自身表示的两个客户端条件字段。 */
static void testHttpCacheValidatePreconditions(void)
{
	static const xhttpfield StoredFields[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"asset\"")
		},
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:37 GMT")
		}
	};
	static const xhttpfield IfNoneMatch[] = {
		{
			XRT_STR_INIT("If-None-Match"),
			XRT_STR_INIT("W/\"asset\"")
		}
	};
	static const xhttpfield IfModifiedSince[] = {
		{
			XRT_STR_INIT("If-Modified-Since"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:37 GMT")
		}
	};
	static const xhttpfield OriginOnly[] = {
		{
			XRT_STR_INIT("If-Match"),
			XRT_STR_INIT("\"other\"")
		},
		{
			XRT_STR_INIT("If-Unmodified-Since"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:00 GMT")
		}
	};
	static const xhttpfield Invalid[] = {
		{
			XRT_STR_INIT("If-None-Match"),
			XRT_STR_INIT("\"unterminated")
		}
	};
	xhttpcacheentry Entry = testHttpCacheValidateEntry(
		StoredFields,
		sizeof(StoredFields) / sizeof(StoredFields[0]),
		0,
		XHTTP_CACHE_ENTRY_NONE
	);

	testRequire(
		xrtHttpCachePreconditionsEvaluate(
			XRT_STR_LITERAL("GET"),
			IfNoneMatch,
			1,
			&Entry
		) == XHTTP_PRECONDITION_NOT_MODIFIED,
		"HTTP cache weak If-None-Match did not match"
	);
	testRequire(
		xrtHttpCachePreconditionsEvaluate(
			XRT_STR_LITERAL("HEAD"),
			IfModifiedSince,
			1,
			&Entry
		) == XHTTP_PRECONDITION_NOT_MODIFIED,
		"HTTP cache If-Modified-Since Date fallback failed"
	);
	testRequire(
		xrtHttpCachePreconditionsEvaluate(
			XRT_STR_LITERAL("GET"),
			OriginOnly,
			2,
			&Entry
		) == XHTTP_PRECONDITION_PROCEED,
		"HTTP cache evaluated origin-only preconditions"
	);
	testRequire(
		xrtHttpCachePreconditionsEvaluate(
			XRT_STR_LITERAL("POST"),
			IfNoneMatch,
			1,
			&Entry
		) == XHTTP_PRECONDITION_PROCEED,
		"HTTP cache evaluated a non-retrieval condition"
	);
	testRequire(
		xrtHttpCachePreconditionsEvaluate(
			XRT_STR_LITERAL("GET"),
			Invalid,
			1,
			&Entry
		) == XHTTP_PRECONDITION_ERROR,
		"HTTP cache accepted malformed If-None-Match"
	);
	xrtClearError();
}



/* 验证最终响应分类和缓存条目结构边界。 */
static void testHttpCacheValidateEdges(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"alias\"")
		}
	};
	union {
		size_t Size;
		char Bytes[64];
	} Aliased;
	unsigned char BeforeAlias[sizeof(Aliased)];
	xhttpcacheentry Entry;
	xhttpcachevalidateplan Before;
	xhttpcachevalidateplan Plan;

	memset(&Entry, 0, sizeof(Entry));
	testRequire(
		xrtHttpCacheEntryValid(&Entry),
		"empty HTTP cache entry was rejected"
	);
	Entry.Flags = XHTTP_CACHE_ENTRY_RANGE_COVERED;
	testRequire(
		!xrtHttpCacheEntryValid(&Entry),
		"covered Range without partial body was accepted"
	);
	testRequire(
		xrtHttpCacheValidateResult(
			304, false
		) == XHTTP_CACHE_VALIDATE_RESULT_NOT_MODIFIED,
		"HTTP cache 304 classification mismatch"
	);
	testRequire(
		xrtHttpCacheValidateResult(
			503, true
		) == XHTTP_CACHE_VALIDATE_RESULT_SERVER_FAILURE,
		"HTTP cache 5xx failure classification mismatch"
	);
	testRequire(
		xrtHttpCacheValidateResult(
			503, false
		) == XHTTP_CACHE_VALIDATE_RESULT_FULL,
		"HTTP cache forwarded 5xx classification mismatch"
	);
	memset(&Before, 0x5A, sizeof(Before));
	Plan = Before;
	testRequire(
		(xrtHttpCacheValidatePlan(
			NULL, 1, false, &Plan
		 ) == XHTTP_CACHE_VALIDATE_ERROR) &&
		(memcmp(&Plan, &Before, sizeof(Plan)) == 0),
		"HTTP cache invalid plan input changed output"
	);
	xrtClearError();

	Entry.Fields = Fields;
	Entry.FieldCount = 1;
	Entry.ResponseTime = 0;
	Entry.Flags = XHTTP_CACHE_ENTRY_NONE;
	memset(&Aliased, 0x5A, sizeof(Aliased));
	memcpy(BeforeAlias, &Aliased, sizeof(Aliased));
	testRequire(
		!xrtHttpCacheValidateETagsWrite(
			&Entry, 1, false,
			Aliased.Bytes, sizeof(Aliased.Bytes),
			&Aliased.Size
		) && (memcmp(
			&Aliased, BeforeAlias, sizeof(Aliased)
		) == 0),
		"HTTP cache ETag alias changed output"
	);
	xrtClearError();
}



/* 执行 HTTP 缓存验证请求与条件求值测试。 */
int main(void)
{
	testHttpCacheValidateRequest();
	testHttpCacheValidateIfRange();
	testHttpCacheValidatePreconditions();
	testHttpCacheValidateEdges();
	printf("[PASS] http_cache_validate\n");
	return 0;
}
