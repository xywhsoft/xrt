#include "../fixtures/tls_server.h"
#include "../../src/internal/xrt_tls_server.h"



/* 解析服务端发送队列中唯一的一条明文 Alert 记录。 */
static bool testTlsServerPlainAlert(
	const xtlssession* pServer,
	xtlsalert Expected
)
{
	xnetspan Span;
	xtlsrecord Record;
	xtlsalertlevel Level;
	xtlsalert Alert;
	size_t iRequired = 0;

	return xrtTlsSessionSendFront(pServer, &Span) &&
		(xrtTlsRecordParse(
			(xbytesview) { Span.Data, Span.Size }, &Record, &iRequired
		) == XTLS_OK) &&
		(Record.EncodedSize == xrtTlsSessionSendSize(pServer)) &&
		(Record.Type == XTLS_RECORD_ALERT) &&
		xrtTlsAlertParse(Record.Payload, &Level, &Alert) &&
		(Level == XTLS_ALERT_FATAL) && (Alert == Expected);
}



/* 首条消息不是 ClientHello 时必须排队明文 fatal Alert。 */
static void testTlsServerAlertPlain(
	const xtlscontext* pContext,
	const xtlsidentity* pIdentity
)
{
	uint8 Message[XTLS_HANDSHAKE_HEADER_SIZE];
	uint8 Record[XTLS_RECORD_HEADER_SIZE + sizeof(Message)];
	xtlsserverconfig Config;
	xtlssession* pServer;

	testRequire(xrtTlsHandshakeEncode(
		XTLS_HANDSHAKE_SERVER_HELLO,
		(xbytesview) { NULL, 0 }, Message, sizeof(Message)
	), "TLS invalid first handshake message encoding failed");
	testRequire(xrtTlsRecordEncode(
		XTLS_RECORD_HANDSHAKE, UINT16_C(0x0303),
		(xbytesview) { Message, sizeof(Message) }, Record, sizeof(Record)
	), "TLS invalid first handshake record encoding failed");
	xrtTlsServerConfigInit(&Config);
	Config.Context = pContext;
	Config.Identity = pIdentity;
	pServer = xrtTlsServerCreate(&Config, NULL);
	testRequire(pServer != NULL, "TLS alert server creation failed");
	testRequire(xrtTlsSessionFeed(
		pServer, Record, sizeof(Record)
	) == XTLS_OK, "TLS invalid first message feed failed");
	xrtClearError();
	testRequire((xrtTlsServerDrive(pServer) == XTLS_ERROR) &&
		(xrtTlsSessionState(pServer) == XTLS_STATE_FAILED) &&
		pServer->FatalSent &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_HANDSHAKE) &&
		testTlsServerPlainAlert(
			pServer, XTLS_ALERT_UNEXPECTED_MESSAGE
		), "TLS server did not preserve and report the first-flight error");
	xrtTlsSessionDestroy(pServer);
}



/* fatal Alert 无法进入满队列时仍必须保留最初的协议错误。 */
static void testTlsServerAlertBackpressure(
	const xtlscontext* pContext,
	const xtlsidentity* pIdentity
)
{
	uint8 Message[XTLS_HANDSHAKE_HEADER_SIZE];
	uint8 Record[XTLS_RECORD_HEADER_SIZE + sizeof(Message)];
	const xtlslimits* pLimits = xrtTlsContextLimits(pContext);
	bytes pFill;
	xtlsserverconfig Config;
	xtlssession* pServer;

	testRequire((pLimits != NULL) && (pLimits->SendLimit != 0),
		"TLS fatal Alert send limit is unavailable");
	pFill = (bytes)xrtMalloc(pLimits->SendLimit);
	testRequire(pFill != NULL, "TLS fatal Alert queue fill allocation failed");
	memset(pFill, 0xA5, pLimits->SendLimit);
	testRequire(xrtTlsHandshakeEncode(
		XTLS_HANDSHAKE_SERVER_HELLO,
		(xbytesview) { NULL, 0 }, Message, sizeof(Message)
	) && xrtTlsRecordEncode(
		XTLS_RECORD_HANDSHAKE, UINT16_C(0x0303),
		(xbytesview) { Message, sizeof(Message) }, Record, sizeof(Record)
	), "TLS fatal Alert backpressure input encoding failed");
	xrtTlsServerConfigInit(&Config);
	Config.Context = pContext;
	Config.Identity = pIdentity;
	pServer = xrtTlsServerCreate(&Config, NULL);
	testRequire((pServer != NULL) && (__xrtTlsSessionSend(
		pServer, pFill, pLimits->SendLimit
	) == XTLS_OK) && (xrtTlsSessionFeed(
		pServer, Record, sizeof(Record)
	) == XTLS_OK), "TLS fatal Alert backpressure setup failed");
	xrtClearError();
	testRequire((xrtTlsServerDrive(pServer) == XTLS_ERROR) &&
		(xrtTlsSessionState(pServer) == XTLS_STATE_FAILED) &&
		!pServer->FatalSent &&
		(xrtTlsSessionSendSize(pServer) == pLimits->SendLimit) &&
		(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_HANDSHAKE),
		"TLS fatal Alert backpressure replaced the root error");
	xrtTlsSessionDestroy(pServer);
	xrtFree(pFill);
}



