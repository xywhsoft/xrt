#include "../test.h"

#include <xrt/http_auth.h>



/* 测试夹具保留创建会话所需的协议描述符和 Secret。 */
typedef struct test_http_digest_session_fixture {
	xhttpdigestchallenge Challenge;
	xhttpdigestchoice Choice;
	xhttpdigestsessionconfig Config;
	char Secret[XHTTP_DIGEST_MAX_TEXT_SIZE];
	size_t SecretSize;
} test_http_digest_session_fixture;



/* 比较公开协议对象中的借用文本。 */
static bool testHttpDigestSessionText(
	xstrview Text,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sExpected, iSize) == 0);
}



/* 初始化一个只启用 SHA-256 auth 的确定性客户端会话配置。 */
static void testHttpDigestSessionFixtureInit(
	test_http_digest_session_fixture* pFixture,
	xstrview Nonce
)
{
	memset(pFixture, 0, sizeof(*pFixture));
	pFixture->Challenge = (xhttpdigestchallenge){
		XHTTP_DIGEST_CHALLENGE_HAS_OPAQUE |
		XHTTP_DIGEST_CHALLENGE_QOP_AUTH |
		XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT,
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XRT_STR_INIT("api"),
		{ NULL, 0 },
		Nonce,
		XRT_STR_INIT("state"),
		{ NULL, 0 }
	};
	pFixture->Choice = (xhttpdigestchoice){
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XHTTP_DIGEST_QOP_AUTH,
		false
	};
	testRequire(xrtHttpDigestSecret(
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XRT_STR_LITERAL("user"),
		XRT_STR_LITERAL("api"),
		XRT_STR_LITERAL("password"),
		pFixture->Secret,
		sizeof(pFixture->Secret),
		&pFixture->SecretSize
	), "HTTP Digest session secret fixture failed");
	pFixture->Config = (xhttpdigestsessionconfig){
		0,
		&pFixture->Challenge,
		&pFixture->Choice,
		XRT_STR_INIT("user"),
		{ NULL, 0 },
		{ pFixture->Secret, pFixture->SecretSize },
		XRT_STR_INIT("client-nonce")
	};
}



/* 为指定 Exchange 构造可验证的 Authentication-Info。 */
static xhttpdigestinfo testHttpDigestSessionInfo(
	const xhttpdigestexchange* pExchange,
	xstrview NextNonce,
	char Response[XHTTP_DIGEST_MAX_TEXT_SIZE]
)
{
	const xhttpdigestproof* pProof =
		xrtHttpDigestExchangeProof(pExchange);
	xhttpdigestinfo Info;
	size_t iSize;

	testRequire((pProof != NULL) && xrtHttpDigestRspAuth(
		pProof,
		Response,
		XHTTP_DIGEST_MAX_TEXT_SIZE,
		&iSize
	), "HTTP Digest session rspauth fixture failed");
	Info = (xhttpdigestinfo){
		XHTTP_DIGEST_INFO_HAS_RESPONSE |
			(NextNonce.Size != 0 ?
			 XHTTP_DIGEST_INFO_HAS_NEXT_NONCE : 0u),
		pProof->Algorithm,
		pProof->Qop,
		pProof->NonceCount,
		NextNonce,
		{ Response, iSize },
		pProof->Cnonce
	};
	return Info;
}



