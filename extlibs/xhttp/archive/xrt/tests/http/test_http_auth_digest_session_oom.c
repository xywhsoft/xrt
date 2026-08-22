#include "../test.h"

#include <xrt/http_auth.h>
#include <xrt/memory_debug.h>



/* 初始化 OOM 测试使用的固定会话配置。 */
static void testHttpDigestSessionOomConfig(
	xhttpdigestchallenge* pChallenge,
	xhttpdigestchoice* pChoice,
	xhttpdigestsessionconfig* pConfig,
	char Secret[XRT_HTTP_DIGEST_MAX_TEXT_SIZE],
	size_t* pSecretSize
)
{
	*pChallenge = (xhttpdigestchallenge){
		XHTTP_DIGEST_CHALLENGE_QOP_AUTH |
		XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT,
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XRT_STR_INIT("api"),
		{ NULL, 0 },
		XRT_STR_INIT("nonce"),
		{ NULL, 0 },
		{ NULL, 0 }
	};
	*pChoice = (xhttpdigestchoice){
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XHTTP_DIGEST_QOP_AUTH,
		false
	};
	testRequire(xrtHttpDigestSecret(
		pChoice->Algorithm,
		XRT_STR_LITERAL("user"),
		pChallenge->Realm,
		XRT_STR_LITERAL("password"),
		Secret,
		XRT_HTTP_DIGEST_MAX_TEXT_SIZE,
		pSecretSize
	), "HTTP Digest session OOM secret failed");
	*pConfig = (xhttpdigestsessionconfig){
		0,
		pChallenge,
		pChoice,
		XRT_STR_INIT("user"),
		{ NULL, 0 },
		{ Secret, *pSecretSize },
		XRT_STR_INIT("client")
	};
}



/* 为 Accept OOM 测试构造一份带 nextnonce 的有效响应证明。 */
static xhttpdigestinfo testHttpDigestSessionOomInfo(
	const xhttpdigestexchange* pExchange,
	char Response[XRT_HTTP_DIGEST_MAX_TEXT_SIZE]
)
{
	const xhttpdigestproof* pProof =
		xrtHttpDigestExchangeProof(pExchange);
	size_t iSize;

	testRequire((pProof != NULL) && xrtHttpDigestRspAuth(
		pProof,
		Response,
		XRT_HTTP_DIGEST_MAX_TEXT_SIZE,
		&iSize
	), "HTTP Digest session OOM rspauth failed");
	return (xhttpdigestinfo){
		XHTTP_DIGEST_INFO_HAS_NEXT_NONCE |
		XHTTP_DIGEST_INFO_HAS_RESPONSE,
		pProof->Algorithm,
		pProof->Qop,
		pProof->NonceCount,
		XRT_STR_INIT("next"),
		{ Response, iSize },
		pProof->Cnonce
	};
}



