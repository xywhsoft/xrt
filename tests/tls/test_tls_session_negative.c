#include "../test.h"
#include "../../src/internal/xrt_tls_session.h"



/* 验证最近一次失败由 TLS 域和预期代码表达。 */
static void testTlsSessionError(xtlserror Code, cstr sMessage)
{
	const xerror* pError = xrtGetError();

	testRequire((pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.tls") == 0) &&
		(xrtErrorCode(pError) == (int32)Code), sMessage);
}



/* 空对象、空所有权与空输出必须失败且不能执行释放过程。 */
static void testTlsSessionArguments(void)
{
	xtlscontext* pContext = xrtTlsContextCreate(NULL);
	xtlssession* pSession;
	xnetbuf Buffer;
	xnetspan Span;
	xnetwspan Write;
	char iByte = 0;
	size_t iRead = 0;

	testRequire(pContext != NULL, "TLS negative context failed");
	xrtClearError();
	testRequire(__xrtTlsSessionCreate(
		NULL, NULL, XTLS_CLIENT
	) == NULL, "TLS session accepted a null context");
	testTlsSessionError(XTLS_ERROR_ARGUMENT,
		"TLS null context error mismatch");
	xrtClearError();
	testRequire(__xrtTlsSessionCreate(
		pContext, NULL, (xtlsrole)0
	) == NULL, "TLS session accepted an invalid role");
	testTlsSessionError(XTLS_ERROR_ARGUMENT,
		"TLS invalid role error mismatch");

	pSession = __xrtTlsSessionCreate(pContext, NULL, XTLS_SERVER);
	xrtTlsContextRelease(pContext);
	testRequire(pSession != NULL, "TLS negative session failed");
	testRequire(xrtNetBufInit(&Buffer, NULL),
		"TLS negative input buffer setup failed");

	xrtClearError();
	testRequire(xrtTlsSessionFeed(
		pSession, NULL, 1
	) == XTLS_ERROR, "TLS feed accepted null data");
	testTlsSessionError(XTLS_ERROR_ARGUMENT,
		"TLS null feed error mismatch");
	xrtClearError();
	testRequire(xrtTlsSessionFeedBorrow(
		pSession, &iByte, 0
	) == XTLS_ERROR, "TLS borrow accepted empty ownership");
	testTlsSessionError(XTLS_ERROR_ARGUMENT,
		"TLS empty borrow error mismatch");
	xrtClearError();
	testRequire(xrtTlsSessionFeedRef(
		pSession, &iByte, 1, NULL, NULL
	) == XTLS_ERROR, "TLS reference accepted a null release procedure");
	testTlsSessionError(XTLS_ERROR_ARGUMENT,
		"TLS null release error mismatch");
	xrtClearError();
	testRequire(xrtTlsSessionFeedBuffer(
		pSession, NULL
	) == XTLS_ERROR, "TLS buffer feed accepted a null buffer");
	testTlsSessionError(XTLS_ERROR_ARGUMENT,
		"TLS null input buffer error mismatch");
	testRequire(xrtNetBufReserve(&Buffer, 1, &Write),
		"TLS reserved input buffer setup failed");
	xrtClearError();
	testRequire(xrtTlsSessionFeedBuffer(
		pSession,
		&Buffer
	) == XTLS_ERROR && (Buffer.Reserved != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"TLS buffer feed accepted an active write reservation");
	testTlsSessionError(XTLS_ERROR_INTERNAL,
		"TLS reserved input buffer error mismatch");
	testRequire(xrtNetBufCancel(&Buffer),
		"TLS reserved input buffer cleanup failed");

	xrtClearError();
	testRequire(!xrtTlsSessionSendFront(pSession, NULL),
		"TLS send front accepted null output");
	testTlsSessionError(XTLS_ERROR_ARGUMENT,
		"TLS null send front error mismatch");
	xrtClearError();
	testRequire(xrtTlsSessionPlainSpans(
		pSession, NULL, 1
	) == 0, "TLS plain spans accepted null output");
	testTlsSessionError(XTLS_ERROR_ARGUMENT,
		"TLS null plain spans error mismatch");
	xrtClearError();
	testRequire(xrtTlsSessionRead(
		pSession, NULL, 1, &iRead
	) == XTLS_ERROR, "TLS read accepted null output");
	testTlsSessionError(XTLS_ERROR_ARGUMENT,
		"TLS null read output error mismatch");
	xrtClearError();
	testRequire(__xrtTlsSessionFeedPullup(
		pSession, 1, &Span
	) == XTLS_AGAIN && (xrtGetError() == NULL),
		"TLS incomplete feed pullup did not return clean AGAIN");

	xrtNetBufClear(&Buffer);
	xrtTlsSessionDestroy(pSession);
	xrtClearError();
	testRequire((xrtTlsSessionRole(NULL) == (xtlsrole)0) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_ARGUMENT),
		"TLS null session query error mismatch");
	xrtClearError();
}



/* 状态、等待、预留和消费必须拒绝越界或不可能的推进。 */
static void testTlsSessionStateAndQueueErrors(void)
{
	xtlscontext* pContext = xrtTlsContextCreate(NULL);
	xtlssession* pSession;
	xnetwspan Write;

	testRequire(pContext != NULL, "TLS queue negative context failed");
	pSession = __xrtTlsSessionCreate(pContext, NULL, XTLS_CLIENT);
	xrtTlsContextRelease(pContext);
	testRequire(pSession != NULL, "TLS queue negative session failed");

	xrtClearError();
	testRequire(!__xrtTlsSessionSetState(pSession, XTLS_STATE_READY),
		"TLS session skipped the handshake state");
	testTlsSessionError(XTLS_ERROR_STATE,
		"TLS invalid state transition error mismatch");
	xrtClearError();
	testRequire(!__xrtTlsSessionSetWait(pSession, 1u << 31),
		"TLS session accepted an unknown wait bit");
	testTlsSessionError(XTLS_ERROR_STATE,
		"TLS unknown wait error mismatch");

	testRequire(__xrtTlsSessionSend(
		pSession, "abc", 3
	) == XTLS_OK, "TLS send overconsume setup failed");
	xrtClearError();
	testRequire(!xrtTlsSessionSendConsume(pSession, 4) &&
		(xrtTlsSessionSendSize(pSession) == 3),
		"TLS send queue silently overconsumed");
	testTlsSessionError(XTLS_ERROR_LIMIT,
		"TLS send overconsume error mismatch");
	testRequire(xrtTlsSessionSendConsume(pSession, 3),
		"TLS send cleanup consumption failed");

	testRequire(__xrtTlsSessionPlain(
		pSession, "abc", 3
	) == XTLS_OK, "TLS plain overconsume setup failed");
	xrtClearError();
	testRequire(!xrtTlsSessionPlainConsume(pSession, 4) &&
		(xrtTlsSessionPlainSize(pSession) == 3),
		"TLS plain queue silently overconsumed");
	testTlsSessionError(XTLS_ERROR_LIMIT,
		"TLS plain overconsume error mismatch");
	testRequire(xrtTlsSessionPlainConsume(pSession, 3),
		"TLS plain cleanup consumption failed");

	testRequire(__xrtTlsSessionSendReserve(
		pSession, 8, &Write
	) == XTLS_OK, "TLS send duplicate reserve setup failed");
	xrtClearError();
	testRequire(__xrtTlsSessionSendReserve(
		pSession, 8, &Write
	) == XTLS_ERROR, "TLS send accepted a duplicate reservation");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE,
		"TLS duplicate reserve lost the state error kind");
	testRequire(__xrtTlsSessionSendCancel(pSession),
		"TLS send reservation cleanup failed");

	testRequire(__xrtTlsSessionPlainReserve(
		pSession, 8, &Write
	) == XTLS_OK, "TLS plain commit limit setup failed");
	xrtClearError();
	testRequire(!__xrtTlsSessionPlainCommit(
		pSession, XTLS_PLAIN_LIMIT_DEFAULT + 1u
	), "TLS plain commit crossed its hard limit");
	testTlsSessionError(XTLS_ERROR_LIMIT,
		"TLS plain commit limit error mismatch");
	testRequire(__xrtTlsSessionPlainCancel(pSession),
		"TLS plain reservation cleanup failed");

	testRequire(__xrtTlsSessionSetState(
		pSession, XTLS_STATE_FAILED
	), "TLS failure transition failed");
	xrtClearError();
	testRequire(!__xrtTlsSessionSetWait(pSession, XTLS_WAIT_INPUT),
		"TLS failed session accepted a wait reason");
	testTlsSessionError(XTLS_ERROR_STATE,
		"TLS terminal wait error mismatch");
	xrtTlsSessionDestroy(pSession);
}



/* 执行 TLS 公共会话底座负向边界回归。 */
int main(void)
{
	testTlsSessionArguments();
	testTlsSessionStateAndQueueErrors();
	return 0;
}
