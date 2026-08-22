#include "../test.h"



/* 计算完整 HTTP/1 Header 中一段固定字节出现的次数。 */
static size_t testHttpDigestPrepareCount(
	xbytesview Head,
	cstr sNeedle
)
{
	size_t iNeedle = strlen(sNeedle);
	size_t iCount = 0;

	if ( iNeedle > Head.Size ) {
		return 0;
	}
	for ( size_t i = 0; i <= (Head.Size - iNeedle); i++ ) {
		if ( memcmp(Head.Data + i, sNeedle, iNeedle) == 0 ) {
			iCount++;
		}
	}
	return iCount;
}



/* 创建测试使用的独立 Digest 会话。 */
static xhttpdigestsession* testHttpDigestPrepareSession(void)
{
	xhttpdigestchallenge Challenge = {
		XHTTP_DIGEST_CHALLENGE_QOP_AUTH |
		XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT,
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XRT_STR_INIT("api"),
		{ NULL, 0 },
		XRT_STR_INIT("server-nonce"),
		{ NULL, 0 },
		{ NULL, 0 }
	};
	xhttpdigestchoice Choice = {
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XHTTP_DIGEST_QOP_AUTH,
		false
	};
	xhttpdigestsessionconfig Config;
	char Secret[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSize;

	testRequire(xrtHttpDigestSecret(
		Choice.Algorithm,
		XRT_STR_LITERAL("user"),
		Challenge.Realm,
		XRT_STR_LITERAL("password"),
		Secret,
		sizeof(Secret),
		&iSize
	), "HTTP Digest prepare secret failed");
	Config = (xhttpdigestsessionconfig){
		0,
		&Challenge,
		&Choice,
		XRT_STR_INIT("user"),
		{ NULL, 0 },
		{ Secret, iSize },
		XRT_STR_INIT("client")
	};
	return xrtHttpDigestSessionCreate(&Config);
}



/* 验证源站认证绑定 origin-form target 且不修改请求构建器。 */
static void testHttpDigestPrepareOrigin(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://example.test/a?b=1#fragment")
	);
	xhttpdigestsession* pSession = testHttpDigestPrepareSession();
	xhttpdigestexchange* pExchange = NULL;
	xhttp1requestplan* pPlan;
	const xhttpdigestproof* pProof;
	xbytesview Head;

	testRequire((pRequest != NULL) && (pSession != NULL) &&
		xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Authorization"),
			XRT_STR_LITERAL("Old source")
		) && xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Proxy-Authorization"),
			XRT_STR_LITERAL("Old proxy")
		), "HTTP Digest origin prepare fixture failed");
	pPlan = xrtHttp1RequestPrepareDigest(
		pRequest,
		NULL,
		pSession,
		(xstrview){ NULL, 0 },
		&pExchange
	);
	pProof = xrtHttpDigestExchangeProof(pExchange);
	Head = xrtHttp1RequestPlanHead(pPlan);
	testRequire((pPlan != NULL) && (pProof != NULL) &&
		(pProof->NonceCount == 1u) &&
		(pProof->Uri.Size == 6u) &&
		(memcmp(pProof->Uri.Data, "/a?b=1", 6u) == 0) &&
		(testHttpDigestPrepareCount(
			Head, "Authorization: Digest "
		) == 1u) &&
		(testHttpDigestPrepareCount(
			Head, "Authorization: Old source"
		) == 0u) &&
		(testHttpDigestPrepareCount(
			Head, "Proxy-Authorization: Old proxy"
		) == 1u) &&
		(xrtHttpRequestHeaderCount(pRequest) == 2u),
		"HTTP Digest origin prepare mismatch");
	xrtHttp1RequestPlanDestroy(pPlan);
	xrtHttpDigestExchangeRelease(pExchange);
	xrtHttpDigestSessionRelease(pSession);
	xrtHttpRequestDestroy(pRequest);
}



