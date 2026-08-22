#include "../test.h"



#if !defined(TEST_TCP_ACCEPT_BACKEND)
	#define TEST_TCP_ACCEPT_BACKEND XNET_PORT_SELECT
	#define TEST_TCP_ACCEPT_BACKEND_NAME "select"
#endif



#define TEST_TCP_ACCEPT_BURST 64u



/* 等待 Listener 统计达到指定状态。 */
static void testTcpAcceptStats(
	xnetlistener* pListener,
	uint64 iAccepted,
	uint64 iRejected,
	uint32 iQueued,
	uint32 iWaiters
)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);
	xnetlistenerstats Stats;

	for ( ;; ) {
		testRequire(xrtNetListenerStats(pListener, &Stats),
			"TCP accept Future stats failed");
		if ( (Stats.Accepted == iAccepted) &&
			 (Stats.Rejected == iRejected) &&
			 (Stats.QueuedAccepts == iQueued) &&
			 (Stats.AcceptWaiters == iWaiters) ) {
			return;
		}
		if ( xrtDeadlineExpired(iDeadline) ) {
			fprintf(
				stderr,
				"TCP accept stats expected %llu/%llu/%u/%u, got %llu/%llu/%u/%u\n",
				(unsigned long long)iAccepted,
				(unsigned long long)iRejected,
				iQueued,
				iWaiters,
				(unsigned long long)Stats.Accepted,
				(unsigned long long)Stats.Rejected,
				Stats.QueuedAccepts,
				Stats.AcceptWaiters
			);
			testRequire(false, "TCP accept Future stats timed out");
		}
		xrtSleep(1);
	}
}



/* 等待成功 Future 并保留其中的 Stream 引用。 */
static xnetstream* testTcpAcceptValue(
	xfuture* pFuture,
	cstr sMessage
)
{
	xnetstream* pStream;

	testRequire((pFuture != NULL) &&
		 (xrtFutureWaitFor(pFuture, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pFuture) == XFUTURE_RESOLVED),
		sMessage);
	pStream = (xnetstream*)xrtFutureValue(pFuture);
	testRequire(pStream != NULL, "TCP accept Future value missing");
	return xrtNetStreamRef(pStream);
}



/* 等待 Stream 条件 Future 成功。 */
static void testTcpAcceptReady(xfuture* pFuture, cstr sMessage)
{
	testRequire((pFuture != NULL) &&
		 (xrtFutureWaitFor(pFuture, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pFuture) == XFUTURE_RESOLVED),
		sMessage);
}



/* 正常关闭并释放一个调用方 Stream 引用。 */
static void testTcpAcceptCloseStream(xnetstream* pStream)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	if ( xrtNetStreamState(pStream) != XNET_STREAM_CLOSED ) {
		testRequire(xrtNetStreamClose(pStream),
			"TCP accept Future stream close failed");
	}
	while ( xrtNetStreamState(pStream) != XNET_STREAM_CLOSED ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"TCP accept Future stream close timed out");
		xrtSleep(1);
	}
	xrtNetStreamDestroy(pStream);
}



