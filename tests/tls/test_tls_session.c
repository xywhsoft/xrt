#include "../test.h"
#include "../../src/internal/xrt_tls_session.h"



typedef struct test_tls_session_role {
	uint8 Secret[32];
} test_tls_session_role;



static size_t TestTlsSessionRoleCleaned = 0;



/* 检查角色尾部状态，并记录统一销毁路径只调用一次。 */
static void testTlsSessionRoleClean(
	xtlssession* pSession,
	ptr pRole
)
{
	test_tls_session_role* pState = (test_tls_session_role*)pRole;

	testRequire((pSession != NULL) && (pState != NULL),
		"TLS role cleanup received no state");
	for ( size_t i = 0; i < sizeof(pState->Secret); i++ ) {
		testRequire(pState->Secret[i] == (uint8)(i + 1u),
			"TLS role cleanup state changed before release");
	}
	TestTlsSessionRoleCleaned++;
}



/* 统计带所有权输入真正离开会话队列的次数。 */
static void testTlsSessionRelease(
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



/* 新会话必须只分配对象本体，不触发三个网络队列的固定预分配。 */
static void testTlsSessionLazyQueues(void)
{
	xnetbufpoolconfig PoolConfig;
	xnetbufpoolinfo Before;
	xnetbufpoolinfo After;
	xnetbufpool* pPool;
	xtlscontext* pContext;
	xtlssession* pSession;

	xrtNetBufPoolConfigInit(&PoolConfig);
	pPool = xrtNetBufPoolCreate(&PoolConfig);
	pContext = xrtTlsContextCreate(NULL);
	testRequire((pPool != NULL) && (pContext != NULL),
		"TLS session lazy queue setup failed");
	xrtNetBufPoolGet(pPool, &Before);
	pSession = __xrtTlsSessionCreate(pContext, pPool, XTLS_CLIENT);
	testRequire(pSession != NULL, "TLS session creation failed");
	xrtNetBufPoolGet(pPool, &After);
	testRequire((After.LiveBlocks == Before.LiveBlocks) &&
		(After.LiveBytes == Before.LiveBytes) &&
		(xrtTlsSessionFeedSize(pSession) == 0) &&
		(xrtTlsSessionSendSize(pSession) == 0) &&
		(xrtTlsSessionPlainSize(pSession) == 0),
		"TLS session preallocated queue storage");
	testRequire((xrtTlsSessionRole(pSession) == XTLS_CLIENT) &&
		(xrtTlsSessionState(pSession) == XTLS_STATE_NEW) &&
		(xrtTlsSessionWait(pSession) == XTLS_WAIT_NONE),
		"TLS session initial metadata mismatch");

	xrtTlsContextRelease(pContext);
	testRequire(xrtTlsContextPolicy(xrtTlsSessionContext(pSession)) != NULL,
		"TLS session did not retain its context");
	xrtTlsSessionDestroy(pSession);
	testRequire(xrtNetBufPoolDestroy(pPool),
		"TLS session left live lazy queue blocks");
}



/* 角色状态必须与会话一次分配，并由公共销毁路径统一清理。 */
static void testTlsSessionRoleStorage(void)
{
	xtlscontext* pContext = xrtTlsContextCreate(NULL);
	xtlssession* pSession;
	test_tls_session_role* pRole;

	testRequire(pContext != NULL, "TLS role storage context failed");
	pSession = __xrtTlsSessionCreateSized(
		pContext, NULL, XTLS_SERVER,
		sizeof(test_tls_session_role), testTlsSessionRoleClean
	);
	xrtTlsContextRelease(pContext);
	testRequire(pSession != NULL, "TLS role storage session failed");
	pRole = (test_tls_session_role*)__xrtTlsSessionRoleData(pSession);
	testRequire((pRole != NULL) &&
		((bytes)pRole == (bytes)(pSession + 1)),
		"TLS role storage is not contiguous");
	for ( size_t i = 0; i < sizeof(pRole->Secret); i++ ) {
		pRole->Secret[i] = (uint8)(i + 1u);
	}
	TestTlsSessionRoleCleaned = 0;
	xrtTlsSessionDestroy(pSession);
	testRequire(TestTlsSessionRoleCleaned == 1u,
		"TLS role cleanup count mismatch");
}



/* 输入队列必须分别遵守复制、借用、引用释放与接管语义。 */
static void testTlsSessionFeedOwnership(void)
{
	xtlscontext* pContext = xrtTlsContextCreate(NULL);
	xtlssession* pSession;
	const xnetbuf* pFeed;
	xnetbuf Buffer;
	xnetspan Span;
	char sCopy[] = "copy";
	char sBorrow[] = "borrow";
	char sRef[] = "reference";
	char sBufferRef[] = "chain";
	char sMoved[16] = { 0 };
	char* pTaken;
	size_t iReleased = 0;

	testRequire(pContext != NULL, "TLS feed ownership context failed");
	pSession = __xrtTlsSessionCreate(pContext, NULL, XTLS_SERVER);
	xrtTlsContextRelease(pContext);
	testRequire(pSession != NULL, "TLS feed ownership session failed");

	testRequire(xrtTlsSessionFeed(pSession, sCopy, 4) == XTLS_OK,
		"TLS copied feed failed");
	sCopy[0] = 'X';
	pFeed = __xrtTlsSessionFeedBuffer(pSession);
	testRequire((pFeed != NULL) && xrtNetBufFront(pFeed, &Span) &&
		(Span.Size == 4) && (memcmp(Span.Data, "copy", 4) == 0),
		"TLS copied feed borrowed caller memory");
	testRequire(__xrtTlsSessionFeedConsume(pSession, 4),
		"TLS copied feed consumption failed");

	testRequire(xrtTlsSessionFeedBorrow(
		pSession, sBorrow, strlen(sBorrow)
	) == XTLS_OK, "TLS borrowed feed failed");
	sBorrow[0] = 'B';
	testRequire(xrtNetBufFront(pFeed, &Span) &&
		(Span.Data[0] == (uint8)'B'),
		"TLS borrowed feed did not preserve reference semantics");
	testRequire(__xrtTlsSessionFeedConsume(pSession, strlen(sBorrow)),
		"TLS borrowed feed consumption failed");

	testRequire(xrtTlsSessionFeedRef(
		pSession, sRef, strlen(sRef), testTlsSessionRelease, &iReleased
	) == XTLS_OK, "TLS referenced feed failed");
	testRequire(__xrtTlsSessionFeedConsume(pSession, 3) &&
		(iReleased == 0),
		"TLS reference released before its complete consumption");
	testRequire(__xrtTlsSessionFeedConsume(
		pSession, strlen(sRef) - 3
	) && (iReleased == 1),
		"TLS reference was not released exactly once");

	pTaken = (char*)xrtMalloc(5);
	testRequire(pTaken != NULL, "TLS taken feed allocation failed");
	memcpy(pTaken, "taken", 5);
	testRequire(xrtTlsSessionFeedTake(pSession, pTaken, 5) == XTLS_OK,
		"TLS taken feed failed");
	testRequire(__xrtTlsSessionFeedConsume(pSession, 5),
		"TLS taken feed consumption failed");

	testRequire(xrtNetBufInit(&Buffer, NULL) &&
		xrtNetBufAppend(&Buffer, "move", 4) &&
		xrtNetBufAppendRef(
			&Buffer,
			sBufferRef,
			strlen(sBufferRef),
			testTlsSessionRelease,
			&iReleased
		), "TLS moved feed setup failed");
	testRequire(xrtTlsSessionFeedBuffer(
		pSession,
		&Buffer
	) == XTLS_OK && xrtNetBufEmpty(&Buffer) &&
		(xrtTlsSessionFeedSize(pSession) == 9) &&
		(xrtNetBufPeek(pFeed, 0, sMoved, 9) == 9) &&
		(memcmp(sMoved, "movechain", 9) == 0),
		"TLS buffer feed did not transfer its block chain");
	testRequire(__xrtTlsSessionFeedConsume(pSession, 8) &&
		(iReleased == 1) &&
		__xrtTlsSessionFeedConsume(pSession, 1) &&
		(iReleased == 2),
		"TLS buffer feed released ownership at the wrong boundary");
	xrtNetBufClear(&Buffer);
	xrtTlsSessionDestroy(pSession);
}



/* 密文和明文队列必须同时提供 Span、严格消费和复制式便捷读取。 */
static void testTlsSessionOutputQueues(void)
{
	xtlscontext* pContext = xrtTlsContextCreate(NULL);
	xtlssession* pSession;
	xnetspan Spans[2];
	xnetwspan Write;
	char sOutput[8] = { 0 };
	size_t iRead = 0;

	testRequire(pContext != NULL, "TLS output queue context failed");
	pSession = __xrtTlsSessionCreate(pContext, NULL, XTLS_CLIENT);
	xrtTlsContextRelease(pContext);
	testRequire(pSession != NULL, "TLS output queue session failed");

	testRequire(__xrtTlsSessionSend(
		pSession, "cipher", 6
	) == XTLS_OK, "TLS send queue append failed");
	testRequire((xrtTlsSessionSendSize(pSession) == 6) &&
		(xrtTlsSessionSendSpanCount(pSession) == 1) &&
		xrtTlsSessionSendFront(pSession, &Spans[0]) &&
		(Spans[0].Size == 6) &&
		(memcmp(Spans[0].Data, "cipher", 6) == 0) &&
		(xrtTlsSessionSendSpans(pSession, Spans, 2) == 1),
		"TLS send queue span view mismatch");
	testRequire(xrtTlsSessionSendConsume(pSession, 2) &&
		(xrtTlsSessionSendSize(pSession) == 4),
		"TLS send queue partial consumption failed");
	testRequire(xrtTlsSessionSendConsume(pSession, 4),
		"TLS send queue final consumption failed");

	testRequire(__xrtTlsSessionSendReserve(
		pSession, 5, &Write
	) == XTLS_OK && (Write.Size >= 5),
		"TLS send queue reservation failed");
	memcpy(Write.Data, "frame", 5);
	testRequire(__xrtTlsSessionSendCommit(pSession, 5) &&
		(xrtTlsSessionSendSize(pSession) == 5),
		"TLS send queue commit failed");
	testRequire(xrtTlsSessionSendConsume(pSession, 5),
		"TLS reserved send consumption failed");

	testRequire(__xrtTlsSessionPlain(
		pSession, "plain", 5
	) == XTLS_OK, "TLS plain queue append failed");
	testRequire((xrtTlsSessionPlainSize(pSession) == 5) &&
		(xrtTlsSessionPlainSpanCount(pSession) == 1) &&
		xrtTlsSessionPlainFront(pSession, &Spans[0]) &&
		(Spans[0].Size == 5) &&
		(xrtTlsSessionPlainSpans(pSession, Spans, 2) == 1),
		"TLS plain queue span view mismatch");
	testRequire(xrtTlsSessionRead(
		pSession, sOutput, 3, &iRead
	) == XTLS_OK && (iRead == 3) &&
		(memcmp(sOutput, "pla", 3) == 0),
		"TLS plain convenience read failed");
	testRequire(xrtTlsSessionPlainConsume(pSession, 2) &&
		(xrtTlsSessionPlainSize(pSession) == 0),
		"TLS plain final consumption failed");
	xrtTlsSessionDestroy(pSession);
}



/* 控制结果和状态推进必须保持错误槽、等待位与终态语义。 */
static void testTlsSessionControl(void)
{
	xtlscontext* pContext = xrtTlsContextCreate(NULL);
	xtlssession* pSession;
	xerror* pMarker;
	char iByte = 0;
	size_t iRead = SIZE_MAX;

	testRequire(pContext != NULL, "TLS session control context failed");
	pSession = __xrtTlsSessionCreate(pContext, NULL, XTLS_CLIENT);
	xrtTlsContextRelease(pContext);
	testRequire(pSession != NULL, "TLS session control setup failed");

	pMarker = xrtErrorCreate(XERR_VALUE, "test.tls", 91, "marker");
	testRequire(pMarker != NULL, "TLS session marker error creation failed");
	xrtSetError(pMarker);
	testRequire(xrtTlsSessionRead(
		pSession, &iByte, 1, &iRead
	) == XTLS_AGAIN && (iRead == 0) && (xrtGetError() == pMarker),
		"TLS AGAIN changed the current error");

	testRequire(__xrtTlsSessionSetState(
		pSession, XTLS_STATE_HANDSHAKE
	) && __xrtTlsSessionSetWait(
		pSession, XTLS_WAIT_INPUT | XTLS_WAIT_OUTPUT
	) && (xrtTlsSessionWait(pSession) ==
		(XTLS_WAIT_INPUT | XTLS_WAIT_OUTPUT)),
		"TLS handshake state or wait publication failed");
	testRequire(__xrtTlsSessionSetState(pSession, XTLS_STATE_READY) &&
		__xrtTlsSessionSetState(pSession, XTLS_STATE_CLOSING) &&
		__xrtTlsSessionSetState(pSession, XTLS_STATE_CLOSED) &&
		(xrtTlsSessionWait(pSession) == XTLS_WAIT_NONE),
		"TLS session lifecycle did not reach a clean terminal state");
	testRequire(xrtTlsSessionRead(
		pSession, &iByte, 1, &iRead
	) == XTLS_CLOSED && (iRead == 0) && (xrtGetError() == pMarker),
		"TLS CLOSED changed the current error");

	xrtClearError();
	xrtErrorFree(pMarker);
	xrtTlsSessionDestroy(pSession);
}



/* ALPN 查询区分尚未协商与参数错误，成功发布后借用稳定的角色存储。 */
static void testTlsSessionProtocol(void)
{
	static const uint8 ProtocolData[] = { 'h', '2' };
	xtlscontext* pContext = xrtTlsContextCreate(NULL);
	xtlssession* pSession;
	xbytesview Protocol = XRT_BYTES_LITERAL("unchanged");

	testRequire(pContext != NULL, "TLS protocol context failed");
	pSession = __xrtTlsSessionCreate(pContext, NULL, XTLS_CLIENT);
	xrtTlsContextRelease(pContext);
	testRequire(pSession != NULL, "TLS protocol session failed");

	xrtClearError();
	testRequire(!xrtTlsSessionProtocol(NULL, &Protocol) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_ARGUMENT),
		"TLS null session protocol query was accepted");
	xrtClearError();
	testRequire(!xrtTlsSessionProtocol(pSession, &Protocol) &&
		(Protocol.Size == sizeof("unchanged") - 1u) &&
		(xrtGetError() == NULL),
		"TLS absent protocol changed output or error state");
	testRequire(!xrtTlsSessionProtocol(pSession, NULL) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_ARGUMENT),
		"TLS null protocol output was accepted");

	xrtClearError();
	testRequire(__xrtTlsSessionSetProtocol(
		pSession,
		(xbytesview) { ProtocolData, sizeof(ProtocolData) }
	) && xrtTlsSessionProtocol(pSession, &Protocol) &&
		(Protocol.Data == ProtocolData) &&
		(Protocol.Size == sizeof(ProtocolData)) &&
		(memcmp(Protocol.Data, "h2", sizeof(ProtocolData)) == 0),
		"TLS negotiated protocol publication failed");
	testRequire(!__xrtTlsSessionSetProtocol(
		pSession,
		(xbytesview) { ProtocolData, sizeof(ProtocolData) }
	) && (xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_STATE),
		"TLS negotiated protocol was published twice");
	xrtTlsSessionDestroy(pSession);
}



