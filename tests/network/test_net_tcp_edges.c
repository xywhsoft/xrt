#include "../test.h"



#if !defined(TEST_TCP_BACKEND)
	#define TEST_TCP_BACKEND XNET_PORT_SELECT
	#define TEST_TCP_BACKEND_NAME "select"
#endif



#define TEST_TCP_LATE_PEERS 4u



typedef struct testtcpreject {
	xatomicptr Stream;
	xatomic32 Accepts;
	xatomic32 Blocked;
	xatomic32 Release;
	xatomic32 Returned;
	xatomic32 Released;
	xatomic32 ReleaseMatched;
	xatomic32 Opens;
	xatomic32 StreamCloses;
	xatomic32 ListenerErrors;
	xatomic32 ListenerClose;
	char Byte;
} testtcpreject;



typedef struct testtcplimit {
	xatomicptr Stream;
	xatomic32 Accepted;
	xatomic32 Reads;
	xatomic32 Consumed;
	xatomic32 Closed;
	xatomic32 CloseMatched;
} testtcplimit;



typedef struct testtcpwritereentrant {
	xatomicptr Stream;
	xatomic32 Accepted;
	xatomic32 Open;
	xatomic32 HighWater;
	xatomic32 LowWater;
	xatomic32 Drain;
	xatomic32 Closed;
	bool AbortHighWater;
} testtcpwritereentrant;



typedef struct testtcpstartclose {
	xatomic32 Blocked;
	xatomic32 Release;
	xatomic32 Noops;
	xatomic32 Open;
	xatomic32 Closed;
	xatomic32 CloseMatched;
} testtcpstartclose;



typedef struct testtcplatelisten {
	xatomic32 Blocked;
	xatomic32 Release;
	xatomic32 Accepts;
	xatomic32 Errors;
	xatomic32 Closes;
} testtcplatelisten;



/* 在测试截止时间前等待原子计数到达下限。 */
static void testTcpEdgeWait(
	xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(10000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(iDeadline), sMessage);
		xrtSleep(1);
	}
}



