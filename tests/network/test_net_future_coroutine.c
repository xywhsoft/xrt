#include "../test.h"
#include "../test_thread.h"



#if !defined(TEST_NET_FUTURE_COROUTINE_BACKEND)
	#define TEST_NET_FUTURE_COROUTINE_BACKEND XNET_PORT_SELECT
	#define TEST_NET_FUTURE_COROUTINE_BACKEND_NAME "select"
#endif



typedef enum testnetfuturekind {
	TEST_NET_FUTURE_TCP = 0,
	TEST_NET_FUTURE_UDP
} testnetfuturekind;



/* 保存一次网络 Future 的协程等待终态和拥有型结果。 */
typedef struct testnetfutureawait {
	testnetfuturekind Kind;
	xnetstream* Stream;
	xnetudp* Udp;
	xwaitresult Wait;
	xfuturestate State;
	xnetbytes* Bytes;
	xnetudppacket* Packet;
	bool Entered;
	bool Returned;
	bool SourceCancelled;
} testnetfutureawait;



/* 保存跨线程生产端和本轮有效载荷。 */
typedef struct testnetfutureproducer {
	xnetstream* Stream;
	xnetudp* Udp;
	cbytes Data;
	size_t Size;
} testnetfutureproducer;



/* 保存同时取消 TCP 与 UDP 等待协程的目标。 */
typedef struct testnetfuturecancel {
	xcoro* Tcp;
	xcoro* Udp;
} testnetfuturecancel;



/* 关闭 Stream 并等待唯一关闭终态。 */
static void testNetFutureCoroutineStreamClose(xnetstream* pStream)
{
	if ( xrtNetStreamState(pStream) != XNET_STREAM_CLOSED ) {
		testRequire(xrtNetStreamClose(pStream) && xrtNetStreamWait(
			pStream,
			XNET_STREAM_WAIT_CLOSE,
			xrtDeadlineAfter(UINT64_C(5000000)),
			NULL
		), "network Future coroutine TCP close failed");
	}
	xrtNetStreamDestroy(pStream);
}



/* 关闭 UDP 并等待唯一关闭终态。 */
static void testNetFutureCoroutineUdpClose(xnetudp* pUdp)
{
	if ( xrtNetUdpState(pUdp) != XNET_UDP_CLOSED ) {
		testRequire(xrtNetUdpClose(pUdp) && xrtNetUdpWait(
			pUdp,
			XNET_UDP_WAIT_CLOSE,
			xrtDeadlineAfter(UINT64_C(5000000)),
			NULL
		), "network Future coroutine UDP close failed");
	}
	xrtNetUdpDestroy(pUdp);
}



/* 在调度协程中直接等待现有 TCP 或 UDP Future。 */
static ptr testNetFutureCoroutineAwait(ptr pData)
{
	testnetfutureawait* pContext = (testnetfutureawait*)pData;
	xfuture* pFuture;

	pFuture = pContext->Kind == TEST_NET_FUTURE_TCP ?
		xrtNetStreamRecvAsync(pContext->Stream, 0) :
		xrtNetUdpReceiveAsync(pContext->Udp);
	testRequire(pFuture != NULL,
		"network Future coroutine submission failed");
	pContext->Entered = true;
	pContext->Wait = xrtFutureAwaitFor(
		pFuture,
		UINT64_C(5000000)
	);
	if ( pContext->Wait == XWAIT_CANCELLED ) {
		pContext->SourceCancelled = xrtFutureCancel(pFuture);
	}
	pContext->State = xrtFutureState(pFuture);
	if ( (pContext->Wait == XWAIT_OK) &&
		 (pContext->State == XFUTURE_RESOLVED) ) {
		if ( pContext->Kind == TEST_NET_FUTURE_TCP ) {
			pContext->Bytes = xrtNetBytesRef(
				(xnetbytes*)xrtFutureValue(pFuture)
			);
		} else {
			pContext->Packet = xrtNetUdpPacketRef(
				(xnetudppacket*)xrtFutureValue(pFuture)
			);
		}
	}
	xrtFutureDestroy(pFuture);
	pContext->Returned = true;
	return pContext;
}



/* 从独立线程延迟发送 TCP 字节和 UDP 数据包。 */
static int testNetFutureCoroutineProduce(ptr pData)
{
	testnetfutureproducer* pContext = (testnetfutureproducer*)pData;

	xrtSleep(10);
	if ( xrtNetStreamSend(
		pContext->Stream,
		pContext->Data,
		pContext->Size
	) != XNET_RESULT_OK ) {
		return 1;
	}
	if ( xrtNetUdpSend(
		pContext->Udp,
		pContext->Data,
		pContext->Size
	) != XNET_RESULT_OK ) {
		return 2;
	}
	return 0;
}



/* 在两个目标均已挂起后发出协程取消。 */
static ptr testNetFutureCoroutineCancel(ptr pData)
{
	testnetfuturecancel* pContext = (testnetfuturecancel*)pData;

	testRequire(xrtCoCancel(pContext->Tcp),
		"network Future TCP coroutine cancel failed");
	testRequire(xrtCoCancel(pContext->Udp),
		"network Future UDP coroutine cancel failed");
	return pContext;
}



