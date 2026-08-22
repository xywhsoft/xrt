#include "../test.h"

#include "../../src/internal/xrt_websocket.h"
#include "../../../../src/internal/xrt_tcp.h"



#ifndef TEST_WS_FUTURE_BACKEND
	#define TEST_WS_FUTURE_BACKEND XNET_PORT_SELECT
	#define TEST_WS_FUTURE_BACKEND_NAME "select"
#endif

#define TEST_WS_FUTURE_ASYNC_BYTES ((size_t)(768u * 1024u))
#define TEST_WS_FUTURE_ASYNC_COUNT UINT32_C(2)
#if defined(TEST_WS_FUTURE_REF)
	#define TEST_WS_FUTURE_BARRIER_COUNT UINT32_C(3)
#endif
#define TEST_WS_FUTURE_OOM_BYTES ((size_t)(512u * 1024u))
#define TEST_WS_FUTURE_TCP_LIMIT ((size_t)(1024u * 1024u))
#define TEST_WS_FUTURE_WS_LIMIT ((size_t)(2u * 1024u * 1024u))
#define TEST_WS_FUTURE_PRODUCERS 8u
#define TEST_WS_FUTURE_STRESS_THREADS 4u
#define TEST_WS_FUTURE_STRESS_ROUNDS 250u



typedef struct testwsfuture {
	xnetengine* Engine;
	xnetlistener* Listener;
	xatomicptr Client;
	xatomicptr Server;
	xatomicptr AwaitFuture;
	xatomic32 Blocked;
	xatomic32 Release;
	xatomic32 Awaiting;
	xatomic32 Messages;
	#if defined(TEST_WS_FUTURE_GROUP_OPERATION)
		xatomic32 MessageLock;
	#endif
	xatomic32 Pong;
	xatomic32 Closed;
	xatomic32 ListenerClosed;
	xatomic32 AllocFail;
	xatomic64 AllocAttempts;
	#if defined(TEST_WS_FUTURE_REF)
		xatomic32 RefReleases;
	#endif
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
		xatomicptr Writer;
		xatomicptr WriterFuture;
		xatomic32 WriterReady;
		xatomic32 WriterHeld;
		xatomic32 WriterMessages;
		xatomic64 WriterMessageBase;
	#endif
	size_t MessageSize;
	char Message[32];
} testwsfuture;



/* 每个生产线程只发布一个独立字节并保存自己的 Future 或线程错误。 */
typedef struct testwsfutureproducer {
	xwsconn* Connection;
	xfuture* Future;
	xerrkind Error;
	uint8 Byte;
} testwsfutureproducer;



/* 竞态压力线程共享 Connection，但各自拥有 Future 生命周期。 */
typedef struct testwsfuturestress {
	xwsconn* Connection;
	xatomic32* Failed;
	size_t Rounds;
} testwsfuturestress;



/* Close 优先级测试只暂借 TCP 原子计数，不建立不存在的发送节点。 */
typedef struct testwsfuturepressure {
	xnetstream* Stream;
	uint64 Bytes;
	xatomic32 Released;
} testwsfuturepressure;



/* 独立门闩用于冻结 Worker 或 Ref 释放回调。 */
#if defined(TEST_WS_FUTURE_REF)
typedef struct testwsfuturegate {
	xatomic32 Blocked;
	xatomic32 Release;
} testwsfuturegate;
#endif



#if defined(TEST_WS_FUTURE_GROUP)
/* 连接组压力线程共享唯一成员并记录任何意外加入失败。 */
typedef struct testwsfuturegroupstress {
	xwsgroup* Group;
	xwsconn* Connection;
	xatomic32* Failed;
	size_t Rounds;
} testwsfuturegroupstress;
#endif



/* 正常阶段转发分配，故障窗口拒绝全部底层内存申请。 */
static ptr testWsFutureAlloc(ptr pData, size_t iSize)
{
	testwsfuture* pTest = (testwsfuture*)pData;

	(void)xrtAtomic64FetchAdd(
		&pTest->AllocAttempts,
		1,
		XMEMORY_RELAXED
	);
	return xrtAtomic32Load(
		&pTest->AllocFail,
		XMEMORY_ACQUIRE
	) != 0 ? NULL : malloc(iSize);
}



/* 重分配和普通分配共享同一个确定故障窗口。 */
static ptr testWsFutureRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	testwsfuture* pTest = (testwsfuture*)pData;

	(void)xrtAtomic64FetchAdd(
		&pTest->AllocAttempts,
		1,
		XMEMORY_RELAXED
	);
	return xrtAtomic32Load(
		&pTest->AllocFail,
		XMEMORY_ACQUIRE
	) != 0 ? NULL : realloc(pMemory, iSize);
}



/* 释放故障窗口之前已经成功取得的系统内存。 */
static void testWsFutureFree(ptr pData, ptr pMemory)
{
	(void)pData;
	free(pMemory);
}



/* 在截止时间前等待原子计数达到目标。 */
static void testWsFutureWaitAtomic(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(
		UINT64_C(10000000)
	);

	while ( xrtAtomic32Load(
		pValue,
		XMEMORY_ACQUIRE
	) < iExpected ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			sMessage
		);
		xrtThreadYield();
	}
}



/* 验证 Future 在截止时间内进入指定终态。 */
static void testWsFutureState(
	xfuture* pFuture,
	xfuturestate State,
	cstr sMessage
)
{
	testRequire(
		(pFuture != NULL) &&
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(10000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == State),
		sMessage
	);
}



#if defined(TEST_WS_FUTURE_COROUTINE)
/* 协程发布已经进入 Await，再挂起到跨线程发送完成。 */
static ptr testWsFutureAwaitProc(ptr pData)
{
	testwsfuture* pTest = (testwsfuture*)pData;
	xfuture* pFuture = (xfuture*)xrtAtomicPtrLoad(
		&pTest->AwaitFuture,
		XMEMORY_ACQUIRE
	);

	xrtAtomic32Store(
		&pTest->Awaiting,
		1,
		XMEMORY_RELEASE
	);
	return (ptr)(uintptr_t)xrtFutureAwait(pFuture);
}



