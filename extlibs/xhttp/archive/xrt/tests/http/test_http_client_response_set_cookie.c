#include "http_client_response_fixture.h"



/* 验证独立 Set-Cookie 字段按线路顺序迭代。 */
static void testHttpResponseSetCookieOrder(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Date"), XRT_STR_INIT("ignored") },
		{
			XRT_STR_INIT("Set-Cookie"),
			XRT_STR_INIT("sid=one; Path=/; HttpOnly")
		},
		{ XRT_STR_INIT("X-Test"), XRT_STR_INIT("ignored") },
		{
			XRT_STR_INIT("set-cookie"),
			XRT_STR_INIT("theme=dark; SameSite=Lax")
		},
		{ XRT_STR_INIT("Cache-Control"), XRT_STR_INIT("no-cache") }
	};
	xhttpresponse* pResponse = testHttpResponseFixtureCreate(
		Fields,
		5
	);
	xsetcookie Cookie;
	size_t iIndex = 0;
	uint8 CookieStorage[sizeof(xsetcookie) + 2u];
	uint8 IndexStorage[sizeof(size_t) + 2u];

	testRequire(
		(xrtHttpResponseSetCookieNext(
			pResponse,
			&iIndex,
			&Cookie
		 ) == XHTTP_NEXT_ITEM) &&
		(iIndex == 2) &&
		testHttpResponseFixtureText(Cookie.Name, "sid") &&
		testHttpResponseFixtureText(Cookie.Value, "one") &&
		((Cookie.Flags & XSET_COOKIE_HTTP_ONLY) != 0),
		"HTTP response first Set-Cookie mismatch"
	);
	testRequire(
		(xrtHttpResponseSetCookieNext(
			pResponse,
			&iIndex,
			&Cookie
		 ) == XHTTP_NEXT_ITEM) &&
		(iIndex == 4) &&
		testHttpResponseFixtureText(Cookie.Name, "theme") &&
		testHttpResponseFixtureText(Cookie.Value, "dark") &&
		(Cookie.SameSite == XCOOKIE_SAME_SITE_LAX),
		"HTTP response second Set-Cookie mismatch"
	);
	testRequire(
		(xrtHttpResponseSetCookieNext(
			pResponse,
			&iIndex,
			&Cookie
		 ) == XHTTP_NEXT_END) &&
		(iIndex == 5) &&
		(Cookie.Name.Data == NULL) &&
		(Cookie.Name.Size == 0),
		"HTTP response Set-Cookie end mismatch"
	);
	iIndex = 0;
	memset(CookieStorage, 0xA5, sizeof(CookieStorage));
	memset(IndexStorage, 0x5A, sizeof(IndexStorage));
	memcpy(IndexStorage + 1u, &iIndex, sizeof(iIndex));
	testRequire(
		(xrtHttpResponseSetCookieNext(
			pResponse,
			(size_t*)(void*)(IndexStorage + 1u),
			(xsetcookie*)(void*)(CookieStorage + 1u)
		 ) == XHTTP_NEXT_ITEM),
		"HTTP response unaligned Set-Cookie output failed"
	);
	memcpy(&iIndex, IndexStorage + 1u, sizeof(iIndex));
	memcpy(&Cookie, CookieStorage + 1u, sizeof(Cookie));
	testRequire(
		(IndexStorage[0] == 0x5A) &&
		(IndexStorage[sizeof(IndexStorage) - 1u] == 0x5A) &&
		(CookieStorage[0] == 0xA5) &&
		(CookieStorage[sizeof(CookieStorage) - 1u] == 0xA5) &&
		(iIndex == 2u) &&
		testHttpResponseFixtureText(Cookie.Name, "sid"),
		"HTTP response unaligned Set-Cookie state mismatch"
	);
	xrtHttpResponseDestroy(pResponse);
}



/* 验证错误字段定位、继续扫描和索引错误。 */
static void testHttpResponseSetCookieErrors(void)
{
	char Oversized[XSET_COOKIE_MAX_PAIR_BYTES + 2u];
	xhttpfield Fields[] = {
		{ XRT_STR_INIT("X-Test"), XRT_STR_INIT("ignored") },
		{
			XRT_STR_INIT("Set-Cookie"),
			{ Oversized, sizeof(Oversized) }
		},
		{ XRT_STR_INIT("Set-Cookie"), XRT_STR_INIT("ok=yes") }
	};
	xhttpresponse* pResponse;
	xsetcookie Cookie;
	size_t iIndex = 0;
	const xhttpfield* pField;

	Oversized[0] = 'a';
	Oversized[1] = '=';
	memset(Oversized + 2, 'x', sizeof(Oversized) - 2u);
	pResponse = testHttpResponseFixtureCreate(Fields, 3);
	testRequire(
		(xrtHttpResponseSetCookieNext(
			pResponse,
			&iIndex,
			&Cookie
		 ) == XHTTP_NEXT_ERROR) &&
		(iIndex == 1),
		"HTTP response invalid Set-Cookie position mismatch"
	);
	testHttpResponseFixtureError(
		XERR_RANGE,
		XHTTP_RESPONSE_ERROR_SET_COOKIE,
		"HTTP response invalid Set-Cookie error mismatch"
	);
	iIndex++;
	testRequire(
		(xrtHttpResponseSetCookieNext(
			pResponse,
			&iIndex,
			&Cookie
		 ) == XHTTP_NEXT_ITEM) &&
		testHttpResponseFixtureText(Cookie.Name, "ok") &&
		testHttpResponseFixtureText(Cookie.Value, "yes"),
		"HTTP response did not continue after invalid Set-Cookie"
	);
	iIndex = 4;
	testRequire(
		xrtHttpResponseSetCookieNext(
			pResponse,
			&iIndex,
			&Cookie
		) == XHTTP_NEXT_ERROR,
		"HTTP response accepted out-of-range Set-Cookie index"
	);
	testHttpResponseFixtureError(
		XERR_RANGE,
		XHTTP_RESPONSE_ERROR_INDEX,
		"HTTP response Set-Cookie index error mismatch"
	);
	pField = xrtHttpResponseHeaderAt(pResponse, 1u);
	iIndex = 0;
	testRequire(
		(pField != NULL) &&
		(xrtHttpResponseSetCookieNext(
			pResponse,
			&iIndex,
			(xsetcookie*)(void*)pField->Value.Data
		 ) == XHTTP_NEXT_ERROR),
		"HTTP response accepted Set-Cookie output over a field"
	);
	testHttpResponseFixtureError(
		XERR_ARGUMENT,
		XHTTP_RESPONSE_ERROR_ARGUMENT,
		"HTTP response overlapping Set-Cookie output error mismatch"
	);
	testRequire(
		xrtHttpResponseSetCookieNext(
			pResponse,
			(size_t*)(uintptr_t)(UINTPTR_MAX - 1u),
			&Cookie
		) == XHTTP_NEXT_ERROR,
		"HTTP response accepted wrapping Set-Cookie cursor"
	);
	testHttpResponseFixtureError(
		XERR_ARGUMENT,
		XHTTP_RESPONSE_ERROR_ARGUMENT,
		"HTTP response wrapping Set-Cookie cursor error mismatch"
	);
	xrtHttpResponseDestroy(pResponse);
}



/* 运行客户端响应 Set-Cookie 便利层测试。 */
int main(void)
{
	testHttpResponseSetCookieOrder();
	testHttpResponseSetCookieErrors();
	printf("[PASS] HTTP client response Set-Cookie helper\n");
	return 0;
}
