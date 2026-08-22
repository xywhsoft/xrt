#include "../test.h"



#if !defined(TEST_NET_SYNC_OOM_BACKEND)
	#define TEST_NET_SYNC_OOM_BACKEND XNET_PORT_SELECT
	#define TEST_NET_SYNC_OOM_BACKEND_NAME "select"
#endif

#define TEST_NET_SYNC_OOM_TCP_BYTES (128u * 1024u)



typedef struct testnetsyncoom {
	xnetaddr ResolveAddress;
	uint16 Port;
} testnetsyncoom;



/* 自定义 Resolver 始终返回端口无关的本地回环地址。 */
static xnetaddrlist* testNetSyncOomLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	testnetsyncoom* pContext = (testnetsyncoom*)pData;

	if ( (strcmp(sHost, "sync-oom.test") != 0) ||
		 ((Family != XNET_FAMILY_UNSPEC) &&
		  (Family != XNET_FAMILY_IPV4)) ) {
		return NULL;
	}
	return xrtNetAddrListCreate(&pContext->ResolveAddress, 1);
}



/* 等待 TCP 拉取缓冲达到指定长度。 */
static void testNetSyncOomAvailable(xnetstream* pStream, size_t iSize)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	while ( xrtNetStreamAvailable(pStream) < iSize ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"network sync OOM TCP buffer timed out");
		xrtThreadYield();
	}
}



/* 等待 UDP 拉取队列达到指定数据包数量。 */
static void testNetSyncOomQueued(xnetudp* pUdp, size_t iCount)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	while ( xrtNetUdpQueued(pUdp) < iCount ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"network sync OOM UDP queue timed out");
		xrtThreadYield();
	}
}



/* 正常关闭 Stream 并释放调用方引用。 */
static void testNetSyncOomStreamClose(xnetstream* pStream)
{
	if ( xrtNetStreamState(pStream) != XNET_STREAM_CLOSED ) {
		testRequire(xrtNetStreamClose(pStream) && xrtNetStreamWait(
			pStream,
			XNET_STREAM_WAIT_CLOSE,
			xrtDeadlineAfter(5000000u),
			NULL
		), "network sync OOM TCP close failed");
	}
	xrtNetStreamDestroy(pStream);
}



/* 正常关闭 UDP 并释放调用方引用。 */
static void testNetSyncOomUdpClose(xnetudp* pUdp)
{
	if ( xrtNetUdpState(pUdp) != XNET_UDP_CLOSED ) {
		testRequire(xrtNetUdpClose(pUdp) && xrtNetUdpWait(
			pUdp,
			XNET_UDP_WAIT_CLOSE,
			xrtDeadlineAfter(5000000u),
			NULL
		), "network sync OOM UDP close failed");
	}
	xrtNetUdpDestroy(pUdp);
}



