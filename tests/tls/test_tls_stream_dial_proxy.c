#include "../fixtures/tls_server.h"



#ifndef TEST_TLS_PROXY_TIMEOUT
#define TEST_TLS_PROXY_TIMEOUT 0
#endif

#ifndef TEST_TLS_PROXY_BACKEND
#define TEST_TLS_PROXY_BACKEND XNET_PORT_SELECT
#endif



typedef struct test_tls_proxy_dial {
	xatomicptr Server;
	xatomic32 ConnectSeen;
	xatomic32 TlsSeen;
	xatomic32 Done;
	xnetresult Result;
	xerrkind ErrorKind;
	xtlsdialstate State;
} test_tls_proxy_dial;



static void testTlsProxyWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(5000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}



/* Done is published from inside the Worker callback.  A different thread may
 * observe it before that callback has unwound and released its runtime holds,
 * so teardown waits on the Engine's authoritative live-object count. */
static void testTlsProxyWaitCleanup(xnetengine* pEngine)
{
	xdeadline Deadline = xrtDeadlineAfter(5000000u);
	xnetenginestats Stats;

	for ( ;; ) {
		testRequire(xrtNetEngineStats(pEngine, &Stats),
			"TLS proxy engine cleanup statistics failed");
		if ( Stats.LiveObjects == 0 ) {
			return;
		}
		testRequire(!xrtDeadlineExpired(Deadline),
			"TLS proxy runtime owners did not drain");
		xrtThreadYield();
	}
}



static bool testTlsProxyAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	test_tls_proxy_dial* pTest = (test_tls_proxy_dial*)pData;

	(void)pListener;
	testRequire(xrtNetStreamSetData(pStream, pTest),
		"TLS proxy accepted stream data failed");
	xrtAtomicPtrStore(&pTest->Server, pStream, XMEMORY_RELEASE);
	return true;
}



/* A minimal local HTTP CONNECT peer opens the tunnel, observes ClientHello,
 * then intentionally withholds ServerHello so cancellation/timeout occurs in
 * the TLS stage after the proxy dial has already reached CONNECTED. */
static void testTlsProxyRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	test_tls_proxy_dial* pTest = (test_tls_proxy_dial*)pData;
	unsigned char iFirst = 0;

	if ( xrtNetBufPeek(pBuffer, 0, &iFirst, 1u) == 1u ) {
		if ( !xrtAtomic32Load(&pTest->ConnectSeen, XMEMORY_ACQUIRE) ) {
			static const char sResponse[] =
				"HTTP/1.1 200 Connection Established\r\n\r\n";

			testRequire(iFirst == (unsigned char)'C',
				"TLS proxy did not send CONNECT");
			xrtAtomic32Store(&pTest->ConnectSeen, 1u, XMEMORY_RELEASE);
			testRequire(xrtNetStreamSend(
				pStream,
				sResponse,
				sizeof(sResponse) - 1u
			) == XNET_RESULT_OK, "TLS proxy CONNECT response failed");
		} else if ( iFirst == 22u ) {
			xrtAtomic32Store(&pTest->TlsSeen, 1u, XMEMORY_RELEASE);
		}
	}
	(void)xrtNetBufConsume(pBuffer, xrtNetBufSize(pBuffer));
}



static void testTlsProxyDone(
	xtlsdial* pDial,
	xnetresult Result,
	xtlsstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	test_tls_proxy_dial* pTest = (test_tls_proxy_dial*)pData;

	testRequire(pStream == NULL,
		"stalled TLS proxy dial unexpectedly returned a stream");
	pTest->Result = Result;
	pTest->ErrorKind = xrtErrorKind(pError);
	pTest->State = xrtTlsDialState(pDial);
	xrtAtomic32Store(&pTest->Done, 1u, XMEMORY_RELEASE);
}



