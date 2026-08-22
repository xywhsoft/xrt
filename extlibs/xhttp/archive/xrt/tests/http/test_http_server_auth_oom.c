#include "http_server_request_fixture.h"
#include "../test.h"



/* 验证服务端 Basic 解码 OOM 不写入部分明文。 */
static void testHttpServerBasicReadOom(
	const xhttpserverrequest* pRequest
)
{
	unsigned char Output[300];
	unsigned char Before[300];
	xhttpbasicauth Basic;
	size_t iSize = 91u;
	bool bTriggered;

	memset(Output, 0xA5, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	testRequire(
		xrtMemDebugFailAfter(0),
		"HTTP server Basic decode OOM setup failed"
	);
	testRequire((xrtHttpServerRequestBasicAuth(
		pRequest,
		Output,
		sizeof(Output),
		&iSize,
		&Basic
	) == XHTTP_NEXT_ERROR) &&
		(iSize == 91u) &&
		(Basic.User.Data == NULL) &&
		(Basic.Password.Data == NULL) &&
		(memcmp(Output, Before, sizeof(Output)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP server Basic decode OOM was not atomic");
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	testRequire(
		bTriggered,
		"HTTP server Basic decode did not reach injected OOM"
	);
	xrtClearError();
}



/* 验证 challenge 临时值和 Header 容器 OOM 都不提交字段。 */
static void testHttpServerReplyAuthOom(
	xhttpreply* pLarge,
	xhttpreply* pBasic,
	xhttpreply* pBearer,
	const char* sLong,
	size_t iLong
)
{
	xhttpbearerchallenge Challenge = {
		XHTTP_BEARER_HAS_REALM,
		{ sLong, iLong },
		{ NULL, 0 }, { NULL, 0 }, { NULL, 0 }, { NULL, 0 }
	};
	bool bTriggered;

	testRequire(
		xrtMemDebugFailAfter(0),
		"HTTP server large challenge OOM setup failed"
	);
	testRequire(!xrtHttpReplyAddChallenge(
		pLarge,
		XRT_STR_LITERAL("Bearer"),
		(xstrview){ sLong, iLong }
	) && (xrtHttpReplyHeaderCount(pLarge) == 0u) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP server large challenge OOM changed Reply");
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	testRequire(
		bTriggered,
		"HTTP server large challenge did not reach injected OOM"
	);
	xrtClearError();
	testRequire(
		xrtMemDebugFailAfter(0),
		"HTTP server Basic challenge OOM setup failed"
	);
	testRequire(!xrtHttpReplyAddBasicChallenge(
		pBasic,
		XRT_STR_LITERAL("private"),
		true
	) && (xrtHttpReplyHeaderCount(pBasic) == 0u) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP server Basic challenge OOM changed Reply");
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	testRequire(
		bTriggered,
		"HTTP server Basic challenge did not reach injected OOM"
	);
	xrtClearError();
	testRequire(
		xrtMemDebugFailAfter(0),
		"HTTP server Bearer challenge OOM setup failed"
	);
	testRequire(!xrtHttpReplyAddBearerChallenge(
		pBearer,
		&Challenge
	) && (xrtHttpReplyHeaderCount(pBearer) == 0u) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP server Bearer challenge OOM changed Reply");
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	testRequire(
		bTriggered,
		"HTTP server Bearer challenge did not reach injected OOM"
	);
	xrtClearError();
}



int main(void)
{
	static const char Prefix[] =
		"GET / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Authorization: Basic ";
	char Plain[281];
	char Head[512];
	char Long[300];
	xhttpserverrequest* pRequest;
	xhttpreply* pLarge = xrtHttpReplyCreate(
		XHTTP_STATUS_UNAUTHORIZED
	);
	xhttpreply* pBasic = xrtHttpReplyCreate(
		XHTTP_STATUS_UNAUTHORIZED
	);
	xhttpreply* pBearer = xrtHttpReplyCreate(
		XHTTP_STATUS_UNAUTHORIZED
	);
	size_t iHead = sizeof(Prefix) - 1u;
	size_t iEncoded;

	memset(Plain, 'u', 180u);
	Plain[180] = ':';
	memset(Plain + 181u, 'p', 100u);
	memcpy(Head, Prefix, iHead);
	testRequire(xrtBase64Encode(
		Plain,
		sizeof(Plain),
		Head + iHead,
		sizeof(Head) - iHead,
		&iEncoded,
		NULL
	), "HTTP server Basic OOM fixture encoding failed");
	iHead += iEncoded;
	memcpy(Head + iHead, "\r\n\r\n", 5u);
	pRequest = testHttpServerRequestFixtureCreate(
		Head,
		(xbytesview){ NULL, 0 },
		XHTTP_SERVER_REQUEST_NONE
	);
	memset(Long, 'a', sizeof(Long));
	testRequire((pRequest != NULL) &&
		(pLarge != NULL) && (pBasic != NULL) &&
		(pBearer != NULL),
		"HTTP server authentication OOM setup failed");
	testHttpServerBasicReadOom(pRequest);
	testHttpServerReplyAuthOom(
		pLarge, pBasic, pBearer, Long, sizeof(Long)
	);
	xrtHttpReplyDestroy(pBearer);
	xrtHttpReplyDestroy(pBasic);
	xrtHttpReplyDestroy(pLarge);
	xrtHttpServerRequestDestroy(pRequest);
	testMemoryDebugDrain(
		"HTTP server authentication OOM leaked memory"
	);
	puts("[PASS] HTTP server authentication OOM");
	return 0;
}
