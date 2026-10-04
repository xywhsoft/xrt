#include "../../src/internal/xrt_mail_net.h"
#include "../../../../tests/fixtures/tls_server.h"


static xnetaddr TestStartTlsAddress;

typedef struct teststarttlspark {
	xatomic32 Entered;
	xatomic32 Release;
	xatomic32 Exited;
	xcancel* Cancel;
} teststarttlspark;

static xnetaddrlist* testStartTlsResolve(cstr sHost, xnetfamily Family,
	ptr pData)
{
	xnetaddr Address = TestStartTlsAddress;
	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "mail.test") != 0 ) {
		return NULL;
	}
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}

static void testStartTlsPark(xnetworker* pWorker, ptr pData)
{
	teststarttlspark* pPark = (teststarttlspark*)pData;
	(void)pWorker;
	xrtAtomic32Store(&pPark->Entered, 1, XMEMORY_RELEASE);
	while ( !xrtAtomic32Load(&pPark->Release, XMEMORY_ACQUIRE) ) {
		xrtThreadYield();
	}
	xrtAtomic32Store(&pPark->Exited, 1, XMEMORY_RELEASE);
}

static void testStartTlsBarrier(xnetworker* pWorker, ptr pData)
{
	(void)pWorker;
	xrtAtomic32Store((xatomic32*)pData, 1, XMEMORY_RELEASE);
}

static void testStartTlsWait(const xatomic32* pFlag, cstr sMessage)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(3000000));
	while ( !xrtAtomic32Load(pFlag, XMEMORY_ACQUIRE) ) {
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}

static int32 testStartTlsRelease(ptr pData)
{
	teststarttlspark* pPark = (teststarttlspark*)pData;
	if ( pPark->Cancel != NULL ) {
		xrtSleep(20);
		if ( !xrtCancelRequest(pPark->Cancel) ) {
			return 1;
		}
		xrtSleep(380);
	} else {
		xrtSleep(400);
	}
	xrtAtomic32Store(&pPark->Release, 1, XMEMORY_RELEASE);
	return 0;
}