/* 在测试截止时间前等待 Stream 进入关闭终态。 */
static void testTcpEdgeWaitClosed(xnetstream* pStream, cstr sMessage)
{
	xdeadline Deadline = xrtDeadlineAfter(5000000u);

	while ( xrtNetStreamState(pStream) != XNET_STREAM_CLOSED ) {
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}



/* 阻塞 Listener Worker，让 Accept 完成与关闭请求稳定竞争。 */
static void testTcpLateListenBlock(xnetworker* pWorker, ptr pData)
{
	testtcplatelisten* pContext = (testtcplatelisten*)pData;

	(void)pWorker;
	xrtAtomic32Store(&pContext->Blocked, 1, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(
		&pContext->Release,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
}



/* 关闭入口建立后，不允许迟到的完成包公开连接。 */
static bool testTcpLateListenAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testtcplatelisten* pContext = (testtcplatelisten*)pData;

	(void)pListener;
	(void)pStream;
	(void)xrtAtomic32FetchAdd(
		&pContext->Accepts,
		1,
		XMEMORY_RELEASE
	);
	return false;
}



/* 关闭后的 Accept 终态不应再泄漏 Listener Error。 */
static void testTcpLateListenError(
	xnetlistener* pListener,
	const xerror* pError,
	ptr pData
)
{
	testtcplatelisten* pContext = (testtcplatelisten*)pData;

	(void)pListener;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pContext->Errors,
		1,
		XMEMORY_RELEASE
	);
}



/* Listener 的迟到完成竞争仍只能发布一次 Close。 */
static void testTcpLateListenClose(xnetlistener* pListener, ptr pData)
{
	testtcplatelisten* pContext = (testtcplatelisten*)pData;

	testRequire(xrtNetListenerState(pListener) == XNET_LISTENER_CLOSED,
		"TCP late-listener Close observed a non-terminal state");
	(void)xrtAtomic32FetchAdd(
		&pContext->Closes,
		1,
		XMEMORY_RELEASE
	);
}



/* 阻塞 Worker，让启动任务稳定跨过单轮命令预算。 */
static void testTcpStartCloseBlock(xnetworker* pWorker, ptr pData)
{
	testtcpstartclose* pContext = (testtcpstartclose*)pData;

	(void)pWorker;
	xrtAtomic32Store(&pContext->Blocked, 1, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(
		&pContext->Release,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
}



/* 统计启动任务前排队的普通命令。 */
static void testTcpStartCloseNoop(xnetworker* pWorker, ptr pData)
{
	testtcpstartclose* pContext = (testtcpstartclose*)pData;

	(void)pWorker;
	(void)xrtAtomic32FetchAdd(
		&pContext->Noops,
		1,
		XMEMORY_RELEASE
	);
}



/* 被启动前关闭的 Stream 不能发布 Open。 */
static void testTcpStartCloseOpen(xnetstream* pStream, ptr pData)
{
	testtcpstartclose* pContext = (testtcpstartclose*)pData;

	(void)pStream;
	(void)xrtAtomic32FetchAdd(
		&pContext->Open,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证迟到启动任务只解除占用，不会重新启动已经关闭的 Stream。 */
static void testTcpStartCloseClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testtcpstartclose* pContext = (testtcpstartclose*)pData;

	if ( (xrtNetStreamState(pStream) == XNET_STREAM_CLOSED) &&
		 (Result == XNET_RESULT_CANCELLED) && (pError == NULL) &&
		 (xrtNetStreamError(pStream) == NULL) ) {
		xrtAtomic32Store(
			&pContext->CloseMatched,
			1,
			XMEMORY_RELEASE
		);
	}
	xrtAtomic32Store(&pContext->Closed, 1, XMEMORY_RELEASE);
}



/* 循环完成一个小型阻塞发送。 */
static void testTcpEdgeSend(
	xnetsocket Socket,
	const void* pData,
	size_t iSize
)
{
	const uint8* pBytes = (const uint8*)pData;
	size_t iOffset = 0;

	while ( iOffset < iSize ) {
		size_t iSent = 0;

		testRequire(xrtNetSocketSend(
			Socket,
			pBytes + iOffset,
			iSize - iOffset,
			&iSent
		) == XNET_RESULT_OK, "TCP edge raw send failed");
		testRequire(iSent != 0, "TCP edge raw send made no progress");
		iOffset += iSent;
	}
}



/* 循环收满一个固定长度的小响应。 */
static void testTcpEdgeRecv(
	xnetsocket Socket,
	void* pData,
	size_t iSize
)
{
	bytes pWrite = (bytes)pData;
	size_t iOffset = 0;

	while ( iOffset < iSize ) {
		size_t iReceived = 0;

		testRequire(xrtNetSocketRecv(
			Socket,
			pWrite + iOffset,
			iSize - iOffset,
			&iReceived
		) == XNET_RESULT_OK, "TCP edge raw receive failed");
		testRequire(iReceived != 0,
			"TCP edge raw receive made no progress");
		iOffset += iReceived;
	}
}



/* 接管水位重入测试连接。 */
static bool testTcpWriteAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testtcpwritereentrant* pContext =
		(testtcpwritereentrant*)pData;

	(void)pListener;
	testRequire(xrtNetStreamSetData(pStream, pContext),
		"TCP reentrant stream data setup failed");
	xrtAtomicPtrStore(&pContext->Stream, pStream, XMEMORY_RELEASE);
	xrtAtomic32Store(&pContext->Accepted, 1, XMEMORY_RELEASE);
	return true;
}



/* 记录水位重入测试 Stream 已经公开。 */
static void testTcpWriteOpen(xnetstream* pStream, ptr pData)
{
	testtcpwritereentrant* pContext =
		(testtcpwritereentrant*)pData;

	(void)pStream;
	xrtAtomic32Store(&pContext->Open, 1, XMEMORY_RELEASE);
}



/* 每个一字节发送周期都必须形成一次高水位转换。 */
static void testTcpWriteHighWater(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	testtcpwritereentrant* pContext =
		(testtcpwritereentrant*)pData;

	(void)pStream;
	testRequire(iQueued == 1,
		"TCP reentrant high-water value mismatch");
	(void)xrtAtomic32FetchAdd(
		&pContext->HighWater,
		1,
		XMEMORY_RELEASE
	);
	if ( pContext->AbortHighWater ) {
		testRequire(xrtNetStreamAbort(pStream),
			"TCP high-water reentrant abort failed");
	}
}



/* 第一次低水位回调内立即开始第二个发送周期。 */
static void testTcpWriteLowWater(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	testtcpwritereentrant* pContext =
		(testtcpwritereentrant*)pData;
	uint32 iPrevious;

	testRequire(iQueued == 0,
		"TCP reentrant low-water value mismatch");
	iPrevious = xrtAtomic32FetchAdd(
		&pContext->LowWater,
		1,
		XMEMORY_ACQ_REL
	);
	if ( iPrevious == 0 ) {
		testRequire(xrtNetStreamSend(
			pStream,
			"B",
			1
		) == XNET_RESULT_OK,
			"TCP reentrant low-water send failed");
	}
}



/* 排空事件应在重入水位回调结束后合并发布一次。 */
static void testTcpWriteDrain(xnetstream* pStream, ptr pData)
{
	testtcpwritereentrant* pContext =
		(testtcpwritereentrant*)pData;

	(void)pStream;
	(void)xrtAtomic32FetchAdd(
		&pContext->Drain,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录水位重入测试 Stream 的唯一关闭。 */
static void testTcpWriteClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testtcpwritereentrant* pContext =
		(testtcpwritereentrant*)pData;

	(void)pStream;
	testRequire((Result == XNET_RESULT_CANCELLED) && (pError == NULL),
		"TCP reentrant stream abort upgrade mismatch");
	xrtAtomic32Store(&pContext->Closed, 1, XMEMORY_RELEASE);
}



/* 在同一 Worker 任务内验证 Abort 能升级尚未执行的普通 Close。 */
static void testTcpWriteAbortUpgrade(
	xnetworker* pWorker,
	ptr pData
)
{
	testtcpwritereentrant* pContext =
		(testtcpwritereentrant*)pData;
	xnetstream* pStream = (xnetstream*)xrtAtomicPtrLoad(
		&pContext->Stream,
		XMEMORY_ACQUIRE
	);

	(void)pWorker;
	testRequire((pStream != NULL) && xrtNetStreamClose(pStream),
		"TCP reentrant close request failed");
	testRequire(!xrtNetStreamResume(pStream),
		"TCP resume crossed an established close gate");
	xrtClearError();
	testRequire(xrtNetStreamAbort(pStream),
		"TCP reentrant abort upgrade request failed");
}



/* 前一个连接立即拒绝，后一个连接阻塞在目标 Worker 上。 */
static bool testTcpRejectAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testtcpreject* pContext = (testtcpreject*)pData;
	uint32 iAccept = xrtAtomic32FetchAdd(
		&pContext->Accepts,
		1,
		XMEMORY_ACQ_REL
	) + 1u;

	(void)pListener;
	testRequire(xrtNetWorkerIsCurrent(xrtNetStreamWorker(pStream)),
		"TCP reject callback worker mismatch");
	testRequire(xrtNetStreamSocket(pStream) != NULL,
		"TCP reject callback socket missing");
	if ( iAccept == 2u ) {
		testRequire(xrtNetStreamRef(pStream) == pStream,
			"TCP rejected stream retain failed");
		xrtAtomicPtrStore(&pContext->Stream, pStream, XMEMORY_RELEASE);
		xrtAtomic32Store(&pContext->Blocked, 1, XMEMORY_RELEASE);
		while ( xrtAtomic32Load(
			&pContext->Release,
			XMEMORY_ACQUIRE
		) == 0 ) {
			xrtThreadYield();
		}
		xrtAtomic32Store(&pContext->Returned, 1, XMEMORY_RELEASE);
	}
	return false;
}



/* 拒绝终态必须等待已经受理的跨线程发送归还所有权。 */
static void testTcpRejectRelease(
	ptr pData,
	cbytes pBytes,
	size_t iSize
)
{
	testtcpreject* pContext = (testtcpreject*)pData;
	xnetstream* pStream = (xnetstream*)xrtAtomicPtrLoad(
		&pContext->Stream,
		XMEMORY_ACQUIRE
	);

	if ( (pBytes == (cbytes)&pContext->Byte) && (iSize == 1) &&
		 (pStream != NULL) &&
		 (xrtNetStreamState(pStream) == XNET_STREAM_CLOSING) ) {
		xrtAtomic32Store(
			&pContext->ReleaseMatched,
			1,
			XMEMORY_RELEASE
		);
	}
	xrtAtomic32Store(&pContext->Released, 1, XMEMORY_RELEASE);
}



/* 被拒绝的 Stream 不能泄漏 Open。 */
static void testTcpRejectOpen(xnetstream* pStream, ptr pData)
{
	testtcpreject* pContext = (testtcpreject*)pData;

	(void)pStream;
	(void)xrtAtomic32FetchAdd(&pContext->Opens, 1, XMEMORY_RELEASE);
}



/* 被拒绝的 Stream 不能泄漏 Close。 */
static void testTcpRejectStreamClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testtcpreject* pContext = (testtcpreject*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pContext->StreamCloses,
		1,
		XMEMORY_RELEASE
	);
}



/* Listener Error 必须留在 Listener Worker。 */
static void testTcpRejectError(
	xnetlistener* pListener,
	const xerror* pError,
	ptr pData
)
{
	testtcpreject* pContext = (testtcpreject*)pData;

	testRequire((pError != NULL) &&
		 xrtNetWorkerIsCurrent(xrtNetListenerWorker(pListener)),
		"TCP reject listener error worker mismatch");
	(void)xrtAtomic32FetchAdd(
		&pContext->ListenerErrors,
		1,
		XMEMORY_RELEASE
	);
}



/* Listener Close 必须等待跨 Worker Accept 返回。 */
static void testTcpRejectListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	testtcpreject* pContext = (testtcpreject*)pData;

	testRequire(xrtNetWorkerIsCurrent(xrtNetListenerWorker(pListener)),
		"TCP reject listener close worker mismatch");
	testRequire(xrtAtomic32Load(
		&pContext->Returned,
		XMEMORY_ACQUIRE
	) != 0, "TCP listener closed before Accept returned");
	(void)xrtAtomic32FetchAdd(
		&pContext->ListenerClose,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证用户拒绝、事件抑制和 Listener 关闭顺序。 */
static void testTcpRejectRace(void)
{
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents StreamEvents;
	xnetlistenerstats Stats;
	testtcpreject Context;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetstream* pRejected;
	xnetsocket FirstPeer;
	xnetsocket SecondPeer;
	xnetaddr Address;

	memset(&Context, 0, sizeof(Context));
	xrtAtomicPtrInit(&Context.Stream, NULL);
	Context.Byte = 'R';
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&StreamEvents, 0, sizeof(StreamEvents));
	ListenerEvents.Accept = testTcpRejectAccept;
	ListenerEvents.Error = testTcpRejectError;
	ListenerEvents.Close = testTcpRejectListenerClose;
	StreamEvents.Open = testTcpRejectOpen;
	StreamEvents.Close = testTcpRejectStreamClose;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TCP_BACKEND;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP reject engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TCP reject listener address failed");
	ListenConfig.Affinity = 0;
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		&StreamEvents,
		&Context
	);
	testRequire((pListener != NULL) &&
		 xrtNetListenerLocal(pListener, &Address),
		"TCP reject listener start failed");

	FirstPeer = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		0
	);
	testRequire((FirstPeer != NULL) &&
		 (xrtNetSocketConnect(FirstPeer, &Address) == XNET_RESULT_OK),
		"TCP first rejected peer failed");
	testTcpEdgeWait(&Context.Accepts, 1,
		"TCP first reject callback missing");
	testRequire(xrtNetSocketClose(FirstPeer),
		"TCP first rejected peer close failed");

	SecondPeer = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		0
	);
	testRequire((SecondPeer != NULL) &&
		 (xrtNetSocketConnect(SecondPeer, &Address) == XNET_RESULT_OK),
		"TCP second rejected peer failed");
	testTcpEdgeWait(&Context.Blocked, 1,
		"TCP second Accept did not block");
	pRejected = (xnetstream*)xrtAtomicPtrLoad(
		&Context.Stream,
		XMEMORY_ACQUIRE
	);
	testRequire((pRejected != NULL) && (xrtNetStreamSendRef(
		pRejected,
		&Context.Byte,
		1,
		testTcpRejectRelease,
		&Context
	) == XNET_RESULT_OK),
		"TCP rejected stream cross-thread send was not accepted");
	testRequire(xrtNetListenerClose(pListener),
		"TCP reject listener close request failed");
	xrtSleep(20);
	testRequire(xrtAtomic32Load(
		&Context.ListenerClose,
		XMEMORY_ACQUIRE
	) == 0, "TCP listener close overtook Accept callback");
	xrtAtomic32Store(&Context.Release, 1, XMEMORY_RELEASE);
	testTcpEdgeWait(&Context.Returned, 1,
		"TCP second Accept did not return");
	testTcpEdgeWait(&Context.Released, 1,
		"TCP rejected stream did not release accepted ownership");
	testTcpEdgeWaitClosed(pRejected,
		"TCP rejected stream did not reach terminal state");
	testTcpEdgeWait(&Context.ListenerClose, 1,
		"TCP reject listener close callback missing");
	{
		xnetstreamstats StreamStats;

		testRequire(xrtNetStreamStats(pRejected, &StreamStats) &&
			(xrtAtomic32Load(
				&Context.ReleaseMatched,
				XMEMORY_ACQUIRE
			 ) == 1) && (StreamStats.State == XNET_STREAM_CLOSED) &&
			(StreamStats.QueuedBytes == 0),
			"TCP rejected stream published terminal state before ownership release");
	}
	testRequire(xrtNetListenerStats(pListener, &Stats) &&
		 (Stats.State == XNET_LISTENER_CLOSED) &&
		 (Stats.Accepted == 0) && (Stats.Rejected == 2) &&
		 (Stats.Errors == 0) && (Stats.ActiveAccepts == 0) &&
		 (Stats.ActiveDispatches == 0),
		"TCP reject listener statistics mismatch");
	testRequire((xrtAtomic32Load(
		&Context.Opens,
		XMEMORY_ACQUIRE
	 ) == 0) && (xrtAtomic32Load(
		&Context.StreamCloses,
		XMEMORY_ACQUIRE
	 ) == 0) && (xrtAtomic32Load(
		&Context.ListenerErrors,
		XMEMORY_ACQUIRE
	 ) == 0), "TCP rejected stream leaked lifecycle events");
	testRequire(xrtNetSocketClose(SecondPeer),
		"TCP second rejected peer close failed");
	xrtNetStreamDestroy(pRejected);
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP reject engine destroy failed");
}



