#include "../test.h"



/* 验证空参数、状态查询和跨层硬限不会静默失败。 */

static xtlsverifydecision testTlsStreamInvalidAcceptPeer(
	const xtlspeer* pPeer,
	ptr pContext
)
{
	(void)pPeer;
	(void)pContext;
	return XTLS_VERIFY_ACCEPT;
}



int main(void)
{
	xnetengineconfig EngineConfig;
	xtlscontext* pTlsContext;
	xtlsverifier* pVerifier;
	xtlsclientconfig TlsConfig;
	xnetstreamconfig Transport;
	xtlsstreamconfig StreamConfig;
	xnetengine* pEngine;
	xnetaddr Address;
	xnetspan InvalidSpan = { NULL, 1u };
	xnetspan OutputSpan;
	size_t iRead = 1u;
	size_t iWritten = 1u;

	xrtTlsStreamConfigInit(&StreamConfig);
	testRequire(
		(StreamConfig.HandshakeTimeout ==
			XTLS_STREAM_HANDSHAKE_TIMEOUT_DEFAULT) &&
		(StreamConfig.CloseTimeout ==
			XTLS_STREAM_CLOSE_TIMEOUT_DEFAULT),
		"TLS stream default config mismatch");
	xrtClearError();
	xrtTlsStreamConfigInit(NULL);
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS stream null config error mismatch");
	xrtClearError();
	testRequire((xrtTlsStreamRef(NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS stream null retain error mismatch");
	xrtClearError();
	testRequire((xrtTlsStreamSend(
		NULL, NULL, 0, &iWritten
	) == XTLS_ERROR) && (iWritten == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS stream null send error mismatch");
	xrtClearError();
	iWritten = 1u;
	testRequire((xrtTlsStreamSendVec(
		NULL, &InvalidSpan, 1u, &iWritten
	) == XTLS_ERROR) && (iWritten == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS stream null vector send error mismatch");
	xrtClearError();
	testRequire(!xrtTlsStreamConsume(NULL, 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS stream null consume error mismatch");
	xrtClearError();
	testRequire((xrtTlsStreamRead(
		NULL,
		NULL,
		0,
		&iRead
	) == XTLS_ERROR) && (iRead == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS stream null read error mismatch");
	xrtClearError();
	testRequire((xrtTlsStreamBuffer(NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS stream null buffer error mismatch");
	xrtClearError();
	testRequire(!xrtTlsStreamPullup(NULL, 1u, &OutputSpan) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS stream null pullup error mismatch");
	xrtClearError();
	testRequire(!xrtTlsStreamReadMore(NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS stream null ReadMore error mismatch");
	xrtClearError();
	testRequire((xrtTlsStreamSession(NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS stream null session error mismatch");
	xrtClearError();
	testRequire(!xrtTlsStreamSetEvents(NULL, NULL, NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS stream null event takeover error mismatch");
	xrtClearError();
	testRequire(!xrtTlsStreamClose(NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS stream null close error mismatch");
	xrtClearError();
	testRequire(!xrtTlsStreamAbort(NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS stream null abort error mismatch");
	xrtClearError();
	testRequire((xrtTlsStreamState(NULL) == XTLS_STREAM_FAILED) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS stream null state error mismatch");
	testRequire((xrtTlsStreamAvailable(NULL) == 0) &&
		(xrtTlsStreamPending(NULL) == 0) &&
		(xrtTlsStreamTransport(NULL) == NULL) &&
		(xrtTlsStreamData(NULL) == NULL) &&
		(xrtTlsStreamError(NULL) == NULL),
		"TLS stream null snapshot mismatch");
	xrtClearError();
	testRequire(!xrtTlsStreamAccept(
		NULL, NULL, NULL, NULL, NULL, NULL
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS stream null accept error mismatch");
	xrtClearError();
	testRequire(!xrtTlsStreamAttach(
		NULL, NULL, NULL, NULL, NULL, NULL
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS stream null attach error mismatch");
	xrtClearError();
	testRequire(!xrtTlsStreamClient(
		NULL, NULL, NULL, NULL, NULL, NULL
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS stream null client error mismatch");
	xrtClearError();
	testRequire((xrtTlsStreamConnect(
		NULL, NULL, 0, NULL, NULL, NULL, NULL, NULL
	) == NULL) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS stream null connect error mismatch");

	xrtNetEngineConfigInit(&EngineConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetAddrLoopback(
		&Address,
		XNET_FAMILY_IPV4,
		443
	), "TLS stream invalid fixture creation failed");
	pTlsContext = xrtTlsContextCreate(NULL);
	testRequire(pTlsContext != NULL,
		"TLS stream invalid context creation failed");
	{
		xtlsverifierconfig VerifierConfig;

		xrtTlsVerifierConfigInit(&VerifierConfig);
		VerifierConfig.Verify = testTlsStreamInvalidAcceptPeer;
		pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	}
	testRequire(pVerifier != NULL,
		"TLS stream invalid verifier creation failed");
	xrtTlsClientConfigInit(&TlsConfig);
	TlsConfig.Context = pTlsContext;
	TlsConfig.Verifier = pVerifier;
	xrtNetStreamConfigInit(&Transport);
	Transport.WriteHighWater = 1u;
	Transport.WriteLowWater = 0;
	Transport.WriteLimit = 1u;
	xrtClearError();
	testRequire((xrtTlsStreamConnect(
		pEngine,
		&Address,
		0,
		&Transport,
		&TlsConfig,
		NULL,
		NULL,
		NULL
	) == NULL) && (xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_LIMIT),
		"TLS stream cross-layer write limit was not enforced");
	testRequire(xrtNetEngineDestroy(pEngine),
		"TLS stream invalid engine destroy failed");
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsContextRelease(pTlsContext);
	return 0;
}
