#include "../test.h"
#include "../../src/internal/xrt_tls_session.h"



/* 记录达到硬上限后未被会话接管的引用是否遭到错误释放。 */
static void testTlsSessionLimitRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	size_t* pReleased = (size_t*)pContext;

	(void)pData;
	(void)iSize;
	(*pReleased)++;
}



/* 创建使用协议允许最小队列上限的上下文。 */
static xtlscontext* testTlsSessionLimitContext(void)
{
	xtlscontextconfig Config;

	xrtTlsContextConfigInit(&Config);
	Config.Limits.FeedLimit = XTLS_RECORD_HEADER_SIZE +
		XTLS12_RECORD_CIPHERTEXT_MAX;
	Config.Limits.SendLimit = Config.Limits.FeedLimit;
	Config.Limits.PlainLimit = XTLS_RECORD_PLAINTEXT_MAX;
	return xrtTlsContextCreate(&Config);
}



/* 三个队列的硬背压必须原子返回 AGAIN 且不覆盖现有错误。 */
int main(void)
{
	xtlscontext* pContext = testTlsSessionLimitContext();
	xtlssession* pSession;
	xnetbuf Extra;
	const xtlslimits* pLimits;
	xerror* pMarker;
	char* pFeed;
	char* pSend;
	char* pPlain;
	char iExtra = 'x';
	size_t iReleased = 0;

	testRequire(pContext != NULL, "TLS session limit context failed");
	pLimits = xrtTlsContextLimits(pContext);
	pFeed = (char*)xrtMalloc(pLimits->FeedLimit);
	pSend = (char*)xrtMalloc(pLimits->SendLimit);
	pPlain = (char*)xrtMalloc(pLimits->PlainLimit);
	testRequire((pFeed != NULL) && (pSend != NULL) && (pPlain != NULL),
		"TLS session limit storage allocation failed");
	memset(pFeed, 'f', pLimits->FeedLimit);
	memset(pSend, 's', pLimits->SendLimit);
	memset(pPlain, 'p', pLimits->PlainLimit);

	pSession = __xrtTlsSessionCreate(pContext, NULL, XTLS_CLIENT);
	xrtTlsContextRelease(pContext);
	testRequire(pSession != NULL, "TLS session limit creation failed");
	testRequire(xrtTlsSessionFeedBorrow(
		pSession, pFeed, pLimits->FeedLimit
	) == XTLS_OK, "TLS feed limit baseline failed");
	testRequire(__xrtTlsSessionSend(
		pSession, pSend, pLimits->SendLimit
	) == XTLS_OK, "TLS send limit baseline failed");
	testRequire(__xrtTlsSessionPlain(
		pSession, pPlain, pLimits->PlainLimit
	) == XTLS_OK, "TLS plain limit baseline failed");

	pMarker = xrtErrorCreate(XERR_VALUE, "test.tls", 92, "limit-marker");
	testRequire(pMarker != NULL, "TLS limit marker creation failed");
	xrtSetError(pMarker);
	testRequire(xrtNetBufInit(&Extra, NULL) &&
		xrtNetBufAppend(&Extra, &iExtra, 1),
		"TLS buffer feed limit setup failed");
	testRequire(xrtTlsSessionFeedBuffer(
		pSession,
		&Extra
	) == XTLS_AGAIN && (xrtNetBufSize(&Extra) == 1) &&
		(xrtGetError() == pMarker),
		"TLS buffer feed limit changed ownership or error");
	testRequire(xrtTlsSessionFeedRef(
		pSession, &iExtra, 1, testTlsSessionLimitRelease, &iReleased
	) == XTLS_AGAIN && (iReleased == 0) && (xrtGetError() == pMarker),
		"TLS feed hard limit changed ownership or error");
	testRequire(__xrtTlsSessionSend(
		pSession, &iExtra, 1
	) == XTLS_AGAIN && (xrtGetError() == pMarker),
		"TLS send hard limit changed the error");
	testRequire(__xrtTlsSessionPlain(
		pSession, &iExtra, 1
	) == XTLS_AGAIN && (xrtGetError() == pMarker),
		"TLS plain hard limit changed the error");

	testRequire(__xrtTlsSessionFeedConsume(
		pSession, pLimits->FeedLimit
	) && xrtTlsSessionSendConsume(
		pSession, pLimits->SendLimit
	) && xrtTlsSessionPlainConsume(
		pSession, pLimits->PlainLimit
	), "TLS full-limit queue consumption failed");
	xrtNetBufClear(&Extra);
	xrtClearError();
	xrtErrorFree(pMarker);
	xrtTlsSessionDestroy(pSession);
	xrtFree(pFeed);
	xrtFree(pSend);
	xrtFree(pPlain);
	return 0;
}