/* 接受 ReadLimit 测试连接并接管 Stream 引用。 */
static bool testTcpLimitAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testtcplimit* pContext = (testtcplimit*)pData;

	(void)pListener;
	testRequire(xrtNetStreamSetData(pStream, pContext),
		"TCP limit stream data setup failed");
	xrtAtomicPtrStore(&pContext->Stream, pStream, XMEMORY_RELEASE);
	xrtAtomic32Store(&pContext->Accepted, 1, XMEMORY_RELEASE);
	return true;
}



/* 故意不消费字节，验证 Stream 在硬上限施加自动背压。 */
static void testTcpLimitRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	testtcplimit* pContext = (testtcplimit*)pData;

	(void)pStream;
	testRequire(xrtNetBufSize(pBuffer) == 8,
		"TCP read exceeded its configured hard limit");
	(void)xrtAtomic32FetchAdd(&pContext->Reads, 1, XMEMORY_RELEASE);
}



/* 在 Stream Worker 上验证借用缓冲并消费一个完整窗口。 */
static void testTcpLimitConsume(xnetworker* pWorker, ptr pData)
{
	testtcplimit* pContext = (testtcplimit*)pData;
	xnetstream* pStream = (xnetstream*)xrtAtomicPtrLoad(
		&pContext->Stream,
		XMEMORY_ACQUIRE
	);
	const xnetbuf* pBuffer;
	char Data[8];

	testRequire((pStream != NULL) &&
		 (xrtNetStreamWorker(pStream) == pWorker),
		"TCP limit consume worker mismatch");
	pBuffer = xrtNetStreamBuffer(pStream);
	testRequire((pBuffer != NULL) &&
		 (xrtNetBufSize(pBuffer) == sizeof(Data)) &&
		 (xrtNetStreamAvailable(pStream) == sizeof(Data)),
		"TCP limit borrowed buffer mismatch");
	testRequire(xrtNetStreamRead(
		pStream,
		Data,
		sizeof(Data)
	) == sizeof(Data), "TCP limit worker read failed");
	(void)xrtAtomic32FetchAdd(
		&pContext->Consumed,
		1,
		XMEMORY_RELEASE
	);
}



