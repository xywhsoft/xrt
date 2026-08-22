#include "../test.h"

#include <xrt/http_cache_range.h>



/* 构造借用 Header 的缓存片段。 */
static xhttpcachefragment testHttpCacheCombineFragment(
	const xhttpfield* pFields,
	size_t iFieldCount,
	uint64 iFirst,
	uint64 iLast,
	uint64 iLength,
	xtime iResponseTime,
	uint16 iSourceStatus
)
{
	xhttpcachefragment Fragment;

	memset(&Fragment, 0, sizeof(Fragment));
	Fragment.Entry.Fields = pFields;
	Fragment.Entry.FieldCount = iFieldCount;
	Fragment.Entry.ResponseTime = iResponseTime;
	Fragment.Entry.Flags = XHTTP_CACHE_ENTRY_PARTIAL;
	Fragment.Range.First = iFirst;
	Fragment.Range.Last = iLast;
	Fragment.Length = iLength;
	Fragment.SourceStatus = iSourceStatus;
	Fragment.Flags =
		XHTTP_CACHE_FRAGMENT_HAS_RANGE |
		XHTTP_CACHE_FRAGMENT_HAS_LENGTH;
	return Fragment;
}



/* 验证相邻区间合并为完整 200，并用新响应更新已有 200 Header。 */
static void testHttpCacheCombineAdjacent(void)
{
	static const xhttpfield StoredFields[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"asset\"")
		},
		{
			XRT_STR_INIT("Content-Length"),
			XRT_STR_INIT("10")
		}
	};
	static const xhttpfield IncomingFields[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"asset\"")
		},
		{
			XRT_STR_INIT("Content-Range"),
			XRT_STR_INIT("bytes 5-9/10")
		}
	};
	xhttpcachefragment Stored =
		testHttpCacheCombineFragment(
			StoredFields,
			sizeof(StoredFields) /
				sizeof(StoredFields[0]),
			0, 4, 10, 10,
			XHTTP_STATUS_OK
		);
	xhttpcachefragment Incoming =
		testHttpCacheCombineFragment(
			IncomingFields,
			sizeof(IncomingFields) /
				sizeof(IncomingFields[0]),
			5, 9, 10, 20,
			XHTTP_STATUS_PARTIAL_CONTENT
		);
	xhttpcachecombineplan Plan;

	testRequire(
		xrtHttpCacheCombinePlan(
			&Stored, 1, &Incoming, &Plan
		) == XHTTP_CACHE_COMBINE_APPLY,
		"HTTP cache adjacent combine plan failed"
	);
	testRequire(
		(Plan.Index == 0) &&
		(Plan.RemoveCount == 1) &&
		(Plan.ResultCount == 1) &&
		(Plan.Range.First == 0) &&
		(Plan.Range.Last == 9) &&
		Plan.Complete &&
		(Plan.HeaderIndex == 0) &&
		((Plan.Actions &
		  XHTTP_CACHE_COMBINE_UPDATE_INCOMING_FIELDS) != 0) &&
		((Plan.Actions &
		  XHTTP_CACHE_COMBINE_SET_CONTENT_LENGTH) != 0),
		"HTTP cache adjacent combine metadata mismatch"
	);
}



/* 验证桥接多个区间和保留未覆盖缺口。 */
static void testHttpCacheCombineWindows(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"asset\"")
		},
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:37 GMT")
		}
	};
	xhttpcachefragment Stored[2];
	xhttpcachefragment Incoming;
	xhttpcachecombineplan Plan;

	Stored[0] = testHttpCacheCombineFragment(
		Fields, 2, 0, 4, 20, 10,
		XHTTP_STATUS_PARTIAL_CONTENT
	);
	Stored[1] = testHttpCacheCombineFragment(
		Fields, 2, 10, 14, 20, 20,
		XHTTP_STATUS_PARTIAL_CONTENT
	);
	Incoming = testHttpCacheCombineFragment(
		Fields, 2, 5, 9, 20, 30,
		XHTTP_STATUS_PARTIAL_CONTENT
	);
	testRequire(
		xrtHttpCacheCombinePlan(
			Stored, 2, &Incoming, &Plan
		) == XHTTP_CACHE_COMBINE_APPLY &&
		(Plan.Index == 0) &&
		(Plan.RemoveCount == 2) &&
		(Plan.ResultCount == 1) &&
		(Plan.Range.First == 0) &&
		(Plan.Range.Last == 14) &&
		!Plan.Complete,
		"HTTP cache bridge combine plan failed"
	);

	Incoming.Range =
		(xhttpbyterange){ 17, 19 };
	testRequire(
		xrtHttpCacheCombinePlan(
			Stored, 2, &Incoming, &Plan
		) == XHTTP_CACHE_COMBINE_APPLY &&
		(Plan.Index == 2) &&
		(Plan.RemoveCount == 0) &&
		(Plan.ResultCount == 3) &&
		(Plan.Range.First == 17) &&
		(Plan.Range.Last == 19),
		"HTTP cache disjoint insertion plan failed"
	);
}



