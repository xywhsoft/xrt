#include "../test.h"
#include "../../../../tests/fixtures/tls_server.h"



typedef struct testpop3stlsupgrade {
	xnetstream* Tcp;
	const xtlsserverconfig* Server;
	const xtlsstreamconfig* Stream;
	xtlsstream* Tls;
	xpromise* Promise;
} testpop3stlsupgrade;



typedef struct testpop3stlsserver {
	xnetlistener* Listener;
	const xtlsserverconfig* Tls;
	xdeadline Deadline;
	bool Success;
} testpop3stlsserver;



static xnetaddr TestPop3StlsAddress;



/* 把测试域名解析到本地 STLS 服务器。 */
static xnetaddrlist* testPop3StlsResolve(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "pop3.test") != 0 ) {
		return NULL;
	}
	Address = TestPop3StlsAddress;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}



/* 等待 TLS Future 成功完成。 */
static bool testPop3StlsFuture(
	xfuture* pFuture,
	xdeadline iDeadline
)
{
	return (pFuture != NULL) &&
		(xrtFutureWaitUntil(pFuture, iDeadline) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED);
}



/* 在 TCP Stream 所属 Worker 上附加服务端 TLS session。 */
static void testPop3StlsUpgradeTask(
	xnetworker* pWorker,
	ptr pData
)
{
	testpop3stlsupgrade* pUpgrade = (testpop3stlsupgrade*)pData;
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



/* 把公开的 POP3 TCP 连接原位升级为 TLS 服务端。 */
static xtlsstream* testPop3StlsUpgrade(
	xnetstream** ppTcp,
	const xtlsserverconfig* pServer,
	xdeadline iDeadline
)
{
	testpop3stlsupgrade Upgrade;
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
		testPop3StlsUpgradeTask,
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
	if ( !testPop3StlsFuture(pOpen, iDeadline) ) {
		xrtFutureDestroy(pOpen);
		(void)xrtTlsStreamAbort(Upgrade.Tls);
		xrtTlsStreamDestroy(Upgrade.Tls);
		return NULL;
	}
	xrtFutureDestroy(pOpen);
	return Upgrade.Tls;
}



/* 发送一段完整的明文 POP3 响应。 */
static bool testPop3StlsPlainSend(
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



/* 精确接收并比较一段明文 POP3 命令。 */
static bool testPop3StlsPlainReceive(
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



/* 发送一段完整的 TLS POP3 响应。 */
static bool testPop3StlsSend(
	xtlsstream* pStream,
	cstr sText,
	size_t iSize,
	xdeadline iDeadline
)
{
	xfuture* pFuture = xrtTlsStreamSendAsync(pStream, sText, iSize);
	bool bSuccess = testPop3StlsFuture(pFuture, iDeadline);

	xrtFutureDestroy(pFuture);
	return bSuccess;
}



/* 精确接收并比较一段 TLS POP3 命令。 */
static bool testPop3StlsReceive(
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

		if ( !testPop3StlsFuture(pFuture, iDeadline) ) {
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



/* 模拟 STLS、升级后 CAPA、USER/PASS 和认证关闭。 */
static int32 testPop3StlsServer(ptr pData)
{
	testpop3stlsserver* pServer = (testpop3stlsserver*)pData;
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
	bSuccess = testPop3StlsPlainSend(
		pTcp,
		"+OK pop3.test ready\r\n",
		sizeof("+OK pop3.test ready\r\n") - 1u,
		pServer->Deadline
	) && testPop3StlsPlainReceive(
		pTcp,
		"CAPA\r\n",
		sizeof("CAPA\r\n") - 1u,
		pServer->Deadline
	) && testPop3StlsPlainSend(
		pTcp,
		"+OK capabilities\r\nSTLS\r\n.\r\n",
		sizeof("+OK capabilities\r\nSTLS\r\n.\r\n") - 1u,
		pServer->Deadline
	) && testPop3StlsPlainReceive(
		pTcp,
		"STLS\r\n",
		sizeof("STLS\r\n") - 1u,
		pServer->Deadline
	) && testPop3StlsPlainSend(
		pTcp,
		"+OK begin TLS\r\n",
		sizeof("+OK begin TLS\r\n") - 1u,
		pServer->Deadline
	);
	if ( !bSuccess ) {
		xrtNetStreamDestroy(pTcp);
		return 2;
	}
	pTls = testPop3StlsUpgrade(&pTcp, pServer->Tls, pServer->Deadline);
	if ( pTls == NULL ) {
		xrtNetStreamDestroy(pTcp);
		return 3;
	}
	bSuccess = testPop3StlsReceive(
		pTls,
		"CAPA\r\n",
		sizeof("CAPA\r\n") - 1u,
		pServer->Deadline
	) && testPop3StlsSend(
		pTls,
		"+OK capabilities\r\nUSER\r\nUIDL\r\n.\r\n",
		sizeof("+OK capabilities\r\nUSER\r\nUIDL\r\n.\r\n") - 1u,
		pServer->Deadline
	) && testPop3StlsReceive(
		pTls,
		"USER user\r\n",
		sizeof("USER user\r\n") - 1u,
		pServer->Deadline
	) && testPop3StlsSend(
		pTls,
		"+OK user\r\n",
		sizeof("+OK user\r\n") - 1u,
		pServer->Deadline
	) && testPop3StlsReceive(
		pTls,
		"PASS pass\r\n",
		sizeof("PASS pass\r\n") - 1u,
		pServer->Deadline
	) && testPop3StlsSend(
		pTls,
		"+OK mailbox\r\n",
		sizeof("+OK mailbox\r\n") - 1u,
		pServer->Deadline
	) && testPop3StlsReceive(
		pTls,
		"QUIT\r\n",
		sizeof("QUIT\r\n") - 1u,
		pServer->Deadline
	) && testPop3StlsSend(
		pTls,
		"+OK signing off\r\n",
		sizeof("+OK signing off\r\n") - 1u,
		pServer->Deadline
	);
	if ( bSuccess ) {
		pClose = xrtTlsStreamWaitAsync(pTls, XTLS_STREAM_WAIT_CLOSE);
		bSuccess = (pClose != NULL) && xrtTlsStreamClose(pTls) &&
			testPop3StlsFuture(pClose, pServer->Deadline);
		xrtFutureDestroy(pClose);
	}
	pServer->Success = bSuccess;
	if ( !bSuccess ) {
		(void)xrtTlsStreamAbort(pTls);
	}
	xrtTlsStreamDestroy(pTls);
	return bSuccess ? 0 : 4;
}



/* 验证真实证书 STLS、能力替换和 TLS 上的 USER/PASS。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xtlsverifierconfig VerifierConfig;
	xtlsserverconfig ServerConfig;
	xpop3clientconfig Config;
	testpop3stlsserver Server;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	xtlsverifier* pVerifier;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	xpop3client* pClient;
	xthread* pThread;
	xdeadline Deadline;

	pContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	testRequire((pContext != NULL) && (pIdentity != NULL),
		"POP3 STLS fixture creation failed");
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(pVerifier != NULL, "POP3 STLS verifier creation failed");
	xrtTlsServerConfigInit(&ServerConfig);
	ServerConfig.Context = pContext;
	ServerConfig.Identity = pIdentity;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"POP3 STLS engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "POP3 STLS loopback address failed");
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire((pListener != NULL) && xrtNetListenerLocal(
		pListener,
		&TestPop3StlsAddress
	), "POP3 STLS listener start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testPop3StlsResolve;
	ResolverConfig.CacheEntries = 0;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "POP3 STLS resolver creation failed");

	Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	Server.Listener = pListener;
	Server.Tls = &ServerConfig;
	Server.Deadline = Deadline;
	Server.Success = false;
	pThread = xrtThreadCreate(testPop3StlsServer, &Server, 0);
	testRequire(pThread != NULL, "POP3 STLS server thread creation failed");
	xrtPop3ClientConfigInit(&Config);
	Config.Net.Engine = pEngine;
	Config.Net.Resolver = pResolver;
	Config.Net.Host = "pop3.test";
	Config.Net.Port = TestPop3StlsAddress.Port;
	Config.Net.Security = XMAIL_SECURITY_STARTTLS;
	Config.Net.Tls.Context = pContext;
	Config.Net.Tls.Verifier = pVerifier;
	pClient = xrtPop3ClientOpen(&Config, Deadline, NULL);
	testRequire((pClient != NULL) &&
		(xrtPop3ClientSecurity(pClient) == XMAIL_SECURITY_TLS) &&
		((xrtPop3ClientCapabilities(pClient) & XPOP3_CAP_STLS) == 0) &&
		((xrtPop3ClientCapabilities(pClient) &
			(XPOP3_CAP_USER | XPOP3_CAP_UIDL)) ==
			(XPOP3_CAP_USER | XPOP3_CAP_UIDL)),
		"POP3 STLS open or post-upgrade CAPA failed");
	testRequire(xrtPop3ClientLogin(
		pClient,
		XRT_STR_LITERAL("user"),
		XRT_STR_LITERAL("pass"),
		false,
		Deadline,
		NULL
	), "POP3 STLS USER/PASS failed");
	testRequire(xrtPop3ClientQuit(pClient, Deadline, NULL),
		"POP3 STLS QUIT failed");
	testRequire(xrtThreadWaitUntil(pThread, Deadline) == XWAIT_OK,
		"POP3 STLS server did not finish");
	testRequire(Server.Success && (xrtThreadExitCode(pThread) == 0),
		"POP3 STLS transcript mismatch");
	xrtThreadDestroy(pThread);
	xrtPop3ClientDestroy(pClient);

	testRequire(xrtNetListenerClose(pListener),
		"POP3 STLS listener close request failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"POP3 STLS resolver destroy failed");
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	testRequire(xrtNetEngineDestroy(pEngine),
		"POP3 STLS engine destroy failed");
	return 0;
}
