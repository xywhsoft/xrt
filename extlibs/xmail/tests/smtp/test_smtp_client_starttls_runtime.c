#include "../test.h"
#include "../../../../tests/fixtures/tls_server.h"



typedef struct testsmtpstarttlsupgrade {
	xnetstream* Tcp;
	const xtlsserverconfig* Server;
	const xtlsstreamconfig* Stream;
	xtlsstream* Tls;
	xpromise* Promise;
} testsmtpstarttlsupgrade;



typedef struct testsmtpstarttlsserver {
	xnetlistener* Listener;
	const xtlsserverconfig* Tls;
	xdeadline Deadline;
	bool Success;
} testsmtpstarttlsserver;



static xnetaddr TestSmtpStartTlsAddress;



/* 把测试域名解析到本地 STARTTLS Listener。 */
static xnetaddrlist* testSmtpStartTlsResolve(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "smtp.test") != 0 ) {
		return NULL;
	}
	Address = TestSmtpStartTlsAddress;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}



/* 等待测试 Future 成功完成。 */
static bool testSmtpStartTlsFuture(
	xfuture* pFuture,
	xdeadline iDeadline
)
{
	return (pFuture != NULL) &&
		(xrtFutureWaitUntil(pFuture, iDeadline) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED);
}



/* 在原 TCP Stream 所属 Worker 上接管为 TLS 服务端。 */
static void testSmtpStartTlsUpgradeTask(
	xnetworker* pWorker,
	ptr pData
)
{
	testsmtpstarttlsupgrade* pUpgrade =
		(testsmtpstarttlsupgrade*)pData;
	xtlssession* pSession;

	(void)pWorker;
	pSession = xrtTlsServerCreate(pUpgrade->Server, NULL);
	if ( (pSession != NULL) && xrtTlsStreamAttach(
		pUpgrade->Tcp,
		pSession,
		pUpgrade->Stream,
		NULL,
		NULL,
		&pUpgrade->Tls
	) ) {
		(void)xrtPromiseResolve(pUpgrade->Promise, NULL);
	} else if ( xrtGetError() != NULL ) {
		xrtTlsSessionDestroy(pSession);
		(void)xrtPromiseReject(pUpgrade->Promise, xrtGetError());
	} else {
		xrtTlsSessionDestroy(pSession);
		(void)xrtPromiseClose(pUpgrade->Promise);
	}
	xrtPromiseDestroy(pUpgrade->Promise);
}



/* 把已协商 STARTTLS 的服务端 TCP Stream 原位升级为 TLS。 */
static xtlsstream* testSmtpStartTlsUpgrade(
	xnetstream** ppTcp,
	const xtlsserverconfig* pServer,
	xdeadline iDeadline
)
{
	testsmtpstarttlsupgrade Upgrade;
	xtlsstreamconfig Stream;
	xnetstream* pTcp = ppTcp != NULL ? *ppTcp : NULL;
	xnetworker* pWorker = xrtNetStreamWorker(pTcp);
	xfuture* pFuture;
	xfuture* pOpen;

	if ( pWorker == NULL ) {
		return NULL;
	}
	xrtTlsStreamConfigInit(&Stream);
	memset(&Upgrade, 0, sizeof(Upgrade));
	Upgrade.Tcp = pTcp;
	Upgrade.Server = pServer;
	Upgrade.Stream = &Stream;
	Upgrade.Promise = xrtPromiseCreate(&pFuture, NULL);
	if ( Upgrade.Promise == NULL ) {
		return NULL;
	}
	if ( !xrtNetEnginePost(
		xrtNetWorkerEngine(pWorker),
		xrtNetWorkerIndex(pWorker),
		testSmtpStartTlsUpgradeTask,
		&Upgrade
	) ) {
		xrtPromiseDestroy(Upgrade.Promise);
		xrtFutureDestroy(pFuture);
		return NULL;
	}
	if ( (xrtFutureWait(pFuture) != XWAIT_OK) ||
		(xrtFutureState(pFuture) != XFUTURE_RESOLVED) ||
		(Upgrade.Tls == NULL) ) {
		xrtFutureDestroy(pFuture);
		return NULL;
	}
	xrtFutureDestroy(pFuture);
	*ppTcp = NULL;
	pOpen = xrtTlsStreamWaitAsync(Upgrade.Tls, XTLS_STREAM_WAIT_OPEN);
	if ( !testSmtpStartTlsFuture(pOpen, iDeadline) ) {
		xrtFutureDestroy(pOpen);
		(void)xrtTlsStreamAbort(Upgrade.Tls);
		xrtTlsStreamDestroy(Upgrade.Tls);
		return NULL;
	}
	xrtFutureDestroy(pOpen);
	return Upgrade.Tls;
}



