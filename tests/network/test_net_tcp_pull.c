#include "../test.h"



#if !defined(TEST_TCP_PULL_BACKEND)
	#define TEST_TCP_PULL_BACKEND XNET_PORT_SELECT
	#define TEST_TCP_PULL_BACKEND_NAME "select"
#endif



/* 等待 Listener 完成指定数量的接受和拒绝。 */
static void testTcpPullStats(
	xnetlistener* pListener,
	uint64 iAccepted,
	uint64 iRejected,
	xnetlistenerstats* pStats
)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	for ( ;; ) {
		testRequire(xrtNetListenerStats(pListener, pStats),
			"TCP pull listener stats failed");
		if ( (pStats->Accepted == iAccepted) &&
			 (pStats->Rejected == iRejected) ) {
			return;
		}
		testRequire(!xrtDeadlineExpired(iDeadline),
			"TCP pull listener stats timed out");
		xrtSleep(1);
	}
}



/* 等待 Stream 进入唯一关闭终态。 */
static void testTcpPullCloseStream(xnetstream* pStream)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	testRequire(xrtNetStreamClose(pStream),
		"TCP pull stream close failed");
	while ( xrtNetStreamState(pStream) != XNET_STREAM_CLOSED ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"TCP pull stream close timed out");
		xrtSleep(1);
	}
	xrtNetStreamDestroy(pStream);
}



/* 验证核心拉取队列的硬上限、所有权转移和关闭回收。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerstats Stats;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetstream* pAccepted;
	xnetsocket First;
	xnetsocket Second;
	xnetaddr Address;
	xdeadline iDeadline;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TCP_PULL_BACKEND;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP pull engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TCP pull listener address failed");
	ListenConfig.AcceptQueueLimit = 1;
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		NULL,
		NULL,
		&Stats
	);
	testRequire((pListener != NULL) &&
		 xrtNetListenerLocal(pListener, &Address) &&
		 (xrtNetListenerData(pListener) == &Stats),
		"TCP pull listener start failed");

	First = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		0
	);
	Second = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		0
	);
	testRequire((First != NULL) && (Second != NULL) &&
		 (xrtNetSocketConnect(First, &Address) == XNET_RESULT_OK) &&
		 (xrtNetSocketConnect(Second, &Address) == XNET_RESULT_OK),
		"TCP pull peers failed");
	testTcpPullStats(pListener, 1, 1, &Stats);
	testRequire((Stats.QueuedAccepts == 1) &&
		 (Stats.PeakQueuedAccepts == 1) &&
		 (Stats.AcceptWaiters == 0),
		"TCP pull queue statistics mismatch");

	pAccepted = xrtNetListenerAccept(pListener);
	testRequire((pAccepted != NULL) &&
		 (xrtNetStreamState(pAccepted) == XNET_STREAM_OPEN) &&
		 (xrtNetStreamData(pAccepted) == NULL) &&
		 (xrtNetListenerAccept(pListener) == NULL),
		"TCP pull queue ownership or data isolation mismatch");
	testRequire(xrtNetListenerStats(pListener, &Stats) &&
		 (Stats.QueuedAccepts == 0) &&
		 (Stats.PeakQueuedAccepts == 1),
		"TCP pull queue drain statistics mismatch");

	testTcpPullCloseStream(pAccepted);
	testRequire(xrtNetSocketClose(First) && xrtNetSocketClose(Second),
		"TCP pull peers close failed");
	testRequire(xrtNetListenerClose(pListener),
		"TCP pull listener close failed");
	iDeadline = xrtDeadlineAfter(5000000u);
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"TCP pull listener close timed out");
		xrtSleep(1);
	}
	testRequire((xrtNetListenerAccept(pListener) == NULL) &&
		 xrtNetListenerStats(pListener, &Stats) &&
		 (Stats.QueuedAccepts == 0) &&
		 (Stats.ActiveAccepts == 0) &&
		 (Stats.ActiveDispatches == 0),
		"TCP pull closed listener mismatch");
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP pull engine destroy failed");
	printf("[PASS] network TCP pull %s\n", TEST_TCP_PULL_BACKEND_NAME);
	return 0;
}
