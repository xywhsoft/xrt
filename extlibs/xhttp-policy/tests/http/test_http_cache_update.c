#include "../test.h"

#include <xrt/http_cache_validate.h>



/* 构造借用 Header 的缓存候选。 */
static xhttpcacheentry testHttpCacheUpdateEntry(
	const xhttpfield* pFields,
	size_t iCount,
	xtime iResponseTime
)
{
	xhttpcacheentry Entry;

	Entry.Fields = pFields;
	Entry.FieldCount = iCount;
	Entry.ResponseTime = iResponseTime;
	Entry.Flags = XHTTP_CACHE_ENTRY_NONE;
	return Entry;
}



/* 验证 304 的强匹配、弱匹配和无验证器规则。 */
static void testHttpCache304Selection(void)
{
	static const xhttpfield StrongResponse[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"same\"")
		}
	};
	static const xhttpfield WeakResponse[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("W/\"same\"")
		}
	};
	static const xhttpfield StrongOne[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"same\"")
		},
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:35 GMT")
		}
	};
	static const xhttpfield StrongTwo[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"same\"")
		},
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:37 GMT")
		}
	};
	static const xhttpfield WeakThree[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("W/\"same\"")
		},
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:36 GMT")
		}
	};
	static const xhttpfield InvalidResponse[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"bad")
		}
	};
	xhttpcacheentry Entries[3];
	size_t Indices[3] = { 9, 9, 9 };
	size_t iAliased;
	size_t iPattern;
	size_t iCount = 0;
	xhttpcacheupdatematch Match;

	Entries[0] = testHttpCacheUpdateEntry(
		StrongOne,
		sizeof(StrongOne) / sizeof(StrongOne[0]),
		0
	);
	Entries[1] = testHttpCacheUpdateEntry(
		StrongTwo,
		sizeof(StrongTwo) / sizeof(StrongTwo[0]),
		0
	);
	Entries[2] = testHttpCacheUpdateEntry(
		WeakThree,
		sizeof(WeakThree) / sizeof(WeakThree[0]),
		0
	);
	Match = xrtHttpCache304Select(
		StrongResponse,
		1,
		Entries,
		3,
		Indices,
		3,
		&iCount
	);
	testRequire(
		(Match == XHTTP_CACHE_UPDATE_MATCH_STRONG) &&
		(iCount == 2) &&
		(Indices[0] == 0) &&
		(Indices[1] == 1),
		"HTTP cache 304 strong selection mismatch"
	);
	Match = xrtHttpCache304Select(
		WeakResponse,
		1,
		Entries,
		3,
		Indices,
		3,
		&iCount
	);
	testRequire(
		(Match == XHTTP_CACHE_UPDATE_MATCH_WEAK) &&
		(iCount == 1) &&
		(Indices[0] == 1),
		"HTTP cache 304 weak newest selection mismatch"
	);
	memset(Indices, 0x5A, sizeof(Indices));
	memset(&iPattern, 0x5A, sizeof(iPattern));
	Match = xrtHttpCache304Select(
		StrongResponse,
		1,
		Entries,
		3,
		Indices,
		1,
		&iCount
	);
	testRequire(
		(Match == XHTTP_CACHE_UPDATE_MATCH_ERROR) &&
		(iCount == 2) &&
		(Indices[0] == iPattern),
		"HTTP cache 304 short selection changed output"
	);
	xrtClearError();
	memset(&iAliased, 0x5A, sizeof(iAliased));
	iPattern = iAliased;
	testRequire(
		(xrtHttpCache304Select(
			StrongResponse,
			1,
			Entries,
			3,
			&iAliased,
			1,
			&iAliased
		 ) == XHTTP_CACHE_UPDATE_MATCH_ERROR) &&
		(iAliased == iPattern),
		"HTTP cache 304 alias changed output"
	);
	xrtClearError();

	Entries[0] = testHttpCacheUpdateEntry(
		NULL, 0, 10
	);
	testRequire(
		(xrtHttpCache304Select(
			NULL, 0,
			Entries, 1,
			Indices, 1,
			&iCount
		 ) == XHTTP_CACHE_UPDATE_MATCH_SINGLE) &&
		(iCount == 1) &&
		(Indices[0] == 0),
		"HTTP cache 304 validator-free single mismatch"
	);
	testRequire(
		(xrtHttpCache304Select(
			InvalidResponse, 1,
			Entries, 1,
			Indices, 1,
			&iCount
		 ) == XHTTP_CACHE_UPDATE_MATCH_NONE) &&
		(iCount == 0),
		"HTTP cache 304 malformed validator became absent"
	);
}