/* 验证每次授权取得唯一递增计数，且对象复制了创建输入。 */
static void testHttpDigestSessionAuthorize(void)
{
	test_http_digest_session_fixture Fixture;
	xhttpdigestsession* pSession;
	xhttpdigestexchange* pFirst;
	xhttpdigestexchange* pSecond;
	const xhttpdigestauth* pAuth;
	const xhttpdigestproof* pProof;

	testHttpDigestSessionFixtureInit(
		&Fixture, XRT_STR_LITERAL("server-nonce")
	);
	pSession = xrtHttpDigestSessionCreate(&Fixture.Config);
	testRequire(pSession != NULL,
		"HTTP Digest session create failed");
	memset(Fixture.Secret, 0, sizeof(Fixture.Secret));
	Fixture.Challenge.Nonce = XRT_STR_LITERAL("changed");
	pFirst = xrtHttpDigestSessionAuthorize(
		pSession,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/first"),
		(xstrview){ NULL, 0 }
	);
	pSecond = xrtHttpDigestSessionAuthorize(
		pSession,
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("/second"),
		(xstrview){ NULL, 0 }
	);
	testRequire((pFirst != NULL) && (pSecond != NULL),
		"HTTP Digest session authorize failed");
	pAuth = xrtHttpDigestExchangeAuth(pFirst);
	pProof = xrtHttpDigestExchangeProof(pFirst);
	testRequire((pAuth != NULL) && (pProof != NULL) &&
		(pAuth->NonceCount == 1u) &&
		(pProof->NonceCount == 1u) &&
		testHttpDigestSessionText(pAuth->Nonce, "server-nonce") &&
		testHttpDigestSessionText(pAuth->Uri, "/first") &&
		testHttpDigestSessionText(pAuth->Username, "user"),
		"HTTP Digest first exchange mismatch");
	pProof = xrtHttpDigestExchangeProof(pSecond);
	testRequire((pProof != NULL) &&
		(pProof->NonceCount == 2u) &&
		testHttpDigestSessionText(pProof->Uri, "/second"),
		"HTTP Digest second exchange mismatch");
	testRequire((xrtHttpDigestExchangeRetain(pFirst) == pFirst) &&
		(xrtHttpDigestSessionRetain(pSession) == pSession),
		"HTTP Digest session retain failed");
	xrtHttpDigestExchangeRelease(pFirst);
	xrtHttpDigestSessionRelease(pSession);
	xrtHttpDigestExchangeRelease(pFirst);
	xrtHttpDigestExchangeRelease(pSecond);
	xrtHttpDigestSessionRelease(pSession);
}



/* 验证 nextnonce 更新、计数复位和无更新的有效证明。 */
static void testHttpDigestSessionNextNonce(void)
{
	test_http_digest_session_fixture Fixture;
	xhttpdigestsession* pSession;
	xhttpdigestexchange* pOld;
	xhttpdigestexchange* pNext;
	xhttpdigestinfo Info;
	const xhttpdigestproof* pProof;
	char Response[XHTTP_DIGEST_MAX_TEXT_SIZE];

	testHttpDigestSessionFixtureInit(
		&Fixture, XRT_STR_LITERAL("old-nonce")
	);
	pSession = xrtHttpDigestSessionCreate(&Fixture.Config);
	pOld = xrtHttpDigestSessionAuthorize(
		pSession,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/resource"),
		(xstrview){ NULL, 0 }
	);
	testRequire((pSession != NULL) && (pOld != NULL),
		"HTTP Digest nextnonce fixture failed");
	Info = testHttpDigestSessionInfo(
		pOld, XRT_STR_LITERAL("next-nonce"), Response
	);
	testRequire(xrtHttpDigestSessionAccept(
		pSession,
		pOld,
		&Info,
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("next-client")
	) == XHTTP_DIGEST_SESSION_UPDATED,
		"HTTP Digest nextnonce was not adopted");
	pNext = xrtHttpDigestSessionAuthorize(
		pSession,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/next"),
		(xstrview){ NULL, 0 }
	);
	pProof = xrtHttpDigestExchangeProof(pNext);
	testRequire((pProof != NULL) &&
		(pProof->NonceCount == 1u) &&
		testHttpDigestSessionText(pProof->Nonce, "next-nonce") &&
		testHttpDigestSessionText(pProof->Cnonce, "next-client"),
		"HTTP Digest nextnonce state mismatch");
	Info = testHttpDigestSessionInfo(
		pNext, (xstrview){ NULL, 0 }, Response
	);
	testRequire(xrtHttpDigestSessionAccept(
		pSession,
		pNext,
		&Info,
		(xstrview){ NULL, 0 },
		(xstrview){ NULL, 0 }
	) == XHTTP_DIGEST_SESSION_VALID,
		"HTTP Digest response-only proof failed");
	xrtHttpDigestExchangeRelease(pOld);
	xrtHttpDigestExchangeRelease(pNext);
	xrtHttpDigestSessionRelease(pSession);
}



