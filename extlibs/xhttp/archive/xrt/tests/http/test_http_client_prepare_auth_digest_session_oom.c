#include "../test.h"

#include <xrt/http_auth.h>
#include <xrt/http_client.h>
#include <xrt/memory_debug.h>



/* 创建 OOM 扫描使用的 Digest 会话。 */
static xhttpdigestsession* testHttpDigestPrepareOomSession(void)
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
	size_t iSecretSize;

	if ( !xrtHttpDigestSecret(
		Choice.Algorithm,
		XRT_STR_LITERAL("user"),
		Challenge.Realm,
		XRT_STR_LITERAL("password"),
		Secret,
		sizeof(Secret),
		&iSecretSize
	) ) {
		return NULL;
	}
	Config = (xhttpdigestsessionconfig){
		0,
		&Challenge,
		&Choice,
		XRT_STR_INIT("user"),
		{ NULL, 0 },
		{ Secret, iSecretSize },
		XRT_STR_INIT("client")
	};
	return xrtHttpDigestSessionCreate(&Config);
}



/* 在一个失败序号下准备完整请求，并验证输出发布保持原子性。 */
static bool testHttpDigestPrepareOomAttempt(size_t iFail)
{
	xhttprequest* pRequest = NULL;
	xhttpdigestsession* pSession = NULL;
	xhttpdigestexchange* pExchange =
		(xhttpdigestexchange*)(uintptr_t)1u;
	xhttp1requestoptions Options;
	xhttp1requestplan* pPlan = NULL;
	bool bTriggered;
	bool bComplete = false;

	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL(
			"http://example.test/upload?q=1#fragment"
		)
	);
	pSession = testHttpDigestPrepareOomSession();
	if ( (pRequest == NULL) || (pSession == NULL) ||
		!xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Authorization"),
			XRT_STR_LITERAL("Old source credential")
		) || !xrtHttpRequestSetBytes(
			pRequest,
			XRT_BYTES_LITERAL("payload"),
			XRT_STR_LITERAL("application/octet-stream")
		) ) {
		goto Finish;
	}
	xrtHttp1RequestOptionsInit(&Options);
	Options.TargetForm = XHTTP1_TARGET_ABSOLUTE;
	if ( !xrtMemDebugFailAfter((uint64)iFail) ) {
		goto Finish;
	}
	pPlan = xrtHttp1RequestPrepareDigest(
		pRequest,
		&Options,
		pSession,
		(xstrview){ NULL, 0 },
		&pExchange
	);
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	if ( pPlan == NULL ) {
		testRequire(bTriggered && (pExchange == NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
			"HTTP Digest prepare failed without atomic OOM output");
	} else {
		testRequire(!bTriggered && (pExchange != NULL) &&
			(xrtHttpDigestExchangeProof(pExchange) != NULL),
			"HTTP Digest prepare ignored an allocation fault");
		bComplete = true;
	}

Finish:
	xrtMemDebugFailClear();
	xrtHttp1RequestPlanDestroy(pPlan);
	xrtHttpDigestExchangeRelease(pExchange ==
		(xhttpdigestexchange*)(uintptr_t)1u ? NULL : pExchange);
	xrtHttpDigestSessionRelease(pSession);
	xrtHttpRequestDestroy(pRequest);
	xrtClearError();
	testMemoryDebugDrain(
		"HTTP Digest prepare OOM leaked storage"
	);
	return bComplete;
}



/* 扫描适配器、请求冻结和认证字段复制的全部分配点。 */
int main(void)
{
	bool bComplete = false;
	size_t iFailures = 0;

	for ( size_t i = 0; i < 32u; i++ ) {
		if ( testHttpDigestPrepareOomAttempt(i) ) {
			bComplete = true;
			break;
		}
		iFailures++;
	}
	testRequire(bComplete && (iFailures >= 4u),
		"HTTP Digest prepare OOM sweep missed allocation stages");
	puts("[PASS] HTTP client Digest session prepare OOM");
	return 0;
}
