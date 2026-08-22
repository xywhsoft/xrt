#include "../test.h"

#include <xrt/http_cache_range.h>



/* 使用常用参数构造片段输入。 */
static xhttpcachefragmentinput testHttpCacheFragmentInput(
	uint16 iStatus,
	const xhttpfield* pFields,
	size_t iFieldCount,
	uint64 iBodySize,
	uint32 iFlags
)
{
	xhttpcachefragmentinput Input;

	xrtHttpCacheFragmentInputInit(&Input);
	Input.Method = XRT_STR_LITERAL("GET");
	Input.Status = iStatus;
	Input.Fields = pFields;
	Input.FieldCount = iFieldCount;
	Input.BodySize = iBodySize;
	Input.ResponseTime = 100;
	Input.Flags = iFlags;
	return Input;
}



/* 验证完整与截断 200 被规范化为正确的表示区间。 */
static void testHttpCacheFragment200(void)
{
	static const xhttpfield LengthTen[] = {
		{
			XRT_STR_INIT("Content-Length"),
			XRT_STR_INIT("10")
		}
	};
	xhttpcachefragmentinput Input;
	xhttpcachefragmentplan Plan;

	Input = testHttpCacheFragmentInput(
		XHTTP_STATUS_OK,
		LengthTen,
		1,
		10,
		XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE |
			XHTTP_CACHE_FRAGMENT_BODY_COMPLETE
	);
	testRequire(
		xrtHttpCacheFragmentPlan(
			&Input, &Plan
		) == XHTTP_CACHE_FRAGMENT_STORE,
		"HTTP cache complete 200 fragment plan failed"
	);
	testRequire(
		xrtHttpCacheFragmentComplete(&Plan.Fragment) &&
		(Plan.Fragment.Range.First == 0) &&
		(Plan.Fragment.Range.Last == 9) &&
		(Plan.Fragment.Length == 10) &&
		(Plan.Actions == 0),
		"HTTP cache complete 200 fragment metadata mismatch"
	);

	Input.BodySize = 4;
	Input.Flags = XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE;
	testRequire(
		xrtHttpCacheFragmentPlan(
			&Input, &Plan
		) == XHTTP_CACHE_FRAGMENT_STORE,
		"HTTP cache incomplete 200 fragment plan failed"
	);
	testRequire(
		!xrtHttpCacheFragmentComplete(&Plan.Fragment) &&
		(Plan.Fragment.Entry.Flags ==
		 XHTTP_CACHE_ENTRY_PARTIAL) &&
		(Plan.Fragment.Range.First == 0) &&
		(Plan.Fragment.Range.Last == 3) &&
		(Plan.Fragment.Length == 10) &&
		((Plan.Actions &
		  XHTTP_CACHE_FRAGMENT_MARK_INCOMPLETE) != 0),
		"HTTP cache incomplete 200 fragment metadata mismatch"
	);

	Input.Fields = NULL;
	Input.FieldCount = 0;
	testRequire(
		xrtHttpCacheFragmentPlan(
			&Input, &Plan
		) == XHTTP_CACHE_FRAGMENT_STORE &&
		((Plan.Fragment.Flags &
		  XHTTP_CACHE_FRAGMENT_HAS_LENGTH) == 0),
		"HTTP cache unknown-length incomplete 200 failed"
	);

	Input.BodySize = 0;
	Input.Flags =
		XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE |
		XHTTP_CACHE_FRAGMENT_BODY_COMPLETE;
	testRequire(
		xrtHttpCacheFragmentPlan(
			&Input, &Plan
		) == XHTTP_CACHE_FRAGMENT_STORE &&
		xrtHttpCacheFragmentComplete(&Plan.Fragment) &&
		((Plan.Fragment.Flags &
		  XHTTP_CACHE_FRAGMENT_HAS_RANGE) == 0) &&
		(Plan.Fragment.Length == 0),
		"HTTP cache empty complete 200 failed"
	);
}