/* 覆盖连接等待、Accept FIFO、取消、排队、溢出和关闭终态。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetenginestats EngineStats;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetstream* pClient1;
	xnetstream* pClient2;
	xnetstream* pClient3;
	xnetstream* pClient4;
	xnetstream* pServer1;
	xnetstream* pServer2;
	xnetstream* pServer3;
	xnetstream* BurstClients[TEST_TCP_ACCEPT_BURST];
	xnetstream* BurstServers[TEST_TCP_ACCEPT_BURST];
	xnetaddr Address;
	xfuture* pCancelled;
	xfuture* pCacheCancelled;
	xfuture* pAccept1;
	xfuture* pAccept3;
	xfuture* pPendingClose;
	xfuture* pLateClose;
	xfuture* pLateClose2;
	xfuture* pClient1Open;
	xfuture* pClient2Open;
	xfuture* pClient3Open;
	xfuture* pServer1Open;
	xfuture* pReceive;
	xfuture* BurstAccepts[TEST_TCP_ACCEPT_BURST];
	xdeadline iDeadline;
	uint64 iNodeHits;
	xnetbytes* pBytes;
	xbytesview View;

	memset(BurstClients, 0, sizeof(BurstClients));
	memset(BurstServers, 0, sizeof(BurstServers));
	memset(BurstAccepts, 0, sizeof(BurstAccepts));

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TCP_ACCEPT_BACKEND;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP accept Future engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TCP accept Future listener address failed");
	ListenConfig.AcceptQueueLimit = 1;
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		NULL,
		NULL,
		NULL
	);
	testRequire((pListener != NULL) &&
		 xrtNetListenerLocal(pListener, &Address),
		"TCP accept Future listener start failed");

	pCancelled = xrtNetListenerAcceptAsync(pListener);
	testTcpAcceptStats(pListener, 0, 0, 0, 1);
	testRequire((pCancelled != NULL) && xrtFutureCancel(pCancelled) &&
		 (xrtFutureWaitFor(pCancelled, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pCancelled) == XFUTURE_CANCELLED),
		"TCP accept Future cancellation failed");
	testTcpAcceptStats(pListener, 0, 0, 0, 0);
	testRequire(xrtNetEngineStats(pEngine, &EngineStats),
		"TCP accept Future node cache initial stats failed");
	iNodeHits = EngineStats.NodeCacheHits;
	pCacheCancelled = xrtNetListenerAcceptAsync(pListener);
	testRequire((pCacheCancelled != NULL) &&
		 xrtFutureCancel(pCacheCancelled) &&
		 (xrtFutureWaitFor(pCacheCancelled, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pCacheCancelled) == XFUTURE_CANCELLED) &&
		 xrtNetEngineStats(pEngine, &EngineStats) &&
		 (EngineStats.NodeCacheHits > iNodeHits) &&
		 (EngineStats.NodeCachedBytes <= EngineConfig.NodeCacheBytes),
		"TCP Listener Future waiter did not reuse the Worker node cache");
	testTcpAcceptStats(pListener, 0, 0, 0, 0);

	pAccept1 = xrtNetListenerAcceptAsync(pListener);
	testTcpAcceptStats(pListener, 0, 0, 0, 1);
	testRequire(xrtNetListenerAccept(pListener) == NULL,
		"TCP direct accept competed with a pending Future");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		 (xrtErrorCode(xrtGetError()) == XNET_ERROR_LISTENER_ACCEPT),
		"TCP concurrent accept consumer error mismatch");
	xrtClearError();
	pClient1 = xrtNetStreamConnect(
		pEngine,
		&Address,
		0,
		NULL,
		NULL,
		NULL
	);
	testRequire(pClient1 != NULL, "TCP accept Future first connect failed");
	pClient1Open = xrtNetStreamWaitAsync(
		pClient1,
		XNET_STREAM_WAIT_OPEN
	);
	pServer1 = testTcpAcceptValue(
		pAccept1,
		"TCP pending accept Future failed"
	);
	testTcpAcceptReady(pClient1Open,
		"TCP client open Future failed");
	pServer1Open = xrtNetStreamWaitAsync(
		pServer1,
		XNET_STREAM_WAIT_OPEN
	);
	testTcpAcceptReady(pServer1Open,
		"TCP accepted stream open Future failed");
	testTcpAcceptStats(pListener, 1, 0, 0, 0);

	pReceive = xrtNetStreamRecvAsync(pServer1, 0);
	testRequire((pReceive != NULL) &&
		 (xrtNetStreamSend(pClient1, "pull", 4) == XNET_RESULT_OK) &&
		 (xrtFutureWaitFor(pReceive, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pReceive) == XFUTURE_RESOLVED),
		"TCP accepted stream receive failed");
	pBytes = (xnetbytes*)xrtFutureValue(pReceive);
	View = xrtNetBytesView(pBytes);
	testRequire((pBytes != NULL) && (View.Size == 4) &&
		 (memcmp(View.Data, "pull", 4) == 0),
		"TCP accepted stream receive payload mismatch");

	pClient2 = xrtNetStreamConnect(
		pEngine,
		&Address,
		1,
		NULL,
		NULL,
		NULL
	);
	testRequire(pClient2 != NULL, "TCP queued accept connect failed");
	pClient2Open = xrtNetStreamWaitAsync(
		pClient2,
		XNET_STREAM_WAIT_OPEN
	);
	testTcpAcceptReady(pClient2Open,
		"TCP queued client open Future failed");
	testTcpAcceptStats(pListener, 2, 0, 1, 0);
	pServer2 = xrtNetListenerAccept(pListener);
	testRequire((pServer2 != NULL) &&
		 (xrtNetStreamState(pServer2) == XNET_STREAM_OPEN),
		"TCP direct pull after queue failed");
	testTcpAcceptStats(pListener, 2, 0, 0, 0);

	pClient3 = xrtNetStreamConnect(
		pEngine,
		&Address,
		0,
		NULL,
		NULL,
		NULL
	);
	testRequire(pClient3 != NULL, "TCP third connect failed");
	pClient3Open = xrtNetStreamWaitAsync(
		pClient3,
		XNET_STREAM_WAIT_OPEN
	);
	testTcpAcceptReady(pClient3Open,
		"TCP third client open Future failed");
	testTcpAcceptStats(pListener, 3, 0, 1, 0);
	pClient4 = xrtNetStreamConnect(
		pEngine,
		&Address,
		1,
		NULL,
		NULL,
		NULL
	);
	testRequire(pClient4 != NULL, "TCP overflow connect failed");
	testTcpAcceptStats(pListener, 3, 1, 1, 0);
	pAccept3 = xrtNetListenerAcceptAsync(pListener);
	pServer3 = testTcpAcceptValue(
		pAccept3,
		"TCP queued accept Future failed"
	);
	testTcpAcceptStats(pListener, 3, 1, 0, 0);
	for ( size_t i = 0; i < TEST_TCP_ACCEPT_BURST; i++ ) {
		BurstAccepts[i] = xrtNetListenerAcceptAsync(pListener);
		testRequire(BurstAccepts[i] != NULL,
			"TCP burst accept Future creation failed");
	}
	testTcpAcceptStats(
		pListener,
		3,
		1,
		0,
		TEST_TCP_ACCEPT_BURST
	);
	for ( size_t i = 0; i < TEST_TCP_ACCEPT_BURST; i++ ) {
		BurstClients[i] = xrtNetStreamConnect(
			pEngine,
			&Address,
			(uint64)(i & 1u),
			NULL,
			NULL,
			NULL
		);
		testRequire(BurstClients[i] != NULL,
			"TCP burst accept connect failed");
	}
	for ( size_t i = 0; i < TEST_TCP_ACCEPT_BURST; i++ ) {
		BurstServers[i] = testTcpAcceptValue(
			BurstAccepts[i],
			"TCP burst accept Future failed"
		);
	}
	testTcpAcceptStats(
		pListener,
		3 + TEST_TCP_ACCEPT_BURST,
		1,
		0,
		0
	);
	for ( size_t i = 0; i < TEST_TCP_ACCEPT_BURST; i++ ) {
		xrtFutureDestroy(BurstAccepts[i]);
		testTcpAcceptCloseStream(BurstClients[i]);
		testTcpAcceptCloseStream(BurstServers[i]);
	}

	pPendingClose = xrtNetListenerAcceptAsync(pListener);
	testTcpAcceptStats(
		pListener,
		3 + TEST_TCP_ACCEPT_BURST,
		1,
		0,
		1
	);
	testRequire(xrtNetListenerClose(pListener),
		"TCP accept Future listener close failed");
	testRequire((xrtFutureWaitFor(pPendingClose, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pPendingClose) == XFUTURE_CLOSED),
		"TCP pending accept did not close with listener");
	iDeadline = xrtDeadlineAfter(5000000u);
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"TCP accept Future listener close timed out");
		xrtSleep(1);
	}
	xrtFutureDestroy(pCancelled);
	xrtFutureDestroy(pCacheCancelled);
	xrtFutureDestroy(pAccept1);
	xrtFutureDestroy(pAccept3);
	xrtFutureDestroy(pPendingClose);
	xrtFutureDestroy(pClient1Open);
	xrtFutureDestroy(pClient2Open);
	xrtFutureDestroy(pClient3Open);
	xrtFutureDestroy(pServer1Open);
	xrtFutureDestroy(pReceive);
	testTcpAcceptCloseStream(pClient1);
	testTcpAcceptCloseStream(pClient2);
	testTcpAcceptCloseStream(pClient3);
	(void)xrtNetStreamAbort(pClient4);
	testTcpAcceptCloseStream(pClient4);
	testTcpAcceptCloseStream(pServer1);
	testTcpAcceptCloseStream(pServer2);
	testTcpAcceptCloseStream(pServer3);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP accept Future engine destroy failed");
	pLateClose = xrtNetListenerAcceptAsync(pListener);
	testRequire((pLateClose != NULL) &&
		 (xrtFutureWaitFor(pLateClose, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pLateClose) == XFUTURE_CLOSED),
		"TCP late accept Future used a destroyed Engine");
	pLateClose2 = xrtNetListenerAcceptAsync(pListener);
	testRequire((pLateClose2 != NULL) &&
		 (xrtFutureWaitFor(pLateClose2, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pLateClose2) == XFUTURE_CLOSED),
		"TCP repeated late accept Future used a destroyed Engine");
	xrtFutureDestroy(pLateClose);
	xrtFutureDestroy(pLateClose2);
	xrtNetListenerDestroy(pListener);
	printf("[PASS] network TCP accept Future %s\n",
		TEST_TCP_ACCEPT_BACKEND_NAME);
	return 0;
}
