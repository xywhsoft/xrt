#include "../test.h"

/* Only public APIs: Open may synchronously close, and Close may consume the
 * caller reference. The constructor publication handshake excludes a test
 * race between Connect's return and that reference being consumed. */
typedef struct testopencontext {
	xatomic32 Ready, Open, Closed, Returned, Barrier;
	xatomic32 Done;
} testopencontext;

static void testOpenWait(xatomic32* pValue, cstr sMessage)
{
	for (unsigned i = 0; i < 10000; ++i) {
		if (xrtAtomic32Load(pValue, XMEMORY_ACQUIRE)) return;
		xrtSleep(1);
	}
	testRequire(false, sMessage);
}

static void testOpenClose(xnetstream* pStream, xnetresult Result,
	const xerror* pError, ptr pData)
{
	testopencontext* pContext = pData;
	(void)Result; (void)pError;
	testRequire(xrtAtomic32FetchAdd(&pContext->Closed, 1, XMEMORY_ACQ_REL) == 0,
		"duplicate Close after reentrant Open");
	xrtNetStreamDestroy(pStream);
}

static void testOpenAbort(xnetstream* pStream, ptr pData)
{
	testopencontext* pContext = pData;
	testOpenWait(&pContext->Ready, "constructor did not publish its reference");
	testRequire(xrtAtomic32FetchAdd(&pContext->Open, 1, XMEMORY_ACQ_REL) == 0,
		"duplicate Open");
	testRequire(xrtNetStreamAbort(pStream), "abort inside Open failed");
	/* Close consumed our reference. Do not touch the borrowed stream again. */
	xrtAtomic32Store(&pContext->Returned, 1, XMEMORY_RELEASE);
}

/* The peer reset is established before Ready. Sending from Open therefore
 * exercises the synchronous send-failure/Close continuation as well. */
static void testOpenReset(xnetstream* pStream, ptr pData)
{
	testopencontext* pContext = pData;
	static const unsigned char Bytes[4096] = {1};
	testOpenWait(&pContext->Ready, "reset peer was not published");
	testRequire(xrtAtomic32FetchAdd(&pContext->Open, 1, XMEMORY_ACQ_REL) == 0,
		"duplicate reset Open");
	/* The return describes queue admission; Close reports the transport error. */
	(void)xrtNetStreamSend(pStream, Bytes, sizeof(Bytes));
	xrtAtomic32Store(&pContext->Returned, 1, XMEMORY_RELEASE);
}

static void testOpenBarrier(xnetworker* pWorker, ptr pData)
{
	(void)pWorker;
	xrtAtomic32Store(&((testopencontext*)pData)->Barrier, 1, XMEMORY_RELEASE);
}

/* The Dial winner is committed before Open, so an Abort request in Open
 * closes the stream but does not retroactively cancel that successful Dial.
 * Dial's Open/Close callbacks borrow the candidate. Its caller reference is
 * transferred only by a successful Done; do not consume it from Close. */
static void testDialClose(xnetstream* pStream, xnetresult Result,
	const xerror* pError, ptr pData)
{
	(void)pStream; (void)Result; (void)pError;
	testRequire(xrtAtomic32FetchAdd(&((testopencontext*)pData)->Closed,
		1, XMEMORY_ACQ_REL) == 0, "duplicate Dial Close");
}

static void testDialDone(xnetdial* pDial, xnetresult Result,
	xnetstream* pStream, const xerror* pError, ptr pData)
{
	(void)pDial; (void)pError;
	xrtNetStreamDestroy(pStream);
	testRequire(Result == XNET_RESULT_OK && pStream != NULL,
		"committed Dial winner was lost after Open requested Abort");
	testRequire(xrtAtomic32FetchAdd(&((testopencontext*)pData)->Done,
		1, XMEMORY_ACQ_REL) == 0, "duplicate Dial Done");
}

