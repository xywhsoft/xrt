#include "http_server_request_fixture.h"



/* 验证服务端 Bearer token 保持请求快照借用视图。 */
int main(void)
{
	xhttpserverrequest* pRequest =
		testHttpServerRequestFixtureCreate(
			"GET / HTTP/1.1\r\n"
			"Host: example.test\r\n"
			"Authorization: Bearer mF_9.B5f-4.1JqM\r\n"
			"\r\n",
			(xbytesview){ NULL, 0 },
			XHTTP_SERVER_REQUEST_NONE
		);
	xstrview Token;
	uint8 TokenStorage[sizeof(xstrview) + 2u];
	xstrview* pUnalignedToken =
		(xstrview*)(void*)(TokenStorage + 1u);

	testRequire((xrtHttpServerRequestBearerAuth(
		pRequest, &Token
	) == XHTTP_NEXT_ITEM) &&
		testHttpServerRequestFixtureText(
			Token, "mF_9.B5f-4.1JqM"
		), "HTTP server Bearer authentication mismatch");
	memset(TokenStorage, 0xA5, sizeof(TokenStorage));
	testRequire(
		xrtHttpServerRequestBearerAuth(
			pRequest, pUnalignedToken
		) == XHTTP_NEXT_ITEM,
		"HTTP server rejected unaligned Bearer output"
	);
	memcpy(&Token, pUnalignedToken, sizeof(Token));
	testRequire(
		testHttpServerRequestFixtureText(
			Token, "mF_9.B5f-4.1JqM"
		) && (TokenStorage[0] == UINT8_C(0xA5)) &&
		(TokenStorage[sizeof(TokenStorage) - 1u] ==
		 UINT8_C(0xA5)),
		"HTTP server unaligned Bearer output mismatch"
	);
	xrtHttpServerRequestDestroy(pRequest);
	puts("[PASS] HTTP server request Bearer authentication");
	return 0;
}