/* ReadLimit 流最终必须正常关闭且不附带范围错误。 */
static void testTcpLimitClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testtcplimit* pContext = (testtcplimit*)pData;

	(void)pStream;
	if ( (Result == XNET_RESULT_OK) && (pError == NULL) ) {
		xrtAtomic32Store(
			&pContext->CloseMatched,
			1,
			XMEMORY_RELEASE
		);
	}
	xrtAtomic32Store(&pContext->Closed, 1, XMEMORY_RELEASE);
}



/* 验证 ReadLimit 形成可恢复硬边界，而不是慢消费者断线条件。 */
static void testTcpReadLimit(void)
{
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents StreamEvents;
	testtcplimit Context;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetstream* pStream;
	xnetsocket Peer;
	xnetstreamstats Stats;
	xnetaddr Address;
	static const char sPayload[] = "0123456789abcdef";

	memset(&Context, 0, sizeof(Context));
	xrtAtomicPtrInit(&Context.Stream, NULL);
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&StreamEvents, 0, sizeof(StreamEvents));
	ListenerEvents.Accept = testTcpLimitAccept;
	StreamEvents.Read = testTcpLimitRead;
	StreamEvents.Close = testTcpLimitClose;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TCP_BACKEND;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP limit engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TCP limit listener address failed");
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
		"TCP limit listener start failed");
	Peer = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_STREAM, 0);
	testRequire((Peer != NULL) &&
		 (xrtNetSocketConnect(Peer, &Address) == XNET_RESULT_OK),
		"TCP limit peer connect failed");
	testTcpEdgeWait(&Context.Accepted, 1,
		"TCP limit accept callback missing");
	testTcpEdgeSend(Peer, sPayload, sizeof(sPayload) - 1u);
	testTcpEdgeWait(&Context.Reads, 1,
		"TCP limit first read callback missing");
	pStream = (xnetstream*)xrtAtomicPtrLoad(
		&Context.Stream,
		XMEMORY_ACQUIRE
	);
	testRequire((pStream != NULL) && xrtNetStreamStats(pStream, &Stats) &&
		 (Stats.State == XNET_STREAM_OPEN) &&
		 (Stats.ReceivedBytes == 8) && (Stats.ReadEvents == 1) &&
		 (Stats.BufferedBytes == 8) && Stats.ReadBlocked &&
		 (xrtNetStreamError(pStream) == NULL),
		"TCP read-limit first backpressure state mismatch");
	testRequire(xrtNetStreamBuffer(pStream) == NULL,
		"TCP borrowed buffer escaped its worker");
	xrtClearError();
	testRequire(xrtNetEnginePost(
		pEngine,
		xrtNetWorkerIndex(xrtNetStreamWorker(pStream)),
		testTcpLimitConsume,
		&Context
	), "TCP limit first consume post failed");
	testTcpEdgeWait(&Context.Consumed, 1,
		"TCP limit first consume task missing");
	testTcpEdgeWait(&Context.Reads, 2,
		"TCP limit did not resume buffered socket data");
	testRequire(xrtNetStreamStats(pStream, &Stats) &&
		 (Stats.State == XNET_STREAM_OPEN) &&
		 (Stats.ReceivedBytes == 16) && (Stats.ReadEvents == 2) &&
		 (Stats.BufferedBytes == 8) && Stats.ReadBlocked,
		"TCP read-limit second backpressure state mismatch");
	testRequire(xrtNetEnginePost(
		pEngine,
		xrtNetWorkerIndex(xrtNetStreamWorker(pStream)),
		testTcpLimitConsume,
		&Context
	), "TCP limit second consume post failed");
	testTcpEdgeWait(&Context.Consumed, 2,
		"TCP limit second consume task missing");
	testRequire(xrtNetStreamStats(pStream, &Stats) &&
		 (Stats.BufferedBytes == 0) && !Stats.ReadBlocked,
		"TCP read-limit did not clear automatic backpressure");
	testRequire(xrtNetSocketClose(Peer),
		"TCP limit peer close failed");
	testRequire(xrtNetStreamClose(pStream),
		"TCP limit stream close failed");
	testTcpEdgeWait(&Context.Closed, 1,
		"TCP limit normal close callback missing");
	testRequire((xrtAtomic32Load(
		&Context.CloseMatched,
		XMEMORY_ACQUIRE
	 ) == 1) && (xrtNetStreamError(pStream) == NULL),
		"TCP read-limit normal close mismatch");
	testRequire(xrtNetListenerClose(pListener),
		"TCP limit listener close failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtSleep(1);
	}
	xrtNetStreamDestroy(pStream);
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP limit engine destroy failed");
}



