#include "test.h"
#include "test_tls.h"
#include "../../../tests/fixtures/mail_tls_partial.h"
#include "../src/internal/xrt_imap_client.h"

#define TEST_IMAP_TLS_PREFIX (4u * 1024u * 1024u)
#define TEST_IMAP_TLS_WRITE (4u * 1024u * 1024u)
#define TEST_IMAP_TLS_TOTAL (TEST_IMAP_TLS_PREFIX + TEST_IMAP_TLS_WRITE)

typedef enum testimaptlsfault {
	TEST_IMAP_TLS_CANCEL,
	TEST_IMAP_TLS_TIMEOUT,
	TEST_IMAP_TLS_DISCONNECT
} testimaptlsfault;

typedef struct testimaptlsserver {
	xnetlistener* Listener;
	const xtlsserverconfig* Tls;
	xdeadline Deadline;
	testimaptlsfault Mode;
	xcancel* Cancel;
	xtlsstream* ClientTls;
	uint64 PrefixAccepted;
	xatomic32 ClientReady;
	xatomic32 PrefixReady;
	xatomic32 Partial;
	xatomic32 Returned;
	size_t Received;
	bool Success;
} testimaptlsserver;

static xnetaddr TestImapTlsAddress;

static xnetaddrlist* testImapTlsResolve(cstr sHost, xnetfamily Family, ptr pData)
{
	xnetaddr Address = TestImapTlsAddress;
	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "imap-partial.test") != 0 ) return NULL;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}

/* 只有第二次 Write 的同一 Future 已部分提交且尚未完成时才触发故障。
 * 相比第一轮检查点，已发送加待发密文增加，证明第二次 Write 已推进；
 * 4 MiB 异步预算仍占用，且唯一操作未退休，证明它尚未完成。 */
static bool testImapTlsWaitPartial(testimaptlsserver* pServer)
{
	if ( !testMailPartialWaitFlag(&pServer->ClientReady, pServer->Deadline) ||
		!testMailPartialWaitFlag(&pServer->PrefixReady, pServer->Deadline) ) return false;
	if ( !testMailPartialWaitProgress(pServer->ClientTls, pServer->PrefixAccepted,
		TEST_IMAP_TLS_WRITE, &pServer->Returned, pServer->Deadline) ) return false;
	xrtAtomic32Store(&pServer->Partial, 1u, XMEMORY_RELEASE);
	return true;
}

/* 恢复接收后须在客户端 Destroy 之前看到异常关闭，不能收到完整 literal
 * 或新命令。Abort 前已经进入内核的字节可能继续到达，不要求网络回滚。 */
static bool testImapTlsDrain(testimaptlsserver* pServer, xtlsstream* pTls)
{
	while ( !xrtDeadlineExpired(pServer->Deadline) ) {
		xfuture* pRead = xrtTlsStreamRecvAsync(pTls, 0);
		xfuturestate State;
		if ( pRead == NULL ) return false;
		if ( xrtFutureWaitUntil(pRead, pServer->Deadline) != XWAIT_OK ) {
			xrtFutureDestroy(pRead);
			return false;
		}
		State = xrtFutureState(pRead);
		if ( State == XFUTURE_FAILED || State == XFUTURE_CLOSED ) {
			xrtFutureDestroy(pRead);
			return pServer->Received > 0 && pServer->Received < TEST_IMAP_TLS_TOTAL;
		}
		if ( State != XFUTURE_RESOLVED ) {
			xrtFutureDestroy(pRead);
			return false;
		}
		xbytesview Data = xrtNetBytesView((const xnetbytes*)xrtFutureValue(pRead));
		bool matched = Data.Size != 0 && Data.Size <= TEST_IMAP_TLS_TOTAL - pServer->Received;
		for ( size_t i = 0; matched && i < Data.Size; i++ ) {
			unsigned char Expected = pServer->Received + i < TEST_IMAP_TLS_PREFIX ? 'P' : 'Q';
			if ( Data.Data[i] != Expected ) matched = false;
		}
		pServer->Received += Data.Size;
		xrtFutureDestroy(pRead);
		if ( !matched || pServer->Received >= TEST_IMAP_TLS_TOTAL ) return false;
	}
	return false;
}

