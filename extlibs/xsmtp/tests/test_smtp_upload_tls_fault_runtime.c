#include <xrt/detail/wait.h>
#include "test.h"
#include "test_tls.h"
#include "../../../tests/fixtures/mail_tls_partial.h"
#include "../src/internal/xrt_smtp_client.h"

#define TEST_SMTP_TLS_PREFIX (4u * 1024u * 1024u)
#define TEST_SMTP_TLS_WRITE (4u * 1024u * 1024u)
#define TEST_SMTP_TLS_TOTAL (TEST_SMTP_TLS_PREFIX + TEST_SMTP_TLS_WRITE)

typedef enum testsmtptlsfault {
	TEST_SMTP_TLS_CANCEL,
	TEST_SMTP_TLS_TIMEOUT,
	TEST_SMTP_TLS_DISCONNECT
} testsmtptlsfault;

typedef struct testsmtptlsserver {
	xnetlistener* Listener;
	const xtlsserverconfig* Tls;
	double Deadline;
	testsmtptlsfault Mode;
	xcancel* Cancel;
	xtlsstream* ClientTls;
	uint64 PrefixAccepted;
	xatomic32 ClientReady;
	xatomic32 PrefixReady;
	xatomic32 Partial;
	xatomic32 Returned;
	size_t Received;
	bool Bdat;
	bool Success;
} testsmtptlsserver;

static xnetaddr TestSmtpTlsAddress;
static const unsigned char* TestSmtpTlsPayload;

static xnetaddrlist* testSmtpTlsResolve(cstr sHost, xnetfamily Family, ptr pData)
{
	xnetaddr Address = TestSmtpTlsAddress;
	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "smtp-partial.test") != 0 ) return NULL;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}

/* 只有第二次 Write 的同一 Future 已部分提交且尚未完成时才触发故障。
 * 相比第一轮检查点，已发送加待发密文增加，证明第二次 Write 已推进；
 * 4 MiB 异步预算仍占用，且唯一操作未退休，证明它尚未完成。 */
static bool testSmtpTlsWaitPartial(testsmtptlsserver* pServer)
{
	if ( !testMailPartialWaitFlag(&pServer->ClientReady, pServer->Deadline) ||
		!testMailPartialWaitFlag(&pServer->PrefixReady, pServer->Deadline) ) return false;
	if ( !testMailPartialWaitProgress(pServer->ClientTls, pServer->PrefixAccepted,
		TEST_SMTP_TLS_WRITE, &pServer->Returned, pServer->Deadline) ) return false;
	xrtAtomic32Store(&pServer->Partial, 1u, XMEMORY_RELEASE);
	return true;
}

/* 恢复接收后须在客户端 Destroy 之前看到异常关闭，不能收到完整 body
 * 或新命令。Abort 前已经进入内核的字节可能继续到达，不要求网络回滚。 */
static bool testSmtpTlsDrain(testsmtptlsserver* pServer, xtlsstream* pTls)
{
	while ( !__xrtWaitExpired(pServer->Deadline) ) {
		xfuture* pRead = xrtTlsStreamRecvAsync(pTls, 0);
		xfuturestate State;
		if ( pRead == NULL ) return false;
		if ( __xrtFutureWaitUntil(pRead, pServer->Deadline) != XWAIT_OK ) {
			xrtFutureDestroy(pRead);
			return false;
		}
		State = xrtFutureState(pRead);
		if ( State == XFUTURE_FAILED || State == XFUTURE_CLOSED ) {
			xrtFutureDestroy(pRead);
			return pServer->Received > 0 && pServer->Received < TEST_SMTP_TLS_TOTAL;
		}
		if ( State != XFUTURE_RESOLVED ) {
			xrtFutureDestroy(pRead);
			return false;
		}
		xbytesview Data = xrtNetBytesView((const xnetbytes*)xrtFutureValue(pRead));
		bool matched = Data.Size != 0 && Data.Size <= TEST_SMTP_TLS_TOTAL - pServer->Received;
		for ( size_t i = 0; matched && i < Data.Size; i++ ) {
			unsigned char Expected = TestSmtpTlsPayload[pServer->Received + i];
			if ( Data.Data[i] != Expected ) matched = false;
		}
		pServer->Received += Data.Size;
		xrtFutureDestroy(pRead);
		if ( !matched || pServer->Received >= TEST_SMTP_TLS_TOTAL ) return false;
	}
	return false;
}