/* 验证同步外观在任一分配失败点都不消费输入并可完整恢复。 */
int main(void)
{
	testnetsyncoom Context;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetresolverconfig ResolverConfig;
	xnetudpconfig UdpConfig;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetresolver* pResolver;
	xnetstream* pClient;
	xnetstream* pServer;
	xnetstream* pDialClient;
	xnetstream* pDialServer;
	xnetudp* pUdpClient;
	xnetudp* pUdpServer;
	xnetbytes* pBytes;
	xnetudppacket* pPacket;
	xnetudpbatch* pBatch;
	xbytesview View;
	xnetaddr Address;
	xdeadline iDeadline;
	uint8* pPayload;
	bool bTriggered;

	memset(&Context, 0, sizeof(Context));
	pPayload = (uint8*)malloc(TEST_NET_SYNC_OOM_TCP_BYTES);
	testRequire(pPayload != NULL,
		"network sync OOM system payload allocation failed");
	for ( size_t i = 0; i < TEST_NET_SYNC_OOM_TCP_BYTES; i++ ) {
		pPayload[i] = (uint8)(i * 29u);
	}

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_NET_SYNC_OOM_BACKEND;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(pEngine != NULL, "network sync OOM engine creation failed");
	testRequire(xrtNetEngineStart(pEngine),
		"network sync OOM engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "network sync OOM listener address failed");
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire(pListener != NULL,
		"network sync OOM listener creation failed");
	testRequire(xrtNetListenerLocal(pListener, &Address),
		"network sync OOM listener local address failed");
	Context.ResolveAddress = Address;
	Context.ResolveAddress.Port = 0;
	Context.Port = Address.Port;
	pClient = xrtNetStreamConnect(
		pEngine,
		&Address,
		1,
		NULL,
		NULL,
		NULL
	);
	pServer = xrtNetListenerAcceptWait(
		pListener,
		xrtDeadlineAfter(5000000u),
		NULL
	);
	testRequire((pClient != NULL) && (pServer != NULL) && xrtNetStreamWait(
		pClient,
		XNET_STREAM_WAIT_OPEN,
		xrtDeadlineAfter(5000000u),
		NULL
	), "network sync OOM TCP setup failed");

	testRequire(xrtNetStreamSend(
		pClient,
		pPayload,
		TEST_NET_SYNC_OOM_TCP_BYTES
	) == XNET_RESULT_OK,
		"network sync OOM TCP setup send failed");
	testNetSyncOomAvailable(pServer, TEST_NET_SYNC_OOM_TCP_BYTES);
	testRequire(
		xrtMemDebugFailAfter(0),
		"network sync TCP OOM injection setup failed"
	);
	pBytes = xrtNetStreamRecv(
		pServer,
		TEST_NET_SYNC_OOM_TCP_BYTES,
		xrtDeadlineAfter(5000000u),
		NULL
	);
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	if ( (pBytes != NULL) ||
		 (xrtErrorKind(xrtGetError()) != XERR_MEMORY) ||
		 (xrtNetStreamAvailable(pServer) != TEST_NET_SYNC_OOM_TCP_BYTES) ||
		 !bTriggered ) {
		fprintf(
			stderr,
			"[ERROR] TCP OOM bytes=%p kind=%d available=%zu triggered=%u\n",
			(void*)pBytes,
			(int)xrtErrorKind(xrtGetError()),
			xrtNetStreamAvailable(pServer),
			(unsigned)bTriggered
		);
	}
	testRequire((pBytes == NULL) &&
		bTriggered &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtNetStreamAvailable(pServer) == TEST_NET_SYNC_OOM_TCP_BYTES),
		"network sync OOM TCP receive consumed bytes");
	xrtClearError();
	pBytes = xrtNetStreamRecv(
		pServer,
		TEST_NET_SYNC_OOM_TCP_BYTES,
		xrtDeadlineAfter(5000000u),
		NULL
	);
	View = xrtNetBytesView(pBytes);
	testRequire((pBytes != NULL) &&
		(View.Size == TEST_NET_SYNC_OOM_TCP_BYTES) &&
		(memcmp(View.Data, pPayload, View.Size) == 0),
		"network sync OOM TCP receive did not recover");
	xrtNetBytesDestroy(pBytes);

	xrtNetUdpConfigInit(&UdpConfig);
	testRequire(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0),
		"network sync OOM UDP address failed");
	pUdpServer = xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&UdpConfig,
		NULL,
		NULL
	);
	testRequire((pUdpServer != NULL) &&
		xrtNetUdpLocal(pUdpServer, &Address),
		"network sync OOM UDP server failed");
	pUdpClient = xrtNetUdpConnect(
		pEngine,
		&Address,
		1,
		&UdpConfig,
		NULL,
		NULL
	);
	testRequire((pUdpClient != NULL) && xrtNetUdpWait(
		pUdpServer,
		XNET_UDP_WAIT_OPEN,
		xrtDeadlineAfter(5000000u),
		NULL
	) && xrtNetUdpWait(
		pUdpClient,
		XNET_UDP_WAIT_OPEN,
		xrtDeadlineAfter(5000000u),
		NULL
	), "network sync OOM UDP setup failed");
	testRequire(xrtNetUdpSend(pUdpClient, "U", 1) == XNET_RESULT_OK,
		"network sync OOM UDP setup send failed");
	testNetSyncOomQueued(pUdpServer, 1);
	testRequire(
		xrtMemDebugFailAfter(0),
		"network sync UDP OOM injection setup failed"
	);
	pBatch = xrtNetUdpReceiveBatchWait(
		pUdpServer,
		16,
		xrtDeadlineAfter(5000000u),
		NULL
	);
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	testRequire((pBatch == NULL) &&
		bTriggered &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtNetUdpQueued(pUdpServer) == 1),
		"network sync OOM UDP batch consumed a packet");
	xrtClearError();
	pPacket = xrtNetUdpReceiveWait(
		pUdpServer,
		xrtDeadlineAfter(5000000u),
		NULL
	);
	testRequire((pPacket != NULL) &&
		(xrtNetUdpPacketSize(pPacket) == 1) &&
		(xrtNetUdpPacketData(pPacket)[0] == 'U'),
		"network sync OOM UDP receive did not recover");
	xrtNetUdpPacketDestroy(pPacket);

	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1;
	ResolverConfig.CacheEntries = 0;
	ResolverConfig.Lookup = testNetSyncOomLookup;
	ResolverConfig.LookupData = &Context;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL,
		"network sync OOM resolver create failed");
	testRequire(
		xrtMemDebugFailAfter(0),
		"network sync Dial OOM injection setup failed"
	);
	pDialClient = xrtNetConnect(
		pEngine,
		pResolver,
		"sync-oom.test",
		Context.Port,
		NULL,
		NULL,
		NULL,
		xrtDeadlineAfter(5000000u),
		NULL
	);
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	testRequire((pDialClient == NULL) &&
		bTriggered &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"network sync OOM Dial submission mismatch");
	xrtClearError();
	pDialClient = xrtNetConnect(
		pEngine,
		pResolver,
		"sync-oom.test",
		Context.Port,
		NULL,
		NULL,
		NULL,
		xrtDeadlineAfter(5000000u),
		NULL
	);
	pDialServer = xrtNetListenerAcceptWait(
		pListener,
		xrtDeadlineAfter(5000000u),
		NULL
	);
	testRequire((pDialClient != NULL) && (pDialServer != NULL),
		"network sync OOM Dial did not recover");

	testNetSyncOomStreamClose(pDialClient);
	testNetSyncOomStreamClose(pDialServer);
	testNetSyncOomStreamClose(pClient);
	testNetSyncOomStreamClose(pServer);
	testNetSyncOomUdpClose(pUdpClient);
	testNetSyncOomUdpClose(pUdpServer);
	testRequire(xrtNetListenerClose(pListener),
		"network sync OOM listener close failed");
	iDeadline = xrtDeadlineAfter(5000000u);
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"network sync OOM listener close timed out");
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"network sync OOM resolver destroy failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"network sync OOM engine retained an object");
	free(pPayload);
	printf("[PASS] network sync %s OOM rollback\n",
		TEST_NET_SYNC_OOM_BACKEND_NAME);
	return 0;
}
