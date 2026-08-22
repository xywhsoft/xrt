#include "../test.h"
#include "../test_tls.h"



typedef struct testimapstarttlsserver {
	xnetlistener* Listener;
	const xtlsserverconfig* Tls;
	xdeadline Deadline;
	bool Success;
} testimapstarttlsserver;



static xnetaddr TestImapStartTlsAddress;



/* 把测试域名解析到本地 STARTTLS Listener。 */
static xnetaddrlist* testImapStartTlsResolve(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "imap.test") != 0 ) {
		return NULL;
	}
	Address = TestImapStartTlsAddress;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}



/* 模拟 STARTTLS 前后的 CAPABILITY 刷新和 TLS 上的 LOGOUT。 */
static int32 testImapStartTlsServer(ptr pData)
{
	testimapstarttlsserver* pServer = (testimapstarttlsserver*)pData;
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
	bSuccess = testMailTcpSend(
		pTcp,
		"* OK imap.test ready\r\n",
		sizeof("* OK imap.test ready\r\n") - 1u,
		pServer->Deadline
	) && testMailTcpReceive(
		pTcp,
		"A00000001 CAPABILITY\r\n",
		sizeof("A00000001 CAPABILITY\r\n") - 1u,
		pServer->Deadline
	) && testMailTcpSend(
		pTcp,
		"* CAPABILITY IMAP4rev1 STARTTLS\r\n"
		"A00000001 OK capability complete\r\n",
		sizeof(
			"* CAPABILITY IMAP4rev1 STARTTLS\r\n"
			"A00000001 OK capability complete\r\n"
		) - 1u,
		pServer->Deadline
	) && testMailTcpReceive(
		pTcp,
		"A00000002 STARTTLS\r\n",
		sizeof("A00000002 STARTTLS\r\n") - 1u,
		pServer->Deadline
	) && testMailTcpSend(
		pTcp,
		"A00000002 OK begin TLS\r\n",
		sizeof("A00000002 OK begin TLS\r\n") - 1u,
		pServer->Deadline
	);
	if ( !bSuccess ) {
		xrtNetStreamDestroy(pTcp);
		return 2;
	}
	pTls = testMailTlsUpgrade(&pTcp, pServer->Tls, pServer->Deadline);
	if ( pTls == NULL ) {
		xrtNetStreamDestroy(pTcp);
		return 3;
	}
	bSuccess = testMailTlsReceive(
		pTls,
		"A00000003 CAPABILITY\r\n",
		sizeof("A00000003 CAPABILITY\r\n") - 1u,
		pServer->Deadline
	) && testMailTlsSend(
		pTls,
		"* CAPABILITY IMAP4rev1 AUTH=PLAIN AUTH=OAUTHBEARER "
		"SASL-IR IDLE\r\n"
		"A00000003 OK capability complete\r\n",
		sizeof(
			"* CAPABILITY IMAP4rev1 AUTH=PLAIN AUTH=OAUTHBEARER "
			"SASL-IR IDLE\r\n"
			"A00000003 OK capability complete\r\n"
		) - 1u,
		pServer->Deadline
	) && testMailTlsReceive(
		pTls,
		"A00000004 AUTHENTICATE OAUTHBEARER "
		"bixhPXU9MkNzPTNEZSwBYXV0aD1CZWFyZXIgdG9rZW4BAQ==\r\n",
		sizeof(
			"A00000004 AUTHENTICATE OAUTHBEARER "
			"bixhPXU9MkNzPTNEZSwBYXV0aD1CZWFyZXIgdG9rZW4BAQ==\r\n"
		) - 1u,
		pServer->Deadline
	) && testMailTlsSend(
		pTls,
		"+ eyJzdGF0dXMiOiJpbnZhbGlkX3Rva2VuIn0=\r\n",
		sizeof("+ eyJzdGF0dXMiOiJpbnZhbGlkX3Rva2VuIn0=\r\n") - 1u,
		pServer->Deadline
	) && testMailTlsReceive(
		pTls,
		"AQ==\r\n",
		sizeof("AQ==\r\n") - 1u,
		pServer->Deadline
	) && testMailTlsSend(
		pTls,
		"A00000004 NO authentication failed\r\n",
		sizeof("A00000004 NO authentication failed\r\n") - 1u,
		pServer->Deadline
	) && testMailTlsReceive(
		pTls,
		"A00000005 LOGOUT\r\n",
		sizeof("A00000005 LOGOUT\r\n") - 1u,
		pServer->Deadline
	) && testMailTlsSend(
		pTls,
		"* BYE signing off\r\n"
		"A00000005 OK logout complete\r\n",
		sizeof(
			"* BYE signing off\r\n"
			"A00000005 OK logout complete\r\n"
		) - 1u,
		pServer->Deadline
	);
	if ( bSuccess ) {
		pClose = xrtTlsStreamWaitAsync(pTls, XTLS_STREAM_WAIT_CLOSE);
		bSuccess = (pClose != NULL) && xrtTlsStreamClose(pTls) &&
			testMailTlsFuture(pClose, pServer->Deadline);
		xrtFutureDestroy(pClose);
	}
	pServer->Success = bSuccess;
	if ( !bSuccess ) {
		(void)xrtTlsStreamAbort(pTls);
	}
	xrtTlsStreamDestroy(pTls);
	return bSuccess ? 0 : 4;
}