/* 初始化一个指定传输类型的等待上下文。 */
static void testNetFutureCoroutineAwaitInit(
	testnetfutureawait* pContext,
	testnetfuturekind Kind,
	xnetstream* pStream,
	xnetudp* pUdp
)
{
	memset(pContext, 0, sizeof(*pContext));
	pContext->Kind = Kind;
	pContext->Stream = pStream;
	pContext->Udp = pUdp;
}



/* 验证一次 TCP 与 UDP 跨线程唤醒、结果引用和有效载荷。 */
static void testNetFutureCoroutineSuccess(
	xnetstream* pSendStream,
	xnetstream* pRecvStream,
	xnetudp* pSendUdp,
	xnetudp* pRecvUdp,
	cbytes pData,
	size_t iSize
)
{
	testnetfutureawait Tcp;
	testnetfutureawait Udp;
	testnetfutureproducer Producer;
	testthread Thread;
	xcosched* pSched;
	xcoro* pTcp;
	xcoro* pUdp;
	xbytesview View;

	testNetFutureCoroutineAwaitInit(
		&Tcp,
		TEST_NET_FUTURE_TCP,
		pRecvStream,
		NULL
	);
	testNetFutureCoroutineAwaitInit(
		&Udp,
		TEST_NET_FUTURE_UDP,
		NULL,
		pRecvUdp
	);
	Producer.Stream = pSendStream;
	Producer.Udp = pSendUdp;
	Producer.Data = pData;
	Producer.Size = iSize;
	pSched = xrtCoSchedCreate();
	testRequire(pSched != NULL,
		"network Future coroutine scheduler create failed");
	pTcp = xrtCoSpawn(pSched, testNetFutureCoroutineAwait, &Tcp, NULL);
	pUdp = xrtCoSpawn(pSched, testNetFutureCoroutineAwait, &Udp, NULL);
	testRequire((pTcp != NULL) && (pUdp != NULL),
		"network Future coroutine spawn failed");
	testRequire((xrtCoSchedStep(pSched) == XWAIT_OK) &&
		 (xrtCoSchedStep(pSched) == XWAIT_OK) &&
		 Tcp.Entered && Udp.Entered &&
		 (xrtCoState(pTcp) == XCORO_SUSPENDED) &&
		 (xrtCoState(pUdp) == XCORO_SUSPENDED),
		"network Futures did not suspend both coroutines");
	Thread.Proc = testNetFutureCoroutineProduce;
	Thread.Data = &Producer;
	testThreadsStart(&Thread, 1);
	testRequire(xrtCoSchedRun(pSched),
		"network Future coroutine scheduler run failed");
	testThreadsJoin(&Thread, 1);
	testRequire(Thread.Result == 0,
		"network Future coroutine producer failed");
	testRequire(Tcp.Returned && Udp.Returned &&
		 (Tcp.Wait == XWAIT_OK) && (Udp.Wait == XWAIT_OK) &&
		 (Tcp.State == XFUTURE_RESOLVED) &&
		 (Udp.State == XFUTURE_RESOLVED) &&
		 (Tcp.Bytes != NULL) && (Udp.Packet != NULL),
		"network Future coroutine terminal mismatch");
	View = xrtNetBytesView(Tcp.Bytes);
	testRequire((View.Size == iSize) &&
		 (memcmp(View.Data, pData, iSize) == 0) &&
		 (xrtNetUdpPacketSize(Udp.Packet) == iSize) &&
		 (memcmp(xrtNetUdpPacketData(Udp.Packet), pData, iSize) == 0),
		"network Future coroutine payload mismatch");
	xrtNetBytesDestroy(Tcp.Bytes);
	xrtNetUdpPacketDestroy(Udp.Packet);
	testRequire(xrtCoDestroy(pTcp) && xrtCoDestroy(pUdp),
		"network Future coroutine destroy failed");
	testRequire(xrtCoSchedDestroy(pSched),
		"network Future coroutine scheduler destroy failed");
}