/* 验证乱序旧响应不能覆盖较新的 nextnonce。 */
static void testHttpDigestSessionSuperseded(void)
{
	test_http_digest_session_fixture Fixture;
	xhttpdigestsession* pSession;
	xhttpdigestexchange* pFirst;
	xhttpdigestexchange* pSecond;
	xhttpdigestexchange* pCurrent;
	xhttpdigestinfo FirstInfo;
	xhttpdigestinfo SecondInfo;
	const xhttpdigestproof* pProof;
	char FirstResponse[XHTTP_DIGEST_MAX_TEXT_SIZE];
	char SecondResponse[XHTTP_DIGEST_MAX_TEXT_SIZE];

	testHttpDigestSessionFixtureInit(
		&Fixture, XRT_STR_LITERAL("shared-nonce")
	);
	pSession = xrtHttpDigestSessionCreate(&Fixture.Config);
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
	FirstInfo = testHttpDigestSessionInfo(
		pFirst, XRT_STR_LITERAL("older-next"), FirstResponse
	);
	SecondInfo = testHttpDigestSessionInfo(
		pSecond, XRT_STR_LITERAL("newer-next"), SecondResponse
	);
	testRequire(xrtHttpDigestSessionAccept(
		pSession, pSecond, &SecondInfo,
		(xstrview){ NULL, 0 }, (xstrview){ NULL, 0 }
	) == XHTTP_DIGEST_SESSION_UPDATED,
		"HTTP Digest newer response update failed");
	testRequire(xrtHttpDigestSessionAccept(
		pSession, pFirst, &FirstInfo,
		(xstrview){ NULL, 0 }, (xstrview){ NULL, 0 }
	) == XHTTP_DIGEST_SESSION_SUPERSEDED,
		"HTTP Digest older response rolled state backward");
	pCurrent = xrtHttpDigestSessionAuthorize(
		pSession,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/current"),
		(xstrview){ NULL, 0 }
	);
	pProof = xrtHttpDigestExchangeProof(pCurrent);
	testRequire((pProof != NULL) &&
		testHttpDigestSessionText(pProof->Nonce, "newer-next") &&
		(pProof->NonceCount == 1u),
		"HTTP Digest superseded state mismatch");
	xrtHttpDigestExchangeRelease(pFirst);
	xrtHttpDigestExchangeRelease(pSecond);
	xrtHttpDigestExchangeRelease(pCurrent);
	xrtHttpDigestSessionRelease(pSession);
}



/* 验证无效证明和显式更新都不会产生模糊状态。 */
static void testHttpDigestSessionRejectAndUpdate(void)
{
	test_http_digest_session_fixture Fixture;
	test_http_digest_session_fixture Updated;
	xhttpdigestsession* pSession;
	xhttpdigestexchange* pOld;
	xhttpdigestexchange* pCurrent;
	xhttpdigestinfo Info;
	const xhttpdigestproof* pProof;
	char Response[XHTTP_DIGEST_MAX_TEXT_SIZE];

	testHttpDigestSessionFixtureInit(
		&Fixture, XRT_STR_LITERAL("initial")
	);
	pSession = xrtHttpDigestSessionCreate(&Fixture.Config);
	pOld = xrtHttpDigestSessionAuthorize(
		pSession,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/old"),
		(xstrview){ NULL, 0 }
	);
	Info = testHttpDigestSessionInfo(
		pOld, XRT_STR_LITERAL("rejected"), Response
	);
	Response[0] = Response[0] == '0' ? '1' : '0';
	testRequire(xrtHttpDigestSessionAccept(
		pSession, pOld, &Info,
		(xstrview){ NULL, 0 }, (xstrview){ NULL, 0 }
	) == XHTTP_DIGEST_SESSION_INVALID,
		"HTTP Digest session accepted invalid rspauth");
	testHttpDigestSessionFixtureInit(
		&Updated, XRT_STR_LITERAL("updated")
	);
	testRequire(xrtHttpDigestSessionUpdate(
		pSession, &Updated.Config
	), "HTTP Digest session explicit update failed");
	pCurrent = xrtHttpDigestSessionAuthorize(
		pSession,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/current"),
		(xstrview){ NULL, 0 }
	);
	pProof = xrtHttpDigestExchangeProof(pCurrent);
	testRequire((pProof != NULL) &&
		testHttpDigestSessionText(pProof->Nonce, "updated") &&
		(pProof->NonceCount == 1u),
		"HTTP Digest explicit update state mismatch");
	xrtHttpDigestExchangeRelease(pOld);
	xrtHttpDigestExchangeRelease(pCurrent);
	xrtHttpDigestSessionRelease(pSession);
}