/* 发送完整的明文 SMTP 响应。 */
static bool testSmtpStartTlsPlainSend(
	xnetstream* pStream,
	cstr sText,
	size_t iSize,
	xdeadline iDeadline
)
{
	for ( ;; ) {
		xnetresult Result = xrtNetStreamSend(pStream, sText, iSize);

		if ( Result == XNET_RESULT_OK ) {
			return true;
		}
		if ( (Result != XNET_RESULT_AGAIN) || !xrtNetStreamWait(
			pStream,
			XNET_STREAM_WAIT_WRITE,
			iDeadline,
			NULL
		) ) {
			return false;
		}
	}
}



/* 精确接收并比较一段明文 SMTP 字节。 */
static bool testSmtpStartTlsPlainReceive(
	xnetstream* pStream,
	cstr sExpected,
	size_t iExpected,
	xdeadline iDeadline
)
{
	size_t iReceived = 0;

	while ( iReceived < iExpected ) {
		xnetbytes* pBytes = xrtNetStreamRecv(
			pStream,
			iExpected - iReceived,
			iDeadline,
			NULL
		);
		xbytesview Bytes;

		if ( pBytes == NULL ) {
			return false;
		}
		Bytes = xrtNetBytesView(pBytes);
		if ( (Bytes.Size == 0) ||
			(memcmp(Bytes.Data, sExpected + iReceived, Bytes.Size) != 0) ) {
			xrtNetBytesDestroy(pBytes);
			return false;
		}
		iReceived += Bytes.Size;
		xrtNetBytesDestroy(pBytes);
	}
	return true;
}



/* 发送完整的 TLS SMTP 响应。 */
static bool testSmtpStartTlsSend(
	xtlsstream* pStream,
	cstr sText,
	size_t iSize,
	xdeadline iDeadline
)
{
	xfuture* pFuture = xrtTlsStreamSendAsync(pStream, sText, iSize);
	bool bSuccess = testSmtpStartTlsFuture(pFuture, iDeadline);

	xrtFutureDestroy(pFuture);
	return bSuccess;
}



/* 精确接收并比较一段 TLS SMTP 明文。 */
static bool testSmtpStartTlsReceive(
	xtlsstream* pStream,
	cstr sExpected,
	size_t iExpected,
	xdeadline iDeadline
)
{
	size_t iReceived = 0;

	while ( iReceived < iExpected ) {
		xfuture* pFuture = xrtTlsStreamRecvAsync(
			pStream,
			iExpected - iReceived
		);
		xnetbytes* pBytes;
		xbytesview Bytes;

		if ( !testSmtpStartTlsFuture(pFuture, iDeadline) ) {
			xrtFutureDestroy(pFuture);
			return false;
		}
		pBytes = xrtNetBytesRef((xnetbytes*)xrtFutureValue(pFuture));
		xrtFutureDestroy(pFuture);
		if ( pBytes == NULL ) {
			return false;
		}
		Bytes = xrtNetBytesView(pBytes);
		if ( (Bytes.Size == 0) ||
			(memcmp(Bytes.Data, sExpected + iReceived, Bytes.Size) != 0) ) {
			xrtNetBytesDestroy(pBytes);
			return false;
		}
		iReceived += Bytes.Size;
		xrtNetBytesDestroy(pBytes);
	}
	return true;
}