/* 会话拥有的明文块归还 Worker 池前必须完成安全擦除。 */
static void testTlsSessionPlainWipe(void)
{
	xnetbufpool* pPool = xrtNetBufPoolCreate(NULL);
	xtlscontext* pContext = xrtTlsContextCreate(NULL);
	xtlssession* pSession;
	xnetbuf Probe;
	xnetwspan Span;
	char sOutput[6];
	size_t iRead = 0;

	testRequire((pPool != NULL) && (pContext != NULL),
		"TLS plaintext wipe setup failed");
	pSession = __xrtTlsSessionCreate(pContext, pPool, XTLS_SERVER);
	xrtTlsContextRelease(pContext);
	testRequire((pSession != NULL) &&
		(__xrtTlsSessionPlain(pSession, "secret", 6) == XTLS_OK),
		"TLS plaintext wipe session failed");
	testRequire(xrtTlsSessionRead(
		pSession, sOutput, sizeof(sOutput), &iRead
	) == XTLS_OK && (iRead == sizeof(sOutput)) &&
		(memcmp(sOutput, "secret", sizeof(sOutput)) == 0),
		"TLS plaintext wipe read failed");

	testRequire(xrtNetBufInit(&Probe, pPool) &&
		xrtNetBufReserve(&Probe, 6, &Span) && (Span.Size >= 6),
		"TLS consumed plaintext wipe probe failed");
	for ( size_t i = 0; i < 6; i++ ) {
		testRequire(Span.Data[i] == 0,
			"TLS consumed plaintext remained in a recycled worker block");
	}
	testRequire(xrtNetBufCancel(&Probe),
		"TLS consumed plaintext wipe probe cancellation failed");
	xrtNetBufClear(&Probe);

	testRequire(__xrtTlsSessionPlainReserve(
		pSession, 6, &Span
	) == XTLS_OK && (Span.Size >= 6),
		"TLS cancelled plaintext wipe setup failed");
	memcpy(Span.Data, "secret", 6);
	testRequire(__xrtTlsSessionPlainCancel(pSession),
		"TLS plaintext reservation cancellation failed");
	testRequire(xrtNetBufInit(&Probe, pPool) &&
		xrtNetBufReserve(&Probe, 6, &Span) && (Span.Size >= 6),
		"TLS cancelled plaintext wipe probe failed");
	for ( size_t i = 0; i < 6; i++ ) {
		testRequire(Span.Data[i] == 0,
			"TLS cancelled plaintext remained in a recycled worker block");
	}
	testRequire(xrtNetBufCancel(&Probe),
		"TLS cancelled plaintext wipe probe cancellation failed");
	xrtNetBufClear(&Probe);

	testRequire(__xrtTlsSessionPlain(
		pSession, "secret", 6
	) == XTLS_OK, "TLS destroy wipe setup failed");
	xrtTlsSessionDestroy(pSession);
	testRequire(xrtNetBufInit(&Probe, pPool) &&
		xrtNetBufReserve(&Probe, 6, &Span) && (Span.Size >= 6),
		"TLS destroyed plaintext wipe probe failed");
	for ( size_t i = 0; i < 6; i++ ) {
		testRequire(Span.Data[i] == 0,
			"TLS destroyed plaintext remained in a recycled worker block");
	}
	testRequire(xrtNetBufCancel(&Probe),
		"TLS destroyed plaintext wipe probe cancellation failed");
	xrtNetBufClear(&Probe);
	testRequire(xrtNetBufPoolDestroy(pPool),
		"TLS plaintext wipe pool remained live");
}



/* 执行 TLS 公共会话底座正常路径回归。 */
int main(void)
{
	testTlsSessionLazyQueues();
	testTlsSessionRoleStorage();
	testTlsSessionFeedOwnership();
	testTlsSessionOutputQueues();
	testTlsSessionControl();
	testTlsSessionProtocol();
	testTlsSessionPlainWipe();
	return 0;
}