/* READY 后的非法后握手消息必须产生可由对端认证的受保护 Alert。 */
static void testTlsServerAlertProtected(
	const xtlscontext* pContext,
	const xtlsidentity* pIdentity,
	xtlsverifier* pVerifier
)
{
	uint8 Message[XTLS_HANDSHAKE_HEADER_SIZE];
	test_tls_server_rng Rng = { UINT32_C(0xFA7A1135) };
	xtlssession* pClient;
	xtlssession* pServer;
	xtlsalertlevel Level = XTLS_ALERT_WARNING;
	xtlsalert Alert = XTLS_ALERT_CLOSE_NOTIFY;
	bool bProgress = false;

	testRequire(testTlsServerReady(
		(xtlscontext*)pContext, (xtlsidentity*)pIdentity,
		pVerifier, &Rng, &pClient, &pServer
	), "TLS protected alert handshake failed");
	testRequire(xrtTlsHandshakeEncode(
		XTLS_HANDSHAKE_CERTIFICATE_REQUEST,
		(xbytesview) { NULL, 0 }, Message, sizeof(Message)
	) && (__xrtTlsSessionRecordProtect(
		pClient, XTLS_RECORD_HANDSHAKE,
		(xbytesview) { Message, sizeof(Message) }, 0
	) == XTLS_OK), "TLS invalid post-handshake message encoding failed");
	while ( xrtTlsSessionSendSize(pClient) != 0 ) {
		testRequire(testTlsServerMove(
			pClient, pServer, &Rng, 13u, &bProgress
		), "TLS invalid post-handshake message move failed");
	}
	xrtClearError();
	testRequire((xrtTlsServerDrive(pServer) == XTLS_ERROR) &&
		(xrtTlsSessionState(pServer) == XTLS_STATE_FAILED) &&
		pServer->FatalSent &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_HANDSHAKE) &&
		(xrtTlsSessionSendSize(pServer) != 0),
		"TLS server did not protect its post-handshake fatal Alert");
	bProgress = false;
	while ( xrtTlsSessionSendSize(pServer) != 0 ) {
		testRequire(testTlsServerMove(
			pServer, pClient, &Rng, 11u, &bProgress
		), "TLS protected fatal Alert move failed");
	}
	xrtClearError();
	testRequire((xrtTlsClientDrive(pClient) == XTLS_ERROR) &&
		(xrtTlsSessionState(pClient) == XTLS_STATE_FAILED) &&
		xrtTlsSessionPeerAlert(pClient, &Level, &Alert) &&
		(Level == XTLS_ALERT_FATAL) &&
		(Alert == XTLS_ALERT_UNEXPECTED_MESSAGE) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_ALERT),
		"TLS client did not authenticate the server fatal Alert");
	xrtTlsSessionDestroy(pServer);
	xrtTlsSessionDestroy(pClient);
}



/* 验证服务端在明文和受保护 epoch 都会尽力发送 fatal Alert。 */
int main(void)
{
	xtlscontext* pContext = testTlsServerContext();
	xtlsidentity* pIdentity = testTlsServerIdentity();
	xtlsverifierconfig VerifierConfig;
	xtlsverifier* pVerifier;

	testRequire((pContext != NULL) && (pIdentity != NULL),
		"TLS server alert fixture creation failed");
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(pVerifier != NULL,
		"TLS server alert verifier creation failed");
	testTlsServerAlertPlain(pContext, pIdentity);
	testTlsServerAlertBackpressure(pContext, pIdentity);
	testTlsServerAlertProtected(pContext, pIdentity, pVerifier);
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	return 0;
}
