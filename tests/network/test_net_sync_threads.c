#include "../test.h"



#if !defined(TEST_NET_SYNC_THREADS_BACKEND)
	#define TEST_NET_SYNC_THREADS_BACKEND XNET_PORT_SELECT
	#define TEST_NET_SYNC_THREADS_BACKEND_NAME "select"
#endif



#define TEST_NET_SYNC_THREAD_COUNT 16u



typedef struct testnetsyncthreads {
	xnetstream* Stream;
	xnetudp* Udp;
	xatomic32 Ready;
	xatomic32 Go;
	bool Datagram;
} testnetsyncthreads;



typedef struct testnetsyncwaiter {
	testnetsyncthreads* Context;
	xcancel* Cancel;
	xerrkind ErrorKind;
	int32 ErrorCode;
} testnetsyncwaiter;



/* 等待所有原生线程进入阻塞调用前的同步点。 */
static void testNetSyncThreadsReady(testnetsyncthreads* pContext)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	while ( xrtAtomic32Load(
		&pContext->Ready,
		XMEMORY_ACQUIRE
	) < TEST_NET_SYNC_THREAD_COUNT ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"network sync wait threads did not start");
		xrtThreadYield();
	}
}



/* 阻塞接收只允许由外部取消或对象关闭终结。 */
static int32 testNetSyncThreadsWait(ptr pData)
{
	testnetsyncwaiter* pWaiter = (testnetsyncwaiter*)pData;
	testnetsyncthreads* pContext = pWaiter->Context;
	bool bUnexpected;

	(void)xrtAtomic32FetchAdd(
		&pContext->Ready,
		1,
		XMEMORY_RELEASE
	);
	while ( xrtAtomic32Load(&pContext->Go, XMEMORY_ACQUIRE) == 0 ) {
		xrtThreadYield();
	}
	if ( pContext->Datagram ) {
		xnetudppacket* pPacket = xrtNetUdpReceiveWait(
			pContext->Udp,
			xrtDeadlineAfter(10000000u),
			pWaiter->Cancel
		);

		bUnexpected = pPacket != NULL;
		xrtNetUdpPacketDestroy(pPacket);
	} else {
		xnetbytes* pBytes = xrtNetStreamRecv(
			pContext->Stream,
			1,
			xrtDeadlineAfter(10000000u),
			pWaiter->Cancel
		);

		bUnexpected = pBytes != NULL;
		xrtNetBytesDestroy(pBytes);
	}
	pWaiter->ErrorKind = xrtErrorKind(xrtGetError());
	pWaiter->ErrorCode = xrtErrorCode(xrtGetError());
	xrtClearError();
	return bUnexpected ? 1 : 0;
}



/* 启动一组阻塞等待者，并让取消与正常关闭形成确定的终态竞争。 */
static void testNetSyncThreadsRun(
	testnetsyncthreads* pContext,
	int32 iErrorCode
)
{
	testnetsyncwaiter Waiters[TEST_NET_SYNC_THREAD_COUNT];
	xthread* Threads[TEST_NET_SYNC_THREAD_COUNT];

	memset(Waiters, 0, sizeof(Waiters));
	memset(Threads, 0, sizeof(Threads));
	xrtAtomic32Init(&pContext->Ready, 0);
	xrtAtomic32Init(&pContext->Go, 0);
	for ( size_t i = 0; i < TEST_NET_SYNC_THREAD_COUNT; i++ ) {
		Waiters[i].Context = pContext;
		Waiters[i].Cancel = xrtCancelCreate();
		Threads[i] = xrtThreadCreate(
			testNetSyncThreadsWait,
			&Waiters[i],
			0
		);
		testRequire((Waiters[i].Cancel != NULL) && (Threads[i] != NULL),
			"network sync wait thread create failed");
	}
	testNetSyncThreadsReady(pContext);
	xrtAtomic32Store(&pContext->Go, 1, XMEMORY_RELEASE);
	xrtSleep(20);
	for ( size_t i = 0; i < TEST_NET_SYNC_THREAD_COUNT; i += 2u ) {
		testRequire(xrtCancelRequest(Waiters[i].Cancel),
			"network sync external cancellation failed");
	}
	if ( pContext->Datagram ) {
		testRequire(xrtNetUdpClose(pContext->Udp),
			"network sync UDP close race failed");
	} else {
		testRequire(xrtNetStreamClose(pContext->Stream),
			"network sync TCP close race failed");
	}
	for ( size_t i = 0; i < TEST_NET_SYNC_THREAD_COUNT; i++ ) {
		xerrkind ExpectedKind = ((i & 1u) == 0) ?
			XERR_CANCELLED : XERR_CLOSED;

		testRequire((xrtThreadWaitFor(
			Threads[i],
			10000000u
		) == XWAIT_OK) && (xrtThreadExitCode(Threads[i]) == 0),
			"network sync wait thread did not finish");
		if ( Waiters[i].ErrorKind != ExpectedKind ) {
			fprintf(
				stderr,
				"[DETAIL] backend=%s transport=%s waiter=%zu "
				"actual-kind=%d expected-kind=%d code=%d\n",
				TEST_NET_SYNC_THREADS_BACKEND_NAME,
				pContext->Datagram ? "UDP" : "TCP",
				i,
				(int)Waiters[i].ErrorKind,
				(int)ExpectedKind,
				(int)Waiters[i].ErrorCode
			);
		}
		testRequire(
			Waiters[i].ErrorKind == ExpectedKind,
			"network sync terminal reason mismatch"
		);
		testRequire(Waiters[i].ErrorCode == iErrorCode,
			"network sync terminal error code mismatch");
		xrtThreadDestroy(Threads[i]);
		xrtCancelDestroy(Waiters[i].Cancel);
	}
}



