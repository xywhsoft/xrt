#include "../test.h"

#include <xrt/http_cache_range.h>



/* 单个片段矩阵项描述状态、完整性和预期决定。 */
typedef struct test_http_cache_fragment_case {
	uint16 Status;
	uint64 BodySize;
	uint32 Flags;
	xhttpcachefragmentdecision Decision;
	uint32 Reason;
} test_http_cache_fragment_case;



/* 验证片段分类矩阵不会随组合实现变化而漂移。 */
static void testHttpCacheFragmentMatrix(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Content-Range"),
			XRT_STR_INIT("bytes 0-4/10")
		},
		{
			XRT_STR_INIT("Content-Length"),
			XRT_STR_INIT("5")
		}
	};
	static const test_http_cache_fragment_case Cases[] = {
		{
			XHTTP_STATUS_OK,
			5,
			XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE |
				XHTTP_CACHE_FRAGMENT_BODY_COMPLETE,
			XHTTP_CACHE_FRAGMENT_STORE,
			0
		},
		{
			XHTTP_STATUS_OK,
			4,
			XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE,
			XHTTP_CACHE_FRAGMENT_STORE,
			0
		},
		{
			XHTTP_STATUS_OK,
			0,
			XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE,
			XHTTP_CACHE_FRAGMENT_SKIP,
			XHTTP_CACHE_FRAGMENT_REASON_EMPTY
		},
		{
			XHTTP_STATUS_PARTIAL_CONTENT,
			5,
			XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE |
				XHTTP_CACHE_FRAGMENT_BODY_COMPLETE,
			XHTTP_CACHE_FRAGMENT_STORE,
			0
		},
		{
			XHTTP_STATUS_PARTIAL_CONTENT,
			4,
			XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE,
			XHTTP_CACHE_FRAGMENT_STORE,
			0
		},
		{
			XHTTP_STATUS_PARTIAL_CONTENT,
			4,
			XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE |
				XHTTP_CACHE_FRAGMENT_BODY_COMPLETE,
			XHTTP_CACHE_FRAGMENT_SKIP,
			XHTTP_CACHE_FRAGMENT_REASON_BODY_LENGTH
		},
		{
			XHTTP_STATUS_NOT_FOUND,
			5,
			XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE |
				XHTTP_CACHE_FRAGMENT_BODY_COMPLETE,
			XHTTP_CACHE_FRAGMENT_SKIP,
			XHTTP_CACHE_FRAGMENT_REASON_STATUS
		}
	};
	size_t i;

	for ( i = 0; i < (sizeof(Cases) / sizeof(Cases[0])); i++ ) {
		xhttpcachefragmentinput Input;
		xhttpcachefragmentplan Plan;

		xrtHttpCacheFragmentInputInit(&Input);
		Input.Method = XRT_STR_LITERAL("GET");
		Input.Fields = Fields;
		Input.FieldCount =
			sizeof(Fields) / sizeof(Fields[0]);
		Input.Status = Cases[i].Status;
		Input.BodySize = Cases[i].BodySize;
		Input.Flags = Cases[i].Flags;
		testRequire(
			xrtHttpCacheFragmentPlan(
				&Input, &Plan
			) == Cases[i].Decision,
			"HTTP cache fragment matrix decision mismatch"
		);
		testRequire(
			(Cases[i].Reason == 0) ||
			((Plan.Reasons & Cases[i].Reason) != 0),
			"HTTP cache fragment matrix reason mismatch"
		);
	}
}



/* 验证各种区间关系映射到稳定的替换窗口。 */
static void testHttpCacheCombineMatrix(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"asset\"")
		}
	};
	static const xhttpbyterange IncomingRanges[] = {
		{ 0, 1 },
		{ 2, 4 },
		{ 5, 9 },
		{ 7, 12 },
		{ 16, 19 }
	};
	static const size_t Indices[] = {
		0, 0, 0, 1, 2
	};
	static const size_t Removes[] = {
		1, 1, 2, 1, 0
	};
	xhttpcachefragment Stored[2];
	size_t i;

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

	for ( i = 0; i < (
		sizeof(IncomingRanges) /
		sizeof(IncomingRanges[0])
	); i++ ) {
		xhttpcachefragment Incoming = Stored[0];
		xhttpcachecombineplan Plan;

		Incoming.Range = IncomingRanges[i];
		Incoming.Entry.ResponseTime = 3;
		testRequire(
			xrtHttpCacheCombinePlan(
				Stored, 2, &Incoming, &Plan
			) == XHTTP_CACHE_COMBINE_APPLY,
			"HTTP cache combine matrix decision mismatch"
		);
		testRequire(
			(Plan.Index == Indices[i]) &&
			(Plan.RemoveCount == Removes[i]),
			"HTTP cache combine matrix window mismatch"
		);
	}
}



/* 执行 HTTP 缓存 Range 协议矩阵。 */
int main(void)
{
	testHttpCacheFragmentMatrix();
	testHttpCacheCombineMatrix();
	return 0;
}