/* 验证代理认证使用实际 absolute-form target，并只替换代理字段。 */
static void testHttpDigestPrepareProxy(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://example.test/a?b=1#fragment")
	);
	xhttpdigestsession* pSession = testHttpDigestPrepareSession();
	xhttpdigestexchange* pExchange = NULL;
	xhttp1requestoptions Options;
	xhttp1requestplan* pPlan;
	const xhttpdigestproof* pProof;
	xbytesview Head;

	xrtHttp1RequestOptionsInit(&Options);
	Options.TargetForm = XHTTP1_TARGET_ABSOLUTE;
	testRequire((pRequest != NULL) && (pSession != NULL) &&
		xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Authorization"),
			XRT_STR_LITERAL("Old source")
		) && xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Proxy-Authorization"),
			XRT_STR_LITERAL("Old proxy")
		), "HTTP Digest proxy prepare fixture failed");
	pPlan = xrtHttp1RequestPrepareProxyDigest(
		pRequest,
		&Options,
		pSession,
		(xstrview){ NULL, 0 },
		&pExchange
	);
	pProof = xrtHttpDigestExchangeProof(pExchange);
	Head = xrtHttp1RequestPlanHead(pPlan);
	testRequire((pPlan != NULL) && (pProof != NULL) &&
		(pProof->Uri.Size == 25u) &&
		(memcmp(
			pProof->Uri.Data,
			"http://example.test/a?b=1",
			25u
		) == 0) &&
		(testHttpDigestPrepareCount(
			Head, "Proxy-Authorization: Digest "
		) == 1u) &&
		(testHttpDigestPrepareCount(
			Head, "Proxy-Authorization: Old proxy"
		) == 0u) &&
		(testHttpDigestPrepareCount(
			Head, "Authorization: Old source"
		) == 1u),
		"HTTP Digest proxy prepare mismatch");
	xrtHttp1RequestPlanDestroy(pPlan);
	xrtHttpDigestExchangeRelease(pExchange);
	xrtHttpDigestSessionRelease(pSession);
	xrtHttpRequestDestroy(pRequest);
}



/* 验证 Exchange 输出槽允许未对齐地址，并且仍能发布完整对象。 */
static void testHttpDigestPrepareUnalignedOutput(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/path")
	);
	xhttpdigestsession* pSession = testHttpDigestPrepareSession();
	unsigned char Storage[sizeof(xhttpdigestexchange*) + 1u];
	xhttpdigestexchange** ppExchange =
		(xhttpdigestexchange**)(void*)(Storage + 1u);
	xhttpdigestexchange* pExchange = NULL;
	xhttp1requestplan* pPlan;
	const xhttpdigestproof* pProof;

	testRequire((pRequest != NULL) && (pSession != NULL),
		"HTTP Digest unaligned prepare fixture failed");
	pPlan = xrtHttp1RequestPrepareDigest(
		pRequest,
		NULL,
		pSession,
		(xstrview){ NULL, 0 },
		ppExchange
	);
	memcpy(&pExchange, ppExchange, sizeof(pExchange));
	pProof = xrtHttpDigestExchangeProof(pExchange);
	testRequire((pPlan != NULL) && (pProof != NULL) &&
		(pProof->NonceCount == 1u),
		"HTTP Digest unaligned prepare output mismatch");
	xrtHttp1RequestPlanDestroy(pPlan);
	xrtHttpDigestExchangeRelease(pExchange);
	xrtHttpDigestSessionRelease(pSession);
	xrtHttpRequestDestroy(pRequest);
}



