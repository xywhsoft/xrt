#include "../../src/internal/xrt_mail_net.h"
#include "../../../../tests/fixtures/tls_server.h"



static xnetaddr TestMailNetTlsAddress;



/* 把测试域名解析到本地 TLS Listener。 */
static xnetaddrlist* testMailNetTlsResolve(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "mail.test") != 0 ) {
		return NULL;
	}
	Address = TestMailNetTlsAddress;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}



/* 等待测试 Future 成功完成。 */
static bool testMailNetTlsFuture(
	xfuture* pFuture,
	xdeadline iDeadline
)
{
	return (pFuture != NULL) &&
		(xrtFutureWaitUntil(pFuture, iDeadline) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED);
}



/* 验证真实证书握手、加密双向收发和认证关闭。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xtlslistenerconfig ListenerConfig;
	xtlsverifierconfig VerifierConfig;
	xmailnetconfig Config;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	xtlsverifier* pVerifier;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xtlslistener* pListener;
	xtlsstream* pServer;
	__xmailtransport Transport;
	xfuture* pFuture;
	xnetbytes* pBytes;
	xbytesview Bytes;
	xstrview Line;
	xdeadline Deadline;

	pContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	testRequire((pContext != NULL) && (pIdentity != NULL),
		"mail TLS runtime fixture creation failed");
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(pVerifier != NULL,
		"mail TLS runtime verifier creation failed");

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"mail TLS runtime engine start failed");
	xrtTlsListenerConfigInit(&ListenerConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenerConfig.Listen.Address,
		XNET_FAMILY_IPV4,
		0
	), "mail TLS runtime loopback address failed");
	ListenerConfig.Tls.Context = pContext;
	ListenerConfig.Tls.Identity = pIdentity;
	pListener = xrtTlsListenerStart(
		pEngine,
		&ListenerConfig,
		NULL,
		NULL,
		NULL
	);
	testRequire((pListener != NULL) && xrtTlsListenerLocal(
		pListener,
		&TestMailNetTlsAddress
	), "mail TLS runtime listener start failed");

	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testMailNetTlsResolve;
	ResolverConfig.CacheEntries = 0;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL,
		"mail TLS runtime resolver creation failed");
	xrtMailNetConfigInit(&Config);
	Config.Engine = pEngine;
	Config.Resolver = pResolver;
	Config.Host = "mail.test";
	Config.Port = TestMailNetTlsAddress.Port;
	Config.Security = XMAIL_SECURITY_TLS;
	Config.Tls.Context = pContext;
	Config.Tls.Verifier = pVerifier;
	Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	testRequire(__xrtMailTransportOpen(
		&Transport,
		&Config,
		Deadline,
		NULL
	), "mail TLS runtime transport open failed");
	pServer = xrtTlsListenerAcceptWait(pListener, Deadline, NULL);
	testRequire(pServer != NULL,
		"mail TLS runtime server accept failed");

	pFuture = xrtTlsStreamSendAsync(
		pServer,
		"220 mail.test ready\r\n",
		21u
	);
	testRequire(testMailNetTlsFuture(pFuture, Deadline),
		"mail TLS runtime server send failed");
	xrtFutureDestroy(pFuture);
	testRequire(__xrtMailTransportLine(
		&Transport,
		&Line,
		Deadline,
		NULL
	) && (Line.Size == 19u) &&
		(memcmp(Line.Data, "220 mail.test ready", 19u) == 0),
		"mail TLS runtime response mismatch");
	testRequire(__xrtMailTransportSend(
		&Transport,
		"EHLO client.test\r\n",
		18u,
		Deadline,
		NULL
	), "mail TLS runtime client send failed");
	pFuture = xrtTlsStreamRecvAsync(pServer, 18u);
	testRequire(testMailNetTlsFuture(pFuture, Deadline),
		"mail TLS runtime server receive failed");
	pBytes = xrtNetBytesRef((xnetbytes*)xrtFutureValue(pFuture));
	xrtFutureDestroy(pFuture);
	testRequire(pBytes != NULL,
		"mail TLS runtime receive ownership failed");
	Bytes = xrtNetBytesView(pBytes);
	testRequire((Bytes.Size == 18u) &&
		(memcmp(Bytes.Data, "EHLO client.test\r\n", 18u) == 0),
		"mail TLS runtime command mismatch");
	xrtNetBytesDestroy(pBytes);

	pFuture = xrtTlsStreamWaitAsync(pServer, XTLS_STREAM_WAIT_CLOSE);
	testRequire((pFuture != NULL) && xrtTlsStreamClose(pServer),
		"mail TLS runtime server close request failed");
	testRequire(__xrtMailTransportClose(&Transport, Deadline),
		"mail TLS runtime client close failed");
	testRequire(testMailNetTlsFuture(pFuture, Deadline),
		"mail TLS runtime server close failed");
	xrtFutureDestroy(pFuture);
	__xrtMailTransportDestroy(&Transport);
	xrtTlsStreamDestroy(pServer);

	testRequire(xrtTlsListenerClose(pListener),
		"mail TLS runtime listener close request failed");
	while ( xrtTlsListenerState(pListener) != XTLS_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtTlsListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"mail TLS runtime resolver destroy failed");
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	testRequire(xrtNetEngineDestroy(pEngine),
		"mail TLS runtime engine destroy failed");
	return 0;
}