/* 扫描会话创建的每个逻辑分配点，并验证失败路径无泄漏。 */
static void testHttpDigestSessionCreateOom(void)
{
	xhttpdigestchallenge Challenge;
	xhttpdigestchoice Choice;
	xhttpdigestsessionconfig Config;
	char Secret[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSecretSize;
	bool bComplete = false;
	size_t iFailures = 0;

	testHttpDigestSessionOomConfig(
		&Challenge, &Choice, &Config, Secret, &iSecretSize
	);
	for ( size_t i = 0; i < 8u; i++ ) {
		xhttpdigestsession* pSession;
		bool bTriggered;

		testRequire(xrtMemDebugFailAfter((uint64)i),
			"HTTP Digest session create OOM setup failed");
		pSession = xrtHttpDigestSessionCreate(&Config);
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( pSession == NULL ) {
			testRequire(bTriggered &&
				(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
				"HTTP Digest session create failed without injected OOM");
			iFailures++;
		} else {
			testRequire(!bTriggered,
				"HTTP Digest session create ignored allocation fault");
			xrtHttpDigestSessionRelease(pSession);
			bComplete = true;
		}
		xrtClearError();
		testMemoryDebugDrain(
			"HTTP Digest session create OOM leaked storage"
		);
		if ( bComplete ) {
			break;
		}
	}
	testRequire(bComplete && (iFailures == 2u),
		"HTTP Digest session create allocation count changed");
}



/* 验证授权 OOM 不消耗 nonce-count，且对象没有固定 8K 缓冲。 */
static void testHttpDigestSessionAuthorizeOom(void)
{
	xhttpdigestchallenge Challenge;
	xhttpdigestchoice Choice;
	xhttpdigestsessionconfig Config;
	xhttpdigestsession* pSession;
	xhttpdigestexchange* pExchange;
	const xhttpdigestproof* pProof;
	xmemdebugsnapshot Snapshot;
	char Secret[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSecretSize;

	testHttpDigestSessionOomConfig(
		&Challenge, &Choice, &Config, Secret, &iSecretSize
	);
	pSession = xrtHttpDigestSessionCreate(&Config);
	testRequire(pSession != NULL,
		"HTTP Digest session authorize OOM fixture failed");
	testRequire(xrtMemDebugFailAfter(0),
		"HTTP Digest session authorize OOM setup failed");
	pExchange = xrtHttpDigestSessionAuthorize(
		pSession,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/small"),
		(xstrview){ NULL, 0 }
	);
	testRequire((pExchange == NULL) &&
		xrtMemDebugFailTriggered() &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP Digest session authorize ignored injected OOM");
	xrtMemDebugFailClear();
	xrtClearError();
	pExchange = xrtHttpDigestSessionAuthorize(
		pSession,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/small"),
		(xstrview){ NULL, 0 }
	);
	pProof = xrtHttpDigestExchangeProof(pExchange);
	testRequire((pProof != NULL) && (pProof->NonceCount == 1u),
		"HTTP Digest session authorize OOM consumed nonce-count");
	xrtMemDebugSnapshot(&Snapshot);
	testRequire((Snapshot.LiveCount == 3u) &&
		(Snapshot.LiveBytes < 2048u),
		"HTTP Digest session reintroduced a fixed per-object buffer");
	xrtHttpDigestExchangeRelease(pExchange);
	xrtHttpDigestSessionRelease(pSession);
	testMemoryDebugDrain(
		"HTTP Digest session authorize OOM leaked storage"
	);
}



/* 验证 Update 和 Accept 的分配失败都保持旧状态，可由调用方重试。 */
static void testHttpDigestSessionTransitionOom(void)
{
	xhttpdigestchallenge Challenge;
	xhttpdigestchallenge UpdatedChallenge;
	xhttpdigestchoice Choice;
	xhttpdigestsessionconfig Config;
	xhttpdigestsessionconfig UpdatedConfig;
	xhttpdigestsession* pSession;
	xhttpdigestexchange* pExchange;
	xhttpdigestexchange* pCurrent;
	xhttpdigestinfo Info;
	const xhttpdigestproof* pProof;
	char Response[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	char Secret[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSecretSize;

	testHttpDigestSessionOomConfig(
		&Challenge, &Choice, &Config, Secret, &iSecretSize
	);
	pSession = xrtHttpDigestSessionCreate(&Config);
	UpdatedChallenge = Challenge;
	UpdatedChallenge.Nonce = XRT_STR_LITERAL("updated");
	UpdatedConfig = Config;
	UpdatedConfig.Challenge = &UpdatedChallenge;
	testRequire(xrtMemDebugFailAfter(0),
		"HTTP Digest session update OOM setup failed");
	testRequire(!xrtHttpDigestSessionUpdate(
		pSession, &UpdatedConfig
	) && xrtMemDebugFailTriggered(),
		"HTTP Digest session update ignored injected OOM");
	xrtMemDebugFailClear();
	xrtClearError();
	pExchange = xrtHttpDigestSessionAuthorize(
		pSession,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/transition"),
		(xstrview){ NULL, 0 }
	);
	pProof = xrtHttpDigestExchangeProof(pExchange);
	testRequire((pProof != NULL) &&
		(pProof->NonceCount == 1u) &&
		(pProof->Nonce.Size == 5u) &&
		(memcmp(pProof->Nonce.Data, "nonce", 5u) == 0),
		"HTTP Digest session update OOM changed state");
	Info = testHttpDigestSessionOomInfo(pExchange, Response);
	testRequire(xrtMemDebugFailAfter(0),
		"HTTP Digest session accept OOM setup failed");
	testRequire(xrtHttpDigestSessionAccept(
		pSession,
		pExchange,
		&Info,
		(xstrview){ NULL, 0 },
		(xstrview){ NULL, 0 }
	) == XHTTP_DIGEST_SESSION_ERROR &&
		xrtMemDebugFailTriggered(),
		"HTTP Digest session accept ignored injected OOM");
	xrtMemDebugFailClear();
	xrtClearError();
	testRequire(xrtHttpDigestSessionAccept(
		pSession,
		pExchange,
		&Info,
		(xstrview){ NULL, 0 },
		(xstrview){ NULL, 0 }
	) == XHTTP_DIGEST_SESSION_UPDATED,
		"HTTP Digest session accept could not retry after OOM");
	pCurrent = xrtHttpDigestSessionAuthorize(
		pSession,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/next"),
		(xstrview){ NULL, 0 }
	);
	pProof = xrtHttpDigestExchangeProof(pCurrent);
	testRequire((pProof != NULL) &&
		(pProof->NonceCount == 1u) &&
		(pProof->Nonce.Size == 4u) &&
		(memcmp(pProof->Nonce.Data, "next", 4u) == 0),
		"HTTP Digest session accept retry state mismatch");
	xrtHttpDigestExchangeRelease(pExchange);
	xrtHttpDigestExchangeRelease(pCurrent);
	xrtHttpDigestSessionRelease(pSession);
	testMemoryDebugDrain(
		"HTTP Digest session transition OOM leaked storage"
	);
}



int main(void)
{
	testHttpDigestSessionCreateOom();
	testHttpDigestSessionAuthorizeOom();
	testHttpDigestSessionTransitionOom();
	puts("[PASS] HTTP Digest client session OOM");
	return 0;
}