static int32 testImapTlsServer(ptr pData)
{
	testimaptlsserver* pServer = (testimaptlsserver*)pData;
	xnetstream* pTcp = xrtNetListenerAcceptWait(pServer->Listener, pServer->Deadline, NULL);
	xtlsstream* pTls;
	bool Success;
	if ( pTcp == NULL ) return 1;
	testMailPartialSmallSocket(pTcp, pServer->Deadline);
	pTls = testMailTlsUpgrade(&pTcp, pServer->Tls, pServer->Deadline);
	if ( pTls == NULL ) {
		xrtNetStreamDestroy(pTcp);
		return 2;
	}
	Success = testMailTlsSend(pTls, "* PREAUTH imap-partial.test ready\r\n",
		sizeof("* PREAUTH imap-partial.test ready\r\n") - 1u, pServer->Deadline) &&
		testMailTlsReceive(pTls, "A00000001 CAPABILITY\r\n",
			sizeof("A00000001 CAPABILITY\r\n") - 1u, pServer->Deadline) &&
		testMailTlsSend(pTls, "* CAPABILITY IMAP4rev1\r\nA00000001 OK capability complete\r\n",
			sizeof("* CAPABILITY IMAP4rev1\r\nA00000001 OK capability complete\r\n") - 1u,
			pServer->Deadline) &&
		testMailTlsReceive(pTls, "A00000002 APPEND \"INBOX\" {8388608}\r\n",
			sizeof("A00000002 APPEND \"INBOX\" {8388608}\r\n") - 1u, pServer->Deadline) &&
		testMailTlsSend(pTls, "+ send literal\r\n",
			sizeof("+ send literal\r\n") - 1u, pServer->Deadline);
	if ( Success ) {
		xfuture* pFirst = xrtTlsStreamRecvAsync(pTls, 1u);
		Success = testMailTlsFuture(pFirst, pServer->Deadline);
		if ( Success ) {
			xbytesview Data = xrtNetBytesView((const xnetbytes*)xrtFutureValue(pFirst));
			Success = Data.Size == 1u && Data.Data[0] == 'P';
			pServer->Received = Data.Size;
		}
		xrtFutureDestroy(pFirst);
	}
	Success = Success && testImapTlsWaitPartial(pServer);
	if ( Success && pServer->Mode == TEST_IMAP_TLS_CANCEL )
		Success = xrtCancelRequest(pServer->Cancel);
	if ( Success && pServer->Mode == TEST_IMAP_TLS_DISCONNECT )
		Success = xrtTlsStreamAbort(pTls);
	Success = Success && testMailPartialWaitFlag(&pServer->Returned, pServer->Deadline);
	if ( Success && pServer->Mode != TEST_IMAP_TLS_DISCONNECT )
		Success = testImapTlsDrain(pServer, pTls);
	(void)xrtTlsStreamAbort(pTls);
	while ( xrtTlsStreamState(pTls) != XTLS_STREAM_FAILED &&
		xrtTlsStreamState(pTls) != XTLS_STREAM_CLOSED ) {
		if ( xrtDeadlineExpired(pServer->Deadline) ) { Success = false; break; }
		xrtThreadYield();
	}
	xrtTlsStreamDestroy(pTls);
	pServer->Success = Success;
	return Success ? 0 : 3;
}