/* 验证真实证书 STARTTLS、能力快照替换和 TLS 关闭。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xtlsverifierconfig VerifierConfig;
	xtlsserverconfig ServerConfig;
	ximapclientconfig Config;
	ximapauthconfig Auth;
	testimapstarttlsserver Server;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	xtlsverifier* pVerifier;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	ximapclient* pClient;
	xthread* pThread;
	xdeadline Deadline;

	pContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	testRequire((pContext != NULL) && (pIdentity != NULL),
		"IMAP STARTTLS fixture creation failed");
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(pVerifier != NULL, "IMAP STARTTLS verifier creation failed");
	xrtTlsServerConfigInit(&ServerConfig);
	ServerConfig.Context = pContext;
	ServerConfig.Identity = pIdentity;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"IMAP STARTTLS engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "IMAP STARTTLS loopback address failed");
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire((pListener != NULL) && xrtNetListenerLocal(
		pListener,
		&TestImapStartTlsAddress
	), "IMAP STARTTLS listener start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testImapStartTlsResolve;
	ResolverConfig.CacheEntries = 0;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "IMAP STARTTLS resolver creation failed");

	Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	Server.Listener = pListener;
	Server.Tls = &ServerConfig;
	Server.Deadline = Deadline;
	Server.Success = false;
	pThread = xrtThreadCreate(testImapStartTlsServer, &Server, 0);
	testRequire(pThread != NULL, "IMAP STARTTLS server thread creation failed");
	xrtImapClientConfigInit(&Config);
	Config.Net.Engine = pEngine;
	Config.Net.Resolver = pResolver;
	Config.Net.Host = "imap.test";
	Config.Net.Port = TestImapStartTlsAddress.Port;
	Config.Net.Security = XMAIL_SECURITY_STARTTLS;
	Config.Net.Tls.Context = pContext;
	Config.Net.Tls.Verifier = pVerifier;
	pClient = xrtImapClientOpen(&Config, Deadline, NULL);
	testRequire((pClient != NULL) &&
		(xrtImapClientSecurity(pClient) == XMAIL_SECURITY_TLS) &&
		((xrtImapClientCapabilities(pClient) & XIMAP_CAP_STARTTLS) == 0) &&
		((xrtImapClientCapabilities(pClient) &
			(XIMAP_CAP_AUTH_PLAIN | XIMAP_CAP_AUTH_OAUTHBEARER |
			 XIMAP_CAP_SASL_IR | XIMAP_CAP_IDLE)) ==
			(XIMAP_CAP_AUTH_PLAIN | XIMAP_CAP_AUTH_OAUTHBEARER |
			 XIMAP_CAP_SASL_IR | XIMAP_CAP_IDLE)),
		"IMAP STARTTLS open or post-upgrade CAPABILITY failed");
	xrtImapAuthConfigInit(&Auth);
	Auth.Method = XIMAP_AUTH_OAUTHBEARER;
	Auth.Username = XRT_STR_LITERAL("user");
	Auth.AuthorizationId = XRT_STR_LITERAL("u,s=e");
	Auth.Secret = XRT_STR_LITERAL("token");
	testRequire(!xrtImapClientAuth(pClient, &Auth, Deadline, NULL) &&
		(xrtImapClientState(pClient) == XIMAP_CLIENT_NOT_AUTHENTICATED),
		"IMAP OAUTHBEARER rejection did not preserve reusable state");
	xrtClearError();
	testRequire(xrtImapClientLogout(pClient, Deadline, NULL),
		"IMAP STARTTLS LOGOUT failed");
	testRequire(xrtThreadWaitUntil(pThread, Deadline) == XWAIT_OK,
		"IMAP STARTTLS server did not finish");
	testRequire(Server.Success && (xrtThreadExitCode(pThread) == 0),
		"IMAP STARTTLS transcript mismatch");
	xrtThreadDestroy(pThread);
	xrtImapClientDestroy(pClient);

	testRequire(xrtNetListenerClose(pListener),
		"IMAP STARTTLS listener close request failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"IMAP STARTTLS resolver destroy failed");
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	testRequire(xrtNetEngineDestroy(pEngine),
		"IMAP STARTTLS engine destroy failed");
	return 0;
}
