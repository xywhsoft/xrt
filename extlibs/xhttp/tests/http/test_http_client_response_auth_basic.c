#include "http_client_response_fixture.h"



/* 验证两个 challenge 游标按字节完全相同。 */
static bool testHttpResponseBasicCursorEqual(
	const xhttpauthcursor* pLeft,
	const xhttpauthcursor* pRight
)
{
	return memcmp(pLeft, pRight, sizeof(*pLeft)) == 0;
}



/* 验证客户端响应 Basic challenge 的过滤、查询和解码契约。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("WWW-Authenticate"),
			XRT_STR_INIT(
				"Digest realm=\"skip\", "
				"Basic realm=\"api\", charset=UTF-8"
			)
		},
		{
			XRT_STR_INIT("WWW-Authenticate"),
			XRT_STR_INIT("Bearer realm=\"skip\", Basic realm=legacy")
		},
		{
			XRT_STR_INIT("Proxy-Authenticate"),
			XRT_STR_INIT("Digest realm=\"skip\", Basic realm=\"proxy\"")
		}
	};
	static const xhttpfield ShortField[] = {
		{
			XRT_STR_INIT("WWW-Authenticate"),
			XRT_STR_INIT("Basic realm=\"abcdef\"")
		}
	};
	static const xhttpfield Malformed[] = {
		{
			XRT_STR_INIT("WWW-Authenticate"),
			XRT_STR_INIT("Digest realm=\"skip\", Basic charset=UTF-8")
		}
	};
	xhttpresponse* pResponse = testHttpResponseFixtureCreate(
		Fields,
		sizeof(Fields) / sizeof(Fields[0])
	);
	xhttpauthcursor Cursor;
	xhttpauthcursor Saved;
	xhttpbasicchallenge Challenge;
	unsigned char CursorStorage[sizeof(xhttpauthcursor) + 1u];
	unsigned char SizeStorage[sizeof(size_t) + 1u];
	unsigned char ChallengeStorage[sizeof(xhttpbasicchallenge) + 1u];
	char Output[16];
	size_t iSize;

	/* 查询完整验证当前 Basic，但把游标保留给精确缓冲重试。 */
	xrtHttpAuthCursorInit(&Cursor);
	Saved = Cursor;
	testRequire(
		(xrtHttpResponseBasicChallengeNext(
			pResponse,
			&Cursor,
			NULL,
			0,
			&iSize,
			&Challenge
		) == XHTTP_NEXT_ITEM) &&
		(iSize == 3u) && Challenge.Utf8 &&
		(Challenge.Realm.Data == NULL) &&
		testHttpResponseBasicCursorEqual(&Cursor, &Saved),
		"HTTP response Basic challenge query contract mismatch"
	);
	testRequire(
		(xrtHttpResponseBasicChallengeNext(
			pResponse,
			&Cursor,
			Output,
			sizeof(Output),
			&iSize,
			&Challenge
		) == XHTTP_NEXT_ITEM) &&
		(iSize == 3u) && Challenge.Utf8 &&
		testHttpResponseFixtureText(Challenge.Realm, "api"),
		"HTTP response Basic challenge decode mismatch"
	);
	testRequire(
		(xrtHttpResponseBasicChallengeNext(
			pResponse,
			&Cursor,
			Output,
			sizeof(Output),
			&iSize,
			&Challenge
		) == XHTTP_NEXT_ITEM) &&
		!Challenge.Utf8 &&
		testHttpResponseFixtureText(Challenge.Realm, "legacy"),
		"HTTP response second Basic challenge mismatch"
	);
	testRequire(
		xrtHttpResponseBasicChallengeNext(
			pResponse,
			&Cursor,
			Output,
			sizeof(Output),
			&iSize,
			&Challenge
		) == XHTTP_NEXT_END,
		"HTTP response Basic challenge iterator did not end"
	);

	/* 代理字段独立过滤，不受源站游标遍历结果影响。 */
	xrtHttpAuthCursorInit(&Cursor);
	testRequire(
		(xrtHttpResponseProxyBasicChallengeNext(
			pResponse,
			&Cursor,
			Output,
			sizeof(Output),
			&iSize,
			&Challenge
		) == XHTTP_NEXT_ITEM) &&
		testHttpResponseFixtureText(Challenge.Realm, "proxy"),
		"HTTP response proxy Basic challenge mismatch"
	);

	/* 公开固定描述符允许未对齐存储，内部只通过快照读取和发布。 */
	xrtHttpAuthCursorInit(&Cursor);
	memcpy(CursorStorage + 1u, &Cursor, sizeof(Cursor));
	testRequire(
		xrtHttpResponseBasicChallengeNext(
			pResponse,
			(xhttpauthcursor*)(void*)(CursorStorage + 1u),
			Output,
			sizeof(Output),
			(size_t*)(void*)(SizeStorage + 1u),
			(xhttpbasicchallenge*)(void*)(ChallengeStorage + 1u)
		) == XHTTP_NEXT_ITEM,
		"HTTP response Basic unaligned decode failed"
	);
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	memcpy(
		&Challenge,
		ChallengeStorage + 1u,
		sizeof(Challenge)
	);
	testRequire(
		(iSize == 3u) && Challenge.Utf8 &&
		testHttpResponseFixtureText(Challenge.Realm, "api"),
		"HTTP response Basic unaligned result mismatch"
	);
	xrtHttpResponseDestroy(pResponse);

	/* 短缓冲只发布所需长度，输出与游标都保持可重试。 */
	pResponse = testHttpResponseFixtureCreate(ShortField, 1u);
	xrtHttpAuthCursorInit(&Cursor);
	Saved = Cursor;
	memset(Output, 0xA5, sizeof(Output));
	memset(&Challenge, 0xA5, sizeof(Challenge));
	testRequire(
		(xrtHttpResponseBasicChallengeNext(
			pResponse,
			&Cursor,
			Output,
			2u,
			&iSize,
			&Challenge
		) == XHTTP_NEXT_ERROR) &&
		(iSize == 6u) &&
		(Challenge.Realm.Data == NULL) &&
		(Challenge.Realm.Size == 0u) &&
		!Challenge.Utf8 &&
		((unsigned char)Output[0] == 0xA5u) &&
		testHttpResponseBasicCursorEqual(&Cursor, &Saved),
		"HTTP response Basic short buffer was not atomic"
	);
	testHttpResponseFixtureError(
		XERR_RANGE,
		XHTTP_RESPONSE_ERROR_AUTH,
		"HTTP response Basic short buffer error mismatch"
	);
	xrtHttpResponseDestroy(pResponse);

	/* 畸形匹配 challenge 不得被跳过，也不得推进调用方游标。 */
	pResponse = testHttpResponseFixtureCreate(Malformed, 1u);
	xrtHttpAuthCursorInit(&Cursor);
	Saved = Cursor;
	testRequire(
		(xrtHttpResponseBasicChallengeNext(
			pResponse,
			&Cursor,
			Output,
			sizeof(Output),
			&iSize,
			&Challenge
		) == XHTTP_NEXT_ERROR) &&
		testHttpResponseBasicCursorEqual(&Cursor, &Saved),
		"HTTP response malformed Basic challenge advanced cursor"
	);
	testHttpResponseFixtureError(
		XERR_VALUE,
		XHTTP_RESPONSE_ERROR_AUTH,
		"HTTP response malformed Basic challenge error mismatch"
	);
	xrtHttpResponseDestroy(pResponse);
	puts("[PASS] HTTP client response Basic authentication");
	return 0;
}