/* 验证 HEAD freshening 的验证器和正文长度约束。 */
static void testHttpCacheHeadUpdate(void)
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
	static const xhttpfield Matching[] = {
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
	static const xhttpfield Changed[] = {
		{
			XRT_STR_INIT("Content-Length"),
			XRT_STR_INIT("101")
		}
	};
	static const xhttpfield Invalid[] = {
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"bad")
		}
	};
	xhttpcacheentry Entry = testHttpCacheUpdateEntry(
		Stored,
		sizeof(Stored) / sizeof(Stored[0]),
		0
	);

	testRequire(
		xrtHttpCacheHeadPlan(
			200, &Entry,
			Matching,
			sizeof(Matching) / sizeof(Matching[0])
		) == XHTTP_CACHE_HEAD_UPDATE,
		"HTTP cache matching HEAD was not freshened"
	);
	testRequire(
		xrtHttpCacheHeadPlan(
			200, &Entry, Changed, 1
		) == XHTTP_CACHE_HEAD_STALE,
		"HTTP cache changed HEAD length stayed fresh"
	);
	testRequire(
		xrtHttpCacheHeadPlan(
			200, &Entry, Invalid, 1
		) == XHTTP_CACHE_HEAD_STALE,
		"HTTP cache malformed HEAD validator stayed fresh"
	);
	testRequire(
		xrtHttpCacheHeadPlan(
			404, &Entry, NULL, 0
		) == XHTTP_CACHE_HEAD_IGNORE,
		"HTTP cache non-200 HEAD was freshened"
	);
}



