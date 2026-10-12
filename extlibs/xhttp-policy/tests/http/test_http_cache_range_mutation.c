#include "../test.h"

#include <xrt/http_cache_range.h>



/* 验证片段计划拒绝结构与输出别名并保持输出不变。 */
static void testHttpCacheFragmentAliases(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Content-Length"),
			XRT_STR_INIT("4")
		}
	};
	union {
		xhttpcachefragmentinput Input;
		xhttpcachefragmentplan Plan;
	} Alias;
	xhttpcachefragmentplan Output;
	xhttpcachefragmentplan Before;

	memset(&Alias, 0, sizeof(Alias));
	xrtHttpCacheFragmentInputInit(&Alias.Input);
	Alias.Input.Method = XRT_STR_LITERAL("GET");
	Alias.Input.Fields = Fields;
	Alias.Input.FieldCount = 1;
	Alias.Input.Status = XHTTP_STATUS_OK;
	Alias.Input.BodySize = 4;
	Alias.Input.Flags =
		XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE |
		XHTTP_CACHE_FRAGMENT_BODY_COMPLETE;
	testRequire(
		xrtHttpCacheFragmentPlan(
			&Alias.Input, &Alias.Plan
		) == XHTTP_CACHE_FRAGMENT_ERROR,
		"HTTP cache fragment accepted input/output alias"
	);

	memset(&Output, 0x5a, sizeof(Output));
	Before = Output;
	Alias.Input.Flags = UINT32_MAX;
	testRequire(
		xrtHttpCacheFragmentPlan(
			&Alias.Input, &Output
		) == XHTTP_CACHE_FRAGMENT_ERROR &&
		(memcmp(
			&Output, &Before, sizeof(Output)
		 ) == 0),
		"HTTP cache fragment error changed output"
	);
}



/* 验证覆盖集和缺口迭代拒绝非法结构及输出别名。 */
static void testHttpCacheCoverageAliases(void)
{
	xhttpbyterange Ranges[] = {
		{ 0, 4 },
		{ 10, 14 }
	};
	xhttpcachecoverage Coverage = {
		Ranges,
		2,
		20,
		true
	};
	xhttpbyterange Target = { 0, 19 };
	xhttpbyterange Missing;
	xhttpbyterange RangeBefore[2];
	xhttpcachecoverage CoverageBefore;
	xhttpbyterange TargetBefore;
	xhttpcachemissingcursor Cursor;

	xrtHttpCacheMissingCursorInit(&Cursor);
	testRequire(
		xrtHttpCacheMissingNext(
			&Coverage,
			&Target,
			&Cursor,
			(xhttpbyterange*)&Cursor
		) == XHTTP_NEXT_ERROR,
		"HTTP cache missing iterator accepted cursor alias"
	);
	xrtHttpCacheMissingCursorInit(&Cursor);
	testRequire(
		xrtHttpCacheMissingNext(
			&Coverage,
			&Target,
			&Cursor,
			&Ranges[0]
		) == XHTTP_NEXT_ERROR,
		"HTTP cache missing iterator accepted range alias"
	);

	CoverageBefore = Coverage;
	TargetBefore = Target;
	memcpy(RangeBefore, Ranges, sizeof(Ranges));
	testRequire(
		xrtHttpCacheMissingNext(
			&Coverage,
			&Target,
			(xhttpcachemissingcursor*)&Coverage,
			&Missing
		) == XHTTP_NEXT_ERROR &&
		(memcmp(
			&Coverage, &CoverageBefore,
			sizeof(Coverage)
		 ) == 0),
		"HTTP cache missing iterator accepted coverage/cursor alias"
	);
	testRequire(
		xrtHttpCacheMissingNext(
			&Coverage,
			&Target,
			(xhttpcachemissingcursor*)&Ranges[0],
			&Missing
		) == XHTTP_NEXT_ERROR &&
		(memcmp(
			Ranges, RangeBefore, sizeof(Ranges)
		 ) == 0),
		"HTTP cache missing iterator accepted ranges/cursor alias"
	);
	testRequire(
		xrtHttpCacheMissingNext(
			&Coverage,
			&Target,
			(xhttpcachemissingcursor*)&Target,
			&Missing
		) == XHTTP_NEXT_ERROR &&
		(memcmp(
			&Target, &TargetBefore, sizeof(Target)
		 ) == 0),
		"HTTP cache missing iterator accepted target/cursor alias"
	);

	Ranges[1] = (xhttpbyterange){ 4, 8 };
	testRequire(
		!xrtHttpCacheCoverageValid(&Coverage),
		"HTTP cache coverage accepted overlapping ranges"
	);
}



