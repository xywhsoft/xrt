#include "../test.h"

#include <xrt/http_cache_validate.h>



/* 把 IMF-fixdate 测试文本解析为内部时间。 */
static xtime testHttpCacheMatrixDate(cstr sText)
{
	xtime iTime = 0;

	testRequire(
		xrtTimeParseHTTPDate(
			(xstrview){ sText, strlen(sText) },
			&iTime
		),
		"HTTP cache matrix date setup failed"
	);
	return iTime;
}



/* 构造借用响应字段的缓存条目。 */
static xhttpcacheentry testHttpCacheMatrixEntry(
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



/* 验证完整、部分和已覆盖部分条目的 Range 资格矩阵。 */
static void testHttpCacheRangeMatrix(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"range\"")
		}
	};
	static const struct {
		uint32 Flags;
		bool Range;
		xhttpcachevalidatedecision Decision;
		size_t Eligible;
	} Cases[] = {
		{
			XHTTP_CACHE_ENTRY_NONE,
			false,
			XHTTP_CACHE_VALIDATE_CONDITIONAL,
			1
		},
		{
			XHTTP_CACHE_ENTRY_NONE,
			true,
			XHTTP_CACHE_VALIDATE_CONDITIONAL,
			1
		},
		{
			XHTTP_CACHE_ENTRY_PARTIAL,
			false,
			XHTTP_CACHE_VALIDATE_NONE,
			0
		},
		{
			XHTTP_CACHE_ENTRY_PARTIAL,
			true,
			XHTTP_CACHE_VALIDATE_NONE,
			0
		},
		{
			XHTTP_CACHE_ENTRY_PARTIAL |
				XHTTP_CACHE_ENTRY_RANGE_COVERED,
			false,
			XHTTP_CACHE_VALIDATE_NONE,
			0
		},
		{
			XHTTP_CACHE_ENTRY_PARTIAL |
				XHTTP_CACHE_ENTRY_RANGE_COVERED,
			true,
			XHTTP_CACHE_VALIDATE_CONDITIONAL,
			1
		}
	};
	xhttpcacheentry Entry;
	xhttpcachevalidateplan Plan;
	size_t i;

	for ( i = 0; i < (sizeof(Cases) / sizeof(Cases[0])); i++ ) {
		Entry = testHttpCacheMatrixEntry(
			Fields, 1, 0, Cases[i].Flags
		);
		testRequire(
			(xrtHttpCacheValidatePlan(
				&Entry, 1, Cases[i].Range, &Plan
			 ) == Cases[i].Decision) &&
			(Plan.EligibleCount == Cases[i].Eligible),
			"HTTP cache Range eligibility matrix mismatch"
		);
	}
	Entry.Flags = XHTTP_CACHE_ENTRY_RANGE_COVERED;
	testRequire(
		!xrtHttpCacheEntryValid(&Entry),
		"HTTP cache accepted covered range without partial body"
	);
}



/* 验证重复单值验证器和 Content-Length 的保守处理。 */
static void testHttpCacheSingletonMatrix(void)
{
	static const xhttpfield DuplicateETag[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"same\"")
		},
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"same\"")
		}
	};
	static const xhttpfield DuplicateModified[] = {
		{
			XRT_STR_INIT("Last-Modified"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:35 GMT")
		},
		{
			XRT_STR_INIT("Last-Modified"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:35 GMT")
		}
	};
	static const xhttpfield StoredLength[] = {
		{
			XRT_STR_INIT("Content-Length"),
			XRT_STR_INIT("100")
		}
	};
	static const xhttpfield EqualLength[] = {
		{
			XRT_STR_INIT("Content-Length"),
			XRT_STR_INIT("100")
		},
		{
			XRT_STR_INIT("Content-Length"),
			XRT_STR_INIT("100, 100")
		}
	};
	static const xhttpfield ConflictLength[] = {
		{
			XRT_STR_INIT("Content-Length"),
			XRT_STR_INIT("100")
		},
		{
			XRT_STR_INIT("Content-Length"),
			XRT_STR_INIT("101")
		}
	};
	xhttpcacheentry Entry;
	xhttpcachevalidateplan Plan;

	Entry = testHttpCacheMatrixEntry(
		DuplicateETag, 2, 0, XHTTP_CACHE_ENTRY_NONE
	);
	testRequire(
		xrtHttpCacheValidatePlan(
			&Entry, 1, false, &Plan
		) == XHTTP_CACHE_VALIDATE_NONE,
		"HTTP cache used duplicate ETag"
	);
	Entry = testHttpCacheMatrixEntry(
		DuplicateModified, 2, 0, XHTTP_CACHE_ENTRY_NONE
	);
	testRequire(
		xrtHttpCacheValidatePlan(
			&Entry, 1, false, &Plan
		) == XHTTP_CACHE_VALIDATE_NONE,
		"HTTP cache used duplicate Last-Modified"
	);
	Entry = testHttpCacheMatrixEntry(
		StoredLength, 1, 0, XHTTP_CACHE_ENTRY_NONE
	);
	testRequire(
		xrtHttpCacheHeadPlan(
			200, &Entry, EqualLength, 2
		) == XHTTP_CACHE_HEAD_UPDATE,
		"HTTP cache rejected equal Content-Length values"
	);
	testRequire(
		xrtHttpCacheHeadPlan(
			200, &Entry, ConflictLength, 2
		) == XHTTP_CACHE_HEAD_STALE,
		"HTTP cache accepted conflicting Content-Length values"
	);
}



/* 验证弱 304 使用 Date、接收时间和下标的稳定最新项排序。 */
static void testHttpCacheWeakLatestMatrix(void)
{
	static const xhttpfield Response[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("W/\"same\"")
		}
	};
	static const xhttpfield InvalidDate[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("W/\"same\"")
		},
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("invalid")
		}
	};
	static const xhttpfield MissingDate[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"same\"")
		}
	};
	static const xhttpfield OlderDate[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("W/\"same\"")
		},
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:37 GMT")
		}
	};
	xhttpcacheentry Entries[4];
	size_t Indices[1] = { XRT_NPOS };
	size_t iCount = 0;
	xtime iNewest = testHttpCacheMatrixDate(
		"Sun, 06 Nov 1994 08:49:38 GMT"
	);

	Entries[0] = testHttpCacheMatrixEntry(
		InvalidDate, 2,
		testHttpCacheMatrixDate(
			"Sun, 06 Nov 1994 08:49:36 GMT"
		),
		XHTTP_CACHE_ENTRY_NONE
	);
	Entries[1] = testHttpCacheMatrixEntry(
		MissingDate, 1, iNewest,
		XHTTP_CACHE_ENTRY_NONE
	);
	Entries[2] = testHttpCacheMatrixEntry(
		OlderDate, 2, 0,
		XHTTP_CACHE_ENTRY_NONE
	);
	Entries[3] = testHttpCacheMatrixEntry(
		MissingDate, 1, iNewest,
		XHTTP_CACHE_ENTRY_NONE
	);
	testRequire(
		(xrtHttpCache304Select(
			Response, 1,
			Entries, 4,
			Indices, 1,
			&iCount
		 ) == XHTTP_CACHE_UPDATE_MATCH_WEAK) &&
		(iCount == 1) &&
		(Indices[0] == 3),
		"HTTP cache weak latest fallback matrix mismatch"
	);
}



