#include "../test.h"



#if !defined(TEST_TCP_FUTURE_BACKEND)
	#define TEST_TCP_FUTURE_BACKEND XNET_PORT_SELECT
	#define TEST_TCP_FUTURE_BACKEND_NAME "select"
#endif



typedef struct testtcpfuture {
	xatomicptr Server;
	xatomic32 Accepted;
	xatomic32 Opened;
	xatomic32 Ended;
	xatomic32 Closed;
} testtcpfuture;



/* 在测试截止时间前等待原子值达到目标。 */
static void testTcpFutureWait(
	xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(iDeadline), sMessage);
		xrtSleep(1);
	}
}



/* 等待 Stream 累积指定数量的拉取字节。 */
static void testTcpFutureAvailable(
	xnetstream* pStream,
	size_t iExpected,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	while ( xrtNetStreamAvailable(pStream) != iExpected ) {
		testRequire(!xrtDeadlineExpired(iDeadline), sMessage);
		xrtSleep(1);
	}
}



/* 记录拉取模式 Stream 已经打开。 */
static void testTcpFutureOpen(xnetstream* pStream, ptr pData)
{
	testtcpfuture* pContext = (testtcpfuture*)pData;

	testRequire(xrtNetWorkerIsCurrent(xrtNetStreamWorker(pStream)),
		"TCP Future open worker mismatch");
	(void)xrtAtomic32FetchAdd(
		&pContext->Opened,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录拉取模式读方向结束。 */
static void testTcpFutureEnd(xnetstream* pStream, ptr pData)
{
	testtcpfuture* pContext = (testtcpfuture*)pData;

	(void)pStream;
	(void)xrtAtomic32FetchAdd(
		&pContext->Ended,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录正常 Stream 关闭。 */
static void testTcpFutureClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testtcpfuture* pContext = (testtcpfuture*)pData;

	(void)pStream;
	testRequire((Result == XNET_RESULT_OK) && (pError == NULL),
		"TCP Future normal close mismatch");
	(void)xrtAtomic32FetchAdd(
		&pContext->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 接管拉取模式服务端 Stream。 */
static bool testTcpFutureAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testtcpfuture* pContext = (testtcpfuture*)pData;

	(void)pListener;
	testRequire(xrtNetStreamSetData(pStream, pContext),
		"TCP Future accepted stream data failed");
	xrtAtomicPtrStore(&pContext->Server, pStream, XMEMORY_RELEASE);
	xrtAtomic32Store(&pContext->Accepted, 1, XMEMORY_RELEASE);
	return true;
}



/* 验证 Future 成功值是一份指定内容的借用字节视图。 */
static void testTcpFutureBytes(
	xfuture* pFuture,
	cstr sExpected,
	size_t iSize,
	cstr sMessage
)
{
	xnetbytes* pBytes;
	xbytesview View;

	testRequire(xrtFutureWaitFor(pFuture, 5000000u) == XWAIT_OK,
		sMessage);
	if ( xrtFutureState(pFuture) != XFUTURE_RESOLVED ) {
		const xerror* pError = xrtFutureError(pFuture);

		fprintf(
			stderr,
			"[TCP Future] %s: state=%d error=%s\n",
			sMessage,
			(int)xrtFutureState(pFuture),
			pError != NULL ? xrtErrorMessage(pError) : "none"
		);
	}
	testRequire(xrtFutureState(pFuture) == XFUTURE_RESOLVED,
		"TCP receive Future did not resolve");
	pBytes = (xnetbytes*)xrtFutureValue(pFuture);
	View = xrtNetBytesView(pBytes);
	testRequire((pBytes != NULL) && (View.Size == iSize) &&
		 (memcmp(View.Data, sExpected, iSize) == 0),
		"TCP receive Future payload mismatch");
}



/* 覆盖拉取接收、等待、取消、背压、半关闭和唯一终态。 */
int main(void)
{
	testtcpfuture Context;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetstreamconfig StreamConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents StreamEvents;
	xnetstreamstats Stats;
	xnetenginestats EngineStats;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetstream* pClient;
	xnetstream* pServer;
	xnetaddr Address;
	xfuture* pWrite;
	xfuture* pCacheOpen;
	xfuture* pReadable;
	xfuture* pMinimum;
	xfuture* pMinimumBytes;
	xfuture* pFirst;
	xfuture* pSecond;
	xfuture* pCancelled;
	xfuture* pDrain;
	xfuture* pWindow;
	xfuture* pMiddle;
	xfuture* pTail;
	xfuture* pEof;
	xfuture* pClientClose;
	xfuture* pServerClose;
	xfuture* pClosedClose;
	xfuture* pClosedRecv;
	uint64 iNodeHits;
	static const char sWindow[] = "12345678ABCDEFGH";

	memset(&Context, 0, sizeof(Context));
	xrtAtomicPtrInit(&Context.Server, NULL);
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&StreamEvents, 0, sizeof(StreamEvents));
	ListenerEvents.Accept = testTcpFutureAccept;
	StreamEvents.Open = testTcpFutureOpen;
	StreamEvents.End = testTcpFutureEnd;
	StreamEvents.Close = testTcpFutureClose;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TCP_FUTURE_BACKEND;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP Future engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TCP Future listener address failed");
	ListenConfig.Stream.ReadSize = 4;
	ListenConfig.Stream.ReadLimit = 8;
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		&StreamEvents,
		&Context
	);
	testRequire((pListener != NULL) &&
		 xrtNetListenerLocal(pListener, &Address),
		"TCP Future listener start failed");
	xrtNetStreamConfigInit(&StreamConfig);
	StreamConfig.ReadSize = 4;
	StreamConfig.ReadLimit = 8;
	pClient = xrtNetStreamConnect(
		pEngine,
		&Address,
		1,
		&StreamConfig,
		&StreamEvents,
		&Context
	);
	testRequire(pClient != NULL, "TCP Future connect failed");
	testTcpFutureWait(&Context.Accepted, 1,
		"TCP Future accept callback missing");
	testTcpFutureWait(&Context.Opened, 2,
		"TCP Future open callbacks missing");
	pServer = (xnetstream*)xrtAtomicPtrLoad(
		&Context.Server,
		XMEMORY_ACQUIRE
	);
	testRequire(pServer != NULL, "TCP Future accepted stream missing");

	pWrite = xrtNetStreamWaitAsync(pClient, XNET_STREAM_WAIT_WRITE);
	testRequire((pWrite != NULL) &&
		 (xrtFutureWaitFor(pWrite, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pWrite) == XFUTURE_RESOLVED),
		"TCP writable Future failed");
	testRequire(xrtNetEngineStats(pEngine, &EngineStats),
		"TCP Future node cache initial stats failed");
	iNodeHits = EngineStats.NodeCacheHits;
	pCacheOpen = xrtNetStreamWaitAsync(
		pClient,
		XNET_STREAM_WAIT_OPEN
	);
	testRequire((pCacheOpen != NULL) &&
		 (xrtFutureWaitFor(pCacheOpen, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pCacheOpen) == XFUTURE_RESOLVED) &&
		 xrtNetEngineStats(pEngine, &EngineStats) &&
		 (EngineStats.NodeCacheHits > iNodeHits) &&
		 (EngineStats.NodeCachedBytes <= EngineConfig.NodeCacheBytes),
		"TCP Stream Future waiter did not reuse the Worker node cache");
	pFirst = xrtNetStreamRecvAsync(pServer, 3);
	pReadable = xrtNetStreamWaitAsync(
		pServer,
		XNET_STREAM_WAIT_READ
	);
	testRequire((pFirst != NULL) && (pReadable != NULL),
		"TCP initial receive Futures failed");
	testRequire(xrtNetStreamSend(pClient, "hello", 5) ==
		XNET_RESULT_OK, "TCP Future initial send failed");
	testTcpFutureBytes(pFirst, "hel", 3,
		"TCP first receive Future timed out");
	testRequire((xrtFutureWaitFor(pReadable, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pReadable) == XFUTURE_RESOLVED),
		"TCP readable Future failed");
	testTcpFutureAvailable(pServer, 2,
		"TCP first receive did not preserve suffix");
	pSecond = xrtNetStreamRecvAsync(pServer, 0);
	testRequire(pSecond != NULL, "TCP second receive Future failed");
	testTcpFutureBytes(pSecond, "lo", 2,
		"TCP second receive Future timed out");
	pMinimum = xrtNetStreamWaitAvailableAsync(pServer, 4);
	testRequire(pMinimum != NULL,
		"TCP minimum readable Future create failed");
	testRequire(xrtNetStreamSend(pClient, "abc", 3) ==
		XNET_RESULT_OK, "TCP minimum readable prefix send failed");
	testRequire((xrtFutureWaitFor(pMinimum, 1000u) == XWAIT_TIMEOUT) &&
		(xrtFutureState(pMinimum) == XFUTURE_PENDING),
		"TCP minimum readable Future accepted an incomplete prefix");
	testRequire(xrtNetStreamSend(pClient, "d", 1) ==
		XNET_RESULT_OK, "TCP minimum readable suffix send failed");
	testRequire((xrtFutureWaitFor(pMinimum, 5000000u) == XWAIT_OK) &&
		(xrtFutureState(pMinimum) == XFUTURE_RESOLVED),
		"TCP minimum readable Future did not observe buffer growth");
	pMinimumBytes = xrtNetStreamRecvAsync(pServer, 4);
	testRequire(pMinimumBytes != NULL,
		"TCP minimum readable payload Future failed");
	testTcpFutureBytes(pMinimumBytes, "abcd", 4,
		"TCP minimum readable payload mismatch");
	testRequire(xrtNetStreamWaitAvailableAsync(pServer, 9) == NULL,
		"TCP minimum readable Future exceeded ReadLimit");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"TCP minimum readable range error mismatch");
	xrtClearError();

	pCancelled = xrtNetStreamRecvAsync(pServer, 1);
	testRequire((pCancelled != NULL) && xrtFutureCancel(pCancelled) &&
		 (xrtFutureWaitFor(pCancelled, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pCancelled) == XFUTURE_CANCELLED) &&
		 (xrtNetStreamState(pServer) == XNET_STREAM_OPEN),
		"TCP receive Future cancellation affected Stream");

	testRequire(xrtNetStreamSend(
		pClient,
		sWindow,
		sizeof(sWindow) - 1u
	) == XNET_RESULT_OK, "TCP Future window send failed");
	pDrain = xrtNetStreamWaitAsync(pClient, XNET_STREAM_WAIT_DRAIN);
	testRequire((pDrain != NULL) &&
		 (xrtFutureWaitFor(pDrain, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pDrain) == XFUTURE_RESOLVED),
		"TCP drain Future failed");
	testTcpFutureAvailable(pServer, 8,
		"TCP pull buffer did not stop at ReadLimit");
	testRequire(xrtNetStreamStats(pServer, &Stats) &&
		 Stats.ReadBlocked && (Stats.BufferedBytes == 8) &&
		 (Stats.ReceivedBytes == 17),
		"TCP pull buffer backpressure stats mismatch");
	pWindow = xrtNetStreamRecvAsync(pServer, 4);
	testRequire(pWindow != NULL, "TCP window receive Future failed");
	testTcpFutureBytes(pWindow, "1234", 4,
		"TCP window receive Future timed out");
	testTcpFutureAvailable(pServer, 8,
		"TCP pull buffer did not refill after consumption");
	pMiddle = xrtNetStreamRecvAsync(pServer, 0);
	testRequire(pMiddle != NULL, "TCP middle receive Future failed");
	testTcpFutureBytes(pMiddle, "5678ABCD", 8,
		"TCP middle receive Future timed out");
	testTcpFutureAvailable(pServer, 4,
		"TCP pull buffer tail did not arrive");
	pTail = xrtNetStreamRecvAsync(pServer, 0);
	testRequire(pTail != NULL, "TCP tail receive Future failed");
	testTcpFutureBytes(pTail, "EFGH", 4,
		"TCP tail receive Future timed out");

	pEof = xrtNetStreamRecvAsync(pServer, 0);
	testRequire((pEof != NULL) && xrtNetStreamShutdownWrite(pClient),
		"TCP EOF receive setup failed");
	testRequire((xrtFutureWaitFor(pEof, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pEof) == XFUTURE_CLOSED),
		"TCP EOF did not close pending receive Future");
	testTcpFutureWait(&Context.Ended, 1,
		"TCP Future peer FIN event missing");

	pClientClose = xrtNetStreamWaitAsync(
		pClient,
		XNET_STREAM_WAIT_CLOSE
	);
	pServerClose = xrtNetStreamWaitAsync(
		pServer,
		XNET_STREAM_WAIT_CLOSE
	);
	testRequire((pClientClose != NULL) && (pServerClose != NULL) &&
		 xrtNetStreamClose(pClient) && xrtNetStreamClose(pServer),
		"TCP close Future setup failed");
	testRequire((xrtFutureWaitFor(pClientClose, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pClientClose) == XFUTURE_RESOLVED) &&
		 (xrtFutureWaitFor(pServerClose, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pServerClose) == XFUTURE_RESOLVED),
		"TCP close Futures failed");
	testTcpFutureWait(&Context.Closed, 2,
		"TCP Future close callbacks missing");

	xrtFutureDestroy(pWrite);
	xrtFutureDestroy(pCacheOpen);
	xrtFutureDestroy(pReadable);
	xrtFutureDestroy(pMinimum);
	xrtFutureDestroy(pMinimumBytes);
	xrtFutureDestroy(pFirst);
	xrtFutureDestroy(pSecond);
	xrtFutureDestroy(pCancelled);
	xrtFutureDestroy(pDrain);
	xrtFutureDestroy(pWindow);
	xrtFutureDestroy(pMiddle);
	xrtFutureDestroy(pTail);
	xrtFutureDestroy(pEof);
	xrtFutureDestroy(pClientClose);
	xrtFutureDestroy(pServerClose);
	testRequire(xrtNetListenerClose(pListener),
		"TCP Future listener close failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtSleep(1);
	}
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP Future engine destroy failed");
	pClosedClose = xrtNetStreamWaitAsync(
		pServer,
		XNET_STREAM_WAIT_CLOSE
	);
	testRequire((pClosedClose != NULL) &&
		 (xrtFutureWaitFor(pClosedClose, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pClosedClose) == XFUTURE_RESOLVED),
		"TCP closed Stream close Future used a destroyed Engine");
	pClosedRecv = xrtNetStreamRecvAsync(pServer, 0);
	testRequire((pClosedRecv != NULL) &&
		 (xrtFutureWaitFor(pClosedRecv, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pClosedRecv) == XFUTURE_CLOSED),
		"TCP closed Stream receive Future used a destroyed Engine");
	xrtFutureDestroy(pClosedClose);
	xrtFutureDestroy(pClosedRecv);
	xrtNetStreamDestroy(pClient);
	xrtNetStreamDestroy(pServer);
	xrtNetListenerDestroy(pListener);
	printf("[PASS] network TCP Future %s\n",
		TEST_TCP_FUTURE_BACKEND_NAME);
	return 0;
}