/* 验证组合计划拒绝输入别名、非规范数组和输出覆盖。 */
static void testHttpCacheCombineAliases(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"asset\"")
		}
	};
	xhttpcachefragment Stored[2];
	xhttpcachefragment Incoming;
	xhttpcachecombineplan Output;
	xhttpcachecombineplan Before;

	memset(Stored, 0, sizeof(Stored));
	Stored[0].Entry = (xhttpcacheentry){
		Fields, 1, 1, XHTTP_CACHE_ENTRY_PARTIAL
	};
	Stored[0].Range = (xhttpbyterange){ 0, 4 };
	Stored[0].Length = 20;
	Stored[0].SourceStatus =
		XHTTP_STATUS_PARTIAL_CONTENT;
	Stored[0].Flags =
		XHTTP_CACHE_FRAGMENT_HAS_RANGE |
		XHTTP_CACHE_FRAGMENT_HAS_LENGTH;
	Stored[1] = Stored[0];
	Stored[1].Range = (xhttpbyterange){ 10, 14 };
	Stored[1].Entry.ResponseTime = 2;
	Incoming = Stored[0];
	Incoming.Range = (xhttpbyterange){ 5, 9 };
	Incoming.Entry.ResponseTime = 3;

	testRequire(
		xrtHttpCacheCombinePlan(
			Stored,
			2,
			&Stored[0],
			&Output
		) == XHTTP_CACHE_COMBINE_ERROR,
		"HTTP cache combine accepted overlapping inputs"
	);
	testRequire(
		xrtHttpCacheCombinePlan(
			Stored,
			2,
			&Incoming,
			(xhttpcachecombineplan*)&Stored[0]
		) == XHTTP_CACHE_COMBINE_ERROR,
		"HTTP cache combine accepted output alias"
	);

	memset(&Output, 0xa5, sizeof(Output));
	Before = Output;
	Stored[1].Range = (xhttpbyterange){ 5, 9 };
	testRequire(
		xrtHttpCacheCombinePlan(
			Stored, 2, &Incoming, &Output
		) == XHTTP_CACHE_COMBINE_ERROR &&
		(memcmp(
			&Output, &Before, sizeof(Output)
		 ) == 0),
		"HTTP cache combine error changed output"
	);
}



/* 验证协议性 Content-Range 拒绝不会污染线程错误。 */
static void testHttpCacheProtocolSkipError(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Content-Range"),
			XRT_STR_INIT("bytes 5-3/10")
		}
	};
	xhttpcachefragmentinput Input;
	xhttpcachefragmentplan Plan;

	xrtHttpCacheFragmentInputInit(&Input);
	Input.Method = XRT_STR_LITERAL("GET");
	Input.Fields = Fields;
	Input.FieldCount = 1;
	Input.Status = XHTTP_STATUS_PARTIAL_CONTENT;
	Input.BodySize = 3;
	Input.Flags =
		XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE |
		XHTTP_CACHE_FRAGMENT_BODY_COMPLETE;
	xrtClearError();
	testRequire(
		xrtHttpCacheFragmentPlan(
			&Input, &Plan
		) == XHTTP_CACHE_FRAGMENT_SKIP &&
		(Plan.Reasons ==
		 XHTTP_CACHE_FRAGMENT_REASON_CONTENT_RANGE) &&
		(xrtGetError() == NULL),
		"HTTP cache protocol skip leaked a thread error"
	);
}



/* 验证随机畸形片段只会被拒绝，不会产生越界区间。 */
static void testHttpCacheFragmentMutation(void)
{
	xhttpcachefragment Fragment;
	uint32 i;

	memset(&Fragment, 0, sizeof(Fragment));
	for ( i = 0; i < 4096; i++ ) {
		Fragment.SourceStatus = (uint16)i;
		Fragment.Flags = i;
		Fragment.Entry.Flags = i << 8u;
		Fragment.Range.First =
			(uint64)i * UINT64_C(0x100000001);
		Fragment.Range.Last =
			~Fragment.Range.First;
		if ( xrtHttpCacheFragmentValid(&Fragment) ) {
			testRequire(
				xrtHttpCacheFragmentComplete(
					&Fragment
				),
				"HTTP cache mutation published an invalid partial"
			);
		}
	}
}



/* 验证来源状态不能声明协议上不可能出现的覆盖形状。 */
static void testHttpCacheFragmentSourceShape(void)
{
	xhttpcachefragment Fragment;

	memset(&Fragment, 0, sizeof(Fragment));
	Fragment.Entry.Flags = XHTTP_CACHE_ENTRY_PARTIAL;
	Fragment.Range = (xhttpbyterange){ 1, 4 };
	Fragment.SourceStatus = XHTTP_STATUS_OK;
	Fragment.Flags = XHTTP_CACHE_FRAGMENT_HAS_RANGE;
	testRequire(
		!xrtHttpCacheFragmentValid(&Fragment),
		"HTTP cache accepted a non-prefix incomplete 200"
	);

	memset(&Fragment, 0, sizeof(Fragment));
	Fragment.Length = 0;
	Fragment.SourceStatus =
		XHTTP_STATUS_PARTIAL_CONTENT;
	Fragment.Flags = XHTTP_CACHE_FRAGMENT_HAS_LENGTH;
	testRequire(
		!xrtHttpCacheFragmentValid(&Fragment),
		"HTTP cache accepted an empty 206 without a range"
	);
}



/* 执行 HTTP 缓存 Range 结构与别名测试。 */
int main(void)
{
	testHttpCacheFragmentAliases();
	testHttpCacheCoverageAliases();
	testHttpCacheCombineAliases();
	testHttpCacheProtocolSkipError();
	testHttpCacheFragmentMutation();
	testHttpCacheFragmentSourceShape();
	return 0;
}