/* 验证单段 206 的实际正文区间、完整长度和规范化动作。 */
static void testHttpCacheFragment206(void)
{
	static const xhttpfield Partial[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"asset\"")
		},
		{
			XRT_STR_INIT("Content-Range"),
			XRT_STR_INIT("bytes 5-9/10")
		},
		{
			XRT_STR_INIT("Content-Length"),
			XRT_STR_INIT("5")
		}
	};
	static const xhttpfield Full[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"asset\"")
		},
		{
			XRT_STR_INIT("Content-Range"),
			XRT_STR_INIT("bytes 0-9/10")
		},
		{
			XRT_STR_INIT("Content-Length"),
			XRT_STR_INIT("10")
		}
	};
	xhttpcachefragmentinput Input;
	xhttpcachefragmentplan Plan;

	Input = testHttpCacheFragmentInput(
		XHTTP_STATUS_PARTIAL_CONTENT,
		Partial,
		sizeof(Partial) / sizeof(Partial[0]),
		5,
		XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE |
			XHTTP_CACHE_FRAGMENT_BODY_COMPLETE
	);
	testRequire(
		xrtHttpCacheFragmentPlan(
			&Input, &Plan
		) == XHTTP_CACHE_FRAGMENT_STORE,
		"HTTP cache single 206 fragment plan failed"
	);
	testRequire(
		(Plan.Fragment.Range.First == 5) &&
		(Plan.Fragment.Range.Last == 9) &&
		(Plan.Fragment.Length == 10) &&
		!xrtHttpCacheFragmentComplete(&Plan.Fragment) &&
		((Plan.Actions &
		  XHTTP_CACHE_FRAGMENT_REMOVE_CONTENT_RANGE) != 0) &&
		((Plan.Actions &
		  XHTTP_CACHE_FRAGMENT_REMOVE_CONTENT_LENGTH) != 0),
		"HTTP cache single 206 fragment metadata mismatch"
	);

	Input.BodySize = 2;
	Input.Flags = XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE;
	testRequire(
		xrtHttpCacheFragmentPlan(
			&Input, &Plan
		) == XHTTP_CACHE_FRAGMENT_STORE &&
		(Plan.Fragment.Range.First == 5) &&
		(Plan.Fragment.Range.Last == 6),
		"HTTP cache truncated 206 fragment failed"
	);

	Input = testHttpCacheFragmentInput(
		XHTTP_STATUS_PARTIAL_CONTENT,
		Full,
		sizeof(Full) / sizeof(Full[0]),
		10,
		XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE |
			XHTTP_CACHE_FRAGMENT_BODY_COMPLETE
	);
	testRequire(
		xrtHttpCacheFragmentPlan(
			&Input, &Plan
		) == XHTTP_CACHE_FRAGMENT_STORE &&
		xrtHttpCacheFragmentComplete(&Plan.Fragment) &&
		((Plan.Actions &
		  XHTTP_CACHE_FRAGMENT_SET_CONTENT_LENGTH) != 0),
		"HTTP cache full-range 206 did not become complete"
	);
}



/* 验证 multipart part 使用外层验证器和 part 自己的 Content-Range。 */
static void testHttpCacheMultipartPart(void)
{
	static const xhttpfield Outer[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"asset\"")
		},
		{
			XRT_STR_INIT("Content-Length"),
			XRT_STR_INIT("500")
		}
	};
	static const xhttpfield Part[] = {
		{
			XRT_STR_INIT("Content-Range"),
			XRT_STR_INIT("bytes 10-12/20")
		}
	};
	xhttpcachefragmentinput Input;
	xhttpcachefragmentplan Plan;

	Input = testHttpCacheFragmentInput(
		XHTTP_STATUS_PARTIAL_CONTENT,
		Outer,
		sizeof(Outer) / sizeof(Outer[0]),
		3,
		XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE |
			XHTTP_CACHE_FRAGMENT_BODY_COMPLETE |
			XHTTP_CACHE_FRAGMENT_MULTIPART_PART
	);
	Input.RangeFields = Part;
	Input.RangeFieldCount =
		sizeof(Part) / sizeof(Part[0]);
	testRequire(
		xrtHttpCacheFragmentPlan(
			&Input, &Plan
		) == XHTTP_CACHE_FRAGMENT_STORE &&
		(Plan.Fragment.Range.First == 10) &&
		(Plan.Fragment.Range.Last == 12) &&
		(Plan.Fragment.Length == 20),
		"HTTP cache multipart part plan failed"
	);
}