/* 验证未对齐公开描述符、地址回绕和跨会话误用都被安全处理。 */
static void testHttpDigestSessionMemoryContracts(void)
{
	test_http_digest_session_fixture Fixture;
	uint8 ChallengeStorage[sizeof(xhttpdigestchallenge) + 2u];
	uint8 ChoiceStorage[sizeof(xhttpdigestchoice) + 2u];
	uint8 ConfigStorage[sizeof(xhttpdigestsessionconfig) + 2u];
	xhttpdigestchallenge* pChallenge =
		(xhttpdigestchallenge*)(void*)(ChallengeStorage + 1u);
	xhttpdigestchoice* pChoice =
		(xhttpdigestchoice*)(void*)(ChoiceStorage + 1u);
	xhttpdigestsessionconfig* pConfig =
		(xhttpdigestsessionconfig*)(void*)(ConfigStorage + 1u);
	xhttpdigestsessionconfig Config;
	xhttpdigestsession* pSession;
	xhttpdigestsession* pOther;
	xhttpdigestexchange* pExchange;
	xhttpdigestinfo Info;
	char Response[XHTTP_DIGEST_MAX_TEXT_SIZE];

	testHttpDigestSessionFixtureInit(
		&Fixture, XRT_STR_LITERAL("unaligned")
	);
	memset(ChallengeStorage, 0xA5, sizeof(ChallengeStorage));
	memset(ChoiceStorage, 0xA5, sizeof(ChoiceStorage));
	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	memcpy(pChallenge, &Fixture.Challenge, sizeof(*pChallenge));
	memcpy(pChoice, &Fixture.Choice, sizeof(*pChoice));
	Config = Fixture.Config;
	Config.Challenge = pChallenge;
	Config.Choice = pChoice;
	memcpy(pConfig, &Config, sizeof(*pConfig));
	pSession = xrtHttpDigestSessionCreate(pConfig);
	pOther = xrtHttpDigestSessionCreate(&Fixture.Config);
	pExchange = xrtHttpDigestSessionAuthorize(
		pSession,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/unaligned"),
		(xstrview){ NULL, 0 }
	);
	testRequire((pSession != NULL) && (pOther != NULL) &&
		(pExchange != NULL) &&
		(ChallengeStorage[0] == 0xA5) &&
		(ChallengeStorage[sizeof(ChallengeStorage) - 1u] == 0xA5) &&
		(ChoiceStorage[0] == 0xA5) &&
		(ChoiceStorage[sizeof(ChoiceStorage) - 1u] == 0xA5) &&
		(ConfigStorage[0] == 0xA5) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == 0xA5),
		"HTTP Digest session rejected unaligned descriptors");
	Info = testHttpDigestSessionInfo(
		pExchange, XRT_STR_LITERAL("next"), Response
	);
	testRequire(xrtHttpDigestSessionAccept(
		pOther,
		pExchange,
		&Info,
		(xstrview){ NULL, 0 },
		(xstrview){ NULL, 0 }
	) == XHTTP_DIGEST_SESSION_ERROR &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Digest session accepted a foreign exchange");
	xrtClearError();
	testRequire((xrtHttpDigestSessionCreate(
		(xhttpdigestsessionconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Digest session accepted a wrapping config");
	xrtClearError();
	testRequire((xrtHttpDigestExchangeAuth(
		(const xhttpdigestexchange*)(uintptr_t)(UINTPTR_MAX - 1u)
	) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Digest session accepted a wrapping exchange");
	xrtClearError();
	testRequire(xrtHttpDigestSessionAuthorize(
		pSession,
		XRT_STR_LITERAL("GET"),
		(xstrview){ (cstr)(uintptr_t)(UINTPTR_MAX - 1u), 8u },
		(xstrview){ NULL, 0 }
	) == NULL,
		"HTTP Digest session accepted a wrapping request target");
	xrtClearError();
	xrtHttpDigestExchangeRelease(pExchange);
	xrtHttpDigestSessionRelease(pOther);
	xrtHttpDigestSessionRelease(pSession);
}



int main(void)
{
	testHttpDigestSessionAuthorize();
	testHttpDigestSessionNextNonce();
	testHttpDigestSessionSuperseded();
	testHttpDigestSessionRejectAndUpdate();
	testHttpDigestSessionMemoryContracts();
	puts("[PASS] HTTP Digest client session");
	return 0;
}