static int32 testSmtpTlsServer(ptr pData)
{
	testsmtptlsserver* pServer = (testsmtptlsserver*)pData;
	xnetstream* pTcp = __xrtNetListenerAcceptWait(pServer->Listener, pServer->Deadline, NULL);
	xtlsstream* pTls;
	bool Success;
	if ( pTcp == NULL ) return 1;
	testMailPartialSmallSocket(pTcp, pServer->Deadline);
	pTls = testMailTlsUpgrade(&pTcp, pServer->Tls, pServer->Deadline);
	if ( pTls == NULL ) {
		xrtNetStreamDestroy(pTcp);
		return 2;
	}
	Success = testMailTlsSend(pTls, "220 smtp-partial.test ready\r\n",
		sizeof("220 smtp-partial.test ready\r\n") - 1u, pServer->Deadline) &&
		testMailTlsReceive(pTls, "EHLO tls-partial.test\r\n",
			sizeof("EHLO tls-partial.test\r\n") - 1u, pServer->Deadline) &&
		testMailTlsSend(pTls, "250-smtp-partial.test\r\n250 CHUNKING\r\n",
			sizeof("250-smtp-partial.test\r\n250 CHUNKING\r\n") - 1u, pServer->Deadline) &&
		testMailTlsReceive(pTls, "MAIL FROM:<sender@test>\r\n",
			sizeof("MAIL FROM:<sender@test>\r\n") - 1u, pServer->Deadline) &&
		testMailTlsSend(pTls, "250 sender accepted\r\n",
			sizeof("250 sender accepted\r\n") - 1u, pServer->Deadline) &&
		testMailTlsReceive(pTls, "RCPT TO:<target@test>\r\n",
			sizeof("RCPT TO:<target@test>\r\n") - 1u, pServer->Deadline) &&
		testMailTlsSend(pTls, "250 recipient accepted\r\n",
			sizeof("250 recipient accepted\r\n") - 1u, pServer->Deadline);
	if ( Success && pServer->Bdat ) {
		char Command[64];
		int Size = snprintf(Command, sizeof(Command), "BDAT %u LAST\r\n", TEST_SMTP_TLS_TOTAL);
		Success = Size > 0 && (size_t)Size < sizeof(Command) &&
			testMailTlsReceive(pTls, Command, (size_t)Size, pServer->Deadline);
	} else if ( Success ) {
		Success = testMailTlsReceive(pTls, "DATA\r\n", sizeof("DATA\r\n") - 1u,
			pServer->Deadline) && testMailTlsSend(pTls, "354 send data\r\n",
				sizeof("354 send data\r\n") - 1u, pServer->Deadline);
	}
	if ( Success ) {
		xfuture* pFirst = xrtTlsStreamRecvAsync(pTls, 1u);
		Success = testMailTlsFuture(pFirst, pServer->Deadline);
		if ( Success ) {
			xbytesview Data = xrtNetBytesView((const xnetbytes*)xrtFutureValue(pFirst));
			Success = Data.Size == 1u && Data.Data[0] == TestSmtpTlsPayload[0];
			pServer->Received = Data.Size;
		}
		xrtFutureDestroy(pFirst);
	}
	Success = Success && testSmtpTlsWaitPartial(pServer);
	if ( Success && pServer->Mode == TEST_SMTP_TLS_CANCEL )
		Success = xrtCancelRequest(pServer->Cancel);
	if ( Success && pServer->Mode == TEST_SMTP_TLS_DISCONNECT )
		Success = xrtTlsStreamAbort(pTls);
	Success = Success && testMailPartialWaitFlag(&pServer->Returned, pServer->Deadline);
	if ( Success && pServer->Mode != TEST_SMTP_TLS_DISCONNECT )
		Success = testSmtpTlsDrain(pServer, pTls);
	(void)xrtTlsStreamAbort(pTls);
	while ( xrtTlsStreamState(pTls) != XTLS_STREAM_FAILED &&
		xrtTlsStreamState(pTls) != XTLS_STREAM_CLOSED ) {
		if ( __xrtWaitExpired(pServer->Deadline) ) { Success = false; break; }
		xrtThreadYield();
	}
	xrtTlsStreamDestroy(pTls);
	pServer->Success = Success;
	return Success ? 0 : 3;
}