/* 验证阻塞 TCP/UDP 调用的跨线程取消、关闭和生命周期收敛。 */
int main(void)
{
	testnetsyncthreads TcpContext;
	testnetsyncthreads UdpContext;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetudpconfig UdpConfig;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetstream* pClient;
	xnetstream* pServer;
	xnetudp* pUdpClient;
	xnetudp* pUdpServer;
	xnetaddr Address;
	xnetenginestats Stats;
	xdeadline iDeadline;
	bool bDestroyed;

	memset(&TcpContext, 0, sizeof(TcpContext));
	memset(&UdpContext, 0, sizeof(UdpContext));
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_NET_SYNC_THREADS_BACKEND;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"network sync thread engine start failed");

	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "network sync thread listener address failed");
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire((pListener != NULL) &&
		xrtNetListenerLocal(pListener, &Address),
		"network sync thread listener create failed");
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
	), "network sync thread TCP setup failed");
	TcpContext.Stream = pServer;
	testNetSyncThreadsRun(&TcpContext, XNET_ERROR_STREAM_READ);
	testRequire(xrtNetStreamWait(
		pServer,
		XNET_STREAM_WAIT_CLOSE,
		xrtDeadlineAfter(5000000u),
		NULL
	), "network sync thread TCP server close wait failed");
	testRequire(xrtNetStreamClose(pClient) && xrtNetStreamWait(
		pClient,
		XNET_STREAM_WAIT_CLOSE,
		xrtDeadlineAfter(5000000u),
		NULL
	), "network sync thread TCP client close failed");
	xrtNetStreamDestroy(pClient);
	xrtNetStreamDestroy(pServer);
	testRequire(xrtNetListenerClose(pListener),
		"network sync thread listener close failed");
	iDeadline = xrtDeadlineAfter(5000000u);
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"network sync thread listener close timed out");
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);

	xrtNetUdpConfigInit(&UdpConfig);
	testRequire(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0),
		"network sync thread UDP address failed");
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
		"network sync thread UDP server failed");
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
	), "network sync thread UDP setup failed");
	UdpContext.Udp = pUdpServer;
	UdpContext.Datagram = true;
	testNetSyncThreadsRun(&UdpContext, XNET_ERROR_UDP_RECEIVE);
	testRequire(xrtNetUdpWait(
		pUdpServer,
		XNET_UDP_WAIT_CLOSE,
		xrtDeadlineAfter(5000000u),
		NULL
	), "network sync thread UDP server close wait failed");
	testRequire(xrtNetUdpClose(pUdpClient) && xrtNetUdpWait(
		pUdpClient,
		XNET_UDP_WAIT_CLOSE,
		xrtDeadlineAfter(5000000u),
		NULL
	), "network sync thread UDP client close failed");
	xrtNetUdpDestroy(pUdpClient);
	xrtNetUdpDestroy(pUdpServer);
	bDestroyed = xrtNetEngineDestroy(pEngine);
	if ( !bDestroyed && xrtNetEngineStats(pEngine, &Stats) ) {
		fprintf(
			stderr,
			"[DETAIL] backend=%s live-objects=%zu pending-commands=%zu "
			"posts-accepted=%llu posts-executed=%llu\n",
			TEST_NET_SYNC_THREADS_BACKEND_NAME,
			Stats.LiveObjects,
			Stats.PendingCommands,
			(unsigned long long)Stats.PostsAccepted,
			(unsigned long long)Stats.PostsExecuted
		);
	}
	testRequire(bDestroyed, "network sync thread engine retained an object");
	printf("[PASS] network sync %s cancel-close threads\n",
		TEST_NET_SYNC_THREADS_BACKEND_NAME);
	return 0;
}