/* 验证保守跳过不会把传输矛盾当成可存储片段。 */
static void testHttpCacheFragmentSkips(void)
{
	static const xhttpfield InvalidLength[] = {
		{
			XRT_STR_INIT("Content-Length"),
			XRT_STR_INIT("5, 6")
		}
	};
	static const xhttpfield InvalidRange[] = {
		{
			XRT_STR_INIT("Content-Range"),
			XRT_STR_INIT("bytes 0-4/10")
		},
		{
			XRT_STR_INIT("Content-Range"),
			XRT_STR_INIT("bytes 5-9/10")
		}
	};
	xhttpcachefragmentinput Input;
	xhttpcachefragmentplan Plan;

	Input = testHttpCacheFragmentInput(
		XHTTP_STATUS_OK,
		InvalidLength,
		1,
		5,
		XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE |
			XHTTP_CACHE_FRAGMENT_BODY_COMPLETE
	);
	testRequire(
		xrtHttpCacheFragmentPlan(
			&Input, &Plan
		) == XHTTP_CACHE_FRAGMENT_SKIP &&
		((Plan.Reasons &
		  XHTTP_CACHE_FRAGMENT_REASON_CONTENT_LENGTH) != 0),
		"HTTP cache fragment accepted invalid Content-Length"
	);

	Input = testHttpCacheFragmentInput(
		XHTTP_STATUS_PARTIAL_CONTENT,
		InvalidRange,
		2,
		5,
		XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE |
			XHTTP_CACHE_FRAGMENT_BODY_COMPLETE
	);
	testRequire(
		xrtHttpCacheFragmentPlan(
			&Input, &Plan
		) == XHTTP_CACHE_FRAGMENT_SKIP &&
		((Plan.Reasons &
		  XHTTP_CACHE_FRAGMENT_REASON_CONTENT_RANGE) != 0),
		"HTTP cache fragment accepted duplicate Content-Range"
	);

	Input = testHttpCacheFragmentInput(
		XHTTP_STATUS_OK,
		NULL,
		0,
		1,
		XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE |
			XHTTP_CACHE_FRAGMENT_BODY_COMPLETE |
			XHTTP_CACHE_FRAGMENT_TRANSFORMED
	);
	testRequire(
		xrtHttpCacheFragmentPlan(
			&Input, &Plan
		) == XHTTP_CACHE_FRAGMENT_SKIP &&
		((Plan.Reasons &
		  XHTTP_CACHE_FRAGMENT_REASON_TRANSFORMED) != 0),
		"HTTP cache fragment accepted transformed bytes"
	);

	Input.Method = XRT_STR_LITERAL("POST");
	Input.Status = XHTTP_STATUS_NOT_FOUND;
	Input.Flags = 0;
	testRequire(
		xrtHttpCacheFragmentPlan(
			&Input, &Plan
		) == XHTTP_CACHE_FRAGMENT_SKIP &&
		((Plan.Reasons &
		  XHTTP_CACHE_FRAGMENT_REASON_METHOD) != 0) &&
		((Plan.Reasons &
		  XHTTP_CACHE_FRAGMENT_REASON_STATUS) != 0) &&
		((Plan.Reasons &
		  XHTTP_CACHE_FRAGMENT_REASON_HEADERS) != 0),
		"HTTP cache fragment did not report all skip reasons"
	);
}



