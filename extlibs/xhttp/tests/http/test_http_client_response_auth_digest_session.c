#include "http_digest_response_session_fixture.h"



/* 验证源站回执采用 nextnonce，并保留缺失字段的独立结果。 */
static void testHttpDigestResponseUpdated(void)
{
	xhttpdigestsession* pSession = testHttpDigestResponseSession();
	xhttpdigestexchange* pExchange;
	xhttpdigestexchange* pNext;
	const xhttpdigestproof* pProof;
	xhttpresponse* pResponse;
	xhttpresponse* pMissing;
	xhttpdigestsessioncheck Check = XHTTP_DIGEST_SESSION_INVALID;
	char Value[512];
	size_t iSize;

	testRequire(pSession != NULL,
		"HTTP Digest response updated session failed");
	pExchange = xrtHttpDigestSessionAuthorize(
		pSession,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/private"),
		(xstrview){ NULL, 0 }
	);
	iSize = testHttpDigestResponseValue(
		pExchange,
		XRT_STR_LITERAL("next-nonce"),
		true,
		Value,
		sizeof(Value)
	);
	pResponse = testHttpDigestResponseCreate(
		XRT_STR_LITERAL("Authentication-Info"),
		(xstrview){ Value, iSize }
	);
	testRequire(xrtHttpResponseDigestSessionAccept(
		pResponse,
		pSession,
		pExchange,
		(xstrview){ NULL, 0 },
		(xstrview){ NULL, 0 },
		&Check
	) == XHTTP_NEXT_ITEM &&
		(Check == XHTTP_DIGEST_SESSION_UPDATED),
		"HTTP Digest response did not adopt nextnonce");
	pNext = xrtHttpDigestSessionAuthorize(
		pSession,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/next"),
		(xstrview){ NULL, 0 }
	);
	pProof = xrtHttpDigestExchangeProof(pNext);
	testRequire((pProof != NULL) &&
		(pProof->NonceCount == 1u) &&
		testHttpResponseFixtureText(
			pProof->Nonce, "next-nonce"
		), "HTTP Digest response next state mismatch");
	pMissing = testHttpResponseFixtureCreate(NULL, 0);
	Check = XHTTP_DIGEST_SESSION_VALID;
	testRequire(xrtHttpResponseDigestSessionAccept(
		pMissing,
		pSession,
		pNext,
		(xstrview){ NULL, 0 },
		(xstrview){ NULL, 0 },
		&Check
	) == XHTTP_NEXT_END &&
		(Check == XHTTP_DIGEST_SESSION_ERROR),
		"HTTP Digest response missing info contract mismatch");
	xrtHttpResponseDestroy(pMissing);
	xrtHttpResponseDestroy(pResponse);
	xrtHttpDigestExchangeRelease(pNext);
	xrtHttpDigestExchangeRelease(pExchange);
	xrtHttpDigestSessionRelease(pSession);
}



/* 验证代理入口、未对齐状态输出和乱序响应的已取代语义。 */
static void testHttpDigestResponseProxyAndSuperseded(void)
{
	xhttpdigestsession* pSession = testHttpDigestResponseSession();
	xhttpdigestexchange* pFirst;
	xhttpdigestexchange* pSecond;
	xhttpresponse* pFirstResponse;
	xhttpresponse* pSecondResponse;
	xhttpresponse* pProxyResponse;
	xhttpdigestsessioncheck Check;
	uint8 Storage[sizeof(Check) + 1u];
	char FirstValue[512];
	char SecondValue[512];
	size_t iFirst;
	size_t iSecond;

	pFirst = xrtHttpDigestSessionAuthorize(
		pSession,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/first"),
		(xstrview){ NULL, 0 }
	);
	pSecond = xrtHttpDigestSessionAuthorize(
		pSession,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/second"),
		(xstrview){ NULL, 0 }
	);
	iFirst = testHttpDigestResponseValue(
		pFirst, XRT_STR_LITERAL("first-next"), true,
		FirstValue, sizeof(FirstValue)
	);
	iSecond = testHttpDigestResponseValue(
		pSecond, XRT_STR_LITERAL("second-next"), true,
		SecondValue, sizeof(SecondValue)
	);
	pFirstResponse = testHttpDigestResponseCreate(
		XRT_STR_LITERAL("Authentication-Info"),
		(xstrview){ FirstValue, iFirst }
	);
	pSecondResponse = testHttpDigestResponseCreate(
		XRT_STR_LITERAL("Authentication-Info"),
		(xstrview){ SecondValue, iSecond }
	);
	testRequire(xrtHttpResponseDigestSessionAccept(
		pFirstResponse,
		pSession,
		pFirst,
		(xstrview){ NULL, 0 },
		(xstrview){ NULL, 0 },
		&Check
	) == XHTTP_NEXT_ITEM &&
		(Check == XHTTP_DIGEST_SESSION_UPDATED),
		"HTTP Digest first response update failed");
	testRequire(xrtHttpResponseDigestSessionAccept(
		pSecondResponse,
		pSession,
		pSecond,
		(xstrview){ NULL, 0 },
		(xstrview){ NULL, 0 },
		&Check
	) == XHTTP_NEXT_ITEM &&
		(Check == XHTTP_DIGEST_SESSION_SUPERSEDED),
		"HTTP Digest old response was not superseded");

	pProxyResponse = testHttpDigestResponseCreate(
		XRT_STR_LITERAL("Proxy-Authentication-Info"),
		(xstrview){ FirstValue, iFirst }
	);
	testRequire(xrtHttpResponseProxyDigestSessionAccept(
		pProxyResponse,
		pSession,
		pFirst,
		(xstrview){ NULL, 0 },
		(xstrview){ NULL, 0 },
		(xhttpdigestsessioncheck*)(Storage + 1u)
	) == XHTTP_NEXT_ITEM,
		"HTTP Digest proxy response accept failed");
	memcpy(&Check, Storage + 1u, sizeof(Check));
	testRequire(Check == XHTTP_DIGEST_SESSION_SUPERSEDED,
		"HTTP Digest proxy unaligned result mismatch");
	xrtHttpResponseDestroy(pProxyResponse);
	xrtHttpResponseDestroy(pSecondResponse);
	xrtHttpResponseDestroy(pFirstResponse);
	xrtHttpDigestExchangeRelease(pSecond);
	xrtHttpDigestExchangeRelease(pFirst);
	xrtHttpDigestSessionRelease(pSession);
}