/* 验证关闭入口建立后，迟到的 Accept 完成只负责释放资源。 */
static void testTcpLateListenerCompletion(void)
{
	testtcplatelisten Context;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents Events;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetsocket Peers[TEST_TCP_LATE_PEERS];
	xnetaddr Address;

	memset(&Context, 0, sizeof(Context));
	memset(&Events, 0, sizeof(Events));
	memset(Peers, 0, sizeof(Peers));
	Events.Accept = testTcpLateListenAccept;
	Events.Error = testTcpLateListenError;
	Events.Close = testTcpLateListenClose;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TCP_BACKEND;
	EngineConfig.Workers = 1;
	EngineConfig.EventBatch = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP late-listener engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TCP late-listener address failed");
	ListenConfig.AcceptConcurrency = TEST_TCP_LATE_PEERS;
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&Events,
		NULL,
		&Context
	);
	testRequire((pListener != NULL) &&
		 xrtNetListenerLocal(pListener, &Address),
		"TCP late-listener start failed");
	testRequire(xrtNetEnginePost(
		pEngine,
		xrtNetWorkerIndex(xrtNetListenerWorker(pListener)),
		testTcpLateListenBlock,
		&Context
	), "TCP late-listener blocker post failed");
	testTcpEdgeWait(&Context.Blocked, 1,
		"TCP late-listener blocker did not enter");

	/* Worker 被阻塞时，让完成式后端积累一批成功 Accept。 */
	for ( uint32 i = 0; i < TEST_TCP_LATE_PEERS; i++ ) {
		Peers[i] = xrtNetSocketOpen(
			XNET_FAMILY_IPV4,
			XNET_SOCKET_STREAM,
			0
		);
		testRequire((Peers[i] != NULL) &&
			 (xrtNetSocketConnect(Peers[i], &Address) == XNET_RESULT_OK),
			"TCP late-listener peer connect failed");
	}
	testRequire(xrtNetListenerClose(pListener) &&
		 (xrtNetListenerState(pListener) == XNET_LISTENER_CLOSING),
		"TCP late-listener close gate failed");
	xrtAtomic32Store(&Context.Release, 1, XMEMORY_RELEASE);
	testTcpEdgeWait(&Context.Closes, 1,
		"TCP late-listener close callback missing");

	for ( uint32 i = 0; i < TEST_TCP_LATE_PEERS; i++ ) {
		testRequire(xrtNetSocketClose(Peers[i]),
			"TCP late-listener peer close failed");
	}
	testRequire(
		(xrtAtomic32Load(&Context.Accepts, XMEMORY_ACQUIRE) == 0) &&
		(xrtAtomic32Load(&Context.Errors, XMEMORY_ACQUIRE) == 0) &&
		(xrtAtomic32Load(&Context.Closes, XMEMORY_ACQUIRE) == 1),
		"TCP late-listener completion escaped its close boundary"
	);
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP late-listener engine retained resources");
	testRequire(xrtAtomic32Load(
		&Context.Closes,
		XMEMORY_ACQUIRE
	) == 1, "TCP late-listener Close was not unique");
}