/* 模拟 STARTTLS 前后两次 EHLO 和认证关闭。 */
static int32 testSmtpStartTlsServer(ptr pData)
{
	testsmtpstarttlsserver* pServer = (testsmtpstarttlsserver*)pData;
	xnetstream* pTcp = xrtNetListenerAcceptWait(
		pServer->Listener,
		pServer->Deadline,
		NULL
	);
	xtlsstream* pTls;
	xfuture* pClose;
	bool bSuccess;

	if ( pTcp == NULL ) {
		return 1;
	}
	bSuccess = testSmtpStartTlsPlainSend(
		pTcp,
		"220 smtp.test ready\r\n",
		21u,
		pServer->Deadline
	) && testSmtpStartTlsPlainReceive(
		pTcp,
		"EHLO client.test\r\n",
		18u,
		pServer->Deadline
	) && testSmtpStartTlsPlainSend(
		pTcp,
		"250-smtp.test\r\n250 STARTTLS\r\n",
		29u,
		pServer->Deadline
	) && testSmtpStartTlsPlainReceive(
		pTcp,
		"STARTTLS\r\n",
		10u,
		pServer->Deadline
	) && testSmtpStartTlsPlainSend(
		pTcp,
		"220 begin TLS\r\n",
		15u,
		pServer->Deadline
	);
	if ( !bSuccess ) {
		xrtNetStreamDestroy(pTcp);
		return 2;
	}
	pTls = testSmtpStartTlsUpgrade(&pTcp, pServer->Tls, pServer->Deadline);
	if ( pTls == NULL ) {
		xrtNetStreamDestroy(pTcp);
		return 3;
	}
	pTcp = NULL;
	bSuccess = testSmtpStartTlsReceive(
		pTls,
		"EHLO client.test\r\n",
		18u,
		pServer->Deadline
	) && testSmtpStartTlsSend(
		pTls,
		"250-smtp.test\r\n250-SIZE 8192\r\n250 AUTH OAUTHBEARER\r\n",
		sizeof(
			"250-smtp.test\r\n250-SIZE 8192\r\n250 AUTH OAUTHBEARER\r\n"
		) - 1u,
		pServer->Deadline
	) && testSmtpStartTlsReceive(
		pTls,
		"AUTH OAUTHBEARER "
		"bixhPXU9MkNzPTNEZSwBYXV0aD1CZWFyZXIgdG9rZW4BAQ==\r\n",
		sizeof(
			"AUTH OAUTHBEARER "
			"bixhPXU9MkNzPTNEZSwBYXV0aD1CZWFyZXIgdG9rZW4BAQ==\r\n"
		) - 1u,
		pServer->Deadline
	) && testSmtpStartTlsSend(
		pTls,
		"334 eyJzdGF0dXMiOiI0MDEifQ==\r\n",
		sizeof("334 eyJzdGF0dXMiOiI0MDEifQ==\r\n") - 1u,
		pServer->Deadline
	) && testSmtpStartTlsReceive(
		pTls,
		"AQ==\r\n",
		sizeof("AQ==\r\n") - 1u,
		pServer->Deadline
	) && testSmtpStartTlsSend(
		pTls,
		"535 OAUTHBEARER rejected\r\n",
		sizeof("535 OAUTHBEARER rejected\r\n") - 1u,
		pServer->Deadline
	) && testSmtpStartTlsReceive(
		pTls,
		"QUIT\r\n",
		6u,
		pServer->Deadline
	) && testSmtpStartTlsSend(
		pTls,
		"221 closing\r\n",
		13u,
		pServer->Deadline
	);
	if ( bSuccess ) {
		pClose = xrtTlsStreamWaitAsync(pTls, XTLS_STREAM_WAIT_CLOSE);
		bSuccess = (pClose != NULL) && xrtTlsStreamClose(pTls) &&
			testSmtpStartTlsFuture(pClose, pServer->Deadline);
		xrtFutureDestroy(pClose);
	}
	pServer->Success = bSuccess;
	if ( !bSuccess ) {
		(void)xrtTlsStreamAbort(pTls);
	}
	xrtTlsStreamDestroy(pTls);
	return bSuccess ? 0 : 4;
}