/* 验证错误证明、重复字段、输出别名和地址回绕均不会更新会话。 */
static void testHttpDigestResponseInvalid(void)
{
	xhttpdigestsession* pSession = testHttpDigestResponseSession();
	xhttpdigestexchange* pExchange;
	xhttpresponse* pResponse;
	xhttpresponse* pDuplicate;
	const xhttpfield* pField;
	xhttpdigestsessioncheck Check;
	xhttpfield Fields[2];
	char Value[512];
	size_t iSize;

	pExchange = xrtHttpDigestSessionAuthorize(
		pSession,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/invalid"),
		(xstrview){ NULL, 0 }
	);
	iSize = testHttpDigestResponseValue(
		pExchange,
		XRT_STR_LITERAL("must-not-apply"),
		false,
		Value,
		sizeof(Value)
	);
	pResponse = testHttpDigestResponseCreate(
		XRT_STR_LITERAL("Authentication-Info"),
		(xstrview){ Value, iSize }
	);
	testRequire(xrtHttpResponseDigestSessionAccept(
		pResponse,
		pSession,
		pExchange,
		(xstrview){ NULL, 0 },
		(xstrview){ NULL, 0 },
		&Check
	) == XHTTP_NEXT_ITEM &&
		(Check == XHTTP_DIGEST_SESSION_INVALID) &&
		(xrtGetError() == NULL),
		"HTTP Digest response accepted an invalid proof");
	pField = xrtHttpResponseHeaderAt(pResponse, 0);
	testRequire(xrtHttpResponseDigestSessionAccept(
		pResponse,
		pSession,
		pExchange,
		(xstrview){ NULL, 0 },
		(xstrview){ NULL, 0 },
		(xhttpdigestsessioncheck*)(uintptr_t)pField->Value.Data
	) == XHTTP_NEXT_ERROR,
		"HTTP Digest response accepted overlapping output");
	testHttpResponseFixtureError(
		XERR_ARGUMENT,
		XHTTP_RESPONSE_ERROR_ARGUMENT,
		"HTTP Digest response overlap error mismatch"
	);
	testRequire(xrtHttpResponseDigestSessionAccept(
		pResponse,
		pSession,
		pExchange,
		(xstrview){ NULL, 0 },
		(xstrview){ NULL, 0 },
		(xhttpdigestsessioncheck*)(void*)pSession
	) == XHTTP_NEXT_ERROR,
		"HTTP Digest response accepted session object output");
	testHttpResponseFixtureError(
		XERR_ARGUMENT,
		XHTTP_RESPONSE_ERROR_ARGUMENT,
		"HTTP Digest response session object error mismatch"
	);
	testRequire(xrtHttpResponseDigestSessionAccept(
		pResponse,
		pSession,
		pExchange,
		(xstrview){ NULL, 0 },
		(xstrview){ NULL, 0 },
		(xhttpdigestsessioncheck*)(void*)pExchange
	) == XHTTP_NEXT_ERROR,
		"HTTP Digest response accepted Exchange object output");
	testHttpResponseFixtureError(
		XERR_ARGUMENT,
		XHTTP_RESPONSE_ERROR_ARGUMENT,
		"HTTP Digest response Exchange object error mismatch"
	);
	testRequire(xrtHttpResponseDigestSessionAccept(
		pResponse,
		pSession,
		pExchange,
		(xstrview){
			(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
		},
		(xstrview){ NULL, 0 },
		&Check
	) == XHTTP_NEXT_ERROR,
		"HTTP Digest response accepted a wrapping hash view");
	testHttpResponseFixtureError(
		XERR_ARGUMENT,
		XHTTP_RESPONSE_ERROR_ARGUMENT,
		"HTTP Digest response wrapping error mismatch"
	);
	Fields[0] = (xhttpfield){
		XRT_STR_LITERAL("Authentication-Info"),
		{ Value, iSize }
	};
	Fields[1] = Fields[0];
	pDuplicate = testHttpResponseFixtureCreate(Fields, 2u);
	testRequire(xrtHttpResponseDigestSessionAccept(
		pDuplicate,
		pSession,
		pExchange,
		(xstrview){ NULL, 0 },
		(xstrview){ NULL, 0 },
		&Check
	) == XHTTP_NEXT_ERROR,
		"HTTP Digest response accepted duplicate info");
	testHttpResponseFixtureError(
		XERR_PROTOCOL,
		XHTTP_RESPONSE_ERROR_HEADER,
		"HTTP Digest response duplicate error mismatch"
	);
	xrtHttpResponseDestroy(pDuplicate);
	xrtHttpResponseDestroy(pResponse);
	xrtHttpDigestExchangeRelease(pExchange);
	xrtHttpDigestSessionRelease(pSession);
}



int main(void)
{
	testHttpDigestResponseUpdated();
	testHttpDigestResponseProxyAndSuperseded();
	testHttpDigestResponseInvalid();
	puts("[PASS] HTTP client response Digest session");
	return 0;
}
