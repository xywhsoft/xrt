#include "../test.h"



#define TEST_TCP_SERVER_CLIENTS 16u



typedef struct testtcpserverreentrant {
	xatomic32 Accepts;
	xatomic32 Closes;
	xatomic32 Valid;
} testtcpserverreentrant;



/* 在 Accept 回调内部释放 Stream 并重入关闭整组 Server。 */
static bool testTcpServerReentrantAccept(
	xnetserver* pServer,
	size_t iEndpoint,
	xnetstream* pStream,
	ptr pData
)
{
	testtcpserverreentrant* pContext =
		(testtcpserverreentrant*)pData;
	bool bValid = (iEndpoint == 0) &&
		xrtNetStreamAbort(pStream) &&
		xrtNetServerClose(pServer);

	xrtNetStreamDestroy(pStream);
	if ( !bValid ) {
		xrtAtomic32Store(&pContext->Valid, 0, XMEMORY_RELEASE);
	}
	(void)xrtAtomic32FetchAdd(
		&pContext->Accepts,
		1,
		XMEMORY_ACQ_REL
	);
	return true;
}



/* 验证关闭回调只发布一次，且全部底层 Listener 已经退出。 */
static void testTcpServerReentrantClose(
	xnetserver* pServer,
	ptr pData
)
{
	testtcpserverreentrant* pContext =
		(testtcpserverreentrant*)pData;

	if ( (xrtNetServerState(pServer) != XNET_SERVER_CLOSED) ||
		 (xrtNetServerListener(pServer, 0) != NULL) ) {
		xrtAtomic32Store(&pContext->Valid, 0, XMEMORY_RELEASE);
	}
	(void)xrtAtomic32FetchAdd(
		&pContext->Closes,
		1,
		XMEMORY_ACQ_REL
	);
}



/* 等待 Server 与指定 Stream 全部进入关闭终态。 */
static void testTcpServerReentrantWait(
	xnetserver* pServer,
	xnetstream* const* ppStreams,
	size_t iCount
)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	for ( ;; ) {
		bool bDone = xrtNetServerState(pServer) == XNET_SERVER_CLOSED;

		for ( size_t i = 0; bDone && (i < iCount); i++ ) {
			bDone = xrtNetStreamState(ppStreams[i]) ==
				XNET_STREAM_CLOSED;
		}
		if ( bDone ) {
			return;
		}
		testRequire(!xrtDeadlineExpired(iDeadline),
			"TCP server reentrant wait timed out");
		xrtThreadYield();
	}
}



