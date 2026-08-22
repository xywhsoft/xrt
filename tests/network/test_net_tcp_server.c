#include "../test.h"



typedef struct testtcpservercontext {
	xatomic32 Closed;
} testtcpservercontext;



/* 在截止时间前等待 Server 或 Stream 进入终态。 */
static void testTcpServerWait(
	const xnetserver* pServer,
	const xnetstream* pFirst,
	const xnetstream* pSecond,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	for ( ;; ) {
		bool bServerDone = (pServer == NULL) ||
			(xrtNetServerState(pServer) == XNET_SERVER_CLOSED);
		bool bFirstDone = (pFirst == NULL) ||
			(xrtNetStreamState(pFirst) == XNET_STREAM_CLOSED);
		bool bSecondDone = (pSecond == NULL) ||
			(xrtNetStreamState(pSecond) == XNET_STREAM_CLOSED);

		if ( bServerDone && bFirstDone && bSecondDone ) {
			return;
		}
		testRequire(!xrtDeadlineExpired(iDeadline), sMessage);
		xrtThreadYield();
	}
}



/* 在截止时间前等待唯一 Server Close 回调发布。 */
static void testTcpServerCloseWait(
	const testtcpservercontext* pContext
)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	while ( xrtAtomic32Load(
		&pContext->Closed,
		XMEMORY_ACQUIRE
	) == 0 ) {
		testRequire(
			!xrtDeadlineExpired(iDeadline),
			"TCP server close callback missing"
		);
		xrtThreadYield();
	}
}



/* 在截止时间前从聚合队列取走一个 Stream。 */
static xnetstream* testTcpServerAccept(xnetserver* pServer)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);
	xnetstream* pStream;

	for ( ;; ) {
		pStream = xrtNetServerAccept(pServer);
		if ( pStream != NULL ) {
			return pStream;
		}
		testRequire(xrtGetError() == NULL,
			"TCP server pull accept returned an error");
		testRequire(!xrtDeadlineExpired(iDeadline),
			"TCP server did not aggregate an accepted stream");
		xrtThreadYield();
	}
}