int main(void)
{
	test_tls_proxy_dial Test;
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents ServerEvents;
	xnetproxyconfig ProxyConfig;
	xtlsverifierconfig VerifierConfig;
	xtlsclientconfig ClientConfig;
	xtlsdialconfig DialConfig;
	xnetdialstats Stats;
	xnetaddr Address;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	xnetproxy* pProxy;
	xtlscontext* pTlsContext;
	xtlsverifier* pVerifier;
	xtlsdial* pDial;
	xnetstream* pServer;

	memset(&Test, 0, sizeof(Test));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&ServerEvents, 0, sizeof(ServerEvents));
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TLS_PROXY_BACKEND;
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TLS proxy engine start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "TLS proxy resolver creation failed");

	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TLS proxy loopback address failed");
	ListenerEvents.Accept = testTlsProxyAccept;
	ServerEvents.Read = testTlsProxyRead;
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		&ServerEvents,
		&Test
	);
	testRequire((pListener != NULL) &&
		xrtNetListenerLocal(pListener, &Address),
		"TLS proxy listener creation failed");

	xrtNetProxyConfigInit(&ProxyConfig);
	ProxyConfig.Type = XNET_PROXY_HTTP_CONNECT;
	ProxyConfig.Host = XRT_STR_LITERAL("127.0.0.1");
	ProxyConfig.Port = Address.Port;
	pProxy = xrtNetProxyCreate(&ProxyConfig);
	testRequire(pProxy != NULL, "TLS proxy object creation failed");
	pTlsContext = testTlsServerContext();
	testRequire(pTlsContext != NULL, "TLS proxy context creation failed");
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(pVerifier != NULL, "TLS proxy verifier creation failed");
	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.Context = pTlsContext;
	ClientConfig.Verifier = pVerifier;
	xrtTlsDialConfigInit(&DialConfig);
	DialConfig.Stream.HandshakeTimeout = 0;
	DialConfig.Timeout = TEST_TLS_PROXY_TIMEOUT ? 1000000u : 0;

	xrtClearError();
	testRequire((xrtTlsDialProxy(
		pEngine, pResolver, NULL, "127.0.0.1", Address.Port,
		&ClientConfig, &DialConfig, NULL, NULL, testTlsProxyDone, &Test
	) == NULL) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS proxy null argument was not rejected");
	xrtClearError();
	pDial = xrtTlsDialProxy(
		pEngine,
		pResolver,
		pProxy,
		"127.0.0.1",
		Address.Port,
		&ClientConfig,
		&DialConfig,
		NULL,
		NULL,
		testTlsProxyDone,
		&Test
	);
	testRequire(pDial != NULL, "TLS proxy dial creation failed");
	/* ProxyDial owns its own proxy reference after submission. */
	xrtNetProxyRelease(pProxy);
	pProxy = NULL;
	testTlsProxyWait(&Test.ConnectSeen, 1u,
		"TLS proxy CONNECT was not observed");
	testTlsProxyWait(&Test.TlsSeen, 1u,
		"TLS ClientHello did not cross the proxy tunnel");
	testRequire((xrtTlsDialState(pDial) == XTLS_DIAL_HANDSHAKE) &&
		xrtTlsDialTransportStats(pDial, &Stats) && Stats.HasWinner,
		"TLS proxy did not expose the connected transport state");

	if ( !TEST_TLS_PROXY_TIMEOUT ) {
		testRequire(xrtTlsDialCancel(pDial),
			"TLS proxy handshake cancellation was not accepted");
	}
	testTlsProxyWait(&Test.Done, 1u,
		"TLS proxy handshake did not reach a terminal state");
	if ( TEST_TLS_PROXY_TIMEOUT ) {
		testRequire((Test.Result == XNET_RESULT_TIMEOUT) &&
			(Test.ErrorKind == XERR_TIMEOUT) &&
			(Test.State == XTLS_DIAL_FAILED),
			"TLS proxy total timeout terminal mismatch");
	} else {
		testRequire((Test.Result == XNET_RESULT_CANCELLED) &&
			(Test.State == XTLS_DIAL_CANCELLED),
			"TLS proxy cancellation terminal mismatch");
	}

	pServer = (xnetstream*)xrtAtomicPtrLoad(&Test.Server, XMEMORY_ACQUIRE);
	if ( pServer != NULL ) (void)xrtNetStreamAbort(pServer);
	(void)xrtNetListenerClose(pListener);
	xrtNetStreamDestroy(pServer);
	xrtTlsDialDestroy(pDial);
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"TLS proxy resolver destruction failed");
	testTlsProxyWaitCleanup(pEngine);
	testRequire(xrtNetEngineStop(pEngine),
		"TLS proxy engine stop failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"TLS proxy engine destruction failed");
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsContextRelease(pTlsContext);
	printf("[PASS] TLS proxy dial %s after CONNECT\n",
		TEST_TLS_PROXY_TIMEOUT ? "timeout" : "cancellation");
	return 0;
}