/* 验证不同或弱验证器不会错误组合，长度矛盾单独报告。 */
static void testHttpCacheCombineValidators(void)
{
	static const xhttpfield StrongA[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"a\"")
		}
	};
	static const xhttpfield StrongB[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"b\"")
		}
	};
	static const xhttpfield Weak[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("W/\"a\"")
		}
	};
	xhttpcachefragment Stored =
		testHttpCacheCombineFragment(
			StrongA, 1, 0, 4, 10, 10,
			XHTTP_STATUS_PARTIAL_CONTENT
		);
	xhttpcachefragment Incoming =
		testHttpCacheCombineFragment(
			StrongB, 1, 5, 9, 10, 20,
			XHTTP_STATUS_PARTIAL_CONTENT
		);
	xhttpcachecombineplan Plan;

	testRequire(
		xrtHttpCacheCombinePlan(
			&Stored, 1, &Incoming, &Plan
		) == XHTTP_CACHE_COMBINE_SEPARATE,
		"HTTP cache combined different strong ETags"
	);
	Stored.Entry.Fields = Weak;
	Incoming.Entry.Fields = Weak;
	testRequire(
		xrtHttpCacheCombinePlan(
			&Stored, 1, &Incoming, &Plan
		) == XHTTP_CACHE_COMBINE_SEPARATE,
		"HTTP cache combined weak ETags"
	);
	Stored.Entry.Fields = StrongA;
	Incoming.Entry.Fields = StrongA;
	Incoming.Length = 11;
	testRequire(
		xrtHttpCacheCombinePlan(
			&Stored, 1, &Incoming, &Plan
		) == XHTTP_CACHE_COMBINE_CONFLICT,
		"HTTP cache ignored complete-length conflict"
	);
}



/* 验证可靠 Last-Modified 可以作为片段组合的强验证器。 */
static void testHttpCacheCombineDates(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Last-Modified"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:35 GMT")
		},
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:37 GMT")
		}
	};
	xhttpcachefragment Stored =
		testHttpCacheCombineFragment(
			Fields, 2, 0, 4, 10, 10,
			XHTTP_STATUS_PARTIAL_CONTENT
		);
	xhttpcachefragment Incoming =
		testHttpCacheCombineFragment(
			Fields, 2, 5, 9, 10, 20,
			XHTTP_STATUS_PARTIAL_CONTENT
		);
	xhttpcachecombineplan Plan;

	testRequire(
		xrtHttpCacheCombinePlan(
			&Stored, 1, &Incoming, &Plan
		) == XHTTP_CACHE_COMBINE_APPLY &&
		Plan.Complete,
		"HTTP cache strong Last-Modified combine failed"
	);
}



/* 验证组合要求整组片段共享同一个强验证器。 */
static void testHttpCacheCombineValidatorSet(void)
{
	static const xhttpfield Both[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"asset\"")
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
	static const xhttpfield DateOnly[] = {
		{
			XRT_STR_INIT("Last-Modified"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:35 GMT")
		},
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:37 GMT")
		}
	};
	static const xhttpfield ETagOnly[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"asset\"")
		}
	};
	static const xhttpfield Conflict[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"other\"")
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
	xhttpcachefragment Stored[3];
	xhttpcachefragment Incoming;
	xhttpcachecombineplan Plan;

	Stored[0] = testHttpCacheCombineFragment(
		Both, 3, 0, 1, 20, 10,
		XHTTP_STATUS_PARTIAL_CONTENT
	);
	Stored[1] = testHttpCacheCombineFragment(
		DateOnly, 2, 4, 5, 20, 20,
		XHTTP_STATUS_PARTIAL_CONTENT
	);
	Incoming = testHttpCacheCombineFragment(
		ETagOnly, 1, 8, 9, 20, 30,
		XHTTP_STATUS_PARTIAL_CONTENT
	);
	testRequire(
		xrtHttpCacheCombinePlan(
			Stored, 2, &Incoming, &Plan
		) == XHTTP_CACHE_COMBINE_SEPARATE,
		"HTTP cache matched only part of the stored validator set"
	);

	Incoming.Entry.Fields = Both;
	Incoming.Entry.FieldCount = 3;
	testRequire(
		xrtHttpCacheCombinePlan(
			Stored, 2, &Incoming, &Plan
		) == XHTTP_CACHE_COMBINE_APPLY,
		"HTTP cache rejected a common strong date validator"
	);

	Stored[1].Entry.Fields = Conflict;
	Stored[1].Entry.FieldCount = 3;
	testRequire(
		xrtHttpCacheCombinePlan(
			Stored, 2, &Incoming, &Plan
		) == XHTTP_CACHE_COMBINE_ERROR,
		"HTTP cache accepted conflicting stored strong ETags"
	);

	Stored[1].Entry.Fields = DateOnly;
	Stored[1].Entry.FieldCount = 2;
	Stored[2] = testHttpCacheCombineFragment(
		ETagOnly, 1, 12, 13, 20, 30,
		XHTTP_STATUS_PARTIAL_CONTENT
	);
	Incoming.Range = (xhttpbyterange){ 16, 17 };
	Incoming.Entry.ResponseTime = 40;
	testRequire(
		xrtHttpCacheCombinePlan(
			Stored, 3, &Incoming, &Plan
		) == XHTTP_CACHE_COMBINE_ERROR,
		"HTTP cache accepted a chain without one common validator"
	);
}