int main(void)
{
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetresolverconfig ResolverConfig;
	ximapclientconfig Config;
	xtlsserverconfig TlsServer;
	xtlsverifierconfig VerifyConfig;
	xtlslimits Limits;
	xrtTlsLimitsInit(&Limits);
	Limits.SendLimit = 32u * 1024u;
	xtlscontext* pContext = testTlsServerContextWithLimits(&Limits);
	xtlsidentity* pIdentity = testTlsServerIdentity();
	testRequire(pContext != NULL && pIdentity != NULL, "IMAP TLS fault identity creation failed");
	xrtTlsVerifierConfigInit(&VerifyConfig);
	VerifyConfig.Verify = testTlsServerAccept;
	xtlsverifier* pVerifier = xrtTlsVerifierCreate(&VerifyConfig);
	testRequire(pVerifier != NULL, "IMAP TLS fault verifier creation failed");
	xrtTlsServerConfigInit(&TlsServer);
	TlsServer.Context = pContext;
	TlsServer.Identity = pIdentity;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2u;
	xnetengine* pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(pEngine != NULL && xrtNetEngineStart(pEngine), "IMAP TLS fault engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	ListenConfig.Stream.ReadLimit = 32u * 1024u;
	testRequire(xrtNetAddrLoopback(&ListenConfig.Address, XNET_FAMILY_IPV4, 0),
		"IMAP TLS fault address creation failed");
	xnetlistener* pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire(pListener != NULL && xrtNetListenerLocal(pListener, &TestImapTlsAddress),
		"IMAP TLS fault listener creation failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testImapTlsResolve;
	ResolverConfig.CacheEntries = 0;
	xnetresolver* pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "IMAP TLS fault resolver creation failed");
	xrtImapClientConfigInit(&Config);
	Config.Net.Engine = pEngine;
	Config.Net.Resolver = pResolver;
	Config.Net.Host = "imap-partial.test";
	Config.Net.Port = TestImapTlsAddress.Port;
	Config.Net.Security = XMAIL_SECURITY_TLS;
	Config.Net.Tls.Context = pContext;
	Config.Net.Tls.Verifier = pVerifier;
	Config.Net.WriteChunk = TEST_IMAP_TLS_WRITE;
	Config.Net.TlsStream.AsyncBytesLimit = TEST_IMAP_TLS_WRITE;
	Config.Net.Dial.Stream.WriteLimit = TEST_IMAP_TLS_WRITE;
	Config.Net.Dial.Stream.ReadLimit = 32u * 1024u;
	testRequire(xrtImapClientConfigValid(&Config), "IMAP TLS fault configuration invalid");
	unsigned char* pPayload = (unsigned char*)xrtMalloc(TEST_IMAP_TLS_TOTAL);
	testRequire(pPayload != NULL, "IMAP TLS fault payload allocation failed");
	memset(pPayload, 'P', TEST_IMAP_TLS_PREFIX);
	memset(pPayload + TEST_IMAP_TLS_PREFIX, 'Q', TEST_IMAP_TLS_WRITE);

	for ( int Mode = TEST_IMAP_TLS_CANCEL; Mode <= TEST_IMAP_TLS_DISCONNECT; Mode++ ) {
		testimaptlsserver Server;
		memset(&Server, 0, sizeof(Server));
		Server.Listener = pListener;
		Server.Tls = &TlsServer;
		Server.Mode = (testimaptlsfault)Mode;
		Server.Deadline = xrtDeadlineAfter(UINT64_C(15000000));
		Server.Cancel = xrtCancelCreate();
		xrtAtomic32Init(&Server.ClientReady, 0u);
		xrtAtomic32Init(&Server.PrefixReady, 0u);
		xrtAtomic32Init(&Server.Partial, 0u);
		xrtAtomic32Init(&Server.Returned, 0u);
		testRequire(Server.Cancel != NULL, "IMAP TLS fault cancel creation failed");
		xthread* pThread = xrtThreadCreate(testImapTlsServer, &Server, 0);
		testRequire(pThread != NULL, "IMAP TLS fault server thread creation failed");
		ximapclient* pClient = xrtImapClientOpen(&Config, Server.Deadline, NULL);
		testRequire(pClient != NULL && xrtImapClientState(pClient) == XIMAP_CLIENT_AUTHENTICATED,
			"IMAP TLS fault client open failed");
		Server.ClientTls = xrtTlsStreamRef(pClient->Transport.Tls);
		testRequire(Server.ClientTls != NULL, "IMAP TLS fault transport reference missing");
		testMailPartialSmallSocket(xrtTlsStreamTransport(Server.ClientTls), Server.Deadline);
		xrtAtomic32Store(&Server.ClientReady, 1u, XMEMORY_RELEASE);
		ximapappendconfig Append;
		xrtImapAppendConfigInit(&Append);
		Append.Mailbox = XRT_STR_LITERAL("INBOX");
		Append.Size = TEST_IMAP_TLS_TOTAL;
		Append.Literal = XIMAP_LITERAL_SYNC;
		testRequire(xrtImapClientAppendBegin(pClient, &Append, Server.Deadline, NULL),
			"IMAP TLS fault APPEND begin failed");
		testRequire(xrtImapClientAppendWrite(pClient, pPayload, TEST_IMAP_TLS_PREFIX,
			Server.Deadline, NULL), "IMAP TLS fault prefix write failed");
		testmailpartialsnapshot Prefix;
		testRequire(testMailPartialSnapshot(Server.ClientTls, Server.Deadline, &Prefix) &&
			Prefix.AsyncBytes == 0, "IMAP TLS prefix checkpoint failed");
		Server.PrefixAccepted = Prefix.Accepted;
		xrtAtomic32Store(&Server.PrefixReady, 1u, XMEMORY_RELEASE);
		xrtClearError();
		bool Written = xrtImapClientAppendWrite(pClient, pPayload + TEST_IMAP_TLS_PREFIX,
			TEST_IMAP_TLS_WRITE, Mode == TEST_IMAP_TLS_TIMEOUT ?
			xrtDeadlineAfter(UINT64_C(3000000)) : Server.Deadline,
			Mode == TEST_IMAP_TLS_CANCEL ? Server.Cancel : NULL);
		xerror* pFailure = xrtTakeError();
		ximapresponseview Last;
		bool FailedAfterProgress = !Written && pFailure != NULL &&
			xrtImapClientState(pClient) == XIMAP_CLIENT_FAILED &&
			xrtImapClientAppendRemaining(pClient) == TEST_IMAP_TLS_WRITE &&
			xrtAtomic32Load(&Server.Partial, XMEMORY_ACQUIRE);
		if ( !FailedAfterProgress ) fprintf(stderr,
			"[IMAP TLS fault] mode=%d written=%d kind=%d code=%d state=%d "
			"remaining=%zu partial=%u async=%zu pending=%zu\n", Mode, (int)Written,
			(int)xrtErrorKind(pFailure), (int)xrtErrorCode(pFailure),
			(int)xrtImapClientState(pClient), xrtImapClientAppendRemaining(pClient),
			(unsigned)xrtAtomic32Load(&Server.Partial, XMEMORY_ACQUIRE),
			xrtTlsStreamAsyncBytes(Server.ClientTls), xrtTlsStreamPending(Server.ClientTls));
		testRequire(FailedAfterProgress,
			"IMAP TLS partial write did not fail after observed progress");
		testRequire(xrtImapClientLastResponse(pClient, &Last) &&
			Last.Status == XIMAP_STATUS_OK &&
			testMailViewEqual(Last.Tag, XRT_STR_LITERAL("A00000001")) &&
			testMailViewEqual(Last.Text, XRT_STR_LITERAL("capability complete")),
			"IMAP TLS partial write lost the last complete response");
		if ( Mode == TEST_IMAP_TLS_CANCEL || Mode == TEST_IMAP_TLS_TIMEOUT ) {
			testRequire(xrtErrorKind(pFailure) == (Mode == TEST_IMAP_TLS_CANCEL ?
				XERR_CANCELLED : XERR_TIMEOUT) && xrtErrorCode(pFailure) == XMAIL_ERROR_PROTOCOL &&
				strcmp(xrtErrorDomain(pFailure), "xrt.mail") == 0,
				"IMAP TLS partial write lost its cancellation/timeout diagnostic");
		} else {
			const xerror* pTlsError = xrtTlsStreamError(Server.ClientTls);
			testRequire(pTlsError != NULL && xrtErrorKind(pFailure) == xrtErrorKind(pTlsError) &&
				xrtErrorCode(pFailure) == xrtErrorCode(pTlsError) &&
				strcmp(xrtErrorDomain(pFailure), xrtErrorDomain(pTlsError)) == 0,
				"IMAP TLS partial write lost the peer disconnect diagnostic");
		}
		xrtErrorFree(pFailure);
		testRequire(!xrtImapClientAppendWrite(pClient, "X", 1u, Server.Deadline, NULL) &&
			xrtErrorKind(xrtGetError()) == XERR_STATE,
			"IMAP TLS failed upload session accepted another write");
		xrtAtomic32Store(&Server.Returned, 1u, XMEMORY_RELEASE);
		testRequire(xrtThreadWaitUntil(pThread, Server.Deadline) == XWAIT_OK &&
			Server.Success && xrtThreadExitCode(pThread) == 0,
			"IMAP TLS partial write did not abort before client destruction");
		xrtThreadDestroy(pThread);
		xrtImapClientDestroy(pClient);
		xrtTlsStreamDestroy(Server.ClientTls);
		xrtCancelDestroy(Server.Cancel);
	}
	xrtFree(pPayload);
	testRequire(xrtNetListenerClose(pListener), "IMAP TLS fault listener close failed");
	xdeadline Retire = xrtDeadlineAfter(UINT64_C(3000000));
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		testRequire(!xrtDeadlineExpired(Retire), "IMAP TLS fault listener did not close");
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver), "IMAP TLS fault resolver destroy failed");
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	for ( ;; ) {
		xnetretireresult Result = xrtNetEngineTryDestroy(pEngine);
		if ( Result == XNET_RETIRE_READY ) break;
		testRequire(Result != XNET_RETIRE_ERROR && !xrtDeadlineExpired(Retire),
			"IMAP TLS fault engine did not retire");
		xrtThreadYield();
	}
	return 0;
}
