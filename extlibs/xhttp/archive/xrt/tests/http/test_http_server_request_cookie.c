#include "http_server_request_fixture.h"



/* 验证多字段 Cookie 查找、缺失项和全量严格校验。 */
int main(void)
{
	xhttpserverrequest* pRequest =
		testHttpServerRequestFixtureCreate(
			"GET / HTTP/1.1\r\n"
			"Host: example.test\r\n"
			"Cookie: sid=one; theme=dark\r\n"
			"Cookie: sid=two; mode=full\r\n"
			"\r\n",
			(xbytesview){ NULL, 0 },
			XHTTP_SERVER_REQUEST_NONE
		);
	xcookiepair Cookie;
	xcookiepair Empty = { 0 };
	xcookiepair Cookies[4];
	xcookiepair Before[4];
	size_t iCount = 0;

	testRequire(
		xrtHttpServerRequestCookies(
			pRequest, NULL, 0, &iCount
		) && (iCount == 4),
		"HTTP server request Cookie count mismatch"
	);
	memset(Cookies, 0xA5, sizeof(Cookies));
	memcpy(Before, Cookies, sizeof(Cookies));
	testRequire(
		!xrtHttpServerRequestCookies(
			pRequest, Cookies, 3, &iCount
		) && (iCount == 4) &&
		(memcmp(Cookies, Before, sizeof(Cookies)) == 0),
		"HTTP server request Cookie short array was not atomic"
	);
	testHttpServerRequestFixtureError(
		XERR_RANGE,
		XHTTP_SERVER_REQUEST_ERROR_HEADER,
		"HTTP server request Cookie capacity error mismatch"
	);
	testRequire(
		xrtHttpServerRequestCookies(
			pRequest, Cookies, 4, &iCount
		) && (iCount == 4) &&
		testHttpServerRequestFixtureText(
			Cookies[0].Value, "one"
		) && testHttpServerRequestFixtureText(
			Cookies[3].Value, "full"
		),
		"HTTP server request Cookie batch order mismatch"
	);
	testRequire(
		(xrtHttpServerRequestCookie(
			pRequest,
			XRT_STR_LITERAL("mode"),
			&Cookie
		 ) == XCOOKIE_NEXT_ITEM) &&
		testHttpServerRequestFixtureText(
			Cookie.Value,
			"full"
		) &&
		(xrtHttpServerRequestCookie(
			pRequest,
			XRT_STR_LITERAL("missing"),
			&Cookie
		 ) == XCOOKIE_NEXT_END) &&
		(memcmp(&Cookie, &Empty, sizeof(Cookie)) == 0),
		"HTTP server request Cookie lookup mismatch"
	);
	testRequire(
		xrtHttpServerRequestCookie(
			pRequest,
			XRT_STR_LITERAL("sid"),
			(xcookiepair*)(void*)pRequest->Fields
		) == XCOOKIE_NEXT_ERROR,
		"HTTP server request Cookie output overwrote request fields"
	);
	testHttpServerRequestFixtureError(
		XERR_ARGUMENT,
		XHTTP_SERVER_REQUEST_ERROR_ARGUMENT,
		"HTTP server request Cookie overlap error mismatch"
	);
	xrtHttpServerRequestDestroy(pRequest);

	/* 批量结果和数量描述符均允许未对齐存储。 */
	pRequest = testHttpServerRequestFixtureCreate(
		"GET / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Cookie: a=1; b=2\r\n"
		"\r\n",
		(xbytesview){ NULL, 0 },
		XHTTP_SERVER_REQUEST_NONE
	);
	{
		uint8 PairStorage[(sizeof(xcookiepair) * 2u) + 2u];
		uint8 CountStorage[sizeof(size_t) + 2u];
		xcookiepair* pPairs =
			(xcookiepair*)(void*)(PairStorage + 1u);
		size_t* pCount =
			(size_t*)(void*)(CountStorage + 1u);
		xcookiepair Pair;
		size_t iParsed;

		memset(PairStorage, 0xA5, sizeof(PairStorage));
		memset(CountStorage, 0xA5, sizeof(CountStorage));
		testRequire(xrtHttpServerRequestCookies(
			pRequest, pPairs, 2, pCount
		), "HTTP server request rejected unaligned Cookie outputs");
		memcpy(&iParsed, pCount, sizeof(iParsed));
		memcpy(
			&Pair,
			PairStorage + 1u + sizeof(Pair),
			sizeof(Pair)
		);
		testRequire(
			(iParsed == 2) &&
			testHttpServerRequestFixtureText(
				Pair.Value, "2"
			) && (PairStorage[0] == UINT8_C(0xA5)) &&
			(PairStorage[sizeof(PairStorage) - 1u] ==
			 UINT8_C(0xA5)),
			"HTTP server request unaligned Cookie output mismatch"
		);
	}
	xrtHttpServerRequestDestroy(pRequest);

	/* 后续坏字段不能因为前一字段已命中而被忽略。 */
	pRequest = testHttpServerRequestFixtureCreate(
		"GET / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Cookie: sid=one\r\n"
		"Cookie: broken\r\n"
		"\r\n",
		(xbytesview){ NULL, 0 },
		XHTTP_SERVER_REQUEST_NONE
	);
	testRequire(
		xrtHttpServerRequestCookie(
			pRequest,
			XRT_STR_LITERAL("sid"),
			&Cookie
		) == XCOOKIE_NEXT_ERROR &&
		(memcmp(&Cookie, &Empty, sizeof(Cookie)) == 0),
		"HTTP server request ignored an invalid trailing Cookie field"
	);
	testHttpServerRequestFixtureError(
		XERR_VALUE,
		XHTTP_SERVER_REQUEST_ERROR_HEADER,
		"HTTP server request Cookie error mismatch"
	);
	iCount = 77;
	testRequire(
		!xrtHttpServerRequestCookies(
			pRequest, Cookies, 4, &iCount
		) && (iCount == 77),
		"HTTP server request Cookie batch accepted invalid fields"
	);
	testHttpServerRequestFixtureError(
		XERR_VALUE,
		XHTTP_SERVER_REQUEST_ERROR_HEADER,
		"HTTP server request Cookie batch error mismatch"
	);
	xrtHttpServerRequestDestroy(pRequest);
	printf("[PASS] HTTP server request Cookie helper\n");
	return 0;
}