/* 验证 HEAD 对收到的每一种元数据执行逐项匹配。 */
static void testHttpCacheHeadMatrix(void)
{
	static const xhttpfield Stored[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("W/\"asset\"")
		},
		{
			XRT_STR_INIT("Last-Modified"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:35 GMT")
		},
		{
			XRT_STR_INIT("Content-Length"),
			XRT_STR_INIT("100")
		}
	};
	static const xhttpfield ETagChanged[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"asset\"")
		}
	};
	static const xhttpfield ModifiedChanged[] = {
		{
			XRT_STR_INIT("Last-Modified"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:36 GMT")
		}
	};
	xhttpcacheentry Entry = testHttpCacheMatrixEntry(
		Stored, 3, 0, XHTTP_CACHE_ENTRY_NONE
	);

	testRequire(
		xrtHttpCacheHeadPlan(
			200, &Entry, NULL, 0
		) == XHTTP_CACHE_HEAD_UPDATE,
		"HTTP cache HEAD without metadata did not update"
	);
	testRequire(
		xrtHttpCacheHeadPlan(
			200, &Entry, ETagChanged, 1
		) == XHTTP_CACHE_HEAD_STALE,
		"HTTP cache HEAD accepted changed ETag strength"
	);
	testRequire(
		xrtHttpCacheHeadPlan(
			200, &Entry, ModifiedChanged, 1
		) == XHTTP_CACHE_HEAD_STALE,
		"HTTP cache HEAD accepted changed Last-Modified"
	);
	testRequire(
		xrtHttpCacheHeadPlan(
			304, &Entry, NULL, 0
		) == XHTTP_CACHE_HEAD_IGNORE,
		"HTTP cache HEAD handled a non-200 response"
	);
}



/* 验证 safe、unsafe 和未知方法的响应状态失效边界。 */
static void testHttpCacheInvalidationMatrix(void)
{
	static const xstrview Methods[] = {
		XRT_STR_INIT("GET"),
		XRT_STR_INIT("HEAD"),
		XRT_STR_INIT("POST"),
		XRT_STR_INIT("UNKNOWN")
	};
	static const uint16 Statuses[] = {
		199, 200, 299, 300, 399, 400, 599
	};
	const xstrview Target =
		XRT_STR_LITERAL("https://example.test/resource");
	xhttpcacheinvalidatecursor Cursor;
	xhttpcacheinvalidateitem Item;
	xhttpnext Next;
	bool bExpected;
	size_t iMethod;
	size_t iStatus;

	for ( iMethod = 0;
		iMethod < (sizeof(Methods) / sizeof(Methods[0]));
		iMethod++ ) {
		for ( iStatus = 0;
			iStatus < (sizeof(Statuses) / sizeof(Statuses[0]));
			iStatus++ ) {
			xrtHttpCacheInvalidationCursorInit(&Cursor);
			Next = xrtHttpCacheInvalidationNext(
				Methods[iMethod],
				Statuses[iStatus],
				Target,
				NULL, 0,
				&Cursor,
				&Item
			);
			bExpected = (iMethod >= 2u) &&
				(Statuses[iStatus] >= 200u) &&
				(Statuses[iStatus] < 400u);
			testRequire(
				Next == (bExpected ?
					XHTTP_NEXT_ITEM :
					XHTTP_NEXT_END),
				"HTTP cache invalidation status matrix mismatch"
			);
		}
	}
}



/* 执行 HTTP 缓存验证协议矩阵。 */
int main(void)
{
	testHttpCacheRangeMatrix();
	testHttpCacheSingletonMatrix();
	testHttpCacheWeakLatestMatrix();
	testHttpCacheHeadMatrix();
	testHttpCacheInvalidationMatrix();
	printf("[PASS] http_cache_validate_matrix\n");
	return 0;
}