/* 记录整组 Listener 已经完全关闭。 */
static void testTcpServerClose(xnetserver* pServer, ptr pData)
{
	testtcpservercontext* pContext =
		(testtcpservercontext*)pData;

	testRequire(xrtNetServerState(pServer) == XNET_SERVER_CLOSED,
		"TCP server close callback observed a non-terminal state");
	(void)xrtAtomic32FetchAdd(
		&pContext->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证双栈共享端口、聚合 Accept、本地分发和整组生命周期。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetserverconfig ServerConfig;
	xnetserverevents ServerEvents;
	xnetlistenconfig Additional;
	testtcpservercontext Context;
	xnetserverstats Stats;
	xnetengine* pEngine;
	xnetserver* pServer;
	xnetlistener* pFirstListener;
	xnetlistener* pSecondListener;
	xnetstream* pClient4;
	xnetstream* pClient6;
	xnetstream* pAccepted4 = NULL;
	xnetstream* pAccepted6 = NULL;
	xnetaddr Local4;
	xnetaddr Local6;

	memset(&Context, 0, sizeof(Context));
	memset(&ServerEvents, 0, sizeof(ServerEvents));
	ServerEvents.Close = testTcpServerClose;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP server engine start failed");

	xrtNetServerConfigInit(&ServerConfig);
	testRequire(xrtNetAddrLoopback(
		&ServerConfig.Listen.Address,
		XNET_FAMILY_IPV6,
		0
	), "TCP server IPv6 loopback address failed");
	ServerConfig.Listen.Affinity = 0;
	ServerConfig.Listen.Distribution = XNET_ACCEPT_LOCAL;
	ServerConfig.Listen.IPv6Only = true;
	xrtNetListenConfigInit(&Additional);
	testRequire(xrtNetAddrLoopback(
		&Additional.Address,
		XNET_FAMILY_IPV4,
		0
	), "TCP server IPv4 loopback address failed");
	Additional.Affinity = 1;
	Additional.Distribution = XNET_ACCEPT_LOCAL;
	ServerConfig.Additional = &Additional;
	ServerConfig.AdditionalCount = 1;
	ServerConfig.SharedPort = true;
	pServer = xrtNetServerStart(
		pEngine,
		&ServerConfig,
		&ServerEvents,
		NULL,
		&Context
	);
	testRequire((pServer != NULL) &&
		 (xrtNetServerState(pServer) == XNET_SERVER_OPEN),
		"TCP server start failed");
	testRequire((xrtNetServerEndpointCount(pServer) == 2) &&
		 (xrtNetServerListenerCount(pServer) == 2) &&
		 (xrtNetServerData(pServer) == &Context),
		"TCP server topology query mismatch");
	testRequire(xrtNetServerLocal(pServer, 0, &Local6) &&
		 xrtNetServerLocal(pServer, 1, &Local4) &&
		 (Local6.Family == XNET_FAMILY_IPV6) &&
		 (Local4.Family == XNET_FAMILY_IPV4) &&
		 (Local6.Port != 0) && (Local6.Port == Local4.Port),
		"TCP server shared dynamic port mismatch");
	pFirstListener = xrtNetServerListener(pServer, 0);
	pSecondListener = xrtNetServerListener(pServer, 1);
	testRequire((pFirstListener != NULL) && (pSecondListener != NULL) &&
		 (xrtNetWorkerIndex(xrtNetListenerWorker(pFirstListener)) == 0) &&
		 (xrtNetWorkerIndex(xrtNetListenerWorker(pSecondListener)) == 1),
		"TCP server listener affinity mismatch");

	pClient6 = xrtNetStreamConnect(
		pEngine,
		&Local6,
		0,
		NULL,
		NULL,
		NULL
	);
	pClient4 = xrtNetStreamConnect(
		pEngine,
		&Local4,
		1,
		NULL,
		NULL,
		NULL
	);
	testRequire((pClient6 != NULL) && (pClient4 != NULL),
		"TCP server loopback clients failed");
	for ( size_t i = 0; i < 2; i++ ) {
		xnetstream* pStream = testTcpServerAccept(pServer);
		xnetaddr Local;

		testRequire(xrtNetStreamLocal(pStream, &Local),
			"TCP server accepted local address failed");
		if ( Local.Family == XNET_FAMILY_IPV6 ) {
			pAccepted6 = pStream;
		} else {
			pAccepted4 = pStream;
		}
	}
	testRequire((pAccepted4 != NULL) && (pAccepted6 != NULL) &&
		 (xrtNetStreamData(pAccepted4) == &Context) &&
		 (xrtNetStreamData(pAccepted6) == &Context) &&
		 (xrtNetStreamWorker(pAccepted6) ==
		  xrtNetListenerWorker(pFirstListener)) &&
		 (xrtNetStreamWorker(pAccepted4) ==
		  xrtNetListenerWorker(pSecondListener)),
		"TCP server local accept distribution mismatch");
	testRequire(xrtNetServerStats(pServer, &Stats) &&
		 (Stats.State == XNET_SERVER_OPEN) &&
		 (Stats.Accepted == 2) && (Stats.Rejected == 0) &&
		 (Stats.Endpoints == 2) && (Stats.Listeners == 2) &&
		 (Stats.QueuedAccepts == 0) &&
		 (Stats.PeakQueuedAccepts >= 1),
		"TCP server aggregate stats mismatch");

	testRequire(xrtNetServerClose(pServer),
		"TCP server close request failed");
	(void)xrtNetStreamAbort(pAccepted4);
	(void)xrtNetStreamAbort(pAccepted6);
	(void)xrtNetStreamAbort(pClient4);
	(void)xrtNetStreamAbort(pClient6);
	testTcpServerWait(
		pServer,
		pAccepted4,
		pAccepted6,
		"TCP server did not close"
	);
	testTcpServerWait(
		NULL,
		pClient4,
		pClient6,
		"TCP server clients did not close"
	);
	testTcpServerCloseWait(&Context);
	testRequire(xrtAtomic32Load(
		&Context.Closed,
		XMEMORY_ACQUIRE
	) == 1, "TCP server close callback count mismatch");
	testRequire(xrtNetServerStats(pServer, &Stats) &&
		 (Stats.State == XNET_SERVER_CLOSED) &&
		 (Stats.ClosedListeners == 2),
		"TCP server terminal stats mismatch");

	xrtNetListenerDestroy(pFirstListener);
	xrtNetListenerDestroy(pSecondListener);
	xrtNetStreamDestroy(pAccepted4);
	xrtNetStreamDestroy(pAccepted6);
	xrtNetStreamDestroy(pClient4);
	xrtNetStreamDestroy(pClient6);
	xrtNetServerDestroy(pServer);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP server engine destroy failed");
	printf("[PASS] network TCP server\n");
	return 0;
}