/* DATA 编码器与 BDAT 原始块均须验证同一次底层 TLS Future。 */
static bool testSmtpTlsWrite(xsmtpclient* pClient, bool Bdat, const void* pData,
	size_t Size, double Deadline, xcancel* pCancel)
{
	xbytesview Data = { (const unsigned char*)pData, Size };
	return Bdat ? __xrtSmtpClientBdatWrite(pClient, Data, Deadline, pCancel) :
		__xrtSmtpClientDataWrite(pClient, Data, Deadline, pCancel);
}

int main(void)
{
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetresolverconfig ResolverConfig;
	xsmtpclientconfig Config;
	xtlsserverconfig TlsServer;
	xtlsverifierconfig VerifyConfig;
	xtlslimits Limits;
	xrtTlsLimitsInit(&Limits);
	Limits.SendLimit = 32u * 1024u;
	xtlscontext* pContext = testTlsServerContextWithLimits(&Limits);
	xtlsidentity* pIdentity = testTlsServerIdentity();
	testRequire(pContext != NULL && pIdentity != NULL, "SMTP TLS fault identity creation failed");
	xrtTlsVerifierConfigInit(&VerifyConfig);
	VerifyConfig.Verify = testTlsServerAccept;
	xtlsverifier* pVerifier = xrtTlsVerifierCreate(&VerifyConfig);
	testRequire(pVerifier != NULL, "SMTP TLS fault verifier creation failed");
	xrtTlsServerConfigInit(&TlsServer);
	TlsServer.Context = pContext;
	TlsServer.Identity = pIdentity;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2u;
	xnetengine* pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(pEngine != NULL && xrtNetEngineStart(pEngine), "SMTP TLS fault engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	ListenConfig.Stream.ReadLimit = 32u * 1024u;
	testRequire(xrtNetAddrLoopback(&ListenConfig.Address, XNET_FAMILY_IPV4, 0),
		"SMTP TLS fault address creation failed");
	xnetlistener* pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire(pListener != NULL && xrtNetListenerLocal(pListener, &TestSmtpTlsAddress),
		"SMTP TLS fault listener creation failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testSmtpTlsResolve;
	ResolverConfig.CacheEntries = 0;
	xnetresolver* pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "SMTP TLS fault resolver creation failed");
	xrtSmtpClientConfigInit(&Config);
	Config.Net.Engine = pEngine;
	Config.Net.Resolver = pResolver;
	Config.Net.Host = "smtp-partial.test";
	Config.Net.Port = TestSmtpTlsAddress.Port;
	Config.Net.Security = XMAIL_SECURITY_TLS;
	Config.Net.Tls.Context = pContext;
	Config.Net.Tls.Verifier = pVerifier;
	Config.Hello = (xstrview)XRT_STR_LITERAL("tls-partial.test");
	Config.Net.WriteChunk = TEST_SMTP_TLS_WRITE;
	Config.Net.TlsStream.AsyncBytesLimit = TEST_SMTP_TLS_WRITE;
	Config.Net.Dial.Stream.WriteLimit = TEST_SMTP_TLS_WRITE;
	Config.Net.Dial.Stream.ReadLimit = 32u * 1024u;
	testRequire(xrtSmtpClientConfigValid(&Config), "SMTP TLS fault configuration invalid");
	unsigned char* pPayload = (unsigned char*)xrtMalloc(TEST_SMTP_TLS_TOTAL);
	testRequire(pPayload != NULL, "SMTP TLS fault payload allocation failed");
	memset(pPayload, 'P', TEST_SMTP_TLS_PREFIX);
	memset(pPayload + TEST_SMTP_TLS_PREFIX, 'Q', TEST_SMTP_TLS_WRITE);
	for ( size_t i = 0; i < TEST_SMTP_TLS_TOTAL; i += 512u ) {
		pPayload[i + 510u] = '\r';
		pPayload[i + 511u] = '\n';
	}
	pPayload[0] = '\r';
	pPayload[1] = '\n';
	TestSmtpTlsPayload = pPayload;

	for ( int Bdat = 0; Bdat <= 1; Bdat++ ) {
		for ( int Mode = TEST_SMTP_TLS_CANCEL; Mode <= TEST_SMTP_TLS_DISCONNECT; Mode++ ) {
			testsmtptlsserver Server;
			memset(&Server, 0, sizeof(Server));
			Server.Listener = pListener;
			Server.Tls = &TlsServer;
			Server.Mode = (testsmtptlsfault)Mode;
			Server.Bdat = Bdat != 0;
			Server.Deadline = __xrtWaitAfter(UINT64_C(15000000));
			Server.Cancel = xrtCancelCreate();
			xrtAtomic32Init(&Server.ClientReady, 0u);
			xrtAtomic32Init(&Server.PrefixReady, 0u);
			xrtAtomic32Init(&Server.Partial, 0u);
			xrtAtomic32Init(&Server.Returned, 0u);
			testRequire(Server.Cancel != NULL, "SMTP TLS fault cancel creation failed");
			xthread* pThread = xrtThreadCreate(testSmtpTlsServer, &Server, 0);
			testRequire(pThread != NULL, "SMTP TLS fault server thread creation failed");
			xsmtpclient* pClient = __xrtSmtpClientOpen(&Config, Server.Deadline, NULL);
			testRequire(pClient != NULL && xrtSmtpClientState(pClient) == XSMTP_CLIENT_READY,
				"SMTP TLS fault client open failed");
			Server.ClientTls = xrtTlsStreamRef(pClient->Transport.Tls);
			testRequire(Server.ClientTls != NULL, "SMTP TLS fault transport reference missing");
			testMailPartialSmallSocket(xrtTlsStreamTransport(Server.ClientTls), Server.Deadline);
			xrtAtomic32Store(&Server.ClientReady, 1u, XMEMORY_RELEASE);
			testRequire(__xrtSmtpClientMail(pClient, XRT_STR_LITERAL("sender@test"),
				XRT_STR_LITERAL(""), Server.Deadline, NULL) &&
				__xrtSmtpClientRcpt(pClient, XRT_STR_LITERAL("target@test"), XRT_STR_LITERAL(""),
					Server.Deadline, NULL), "SMTP TLS fault envelope failed");
			testRequire(Bdat ? __xrtSmtpClientBdatBegin(pClient, TEST_SMTP_TLS_TOTAL, true,
				Server.Deadline, NULL) : __xrtSmtpClientDataBegin(pClient, Server.Deadline, NULL),
				"SMTP TLS fault upload begin failed");
			testRequire(testSmtpTlsWrite(pClient, Server.Bdat, pPayload, TEST_SMTP_TLS_PREFIX,
				Server.Deadline, NULL), "SMTP TLS fault prefix write failed");
			testmailpartialsnapshot Prefix;
			testRequire(testMailPartialSnapshot(Server.ClientTls, Server.Deadline, &Prefix) &&
				Prefix.AsyncBytes == 0, "SMTP TLS prefix checkpoint failed");
			Server.PrefixAccepted = Prefix.Accepted;
			xrtAtomic32Store(&Server.PrefixReady, 1u, XMEMORY_RELEASE);
			xrtClearError();
			bool Written = testSmtpTlsWrite(pClient, Server.Bdat, pPayload + TEST_SMTP_TLS_PREFIX,
				TEST_SMTP_TLS_WRITE, Mode == TEST_SMTP_TLS_TIMEOUT ?
				__xrtWaitAfter(UINT64_C(3000000)) : Server.Deadline,
				Mode == TEST_SMTP_TLS_CANCEL ? Server.Cancel : NULL);
			xerror* pFailure = xrtTakeError();
			xsmtpreply Last;
			bool FailedAfterProgress = !Written && pFailure != NULL &&
				xrtSmtpClientState(pClient) == XSMTP_CLIENT_FAILED &&
				(Bdat ? pClient->ChunkRemaining == TEST_SMTP_TLS_WRITE : pClient->DataWriter.Finished) &&
				xrtAtomic32Load(&Server.Partial, XMEMORY_ACQUIRE);
			if ( !FailedAfterProgress ) fprintf(stderr,
				"[SMTP TLS fault] bdat=%d mode=%d written=%d kind=%d code=%d state=%d "
				"remaining=%zu partial=%u async=%zu pending=%zu\n", Bdat, Mode, (int)Written,
				(int)xrtErrorKind(pFailure), (int)xrtErrorCode(pFailure),
				(int)xrtSmtpClientState(pClient), pClient->ChunkRemaining,
				(unsigned)xrtAtomic32Load(&Server.Partial, XMEMORY_ACQUIRE),
				xrtTlsStreamAsyncBytes(Server.ClientTls), xrtTlsStreamPending(Server.ClientTls));
			testRequire(FailedAfterProgress,
				"SMTP TLS partial write did not fail after observed progress");
			testRequire(xrtSmtpClientLastReply(pClient, &Last) &&
				Last.Code == (Bdat ? 250 : 354) && Last.Lines == 1u &&
				testMailViewEqual(Last.Text, Bdat ? (xstrview)XRT_STR_LITERAL("recipient accepted") :
					(xstrview)XRT_STR_LITERAL("send data")),
				"SMTP TLS partial write lost the last complete reply");
			if ( Mode == TEST_SMTP_TLS_CANCEL || Mode == TEST_SMTP_TLS_TIMEOUT ) {
				testRequire(xrtErrorKind(pFailure) == (Mode == TEST_SMTP_TLS_CANCEL ?
					XERR_CANCELLED : XERR_TIMEOUT) && xrtErrorCode(pFailure) == XMAIL_ERROR_PROTOCOL &&
					strcmp(xrtErrorDomain(pFailure), "xrt.mail") == 0,
					"SMTP TLS partial write lost its cancellation/timeout diagnostic");
			} else {
				const xerror* pTlsError = xrtTlsStreamError(Server.ClientTls);
				testRequire(pTlsError != NULL && xrtErrorKind(pFailure) == xrtErrorKind(pTlsError) &&
					xrtErrorCode(pFailure) == xrtErrorCode(pTlsError) &&
					strcmp(xrtErrorDomain(pFailure), xrtErrorDomain(pTlsError)) == 0,
					"SMTP TLS partial write lost the peer disconnect diagnostic");
			}
			xrtErrorFree(pFailure);
			testRequire(!testSmtpTlsWrite(pClient, Server.Bdat, "X", 1u, Server.Deadline, NULL) &&
				xrtErrorKind(xrtGetError()) == XERR_STATE,
				"SMTP TLS failed upload session accepted another write");
			xrtAtomic32Store(&Server.Returned, 1u, XMEMORY_RELEASE);
			testRequire(__xrtThreadWaitUntil(pThread, Server.Deadline) == XWAIT_OK &&
				Server.Success && xrtThreadExitCode(pThread) == 0,
				"SMTP TLS partial write did not abort before client destruction");
			xrtThreadDestroy(pThread);
			xrtSmtpClientDestroy(pClient);
			xrtTlsStreamDestroy(Server.ClientTls);
			xrtCancelDestroy(Server.Cancel);
		}
	}
	xrtFree(pPayload);
	testRequire(xrtNetListenerClose(pListener), "SMTP TLS fault listener close failed");
	double Retire = __xrtWaitAfter(UINT64_C(3000000));
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		testRequire(!__xrtWaitExpired(Retire), "SMTP TLS fault listener did not close");
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver), "SMTP TLS fault resolver destroy failed");
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	for ( ;; ) {
		xnetretireresult Result = xrtNetEngineTryDestroy(pEngine);
		if ( Result == XNET_RETIRE_READY ) break;
		testRequire(Result != XNET_RETIRE_ERROR && !__xrtWaitExpired(Retire),
			"SMTP TLS fault engine did not retire");
		xrtThreadYield();
	}
	return 0;
}