/* 验证优先关闭不能越过仍在普通命令队列中的 Stream 启动任务。 */
static void testTcpStartCloseRace(void)
{
	testtcpstartclose Context;
	xnetengineconfig EngineConfig;
	xnetstreamconfig StreamConfig;
	xnetstreamevents Events;
	xnetengine* pEngine;
	xnetworker* pWorker;
	xnetstream* pStream;
	xnetaddr Address;

	memset(&Context, 0, sizeof(Context));
	memset(&Events, 0, sizeof(Events));
	xrtAtomic32Init(&Context.Blocked, 0);
	xrtAtomic32Init(&Context.Release, 0);
	xrtAtomic32Init(&Context.Noops, 0);
	xrtAtomic32Init(&Context.Open, 0);
	xrtAtomic32Init(&Context.Closed, 0);
	xrtAtomic32Init(&Context.CloseMatched, 0);
	Events.Open = testTcpStartCloseOpen;
	Events.Close = testTcpStartCloseClose;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TCP_BACKEND;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP start-close engine start failed");
	pWorker = xrtNetEngineWorker(pEngine, 0);
	testRequire((pWorker != NULL) && xrtNetEnginePost(
		pEngine,
		0,
		testTcpStartCloseBlock,
		&Context
	), "TCP start-close blocker post failed");
	testTcpEdgeWait(&Context.Blocked, 1,
		"TCP start-close blocker did not enter");

	/* 300 条任务保证 Connect 落在当前 256 条命令预算之外。 */
	for ( uint32 i = 0; i < 300; i++ ) {
		testRequire(xrtNetEnginePost(
			pEngine,
			0,
			testTcpStartCloseNoop,
			&Context
		), "TCP start-close noop post failed");
	}
	testRequire(xrtNetAddrLoopback(
		&Address,
		XNET_FAMILY_IPV4,
		1
	), "TCP start-close address failed");
	xrtNetStreamConfigInit(&StreamConfig);
	pStream = xrtNetStreamConnect(
		pEngine,
		&Address,
		0,
		&StreamConfig,
		&Events,
		&Context
	);
	testRequire((pStream != NULL) && xrtNetStreamAbort(pStream),
		"TCP start-close abort request failed");

	/* 只保留运行时占用，放大迟到启动访问已释放对象的风险。 */
	xrtNetStreamDestroy(pStream);
	xrtAtomic32Store(&Context.Release, 1, XMEMORY_RELEASE);
	testTcpEdgeWait(&Context.Noops, 300,
		"TCP start-close queued commands did not drain");
	testTcpEdgeWait(&Context.Closed, 1,
		"TCP start-close terminal callback missing");
	testRequire(
		(xrtAtomic32Load(&Context.Open, XMEMORY_ACQUIRE) == 0) &&
		(xrtAtomic32Load(
			&Context.CloseMatched,
			XMEMORY_ACQUIRE
		 ) == 1),
		"TCP start-close lifecycle escaped its terminal boundary"
	);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP start-close engine retained leaked resources");
}