/* 验证协程取消会显式撤销两个独占网络 Future。 */
static void testNetFutureCoroutineCancellation(
	xnetstream* pStream,
	xnetudp* pUdp
)
{
	testnetfutureawait Tcp;
	testnetfutureawait Udp;
	testnetfuturecancel Cancel;
	xcosched* pSched;
	xcoro* pTcp;
	xcoro* pUdpCoroutine;

	testNetFutureCoroutineAwaitInit(
		&Tcp,
		TEST_NET_FUTURE_TCP,
		pStream,
		NULL
	);
	testNetFutureCoroutineAwaitInit(
		&Udp,
		TEST_NET_FUTURE_UDP,
		NULL,
		pUdp
	);
	pSched = xrtCoSchedCreate();
	testRequire(pSched != NULL,
		"network Future cancel scheduler create failed");
	pTcp = xrtCoSpawn(pSched, testNetFutureCoroutineAwait, &Tcp, NULL);
	pUdpCoroutine = xrtCoSpawn(
		pSched,
		testNetFutureCoroutineAwait,
		&Udp,
		NULL
	);
	testRequire((pTcp != NULL) && (pUdpCoroutine != NULL),
		"network Future cancel coroutine spawn failed");
	testRequire((xrtCoSchedStep(pSched) == XWAIT_OK) &&
		 (xrtCoSchedStep(pSched) == XWAIT_OK) &&
		 (xrtCoState(pTcp) == XCORO_SUSPENDED) &&
		 (xrtCoState(pUdpCoroutine) == XCORO_SUSPENDED),
		"network Future cancel targets did not suspend");
	Cancel.Tcp = pTcp;
	Cancel.Udp = pUdpCoroutine;
	testRequire(xrtCoGo(
		pSched,
		testNetFutureCoroutineCancel,
		&Cancel,
		NULL
	), "network Future cancel helper spawn failed");
	testRequire(xrtCoSchedRun(pSched),
		"network Future cancel scheduler run failed");
	testRequire(Tcp.Returned && Udp.Returned &&
		 (Tcp.Wait == XWAIT_CANCELLED) &&
		 (Udp.Wait == XWAIT_CANCELLED) &&
		 Tcp.SourceCancelled && Udp.SourceCancelled &&
		 (Tcp.State == XFUTURE_CANCELLED) &&
		 (Udp.State == XFUTURE_CANCELLED) &&
		 (xrtCoTerm(pTcp) == XCORO_TERM_RETURNED) &&
		 (xrtCoTerm(pUdpCoroutine) == XCORO_TERM_RETURNED),
		"network Future cancellation terminal mismatch");
	testRequire(xrtCoDestroy(pTcp) && xrtCoDestroy(pUdpCoroutine),
		"network Future cancel coroutine destroy failed");
	testRequire(xrtCoSchedDestroy(pSched),
		"network Future cancel scheduler destroy failed");
}



/* 验证网络 Future 可直接进入通用协程调度器且取消后无残留等待。 */
int main(void)
{
	static const uint8 First[] = "future-coroutine";
	static const uint8 Recovery[] = "after-cancel";
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetudpconfig UdpConfig;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetstream* pTcpClient;
	xnetstream* pTcpServer;
	xnetudp* pUdpClient;
	xnetudp* pUdpServer;
	xnetaddr Address;
	xdeadline iDeadline;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_NET_FUTURE_COROUTINE_BACKEND;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"network Future coroutine Engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "network Future coroutine listener address failed");
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire((pListener != NULL) &&
		xrtNetListenerLocal(pListener, &Address),
		"network Future coroutine listener create failed");
	pTcpClient = xrtNetStreamConnect(
		pEngine,
		&Address,
		1,
		NULL,
		NULL,
		NULL
	);
	pTcpServer = xrtNetListenerAcceptWait(
		pListener,
		xrtDeadlineAfter(UINT64_C(5000000)),
		NULL
	);
	testRequire((pTcpClient != NULL) && (pTcpServer != NULL) &&
		xrtNetStreamWait(
			pTcpClient,
			XNET_STREAM_WAIT_OPEN,
			xrtDeadlineAfter(UINT64_C(5000000)),
			NULL
		), "network Future coroutine TCP setup failed");

	xrtNetUdpConfigInit(&UdpConfig);
	testRequire(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0),
		"network Future coroutine UDP address failed");
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
		"network Future coroutine UDP bind failed");
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
		xrtDeadlineAfter(UINT64_C(5000000)),
		NULL
	) && xrtNetUdpWait(
		pUdpClient,
		XNET_UDP_WAIT_OPEN,
		xrtDeadlineAfter(UINT64_C(5000000)),
		NULL
	), "network Future coroutine UDP setup failed");

	testNetFutureCoroutineSuccess(
		pTcpClient,
		pTcpServer,
		pUdpClient,
		pUdpServer,
		First,
		sizeof(First) - 1u
	);
	testNetFutureCoroutineCancellation(pTcpServer, pUdpServer);
	testNetFutureCoroutineSuccess(
		pTcpClient,
		pTcpServer,
		pUdpClient,
		pUdpServer,
		Recovery,
		sizeof(Recovery) - 1u
	);

	testNetFutureCoroutineStreamClose(pTcpClient);
	testNetFutureCoroutineStreamClose(pTcpServer);
	testNetFutureCoroutineUdpClose(pUdpClient);
	testNetFutureCoroutineUdpClose(pUdpServer);
	testRequire(xrtNetListenerClose(pListener),
		"network Future coroutine listener close failed");
	iDeadline = xrtDeadlineAfter(UINT64_C(5000000));
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"network Future coroutine listener close timed out");
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(pEngine),
		"network Future coroutine retained an Engine object");
	printf("[PASS] network Future coroutine contract (%s)\n",
		TEST_NET_FUTURE_COROUTINE_BACKEND_NAME);
	return 0;
}
