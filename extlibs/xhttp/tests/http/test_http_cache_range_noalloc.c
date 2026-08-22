#include "../test_allocator.h"

#include <xrt/http_cache_range.h>



/* 验证片段、覆盖和组合协议计划全程不分配内存。 */
int main(void)
{
	static const xhttpfield Fields[] = {
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
	const xhttpbyterange Ranges[] = {
		{ 0, 4 }
	};
	xhttpcachecoverage Coverage = {
		Ranges, 1, 10, true
	};
	xhttpcachefragmentinput Input;
	xhttpcachefragmentplan FragmentPlan;
	xhttpcachefragment Stored;
	xhttpcachecombineplan CombinePlan;
	xhttpcachemissingcursor Cursor;
	xhttpbyterange Target = { 0, 9 };
	xhttpbyterange Missing;
	bool bPass;

	xrtHttpCacheFragmentInputInit(&Input);
	Input.Method = XRT_STR_LITERAL("GET");
	Input.Fields = Fields;
	Input.FieldCount =
		sizeof(Fields) / sizeof(Fields[0]);
	Input.Status = XHTTP_STATUS_PARTIAL_CONTENT;
	Input.BodySize = 5;
	Input.Flags =
		XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE |
		XHTTP_CACHE_FRAGMENT_BODY_COMPLETE;

	memset(&Stored, 0, sizeof(Stored));
	Stored.Entry.Fields = Fields;
	Stored.Entry.FieldCount =
		sizeof(Fields) / sizeof(Fields[0]);
	Stored.Entry.Flags = XHTTP_CACHE_ENTRY_PARTIAL;
	Stored.Range = (xhttpbyterange){ 0, 4 };
	Stored.Length = 10;
	Stored.SourceStatus =
		XHTTP_STATUS_PARTIAL_CONTENT;
	Stored.Flags =
		XHTTP_CACHE_FRAGMENT_HAS_RANGE |
		XHTTP_CACHE_FRAGMENT_HAS_LENGTH;

	testRequire(
		testInstallFailAllocator(),
		"HTTP cache range failure allocator install failed"
	);
	xrtHttpCacheMissingCursorInit(&Cursor);
	bPass =
		(xrtHttpCacheFragmentPlan(
			&Input, &FragmentPlan
		 ) == XHTTP_CACHE_FRAGMENT_STORE) &&
		(xrtHttpCacheCoverageCovers(
			&Coverage, &Ranges[0]
		 ) == XHTTP_CACHE_COVERAGE_HIT) &&
		(xrtHttpCacheMissingNext(
			&Coverage, &Target,
			&Cursor, &Missing
		 ) == XHTTP_NEXT_ITEM) &&
		(xrtHttpCacheCombinePlan(
			&Stored, 1,
			&FragmentPlan.Fragment,
			&CombinePlan
		 ) == XHTTP_CACHE_COMBINE_APPLY);
	testRequire(
		bPass,
		"HTTP cache range protocol path allocated memory"
	);
	return 0;
}