/* 验证 LowWater 内重入发送不会破坏链表或重复发布 Drain。 */
static void testTcpWriteReentrant(void)
{
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents StreamEvents;
	testtcpwritereentrant Context;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetstream* pStream;
	xnetsocket Peer;
	xnetaddr Address;
	xnetstreamstats Stats;
	char sReply[2] = { 0 };

	memset(&Context, 0, sizeof(Context));
	xrtAtomicPtrInit(&Context.Stream, NULL);
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&StreamEvents, 0, sizeof(StreamEvents));
	ListenerEvents.Accept = testTcpWriteAccept;
	StreamEvents.Open = testTcpWriteOpen;
	StreamEvents.HighWater = testTcpWriteHighWater;
	StreamEvents.LowWater = testTcpWriteLowWater;
	StreamEvents.Drain = testTcpWriteDrain;
	StreamEvents.Close = testTcpWriteClose;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TCP_BACKEND;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP reentrant engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TCP reentrant listener address failed");
	ListenConfig.Stream.WriteHighWater = 1;
	ListenConfig.Stream.WriteLowWater = 0;
	ListenConfig.Stream.WriteLimit = 1;
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		&StreamEvents,
		&Context
	);
	testRequire((pListener != NULL) &&
		xrtNetListenerLocal(pListener, &Address),
		"TCP reentrant listener start failed");
	Peer = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_STREAM, 0);
	testRequire((Peer != NULL) &&
		(xrtNetSocketConnect(Peer, &Address) == XNET_RESULT_OK),
		"TCP reentrant peer connect failed");
	testTcpEdgeWait(&Context.Accepted, 1,
		"TCP reentrant accept callback missing");
	testTcpEdgeWait(&Context.Open, 1,
		"TCP reentrant open callback missing");
	pStream = (xnetstream*)xrtAtomicPtrLoad(
		&Context.Stream,
		XMEMORY_ACQUIRE
	);
	testRequire((pStream != NULL) && (xrtNetStreamSend(
		pStream,
		"A",
		1
	) == XNET_RESULT_OK), "TCP reentrant first send failed");
	testTcpEdgeWait(&Context.LowWater, 2,
		"TCP reentrant low-water callbacks missing");
	testTcpEdgeWait(&Context.Drain, 1,
		"TCP reentrant drain callback missing");
	testTcpEdgeRecv(Peer, sReply, sizeof(sReply));
	xrtSleep(20);
	testRequire((memcmp(sReply, "AB", sizeof(sReply)) == 0) &&
		(xrtAtomic32Load(
			&Context.HighWater,
			XMEMORY_ACQUIRE
		 ) == 2) &&
		(xrtAtomic32Load(
			&Context.LowWater,
			XMEMORY_ACQUIRE
		 ) == 2) &&
		(xrtAtomic32Load(
			&Context.Drain,
			XMEMORY_ACQUIRE
		 ) == 1),
		"TCP reentrant water events were not coalesced");
	testRequire(xrtNetStreamStats(pStream, &Stats) &&
		(Stats.SentBytes == 2) && (Stats.QueuedBytes == 0) &&
		!Stats.WriteBackpressured,
		"TCP reentrant write statistics mismatch");
	testRequire(xrtNetEnginePost(
		pEngine,
		xrtNetWorkerIndex(xrtNetStreamWorker(pStream)),
		testTcpWriteAbortUpgrade,
		&Context
	), "TCP reentrant abort upgrade post failed");
	testTcpEdgeWait(&Context.Closed, 1,
		"TCP reentrant close callback missing");
	testRequire(xrtNetSocketClose(Peer),
		"TCP reentrant peer close failed");
	xrtNetStreamDestroy(pStream);
	xrtAtomicPtrStore(&Context.Stream, NULL, XMEMORY_RELEASE);
	xrtAtomic32Store(&Context.Accepted, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&Context.Open, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&Context.HighWater, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&Context.LowWater, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&Context.Drain, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&Context.Closed, 0, XMEMORY_RELEASE);
	Context.AbortHighWater = true;
	Peer = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_STREAM, 0);
	testRequire((Peer != NULL) &&
		(xrtNetSocketConnect(Peer, &Address) == XNET_RESULT_OK),
		"TCP high-water abort peer connect failed");
	testTcpEdgeWait(&Context.Accepted, 1,
		"TCP high-water abort accept callback missing");
	testTcpEdgeWait(&Context.Open, 1,
		"TCP high-water abort open callback missing");
	pStream = (xnetstream*)xrtAtomicPtrLoad(
		&Context.Stream,
		XMEMORY_ACQUIRE
	);
	testRequire((pStream != NULL) && (xrtNetStreamSend(
		pStream,
		"C",
		1
	) == XNET_RESULT_OK), "TCP high-water abort send failed");
	testTcpEdgeWait(&Context.Closed, 1,
		"TCP high-water abort close callback missing");
	testRequire(xrtNetStreamStats(pStream, &Stats) &&
		(Stats.SentBytes == 0) && (Stats.QueuedBytes == 0) &&
		(xrtAtomic32Load(
			&Context.HighWater,
			XMEMORY_ACQUIRE
		 ) == 1) && (xrtAtomic32Load(
			&Context.LowWater,
			XMEMORY_ACQUIRE
		 ) == 0) && (xrtAtomic32Load(
			&Context.Drain,
			XMEMORY_ACQUIRE
		 ) == 0), "TCP high-water abort advanced the write queue");
	testRequire(xrtNetSocketClose(Peer),
		"TCP high-water abort peer close failed");
	testRequire(xrtNetListenerClose(pListener),
		"TCP reentrant listener close failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtSleep(1);
	}
	xrtNetStreamDestroy(pStream);
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP reentrant engine destroy failed");
}



/* 覆盖 TCP 接受关闭竞态和接收硬上限。 */
int main(void)
{
	testTcpLateListenerCompletion();
	testTcpStartCloseRace();
	testTcpRejectRace();
	testTcpReadLimit();
	testTcpWriteReentrant();
	printf("[PASS] network TCP %s edges\n", TEST_TCP_BACKEND_NAME);
	return 0;
}
