#include "http_client_response_fixture.h"



/* 验证两个 challenge 游标按字节完全相同。 */
static bool testHttpResponseBearerCursorEqual(
	const xhttpauthcursor* pLeft,
	const xhttpauthcursor* pRight
)
{
	return memcmp(pLeft, pRight, sizeof(*pLeft)) == 0;
}



/* 验证客户端响应 Bearer challenge 的过滤、查询和解码契约。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("WWW-Authenticate"),
			XRT_STR_INIT(
				"Basic realm=\"skip\", Bearer realm=\"api\", "
				"scope=\"read write\", error=\"invalid_token\", "
				"error_description=\"expired\", "
				"error_uri=\"https://example.com/help\", trace=one"
			)
		},
		{
			XRT_STR_INIT("WWW-Authenticate"),
			XRT_STR_INIT("Bearer realm=legacy")
		},
		{
			XRT_STR_INIT("Proxy-Authenticate"),
			XRT_STR_INIT(
				"Bearer error=\"insufficient_scope\", scope=admin"
			)
		}
	};
	static const xhttpfield ShortField[] = {
		{
			XRT_STR_INIT("WWW-Authenticate"),
			XRT_STR_INIT("Bearer error_description=\"abcdef\"")
		}
	};
	static const xhttpfield Malformed[] = {
		{
			XRT_STR_INIT("WWW-Authenticate"),
			XRT_STR_INIT(
				"Basic realm=\"skip\", Bearer error_uri=\"relative\""
			)
		}
	};
	xhttpresponse* pResponse = testHttpResponseFixtureCreate(
		Fields,
		sizeof(Fields) / sizeof(Fields[0])
	);
	xhttpauthcursor Cursor;
	xhttpauthcursor Saved;
	xhttpbearerchallenge Challenge;
	unsigned char CursorStorage[sizeof(xhttpauthcursor) + 1u];
	unsigned char SizeStorage[sizeof(size_t) + 1u];
	unsigned char ChallengeStorage[sizeof(xhttpbearerchallenge) + 1u];
	char Output[128];
	size_t iSize;
	size_t iExpected = strlen("api") + strlen("read write") +
		strlen("invalid_token") + strlen("expired") +
		strlen("https://example.com/help");

	/* 查询发布存在位和精确长度，但不消费当前 Bearer challenge。 */
	xrtHttpAuthCursorInit(&Cursor);
	Saved = Cursor;
	testRequire(
		(xrtHttpResponseBearerChallengeNext(
			pResponse,
			&Cursor,
			NULL,
			0,
			&iSize,
			&Challenge
		) == XHTTP_NEXT_ITEM) &&
		(iSize == iExpected) &&
		(Challenge.Flags == (
			XHTTP_BEARER_HAS_REALM |
			XHTTP_BEARER_HAS_SCOPE |
			XHTTP_BEARER_HAS_ERROR |
			XHTTP_BEARER_HAS_ERROR_DESCRIPTION |
			XHTTP_BEARER_HAS_ERROR_URI
		)) &&
		(Challenge.Realm.Data == NULL) &&
		testHttpResponseBearerCursorEqual(&Cursor, &Saved),
		"HTTP response Bearer challenge query contract mismatch"
	);
	testRequire(
		(xrtHttpResponseBearerChallengeNext(
			pResponse,
			&Cursor,
			Output,
			sizeof(Output),
			&iSize,
			&Challenge
		) == XHTTP_NEXT_ITEM) &&
		testHttpResponseFixtureText(Challenge.Realm, "api") &&
		testHttpResponseFixtureText(Challenge.Scope, "read write") &&
		testHttpResponseFixtureText(Challenge.Error, "invalid_token") &&
		testHttpResponseFixtureText(
			Challenge.ErrorDescription, "expired"
		) && testHttpResponseFixtureText(
			Challenge.ErrorUri, "https://example.com/help"
		),
		"HTTP response Bearer challenge decode mismatch"
	);
	testRequire(
		(xrtHttpResponseBearerChallengeNext(
			pResponse,
			&Cursor,
			Output,
			sizeof(Output),
			&iSize,
			&Challenge
		) == XHTTP_NEXT_ITEM) &&
		(Challenge.Flags == XHTTP_BEARER_HAS_REALM) &&
		testHttpResponseFixtureText(Challenge.Realm, "legacy"),
		"HTTP response second Bearer challenge mismatch"
	);
	testRequire(
		xrtHttpResponseBearerChallengeNext(
			pResponse,
			&Cursor,
			Output,
			sizeof(Output),
			&iSize,
			&Challenge
		) == XHTTP_NEXT_END,
		"HTTP response Bearer challenge iterator did not end"
	);

	/* Proxy-Authenticate 使用独立入口和相同标准参数语义。 */
	xrtHttpAuthCursorInit(&Cursor);
	testRequire(
		(xrtHttpResponseProxyBearerChallengeNext(
			pResponse,
			&Cursor,
			Output,
			sizeof(Output),
			&iSize,
			&Challenge
		) == XHTTP_NEXT_ITEM) &&
		testHttpResponseFixtureText(
			Challenge.Error, "insufficient_scope"
		) && testHttpResponseFixtureText(Challenge.Scope, "admin"),
		"HTTP response proxy Bearer challenge mismatch"
	);

	/* 公开固定描述符允许未对齐存储，结果仍借用调用方输出区。 */
	xrtHttpAuthCursorInit(&Cursor);
	memcpy(CursorStorage + 1u, &Cursor, sizeof(Cursor));
	testRequire(
		xrtHttpResponseBearerChallengeNext(
			pResponse,
			(xhttpauthcursor*)(void*)(CursorStorage + 1u),
			Output,
			sizeof(Output),
			(size_t*)(void*)(SizeStorage + 1u),
			(xhttpbearerchallenge*)(void*)(ChallengeStorage + 1u)
		) == XHTTP_NEXT_ITEM,
		"HTTP response Bearer unaligned decode failed"
	);
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	memcpy(
		&Challenge,
		ChallengeStorage + 1u,
		sizeof(Challenge)
	);
	testRequire(
		(iSize == iExpected) &&
		testHttpResponseFixtureText(Challenge.Realm, "api") &&
		testHttpResponseFixtureText(Challenge.Error, "invalid_token"),
		"HTTP response Bearer unaligned result mismatch"
	);
	xrtHttpResponseDestroy(pResponse);

	/* 短缓冲和语义错误均保留当前匹配项供调用方重试或诊断。 */
	pResponse = testHttpResponseFixtureCreate(ShortField, 1u);
	xrtHttpAuthCursorInit(&Cursor);
	Saved = Cursor;
	memset(Output, 0xA5, sizeof(Output));
	testRequire(
		(xrtHttpResponseBearerChallengeNext(
			pResponse,
			&Cursor,
			Output,
			2u,
			&iSize,
			&Challenge
		) == XHTTP_NEXT_ERROR) &&
		(iSize == 6u) && (Challenge.Flags == 0u) &&
		((unsigned char)Output[0] == 0xA5u) &&
		testHttpResponseBearerCursorEqual(&Cursor, &Saved),
		"HTTP response Bearer short buffer was not atomic"
	);
	testHttpResponseFixtureError(
		XERR_RANGE,
		XHTTP_RESPONSE_ERROR_AUTH,
		"HTTP response Bearer short buffer error mismatch"
	);
	xrtHttpResponseDestroy(pResponse);

	pResponse = testHttpResponseFixtureCreate(Malformed, 1u);
	xrtHttpAuthCursorInit(&Cursor);
	Saved = Cursor;
	testRequire(
		(xrtHttpResponseBearerChallengeNext(
			pResponse,
			&Cursor,
			Output,
			sizeof(Output),
			&iSize,
			&Challenge
		) == XHTTP_NEXT_ERROR) &&
		testHttpResponseBearerCursorEqual(&Cursor, &Saved),
		"HTTP response malformed Bearer challenge advanced cursor"
	);
	testHttpResponseFixtureError(
		XERR_VALUE,
		XHTTP_RESPONSE_ERROR_AUTH,
		"HTTP response malformed Bearer challenge error mismatch"
	);
	xrtHttpResponseDestroy(pResponse);
	puts("[PASS] HTTP client response Bearer authentication");
	return 0;
}
