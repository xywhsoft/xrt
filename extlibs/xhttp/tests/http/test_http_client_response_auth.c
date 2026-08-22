#include "http_client_response_fixture.h"



/* 验证客户端响应跨重复字段读取源站与代理 challenge。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("WWW-Authenticate"),
			XRT_STR_INIT("Digest realm=\"api\", Basic abc==")
		},
		{
			XRT_STR_INIT("Proxy-Authenticate"),
			XRT_STR_INIT("Basic proxy==")
		},
		{
			XRT_STR_INIT("WWW-Authenticate"),
			XRT_STR_INIT("Bearer token")
		}
	};
	static const xhttpfield Malformed[] = {
		{
			XRT_STR_INIT("WWW-Authenticate"),
			XRT_STR_INIT("Digest realm =")
		}
	};
	xhttpresponse* pResponse = testHttpResponseFixtureCreate(
		Fields,
		sizeof(Fields) / sizeof(Fields[0])
	);
	const xerror* pError;
	xhttpauthcursor Cursor;
	xhttpauth Auth;
	unsigned char CursorStorage[sizeof(xhttpauthcursor) + 1u];
	unsigned char AuthStorage[sizeof(xhttpauth) + 1u];

	xrtHttpAuthCursorInit(&Cursor);
	testRequire((xrtHttpResponseChallengeNext(
		pResponse, &Cursor, &Auth
	) == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(Auth.Scheme, XRT_STR_LITERAL("Digest")) &&
		(xrtHttpResponseChallengeNext(
			pResponse, &Cursor, &Auth
		) == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(Auth.Scheme, XRT_STR_LITERAL("Basic")) &&
		(xrtHttpResponseChallengeNext(
			pResponse, &Cursor, &Auth
		) == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(Auth.Scheme, XRT_STR_LITERAL("Bearer")) &&
		(xrtHttpResponseChallengeNext(
			pResponse, &Cursor, &Auth
		) == XHTTP_NEXT_END),
		"HTTP client response challenge order mismatch");
	xrtHttpAuthCursorInit(&Cursor);
	testRequire((xrtHttpResponseProxyChallengeNext(
		pResponse, &Cursor, &Auth
	) == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(Auth.Scheme, XRT_STR_LITERAL("Basic")),
		"HTTP client proxy challenge mismatch");
	xrtHttpAuthCursorInit(&Cursor);
	memcpy(CursorStorage + 1u, &Cursor, sizeof(Cursor));
	testRequire(
		xrtHttpResponseChallengeNext(
			pResponse,
			(xhttpauthcursor*)(void*)(CursorStorage + 1u),
			(xhttpauth*)(void*)(AuthStorage + 1u)
		) == XHTTP_NEXT_ITEM,
		"HTTP client unaligned response challenge failed"
	);
	memcpy(&Auth, AuthStorage + 1u, sizeof(Auth));
	testRequire(
		xrtHttpTokenEqual(Auth.Scheme, XRT_STR_LITERAL("Digest")),
		"HTTP client unaligned response challenge mismatch"
	);

	/* 输出不得覆盖响应对象、响应文本或动态 Header 容器本体。 */
	xrtHttpAuthCursorInit(&Cursor);
	testRequire(xrtHttpResponseChallengeNext(
		pResponse,
		(xhttpauthcursor*)(void*)pResponse,
		&Auth
	) == XHTTP_NEXT_ERROR,
		"HTTP client challenge accepted response object output");
	testHttpResponseFixtureError(
		XERR_ARGUMENT,
		XHTTP_RESPONSE_ERROR_ARGUMENT,
		"HTTP client challenge response object error mismatch"
	);
	xrtHttpAuthCursorInit(&Cursor);
	testRequire(xrtHttpResponseChallengeNext(
		pResponse,
		&Cursor,
		(xhttpauth*)(void*)xrtHttpResponseReason(pResponse).Data
	) == XHTTP_NEXT_ERROR,
		"HTTP client challenge accepted response text output");
	testHttpResponseFixtureError(
		XERR_ARGUMENT,
		XHTTP_RESPONSE_ERROR_ARGUMENT,
		"HTTP client challenge response text error mismatch"
	);
	xrtHttpAuthCursorInit(&Cursor);
	testRequire(xrtHttpResponseChallengeNext(
		pResponse,
		&Cursor,
		(xhttpauth*)(void*)xrtHttpResponseHeaders(pResponse)
	) == XHTTP_NEXT_ERROR,
		"HTTP client challenge accepted Header container output");
	testHttpResponseFixtureError(
		XERR_ARGUMENT,
		XHTTP_RESPONSE_ERROR_ARGUMENT,
		"HTTP client challenge Header container error mismatch"
	);
	testRequire(
		xrtHttpResponseStatus(pResponse) == XHTTP_STATUS_OK,
		"HTTP client challenge alias damaged response"
	);
	xrtHttpResponseDestroy(pResponse);

	/* 畸形 challenge 必须进入稳定响应错误域，并保留协议值错误原因。 */
	pResponse = testHttpResponseFixtureCreate(
		Malformed,
		sizeof(Malformed) / sizeof(Malformed[0])
	);
	testRequire(
		pResponse != NULL,
		"HTTP client malformed challenge setup failed"
	);
	xrtHttpAuthCursorInit(&Cursor);
	memset(&Auth, 0xA5, sizeof(Auth));
	testRequire((xrtHttpResponseChallengeNext(
		pResponse, &Cursor, &Auth
	) == XHTTP_NEXT_ERROR) &&
		(Auth.Scheme.Data == NULL) &&
		(Auth.Scheme.Size == 0u) &&
		(Auth.Data.Data == NULL) &&
		(Auth.Data.Size == 0u),
		"HTTP client accepted malformed authentication challenge");
	pError = xrtGetError();
	testRequire((pError != NULL) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.client.response"
		) == 0) &&
		(xrtErrorCode(pError) == XHTTP_RESPONSE_ERROR_AUTH) &&
		(xrtErrorKind(pError) == XERR_VALUE) &&
		(xrtErrorCause(pError) != NULL) &&
		(xrtErrorKind(xrtErrorCause(pError)) == XERR_VALUE),
		"HTTP client malformed challenge error chain mismatch");
	xrtClearError();
	xrtHttpResponseDestroy(pResponse);
	puts("[PASS] HTTP client response authentication");
	return 0;
}
