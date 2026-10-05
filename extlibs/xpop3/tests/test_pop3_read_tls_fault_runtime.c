#include <xrt/detail/wait.h>
#include "test.h"
#include "test_tls.h"
#include "../../../tests/fixtures/mail_tls_partial.h"
#include "../src/internal/xrt_pop3_client.h"

typedef enum testpop3tlsfault {
	TEST_POP3_TLS_CANCEL,
	TEST_POP3_TLS_TIMEOUT,
	TEST_POP3_TLS_DISCONNECT
} testpop3tlsfault;

typedef enum testpop3tlsstage {
	TEST_POP3_TLS_STATUS,
	TEST_POP3_TLS_NEXT,
	TEST_POP3_TLS_WRITE,
	TEST_POP3_TLS_BYTES
} testpop3tlsstage;

typedef struct testpop3tlsserver {
	xnetlistener* Listener;
	const xtlsserverconfig* Tls;
	double Deadline;
	testpop3tlsfault Fault;
	testpop3tlsstage Stage;
	xcancel* Cancel;
	xtlsstream* ClientTls;
	xatomic32 ClientReady;
	xatomic32 Partial;
	xatomic32 Returned;
	bool Success;
} testpop3tlsserver;

typedef struct testpop3tlssink {
	char Data[32];
	size_t Size;
	size_t Calls;
} testpop3tlssink;

static xnetaddr TestPop3TlsAddress;
static const char TestPop3TlsStatusPartial[] = "+OK 2 ";
static const char TestPop3TlsBodyPartial[] = "..partial";
static const char TestPop3TlsBodyComplete[] = ".first\r\n";

static xnetaddrlist* testPop3TlsResolve(cstr sHost, xnetfamily Family, ptr pData)
{
	xnetaddr Address = TestPop3TlsAddress;
	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "pop3-fault.test") != 0 ) return NULL;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}

static bool testPop3TlsSend(xtlsstream* pTls, cstr Text, double Deadline)
{
	return testMailTlsSend(pTls, Text, strlen(Text), Deadline);
}

static bool testPop3TlsReceive(xtlsstream* pTls, cstr Text, double Deadline)
{
	return testMailTlsReceive(pTls, Text, strlen(Text), Deadline);
}

/* 服务器必须在 Client Destroy 前收到异常关闭，且不能收到后续命令。 */
static bool testPop3TlsPeerClosed(xtlsstream* pTls, double Deadline)
{
	xfuture* pRead = xrtTlsStreamRecvAsync(pTls, 1u);
	bool Closed;
	if ( pRead == NULL ) return xrtTlsStreamState(pTls) == XTLS_STREAM_FAILED;
	Closed = __xrtFutureWaitUntil(pRead, Deadline) == XWAIT_OK &&
		xrtFutureState(pRead) == XFUTURE_FAILED && xrtTlsStreamState(pTls) == XTLS_STREAM_FAILED;
	xrtFutureDestroy(pRead);
	return Closed;
}

/* 通过实际密文收发计数和待定接收验证窗口，不以固定睡眠推测前缀已到达。 */
static bool testPop3TlsPrefix(testpop3tlsserver* pServer, xtlsstream* pTls)
{
	testmailpartialsnapshot ClientBefore;
	testmailpartialsnapshot ServerBefore;
	testmailpartialsnapshot ServerAfter;
	cstr Response = pServer->Stage == TEST_POP3_TLS_STATUS ? TestPop3TlsStatusPartial :
		"+OK message follows\r\n..first\r\n..partial";
	if ( !testMailPartialWaitFlag(&pServer->ClientReady, pServer->Deadline) ||
		!testMailPartialSnapshot(pServer->ClientTls, pServer->Deadline, &ClientBefore) ||
		!testMailPartialSnapshot(pTls, pServer->Deadline, &ServerBefore) ||
		!testPop3TlsSend(pTls, Response, pServer->Deadline) ||
		!testMailPartialSnapshot(pTls, pServer->Deadline, &ServerAfter) ||
		ServerAfter.Accepted <= ServerBefore.Accepted ) return false;
	if ( !testMailPartialWaitRead(pServer->ClientTls,
		ClientBefore.Received + ServerAfter.Accepted - ServerBefore.Accepted,
		&pServer->Returned, pServer->Deadline) ) return false;
	xrtAtomic32Store(&pServer->Partial, 1u, XMEMORY_RELEASE);
	return true;
}