/* 验证真实证书 STARTTLS 升级、二次 EHLO 和 TLS 关闭。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xtlsverifierconfig VerifierConfig;
	xtlsserverconfig ServerConfig;
	xsmtpclientconfig Config;
	xsmtpauthconfig Auth;
	testsmtpstarttlsserver Server;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	xtlsverifier* pVerifier;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	xsmtpclient* pClient;
	xthread* pThread;
	xdeadline Deadline;

	pContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	testRequire((pContext != NULL) && (pIdentity != NULL),
		"SMTP STARTTLS fixture creation failed");
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(pVerifier != NULL,
		"SMTP STARTTLS verifier creation failed");
	xrtTlsServerConfigInit(&ServerConfig);
	ServerConfig.Context = pContext;
	ServerConfig.Identity = pIdentity;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"SMTP STARTTLS engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "SMTP STARTTLS loopback address failed");
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire((pListener != NULL) && xrtNetListenerLocal(
		pListener,
		&TestSmtpStartTlsAddress
	), "SMTP STARTTLS listener start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testSmtpStartTlsResolve;
	ResolverConfig.CacheEntries = 0;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL,
		"SMTP STARTTLS resolver creation failed");

	Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	Server.Listener = pListener;
	Server.Tls = &ServerConfig;
	Server.Deadline = Deadline;
	Server.Success = false;
	pThread = xrtThreadCreate(testSmtpStartTlsServer, &Server, 0);
	testRequire(pThread != NULL,
		"SMTP STARTTLS server thread creation failed");
	xrtSmtpClientConfigInit(&Config);
	Config.Net.Engine = pEngine;
	Config.Net.Resolver = pResolver;
	Config.Net.Host = "smtp.test";
	Config.Net.Port = TestSmtpStartTlsAddress.Port;
	Config.Net.Security = XMAIL_SECURITY_STARTTLS;
	Config.Net.Tls.Context = pContext;
	Config.Net.Tls.Verifier = pVerifier;
	Config.Hello = (xstrview)XRT_STR_LITERAL("client.test");
	pClient = xrtSmtpClientOpen(&Config, Deadline, NULL);
	testRequire((pClient != NULL) &&
		((xrtSmtpClientCapabilities(pClient) & XSMTP_CAP_SIZE) != 0) &&
		((xrtSmtpClientCapabilities(pClient) &
		  XSMTP_CAP_AUTH_OAUTHBEARER) != 0) &&
		((xrtSmtpClientCapabilities(pClient) & XSMTP_CAP_STARTTLS) == 0) &&
		(xrtSmtpClientSizeLimit(pClient) == UINT64_C(8192)),
		"SMTP STARTTLS open or post-upgrade EHLO failed");
	xrtSmtpAuthConfigInit(&Auth);
	Auth.Method = XSMTP_AUTH_OAUTHBEARER;
	Auth.Username = XRT_STR_LITERAL("user");
	Auth.AuthorizationId = XRT_STR_LITERAL("u,s=e");
	Auth.Secret = XRT_STR_LITERAL("token");
	testRequire(!xrtSmtpClientAuth(pClient, &Auth, Deadline, NULL) &&
		!xrtSmtpClientAuthenticated(pClient),
		"SMTP OAUTHBEARER rejection did not preserve reusable state");
	xrtClearError();
	testRequire(xrtSmtpClientQuit(pClient, Deadline, NULL),
		"SMTP STARTTLS QUIT failed");
	testRequire(xrtThreadWaitUntil(pThread, Deadline) == XWAIT_OK,
		"SMTP STARTTLS server did not finish");
	testRequire(Server.Success && (xrtThreadExitCode(pThread) == 0),
		"SMTP STARTTLS transcript mismatch");
	xrtThreadDestroy(pThread);
	xrtSmtpClientDestroy(pClient);

	testRequire(xrtNetListenerClose(pListener),
		"SMTP STARTTLS listener close request failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"SMTP STARTTLS resolver destroy failed");
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	testRequire(xrtNetEngineDestroy(pEngine),
		"SMTP STARTTLS engine destroy failed");
	return 0;
}
