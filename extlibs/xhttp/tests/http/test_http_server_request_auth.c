#include "http_server_request_fixture.h"



/* 验证服务端通用认证的唯一字段、缺失、重复和格式错误。 */
int main(void)
{
	xhttpserverrequest* pRequest =
		testHttpServerRequestFixtureCreate(
			"GET / HTTP/1.1\r\n"
			"Host: example.test\r\n"
			"Authorization: Custom token\r\n"
			"Proxy-Authorization: Proxy data\r\n"
			"\r\n",
			(xbytesview){ NULL, 0 },
			XHTTP_SERVER_REQUEST_NONE
		);
	xhttpauth Auth;
	uint8 AuthStorage[sizeof(xhttpauth) + 2u];
	xhttpauth* pUnalignedAuth =
		(xhttpauth*)(void*)(AuthStorage + 1u);

	testRequire((xrtHttpServerRequestAuth(
		pRequest, &Auth
	) == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(Auth.Scheme, XRT_STR_LITERAL("Custom")) &&
		(xrtHttpServerRequestProxyAuth(
			pRequest, &Auth
		) == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(Auth.Scheme, XRT_STR_LITERAL("Proxy")),
		"HTTP server generic authentication mismatch");
	memset(AuthStorage, 0xA5, sizeof(AuthStorage));
	testRequire(
		xrtHttpServerRequestAuth(
			pRequest, pUnalignedAuth
		) == XHTTP_NEXT_ITEM,
		"HTTP server rejected unaligned authentication output"
	);
	memcpy(&Auth, pUnalignedAuth, sizeof(Auth));
	testRequire(
		xrtHttpTokenEqual(
			Auth.Scheme, XRT_STR_LITERAL("Custom")
		) && (AuthStorage[0] == UINT8_C(0xA5)) &&
		(AuthStorage[sizeof(AuthStorage) - 1u] ==
		 UINT8_C(0xA5)),
		"HTTP server unaligned authentication output mismatch"
	);
	testRequire(
		xrtHttpServerRequestAuth(
			pRequest,
			(xhttpauth*)(void*)pRequest->Fields
		) == XHTTP_NEXT_ERROR,
		"HTTP server authentication output overwrote request fields"
	);
	testHttpServerRequestFixtureError(
		XERR_ARGUMENT,
		XHTTP_SERVER_REQUEST_ERROR_ARGUMENT,
		"HTTP server authentication overlap error mismatch"
	);
	xrtHttpServerRequestDestroy(pRequest);

	pRequest = testHttpServerRequestFixtureCreate(
		"GET / HTTP/1.1\r\nHost: example.test\r\n\r\n",
		(xbytesview){ NULL, 0 },
		XHTTP_SERVER_REQUEST_NONE
	);
	testRequire((xrtHttpServerRequestAuth(
		pRequest, &Auth
	) == XHTTP_NEXT_END) &&
		(Auth.Scheme.Data == NULL),
		"HTTP server missing authentication mismatch");
	xrtHttpServerRequestDestroy(pRequest);

	pRequest = testHttpServerRequestFixtureCreate(
		"GET / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Authorization: Custom one\r\n"
		"Authorization: Custom two\r\n"
		"\r\n",
		(xbytesview){ NULL, 0 },
		XHTTP_SERVER_REQUEST_NONE
	);
	testRequire(xrtHttpServerRequestAuth(
		pRequest, &Auth
	) == XHTTP_NEXT_ERROR,
		"HTTP server accepted duplicate Authorization");
	testHttpServerRequestFixtureError(
		XERR_VALUE,
		XHTTP_SERVER_REQUEST_ERROR_HEADER,
		"HTTP server duplicate Authorization error mismatch"
	);
	xrtHttpServerRequestDestroy(pRequest);

	pRequest = testHttpServerRequestFixtureCreate(
		"GET / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Authorization: Basic abc, Bearer def\r\n"
		"\r\n",
		(xbytesview){ NULL, 0 },
		XHTTP_SERVER_REQUEST_NONE
	);
	testRequire(xrtHttpServerRequestAuth(
		pRequest, &Auth
	) == XHTTP_NEXT_ERROR,
		"HTTP server accepted authentication list as credentials");
	testHttpServerRequestFixtureError(
		XERR_VALUE,
		XHTTP_SERVER_REQUEST_ERROR_AUTH,
		"HTTP server malformed Authorization error mismatch"
	);
	xrtHttpServerRequestDestroy(pRequest);
	puts("[PASS] HTTP server request authentication");
	return 0;
}