/* 验证聚合 Accept 队列硬上限会拒绝并关闭超量连接。 */
static void testTcpServerQueueLimit(xnetengine* pEngine)
{
	xnetserverconfig Config;
	xnetserverstats Stats;
	xnetserver* pServer;
	xnetstream* aClients[TEST_TCP_SERVER_CLIENTS];
	xnetstream* pAccepted;
	xnetaddr Local;
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	memset(aClients, 0, sizeof(aClients));
	xrtNetServerConfigInit(&Config);
	Config.AcceptQueueLimit = 1;
	testRequire(xrtNetAddrLoopback(
		&Config.Listen.Address,
		XNET_FAMILY_IPV4,
		0
	), "TCP server queue loopback address failed");
	pServer = xrtNetServerStart(pEngine, &Config, NULL, NULL, NULL);
	testRequire((pServer != NULL) &&
		 xrtNetServerLocal(pServer, 0, &Local),
		"TCP server queue start failed");
	for ( size_t i = 0; i < TEST_TCP_SERVER_CLIENTS; i++ ) {
		aClients[i] = xrtNetStreamConnect(
			pEngine,
			&Local,
			i,
			NULL,
			NULL,
			NULL
		);
		testRequire(aClients[i] != NULL,
			"TCP server queue client create failed");
	}
	for ( ;; ) {
		testRequire(xrtNetServerStats(pServer, &Stats),
			"TCP server queue stats failed");
		if ( (Stats.Accepted + Stats.Rejected) ==
			 TEST_TCP_SERVER_CLIENTS ) {
			break;
		}
		testRequire(!xrtDeadlineExpired(iDeadline),
			"TCP server queue accounting timed out");
		xrtThreadYield();
	}
	testRequire((Stats.Accepted == 1u) &&
		 (Stats.Rejected == (TEST_TCP_SERVER_CLIENTS - 1u)) &&
		 (Stats.QueuedAccepts == 1u) &&
		 (Stats.PeakQueuedAccepts == 1u),
		"TCP server queue limit was not enforced");
	pAccepted = xrtNetServerAccept(pServer);
	testRequire((pAccepted != NULL) &&
		 (xrtNetServerAccept(pServer) == NULL),
		"TCP server queue pull result mismatch");
	testRequire(xrtNetServerClose(pServer),
		"TCP server queue close failed");
	(void)xrtNetStreamAbort(pAccepted);
	for ( size_t i = 0; i < TEST_TCP_SERVER_CLIENTS; i++ ) {
		(void)xrtNetStreamAbort(aClients[i]);
	}
	testTcpServerReentrantWait(
		pServer,
		aClients,
		TEST_TCP_SERVER_CLIENTS
	);
	xrtNetStreamDestroy(pAccepted);
	for ( size_t i = 0; i < TEST_TCP_SERVER_CLIENTS; i++ ) {
		xrtNetStreamDestroy(aClients[i]);
	}
	xrtNetServerDestroy(pServer);
}



/* 验证回调重入关闭、唯一终态和聚合队列背压。 */
int main(void)
{
	testtcpserverreentrant Context;
	xnetengineconfig EngineConfig;
	xnetserverconfig ServerConfig;
	xnetserverevents Events;
	xnetengine* pEngine;
	xnetserver* pServer;
	xnetstream* pClient;
	xnetaddr Local;

	memset(&Context, 0, sizeof(Context));
	memset(&Events, 0, sizeof(Events));
	xrtAtomic32Init(&Context.Valid, 1);
	Events.Accept = testTcpServerReentrantAccept;
	Events.Close = testTcpServerReentrantClose;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP server reentrant engine start failed");

	xrtNetServerConfigInit(&ServerConfig);
	testRequire(xrtNetAddrLoopback(
		&ServerConfig.Listen.Address,
		XNET_FAMILY_IPV4,
		0
	), "TCP server reentrant loopback address failed");
	pServer = xrtNetServerStart(
		pEngine,
		&ServerConfig,
		&Events,
		NULL,
		&Context
	);
	testRequire((pServer != NULL) &&
		 xrtNetServerLocal(pServer, 0, &Local),
		"TCP server reentrant start failed");
	pClient = xrtNetStreamConnect(
		pEngine,
		&Local,
		1,
		NULL,
		NULL,
		NULL
	);
	testRequire(pClient != NULL,
		"TCP server reentrant client create failed");
	testTcpServerReentrantWait(pServer, NULL, 0);
	(void)xrtNetStreamAbort(pClient);
	testTcpServerReentrantWait(pServer, &pClient, 1);
	testRequire((xrtAtomic32Load(
		&Context.Valid,
		XMEMORY_ACQUIRE
	 ) != 0) &&
		 (xrtAtomic32Load(&Context.Accepts, XMEMORY_ACQUIRE) == 1u) &&
		 (xrtAtomic32Load(&Context.Closes, XMEMORY_ACQUIRE) == 1u),
		"TCP server reentrant callback contract mismatch");
	xrtNetStreamDestroy(pClient);
	xrtNetServerDestroy(pServer);

	testTcpServerQueueLimit(pEngine);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP server reentrant engine destroy failed");
	printf("[PASS] network TCP server reentrant and queue limit\n");
	return 0;
}
