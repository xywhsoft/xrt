#include "../fixtures/tls_server.h"
#include "../../src/internal/xrt_tls_server.h"

/* 用测试密文占满发送硬上限，但不改变服务端 TLS epoch。 */
static bytes testTlsServerLimitsFill(
	xtlssession* pServer,
	size_t iRequired,
	size_t* pFillSize
)
{
	const xtlslimits* pLimits = xrtTlsContextLimits(
		xrtTlsSessionContext(pServer)
	);
	bytes pFill;

	*pFillSize = 0;
	if ( (pLimits == NULL) || (iRequired == 0) ||
		(iRequired > pLimits->SendLimit) ) {
		return NULL;
	}
	*pFillSize = pLimits->SendLimit - iRequired + 1u;
	pFill = (bytes)xrtMalloc(*pFillSize);
	if ( pFill == NULL ) {
		return NULL;
	}
	memset(pFill, 0xA7, *pFillSize);
	if ( __xrtTlsSessionSend(
		pServer, pFill, *pFillSize
	) != XTLS_OK ) {
		xrtFree(pFill);
		return NULL;
	}
	return pFill;
}



/* 主动 KeyUpdate 在参数错误和背压下不得修改写 epoch。 */
static void testTlsServerKeyUpdateLimits(
	xtlssession* pServer,
	xtlsserverstate* pState
)
{
	uint8 Traffic[XTLS_SERVER_SECRET_MAX_SIZE];
	size_t iMessage = xrtTlsHandshakeSize(1u);
	size_t iRecord = __xrtTlsRecordSealSize(
		&pServer->WriteKey, iMessage, 0
	);
	size_t iFill;
	bytes pFill;
	uint64 iSequence = pServer->WriteKey.Sequence;

	memcpy(Traffic, pState->ServerApplicationTraffic, pState->HashSize);
	xrtClearError();
	testRequire((xrtTlsServerKeyUpdate(
		pServer, (xtlskeyupdate)2
	) == XTLS_ERROR) &&
		(xrtTlsSessionState(pServer) == XTLS_STATE_READY) &&
		(xrtTlsSessionSendSize(pServer) == 0) &&
		(pServer->WriteKey.Sequence == iSequence) &&
		(memcmp(
			pState->ServerApplicationTraffic,
			Traffic, pState->HashSize
		) == 0) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_HANDSHAKE),
		"TLS server KeyUpdate accepted an invalid request");
	pFill = testTlsServerLimitsFill(pServer, iRecord, &iFill);
	testRequire(pFill != NULL,
		"TLS server KeyUpdate backpressure setup failed");
	xrtFree(pFill);
	testRequire((xrtTlsServerKeyUpdate(
		pServer, XTLS_KEY_UPDATE_REQUESTED
	) == XTLS_AGAIN) &&
		(xrtTlsSessionState(pServer) == XTLS_STATE_READY) &&
		(xrtTlsSessionSendSize(pServer) == iFill) &&
		(pServer->WriteKey.Sequence == iSequence) &&
		((xrtTlsSessionWait(pServer) & XTLS_WAIT_OUTPUT) != 0) &&
		(memcmp(
			pState->ServerApplicationTraffic,
			Traffic, pState->HashSize
		) == 0),
		"TLS server KeyUpdate backpressure changed the write epoch");
	testRequire(xrtTlsSessionSendConsume(pServer, iFill) &&
		(xrtTlsServerKeyUpdate(
			pServer, XTLS_KEY_UPDATE_REQUESTED
		) == XTLS_OK) &&
		(xrtTlsSessionSendSize(pServer) == iRecord) &&
		(pServer->WriteKey.Sequence == 0) &&
		(memcmp(
			pState->ServerApplicationTraffic,
			Traffic, pState->HashSize
		) != 0),
		"TLS server KeyUpdate did not recover after output drain");
	testRequire(xrtTlsSessionSendConsume(pServer, iRecord),
		"TLS server KeyUpdate output consumption failed");
	xrtSecureZero(Traffic, sizeof(Traffic));
	xrtClearError();
}