/* 验证规范覆盖、命中判断和缺口迭代。 */
static void testHttpCacheCoverage(void)
{
	const xhttpbyterange Ranges[] = {
		{ 0, 4 },
		{ 10, 19 },
		{ 25, 29 }
	};
	const xhttpbyterange Adjacent[] = {
		{ 0, 4 },
		{ 5, 9 }
	};
	xhttpcachecoverage Coverage = {
		Ranges,
		sizeof(Ranges) / sizeof(Ranges[0]),
		30,
		true
	};
	xhttpcachecoverage Invalid = {
		Adjacent,
		sizeof(Adjacent) / sizeof(Adjacent[0]),
		10,
		true
	};
	xhttpbyterange Target = { 0, 29 };
	xhttpbyterange Range = { 1, 3 };
	xhttpbyterange Missing;
	xhttpcachemissingcursor Cursor;

	testRequire(
		xrtHttpCacheCoverageValid(&Coverage) &&
		!xrtHttpCacheCoverageValid(&Invalid),
		"HTTP cache coverage canonical validation failed"
	);
	testRequire(
		xrtHttpCacheCoverageCovers(
			&Coverage, &Range
		) == XHTTP_CACHE_COVERAGE_HIT,
		"HTTP cache coverage missed a contained range"
	);
	Range = (xhttpbyterange){ 3, 12 };
	testRequire(
		xrtHttpCacheCoverageCovers(
			&Coverage, &Range
		) == XHTTP_CACHE_COVERAGE_MISS,
		"HTTP cache coverage crossed a gap"
	);

	xrtHttpCacheMissingCursorInit(&Cursor);
	testRequire(
		xrtHttpCacheMissingNext(
			&Coverage, &Target, &Cursor, &Missing
		) == XHTTP_NEXT_ITEM &&
		(Missing.First == 5) &&
		(Missing.Last == 9),
		"HTTP cache first missing range mismatch"
	);
	testRequire(
		xrtHttpCacheMissingNext(
			&Coverage, &Target, &Cursor, &Missing
		) == XHTTP_NEXT_ITEM &&
		(Missing.First == 20) &&
		(Missing.Last == 24),
		"HTTP cache second missing range mismatch"
	);
	testRequire(
		xrtHttpCacheMissingNext(
			&Coverage, &Target, &Cursor, &Missing
		) == XHTTP_NEXT_END,
		"HTTP cache missing iterator did not terminate"
	);
}



/* 验证 uint64 最大位置不会在覆盖和缺口迭代中回绕。 */
static void testHttpCacheCoverageBoundary(void)
{
	const xhttpbyterange Ranges[] = {
		{ UINT64_MAX, UINT64_MAX }
	};
	xhttpcachecoverage Coverage = {
		Ranges,
		1,
		0,
		false
	};
	xhttpbyterange Target = {
		UINT64_MAX,
		UINT64_MAX
	};
	xhttpbyterange Missing = { 1, 1 };
	xhttpcachemissingcursor Cursor;

	testRequire(
		xrtHttpCacheCoverageCovers(
			&Coverage, &Target
		) == XHTTP_CACHE_COVERAGE_HIT,
		"HTTP cache maximum offset coverage failed"
	);
	xrtHttpCacheMissingCursorInit(&Cursor);
	testRequire(
		xrtHttpCacheMissingNext(
			&Coverage, &Target, &Cursor, &Missing
		) == XHTTP_NEXT_END,
		"HTTP cache maximum offset iterator wrapped"
	);
}



/* 执行 HTTP 缓存片段与覆盖集测试。 */
int main(void)
{
	testHttpCacheFragment200();
	testHttpCacheFragment206();
	testHttpCacheMultipartPart();
	testHttpCacheFragmentSkips();
	testHttpCacheCoverage();
	testHttpCacheCoverageBoundary();
	return 0;
}