/* 验证 RFC 字段替换例外和限定字段列表。 */
static void testHttpCacheFieldUpdates(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Connection"),
			XRT_STR_INIT("X-Hop")
		},
		{
			XRT_STR_INIT("X-Hop"),
			XRT_STR_INIT("secret")
		},
		{
			XRT_STR_INIT("Content-Length"),
			XRT_STR_INIT("5")
		},
		{
			XRT_STR_INIT("Proxy-Authenticate"),
			XRT_STR_INIT("Basic")
		},
		{
			XRT_STR_INIT("X-Secret"),
			XRT_STR_INIT("hidden")
		},
		{
			XRT_STR_INIT("Set-Cookie"),
			XRT_STR_INIT("sid=1")
		},
		{
			XRT_STR_INIT("ETag"),
			XRT_STR_INIT("\"new\"")
		},
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT(
				"no-cache=\"X\\-Secret\", "
				"private=\"Set-Cookie\""
			)
		}
	};
	static const xhttpfield EmptyElements[] = {
		{
			XRT_STR_INIT("X-Secret"),
			XRT_STR_INIT("hidden")
		},
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT(
				"no-cache=\" , X-Secret, , \""
			)
		}
	};
	static const xhttpfield MalformedName[] = {
		{
			XRT_STR_INIT("X-Secret"),
			XRT_STR_INIT("hidden")
		},
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT(
				"no-cache=\"bad name\""
			)
		}
	};
	static const xhttpfield EmptyList[] = {
		{
			XRT_STR_INIT("X-Secret"),
			XRT_STR_INIT("hidden")
		},
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT(
				"no-cache=\" , , \""
			)
		}
	};
	static const xhttpfield Framing[] = {
		{
			XRT_STR_INIT("Transfer-Encoding"),
			XRT_STR_INIT("chunked")
		}
	};

	testRequire(
		xrtHttpCacheFieldUpdate(
			Fields, 8, 0, true,
			XHTTP_CACHE_UPDATE_FIELD_NONE
		) == XHTTP_CACHE_FIELD_UPDATE_SKIP,
		"HTTP cache updated Connection"
	);
	testRequire(
		xrtHttpCacheFieldUpdate(
			Fields, 8, 1, true,
			XHTTP_CACHE_UPDATE_FIELD_NONE
		) == XHTTP_CACHE_FIELD_UPDATE_SKIP,
		"HTTP cache updated Connection-nominated field"
	);
	testRequire(
		xrtHttpCacheFieldUpdate(
			Fields, 8, 2, true,
			XHTTP_CACHE_UPDATE_FIELD_NONE
		) == XHTTP_CACHE_FIELD_UPDATE_SKIP,
		"HTTP cache updated Content-Length"
	);
	testRequire(
		xrtHttpCacheFieldUpdate(
			Fields, 8, 3, true,
			XHTTP_CACHE_UPDATE_FIELD_NONE
		) == XHTTP_CACHE_FIELD_UPDATE_SKIP,
		"HTTP cache updated proxy-specific field"
	);
	testRequire(
		xrtHttpCacheFieldUpdate(
			Fields, 8, 4, true,
			XHTTP_CACHE_UPDATE_FIELD_NONE
		) == XHTTP_CACHE_FIELD_UPDATE_SKIP,
		"HTTP cache updated qualified no-cache field"
	);
	testRequire(
		xrtHttpCacheFieldUpdate(
			Fields, 8, 5, true,
			XHTTP_CACHE_UPDATE_FIELD_NONE
		) == XHTTP_CACHE_FIELD_UPDATE_SKIP,
		"shared HTTP cache updated qualified private field"
	);
	testRequire(
		xrtHttpCacheFieldUpdate(
			Fields, 8, 5, false,
			XHTTP_CACHE_UPDATE_FIELD_NONE
		) == XHTTP_CACHE_FIELD_UPDATE_REPLACE,
		"private HTTP cache removed private-qualified field"
	);
	testRequire(
		xrtHttpCacheFieldUpdate(
			Fields, 8, 6, true,
			XHTTP_CACHE_UPDATE_FIELD_NONE
		) == XHTTP_CACHE_FIELD_UPDATE_REPLACE,
		"HTTP cache refused ordinary metadata update"
	);
	testRequire(
		xrtHttpCacheFieldUpdate(
			Fields, 8, 6, true,
			XHTTP_CACHE_UPDATE_FIELD_PROCESSED
		) == XHTTP_CACHE_FIELD_UPDATE_SKIP,
		"HTTP cache updated processed metadata"
	);
	testRequire(
		xrtHttpCacheFieldUpdate(
			EmptyElements, 2, 0, true,
			XHTTP_CACHE_UPDATE_FIELD_NONE
		) == XHTTP_CACHE_FIELD_UPDATE_SKIP,
		"HTTP cache rejected legal empty list elements"
	);
	testRequire(
		xrtHttpCacheFieldUpdate(
			MalformedName, 2, 0, true,
			XHTTP_CACHE_UPDATE_FIELD_NONE
		) == XHTTP_CACHE_FIELD_UPDATE_ERROR,
		"HTTP cache accepted malformed qualified field name"
	);
	xrtClearError();
	testRequire(
		xrtHttpCacheFieldUpdate(
			EmptyList, 2, 0, true,
			XHTTP_CACHE_UPDATE_FIELD_NONE
		) == XHTTP_CACHE_FIELD_UPDATE_ERROR,
		"HTTP cache accepted an empty qualified field list"
	);
	xrtClearError();
	testRequire(
		(xrtHttpCacheFieldStore(
			Fields, 8, 0, true,
			XHTTP_CACHE_STORE_REMOVE_CONNECTION
		 ) == XHTTP_CACHE_FIELD_STORE_SKIP) &&
		(xrtHttpCacheFieldStore(
			Fields, 8, 1, true,
			XHTTP_CACHE_STORE_REMOVE_CONNECTION
		 ) == XHTTP_CACHE_FIELD_STORE_SKIP) &&
		(xrtHttpCacheFieldStore(
			Framing, 1, 0, true,
			XHTTP_CACHE_STORE_REMOVE_CONNECTION
		 ) == XHTTP_CACHE_FIELD_STORE_SKIP),
		"HTTP cache initial store retained hop metadata"
	);
	testRequire(
		(xrtHttpCacheFieldStore(
			Fields, 8, 2, true,
			XHTTP_CACHE_STORE_REMOVE_CONNECTION
		 ) == XHTTP_CACHE_FIELD_STORE_KEEP) &&
		(xrtHttpCacheFieldStore(
			Fields, 8, 3, true,
			XHTTP_CACHE_STORE_REMOVE_PROXY
		 ) == XHTTP_CACHE_FIELD_STORE_SKIP) &&
		(xrtHttpCacheFieldStore(
			Fields, 8, 4, true,
			XHTTP_CACHE_STORE_REMOVE_NO_CACHE
		 ) == XHTTP_CACHE_FIELD_STORE_SKIP) &&
		(xrtHttpCacheFieldStore(
			Fields, 8, 5, true,
			XHTTP_CACHE_STORE_REMOVE_PRIVATE
		 ) == XHTTP_CACHE_FIELD_STORE_SKIP) &&
		(xrtHttpCacheFieldStore(
			Fields, 8, 6, true,
			XHTTP_CACHE_STORE_REMOVE_CONNECTION
		 ) == XHTTP_CACHE_FIELD_STORE_KEEP),
		"HTTP cache initial store field plan mismatch"
	);
}



/* 执行 HTTP 缓存 304、HEAD 与字段更新测试。 */
int main(void)
{
	testHttpCache304Selection();
	testHttpCacheHeadUpdate();
	testHttpCacheFieldUpdates();
	printf("[PASS] http_cache_update\n");
	return 0;
}