/* 原生线程确认协程已经挂起后才释放网络 Worker。 */
static int32 testWsFutureReleaseThread(ptr pData)
{
	testwsfuture* pTest = (testwsfuture*)pData;

	while ( xrtAtomic32Load(
		&pTest->Awaiting,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
	xrtAtomic32Store(
		&pTest->Release,
		1,
		XMEMORY_RELEASE
	);
	return 0;
}



/* 验证通用 Future 可以直接挂起和唤醒 xrt 协程。 */
static void testWsFutureAwaitSend(
	testwsfuture* pTest,
	xfuture* pFuture
)
{
	xcosched* pScheduler = xrtCoSchedCreate();
	xcoro* pCoroutine;
	xthread* pThread;

	testRequire(
		pScheduler != NULL,
		"WebSocket Future coroutine scheduler create failed"
	);
	xrtAtomicPtrStore(
		&pTest->AwaitFuture,
		pFuture,
		XMEMORY_RELEASE
	);
	pCoroutine = xrtCoSpawn(
		pScheduler,
		testWsFutureAwaitProc,
		pTest,
		NULL
	);
	pThread = xrtThreadCreate(
		testWsFutureReleaseThread,
		pTest,
		0
	);
	testRequire(
		(pCoroutine != NULL) &&
		(pThread != NULL) &&
		xrtCoSchedRun(pScheduler) &&
		(xrtThreadWait(pThread) == XWAIT_OK) &&
		(xrtThreadExitCode(pThread) == 0) &&
		((xwaitresult)(uintptr_t)
		 xrtCoResult(pCoroutine) == XWAIT_OK) &&
		(xrtFutureState(pFuture) ==
		 XFUTURE_RESOLVED),
		"WebSocket Future coroutine await failed"
	);
	xrtThreadDestroy(pThread);
	testRequire(
		xrtCoDestroy(pCoroutine) &&
		xrtCoSchedDestroy(pScheduler),
		"WebSocket Future coroutine cleanup failed"
	);
	xrtAtomicPtrStore(
		&pTest->AwaitFuture,
		NULL,
		XMEMORY_RELEASE
	);
}
#endif



/* 阻塞客户端 Worker，构造确定的跨线程排队窗口。 */
static void testWsFutureBlock(
	xnetworker* pWorker,
	ptr pData
)
{
	testwsfuture* pTest = (testwsfuture*)pData;

	(void)pWorker;
	xrtAtomic32Store(
		&pTest->Blocked,
		1,
		XMEMORY_RELEASE
	);
	while ( xrtAtomic32Load(
		&pTest->Release,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
}



/* 重置门闩并把确定阻塞任务投递到客户端 Worker。 */
static void testWsFutureBlockClient(
	testwsfuture* pTest,
	xwsconn* pClient
)
{
	xrtAtomic32Store(
		&pTest->Blocked,
		0,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pTest->Release,
		0,
		XMEMORY_RELEASE
	);
	testRequire(
		xrtNetEnginePost(
			pTest->Engine,
			xrtNetWorkerIndex(
				xrtWsConnWorker(pClient)
			),
			testWsFutureBlock,
			pTest
		),
		"WebSocket Future blocker post failed"
	);
	testWsFutureWaitAtomic(
		&pTest->Blocked,
		1,
		"WebSocket Future client Worker did not block"
	);
}



#if !defined(TEST_WS_FUTURE_WRITER_ABANDON)
/* 在 Future 驱动之后扣回测试压力，并释放临时 Stream 引用。 */
static void testWsFuturePressureRelease(
	xnetworker* pWorker,
	ptr pData
)
{
	testwsfuturepressure* pPressure =
		(testwsfuturepressure*)pData;
	uint64 iQueued = xrtAtomic64FetchSub(
		&pPressure->Stream->QueuedBytes,
		pPressure->Bytes,
		XMEMORY_ACQ_REL
	);

	(void)pWorker;
	testRequire(
		iQueued >= pPressure->Bytes,
		"WebSocket Future pressure counter underflowed"
	);
	if ( pPressure->Stream->Events.Drain != NULL ) {
		pPressure->Stream->Events.Drain(
			pPressure->Stream,
			xrtAtomicPtrLoad(
				&pPressure->Stream->Data,
				XMEMORY_ACQUIRE
			)
		);
	}
	xrtAtomic32Store(
		&pPressure->Released,
		1,
		XMEMORY_RELEASE
	);
	xrtNetStreamDestroy(pPressure->Stream);
}



/* 验证受阻的手动 Ping 不能阻塞其后的唯一 Close。 */
static void testWsFutureClosePriority(
	testwsfuture* pTest,
	xwsconn* pClient
)
{
	static const uint8 Ping[XWS_CLOSE_PAYLOAD_MAX] = { 0 };
	testwsfuturepressure Pressure;
	xfuture* pPing;
	xfuture* pClose;
	xfuture* pClosed;
	size_t iControlSlot = 2u + XWS_CLOSE_PAYLOAD_MAX +
		XWS_MASK_SIZE;
	size_t iWriteLimit;
	uint64 iQueued;

	memset(&Pressure, 0, sizeof(Pressure));
	xrtAtomic32Init(&Pressure.Released, 0);
	Pressure.Stream = xrtWsConnTcpRef(pClient);
	testRequire(
		Pressure.Stream != NULL,
		"WebSocket Future pressure transport is unavailable"
	);
	iWriteLimit = xrtNetStreamWriteLimit(Pressure.Stream);
	iQueued = xrtAtomic64Load(
		&Pressure.Stream->QueuedBytes,
		XMEMORY_ACQUIRE
	);
	testRequire(
		(iQueued == 0) && (iWriteLimit > (iControlSlot * 2u)),
		"WebSocket Future pressure test entered with pending output"
	);
	Pressure.Bytes = (uint64)(iWriteLimit - (iControlSlot * 2u));

	testWsFutureBlockClient(pTest, pClient);
	(void)xrtAtomic64FetchAdd(
		&Pressure.Stream->QueuedBytes,
		Pressure.Bytes,
		XMEMORY_ACQ_REL
	);
	pPing = xrtWsConnPingAsync(
		pClient,
		(xbytesview) { Ping, sizeof(Ping) }
	);
	pClose = xrtWsConnCloseAsync(
		pClient,
		XWS_CLOSE_NORMAL,
		XRT_STR_LITERAL("done")
	);
	testRequire(
		(pPing != NULL) &&
		(pClose != NULL) &&
		(xrtFutureState(pPing) == XFUTURE_PENDING) &&
		(xrtFutureState(pClose) == XFUTURE_PENDING) &&
		(xrtWsConnAsyncCount(pClient) == 2u),
		"WebSocket Future Close priority setup failed"
	);
	xrtAtomic32Store(
		&pTest->Release,
		1,
		XMEMORY_RELEASE
	);
	testWsFutureState(
		pClose,
		XFUTURE_RESOLVED,
		"WebSocket Close did not pass a pressured Ping"
	);
	testWsFutureState(
		pPing,
		XFUTURE_CLOSED,
		"WebSocket pressured Ping did not close after priority Close"
	);
	testRequire(
		xrtNetEnginePost(
			pTest->Engine,
			xrtNetWorkerIndex(xrtWsConnWorker(pClient)),
			testWsFuturePressureRelease,
			&Pressure
		),
		"WebSocket Future pressure release post failed"
	);
	testWsFutureWaitAtomic(
		&Pressure.Released,
		1,
		"WebSocket Future pressure was not released"
	);
	pClosed = xrtWsConnWaitAsync(
		pClient,
		XWS_CONN_WAIT_CLOSE
	);
	testWsFutureState(
		pClosed,
		XFUTURE_RESOLVED,
		"WebSocket priority Close did not finish its handshake"
	);
	testWsFutureWaitAtomic(
		&pTest->Closed,
		2,
		"WebSocket priority Close callbacks missing"
	);
	testRequire(
		(xrtWsConnAsyncCount(pClient) == 0) &&
		(xrtWsConnAsyncBytes(pClient) == 0),
		"WebSocket priority Close leaked asynchronous budget"
	);
	xrtFutureDestroy(pPing);
	xrtFutureDestroy(pClose);
	xrtFutureDestroy(pClosed);
}
#endif



#if defined(TEST_WS_FUTURE_REF)
/* 在目标 Connection Worker 上阻塞，直到测试线程释放独立门闩。 */
static void testWsFutureGateTask(
	xnetworker* pWorker,
	ptr pData
)
{
	testwsfuturegate* pGate = (testwsfuturegate*)pData;

	(void)pWorker;
	xrtAtomic32Store(
		&pGate->Blocked,
		1,
		XMEMORY_RELEASE
	);
	while ( xrtAtomic32Load(
		&pGate->Release,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
}



/* 初始化并把一个独立门闩排到指定 Connection Worker。 */
static void testWsFutureGatePost(
	testwsfuture* pTest,
	xwsconn* pConnection,
	testwsfuturegate* pGate
)
{
	xrtAtomic32Init(&pGate->Blocked, 0);
	xrtAtomic32Init(&pGate->Release, 0);
	testRequire(
		xrtNetEnginePost(
			pTest->Engine,
			xrtNetWorkerIndex(
				xrtWsConnWorker(pConnection)
			),
			testWsFutureGateTask,
			pGate
		),
		"WebSocket Future gate post failed"
	);
}



/* 等待独立门闩进入 Worker，并在检查完成后显式释放。 */
static void testWsFutureGateWait(
	testwsfuturegate* pGate,
	cstr sMessage
)
{
	testWsFutureWaitAtomic(
		&pGate->Blocked,
		1,
		sMessage
	);
}



/* 释放一个已经进入 Worker 的独立门闩。 */
static void testWsFutureGateRelease(testwsfuturegate* pGate)
{
	xrtAtomic32Store(
		&pGate->Release,
		1,
		XMEMORY_RELEASE
	);
}
#endif



#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
/* 在客户端 Worker 上开始分片消息并占用唯一的数据消息发送权。 */
static void testWsFutureWriterBegin(
	xnetworker* pWorker,
	ptr pData
)
{
	testwsfuture* pTest = (testwsfuture*)pData;
	xwsconn* pConnection = (xwsconn*)xrtAtomicPtrLoad(
		&pTest->Client,
		XMEMORY_ACQUIRE
	);
	xwswriter* pWriter;

	(void)pWorker;
	pWriter = xrtWsConnBeginBinary(pConnection);
	testRequire(
		(pWriter != NULL) &&
		(xrtWsWriterWrite(
			pWriter,
			XRT_BYTES_LITERAL("writer-")
		 ) == XNET_RESULT_OK),
		"WebSocket Writer did not acquire the Future connection"
	);
	xrtAtomicPtrStore(
		&pTest->Writer,
		pWriter,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pTest->WriterReady,
		1,
		XMEMORY_RELEASE
	);
}



/* 排在异步发送命令之后，确认 Future 没有越过活动 Writer。 */
static void testWsFutureWriterProbe(
	xnetworker* pWorker,
	ptr pData
)
{
	testwsfuture* pTest = (testwsfuture*)pData;
	xfuture* pFuture = (xfuture*)xrtAtomicPtrLoad(
		&pTest->WriterFuture,
		XMEMORY_ACQUIRE
	);
	xwsconn* pConnection = (xwsconn*)xrtAtomicPtrLoad(
		&pTest->Client,
		XMEMORY_ACQUIRE
	);

	(void)pWorker;
	testRequire(
		(pFuture != NULL) &&
		(xrtFutureState(pFuture) == XFUTURE_PENDING) &&
		(xrtWsConnAsyncCount(pConnection) == 1),
		"WebSocket Future crossed an active Writer"
	);
	xrtAtomic32Store(
		&pTest->WriterHeld,
		1,
		XMEMORY_RELEASE
	);
}



/* 完成分片消息并释放发送权，随后 Future 队列应被主动唤醒。 */
static void testWsFutureWriterFinish(
	xnetworker* pWorker,
	ptr pData
)
{
	testwsfuture* pTest = (testwsfuture*)pData;
	xwswriter* pWriter = (xwswriter*)xrtAtomicPtrLoad(
		&pTest->Writer,
		XMEMORY_ACQUIRE
	);

	(void)pWorker;
	#if defined(TEST_WS_FUTURE_WRITER_ABANDON)
		xrtWsWriterDestroy(pWriter);
	#else
		testRequire(
			xrtWsWriterFinish(
				pWriter,
				(xbytesview) { NULL, 0 }
			) == XNET_RESULT_OK,
			"WebSocket Writer did not release the Future connection"
		);
		xrtWsWriterDestroy(pWriter);
	#endif
	xrtAtomicPtrStore(
		&pTest->Writer,
		NULL,
		XMEMORY_RELEASE
	);
}



/* 验证 Writer 独占期间 Future 保持排队，结束后按原顺序恢复。 */
static void testWsFutureWriterQueue(
	testwsfuture* pTest,
	xwsconn* pClient
)
{
	xfuture* pFuture;
	xfuture* pControl;
	size_t iWorker = xrtNetWorkerIndex(
		xrtWsConnWorker(pClient)
	);
	size_t iMessageBase = pTest->MessageSize;

	/* 只校验本阶段追加的两条消息，不依赖前序变体累计的测试数据。 */
	xrtAtomic32Store(
		&pTest->WriterMessages,
		0,
		XMEMORY_RELEASE
	);
	xrtAtomic64Store(
		&pTest->WriterMessageBase,
		(uint64)iMessageBase,
		XMEMORY_RELEASE
	);

	testRequire(
		xrtNetEnginePost(
			pTest->Engine,
			iWorker,
			testWsFutureWriterBegin,
			pTest
		),
		"WebSocket Writer begin post failed"
	);
	testWsFutureWaitAtomic(
		&pTest->WriterReady,
		1,
		"WebSocket Writer did not become active"
	);

	pFuture = xrtWsConnBinaryAsync(
		pClient,
		XRT_BYTES_LITERAL("queued")
	);
	testRequire(
		pFuture != NULL,
		"WebSocket Future enqueue behind Writer failed"
	);
	xrtAtomicPtrStore(
		&pTest->WriterFuture,
		pFuture,
		XMEMORY_RELEASE
	);
	pControl = xrtWsConnPongAsync(
		pClient,
		XRT_BYTES_LITERAL("probe")
	);
	testWsFutureState(
		pControl,
		XFUTURE_RESOLVED,
		"WebSocket Pong did not cross a blocked data Future"
	);
	xrtFutureDestroy(pControl);
	testWsFutureWaitAtomic(
		&pTest->Pong,
		1,
		"WebSocket peer missed Pong behind active Writer"
	);
	testRequire(
		xrtNetEnginePost(
			pTest->Engine,
			iWorker,
			testWsFutureWriterProbe,
			pTest
		),
		"WebSocket Writer probe post failed"
	);
	testWsFutureWaitAtomic(
		&pTest->WriterHeld,
		1,
		"WebSocket Future was not held behind Writer"
	);
	testRequire(
		xrtNetEnginePost(
			pTest->Engine,
			iWorker,
			testWsFutureWriterFinish,
			pTest
		),
		"WebSocket Writer finish post failed"
	);
	testWsFutureState(
		pFuture,
		#if defined(TEST_WS_FUTURE_WRITER_ABANDON)
			XFUTURE_CLOSED,
			"WebSocket abandoned Writer did not close its queued Future"
		#else
		XFUTURE_RESOLVED,
		"WebSocket Future did not resume after Writer"
		#endif
	);
	#if defined(TEST_WS_FUTURE_WRITER_ABANDON)
		xrtAtomicPtrStore(
			&pTest->WriterFuture,
			NULL,
			XMEMORY_RELEASE
		);
		xrtFutureDestroy(pFuture);
		return;
	#endif
	testWsFutureWaitAtomic(
		&pTest->WriterMessages,
		2,
		"WebSocket Writer and queued Future messages did not arrive"
	);
	testRequire(
		(pTest->MessageSize == (iMessageBase + 13u)) &&
		(memcmp(
			pTest->Message + iMessageBase,
			"writer-queued",
			13
		 ) == 0),
		"WebSocket Writer changed Future FIFO payload order"
	);
	xrtAtomicPtrStore(
		&pTest->WriterFuture,
		NULL,
		XMEMORY_RELEASE
	);
	xrtFutureDestroy(pFuture);
}
#endif



/* 从独立原生线程竞争提交一个复制发送。 */
static int32 testWsFutureProduce(ptr pData)
{
	testwsfutureproducer* pProducer =
		(testwsfutureproducer*)pData;

	xrtClearError();
	pProducer->Future = xrtWsConnBinaryAsync(
		pProducer->Connection,
		(xbytesview) {
			&pProducer->Byte,
			1
		}
	);
	pProducer->Error = xrtErrorKind(xrtGetError());
	return 0;
}



/* 验证并发生产者不会突破计数和字节硬上限。 */
static void testWsFutureConcurrentSubmit(
	testwsfuture* pTest,
	xwsconn* pClient
)
{
	testwsfutureproducer Producers[
		TEST_WS_FUTURE_PRODUCERS
	];
	xthread* Threads[TEST_WS_FUTURE_PRODUCERS];
	size_t iAccepted = 0;

	memset(Producers, 0, sizeof(Producers));
	memset(Threads, 0, sizeof(Threads));
	for ( size_t i = 0;
		i < TEST_WS_FUTURE_PRODUCERS;
		i++ ) {
		Producers[i].Connection = pClient;
		Producers[i].Byte = (uint8)i;
		Threads[i] = xrtThreadCreate(
			testWsFutureProduce,
			&Producers[i],
			0
		);
		testRequire(
			Threads[i] != NULL,
			"WebSocket Future producer create failed"
		);
	}
	for ( size_t i = 0;
		i < TEST_WS_FUTURE_PRODUCERS;
		i++ ) {
		testRequire(
			(xrtThreadWait(Threads[i]) == XWAIT_OK) &&
			(xrtThreadExitCode(Threads[i]) == 0),
			"WebSocket Future producer failed"
		);
		xrtThreadDestroy(Threads[i]);
		if ( Producers[i].Future != NULL ) {
			iAccepted++;
		} else {
			testRequire(
				Producers[i].Error == XERR_AGAIN,
				"WebSocket Future producer rejection mismatch"
			);
		}
	}
	testRequire(
		(iAccepted == TEST_WS_FUTURE_ASYNC_COUNT) &&
		(xrtWsConnAsyncCount(pClient) ==
		 TEST_WS_FUTURE_ASYNC_COUNT) &&
		(xrtWsConnAsyncBytes(pClient) ==
		 TEST_WS_FUTURE_ASYNC_COUNT),
		"WebSocket Future concurrent admission exceeded its hard limit"
	);
	for ( size_t i = 0;
		i < TEST_WS_FUTURE_PRODUCERS;
		i++ ) {
		if ( Producers[i].Future != NULL ) {
			testRequire(
				xrtFutureCancel(
					Producers[i].Future
				),
				"WebSocket Future producer cancel failed"
			);
		}
	}
	xrtAtomic32Store(
		&pTest->Release,
		1,
		XMEMORY_RELEASE
	);
	for ( size_t i = 0;
		i < TEST_WS_FUTURE_PRODUCERS;
		i++ ) {
		if ( Producers[i].Future != NULL ) {
			testWsFutureState(
				Producers[i].Future,
				XFUTURE_CANCELLED,
				"WebSocket Future producer cancel state mismatch"
			);
			xrtFutureDestroy(
				Producers[i].Future
			);
		}
	}
	testRequire(
		(xrtWsConnAsyncCount(pClient) == 0) &&
		(xrtWsConnAsyncBytes(pClient) == 0) &&
		(xrtAtomic32Load(
			&pTest->Messages,
			XMEMORY_ACQUIRE
		 ) == 0),
		"WebSocket Future concurrent cancellation leaked state"
	);
}



/* 连续竞争提交、取消和完成空消息，覆盖取消与 Worker 出队的竞态。 */
static int32 testWsFutureStressProc(ptr pData)
{
	testwsfuturestress* pStress =
		(testwsfuturestress*)pData;

	for ( size_t i = 0; i < pStress->Rounds; ) {
		xfuture* pFuture;
		xfuturestate State;

		xrtClearError();
		pFuture = xrtWsConnBinaryAsync(
			pStress->Connection,
			(xbytesview) { NULL, 0 }
		);
		if ( pFuture == NULL ) {
			if ( xrtErrorKind(xrtGetError()) ==
				XERR_AGAIN ) {
				xrtThreadYield();
				continue;
			}
			xrtAtomic32Store(
				pStress->Failed,
				1,
				XMEMORY_RELEASE
			);
			return 1;
		}
		if ( (i & 1u) != 0 ) {
			(void)xrtFutureCancel(pFuture);
		}
		if ( xrtFutureWaitFor(
			pFuture,
			UINT64_C(10000000)
		) != XWAIT_OK ) {
			xrtFutureDestroy(pFuture);
			xrtAtomic32Store(
				pStress->Failed,
				1,
				XMEMORY_RELEASE
			);
			return 1;
		}
		State = xrtFutureState(pFuture);
		xrtFutureDestroy(pFuture);
		if ( (State != XFUTURE_RESOLVED) &&
			(State != XFUTURE_CANCELLED) ) {
			xrtAtomic32Store(
				pStress->Failed,
				1,
				XMEMORY_RELEASE
			);
			return 1;
		}
		i++;
	}
	return 0;
}



/* 多线程反复穿过空负载计数门禁，确保字节预算不能旁路操作数上限。 */
static void testWsFutureStress(xwsconn* pClient)
{
	testwsfuturestress Stress[
		TEST_WS_FUTURE_STRESS_THREADS
	];
	xthread* Threads[TEST_WS_FUTURE_STRESS_THREADS];
	xatomic32 Failed;

	memset(Stress, 0, sizeof(Stress));
	memset(Threads, 0, sizeof(Threads));
	xrtAtomic32Init(&Failed, 0);
	for ( size_t i = 0;
		i < TEST_WS_FUTURE_STRESS_THREADS;
		i++ ) {
		Stress[i].Connection = pClient;
		Stress[i].Failed = &Failed;
		Stress[i].Rounds =
			TEST_WS_FUTURE_STRESS_ROUNDS;
		Threads[i] = xrtThreadCreate(
			testWsFutureStressProc,
			&Stress[i],
			0
		);
		testRequire(
			Threads[i] != NULL,
			"WebSocket Future stress thread create failed"
		);
	}
	for ( size_t i = 0;
		i < TEST_WS_FUTURE_STRESS_THREADS;
		i++ ) {
		testRequire(
			(xrtThreadWait(Threads[i]) == XWAIT_OK) &&
			(xrtThreadExitCode(Threads[i]) == 0),
			"WebSocket Future stress thread failed"
		);
		xrtThreadDestroy(Threads[i]);
	}
	testRequire(
		(xrtAtomic32Load(
			&Failed,
			XMEMORY_ACQUIRE
		 ) == 0) &&
		(xrtWsConnAsyncCount(pClient) == 0) &&
		(xrtWsConnAsyncBytes(pClient) == 0),
		"WebSocket Future stress leaked queue state"
	);
}



/* 验证全部 Future 入口在解引用前拒绝回绕对象与负载范围。 */
static void testWsFutureBoundaries(xwsconn* pConnection)
{
	xwsconn* pWrapping =
		(xwsconn*)(uintptr_t)(UINTPTR_MAX - 1u);
	xbytesview Wrapping = {
		(cbytes)(uintptr_t)(UINTPTR_MAX - 1u),
		4u
	};

	xrtClearError();
	testRequire(
		(xrtWsConnAsyncBytes(pWrapping) == 0) &&
		(xrtWsConnAsyncCount(pWrapping) == 0) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_CONN_ERROR_ARGUMENT),
		"WebSocket Future queries accepted a wrapping Connection"
	);
	xrtClearError();
	testRequire(
		(xrtWsConnWaitAsync(
			pWrapping,
			XWS_CONN_WAIT_DRAIN
		 ) == NULL) &&
		(xrtWsConnTextAsync(
			pWrapping,
			XRT_STR_LITERAL("x")
		 ) == NULL) &&
		(xrtWsConnPingAsync(
			pWrapping,
			XRT_BYTES_LITERAL("x")
		 ) == NULL) &&
		(xrtWsConnCloseAsync(
			pWrapping,
			XWS_CLOSE_NORMAL,
			XRT_STR_LITERAL("x")
		 ) == NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_CONN_ERROR_ARGUMENT),
		"WebSocket Future accepted a wrapping Connection"
	);
	xrtClearError();
	testRequire(
		(xrtWsConnBinaryAsync(
			pConnection,
			Wrapping
		 ) == NULL) &&
		(xrtWsConnPingAsync(
			pConnection,
			Wrapping
		 ) == NULL) &&
		(xrtWsConnCloseAsync(
			pConnection,
			XWS_CLOSE_NORMAL,
			(xstrview) {
				(const char*)Wrapping.Data,
				Wrapping.Size
			}
		 ) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtWsConnAsyncCount(pConnection) == 0) &&
		(xrtWsConnAsyncBytes(pConnection) == 0),
		"WebSocket Future accepted a wrapping payload range"
	);
	xrtClearError();
	testRequire(
		(xrtWsConnWaitAsync(
			pConnection,
			(xwsconnwait)0
		 ) == NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_CONN_ERROR_ARGUMENT),
		"WebSocket Future accepted an invalid wait condition"
	);
	xrtClearError();
}



#if defined(TEST_WS_FUTURE_REF)
/* 在后续 Ref 已发送但尚未完成 Promise 时冻结所属 Worker。 */
static void testWsFutureBarrierRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	testwsfuturegate* pGate = (testwsfuturegate*)pContext;

	(void)iSize;
	xrtAtomic32Store(
		&pGate->Blocked,
		1,
		XMEMORY_RELEASE
	);
	while ( xrtAtomic32Load(
		&pGate->Release,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
	free((ptr)pData);
}



/* 验证 WRITE 屏障只覆盖此前发送，后续发送不能反向延长等待。 */
static void testWsFutureBarrier(
	testwsfuture* pTest,
	xwsconn* pServer
)
{
	testwsfuturegate WorkerGate;
	testwsfuturegate ReleaseGate;
	xnetref Ref;
	xfuture* pBefore;
	xfuture* pBarrier;
	xfuture* pAfter;
	bytes pPayload = (bytes)malloc(1u);

	testRequire(
		pPayload != NULL,
		"WebSocket Future barrier payload allocation failed"
	);
	pPayload[0] = UINT8_C(0x5A);
	xrtAtomic32Init(&ReleaseGate.Blocked, 0);
	xrtAtomic32Init(&ReleaseGate.Release, 0);
	testWsFutureGatePost(pTest, pServer, &WorkerGate);
	testWsFutureGateWait(
		&WorkerGate,
		"WebSocket Future barrier worker gate did not block"
	);
	pBefore = xrtWsConnTextAsync(
		pServer,
		XRT_STR_LITERAL("before")
	);
	pBarrier = xrtWsConnWaitAsync(
		pServer,
		XWS_CONN_WAIT_WRITE
	);
	Ref = (xnetref) {
		pPayload,
		1u,
		testWsFutureBarrierRelease,
		&ReleaseGate
	};
	pAfter = xrtWsConnBinaryRefAsync(pServer, &Ref);
	testRequire(
		(pBefore != NULL) &&
		(pBarrier != NULL) &&
		(pAfter != NULL) &&
		(xrtWsConnAsyncCount(pServer) ==
		 TEST_WS_FUTURE_BARRIER_COUNT),
		"WebSocket Future barrier queue admission failed"
	);
	testWsFutureGateRelease(&WorkerGate);
	testWsFutureGateWait(
		&ReleaseGate,
		"WebSocket Future post-barrier Ref release did not block"
	);
	testRequire(
		(xrtFutureState(pBefore) == XFUTURE_RESOLVED) &&
		(xrtFutureState(pBarrier) == XFUTURE_RESOLVED) &&
		(xrtFutureState(pAfter) == XFUTURE_PENDING),
		"WebSocket Future allowed a later send to extend its barrier"
	);
	testWsFutureGateRelease(&ReleaseGate);
	testWsFutureState(
		pAfter,
		XFUTURE_RESOLVED,
		"WebSocket Future post-barrier send did not resolve"
	);
	testRequire(
		(xrtWsConnAsyncCount(pServer) == 0) &&
		(xrtWsConnAsyncBytes(pServer) == 0),
		"WebSocket Future barrier leaked queue budget"
	);
	xrtFutureDestroy(pBefore);
	xrtFutureDestroy(pBarrier);
	xrtFutureDestroy(pAfter);
}
#endif



#if defined(TEST_WS_FUTURE_REF)
/* 非法 Ref 测试如果错误接管所有权，只记录回调而不访问边界地址。 */
static void testWsFutureBoundaryRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	xatomic32* pReleased = (xatomic32*)pContext;

	(void)pData;
	(void)iSize;
	(void)xrtAtomic32FetchAdd(
		pReleased,
		1,
		XMEMORY_RELEASE
	);
}



/* 释放所有权 Future 接管的测试负载，并发布唯一释放计数。 */
static void testWsFutureRefRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	testwsfuture* pTest = (testwsfuture*)pContext;

	(void)iSize;
	free((ptr)pData);
	(void)xrtAtomic32FetchAdd(
		&pTest->RefReleases,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证 Ref Future 的接管点、成功释放与取消释放都恰好发生一次。 */
static void testWsFutureReferences(
	testwsfuture* pTest,
	xwsconn* pClient,
	xwsconn* pServer
)
{
	static const uint8 InvalidText[] = { UINT8_C(0xC0) };
	uint8 RefStorage[sizeof(xnetref) + 2u];
	xatomic32 BoundaryRelease;
	xnetref Ref;
	xfuture* pFuture;
	bytes pPayload;
	uint32 iMessages = xrtAtomic32Load(
		&pTest->Messages,
		XMEMORY_ACQUIRE
	);

	xrtAtomic32Init(&BoundaryRelease, 0);
	Ref = (xnetref) {
		(cbytes)(uintptr_t)(UINTPTR_MAX - 1u),
		4u,
		testWsFutureBoundaryRelease,
		&BoundaryRelease
	};
	testRequire(
		(xrtWsConnBinaryRefAsync(
			pClient,
			(const xnetref*)(uintptr_t)(UINTPTR_MAX - 1u)
		 ) == NULL) &&
		(xrtWsConnBinaryRefAsync(
			pClient,
			(const xnetref*)(const void*)pClient
		 ) == NULL) &&
		(xrtWsConnBinaryRefAsync(pClient, &Ref) == NULL) &&
		(xrtAtomic32Load(
			&BoundaryRelease,
			XMEMORY_ACQUIRE
		 ) == 0),
		"WebSocket Ref Future accepted invalid reference ranges"
	);
	xrtClearError();
	Ref = (xnetref) {
		(cbytes)(const void*)pClient,
		1u,
		testWsFutureBoundaryRelease,
		&BoundaryRelease
	};
	testRequire(
		(xrtWsConnBinaryRefAsync(pClient, &Ref) == NULL) &&
		(xrtAtomic32Load(
			&BoundaryRelease,
			XMEMORY_ACQUIRE
		 ) == 0),
		"WebSocket Ref Future accepted Connection-backed ownership"
	);
	xrtClearError();

	Ref = (xnetref) {
		InvalidText,
		sizeof(InvalidText),
		testWsFutureRefRelease,
		pTest
	};
	testRequire(
		(xrtWsConnTextRefAsync(pClient, &Ref) == NULL) &&
		(xrtAtomic32Load(
			&pTest->RefReleases,
			XMEMORY_ACQUIRE
		 ) == 0),
		"WebSocket rejected Ref Future transferred ownership"
	);
	xrtClearError();

	#if defined(TEST_WS_FUTURE_REF_OOM)
		pPayload = (bytes)malloc(4u);
		testRequire(
			pPayload != NULL,
			"WebSocket Ref Future OOM payload allocation failed"
		);
		memcpy(pPayload, "oom!", 4u);
		Ref = (xnetref) {
			pPayload,
			4u,
			testWsFutureRefRelease,
			pTest
		};
		testRequire(
			xrtMemDebugFailAfter(0),
			"WebSocket Ref Future OOM fault setup failed"
		);
		pFuture = xrtWsConnBinaryRefAsync(pClient, &Ref);
		testRequire(
			(pFuture == NULL) &&
			xrtMemDebugFailTriggered() &&
			(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
			(xrtAtomic32Load(
				&pTest->RefReleases,
				XMEMORY_ACQUIRE
			 ) == 0) &&
			(xrtWsConnAsyncCount(pClient) == 0) &&
			(xrtWsConnAsyncBytes(pClient) == 0),
			"WebSocket Ref Future OOM transferred ownership or budget"
		);
		xrtMemDebugFailClear();
		xrtClearError();
		free(pPayload);
	#endif

	pPayload = (bytes)malloc(5u);
	testRequire(
		pPayload != NULL,
		"WebSocket Ref Future payload allocation failed"
	);
	memcpy(pPayload, "owned", 5u);
	Ref = (xnetref) {
		pPayload,
		5u,
		testWsFutureRefRelease,
		pTest
	};
	pFuture = xrtWsConnTextRefAsync(pClient, &Ref);
	testWsFutureState(
		pFuture,
		XFUTURE_RESOLVED,
		"WebSocket Ref Future did not resolve"
	);
	xrtFutureDestroy(pFuture);
	testWsFutureWaitAtomic(
		&pTest->RefReleases,
		1,
		"WebSocket Ref Future payload was not released"
	);
	testWsFutureWaitAtomic(
		&pTest->Messages,
		iMessages + 1u,
		"WebSocket Ref Future message did not arrive"
	);

	pPayload = (bytes)malloc(1u);
	testRequire(
		pPayload != NULL,
		"WebSocket unaligned Ref Future allocation failed"
	);
	pPayload[0] = UINT8_C(0x5A);
	Ref = (xnetref) {
		pPayload,
		1u,
		testWsFutureRefRelease,
		pTest
	};
	memset(RefStorage, 0xA5, sizeof(RefStorage));
	memcpy(RefStorage + 1u, &Ref, sizeof(Ref));
	pFuture = xrtWsConnBinaryRefAsync(
		pServer,
		(const xnetref*)(const void*)(RefStorage + 1u)
	);
	testWsFutureState(
		pFuture,
		XFUTURE_RESOLVED,
		"WebSocket unaligned Ref Future did not resolve"
	);
	xrtFutureDestroy(pFuture);
	testWsFutureWaitAtomic(
		&pTest->RefReleases,
		2,
		"WebSocket unaligned Ref Future payload was not released"
	);
	testRequire(
		(RefStorage[0] == UINT8_C(0xA5)) &&
		(RefStorage[sizeof(RefStorage) - 1u] == UINT8_C(0xA5)),
		"WebSocket unaligned Ref Future changed guard bytes"
	);

	testWsFutureBlockClient(pTest, pClient);
	pPayload = (bytes)malloc(6u);
	testRequire(
		pPayload != NULL,
		"WebSocket cancelled Ref Future allocation failed"
	);
	memcpy(pPayload, "cancel", 6u);
	Ref = (xnetref) {
		pPayload,
		6u,
		testWsFutureRefRelease,
		pTest
	};
	pFuture = xrtWsConnBinaryRefAsync(pClient, &Ref);
	testRequire(
		(pFuture != NULL) && xrtFutureCancel(pFuture),
		"WebSocket Ref Future cancellation failed"
	);
	xrtAtomic32Store(
		&pTest->Release,
		1,
		XMEMORY_RELEASE
	);
	testWsFutureState(
		pFuture,
		XFUTURE_CANCELLED,
		"WebSocket Ref Future cancellation state mismatch"
	);
	xrtFutureDestroy(pFuture);
	testWsFutureWaitAtomic(
		&pTest->RefReleases,
		3,
		"WebSocket cancelled Ref Future payload was not released"
	);
	testRequire(
		(xrtAtomic32Load(
			&pTest->Messages,
			XMEMORY_ACQUIRE
		 ) == (iMessages + 1u)) &&
		(xrtWsConnAsyncCount(pClient) == 0) &&
		(xrtWsConnAsyncBytes(pClient) == 0),
		"WebSocket cancelled Ref Future leaked data or budget"
	);
}
#endif



/* 验证永久容量、异步字节上限和 OOM 都在发布前原子失败。 */
static void testWsFutureAdmissionFailures(
	testwsfuture* pTest,
	xwsconn* pClient
)
{
	size_t iPermanent =
		TEST_WS_FUTURE_TCP_LIMIT - 256u;
	size_t iAsync =
		TEST_WS_FUTURE_ASYNC_BYTES + 1u;
	bytes pPayload = (bytes)malloc(iPermanent);
	xfuture* pFuture;
	uint64 iAttempts;

	testRequire(
		pPayload != NULL,
		"WebSocket Future admission payload allocation failed"
	);
	memset(pPayload, 0x5a, iPermanent);
	xrtClearError();
	pFuture = xrtWsConnBinaryAsync(
		pClient,
		(xbytesview) {
			pPayload,
			iPermanent
		}
	);
	testRequire(
		(pFuture == NULL) &&
		(xrtErrorKind(xrtGetError()) ==
		 XERR_RANGE) &&
		(strcmp(
			xrtErrorOperation(xrtGetError()),
			"send-websocket-message"
		 ) == 0) &&
		(xrtWsConnAsyncCount(pClient) == 0) &&
		(xrtWsConnAsyncBytes(pClient) == 0),
		"WebSocket Future accepted a frame larger than TCP capacity"
	);

	xrtClearError();
	pFuture = xrtWsConnBinaryAsync(
		pClient,
		(xbytesview) {
			pPayload,
			iAsync
		}
	);
	testRequire(
		(pFuture == NULL) &&
		(xrtErrorKind(xrtGetError()) ==
		 XERR_RANGE) &&
		(strcmp(
			xrtErrorOperation(xrtGetError()),
			"submit-websocket-async"
		 ) == 0) &&
		(xrtWsConnAsyncCount(pClient) == 0) &&
		(xrtWsConnAsyncBytes(pClient) == 0),
		"WebSocket Future asynchronous byte limit failed"
	);

	iAttempts = xrtAtomic64Load(
		&pTest->AllocAttempts,
		XMEMORY_ACQUIRE
	);
	xrtClearError();
	xrtAtomic32Store(
		&pTest->AllocFail,
		1,
		XMEMORY_RELEASE
	);
	pFuture = xrtWsConnBinaryAsync(
		pClient,
		(xbytesview) {
			pPayload,
			TEST_WS_FUTURE_OOM_BYTES
		}
	);
	xrtAtomic32Store(
		&pTest->AllocFail,
		0,
		XMEMORY_RELEASE
	);
	testRequire(
		(pFuture == NULL) &&
		(xrtErrorKind(xrtGetError()) ==
		 XERR_MEMORY) &&
		(xrtAtomic64Load(
			&pTest->AllocAttempts,
			XMEMORY_ACQUIRE
		 ) > iAttempts) &&
		(xrtWsConnAsyncCount(pClient) == 0) &&
		(xrtWsConnAsyncBytes(pClient) == 0),
		"WebSocket Future OOM leaked admission budget"
	);
	xrtClearError();
	free(pPayload);
}



/* 服务端复制一条完整测试消息。 */
static void testWsFutureMessageData(
	xwsconn* pConnection,
	xbytesview Data,
	ptr pData
)
{
	testwsfuture* pTest = (testwsfuture*)pData;
	#if defined(TEST_WS_FUTURE_GROUP_OPERATION)
		uint32 iExpected;
	#endif

	(void)pConnection;
	#if defined(TEST_WS_FUTURE_GROUP_OPERATION)
		do {
			iExpected = 0;
			if ( !xrtAtomic32CompareExchange(
				&pTest->MessageLock,
				&iExpected,
				1,
				XMEMORY_ACQUIRE,
				XMEMORY_RELAXED
			) ) {
				xrtThreadYield();
			}
		} while ( iExpected != 0 );
	#endif
	testRequire(
		Data.Size <=
			(sizeof(pTest->Message) -
			 pTest->MessageSize),
		"WebSocket Future test message overflow"
	);
	if ( Data.Size != 0 ) {
		memcpy(
			pTest->Message + pTest->MessageSize,
			Data.Data,
			Data.Size
		);
	}
	pTest->MessageSize += Data.Size;
	#if defined(TEST_WS_FUTURE_GROUP_OPERATION)
		xrtAtomic32Store(
			&pTest->MessageLock,
			0,
			XMEMORY_RELEASE
		);
	#endif
}



/* 发布服务端完整消息计数。 */
static void testWsFutureMessageEnd(
	xwsconn* pConnection,
	ptr pData
)
{
	testwsfuture* pTest = (testwsfuture*)pData;
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
		size_t iWriterBase = (size_t)xrtAtomic64Load(
			&pTest->WriterMessageBase,
			XMEMORY_ACQUIRE
		);
	#endif

	(void)pConnection;
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
		if ( (pTest->MessageSize == (iWriterBase + 7u)) ||
			(pTest->MessageSize == (iWriterBase + 13u)) ) {
			(void)xrtAtomic32FetchAdd(
				&pTest->WriterMessages,
				1,
				XMEMORY_RELEASE
			);
		}
	#endif
	(void)xrtAtomic32FetchAdd(
		&pTest->Messages,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录客户端收到的自动 Pong。 */
static void testWsFuturePong(
	xwsconn* pConnection,
	xbytesview Payload,
	ptr pData
)
{
	testwsfuture* pTest = (testwsfuture*)pData;

	(void)pConnection;
	testRequire(
		(Payload.Size == 5) &&
		(memcmp(Payload.Data, "probe", 5) == 0),
		"WebSocket Future Pong payload mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pTest->Pong,
		1,
		XMEMORY_RELEASE
	);
}



/* 两端都必须完成正常 WebSocket 关闭握手。 */
static void testWsFutureClose(
	xwsconn* pConnection,
	const xwsconnclose* pClose,
	ptr pData
)
{
	testwsfuture* pTest = (testwsfuture*)pData;

	(void)pConnection;
	testRequire(
		(pClose != NULL) &&
		((pClose->Flags & XWS_CONN_CLOSE_CLEAN) != 0) &&
		(pClose->Transport == XNET_RESULT_OK),
		"WebSocket Future Close snapshot mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pTest->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* Future 测试不允许 Connection 发布错误事件。 */
static void testWsFutureError(
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	(void)pConnection;
	(void)pError;
	(void)pData;
	testRequire(
		false,
		"WebSocket Future emitted an unexpected error"
	);
}



/* 按本端角色接管开放 TCP Stream。 */
static xwsconn* testWsFutureAttach(
	xnetstream* pStream,
	xwsrole Role,
	testwsfuture* pTest
)
{
	xwsconnconfig Config;
	xwsconnevents Events;
	xwsconn* pConnection;
	#if defined(TEST_WS_FUTURE_DEFLATE)
		uint8 DeflateStorage[sizeof(xwsdeflate) + 2u];
		xwsdeflate Deflate;
	#endif

	xrtWsConnConfigInit(&Config);
	Config.Role = Role;
	Config.MessageLimit = TEST_WS_FUTURE_WS_LIMIT;
	Config.FrameLimit = TEST_WS_FUTURE_WS_LIMIT;
	Config.SendLimit = TEST_WS_FUTURE_WS_LIMIT;
	Config.ControlReserve = 512;
	Config.AsyncBytesLimit =
		TEST_WS_FUTURE_ASYNC_BYTES;
	Config.AsyncCountLimit =
		TEST_WS_FUTURE_ASYNC_COUNT;
	#if defined(TEST_WS_FUTURE_REF)
	if ( Role == XWS_ROLE_SERVER ) {
		Config.AsyncCountLimit =
			TEST_WS_FUTURE_BARRIER_COUNT;
	}
	#endif
	#if defined(TEST_WS_FUTURE_DEFLATE)
		xrtWsDeflateInit(&Config.Deflate);
		Config.DeflateEnabled = true;
	#endif
	memset(&Events, 0, sizeof(Events));
	if ( Role == XWS_ROLE_SERVER ) {
		Events.MessageData =
			testWsFutureMessageData;
		Events.MessageEnd =
			testWsFutureMessageEnd;
		Events.Pong = testWsFuturePong;
	} else {
		Events.Pong = testWsFuturePong;
	}
	Events.Error = testWsFutureError;
	Events.Close = testWsFutureClose;
	if ( Role == XWS_ROLE_CLIENT ) {
		Config.AsyncCountLimit = 0;
		testRequire(
			(xrtWsConnAttach(
				pStream,
				&Config,
				&Events,
				pTest
			 ) == NULL) &&
			(xrtErrorCode(xrtGetError()) ==
			 XWS_CONN_ERROR_CONFIG),
			"WebSocket Future accepted a zero operation limit"
		);
		xrtClearError();
		Config.AsyncCountLimit =
			TEST_WS_FUTURE_ASYNC_COUNT;
		Config.AsyncBytesLimit = 0;
		testRequire(
			(xrtWsConnAttach(
				pStream,
				&Config,
				&Events,
				pTest
			 ) == NULL) &&
			(xrtErrorCode(xrtGetError()) ==
			 XWS_CONN_ERROR_CONFIG),
			"WebSocket Future accepted a zero byte limit"
		);
		xrtClearError();
		Config.AsyncBytesLimit =
			TEST_WS_FUTURE_ASYNC_BYTES;
		Config.AsyncBatch = 0;
		testRequire(
			(xrtWsConnAttach(
				pStream,
				&Config,
				&Events,
				pTest
			 ) == NULL) &&
			(xrtErrorCode(xrtGetError()) ==
			 XWS_CONN_ERROR_CONFIG),
			"WebSocket Future accepted a zero drive batch"
		);
		xrtClearError();
		Config.AsyncBatch =
			XWS_CONN_ASYNC_BATCH_DEFAULT;
	}
	pConnection = xrtWsConnAttach(
		pStream,
		&Config,
		&Events,
		pTest
	);
	#if defined(TEST_WS_FUTURE_DEFLATE)
		if ( pConnection != NULL ) {
			memset(
				DeflateStorage,
				0xA5,
				sizeof(DeflateStorage)
			);
			testRequire(
				xrtWsConnDeflate(
					pConnection,
					(xwsdeflate*)(void*)(DeflateStorage + 1u)
				) &&
				(DeflateStorage[0] == 0xA5) &&
				(DeflateStorage[
					sizeof(DeflateStorage) - 1u
				 ] == 0xA5),
				"WebSocket Deflate query rejected unaligned output"
			);
			memcpy(
				&Deflate,
				DeflateStorage + 1u,
				sizeof(Deflate)
			);
			testRequire(
				(memcmp(
					&Deflate,
					&Config.Deflate,
					sizeof(Deflate)
				 ) == 0) &&
				!xrtWsConnDeflate(
					pConnection,
					(xwsdeflate*)(uintptr_t)(UINTPTR_MAX - 1u)
				) &&
				!xrtWsConnDeflate(
					pConnection,
					(xwsdeflate*)(void*)pConnection
				) &&
				(xrtWsConnState(pConnection) == XWS_CONN_OPEN),
				"WebSocket Deflate query accepted invalid output storage"
			);
			xrtClearError();
		}
	#endif
	return pConnection;
}



/* Listener 在自己的 Worker 上接管服务端 Stream。 */
static bool testWsFutureAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testwsfuture* pTest = (testwsfuture*)pData;
	xwsconn* pConnection;

	(void)pListener;
	testRequire(
		xrtNetStreamWriteLimit(pStream) ==
		TEST_WS_FUTURE_TCP_LIMIT,
		"WebSocket Future accepted TCP write limit mismatch"
	);
	pConnection = testWsFutureAttach(
		pStream,
		XWS_ROLE_SERVER,
		pTest
	);
	testRequire(
		pConnection != NULL,
		"WebSocket Future server attach failed"
	);
	xrtAtomicPtrStore(
		&pTest->Server,
		pConnection,
		XMEMORY_RELEASE
	);
	return true;
}



/* 客户端 Connect 完成后把 Stream 所有权交给 Connection。 */
static void testWsFutureOpen(
	xnetstream* pStream,
	ptr pData
)
{
	testwsfuture* pTest = (testwsfuture*)pData;
	xwsconn* pConnection;

	testRequire(
		xrtNetStreamWriteLimit(pStream) ==
		TEST_WS_FUTURE_TCP_LIMIT,
		"WebSocket Future client TCP write limit mismatch"
	);
	pConnection = testWsFutureAttach(
		pStream,
		XWS_ROLE_CLIENT,
		pTest
	);

	testRequire(
		pConnection != NULL,
		"WebSocket Future client attach failed"
	);
	xrtAtomicPtrStore(
		&pTest->Client,
		pConnection,
		XMEMORY_RELEASE
	);
}



/* 记录 Listener 唯一关闭事件。 */
static void testWsFutureListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	testwsfuture* pTest = (testwsfuture*)pData;

	(void)pListener;
	xrtAtomic32Store(
		&pTest->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 创建 TCP 对并在两个 Worker 上附加共享 WebSocket Connection。 */
static void testWsFutureConnect(
	testwsfuture* pTest
)
{
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents ClientEvents;
	xnetstreamconfig StreamConfig;
	xnetaddr Address;

	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&ClientEvents, 0, sizeof(ClientEvents));
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_WS_FUTURE_BACKEND;
	EngineConfig.Workers = 2;
	pTest->Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pTest->Engine != NULL) &&
		xrtNetEngineStart(pTest->Engine),
		"WebSocket Future engine start failed"
	);

	xrtNetListenConfigInit(&ListenConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ListenConfig.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"WebSocket Future listener address failed"
	);
	ListenConfig.Affinity = 0;
	ListenConfig.Stream.ReadSize = 64;
	ListenConfig.Stream.ReadLimit = 65536;
	ListenConfig.Stream.WriteLimit =
		TEST_WS_FUTURE_TCP_LIMIT;
	ListenConfig.Stream.WriteHighWater =
		TEST_WS_FUTURE_TCP_LIMIT / 2u;
	ListenConfig.Stream.WriteLowWater =
		TEST_WS_FUTURE_TCP_LIMIT / 8u;
	ListenerEvents.Accept = testWsFutureAccept;
	ListenerEvents.Close = testWsFutureListenerClose;
	pTest->Listener = xrtNetListen(
		pTest->Engine,
		&ListenConfig,
		&ListenerEvents,
		NULL,
		pTest
	);
	testRequire(
		(pTest->Listener != NULL) &&
		xrtNetListenerLocal(pTest->Listener, &Address),
		"WebSocket Future listener start failed"
	);

	xrtNetStreamConfigInit(&StreamConfig);
	StreamConfig.ReadSize = 64;
	StreamConfig.ReadLimit = 65536;
	StreamConfig.WriteLimit =
		TEST_WS_FUTURE_TCP_LIMIT;
	StreamConfig.WriteHighWater =
		TEST_WS_FUTURE_TCP_LIMIT / 2u;
	StreamConfig.WriteLowWater =
		TEST_WS_FUTURE_TCP_LIMIT / 8u;
	ClientEvents.Open = testWsFutureOpen;
	testRequire(
		xrtNetStreamConnect(
			pTest->Engine,
			&Address,
			1,
			&StreamConfig,
			&ClientEvents,
			pTest
		) != NULL,
		"WebSocket Future client connect failed"
	);
}



#if defined(TEST_WS_FUTURE_GROUP)
/* 在独立线程中反复加入和移除同一个 Connection。 */
static int32 testWsFutureGroupStressProc(ptr pData)
{
	testwsfuturegroupstress* pStress =
		(testwsfuturegroupstress*)pData;

	for ( size_t i = 0; i < pStress->Rounds; i++ ) {
		xwsgroupsnapshot* pSnapshot;
		size_t iCount;

		if ( !xrtWsGroupAdd(
			pStress->Group,
			pStress->Connection
		) ) {
			(void)xrtAtomic32FetchAdd(
				pStress->Failed,
				1,
				XMEMORY_RELAXED
			);
			xrtClearError();
		}
		(void)xrtWsGroupRemove(
			pStress->Group,
			pStress->Connection
		);

		/* 周期性快照验证锁外分配重试与并发成员线性化。 */
		if ( (i & 31u) != 0 ) {
			continue;
		}
		pSnapshot = xrtWsGroupSnapshotCreate(pStress->Group);
		if ( pSnapshot == NULL ) {
			(void)xrtAtomic32FetchAdd(
				pStress->Failed,
				1,
				XMEMORY_RELAXED
			);
			xrtClearError();
			continue;
		}
		iCount = xrtWsGroupSnapshotCount(pSnapshot);
		if ( (iCount > 1u) ||
			((iCount == 1u) &&
			 (xrtWsGroupSnapshotGet(pSnapshot, 0) !=
			  pStress->Connection)) ) {
			(void)xrtAtomic32FetchAdd(
				pStress->Failed,
				1,
				XMEMORY_RELAXED
			);
		}
		xrtWsGroupSnapshotDestroy(pSnapshot);
	}
	return 0;
}



/* 验证多线程高争用不会产生重复成员或遗留引用。 */
static void testWsFutureGroupStress(xwsconn* pConnection)
{
	enum {
		TEST_WS_GROUP_STRESS_THREADS = 8,
		TEST_WS_GROUP_STRESS_ROUNDS = 1000
	};

	xwsgroup* pGroup = xrtWsGroupCreate(1u);
	testwsfuturegroupstress Stress[TEST_WS_GROUP_STRESS_THREADS];
	xthread* Threads[TEST_WS_GROUP_STRESS_THREADS];
	xatomic32 Failed;

	testRequire(
		pGroup != NULL,
		"WebSocket group stress creation failed"
	);
	memset(Stress, 0, sizeof(Stress));
	memset(Threads, 0, sizeof(Threads));
	xrtAtomic32Init(&Failed, 0);
	for ( size_t i = 0; i < TEST_WS_GROUP_STRESS_THREADS; i++ ) {
		Stress[i].Group = pGroup;
		Stress[i].Connection = pConnection;
		Stress[i].Failed = &Failed;
		Stress[i].Rounds = TEST_WS_GROUP_STRESS_ROUNDS;
		Threads[i] = xrtThreadCreate(
			testWsFutureGroupStressProc,
			&Stress[i],
			0
		);
		testRequire(
			Threads[i] != NULL,
			"WebSocket group stress thread creation failed"
		);
	}
	for ( size_t i = 0; i < TEST_WS_GROUP_STRESS_THREADS; i++ ) {
		testRequire(
			xrtThreadWait(Threads[i]) == XWAIT_OK,
			"WebSocket group stress thread wait failed"
		);
		xrtThreadDestroy(Threads[i]);
	}
	testRequire(
		(xrtAtomic32Load(&Failed, XMEMORY_ACQUIRE) == 0) &&
		(xrtWsGroupCount(pGroup) == 0),
		"WebSocket group stress lost uniqueness or membership"
	);
	xrtWsGroupDestroy(pGroup);
}



/* 验证连接组的唯一成员、硬上限、封闭状态和独立快照。 */
static void testWsFutureGroup(xwsconn* pClient, xwsconn* pServer)
{
	xwsgroup* pGroup;
	xwsgroupsnapshot* pSnapshot;
	xwsgroup* pWrappingGroup =
		(xwsgroup*)(uintptr_t)(UINTPTR_MAX - 1u);
	xwsgroupsnapshot* pWrappingSnapshot =
		(xwsgroupsnapshot*)(uintptr_t)(UINTPTR_MAX - 1u);
	xwsconn* pWrappingConnection =
		(xwsconn*)(uintptr_t)(UINTPTR_MAX - 1u);

	#if defined(TEST_WS_FUTURE_GROUP_OOM)
		testRequire(
			xrtMemDebugFailAfter(0),
			"WebSocket group create OOM setup failed"
		);
		pGroup = xrtWsGroupCreate(1u);
		testRequire(
			(pGroup == NULL) &&
			xrtMemDebugFailTriggered() &&
			(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
			(xrtErrorCode(xrtGetError()) == XWS_GROUP_ERROR_MEMORY),
			"WebSocket group create OOM published partial state"
		);
		xrtMemDebugFailClear();
		xrtClearError();
	#endif

	pGroup = xrtWsGroupCreate(1u);
	testRequire(
		(pGroup != NULL) &&
		(xrtWsGroupRef(pWrappingGroup) == NULL) &&
		!xrtWsGroupAdd(pWrappingGroup, pClient) &&
		!xrtWsGroupRemove(pWrappingGroup, pClient) &&
		!xrtWsGroupHas(pWrappingGroup, pClient) &&
		(xrtWsGroupCount(pWrappingGroup) == 0) &&
		(xrtWsGroupLimit(pWrappingGroup) == 0) &&
		!xrtWsGroupSeal(pWrappingGroup) &&
		!xrtWsGroupSealed(pWrappingGroup) &&
		(xrtWsGroupClear(pWrappingGroup) == 0) &&
		(xrtWsGroupSnapshotCreate(pWrappingGroup) == NULL) &&
		(xrtWsGroupSnapshotCount(pWrappingSnapshot) == 0) &&
		(xrtWsGroupSnapshotGet(
			pWrappingSnapshot,
			0
		 ) == NULL) &&
		!xrtWsGroupAdd(pGroup, pWrappingConnection) &&
		!xrtWsGroupRemove(pGroup, pWrappingConnection) &&
		!xrtWsGroupHas(pGroup, pWrappingConnection) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_GROUP_ERROR_ARGUMENT),
		"WebSocket group accepted a wrapping object range"
	);
	xrtWsGroupSnapshotDestroy(pWrappingSnapshot);
	xrtWsGroupDestroy(pWrappingGroup);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_GROUP_ERROR_ARGUMENT),
		"WebSocket group destroy accepted a wrapping object range"
	);
	xrtClearError();
	#if defined(TEST_WS_FUTURE_GROUP_OOM)
		testRequire(
			xrtMemDebugFailAfter(0),
			"WebSocket group add OOM setup failed"
		);
		testRequire(
			!xrtWsGroupAdd(pGroup, pClient) &&
			xrtMemDebugFailTriggered() &&
			(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
			(xrtErrorCode(xrtGetError()) == XWS_GROUP_ERROR_MEMORY) &&
			(xrtWsGroupCount(pGroup) == 0),
			"WebSocket group add OOM changed membership"
		);
		xrtMemDebugFailClear();
		xrtClearError();
	#endif

	testRequire(
		(pGroup != NULL) &&
		(xrtWsGroupLimit(pGroup) == 1u) &&
		xrtWsGroupAdd(pGroup, pClient) &&
		xrtWsGroupAdd(pGroup, pClient) &&
		(xrtWsGroupCount(pGroup) == 1u) &&
		xrtWsGroupHas(pGroup, pClient),
		"WebSocket group unique membership failed"
	);
	testRequire(
		!xrtWsGroupAdd(pGroup, pServer) &&
		(xrtErrorKind(xrtGetError()) == XERR_AGAIN) &&
		(xrtErrorCode(xrtGetError()) == XWS_GROUP_ERROR_CAPACITY),
		"WebSocket group hard member limit failed"
	);
	xrtClearError();

	#if defined(TEST_WS_FUTURE_GROUP_OOM)
		testRequire(
			xrtMemDebugFailAfter(0),
			"WebSocket group snapshot OOM setup failed"
		);
		pSnapshot = xrtWsGroupSnapshotCreate(pGroup);
		testRequire(
			(pSnapshot == NULL) &&
			xrtMemDebugFailTriggered() &&
			(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
			(xrtErrorCode(xrtGetError()) == XWS_GROUP_ERROR_MEMORY) &&
			(xrtWsGroupCount(pGroup) == 1u) &&
			xrtWsGroupHas(pGroup, pClient),
			"WebSocket group snapshot OOM changed membership"
		);
		xrtMemDebugFailClear();
		xrtClearError();
	#endif

	pSnapshot = xrtWsGroupSnapshotCreate(pGroup);
	testRequire(
		(pSnapshot != NULL) &&
		(xrtWsGroupSnapshotCount(pSnapshot) == 1u) &&
		(xrtWsGroupSnapshotGet(pSnapshot, 0) == pClient),
		"WebSocket group snapshot did not preserve membership"
	);
	testRequire(
		(xrtWsGroupSnapshotGet(pSnapshot, 1) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"WebSocket group snapshot accepted an invalid index"
	);
	xrtClearError();

	testRequire(
		xrtWsGroupRemove(pGroup, pClient) &&
		!xrtWsGroupRemove(pGroup, pClient) &&
		(xrtWsGroupCount(pGroup) == 0) &&
		xrtWsGroupAdd(pGroup, pServer) &&
		xrtWsGroupSeal(pGroup) &&
		xrtWsGroupSeal(pGroup) &&
		xrtWsGroupSealed(pGroup),
		"WebSocket group removal or sealing failed"
	);
	testRequire(
		xrtWsGroupAdd(pGroup, pServer),
		"WebSocket group rejected an existing member after seal"
	);
	testRequire(
		xrtWsGroupRemove(pGroup, pServer) &&
		!xrtWsGroupAdd(pGroup, pClient) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtErrorCode(xrtGetError()) == XWS_GROUP_ERROR_STATE) &&
		(xrtWsGroupClear(pGroup) == 0),
		"WebSocket sealed group accepted a new member"
	);
	xrtClearError();
	xrtWsGroupDestroy(pGroup);

	testRequire(
		(xrtWsGroupSnapshotCount(pSnapshot) == 1u) &&
		(xrtWsGroupSnapshotGet(pSnapshot, 0) == pClient),
		"WebSocket group snapshot depended on group lifetime"
	);
	xrtWsGroupSnapshotDestroy(pSnapshot);
	testWsFutureGroupStress(pClient);
}
#endif



#if defined(TEST_WS_FUTURE_GROUP_OPERATION)
/* 共享 Ref 的最后一个成员引用归还后释放测试负载。 */
static void testWsFutureGroupRefRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	xatomic32* pReleased = (xatomic32*)pContext;

	(void)iSize;
	free((ptr)pData);
	(void)xrtAtomic32FetchAdd(
		pReleased,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证广播的部分接纳、共享所有权、逐项结果、Drain 和取消契约。 */
static void testWsFutureGroupOperation(
	testwsfuture* pTest,
	xwsconn* pClient,
	xwsconn* pServer
)
{
	enum {
		TEST_WS_GROUP_OPERATION_STRESS = 128
	};

	xwsgroup* pGroup = xrtWsGroupCreate(2u);
	xwsgroup* pSingle;
	xwsgroupop* pOperation;
	xwsgroupop* Operations[TEST_WS_GROUP_OPERATION_STRESS];
	xwsgroupopresult Result;
	uint8 ResultStorage[sizeof(xwsgroupopresult) + 2u];
	uint8 RefStorage[sizeof(xnetref) + 2u];
	xfuture* pFuture;
	xfuture* pFirst;
	xfuture* pSecond;
	xnetref Ref;
	bytes pPayload;
	xatomic32 Released;
	xwsgroupop* pWrappingOperation =
		(xwsgroupop*)(uintptr_t)(UINTPTR_MAX - 1u);
	uint32 iMessages;
	size_t iStressAccepted = 0;

	testRequire(
		(pGroup != NULL) &&
		xrtWsGroupAdd(pGroup, pClient) &&
		xrtWsGroupAdd(pGroup, pServer),
		"WebSocket group operation members failed"
	);
	testRequire(
		(xrtWsGroupOpRef(pWrappingOperation) == NULL) &&
		(xrtWsGroupOpCount(pWrappingOperation) == 0) &&
		(xrtWsGroupOpAccepted(pWrappingOperation) == 0) &&
		(xrtWsGroupOpRejected(pWrappingOperation) == 0) &&
		(xrtWsGroupOpDoneCount(pWrappingOperation) == 0) &&
		!xrtWsGroupOpResult(
			pWrappingOperation,
			0,
			&Result
		) &&
		(xrtWsGroupOpItemFutureRef(
			pWrappingOperation,
			0
		 ) == NULL) &&
		(xrtWsGroupOpFutureRef(pWrappingOperation) == NULL) &&
		(xrtWsGroupOpCancel(pWrappingOperation) == 0) &&
		(xrtWsGroupOpWait(pWrappingOperation) == XWAIT_ERROR) &&
		(xrtWsGroupOpWaitFor(
			pWrappingOperation,
			1u
		 ) == XWAIT_ERROR) &&
		(xrtWsGroupOpWaitUntil(
			pWrappingOperation,
			0
		 ) == XWAIT_ERROR) &&
		(xrtWsGroupOpWaitUntilCancel(
			pWrappingOperation,
			0,
			NULL
		 ) == XWAIT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_GROUP_ERROR_ARGUMENT),
		"WebSocket group operation accepted a wrapping range"
	);
	xrtWsGroupOpDestroy(pWrappingOperation);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_GROUP_ERROR_ARGUMENT),
		"WebSocket group operation destroy accepted a wrapping range"
	);
	xrtClearError();
	testRequire(
		(xrtWsGroupBinaryAsync(
			pGroup,
			(xbytesview) {
				(cbytes)(uintptr_t)(UINTPTR_MAX - 1u),
				8u
			}
		 ) == NULL) &&
		(xrtWsGroupPingAsync(
			pGroup,
			(xbytesview) {
				(cbytes)(uintptr_t)(UINTPTR_MAX - 1u),
				8u
			}
		 ) == NULL) &&
		(xrtWsGroupPongAsync(
			pGroup,
			(xbytesview) {
				(cbytes)(uintptr_t)(UINTPTR_MAX - 1u),
				8u
			}
		 ) == NULL) &&
		(xrtWsGroupBinaryRefAsync(
			pGroup,
			(const xnetref*)(uintptr_t)(UINTPTR_MAX - 1u)
		 ) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_GROUP_ERROR_ARGUMENT),
		"WebSocket group send accepted a wrapping range"
	);
	xrtAtomic32Init(&Released, 0);
	Ref = (xnetref) {
		(cbytes)(uintptr_t)(UINTPTR_MAX - 1u),
		8u,
		testWsFutureGroupRefRelease,
		&Released
	};
	testRequire(
		(xrtWsGroupBinaryRefAsync(pGroup, &Ref) == NULL) &&
		(xrtAtomic32Load(&Released, XMEMORY_ACQUIRE) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"WebSocket group failure transferred an invalid Ref"
	);
	xrtClearError();
	iMessages = xrtAtomic32Load(
		&pTest->Messages,
		XMEMORY_ACQUIRE
	);
	pOperation = xrtWsGroupTextAsync(
		pGroup,
		XRT_STR_LITERAL("g")
	);
	testRequire(
		(pOperation != NULL) &&
		(xrtWsGroupOpCount(pOperation) == 2u) &&
		(xrtWsGroupOpAccepted(pOperation) == 2u) &&
		(xrtWsGroupOpRejected(pOperation) == 0) &&
		(xrtWsGroupOpWaitFor(
			pOperation,
			UINT64_C(10000000)
		 ) == XWAIT_OK) &&
		(xrtWsGroupOpDoneCount(pOperation) == 2u),
		"WebSocket group broadcast completion failed"
	);
	memset(ResultStorage, 0xA5, sizeof(ResultStorage));
	testRequire(
		!xrtWsGroupOpResult(
			pOperation,
			0,
			(xwsgroupopresult*)(void*)pOperation
		) &&
		!xrtWsGroupOpResult(
			pOperation,
			0,
			(xwsgroupopresult*)(uintptr_t)(UINTPTR_MAX - 1u)
		) &&
		xrtWsGroupOpResult(
			pOperation,
			0,
			(xwsgroupopresult*)(void*)(ResultStorage + 1u)
		) &&
		(ResultStorage[0] == 0xA5) &&
		(ResultStorage[sizeof(ResultStorage) - 1u] == 0xA5),
		"WebSocket group operation result range contract failed"
	);
	memcpy(&Result, ResultStorage + 1u, sizeof(Result));
	testRequire(
		(Result.State == XWS_GROUP_OP_RESOLVED) &&
		(Result.Error == NULL),
		"WebSocket group operation unaligned result mismatch"
	);
	xrtClearError();
	for ( size_t i = 0; i < 2u; i++ ) {
		testRequire(
			xrtWsGroupOpResult(pOperation, i, &Result) &&
			(Result.State == XWS_GROUP_OP_RESOLVED) &&
			(Result.Error == NULL),
			"WebSocket group broadcast item failed"
		);
	}
	pFuture = xrtWsGroupOpFutureRef(pOperation);
	testRequire(
		(pFuture != NULL) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED),
		"WebSocket group completion Future mismatch"
	);
	xrtFutureDestroy(pFuture);
	pFuture = xrtWsGroupOpItemFutureRef(pOperation, 0);
	testRequire(
		(pFuture != NULL) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED),
		"WebSocket group item Future mismatch"
	);
	xrtFutureDestroy(pFuture);
	xrtWsGroupOpDestroy(pOperation);
	testWsFutureWaitAtomic(
		&pTest->Messages,
		iMessages + 1u,
		"WebSocket group broadcast messages missing"
	);

	/* 提交线程登记双成员操作时，两个 Worker 可并发完成前序槽位。 */
	memset(Operations, 0, sizeof(Operations));
	for ( size_t i = 0; i < TEST_WS_GROUP_OPERATION_STRESS; i++ ) {
		Operations[i] = xrtWsGroupTextAsync(
			pGroup,
			XRT_STR_LITERAL("")
		);
		testRequire(
			(Operations[i] != NULL) &&
			((xrtWsGroupOpAccepted(Operations[i]) +
			  xrtWsGroupOpRejected(Operations[i])) == 2u),
			"WebSocket group concurrent completion submission failed"
		);
		iStressAccepted += xrtWsGroupOpAccepted(Operations[i]);
	}
	for ( size_t i = 0; i < TEST_WS_GROUP_OPERATION_STRESS; i++ ) {
		testRequire(
			(xrtWsGroupOpWaitFor(
				Operations[i],
				UINT64_C(10000000)
			 ) == XWAIT_OK) &&
			(xrtWsGroupOpDoneCount(Operations[i]) == 2u),
			"WebSocket group concurrent completion count mismatch"
		);
		for ( size_t j = 0; j < 2u; j++ ) {
			testRequire(
				xrtWsGroupOpResult(Operations[i], j, &Result) &&
				((Result.State == XWS_GROUP_OP_RESOLVED) ||
				 ((Result.State == XWS_GROUP_OP_REJECTED) &&
				  (Result.Error != NULL) &&
				  (xrtErrorKind(Result.Error) == XERR_AGAIN))),
				"WebSocket group concurrent slot did not resolve"
			);
		}
		xrtWsGroupOpDestroy(Operations[i]);
	}
	testRequire(
		iStressAccepted != 0,
		"WebSocket group stress did not admit any member operation"
	);

	iMessages = xrtAtomic32Load(
		&pTest->Messages,
		XMEMORY_ACQUIRE
	);
	testWsFutureBlockClient(pTest, pClient);
	pFirst = xrtWsConnTextAsync(
		pClient,
		XRT_STR_LITERAL("")
	);
	pSecond = xrtWsConnTextAsync(
		pClient,
		XRT_STR_LITERAL("")
	);
	pOperation = xrtWsGroupTextAsync(
		pGroup,
		XRT_STR_LITERAL("")
	);
	testRequire(
		(pFirst != NULL) &&
		(pSecond != NULL) &&
		(pOperation != NULL) &&
		(xrtWsGroupOpAccepted(pOperation) == 1u) &&
		(xrtWsGroupOpRejected(pOperation) == 1u) &&
		xrtWsGroupOpResult(pOperation, 0, &Result) &&
		(Result.State == XWS_GROUP_OP_REJECTED) &&
		(Result.Error != NULL) &&
		(xrtErrorKind(Result.Error) == XERR_AGAIN),
		"WebSocket group partial admission was not reported"
	);
	xrtAtomic32Store(
		&pTest->Release,
		1,
		XMEMORY_RELEASE
	);
	testWsFutureState(
		pFirst,
		XFUTURE_RESOLVED,
		"WebSocket group first queue filler failed"
	);
	testWsFutureState(
		pSecond,
		XFUTURE_RESOLVED,
		"WebSocket group second queue filler failed"
	);
	testRequire(
		(xrtWsGroupOpWaitFor(
			pOperation,
			UINT64_C(10000000)
		 ) == XWAIT_OK) &&
		xrtWsGroupOpResult(pOperation, 1, &Result) &&
		(Result.State == XWS_GROUP_OP_RESOLVED),
		"WebSocket group accepted partial item failed"
	);
	testWsFutureWaitAtomic(
		&pTest->Messages,
		iMessages + 2u,
		"WebSocket group partial messages missing"
	);
	xrtFutureDestroy(pFirst);
	xrtFutureDestroy(pSecond);
	xrtWsGroupOpDestroy(pOperation);

	xrtAtomic32Init(&Released, 0);
	pPayload = (bytes)malloc(1u);
	testRequire(
		pPayload != NULL,
		"WebSocket group shared Ref payload allocation failed"
	);
	pPayload[0] = UINT8_C(0x5A);
	Ref = (xnetref) {
		pPayload,
		1u,
		testWsFutureGroupRefRelease,
		&Released
	};
	iMessages = xrtAtomic32Load(
		&pTest->Messages,
		XMEMORY_ACQUIRE
	);
	pOperation = xrtWsGroupBinaryRefAsync(pGroup, &Ref);
	testRequire(
		(pOperation != NULL) &&
		(xrtWsGroupOpAccepted(pOperation) == 2u) &&
		(xrtWsGroupOpWaitFor(
			pOperation,
			UINT64_C(10000000)
		 ) == XWAIT_OK),
		"WebSocket group shared Ref operation failed"
	);
	testWsFutureWaitAtomic(
		&Released,
		1,
		"WebSocket group shared Ref was not released exactly once"
	);
	testWsFutureWaitAtomic(
		&pTest->Messages,
		iMessages + 1u,
		"WebSocket group shared Ref messages missing"
	);
	xrtWsGroupOpDestroy(pOperation);

	xrtAtomic32Init(&Released, 0);
	pPayload = (bytes)malloc(1u);
	testRequire(
		pPayload != NULL,
		"WebSocket group unaligned Ref payload allocation failed"
	);
	pPayload[0] = UINT8_C(0x6B);
	Ref = (xnetref) {
		pPayload,
		1u,
		testWsFutureGroupRefRelease,
		&Released
	};
	memset(RefStorage, 0xA5, sizeof(RefStorage));
	memcpy(RefStorage + 1u, &Ref, sizeof(Ref));
	pOperation = xrtWsGroupBinaryRefAsync(
		pGroup,
		(const xnetref*)(const void*)(RefStorage + 1u)
	);
	testRequire(
		(pOperation != NULL) &&
		(xrtWsGroupOpWaitFor(
			pOperation,
			UINT64_C(10000000)
		 ) == XWAIT_OK) &&
		(RefStorage[0] == 0xA5) &&
		(RefStorage[sizeof(RefStorage) - 1u] == 0xA5),
		"WebSocket group rejected or modified an unaligned Ref"
	);
	testWsFutureWaitAtomic(
		&Released,
		1,
		"WebSocket group unaligned Ref was not released once"
	);
	xrtWsGroupOpDestroy(pOperation);

	pOperation = xrtWsGroupWaitAsync(
		pGroup,
		XWS_CONN_WAIT_DRAIN
	);
	testRequire(
		(pOperation != NULL) &&
		(xrtWsGroupOpWaitFor(
			pOperation,
			UINT64_C(10000000)
		 ) == XWAIT_OK) &&
		(xrtWsGroupOpDoneCount(pOperation) == 2u),
		"WebSocket group Drain operation failed"
	);
	xrtWsGroupOpDestroy(pOperation);

	pSingle = xrtWsGroupCreate(1u);
	testRequire(
		(pSingle != NULL) &&
		xrtWsGroupAdd(pSingle, pClient),
		"WebSocket single-member group failed"
	);
	testWsFutureBlockClient(pTest, pClient);
	pOperation = xrtWsGroupTextAsync(
		pSingle,
		XRT_STR_LITERAL("cancel")
	);
	pFuture = pOperation != NULL ?
		xrtWsGroupOpFutureRef(pOperation) : NULL;
	pFirst = pOperation != NULL ?
		xrtWsGroupOpItemFutureRef(pOperation, 0) : NULL;
	testRequire(
		(pOperation != NULL) &&
		(pFuture != NULL) &&
		(pFirst != NULL) &&
		xrtFutureCancel(pFuture) &&
		(xrtFutureState(pFuture) == XFUTURE_CANCELLED),
		"WebSocket group completion cancellation did not propagate"
	);
	xrtAtomic32Store(
		&pTest->Release,
		1,
		XMEMORY_RELEASE
	);
	testWsFutureState(
		pFirst,
		XFUTURE_CANCELLED,
		"WebSocket group member did not accept aggregate cancellation"
	);
	testRequire(
		xrtWsGroupOpResult(pOperation, 0, &Result) &&
		(Result.State == XWS_GROUP_OP_CANCELLED),
		"WebSocket group cancelled item reached wrong terminal state"
	);
	xrtFutureDestroy(pFirst);
	xrtFutureDestroy(pFuture);
	xrtWsGroupOpDestroy(pOperation);
	xrtWsGroupDestroy(pSingle);
	xrtWsGroupDestroy(pGroup);
}
#endif



/* 验证跨线程发送、硬队列、取消、等待和 Close 契约。 */
int main(void)
{
	testwsfuture Test;
	xwsconn* pClient;
	xwsconn* pServer;
	xfuture* pFirst;
	xfuture* pSecond;
	xfuture* pThird;
	#if !defined(TEST_WS_FUTURE_WRITER_ABANDON)
		xfuture* pWrite;
		xfuture* pPing;
		xfuture* pDrain;
		xfuture* pLate;
	#endif
	xdeadline AttachDeadline;
	xallocator Allocator;

	memset(&Test, 0, sizeof(Test));
	xrtAtomicPtrInit(&Test.Client, NULL);
	xrtAtomicPtrInit(&Test.Server, NULL);
	xrtAtomicPtrInit(&Test.AwaitFuture, NULL);
	xrtAtomic32Init(&Test.Blocked, 0);
	xrtAtomic32Init(&Test.Release, 0);
	xrtAtomic32Init(&Test.Awaiting, 0);
	xrtAtomic32Init(&Test.Messages, 0);
	#if defined(TEST_WS_FUTURE_GROUP_OPERATION)
		xrtAtomic32Init(&Test.MessageLock, 0);
	#endif
	xrtAtomic32Init(&Test.Pong, 0);
	xrtAtomic32Init(&Test.Closed, 0);
	xrtAtomic32Init(&Test.ListenerClosed, 0);
	xrtAtomic32Init(&Test.AllocFail, 0);
	xrtAtomic64Init(&Test.AllocAttempts, 0);
	#if defined(TEST_WS_FUTURE_REF)
		xrtAtomic32Init(&Test.RefReleases, 0);
	#endif
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
		xrtAtomicPtrInit(&Test.Writer, NULL);
		xrtAtomicPtrInit(&Test.WriterFuture, NULL);
		xrtAtomic32Init(&Test.WriterReady, 0);
		xrtAtomic32Init(&Test.WriterHeld, 0);
		xrtAtomic32Init(&Test.WriterMessages, 0);
		xrtAtomic64Init(&Test.WriterMessageBase, 0);
	#endif
	Allocator.Context = &Test;
	Allocator.Alloc = testWsFutureAlloc;
	Allocator.Realloc = testWsFutureRealloc;
	Allocator.Free = testWsFutureFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"WebSocket Future allocator install failed"
	);

	testRequire(
		(xrtWsConnWaitAsync(
			NULL,
			XWS_CONN_WAIT_CLOSE
		 ) == NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_CONN_ERROR_ARGUMENT),
		"WebSocket Future accepted a null Connection"
	);
	xrtClearError();
	testRequire(
		(xrtWsConnPongAsync(
			NULL,
			XRT_BYTES_LITERAL("")
		 ) == NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_CONN_ERROR_ARGUMENT),
		"WebSocket Pong Future accepted a null Connection"
	);
	xrtClearError();
	testWsFutureConnect(&Test);
	AttachDeadline = xrtDeadlineAfter(
		UINT64_C(10000000)
	);
	while ( ((pClient = (xwsconn*)xrtAtomicPtrLoad(
		&Test.Client,
		XMEMORY_ACQUIRE
	)) == NULL) || ((pServer = (xwsconn*)xrtAtomicPtrLoad(
		&Test.Server,
		XMEMORY_ACQUIRE
	)) == NULL) ) {
		testRequire(
			!xrtDeadlineExpired(AttachDeadline),
			"WebSocket Future connections were not attached"
		);
		xrtThreadYield();
	}
	testWsFutureBoundaries(pClient);
	#if defined(TEST_WS_FUTURE_REF)
		testWsFutureBarrier(&Test, pServer);
	#endif

	testWsFutureBlockClient(&Test, pClient);
	testWsFutureAdmissionFailures(
		&Test,
		pClient
	);
	testWsFutureConcurrentSubmit(
		&Test,
		pClient
	);
	testWsFutureBlockClient(&Test, pClient);

	pFirst = xrtWsConnTextAsync(
		pClient,
		XRT_STR_LITERAL("first")
	);
	#if defined(TEST_WS_FUTURE_DEFLATE)
		pSecond = xrtWsConnTextCompressedAsync(
			pClient,
			XRT_STR_LITERAL("second")
		);
	#else
		pSecond = xrtWsConnTextAsync(
			pClient,
			XRT_STR_LITERAL("second")
		);
	#endif
	pThird = xrtWsConnTextAsync(
		pClient,
		XRT_STR_LITERAL("third")
	);
	testRequire(
		(pFirst != NULL) &&
		(pSecond != NULL) &&
		(pThird == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_AGAIN) &&
		(xrtWsConnAsyncCount(pClient) == 2) &&
		(xrtWsConnAsyncBytes(pClient) == 11),
		"WebSocket Future hard queue limits failed"
	);
	xrtClearError();
	testRequire(
		xrtFutureCancel(pFirst),
		"WebSocket Future cancellation request failed"
	);
	#if defined(TEST_WS_FUTURE_COROUTINE)
		testWsFutureAwaitSend(&Test, pSecond);
	#else
		xrtAtomic32Store(
			&Test.Release,
			1,
			XMEMORY_RELEASE
		);
		testWsFutureState(
			pSecond,
			XFUTURE_RESOLVED,
			"WebSocket cross-thread send did not resolve"
		);
	#endif
	testWsFutureState(
		pFirst,
		XFUTURE_CANCELLED,
		"WebSocket cancelled send reached wrong terminal state"
	);
	testWsFutureWaitAtomic(
		&Test.Messages,
		1,
		"WebSocket Future message did not arrive"
	);
	testRequire(
		(Test.MessageSize == 6) &&
		(memcmp(Test.Message, "second", 6) == 0) &&
		(xrtWsConnAsyncCount(pClient) == 0) &&
		(xrtWsConnAsyncBytes(pClient) == 0),
		"WebSocket Future cancellation changed FIFO payload"
	);
	xrtFutureDestroy(pFirst);
	xrtFutureDestroy(pSecond);

	testWsFutureStress(pClient);
	#if defined(TEST_WS_FUTURE_GROUP)
		testWsFutureGroup(pClient, pServer);
	#endif
	#if defined(TEST_WS_FUTURE_GROUP_OPERATION)
		testWsFutureGroupOperation(&Test, pClient, pServer);
	#endif
	#if defined(TEST_WS_FUTURE_REF)
		testWsFutureReferences(&Test, pClient, pServer);
	#endif
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
		testWsFutureWriterQueue(&Test, pClient);
	#endif
	#if defined(TEST_WS_FUTURE_WRITER_ABANDON)
		testWsFutureWaitAtomic(
			&Test.Closed,
			2,
			"WebSocket abandoned Writer close callbacks missing"
		);
		testRequire(
			(Test.MessageSize == 13) &&
			(memcmp(
				Test.Message + 6,
				"writer-",
				7
			 ) == 0) &&
			(xrtWsConnAsyncCount(pClient) == 0) &&
			(xrtWsConnAsyncBytes(pClient) == 0),
			"WebSocket abandoned Writer leaked queued data or budget"
		);
	#else
		pWrite = xrtWsConnWaitAsync(
			pClient,
			XWS_CONN_WAIT_WRITE
		);
		testWsFutureState(
			pWrite,
			XFUTURE_RESOLVED,
			"WebSocket writable Future did not resolve"
		);
		xrtFutureDestroy(pWrite);
		pPing = xrtWsConnPingAsync(
			pClient,
			XRT_BYTES_LITERAL("probe")
		);
		testWsFutureState(
			pPing,
			XFUTURE_RESOLVED,
			"WebSocket asynchronous Ping did not resolve"
		);
		testWsFutureWaitAtomic(
			&Test.Pong,
			#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
				2,
			#else
				1,
			#endif
			"WebSocket asynchronous Ping missed Pong"
		);
		xrtFutureDestroy(pPing);
		pDrain = xrtWsConnWaitAsync(
			pClient,
			XWS_CONN_WAIT_DRAIN
		);
		testWsFutureState(
			pDrain,
			XFUTURE_RESOLVED,
			"WebSocket drain Future did not resolve"
		);
		xrtFutureDestroy(pDrain);

		testWsFutureClosePriority(&Test, pClient);

		pLate = xrtWsConnTextAsync(
			pClient,
			XRT_STR_LITERAL("late")
		);
		testWsFutureState(
			pLate,
			XFUTURE_CLOSED,
			"WebSocket post-close send did not close immediately"
		);
		xrtFutureDestroy(pLate);
		pLate = xrtWsConnWaitAsync(
			pClient,
			XWS_CONN_WAIT_CLOSE
		);
		testWsFutureState(
			pLate,
			XFUTURE_RESOLVED,
			"WebSocket post-close wait did not resolve immediately"
		);
		xrtFutureDestroy(pLate);
		testRequire(
			(xrtWsConnAsyncCount(pClient) == 0) &&
			(xrtWsConnAsyncBytes(pClient) == 0),
			"WebSocket terminal Future leaked queue budget"
		);
	#endif

	testRequire(
		xrtNetListenerClose(Test.Listener),
		"WebSocket Future listener close failed"
	);
	testWsFutureWaitAtomic(
		&Test.ListenerClosed,
		1,
		"WebSocket Future listener close callback missing"
	);
	xrtWsConnDestroy(pClient);
	xrtWsConnDestroy(pServer);
	xrtNetListenerDestroy(Test.Listener);
	testRequire(
		xrtNetEngineDestroy(Test.Engine),
		"WebSocket Future engine destroy failed"
	);
	printf(
		"[PASS] WebSocket connection Future %s\n",
		TEST_WS_FUTURE_BACKEND_NAME
	);
	return 0;
}