/* 验证 Header 来源遵循最新 200 和全 206 更新规则。 */
static void testHttpCacheCombineHeaders(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"asset\"")
		}
	};
	xhttpcachefragment Stored[2];
	xhttpcachefragment Incoming;
	xhttpcachecombineplan Plan;

	Stored[0] = testHttpCacheCombineFragment(
		Fields, 1, 0, 1, 10, 30,
		XHTTP_STATUS_PARTIAL_CONTENT
	);
	Stored[1] = testHttpCacheCombineFragment(
		Fields, 1, 4, 5, 10, 40,
		XHTTP_STATUS_PARTIAL_CONTENT
	);
	Incoming = testHttpCacheCombineFragment(
		Fields, 1, 8, 9, 10, 50,
		XHTTP_STATUS_PARTIAL_CONTENT
	);
	testRequire(
		xrtHttpCacheCombinePlan(
			Stored, 2, &Incoming, &Plan
		) == XHTTP_CACHE_COMBINE_APPLY &&
		(Plan.HeaderIndex == 1) &&
		((Plan.Actions &
		  XHTTP_CACHE_COMBINE_UPDATE_INCOMING_FIELDS) != 0),
		"HTTP cache all-206 Header update plan failed"
	);

	Stored[0].SourceStatus = XHTTP_STATUS_OK;
	testRequire(
		xrtHttpCacheCombinePlan(
			Stored, 2, &Incoming, &Plan
		) == XHTTP_CACHE_COMBINE_APPLY &&
		(Plan.HeaderIndex == 0) &&
		((Plan.Actions &
		  XHTTP_CACHE_COMBINE_UPDATE_INCOMING_FIELDS) != 0),
		"HTTP cache stored 200 Header update plan failed"
	);

	Incoming.SourceStatus = XHTTP_STATUS_OK;
	Incoming.Range = (xhttpbyterange){ 0, 7 };
	testRequire(
		xrtHttpCacheCombinePlan(
			Stored, 2, &Incoming, &Plan
		) == XHTTP_CACHE_COMBINE_APPLY &&
		(Plan.HeaderIndex == XRT_NPOS) &&
		((Plan.Actions &
		  XHTTP_CACHE_COMBINE_USE_INCOMING_FIELDS) != 0),
		"HTTP cache newest incomplete 200 Header was not selected"
	);
}



/* 验证完整新响应无需验证器即可替换全部旧片段。 */
static void testHttpCacheCombineReplace(void)
{
	static const xhttpfield StoredFields[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"old\"")
		}
	};
	xhttpcachefragment Stored =
		testHttpCacheCombineFragment(
			StoredFields, 1, 0, 4, 10, 10,
			XHTTP_STATUS_PARTIAL_CONTENT
		);
	xhttpcachefragment Incoming;
	xhttpcachecombineplan Plan;

	memset(&Incoming, 0, sizeof(Incoming));
	Incoming.Entry.Fields = NULL;
	Incoming.Entry.FieldCount = 0;
	Incoming.Entry.ResponseTime = 20;
	Incoming.Range = (xhttpbyterange){ 0, 9 };
	Incoming.Length = 10;
	Incoming.SourceStatus = XHTTP_STATUS_OK;
	Incoming.Flags =
		XHTTP_CACHE_FRAGMENT_HAS_RANGE |
		XHTTP_CACHE_FRAGMENT_HAS_LENGTH;
	testRequire(
		xrtHttpCacheCombinePlan(
			&Stored, 1, &Incoming, &Plan
		) == XHTTP_CACHE_COMBINE_REPLACE &&
		(Plan.RemoveCount == 1) &&
		Plan.Complete &&
		(Plan.HeaderIndex == XRT_NPOS),
		"HTTP cache complete replacement plan failed"
	);
}



/* 执行 HTTP 缓存部分响应组合测试。 */
int main(void)
{
	testHttpCacheCombineAdjacent();
	testHttpCacheCombineWindows();
	testHttpCacheCombineValidators();
	testHttpCacheCombineDates();
	testHttpCacheCombineValidatorSet();
	testHttpCacheCombineHeaders();
	testHttpCacheCombineReplace();
	return 0;
}