/* 验证准备前错误不发布 Exchange，也不消耗 nonce-count。 */
static void testHttpDigestPrepareFailure(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/path")
	);
	xhttpdigestsession* pSession = testHttpDigestPrepareSession();
	xhttpdigestexchange* pExchange =
		(xhttpdigestexchange*)(uintptr_t)1u;
	xhttp1requestoptions Options = {
		(xhttp1targetform)99,
		{ NULL, 0 }
	};
	xhttp1requestplan* pPlan;
	const xhttpdigestproof* pProof;

	testRequire(xrtHttpRequestSetBytes(
		pRequest,
		XRT_BYTES_LITERAL("body"),
		(xstrview){ NULL, 0 }
	), "HTTP Digest prepare body fixture failed");
	testRequire(xrtHttp1RequestPrepareDigest(
		pRequest,
		&Options,
		pSession,
		(xstrview){ NULL, 0 },
		&pExchange
	) == NULL && (pExchange == NULL),
		"HTTP Digest prepare failure published an exchange");
	xrtClearError();
	testRequire(xrtHttp1RequestPrepareDigest(
		pRequest,
		NULL,
		pSession,
		(xstrview){ NULL, 0 },
		(xhttpdigestexchange**)(void*)
			xrtHttpRequestMethod(pRequest).Data
	) == NULL,
		"HTTP Digest prepare accepted Method output");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Digest prepare Method output error mismatch");
	xrtClearError();
	testRequire(xrtHttp1RequestPrepareDigest(
		pRequest,
		NULL,
		pSession,
		(xstrview){ NULL, 0 },
		(xhttpdigestexchange**)(void*)
			xrtHttpRequestHeaders(pRequest)
	) == NULL,
		"HTTP Digest prepare accepted Header container output");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Digest prepare Header output error mismatch");
	xrtClearError();
	testRequire(xrtHttp1RequestPrepareDigest(
		pRequest,
		NULL,
		pSession,
		(xstrview){ NULL, 0 },
		(xhttpdigestexchange**)(void*)
			xrtHttpRequestBody(pRequest)
	) == NULL,
		"HTTP Digest prepare accepted Body object output");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Digest prepare Body output error mismatch");
	xrtClearError();
	testRequire(xrtHttp1RequestPrepareDigest(
		pRequest,
		NULL,
		pSession,
		(xstrview){ NULL, 0 },
		(xhttpdigestexchange**)(void*)pSession
	) == NULL,
		"HTTP Digest prepare accepted session object output");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Digest prepare session output error mismatch");
	xrtClearError();
	testRequire(xrtHttp1RequestPrepareDigest(
		pRequest,
		NULL,
		pSession,
		(xstrview){ NULL, 0 },
		(xhttpdigestexchange**)(uintptr_t)(UINTPTR_MAX - 1u)
	) == NULL,
		"HTTP Digest prepare accepted wrapping output");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Digest prepare wrapping output error mismatch");
	xrtClearError();
	testRequire(
		xrtHttpTokenEqual(
			xrtHttpRequestMethod(pRequest), XRT_STR_LITERAL("GET")
		) && (xrtHttpRequestHeaderCount(pRequest) == 0u) &&
		(xrtHttpBodyLength(xrtHttpRequestBody(pRequest)) == 4u),
		"HTTP Digest prepare alias damaged request"
	);
	xrtHttp1RequestOptionsInit(&Options);
	Options.TargetForm = XHTTP1_TARGET_CUSTOM;
	Options.CustomTarget = XRT_STR_LITERAL("/signed?raw");
	pPlan = xrtHttp1RequestPrepareDigest(
		pRequest,
		&Options,
		pSession,
		(xstrview){ NULL, 0 },
		&pExchange
	);
	pProof = xrtHttpDigestExchangeProof(pExchange);
	testRequire((pPlan != NULL) && (pProof != NULL) &&
		(pProof->NonceCount == 1u) &&
		(pProof->Uri.Size == 11u) &&
		(memcmp(pProof->Uri.Data, "/signed?raw", 11u) == 0),
		"HTTP Digest custom target prepare mismatch");
	xrtHttp1RequestPlanDestroy(pPlan);
	xrtHttpDigestExchangeRelease(pExchange);
	xrtHttpDigestSessionRelease(pSession);
	xrtHttpRequestDestroy(pRequest);
}



int main(void)
{
	testHttpDigestPrepareOrigin();
	testHttpDigestPrepareProxy();
	testHttpDigestPrepareUnalignedOutput();
	testHttpDigestPrepareFailure();
	puts("[PASS] HTTP client Digest session prepare");
	return 0;
}