static int32 testPop3TlsServer(ptr pData)
{
	testpop3tlsserver* pServer = (testpop3tlsserver*)pData;
	xnetstream* pTcp = __xrtNetListenerAcceptWait(pServer->Listener, pServer->Deadline, NULL);
	xtlsstream* pTls;
	bool Success;
	if ( pTcp == NULL ) return 1;
	pTls = testMailTlsUpgrade(&pTcp, pServer->Tls, pServer->Deadline);
	if ( pTls == NULL ) {
		xrtNetStreamDestroy(pTcp);
		return 2;
	}
	Success = testPop3TlsSend(pTls, "+OK ready\r\n", pServer->Deadline) &&
		testPop3TlsReceive(pTls, "USER user\r\n", pServer->Deadline) &&
		testPop3TlsSend(pTls, "+OK user\r\n", pServer->Deadline) &&
		testPop3TlsReceive(pTls, "PASS pass\r\n", pServer->Deadline) &&
		testPop3TlsSend(pTls, "+OK locked\r\n", pServer->Deadline) &&
		testPop3TlsReceive(pTls, pServer->Stage == TEST_POP3_TLS_STATUS ?
			"STAT\r\n" : "RETR 1\r\n", pServer->Deadline) &&
		testPop3TlsPrefix(pServer, pTls);
	if ( Success && pServer->Fault == TEST_POP3_TLS_CANCEL )
		Success = xrtCancelRequest(pServer->Cancel);
	if ( Success && pServer->Fault == TEST_POP3_TLS_DISCONNECT )
		Success = xrtTlsStreamAbort(pTls);
	Success = Success && testMailPartialWaitFlag(&pServer->Returned, pServer->Deadline);
	if ( Success && pServer->Fault != TEST_POP3_TLS_DISCONNECT )
		Success = testPop3TlsPeerClosed(pTls, pServer->Deadline);
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

static bool testPop3TlsWrite(xbytesview Data, ptr pData)
{
	testpop3tlssink* pSink = (testpop3tlssink*)pData;
	if ( Data.Size > sizeof(pSink->Data) - pSink->Size ) return false;
	memcpy(pSink->Data + pSink->Size, Data.Data, Data.Size);
	pSink->Size += Data.Size;
	pSink->Calls++;
	return true;
}

int main(void)
{
	xtlscontext* pContext = testTlsServerContext();
	xtlsidentity* pIdentity = testTlsServerIdentity();
	xtlsserverconfig TlsServer;
	xtlsverifierconfig VerifyConfig;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetresolverconfig ResolverConfig;
	xpop3clientconfig Config;
	testRequire(pContext != NULL && pIdentity != NULL, "POP3 TLS fault identity creation failed");
	xrtTlsVerifierConfigInit(&VerifyConfig);
	VerifyConfig.Verify = testTlsServerAccept;
	xtlsverifier* pVerifier = xrtTlsVerifierCreate(&VerifyConfig);
	testRequire(pVerifier != NULL, "POP3 TLS fault verifier creation failed");
	xrtTlsServerConfigInit(&TlsServer);
	TlsServer.Context = pContext;
	TlsServer.Identity = pIdentity;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2u;
	xnetengine* pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(pEngine != NULL && xrtNetEngineStart(pEngine), "POP3 TLS fault engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(&ListenConfig.Address, XNET_FAMILY_IPV4, 0),
		"POP3 TLS fault address creation failed");
	xnetlistener* pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire(pListener != NULL && xrtNetListenerLocal(pListener, &TestPop3TlsAddress),
		"POP3 TLS fault listener creation failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testPop3TlsResolve;
	ResolverConfig.CacheEntries = 0;
	xnetresolver* pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "POP3 TLS fault resolver creation failed");
	xrtPop3ClientConfigInit(&Config);
	Config.Net.Engine = pEngine;
	Config.Net.Resolver = pResolver;
	Config.Net.Host = "pop3-fault.test";
	Config.Net.Port = TestPop3TlsAddress.Port;
	Config.Net.Security = XMAIL_SECURITY_TLS;
	Config.Net.Tls.Context = pContext;
	Config.Net.Tls.Verifier = pVerifier;
	Config.ReadCapabilities = false;
	testRequire(xrtPop3ClientConfigValid(&Config), "POP3 TLS fault configuration invalid");
	for ( int Stage = TEST_POP3_TLS_STATUS; Stage <= TEST_POP3_TLS_BYTES; Stage++ ) {
		for ( int Fault = TEST_POP3_TLS_CANCEL; Fault <= TEST_POP3_TLS_DISCONNECT; Fault++ ) {
			testpop3tlsserver Server;
			testpop3tlssink Sink = { {0}, 0, 0 };
			xpop3stat Stat = {0};
			xstrview Line = {0};
			xpop3reply Last;
			size_t OutputSize = 0;
			memset(&Server, 0, sizeof(Server));
			Server.Listener = pListener;
			Server.Tls = &TlsServer;
			Server.Stage = (testpop3tlsstage)Stage;
			Server.Fault = (testpop3tlsfault)Fault;
			Server.Deadline = __xrtWaitAfter(UINT64_C(15000000));
			Server.Cancel = xrtCancelCreate();
			xrtAtomic32Init(&Server.ClientReady, 0u);
			xrtAtomic32Init(&Server.Partial, 0u);
			xrtAtomic32Init(&Server.Returned, 0u);
			testRequire(Server.Cancel != NULL, "POP3 TLS fault cancel creation failed");
			xthread* pThread = xrtThreadCreate(testPop3TlsServer, &Server, 0);
			testRequire(pThread != NULL, "POP3 TLS fault server thread creation failed");
			xpop3client* pClient = __xrtPop3ClientOpen(&Config, Server.Deadline, NULL);
			testRequire(pClient != NULL && xrtPop3ClientSecurity(pClient) == XMAIL_SECURITY_TLS,
				"POP3 TLS fault client open failed");
			Server.ClientTls = xrtTlsStreamRef(pClient->Transport.Tls);
			testRequire(Server.ClientTls != NULL, "POP3 TLS fault transport reference missing");
			xrtAtomic32Store(&Server.ClientReady, 1u, XMEMORY_RELEASE);
			testRequire(__xrtPop3ClientLogin(pClient, XRT_STR_LITERAL("user"),
				XRT_STR_LITERAL("pass"), false, Server.Deadline, NULL), "POP3 TLS fault login failed");
			double Deadline = Fault == TEST_POP3_TLS_TIMEOUT ?
				__xrtWaitAfter(UINT64_C(2000000)) : Server.Deadline;
			xcancel* pCancel = Fault == TEST_POP3_TLS_CANCEL ? Server.Cancel : NULL;
			bool Succeeded;
			xrtClearError();
			if ( Stage == TEST_POP3_TLS_STATUS ) {
				Succeeded = __xrtPop3ClientStat(pClient, &Stat, Deadline, pCancel);
			} else if ( Stage == TEST_POP3_TLS_NEXT ) {
				testRequire(__xrtPop3ClientRetr(pClient, 1u, Deadline, pCancel) &&
					__xrtPop3ClientNext(pClient, &Line, Deadline, pCancel) == XMAIL_NEXT_ITEM &&
					testMailViewEqual(Line, XRT_STR_LITERAL(".first")),
					"POP3 TLS fault lost a complete dot-transparent line");
				Line = (xstrview){0};
				Succeeded = __xrtPop3ClientNext(pClient, &Line, Deadline, pCancel) != XMAIL_NEXT_ERROR;
			} else if ( Stage == TEST_POP3_TLS_WRITE ) {
				Succeeded = __xrtPop3ClientRetrWrite(pClient, 1u, 1024u, testPop3TlsWrite, &Sink,
					&OutputSize, Deadline, pCancel);
			} else {
				bytes pData = __xrtPop3ClientRetrBytes(pClient, 1u, 1024u, &OutputSize, Deadline, pCancel);
				Succeeded = pData != NULL;
				xrtFree(pData);
			}
			xerror* pFailure = xrtTakeError();
			cstr Partial = Stage == TEST_POP3_TLS_STATUS ? TestPop3TlsStatusPartial : TestPop3TlsBodyPartial;
			bool FailedAfterPrefix = !Succeeded && pFailure != NULL &&
				xrtPop3ClientState(pClient) == XPOP3_CLIENT_FAILED &&
				xrtAtomic32Load(&Server.Partial, XMEMORY_ACQUIRE) &&
				pClient->Transport.PendingSize == strlen(Partial) &&
				memcmp(pClient->Transport.Pending, Partial, strlen(Partial)) == 0;
			if ( !FailedAfterPrefix ) fprintf(stderr,
				"[POP3 TLS fault] stage=%d fault=%d succeeded=%d kind=%d code=%d state=%d pending=%zu partial=%u\n",
				Stage, Fault, (int)Succeeded, (int)xrtErrorKind(pFailure), (int)xrtErrorCode(pFailure),
				(int)xrtPop3ClientState(pClient), pClient->Transport.PendingSize,
				(unsigned)xrtAtomic32Load(&Server.Partial, XMEMORY_ACQUIRE));
			testRequire(FailedAfterPrefix, "POP3 TLS read did not fail after consuming the partial prefix");
			testRequire(xrtPop3ClientLastReply(pClient, &Last) && Last.Ok &&
				testMailViewEqual(Last.Text, Stage == TEST_POP3_TLS_STATUS ?
					(xstrview)XRT_STR_LITERAL("locked") : (xstrview)XRT_STR_LITERAL("message follows")),
				"POP3 TLS failure replaced the last complete reply");
			if ( Fault == TEST_POP3_TLS_DISCONNECT ) {
				const xerror* pTlsError = xrtTlsStreamError(Server.ClientTls);
				testRequire(pTlsError != NULL && xrtErrorKind(pFailure) == xrtErrorKind(pTlsError) &&
					xrtErrorCode(pFailure) == xrtErrorCode(pTlsError) &&
					strcmp(xrtErrorDomain(pFailure), xrtErrorDomain(pTlsError)) == 0,
					"POP3 TLS failure replaced the disconnect error");
			} else {
				testRequire(xrtErrorKind(pFailure) == (Fault == TEST_POP3_TLS_CANCEL ?
					XERR_CANCELLED : XERR_TIMEOUT) && xrtErrorCode(pFailure) == XMAIL_ERROR_PROTOCOL &&
					strcmp(xrtErrorDomain(pFailure), "xrt.mail") == 0,
					"POP3 TLS failure replaced the cancellation/timeout error");
			}
			xrtErrorFree(pFailure);
			testRequire(Stat.Messages == 0 && Stat.Bytes == 0 && Line.Data == NULL &&
				Line.Size == 0 && OutputSize == 0, "POP3 TLS failure published an incomplete result");
			if ( Stage == TEST_POP3_TLS_WRITE )
				testRequire(Sink.Calls == 2u && Sink.Size == sizeof(TestPop3TlsBodyComplete) - 1u &&
					memcmp(Sink.Data, TestPop3TlsBodyComplete, Sink.Size) == 0,
					"POP3 TLS streaming sink received partial data or lost a complete line");
			testRequire(!__xrtPop3ClientNoop(pClient, Server.Deadline, NULL) &&
				xrtErrorKind(xrtGetError()) == XERR_STATE, "POP3 TLS failed session allowed another command");
			xrtAtomic32Store(&Server.Returned, 1u, XMEMORY_RELEASE);
			testRequire(__xrtThreadWaitUntil(pThread, Server.Deadline) == XWAIT_OK && Server.Success &&
				xrtThreadExitCode(pThread) == 0, "POP3 TLS failure did not close before client destruction");
			xrtThreadDestroy(pThread);
			xrtPop3ClientDestroy(pClient);
			xrtTlsStreamDestroy(Server.ClientTls);
			xrtCancelDestroy(Server.Cancel);
		}
	}
	testRequire(xrtNetListenerClose(pListener), "POP3 TLS fault listener close failed");
	double Retire = __xrtWaitAfter(UINT64_C(3000000));
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		testRequire(!__xrtWaitExpired(Retire), "POP3 TLS fault listener did not close");
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver), "POP3 TLS fault resolver destroy failed");
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	for ( ;; ) {
		xnetretireresult Result = xrtNetEngineTryDestroy(pEngine);
		if ( Result == XNET_RETIRE_READY ) break;
		testRequire(Result != XNET_RETIRE_ERROR && !__xrtWaitExpired(Retire),
			"POP3 TLS fault engine did not retire");
		xrtThreadYield();
	}
	return 0;
}
