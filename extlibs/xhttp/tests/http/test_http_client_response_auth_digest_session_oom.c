#include "http_digest_response_session_fixture.h"

#include <xrt/memory_debug.h>



/* 在一个失败序号下验证回执解码与状态更新保持事务性。 */
static bool testHttpDigestResponseSessionOomAttempt(size_t iFail)
{
	xhttpdigestsession* pSession = testHttpDigestResponseSession();
	xhttpdigestexchange* pExchange = NULL;
	xhttpdigestexchange* pCurrent = NULL;
	const xhttpdigestproof* pProof;
	xhttpresponse* pResponse = NULL;
	xhttpdigestsessioncheck Check = XHTTP_DIGEST_SESSION_VALID;
	xhttpnext Next;
	const xerror* pError;
	char Value[512];
	size_t iSize;
	bool bTriggered;
	bool bComplete = false;

	if ( pSession == NULL ) {
		goto Finish;
	}
	pExchange = xrtHttpDigestSessionAuthorize(
		pSession,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/oom"),
		(xstrview){ NULL, 0 }
	);
	if ( pExchange == NULL ) {
		goto Finish;
	}
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
	if ( !xrtMemDebugFailAfter((uint64)iFail) ) {
		goto Finish;
	}
	Next = xrtHttpResponseDigestSessionAccept(
		pResponse,
		pSession,
		pExchange,
		(xstrview){ NULL, 0 },
		(xstrview){ NULL, 0 },
		&Check
	);
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	if ( Next == XHTTP_NEXT_ERROR ) {
		pError = xrtGetError();
		testRequire(bTriggered &&
			(Check == XHTTP_DIGEST_SESSION_ERROR) &&
			(pError != NULL) &&
			(xrtErrorKind(pError) == XERR_MEMORY) &&
			(strcmp(
				xrtErrorDomain(pError),
				"xrt.http.client.response"
			) == 0) &&
			(xrtErrorCode(pError) == XHTTP_RESPONSE_ERROR_AUTH),
			"HTTP Digest response OOM error contract mismatch");
		xrtClearError();
		pCurrent = xrtHttpDigestSessionAuthorize(
			pSession,
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/still-current"),
			(xstrview){ NULL, 0 }
		);
		pProof = xrtHttpDigestExchangeProof(pCurrent);
		testRequire((pProof != NULL) &&
			(pProof->NonceCount == 2u) &&
			testHttpResponseFixtureText(
				pProof->Nonce, "server-nonce"
			), "HTTP Digest response OOM changed session state");
	} else {
		testRequire(!bTriggered &&
			(Next == XHTTP_NEXT_ITEM) &&
			(Check == XHTTP_DIGEST_SESSION_UPDATED),
			"HTTP Digest response ignored allocation fault");
		pCurrent = xrtHttpDigestSessionAuthorize(
			pSession,
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/updated"),
			(xstrview){ NULL, 0 }
		);
		pProof = xrtHttpDigestExchangeProof(pCurrent);
		testRequire((pProof != NULL) &&
			(pProof->NonceCount == 1u) &&
			testHttpResponseFixtureText(
				pProof->Nonce, "next-nonce"
			), "HTTP Digest response OOM success state mismatch");
		bComplete = true;
	}

Finish:
	xrtMemDebugFailClear();
	xrtHttpDigestExchangeRelease(pCurrent);
	xrtHttpResponseDestroy(pResponse);
	xrtHttpDigestExchangeRelease(pExchange);
	xrtHttpDigestSessionRelease(pSession);
	xrtClearError();
	testMemoryDebugDrain(
		"HTTP Digest response OOM leaked storage"
	);
	return bComplete;
}



/* 扫描精确解码缓冲与 nextnonce 状态创建两个分配点。 */
int main(void)
{
	bool bComplete = false;
	size_t iFailures = 0;

	for ( size_t i = 0; i < 8u; i++ ) {
		if ( testHttpDigestResponseSessionOomAttempt(i) ) {
			bComplete = true;
			break;
		}
		iFailures++;
	}
	testRequire(bComplete && (iFailures == 2u),
		"HTTP Digest response allocation count changed");
	puts("[PASS] HTTP client response Digest session OOM");
	return 0;
}