static void testStartTlsQueued(xnetengine* pEngine,
	xnetlistener* pListener, const xmailnetconfig* pConfig,
	bool bCancel)
{
	__xmailtransport Transport;
	xnetstream* pServer;
	xnetworker* pWorker;
	teststarttlspark Park;
	xatomic32 Barrier;
	xthread* pReleaser;
	xnetbytes* pBytes;
	xbytesview Bytes;
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(5000000));
	bool bUpgraded;

	testRequire(__xrtMailTransportOpen(&Transport, pConfig, Deadline, NULL),
		"STARTTLS queue TCP dial failed");
	pServer = xrtNetListenerAcceptWait(pListener, Deadline, NULL);
	testRequire(pServer != NULL, "STARTTLS queue accept failed");
	pWorker = xrtNetStreamWorker(Transport.Tcp);
	testRequire(pWorker != NULL, "STARTTLS queue Worker missing");
	xrtAtomic32Init(&Park.Entered, 0);
	xrtAtomic32Init(&Park.Release, 0);
	xrtAtomic32Init(&Park.Exited, 0);
	Park.Cancel = bCancel ? xrtCancelCreate() : NULL;
	testRequire(!bCancel || Park.Cancel != NULL,
		"STARTTLS queue cancel creation failed");
	testRequire(xrtNetEnginePost(pEngine, xrtNetWorkerIndex(pWorker),
		testStartTlsPark, &Park), "STARTTLS queue Worker post failed");
	testStartTlsWait(&Park.Entered, "STARTTLS queue Worker did not park");
	pReleaser = xrtThreadCreate(testStartTlsRelease, &Park, 0);
	testRequire(pReleaser != NULL,
		"STARTTLS queue release thread creation failed");
	xrtClearError();
	bUpgraded = __xrtMailTransportStartTls(&Transport, pConfig,
		bCancel ? Deadline : xrtDeadlineAfter(UINT64_C(80000)),
		Park.Cancel);
	testRequire(!bUpgraded && xrtGetError() != NULL &&
		xrtErrorKind(xrtGetError()) ==
			(bCancel ? XERR_CANCELLED : XERR_TIMEOUT) &&
		!xrtAtomic32Load(&Park.Exited, XMEMORY_ACQUIRE) &&
		Transport.Tcp != NULL && Transport.Tls == NULL,
		"STARTTLS queue did not stop before Worker takeover");
	testRequire(xrtThreadWaitFor(pReleaser, UINT64_C(3000000)) ==
		XWAIT_OK && xrtThreadExitCode(pReleaser) == 0,
		"STARTTLS queue release thread failed");
	xrtThreadDestroy(pReleaser);
	testStartTlsWait(&Park.Exited,
		"STARTTLS queue Worker did not resume");
	xrtAtomic32Init(&Barrier, 0);
	testRequire(xrtNetEnginePost(pEngine, xrtNetWorkerIndex(pWorker),
		testStartTlsBarrier, &Barrier),
		"STARTTLS queue barrier post failed");
	testStartTlsWait(&Barrier,
		"STARTTLS queue task did not drain");
	testRequire(Transport.Tcp != NULL && Transport.Tls == NULL &&
		__xrtMailTransportSend(&Transport, "N", 1, Deadline, NULL),
		"STARTTLS queue cancellation damaged plaintext TCP");
	pBytes = xrtNetStreamRecv(pServer, 1, Deadline, NULL);
	testRequire(pBytes != NULL,
		"STARTTLS queue server did not receive plaintext");
	Bytes = xrtNetBytesView(pBytes);
	testRequire(Bytes.Size == 1 && Bytes.Data[0] == 'N',
		"STARTTLS queue sent unexpected bytes");
	xrtNetBytesDestroy(pBytes);
	__xrtMailTransportDestroy(&Transport);
	(void)xrtNetStreamAbort(pServer);
	xrtNetStreamDestroy(pServer);
	xrtCancelDestroy(Park.Cancel);
	xrtClearError();
}

int main(void)
{
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetresolverconfig ResolverConfig;
	xmailnetconfig Config;
	xtlsverifierconfig VerifierConfig;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetresolver* pResolver;
	xtlscontext* pContext = testTlsServerContext();
	xtlsverifier* pVerifier;

	testRequire(pContext != NULL, "STARTTLS queue TLS context failed");
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(pVerifier != NULL,
		"STARTTLS queue TLS verifier failed");
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(pEngine != NULL && xrtNetEngineStart(pEngine),
		"STARTTLS queue engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(&ListenConfig.Address,
		XNET_FAMILY_IPV4, 0), "STARTTLS queue listen address failed");
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire(pListener != NULL &&
		xrtNetListenerLocal(pListener, &TestStartTlsAddress),
		"STARTTLS queue listener failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1;
	ResolverConfig.Lookup = testStartTlsResolve;
	ResolverConfig.CacheEntries = 0;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "STARTTLS queue resolver failed");
	xrtMailNetConfigInit(&Config);
	Config.Engine = pEngine;
	Config.Resolver = pResolver;
	Config.Host = "mail.test";
	Config.Port = TestStartTlsAddress.Port;
	Config.Security = XMAIL_SECURITY_STARTTLS;
	Config.Tls.Context = pContext;
	Config.Tls.Verifier = pVerifier;
	testRequire(xrtMailNetConfigValid(&Config),
		"STARTTLS queue config invalid");
	testStartTlsQueued(pEngine, pListener, &Config, false);
	testStartTlsQueued(pEngine, pListener, &Config, true);
	testRequire(xrtNetListenerClose(pListener),
		"STARTTLS queue listener close failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"STARTTLS queue resolver destroy failed");
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsContextRelease(pContext);
	testRequire(xrtNetEngineDestroy(pEngine),
		"STARTTLS queue engine destroy failed");
	return 0;
}