/* NewSessionTicket 必须在随机数、派生和分配前执行发送上限预检。 */
static void testTlsServerTicketLimits(
	xtlssession* pServer,
	xtlsserverstate* pState
)
{
	uint8 Ticket[32];
	uint8 Nonce[8] = { 0 };
	uint8 Traffic[XTLS_SERVER_SECRET_MAX_SIZE];
	xtlssessionticket Message;
	xtlsresume* pResume = (xtlsresume*)(uintptr_t)1u;
	size_t iBody;
	size_t iMessage;
	size_t iRecord;
	size_t iFill;
	bytes pFill;
	uint64 iSequence = pServer->WriteKey.Sequence;

	memset(Ticket, 0x6Du, sizeof(Ticket));
	memset(&Message, 0, sizeof(Message));
	memcpy(Traffic, pState->ServerApplicationTraffic, pState->HashSize);
	Message.Version = XTLS_VERSION_13;
	Message.Lifetime = 60u;
	Message.Nonce = (xbytesview) { Nonce, sizeof(Nonce) };
	Message.Ticket = (xbytesview) { Ticket, sizeof(Ticket) };
	Message.Extensions = (xbytesview) { NULL, 0 };
	iBody = xrtTlsSessionTicketSize(&Message);
	iMessage = xrtTlsHandshakeSize(iBody);
	iRecord = __xrtTlsRecordSealSize(
		&pServer->WriteKey, iMessage, 0
	);
	pFill = testTlsServerLimitsFill(pServer, iRecord, &iFill);
	testRequire(pFill != NULL,
		"TLS server ticket backpressure setup failed");
	xrtFree(pFill);
	testRequire((xrtTlsServerTicket(
		pServer, (xbytesview) { Ticket, sizeof(Ticket) },
		60u, &pResume
	) == XTLS_AGAIN) && (pResume == NULL) &&
		(xrtTlsSessionState(pServer) == XTLS_STATE_READY) &&
		(xrtTlsSessionSendSize(pServer) == iFill) &&
		(pServer->WriteKey.Sequence == iSequence) &&
		((xrtTlsSessionWait(pServer) & XTLS_WAIT_OUTPUT) != 0) &&
		(memcmp(
			pState->ServerApplicationTraffic,
			Traffic, pState->HashSize
		) == 0),
		"TLS server ticket backpressure changed session state");
	testRequire(xrtTlsSessionSendConsume(pServer, iFill) &&
		(xrtTlsServerTicket(
			pServer, (xbytesview) { Ticket, sizeof(Ticket) },
			60u, &pResume
		) == XTLS_OK) && (pResume != NULL) &&
		(xrtTlsSessionSendSize(pServer) == iRecord) &&
		(pServer->WriteKey.Sequence == (iSequence + 1u)) &&
		(memcmp(
			pState->ServerApplicationTraffic,
			Traffic, pState->HashSize
		) == 0),
		"TLS server ticket did not recover after output drain");
	testRequire(xrtTlsSessionSendConsume(pServer, iRecord),
		"TLS server ticket output consumption failed");
	xrtTlsResumeRelease(pResume);
	xrtSecureZero(Traffic, sizeof(Traffic));
	xrtSecureZero(Ticket, sizeof(Ticket));
}



/* 验证服务端后握手写操作的参数、背压和重试原子性。 */
int main(void)
{
	xtlscontext* pContext = testTlsServerContext();
	xtlsidentity* pIdentity = testTlsServerIdentity();
	xtlsverifierconfig VerifierConfig;
	xtlsverifier* pVerifier;
	xtlssession* pClient;
	xtlssession* pServer;
	xtlsserverstate* pState;
	test_tls_server_rng Rng = { UINT32_C(0x71A17E55) };

	testRequire((pContext != NULL) && (pIdentity != NULL),
		"TLS server limits fixture creation failed");
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(pVerifier != NULL,
		"TLS server limits verifier creation failed");
	testRequire(testTlsServerReady(
		pContext, pIdentity, pVerifier, &Rng, &pClient, &pServer
	), "TLS server limits handshake failed");
	pState = (xtlsserverstate*)__xrtTlsSessionRoleData(pServer);
	testRequire((pState != NULL) && pState->ResumptionReady &&
		(pState->Step == XTLS_SERVER_READY),
		"TLS server limits role state is not ready");

	testTlsServerKeyUpdateLimits(pServer, pState);
	testTlsServerTicketLimits(pServer, pState);

	xrtTlsSessionDestroy(pServer);
	xrtTlsSessionDestroy(pClient);
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	return 0;
}