static void testDialRun(xnetportbackend Backend, xnetfamily Family)
{
	testopencontext Context = {0};
	xrtAtomic32Init(&Context.Ready, 0);
	xrtAtomic32Init(&Context.Open, 0);
	xrtAtomic32Init(&Context.Closed, 0);
	xrtAtomic32Init(&Context.Returned, 0);
	xrtAtomic32Init(&Context.Barrier, 0);
	xrtAtomic32Init(&Context.Done, 0);
	xnetaddr Address;
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetdialconfig DialConfig;
	xnetstreamevents Events = {0};
	xnetsocket Listener = xrtNetSocketOpen(Family, XNET_SOCKET_STREAM, 0);
	testRequire(Listener && xrtNetAddrLoopback(&Address, Family, 0) &&
		xrtNetSocketBind(Listener, &Address) && xrtNetSocketListen(Listener, 8) &&
		xrtNetSocketLocal(Listener, &Address), "Dial raw listener setup failed");
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = Backend; EngineConfig.Workers = 1;
	xnetengine* pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(pEngine && xrtNetEngineStart(pEngine), "Dial engine start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	xnetresolver* pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "resolver create failed");
	xrtNetDialConfigInit(&DialConfig);
	/* No overall timer may paper over a stranded winning candidate. */
	DialConfig.Timeout = 0; DialConfig.Family = Family;
	Events.Open = testOpenAbort; Events.Close = testDialClose;
	xnetdial* pDial = xrtNetDial(pEngine, pResolver,
		Family == XNET_FAMILY_IPV4 ? "127.0.0.1" : "::1", Address.Port,
		&DialConfig, &Events, &Context, testDialDone, &Context);
	testRequire(pDial != NULL, "Dial submission failed");
	xrtAtomic32Store(&Context.Ready, 1, XMEMORY_RELEASE);
	testOpenWait(&Context.Returned, "Dial Open did not return");
	testOpenWait(&Context.Closed, "Dial Close missing");
	testOpenWait(&Context.Done, "Dial stranded after Open aborted its winner");
	testRequire(xrtNetDialState(pDial) == XNET_DIAL_CONNECTED,
		"committed Dial winner state mismatch");
	testRequire(xrtNetEnginePost(pEngine, 0, testOpenBarrier, &Context),
		"Dial callback-tail barrier post failed");
	testOpenWait(&Context.Barrier, "Dial callback tail missing");
	xrtNetDialDestroy(pDial);
	testRequire(xrtNetResolverDestroy(pResolver), "resolver did not drain");
	testRequire(xrtNetSocketClose(Listener), "Dial listener close failed");
	testRequire(xrtNetEngineDestroy(pEngine), "Dial engine did not drain and join");
}

static void testOpenRun(xnetportbackend Backend, xnetfamily Family, bool Reset)
{
	for (unsigned i = 0; i < 16; ++i) {
		testopencontext Context;
		xnetaddr Address;
		xnetengineconfig EngineConfig;
		xnetstreamconfig StreamConfig;
		xnetstreamevents Events = {0};
		memset(&Context, 0, sizeof(Context));
		xrtAtomic32Init(&Context.Ready, 0);
		xrtAtomic32Init(&Context.Open, 0);
		xrtAtomic32Init(&Context.Closed, 0);
		xrtAtomic32Init(&Context.Returned, 0);
		xrtAtomic32Init(&Context.Barrier, 0);
		xnetsocket Listener = xrtNetSocketOpen(Family, XNET_SOCKET_STREAM, 0);
		testRequire(Listener && xrtNetAddrLoopback(&Address, Family, 0) &&
			xrtNetSocketBind(Listener, &Address) && xrtNetSocketListen(Listener, 8) &&
			xrtNetSocketLocal(Listener, &Address), "raw listener setup failed");
		xrtNetEngineConfigInit(&EngineConfig);
		EngineConfig.Backend = Backend;
		EngineConfig.Workers = 1;
		xnetengine* pEngine = xrtNetEngineCreate(&EngineConfig);
		testRequire(pEngine && xrtNetEngineStart(pEngine), "engine start failed");
		xrtNetStreamConfigInit(&StreamConfig);
		Events.Open = Reset ? testOpenReset : testOpenAbort;
		Events.Close = testOpenClose;
		xnetstream* pStream = xrtNetStreamConnect(pEngine, &Address, 0,
			&StreamConfig, &Events, &Context);
		testRequire(pStream != NULL, "connect failed");
		if (Reset) {
			xnetsocket Peer = NULL;
			testRequire(xrtNetSocketAccept(Listener, &Peer, NULL) == XNET_RESULT_OK &&
				xrtNetSocketSet(Peer, XNET_OPTION_LINGER, 0) && xrtNetSocketClose(Peer),
				"peer reset setup failed");
		}
		xrtAtomic32Store(&Context.Ready, 1, XMEMORY_RELEASE);
		testOpenWait(&Context.Returned, "Open did not return");
		testOpenWait(&Context.Closed, "Close missing");
		testRequire(xrtNetEnginePost(pEngine, 0, testOpenBarrier, &Context),
			"callback-tail barrier post failed");
		testOpenWait(&Context.Barrier, "callback tail did not complete");
		testRequire(xrtNetSocketClose(Listener), "listener close failed");
		testRequire(xrtNetEngineDestroy(pEngine), "engine did not drain and join");
		testRequire(xrtAtomic32Load(&Context.Open, XMEMORY_ACQUIRE) == 1 &&
			xrtAtomic32Load(&Context.Closed, XMEMORY_ACQUIRE) == 1,
			"Open/Close counts mismatch");
	}
}

int main(int argc, char** argv)
{
	(void)argc; (void)argv;
	testDialRun(XNET_PORT_SELECT, XNET_FAMILY_IPV4);
	testDialRun(XNET_PORT_SELECT, XNET_FAMILY_IPV6);
	testOpenRun(XNET_PORT_SELECT, XNET_FAMILY_IPV4, false);
	testOpenRun(XNET_PORT_SELECT, XNET_FAMILY_IPV6, false);
	testOpenRun(XNET_PORT_SELECT, XNET_FAMILY_IPV4, true);
	testOpenRun(XNET_PORT_SELECT, XNET_FAMILY_IPV6, true);
	#if defined(XRT_FEATURE_NET_PORT_IOCP)
		testDialRun(XNET_PORT_IOCP, XNET_FAMILY_IPV4);
		testDialRun(XNET_PORT_IOCP, XNET_FAMILY_IPV6);
		testOpenRun(XNET_PORT_IOCP, XNET_FAMILY_IPV4, false);
		testOpenRun(XNET_PORT_IOCP, XNET_FAMILY_IPV6, false);
		testOpenRun(XNET_PORT_IOCP, XNET_FAMILY_IPV4, true);
		testOpenRun(XNET_PORT_IOCP, XNET_FAMILY_IPV6, true);
	#elif defined(XRT_FEATURE_NET_PORT_EPOLL)
		testDialRun(XNET_PORT_EPOLL, XNET_FAMILY_IPV4);
		testDialRun(XNET_PORT_EPOLL, XNET_FAMILY_IPV6);
		testOpenRun(XNET_PORT_EPOLL, XNET_FAMILY_IPV4, false);
		testOpenRun(XNET_PORT_EPOLL, XNET_FAMILY_IPV6, false);
		testOpenRun(XNET_PORT_EPOLL, XNET_FAMILY_IPV4, true);
		testOpenRun(XNET_PORT_EPOLL, XNET_FAMILY_IPV6, true);
	#endif
	puts("[PASS] TCP reentrant Open abort/reset and committed Dial ownership");
	return 0;
}
