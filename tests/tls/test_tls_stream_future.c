#include "../fixtures/tls_server.h"



#if !defined(TEST_TLS_STREAM_FUTURE_BACKEND)
	#define TEST_TLS_STREAM_FUTURE_BACKEND XNET_PORT_SELECT
	#define TEST_TLS_STREAM_FUTURE_BACKEND_NAME "select"
#endif

#define TEST_TLS_STREAM_FUTURE_ASYNC_BYTES (4u * 1024u * 1024u)
#define TEST_TLS_STREAM_FUTURE_OOM_BYTES 4096u
#define TEST_TLS_STREAM_FUTURE_THREADS 8u



typedef struct test_tls_stream_future_context
	test_tls_stream_future_context;



/* 每个并发提交线程保存自己的 Future 和线程局部错误快照。 */
typedef struct test_tls_stream_future_producer {
	test_tls_stream_future_context* Context;
	xfuture* Future;
	xerrkind ErrorKind;
	int32 ErrorCode;
	bool Cancelled;
} test_tls_stream_future_producer;



/* 单次构造故障线程记录关闭线性化期间的提交结果。 */
typedef struct test_tls_stream_future_rejection {
	test_tls_stream_future_context* Context;
	const void* Data;
	size_t Size;
	xfuture* Future;
	xerrkind ErrorKind;
	int32 ErrorCode;
} test_tls_stream_future_rejection;



/* 事件切换必须在 Stream 所属 Worker 上完成。 */
typedef struct test_tls_stream_future_events {
	xtlsstream* Stream;
	const xtlsstreamevents* Events;
	ptr Data;
	xatomic32 Done;
	bool Result;
	xerrkind ErrorKind;
	int32 ErrorCode;
} test_tls_stream_future_events;



/* 每端只保留调用方引用和可并发观察的生命周期计数。 */
typedef struct test_tls_stream_future_endpoint {
	test_tls_stream_future_context* Context;
	xtlsstream* Stream;
	xatomic32 Open;
	xatomic32 End;
	xatomic32 Close;
	xatomic32 Error;
	bool Server;
} test_tls_stream_future_endpoint;



/* Listener 在唯一测试连接建立期间借用全部 TLS 配置。 */
struct test_tls_stream_future_context {
	test_tls_stream_future_endpoint Client;
	test_tls_stream_future_endpoint Server;
	xtlsserverconfig ServerConfig;
	xtlsstreamconfig StreamConfig;
	xtlsstreamevents Events;
	xatomic32 Accepted;
	xatomic32 ListenerClose;
	xatomic32 ListenerError;
	xatomic32 AllocFail;
	xatomic32 AllocBlock;
	xatomic32 AllocBlockStarted;
	xatomic32 AllocBlockRelease;
	xatomic32 CloseCommandPassed;
	xatomic32 BlockStarted;
	xatomic32 BlockRelease;
	#if defined(TEST_TLS_STREAM_FUTURE_COROUTINE)
		xatomic32 Awaiting;
	#endif
	xatomic32 ProducerGo;
	xatomic32 ProducerSubmitted;
	xatomic32 ProducerCancelGo;
	xatomic32 ProducerCancelled;
	xatomic32 CloseOomGo;
	xatomic64 AllocFailThread;
	xatomic64 AllocAttempts;
};



#if defined(TEST_TLS_STREAM_FUTURE_COROUTINE)
/* 协程桥只保存一次通用 Future await 的输入与结果。 */
typedef struct test_tls_stream_future_await {
	test_tls_stream_future_context* Context;
	xfuture* Future;
	xwaitresult Result;
} test_tls_stream_future_await;
#endif



/* 在定向 OOM 点暂停分配线程，使 Close 观察到尚未完成构造的预算预留。 */
static bool testTlsStreamFutureAllocationFails(
	test_tls_stream_future_context* pContext
)
{
	if ( xrtAtomic32Load(
		&pContext->AllocFail,
		XMEMORY_ACQUIRE
	) == 0 ) {
		return false;
	}
	if ( xrtAtomic32Load(
		&pContext->AllocBlock,
		XMEMORY_ACQUIRE
	) != 0 ) {
		if ( xrtAtomic64Load(
			&pContext->AllocFailThread,
			XMEMORY_ACQUIRE
		) != xrtThreadCurrentId() ) {
			return false;
		}
		xrtAtomic32Store(
			&pContext->AllocBlockStarted,
			1,
			XMEMORY_RELEASE
		);
		while ( xrtAtomic32Load(
			&pContext->AllocBlockRelease,
			XMEMORY_ACQUIRE
		) == 0 ) {
			xrtThreadYield();
		}
	}
	return true;
}



/* 正常阶段转发系统分配，故障阶段拒绝全部新内存。 */
static ptr testTlsStreamFutureAlloc(ptr pData, size_t iSize)
{
	test_tls_stream_future_context* pContext =
		(test_tls_stream_future_context*)pData;

	(void)xrtAtomic64FetchAdd(
		&pContext->AllocAttempts,
		1,
		XMEMORY_RELAXED
	);
	return testTlsStreamFutureAllocationFails(pContext) ?
		NULL : malloc(iSize);
}



/* 故障阶段拒绝扩容，正常阶段保持标准 realloc 语义。 */
static ptr testTlsStreamFutureRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	test_tls_stream_future_context* pContext =
		(test_tls_stream_future_context*)pData;

	(void)xrtAtomic64FetchAdd(
		&pContext->AllocAttempts,
		1,
		XMEMORY_RELAXED
	);
	return testTlsStreamFutureAllocationFails(pContext) ?
		NULL : realloc(pMemory, iSize);
}



/* 释放正常阶段已经取得的底层内存。 */
static void testTlsStreamFutureFree(ptr pData, ptr pMemory)
{
	(void)pData;
	free(pMemory);
}



/* 在截止时间内等待一个原子计数达到目标。 */
static void testTlsStreamFutureWaitCount(
	xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(10000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}



/* 在截止时间内等待 TLS 明文缓冲达到目标。 */
static void testTlsStreamFutureWaitAvailable(
	xtlsstream* pStream,
	size_t iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(10000000u);

	while ( xrtTlsStreamAvailable(pStream) != iExpected ) {
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}



/* 在截止时间内等待接收端出现至少一个完整 TLS 明文记录。 */
static void testTlsStreamFutureWaitReadable(
	xtlsstream* pStream,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(10000000u);

	while ( xrtTlsStreamAvailable(pStream) == 0 ) {
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}



/* 占住所属 Worker，使已受理接收在指定分配状态下复制结果。 */
static void testTlsStreamFutureBlock(
	xnetworker* pWorker,
	ptr pData
)
{
	test_tls_stream_future_context* pContext =
		(test_tls_stream_future_context*)pData;

	(void)pWorker;
	xrtAtomic32Store(
		&pContext->BlockStarted,
		1,
		XMEMORY_RELEASE
	);
	while ( xrtAtomic32Load(
		&pContext->BlockRelease,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
}



/* 标记同一 Worker 中排在 Close 命令之后的应用命令已经执行。 */
static void testTlsStreamFutureClosePassed(
	xnetworker* pWorker,
	ptr pData
)
{
	test_tls_stream_future_context* pContext =
		(test_tls_stream_future_context*)pData;

	(void)pWorker;
	xrtAtomic32Store(
		&pContext->CloseCommandPassed,
		1,
		XMEMORY_RELEASE
	);
}



/* 从外部线程同时提交发送，再同时请求取消尚未开始的节点。 */
static int32 testTlsStreamFutureProduce(ptr pData)
{
	test_tls_stream_future_producer* pProducer =
		(test_tls_stream_future_producer*)pData;
	test_tls_stream_future_context* pContext =
		pProducer->Context;
	const xerror* pError;

	while ( xrtAtomic32Load(
		&pContext->ProducerGo,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
	xrtClearError();
	pProducer->Future = xrtTlsStreamSendAsync(
		pContext->Client.Stream,
		"p",
		1u
	);
	if ( pProducer->Future == NULL ) {
		pError = xrtGetError();
		pProducer->ErrorKind = xrtErrorKind(pError);
		pProducer->ErrorCode = xrtErrorCode(pError);
	}
	(void)xrtAtomic32FetchAdd(
		&pContext->ProducerSubmitted,
		1,
		XMEMORY_RELEASE
	);

	while ( xrtAtomic32Load(
		&pContext->ProducerCancelGo,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
	if ( pProducer->Future != NULL ) {
		pProducer->Cancelled =
			xrtFutureCancel(pProducer->Future);
	}
	(void)xrtAtomic32FetchAdd(
		&pContext->ProducerCancelled,
		1,
		XMEMORY_RELEASE
	);
	return 0;
}



/* 在独立线程中命中发送节点构造 OOM，并保存线程局部结构化错误。 */
static int32 testTlsStreamFutureRejectDuringClose(ptr pData)
{
	test_tls_stream_future_rejection* pRejection =
		(test_tls_stream_future_rejection*)pData;
	test_tls_stream_future_context* pContext =
		pRejection->Context;
	const xerror* pError;

	while ( xrtAtomic32Load(
		&pContext->CloseOomGo,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
	xrtClearError();
	pRejection->Future = xrtTlsStreamSendAsync(
		pContext->Client.Stream,
		pRejection->Data,
		pRejection->Size
	);
	pError = xrtGetError();
	pRejection->ErrorKind = xrtErrorKind(pError);
	pRejection->ErrorCode = xrtErrorCode(pError);
	return 0;
}



#if defined(TEST_TLS_STREAM_FUTURE_COROUTINE)
/* 在调度协程中等待 TLS Future，不引入 TLS 专用协程 API。 */
static ptr testTlsStreamFutureAwaitProc(ptr pData)
{
	test_tls_stream_future_await* pAwait =
		(test_tls_stream_future_await*)pData;

	xrtAtomic32Store(
		&pAwait->Context->Awaiting,
		1,
		XMEMORY_RELEASE
	);
	pAwait->Result = xrtFutureAwait(pAwait->Future);
	return pAwait;
}



/* 确认协程已经挂起后释放网络 Worker。 */
static int32 testTlsStreamFutureAwaitRelease(ptr pData)
{
	test_tls_stream_future_context* pContext =
		(test_tls_stream_future_context*)pData;

	while ( xrtAtomic32Load(
		&pContext->Awaiting,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
	xrtAtomic32Store(
		&pContext->BlockRelease,
		1,
		XMEMORY_RELEASE
	);
	return 0;
}



/* 运行一次被网络 Worker 跨线程唤醒的协程等待。 */
static void testTlsStreamFutureAwait(
	test_tls_stream_future_context* pContext,
	xfuture* pFuture
)
{
	test_tls_stream_future_await Await;
	xcosched* pScheduler;
	xcoro* pCoroutine;
	xthread* pRelease;

	memset(&Await, 0, sizeof(Await));
	Await.Context = pContext;
	Await.Future = pFuture;
	pScheduler = xrtCoSchedCreate();
	testRequire(pScheduler != NULL,
		"TLS Stream Future coroutine scheduler creation failed");
	pCoroutine = xrtCoSpawn(
		pScheduler,
		testTlsStreamFutureAwaitProc,
		&Await,
		NULL
	);
	pRelease = xrtThreadCreate(
		testTlsStreamFutureAwaitRelease,
		pContext,
		0
	);
	testRequire(
		(pCoroutine != NULL) &&
		(pRelease != NULL) &&
		xrtCoSchedRun(pScheduler) &&
		(xrtThreadWait(pRelease) == XWAIT_OK) &&
		(xrtThreadExitCode(pRelease) == 0) &&
		(Await.Result == XWAIT_OK) &&
		(xrtCoResult(pCoroutine) == &Await),
		"TLS Stream Future coroutine await failed"
	);
	xrtThreadDestroy(pRelease);
	testRequire(
		xrtCoDestroy(pCoroutine) &&
		xrtCoSchedDestroy(pScheduler),
		"TLS Stream Future coroutine cleanup failed"
	);
}
#endif



/* Read 回调只用于验证 callback 与 pull 模式不能同时消费。 */
static void testTlsStreamFutureRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	(void)pStream;
	(void)pBuffer;
	(void)pData;
}



/* 在所属 Worker 上切换事件并保存完整结果。 */
static void testTlsStreamFutureSetEvents(
	xnetworker* pWorker,
	ptr pData
)
{
	test_tls_stream_future_events* pChange =
		(test_tls_stream_future_events*)pData;
	const xerror* pError;

	(void)pWorker;
	xrtClearError();
	pChange->Result = xrtTlsStreamSetEvents(
		pChange->Stream,
		pChange->Events,
		pChange->Data
	);
	if ( !pChange->Result ) {
		pError = xrtGetError();
		pChange->ErrorKind = xrtErrorKind(pError);
		pChange->ErrorCode = xrtErrorCode(pError);
	}
	xrtAtomic32Store(&pChange->Done, 1, XMEMORY_RELEASE);
}



/* 等待 Future 进入指定终态，并在失败时输出结构化原因。 */
static void testTlsStreamFutureState(
	xfuture* pFuture,
	xfuturestate State,
	cstr sMessage
)
{
	const xerror* pError;
	xwaitresult WaitResult;

	testRequire(pFuture != NULL, sMessage);
	WaitResult = xrtFutureWaitFor(pFuture, 10000000u);
	if ( WaitResult != XWAIT_OK ) {
		pError = xrtFutureError(pFuture);
		fprintf(
			stderr,
			"[TLS Stream Future] %s: wait=%d state=%d "
			"operation=%s error=%s\n",
			sMessage,
			(int)WaitResult,
			(int)xrtFutureState(pFuture),
			pError != NULL ? xrtErrorOperation(pError) : "none",
			pError != NULL ? xrtErrorMessage(pError) : "none"
		);
	}
	testRequire(WaitResult == XWAIT_OK, sMessage);
	if ( xrtFutureState(pFuture) != State ) {
		pError = xrtFutureError(pFuture);
		fprintf(
			stderr,
			"[TLS Stream Future] %s: state=%d operation=%s error=%s\n",
			sMessage,
			(int)xrtFutureState(pFuture),
			pError != NULL ? xrtErrorOperation(pError) : "none",
			pError != NULL ? xrtErrorMessage(pError) : "none"
		);
	}
	testRequire(xrtFutureState(pFuture) == State, sMessage);
}



/* 验证拥有结果的接收 Future 内容。 */
static void testTlsStreamFutureBytes(
	xfuture* pFuture,
	cstr sExpected,
	size_t iSize,
	cstr sMessage
)
{
	const xnetbytes* pBytes;
	xbytesview View;

	testTlsStreamFutureState(pFuture, XFUTURE_RESOLVED, sMessage);
	pBytes = (const xnetbytes*)xrtFutureValue(pFuture);
	View = xrtNetBytesView(pBytes);
	testRequire((pBytes != NULL) && (View.Size == iSize) &&
		((iSize == 0) ||
		 (memcmp(View.Data, sExpected, iSize) == 0)),
		sMessage);
}



/* 记录 TLS Stream 已经完成握手。 */
static void testTlsStreamFutureOpen(
	xtlsstream* pStream,
	ptr pData
)
{
	test_tls_stream_future_endpoint* pEndpoint =
		(test_tls_stream_future_endpoint*)pData;

	testRequire(
		xrtNetWorkerIsCurrent(xrtNetStreamWorker(
			xrtTlsStreamTransport(pStream)
		)),
		"TLS Stream Future Open worker mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Open,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录对端认证 close_notify。 */
static void testTlsStreamFutureEnd(
	xtlsstream* pStream,
	ptr pData
)
{
	test_tls_stream_future_endpoint* pEndpoint =
		(test_tls_stream_future_endpoint*)pData;

	(void)pStream;
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->End,
		1,
		XMEMORY_RELEASE
	);
}



/* 正常路径只接受完成认证关闭的无错误终态。 */
static void testTlsStreamFutureClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_tls_stream_future_endpoint* pEndpoint =
		(test_tls_stream_future_endpoint*)pData;

	if ( (Result != XNET_RESULT_OK) || (pError != NULL) ||
		(xrtTlsStreamState(pStream) != XTLS_STREAM_CLOSED) ) {
		(void)xrtAtomic32FetchAdd(
			&pEndpoint->Error,
			1,
			XMEMORY_RELAXED
		);
	}
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Close,
		1,
		XMEMORY_RELEASE
	);
}



/* 在 Listener Worker 上把已接受 TCP 引用转移给 TLS Server。 */
static bool testTlsStreamFutureAccept(
	xnetlistener* pListener,
	xnetstream* pTransport,
	ptr pData
)
{
	test_tls_stream_future_context* pContext =
		(test_tls_stream_future_context*)pData;
	bool bAccepted;

	(void)pListener;
	bAccepted = xrtTlsStreamAccept(
		pTransport,
		&pContext->ServerConfig,
		&pContext->StreamConfig,
		&pContext->Events,
		&pContext->Server,
		&pContext->Server.Stream
	);
	if ( bAccepted ) {
		xrtAtomic32Store(
			&pContext->Accepted,
			1,
			XMEMORY_RELEASE
		);
	}
	return bAccepted;
}



/* Listener 错误必须独立于连接 Future 终态。 */
static void testTlsStreamFutureListenerError(
	xnetlistener* pListener,
	const xerror* pError,
	ptr pData
)
{
	test_tls_stream_future_context* pContext =
		(test_tls_stream_future_context*)pData;

	(void)pListener;
	testRequire(pError != NULL,
		"TLS Stream Future listener error is null");
	(void)xrtAtomic32FetchAdd(
		&pContext->ListenerError,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 Listener 唯一关闭事件。 */
static void testTlsStreamFutureListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_tls_stream_future_context* pContext =
		(test_tls_stream_future_context*)pData;

	(void)pListener;
	(void)xrtAtomic32FetchAdd(
		&pContext->ListenerClose,
		1,
		XMEMORY_RELEASE
	);
}



/* 覆盖 TLS Stream Future 的拉取、发送、取消、背压和终态契约。 */
int main(void)
{
	static const xstrview Protocols[] = {
		XRT_STR_INIT("http/1.1")
	};
	static const xnetspan Message[] = {
		{ (cbytes)"hello", 5u },
		{ (cbytes)"-", 1u },
		{ (cbytes)"world", 5u }
	};
	static const xnetspan InvalidMessage[] = {
		{ NULL, 1u }
	};
	static const xnetspan OverflowMessage[] = {
		{ (cbytes)"x", SIZE_MAX },
		{ (cbytes)"x", 1u }
	};
	static const uint8 OomPayload[
		TEST_TLS_STREAM_FUTURE_OOM_BYTES
	] = { 0x5Au };
	static const uint8 StartedPayload[
		TEST_TLS_STREAM_FUTURE_ASYNC_BYTES
	] = { 0xC3u };
	static const uint8 TooLarge = 0xA5u;
	static const char AbortPayload[] = "buffered-before-abort";
	test_tls_stream_future_context Test;
	xnetengineconfig EngineConfig;
	xnetenginestats EngineStats;
	xnetlistenconfig ListenConfig;
	xnetstreamconfig TransportConfig;
	xnetlistenerevents ListenerEvents;
	xtlsclientconfig ClientConfig;
	xtlsstreamconfig InvalidConfig;
	xtlsstreamevents ReadEvents;
	xtlsverifierconfig VerifierConfig;
	xallocator Allocator;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	xtlsverifier* pVerifier;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xtlsstream* pRetainedClient;
	xtlsstream* pRetainedServer;
	xnetaddr Address;
	xtlsstream* pInvalidStream;
	xfuture* pClientOpen;
	xfuture* pServerOpen;
	xfuture* pCacheOpen;
	xfuture* pRead;
	xfuture* pReceive;
	xfuture* pSend;
	xfuture* pTail;
	xfuture* pDrain;
	xfuture* pOomSend;
	xfuture* pOomReceive;
	xfuture* pRecovered;
	xfuture* pStartedSend;
	xfuture* pEmptySend;
	xfuture* pEmptyVec;
	xfuture* pCloseSend;
	xfuture* pCloseOpen;
	xfuture* pCloseReceive;
	xfuture* pChunk;
	xfuture* pModeReceive;
	xfuture* pCancelled[4];
	xfuture* pRejected;
	xfuture* pClientEnd;
	xfuture* pServerEnd;
	xfuture* pClientClose;
	xfuture* pServerClose;
	xfuture* pClosedClose;
	xfuture* pClosedRecv;
	xfuture* pClosedSend;
	xfuture* pAbortClientOpen;
	xfuture* pAbortServerOpen;
	xfuture* pAbortBufferedSend;
	xfuture* pAbortBufferedRecv;
	xfuture* pAbortRecv;
	xfuture* pAbortEnd;
	xfuture* pAbortClose;
	xfuture* pAfterEngineClose;
	xfuture* pAfterEngineRecv;
	xfuture* pAfterEngineSend;
	test_tls_stream_future_producer Producers[
		TEST_TLS_STREAM_FUTURE_THREADS
	];
	test_tls_stream_future_rejection CloseRejection;
	test_tls_stream_future_events EventChange;
	xthread* Threads[TEST_TLS_STREAM_FUTURE_THREADS];
	xthread* pCloseRejectThread;
	size_t iAccepted = 0;
	size_t iLimited = 0;
	size_t iReceived = 0;
	uint64 iNodeHits;
	xwaitresult ChunkWait;

	memset(&Test, 0, sizeof(Test));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&Test.Events, 0, sizeof(Test.Events));
	Allocator.Context = &Test;
	Allocator.Alloc = testTlsStreamFutureAlloc;
	Allocator.Realloc = testTlsStreamFutureRealloc;
	Allocator.Free = testTlsStreamFutureFree;
	testRequire(xrtSetAllocator(&Allocator),
		"TLS Stream Future allocator install failed");
	Test.Client.Context = &Test;
	Test.Server.Context = &Test;
	Test.Server.Server = true;
	Test.Events.Open = testTlsStreamFutureOpen;
	Test.Events.End = testTlsStreamFutureEnd;
	Test.Events.Close = testTlsStreamFutureClose;
	ListenerEvents.Accept = testTlsStreamFutureAccept;
	ListenerEvents.Error = testTlsStreamFutureListenerError;
	ListenerEvents.Close = testTlsStreamFutureListenerClose;

	pContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	testRequire((pContext != NULL) && (pIdentity != NULL),
		"TLS Stream Future fixture creation failed");
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(pVerifier != NULL,
		"TLS Stream Future verifier creation failed");
	xrtTlsServerConfigInit(&Test.ServerConfig);
	Test.ServerConfig.Context = pContext;
	Test.ServerConfig.Identity = pIdentity;
	Test.ServerConfig.Protocols = Protocols;
	Test.ServerConfig.ProtocolCount = 1u;
	Test.ServerConfig.RequireProtocol = true;
	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.Context = pContext;
	ClientConfig.ServerName = XRT_STR_LITERAL("example.com");
	ClientConfig.Protocols = Protocols;
	ClientConfig.ProtocolCount = 1u;
	ClientConfig.Verifier = pVerifier;
	xrtTlsStreamConfigInit(&Test.StreamConfig);
	Test.StreamConfig.AsyncBytesLimit =
		TEST_TLS_STREAM_FUTURE_ASYNC_BYTES +
		TEST_TLS_STREAM_FUTURE_OOM_BYTES;
	Test.StreamConfig.AsyncCountLimit = 4u;
	Test.StreamConfig.AsyncBatch = 2u;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TLS_STREAM_FUTURE_BACKEND;
	EngineConfig.Workers = 2u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TLS Stream Future engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	xrtNetStreamConfigInit(&TransportConfig);
	TransportConfig.ReadLimit = 32u * 1024u;
	TransportConfig.WriteHighWater = 128u * 1024u;
	TransportConfig.WriteLowWater = 64u * 1024u;
	TransportConfig.WriteLimit = 256u * 1024u;
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TLS Stream Future loopback address failed");
	ListenConfig.Stream = TransportConfig;
	ListenConfig.AcceptConcurrency = 4u;
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		NULL,
		&Test
	);
	testRequire((pListener != NULL) &&
		xrtNetListenerLocal(pListener, &Address),
		"TLS Stream Future listener creation failed");
	for ( uint32 i = 0; i < 3u; i++ ) {
		InvalidConfig = Test.StreamConfig;
		if ( i == 0 ) {
			InvalidConfig.AsyncBytesLimit = 0;
		} else if ( i == 1 ) {
			InvalidConfig.AsyncCountLimit = 0;
		} else {
			InvalidConfig.AsyncBatch = 0;
		}
		xrtClearError();
		pInvalidStream = xrtTlsStreamConnect(
			pEngine,
			&Address,
			1u,
			&TransportConfig,
			&ClientConfig,
			&InvalidConfig,
			&Test.Events,
			&Test.Client
		);
		testRequire(
			(pInvalidStream == NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
			(xrtErrorCode(xrtGetError()) ==
			 (int32)XTLS_ERROR_LIMIT),
			"TLS Stream Future accepted a zero configuration limit"
		);
	}
	xrtClearError();
	Test.Client.Stream = xrtTlsStreamConnect(
		pEngine,
		&Address,
		1u,
		&TransportConfig,
		&ClientConfig,
		&Test.StreamConfig,
		&Test.Events,
		&Test.Client
	);
	testRequire(Test.Client.Stream != NULL,
		"TLS Stream Future client creation failed");
	pClientOpen = xrtTlsStreamWaitAsync(
		Test.Client.Stream,
		XTLS_STREAM_WAIT_OPEN
	);
	testRequire(pClientOpen != NULL,
		"TLS Stream Future client Open wait failed");
	testTlsStreamFutureWaitCount(
		&Test.Accepted,
		1u,
		"TLS Stream Future server was not accepted"
	);
	pServerOpen = xrtTlsStreamWaitAsync(
		Test.Server.Stream,
		XTLS_STREAM_WAIT_OPEN
	);
	testTlsStreamFutureState(
		pClientOpen,
		XFUTURE_RESOLVED,
		"TLS Stream Future client Open did not resolve"
	);
	testTlsStreamFutureState(
		pServerOpen,
		XFUTURE_RESOLVED,
		"TLS Stream Future server Open did not resolve"
	);
	testRequire(
		(xrtAtomic32Load(&Test.Client.Open, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Test.Server.Open, XMEMORY_ACQUIRE) == 1u),
		"TLS Stream Future Open callbacks mismatch"
	);
	testRequire(xrtNetEngineStats(pEngine, &EngineStats),
		"TLS Stream Future node cache initial stats failed");
	iNodeHits = EngineStats.NodeCacheHits;
	pCacheOpen = xrtTlsStreamWaitAsync(
		Test.Client.Stream,
		XTLS_STREAM_WAIT_OPEN
	);
	testRequire(
		(pCacheOpen != NULL) &&
		(xrtFutureWaitFor(pCacheOpen, 5000000u) == XWAIT_OK) &&
		(xrtFutureState(pCacheOpen) == XFUTURE_RESOLVED) &&
		xrtNetEngineStats(pEngine, &EngineStats) &&
		(EngineStats.NodeCacheHits > iNodeHits) &&
		(EngineStats.NodeCachedBytes <= EngineConfig.NodeCacheBytes),
		"TLS Stream Future node did not reuse the Worker cache"
	);

	xrtClearError();
	pRejected = xrtTlsStreamWaitAsync(
		Test.Client.Stream,
		(xtlsstreamwait)(XTLS_STREAM_WAIT_CLOSE + 1)
	);
	testRequire(
		(pRejected == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 (int32)XTLS_ERROR_ARGUMENT),
		"TLS Stream Future accepted an invalid wait condition"
	);
	xrtClearError();
	pRejected = xrtTlsStreamSendAsync(
		Test.Client.Stream,
		NULL,
		1u
	);
	testRequire(
		(pRejected == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS Stream Future accepted a null send payload"
	);
	xrtClearError();
	pRejected = xrtTlsStreamSendVecAsync(
		Test.Client.Stream,
		InvalidMessage,
		sizeof(InvalidMessage) / sizeof(InvalidMessage[0])
	);
	testRequire(
		(pRejected == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS Stream Future accepted an invalid send span"
	);
	xrtClearError();
	pRejected = xrtTlsStreamSendVecAsync(
		Test.Client.Stream,
		OverflowMessage,
		sizeof(OverflowMessage) / sizeof(OverflowMessage[0])
	);
	testRequire(
		(pRejected == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_LIMIT),
		"TLS Stream Future accepted an overflowing send vector"
	);
	xrtClearError();
	ReadEvents = Test.Events;
	ReadEvents.Read = testTlsStreamFutureRead;
	memset(&EventChange, 0, sizeof(EventChange));
	EventChange.Stream = Test.Server.Stream;
	EventChange.Events = &ReadEvents;
	EventChange.Data = &Test.Server;
	testRequire(xrtNetEnginePost(
		pEngine,
		xrtNetWorkerIndex(xrtNetStreamWorker(
			xrtTlsStreamTransport(Test.Server.Stream)
		)),
		testTlsStreamFutureSetEvents,
		&EventChange
	), "TLS Stream Future Read callback install post failed");
	testTlsStreamFutureWaitCount(
		&EventChange.Done,
		1u,
		"TLS Stream Future Read callback install did not finish"
	);
	testRequire(EventChange.Result,
		"TLS Stream Future Read callback install failed");
	xrtClearError();
	pRejected = xrtTlsStreamRecvAsync(Test.Server.Stream, 1u);
	testRequire(
		(pRejected == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_STATE),
		"TLS Stream Future mixed callback and pull receive"
	);
	memset(&EventChange, 0, sizeof(EventChange));
	EventChange.Stream = Test.Server.Stream;
	EventChange.Events = &Test.Events;
	EventChange.Data = &Test.Server;
	testRequire(xrtNetEnginePost(
		pEngine,
		xrtNetWorkerIndex(xrtNetStreamWorker(
			xrtTlsStreamTransport(Test.Server.Stream)
		)),
		testTlsStreamFutureSetEvents,
		&EventChange
	), "TLS Stream Future pull mode restore post failed");
	testTlsStreamFutureWaitCount(
		&EventChange.Done,
		1u,
		"TLS Stream Future pull mode restore did not finish"
	);
	testRequire(EventChange.Result,
		"TLS Stream Future pull mode restore failed");
	pModeReceive = xrtTlsStreamRecvAsync(Test.Server.Stream, 1u);
	testRequire(pModeReceive != NULL,
		"TLS Stream Future pull mode setup failed");
	memset(&EventChange, 0, sizeof(EventChange));
	EventChange.Stream = Test.Server.Stream;
	EventChange.Events = &ReadEvents;
	EventChange.Data = &Test.Server;
	testRequire(xrtNetEnginePost(
		pEngine,
		xrtNetWorkerIndex(xrtNetStreamWorker(
			xrtTlsStreamTransport(Test.Server.Stream)
		)),
		testTlsStreamFutureSetEvents,
		&EventChange
	), "TLS Stream Future callback conflict post failed");
	testTlsStreamFutureWaitCount(
		&EventChange.Done,
		1u,
		"TLS Stream Future callback conflict did not finish"
	);
	testRequire(
		!EventChange.Result &&
		(EventChange.ErrorKind == XERR_STATE) &&
		(EventChange.ErrorCode == (int32)XTLS_ERROR_STATE),
		"TLS Stream Future allowed Read callback during pull receive"
	);
	testRequire(xrtFutureCancel(pModeReceive),
		"TLS Stream Future pull mode cancellation failed");
	testTlsStreamFutureState(
		pModeReceive,
		XFUTURE_CANCELLED,
		"TLS Stream Future pull mode cancellation did not finish"
	);
	xrtFutureDestroy(pModeReceive);
	xrtClearError();

	#if defined(TEST_TLS_STREAM_FUTURE_COROUTINE)
		testRequire(xrtNetEnginePost(
			pEngine,
			xrtNetWorkerIndex(xrtNetStreamWorker(
				xrtTlsStreamTransport(Test.Server.Stream)
			)),
			testTlsStreamFutureBlock,
			&Test
		), "TLS Stream Future coroutine blocker post failed");
		testTlsStreamFutureWaitCount(
			&Test.BlockStarted,
			1u,
			"TLS Stream Future coroutine blocker did not start"
		);
	#endif
	pReceive = xrtTlsStreamRecvAsync(Test.Server.Stream, 5u);
	pRead = xrtTlsStreamWaitAsync(
		Test.Server.Stream,
		XTLS_STREAM_WAIT_READ
	);
	pSend = xrtTlsStreamSendVecAsync(
		Test.Client.Stream,
		Message,
		sizeof(Message) / sizeof(Message[0])
	);
	testRequire((pReceive != NULL) && (pRead != NULL) &&
		(pSend != NULL),
		"TLS Stream Future initial operations failed");
	#if defined(TEST_TLS_STREAM_FUTURE_COROUTINE)
		testTlsStreamFutureAwait(&Test, pReceive);
		xrtAtomic32Store(&Test.BlockStarted, 0, XMEMORY_RELEASE);
		xrtAtomic32Store(&Test.BlockRelease, 0, XMEMORY_RELEASE);
	#endif
	testTlsStreamFutureState(
		pSend,
		XFUTURE_RESOLVED,
		"TLS Stream Future vector send did not resolve"
	);
	testTlsStreamFutureBytes(
		pReceive,
		"hello",
		5u,
		"TLS Stream Future prefix receive mismatch"
	);
	{
		xnetbytes* pHeldBytes = xrtNetBytesRef(
			(xnetbytes*)xrtFutureValue(pReceive)
		);
		xbytesview HeldView;

		testRequire(pHeldBytes != NULL,
			"TLS Stream Future receive result retain failed");
		xrtFutureDestroy(pReceive);
		pReceive = NULL;
		HeldView = xrtNetBytesView(pHeldBytes);
		testRequire(
			(HeldView.Size == 5u) &&
			(memcmp(HeldView.Data, "hello", 5u) == 0),
			"TLS Stream Future retained result did not outlive Future"
		);
		xrtNetBytesDestroy(pHeldBytes);
	}
	testTlsStreamFutureState(
		pRead,
		XFUTURE_RESOLVED,
		"TLS Stream Future readable wait did not resolve"
	);
	pTail = xrtTlsStreamRecvAsync(Test.Server.Stream, 0);
	testTlsStreamFutureBytes(
		pTail,
		"-world",
		6u,
		"TLS Stream Future suffix receive mismatch"
	);
	pDrain = xrtTlsStreamWaitAsync(
		Test.Client.Stream,
		XTLS_STREAM_WAIT_DRAIN
	);
	testTlsStreamFutureState(
		pDrain,
		XFUTURE_RESOLVED,
		"TLS Stream Future drain did not resolve"
	);
	testRequire(
		(xrtTlsStreamAsyncBytes(Test.Client.Stream) == 0) &&
		(xrtTlsStreamAsyncCount(Test.Client.Stream) == 0) &&
		(xrtTlsStreamAsyncCount(Test.Server.Stream) == 0),
		"TLS Stream Future budgets did not return to zero"
	);

	pStartedSend = xrtTlsStreamSendAsync(
		Test.Client.Stream,
		StartedPayload,
		sizeof(StartedPayload)
	);
	testRequire(pStartedSend != NULL,
		"TLS Stream Future started-send creation failed");
	testTlsStreamFutureWaitReadable(
		Test.Server.Stream,
		"TLS Stream Future started send made no network progress"
	);
	testRequire(
		(xrtFutureState(pStartedSend) == XFUTURE_PENDING) &&
		(xrtTlsStreamAsyncBytes(Test.Client.Stream) ==
		 sizeof(StartedPayload)) &&
		xrtFutureCancel(pStartedSend),
		"TLS Stream Future could not cancel a started short write"
	);
	while ( iReceived < sizeof(StartedPayload) ) {
		const xnetbytes* pBytes;
		xbytesview View;

		pChunk = xrtTlsStreamRecvAsync(Test.Server.Stream, 0);
		ChunkWait = xrtFutureWaitFor(pChunk, 10000000u);
		if ( ChunkWait != XWAIT_OK ) {
			fprintf(
				stderr,
				"[TLS Stream Future] started-send stalled: "
				"client-state=%d server-state=%d "
				"send-state=%d available=%zu pending=%zu "
				"async-bytes=%zu async-count=%u\n",
				(int)xrtTlsStreamState(Test.Client.Stream),
				(int)xrtTlsStreamState(Test.Server.Stream),
				(int)xrtFutureState(pStartedSend),
				xrtTlsStreamAvailable(Test.Server.Stream),
				xrtTlsStreamPending(Test.Client.Stream),
				xrtTlsStreamAsyncBytes(Test.Client.Stream),
				xrtTlsStreamAsyncCount(Test.Client.Stream)
			);
		}
		testRequire(ChunkWait == XWAIT_OK,
			"TLS Stream Future started-send receive timed out");
		testTlsStreamFutureState(
			pChunk,
			XFUTURE_RESOLVED,
			"TLS Stream Future started-send receive failed"
		);
		pBytes = (const xnetbytes*)xrtFutureValue(pChunk);
		View = xrtNetBytesView(pBytes);
		testRequire(
			(pBytes != NULL) &&
			(View.Size != 0) &&
			(View.Size <=
			 (sizeof(StartedPayload) - iReceived)) &&
			(memcmp(
				View.Data,
				StartedPayload + iReceived,
				View.Size
			 ) == 0),
			"TLS Stream Future started-send bytes mismatch"
		);
		iReceived += View.Size;
		xrtFutureDestroy(pChunk);
	}
	testTlsStreamFutureState(
		pStartedSend,
		XFUTURE_RESOLVED,
		"TLS Stream Future cancelled a send after first progress"
	);
	testRequire(
		(xrtTlsStreamAsyncBytes(Test.Client.Stream) == 0) &&
		(xrtTlsStreamAsyncCount(Test.Client.Stream) == 0),
		"TLS Stream Future started send leaked budget"
	);

	pOomSend = xrtTlsStreamSendAsync(
		Test.Client.Stream,
		OomPayload,
		sizeof(OomPayload)
	);
	testTlsStreamFutureState(
		pOomSend,
		XFUTURE_RESOLVED,
		"TLS Stream Future OOM setup send failed"
	);
	testTlsStreamFutureWaitAvailable(
		Test.Server.Stream,
		sizeof(OomPayload),
		"TLS Stream Future OOM setup bytes did not arrive"
	);
	testRequire(xrtNetEnginePost(
		pEngine,
		xrtNetWorkerIndex(xrtNetStreamWorker(
			xrtTlsStreamTransport(Test.Server.Stream)
		)),
		testTlsStreamFutureBlock,
		&Test
	), "TLS Stream Future Worker blocker post failed");
	testTlsStreamFutureWaitCount(
		&Test.BlockStarted,
		1u,
		"TLS Stream Future Worker blocker did not start"
	);
	pOomReceive = xrtTlsStreamRecvAsync(Test.Server.Stream, 0);
	testRequire(pOomReceive != NULL,
		"TLS Stream Future OOM receive was not accepted");
	xrtAtomic32Store(&Test.AllocFail, 1, XMEMORY_RELEASE);
	xrtAtomic32Store(&Test.BlockRelease, 1, XMEMORY_RELEASE);
	testTlsStreamFutureState(
		pOomReceive,
		XFUTURE_FAILED,
		"TLS Stream Future result OOM did not fail"
	);
	testRequire(
		(xrtErrorKind(xrtFutureError(pOomReceive)) == XERR_MEMORY) &&
		(xrtTlsStreamAvailable(Test.Server.Stream) ==
		 sizeof(OomPayload)) &&
		(xrtTlsStreamAsyncCount(Test.Server.Stream) == 0),
		"TLS Stream Future result OOM consumed data or leaked budget"
	);
	xrtAtomic32Store(&Test.AllocFail, 0, XMEMORY_RELEASE);
	pRecovered = xrtTlsStreamRecvAsync(Test.Server.Stream, 0);
	testTlsStreamFutureBytes(
		pRecovered,
		(cstr)OomPayload,
		sizeof(OomPayload),
		"TLS Stream Future receive did not recover after OOM"
	);
	xrtAtomic32Store(&Test.AllocFail, 1, XMEMORY_RELEASE);
	xrtClearError();
	pRejected = xrtTlsStreamSendAsync(
		Test.Client.Stream,
		OomPayload,
		sizeof(OomPayload)
	);
	testRequire((pRejected == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtTlsStreamAsyncCount(Test.Client.Stream) == 0) &&
		(xrtTlsStreamAsyncBytes(Test.Client.Stream) == 0),
		"TLS Stream Future submit OOM leaked admission budget");
	xrtAtomic32Store(&Test.AllocFail, 0, XMEMORY_RELEASE);
	xrtClearError();

	xrtAtomic32Store(&Test.BlockStarted, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&Test.BlockRelease, 0, XMEMORY_RELEASE);
	testRequire(xrtNetEnginePost(
		pEngine,
		xrtNetWorkerIndex(xrtNetStreamWorker(
			xrtTlsStreamTransport(Test.Client.Stream)
		)),
		testTlsStreamFutureBlock,
		&Test
	), "TLS Stream Future producer blocker post failed");
	testTlsStreamFutureWaitCount(
		&Test.BlockStarted,
		1u,
		"TLS Stream Future producer blocker did not start"
	);
	memset(Producers, 0, sizeof(Producers));
	for ( size_t i = 0;
		i < TEST_TLS_STREAM_FUTURE_THREADS;
		i++ ) {
		Producers[i].Context = &Test;
		Threads[i] = xrtThreadCreate(
			testTlsStreamFutureProduce,
			&Producers[i],
			0
		);
		testRequire(Threads[i] != NULL,
			"TLS Stream Future producer thread creation failed");
	}
	xrtAtomic32Store(&Test.ProducerGo, 1, XMEMORY_RELEASE);
	testTlsStreamFutureWaitCount(
		&Test.ProducerSubmitted,
		TEST_TLS_STREAM_FUTURE_THREADS,
		"TLS Stream Future producers did not submit"
	);
	for ( size_t i = 0;
		i < TEST_TLS_STREAM_FUTURE_THREADS;
		i++ ) {
		if ( Producers[i].Future != NULL ) {
			iAccepted++;
		} else if (
			(Producers[i].ErrorKind == XERR_AGAIN) &&
			(Producers[i].ErrorCode == (int32)XTLS_ERROR_LIMIT)
		) {
			iLimited++;
		}
	}
	testRequire(
		(iAccepted == 4u) &&
		(iLimited == 4u) &&
		(xrtTlsStreamAsyncCount(Test.Client.Stream) == 4u) &&
		(xrtTlsStreamAsyncBytes(Test.Client.Stream) == 4u),
		"TLS Stream Future concurrent hard admission mismatch"
	);
	xrtAtomic32Store(
		&Test.ProducerCancelGo,
		1,
		XMEMORY_RELEASE
	);
	testTlsStreamFutureWaitCount(
		&Test.ProducerCancelled,
		TEST_TLS_STREAM_FUTURE_THREADS,
		"TLS Stream Future producers did not cancel"
	);
	xrtAtomic32Store(&Test.BlockRelease, 1, XMEMORY_RELEASE);
	for ( size_t i = 0;
		i < TEST_TLS_STREAM_FUTURE_THREADS;
		i++ ) {
		testRequire(xrtThreadWait(Threads[i]) == XWAIT_OK,
			"TLS Stream Future producer thread wait failed");
		testRequire(xrtThreadExitCode(Threads[i]) == 0,
			"TLS Stream Future producer thread failed");
		xrtThreadDestroy(Threads[i]);
		if ( Producers[i].Future != NULL ) {
			testRequire(Producers[i].Cancelled,
				"TLS Stream Future producer cancellation was rejected");
			testTlsStreamFutureState(
				Producers[i].Future,
				XFUTURE_CANCELLED,
				"TLS Stream Future producer cancellation did not finish"
			);
			xrtFutureDestroy(Producers[i].Future);
		}
	}
	testRequire(
		(xrtTlsStreamAsyncCount(Test.Client.Stream) == 0) &&
		(xrtTlsStreamAsyncBytes(Test.Client.Stream) == 0),
		"TLS Stream Future concurrent cancellation leaked budget"
	);

	for ( size_t i = 0; i < 4u; i++ ) {
		pCancelled[i] = xrtTlsStreamRecvAsync(
			Test.Server.Stream,
			1u
		);
		testRequire(pCancelled[i] != NULL,
			"TLS Stream Future count-limit setup failed");
	}
	xrtClearError();
	pRejected = xrtTlsStreamRecvAsync(Test.Server.Stream, 1u);
	testRequire((pRejected == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_AGAIN) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_LIMIT),
		"TLS Stream Future operation hard limit mismatch");
	xrtClearError();
	for ( size_t i = 0; i < 4u; i++ ) {
		testRequire(xrtFutureCancel(pCancelled[i]),
			"TLS Stream Future receive cancellation request failed");
		testTlsStreamFutureState(
			pCancelled[i],
			XFUTURE_CANCELLED,
			"TLS Stream Future receive cancellation was not confirmed"
		);
	}
	testRequire(
		(xrtTlsStreamState(Test.Server.Stream) == XTLS_STREAM_OPEN) &&
		(xrtTlsStreamAsyncCount(Test.Server.Stream) == 0),
		"TLS Stream Future cancellation affected the connection"
	);
	xrtClearError();
	pRejected = xrtTlsStreamSendAsync(
		Test.Client.Stream,
		&TooLarge,
		TEST_TLS_STREAM_FUTURE_ASYNC_BYTES +
			TEST_TLS_STREAM_FUTURE_OOM_BYTES + 1u
	);
	testRequire((pRejected == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_LIMIT) &&
		(xrtTlsStreamAsyncBytes(Test.Client.Stream) == 0),
		"TLS Stream Future byte hard limit mismatch");
	xrtClearError();

	pEmptySend = xrtTlsStreamSendAsync(
		Test.Client.Stream,
		NULL,
		0
	);
	pEmptyVec = xrtTlsStreamSendVecAsync(
		Test.Client.Stream,
		NULL,
		0
	);
	testRequire((pEmptySend != NULL) && (pEmptyVec != NULL),
		"TLS Stream Future empty sends were rejected");
	testTlsStreamFutureState(
		pEmptySend,
		XFUTURE_RESOLVED,
		"TLS Stream Future empty send did not resolve"
	);
	testTlsStreamFutureState(
		pEmptyVec,
		XFUTURE_RESOLVED,
		"TLS Stream Future empty vector send did not resolve"
	);

	xrtAtomic32Store(&Test.BlockStarted, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&Test.BlockRelease, 0, XMEMORY_RELEASE);
	testRequire(xrtNetEnginePost(
		pEngine,
		xrtNetWorkerIndex(xrtNetStreamWorker(
			xrtTlsStreamTransport(Test.Client.Stream)
		)),
		testTlsStreamFutureBlock,
		&Test
	), "TLS Stream Future close blocker post failed");
	testTlsStreamFutureWaitCount(
		&Test.BlockStarted,
		1u,
		"TLS Stream Future close blocker did not start"
	);
	pCloseSend = xrtTlsStreamSendAsync(
		Test.Client.Stream,
		StartedPayload,
		sizeof(StartedPayload)
	);
	testRequire(pCloseSend != NULL,
		"TLS Stream Future pre-close send was not accepted");
	pClientEnd = xrtTlsStreamWaitAsync(
		Test.Client.Stream,
		XTLS_STREAM_WAIT_END
	);
	pServerEnd = xrtTlsStreamWaitAsync(
		Test.Server.Stream,
		XTLS_STREAM_WAIT_END
	);
	pClientClose = xrtTlsStreamWaitAsync(
		Test.Client.Stream,
		XTLS_STREAM_WAIT_CLOSE
	);
	pServerClose = xrtTlsStreamWaitAsync(
		Test.Server.Stream,
		XTLS_STREAM_WAIT_CLOSE
	);
	testRequire((pClientEnd != NULL) && (pServerEnd != NULL) &&
		(pClientClose != NULL) && (pServerClose != NULL),
		"TLS Stream Future close setup failed");
	memset(&CloseRejection, 0, sizeof(CloseRejection));
	CloseRejection.Context = &Test;
	CloseRejection.Data = OomPayload;
	CloseRejection.Size = sizeof(OomPayload);
	pCloseRejectThread = xrtThreadCreate(
		testTlsStreamFutureRejectDuringClose,
		&CloseRejection,
		0
	);
	testRequire(pCloseRejectThread != NULL,
		"TLS Stream Future close OOM thread creation failed");
	xrtAtomic64Store(
		&Test.AllocFailThread,
		xrtThreadId(pCloseRejectThread),
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(&Test.AllocBlock, 1, XMEMORY_RELEASE);
	xrtAtomic32Store(&Test.AllocFail, 1, XMEMORY_RELEASE);
	xrtAtomic32Store(&Test.CloseOomGo, 1, XMEMORY_RELEASE);
	testTlsStreamFutureWaitCount(
		&Test.AllocBlockStarted,
		1u,
		"TLS Stream Future close OOM reservation did not block"
	);
	testRequire(xrtTlsStreamClose(Test.Client.Stream),
		"TLS Stream Future close request failed");
	testRequire(xrtNetEnginePost(
		pEngine,
		xrtNetWorkerIndex(xrtNetStreamWorker(
			xrtTlsStreamTransport(Test.Client.Stream)
		)),
		testTlsStreamFutureClosePassed,
		&Test
	), "TLS Stream Future close marker post failed");
	xrtClearError();
	pRejected = xrtTlsStreamSendAsync(
		Test.Client.Stream,
		"late",
		4u
	);
	testRequire((pRejected == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtErrorCode(xrtGetError()) == (int32)XTLS_ERROR_STATE),
		"TLS Stream Future accepted a send after Close");
	xrtClearError();
	xrtAtomic32Store(&Test.BlockRelease, 1, XMEMORY_RELEASE);
	testTlsStreamFutureWaitCount(
		&Test.CloseCommandPassed,
		1u,
		"TLS Stream Future deferred Close command did not run"
	);

	iReceived = 0;
	while ( iReceived < sizeof(StartedPayload) ) {
		const xnetbytes* pBytes;
		xbytesview View;

		pCloseReceive = xrtTlsStreamRecvAsync(
			Test.Server.Stream,
			sizeof(StartedPayload) - iReceived
		);
		testRequire(pCloseReceive != NULL,
			"TLS Stream Future pre-close receive was not accepted");
		testTlsStreamFutureState(
			pCloseReceive,
			XFUTURE_RESOLVED,
			"TLS Stream Future Close truncated an accepted send"
		);
		pBytes = (const xnetbytes*)xrtFutureValue(pCloseReceive);
		View = xrtNetBytesView(pBytes);
		testRequire(
			(pBytes != NULL) &&
			(View.Size != 0) &&
			(View.Size <= (sizeof(StartedPayload) - iReceived)) &&
			(memcmp(
				View.Data,
				StartedPayload + iReceived,
				View.Size
			) == 0),
			"TLS Stream Future pre-close payload mismatch"
		);
		iReceived += View.Size;
		xrtFutureDestroy(pCloseReceive);
	}
	testTlsStreamFutureState(
		pCloseSend,
		XFUTURE_RESOLVED,
		"TLS Stream Future Close did not finish an accepted send"
	);
	testRequire(
		(xrtTlsStreamState(Test.Client.Stream) == XTLS_STREAM_OPEN) &&
		(xrtTlsStreamAsyncBytes(Test.Client.Stream) ==
		 sizeof(OomPayload)),
		"TLS Stream Future Close did not wait for reserved construction"
	);
	xrtAtomic32Store(&Test.BlockStarted, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&Test.BlockRelease, 0, XMEMORY_RELEASE);
	testRequire(xrtNetEnginePost(
		pEngine,
		xrtNetWorkerIndex(xrtNetStreamWorker(
			xrtTlsStreamTransport(Test.Client.Stream)
		)),
		testTlsStreamFutureBlock,
		&Test
	), "TLS Stream Future Open gate blocker post failed");
	testTlsStreamFutureWaitCount(
		&Test.BlockStarted,
		1u,
		"TLS Stream Future Open gate blocker did not start"
	);
	xrtAtomic32Store(&Test.AllocBlockRelease, 1, XMEMORY_RELEASE);
	if ( xrtThreadWait(pCloseRejectThread) != XWAIT_OK ) {
		testRequire(false,
			"TLS Stream Future close OOM thread wait failed");
	}
	if ( (xrtThreadExitCode(pCloseRejectThread) != 0) ||
		(CloseRejection.Future != NULL) ||
		(CloseRejection.ErrorKind != XERR_MEMORY) ) {
		fprintf(
			stderr,
			"[TLS Stream Future] close OOM result: exit=%d future=%p "
			"kind=%d code=%d\n",
			(int)xrtThreadExitCode(pCloseRejectThread),
			(void*)CloseRejection.Future,
			(int)CloseRejection.ErrorKind,
			(int)CloseRejection.ErrorCode
		);
	}
	testRequire(
		(xrtThreadExitCode(pCloseRejectThread) == 0) &&
		(CloseRejection.Future == NULL) &&
		(CloseRejection.ErrorKind == XERR_MEMORY),
		"TLS Stream Future close OOM rollback mismatch"
	);
	xrtThreadDestroy(pCloseRejectThread);
	xrtAtomic32Store(&Test.AllocFail, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&Test.AllocBlock, 0, XMEMORY_RELEASE);
	pCloseOpen = xrtTlsStreamWaitAsync(
		Test.Client.Stream,
		XTLS_STREAM_WAIT_OPEN
	);
	testRequire(
		(pCloseOpen != NULL) &&
		(xrtFutureState(pCloseOpen) == XFUTURE_PENDING),
		"TLS Stream Future observed stale Open after Close gate"
	);
	xrtAtomic32Store(&Test.BlockRelease, 1, XMEMORY_RELEASE);
	testRequire(
		(xrtTlsStreamAsyncBytes(Test.Client.Stream) == 0) &&
		xrtTlsStreamClose(Test.Server.Stream),
		"TLS Stream Future pre-close send budget or peer Close mismatch"
	);
	testTlsStreamFutureState(
		pClientEnd,
		XFUTURE_RESOLVED,
		"TLS Stream Future client End did not resolve"
	);
	testTlsStreamFutureState(
		pServerEnd,
		XFUTURE_RESOLVED,
		"TLS Stream Future server End did not resolve"
	);
	testTlsStreamFutureState(
		pClientClose,
		XFUTURE_RESOLVED,
		"TLS Stream Future client Close did not resolve"
	);
	testTlsStreamFutureState(
		pServerClose,
		XFUTURE_RESOLVED,
		"TLS Stream Future server Close did not resolve"
	);
	testTlsStreamFutureState(
		pCloseOpen,
		XFUTURE_CLOSED,
		"TLS Stream Future post-close Open wait did not close"
	);
	testRequire(
		(xrtAtomic32Load(&Test.Client.End, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Test.Server.End, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Test.Client.Close, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Test.Server.Close, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Test.Client.Error, XMEMORY_ACQUIRE) == 0) &&
		(xrtAtomic32Load(&Test.Server.Error, XMEMORY_ACQUIRE) == 0),
		"TLS Stream Future authenticated close callbacks mismatch"
	);

	pClosedClose = xrtTlsStreamWaitAsync(
		Test.Server.Stream,
		XTLS_STREAM_WAIT_CLOSE
	);
	pClosedRecv = xrtTlsStreamRecvAsync(Test.Server.Stream, 0);
	pClosedSend = xrtTlsStreamSendAsync(
		Test.Server.Stream,
		"",
		0
	);
	testTlsStreamFutureState(
		pClosedClose,
		XFUTURE_RESOLVED,
		"TLS Stream Future closed Close wait mismatch"
	);
	testTlsStreamFutureState(
		pClosedRecv,
		XFUTURE_CLOSED,
		"TLS Stream Future closed receive mismatch"
	);
	testTlsStreamFutureState(
		pClosedSend,
		XFUTURE_CLOSED,
		"TLS Stream Future closed send mismatch"
	);

	xrtFutureDestroy(pClientOpen);
	xrtFutureDestroy(pServerOpen);
	xrtFutureDestroy(pCacheOpen);
	xrtFutureDestroy(pRead);
	xrtFutureDestroy(pReceive);
	xrtFutureDestroy(pSend);
	xrtFutureDestroy(pTail);
	xrtFutureDestroy(pDrain);
	xrtFutureDestroy(pStartedSend);
	xrtFutureDestroy(pOomSend);
	xrtFutureDestroy(pOomReceive);
	xrtFutureDestroy(pRecovered);
	xrtFutureDestroy(pEmptySend);
	xrtFutureDestroy(pEmptyVec);
	xrtFutureDestroy(pCloseSend);
	xrtFutureDestroy(pCloseOpen);
	for ( size_t i = 0; i < 4u; i++ ) {
		xrtFutureDestroy(pCancelled[i]);
	}
	xrtFutureDestroy(pClientEnd);
	xrtFutureDestroy(pServerEnd);
	xrtFutureDestroy(pClientClose);
	xrtFutureDestroy(pServerClose);
	xrtFutureDestroy(pClosedClose);
	xrtFutureDestroy(pClosedRecv);
	xrtFutureDestroy(pClosedSend);
	pRetainedClient = Test.Client.Stream;
	pRetainedServer = Test.Server.Stream;
	Test.Client.Stream = NULL;
	Test.Server.Stream = NULL;
	xrtAtomic32Store(&Test.Accepted, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&Test.Client.Open, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&Test.Client.End, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&Test.Client.Close, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&Test.Client.Error, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&Test.Server.Open, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&Test.Server.End, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&Test.Server.Close, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&Test.Server.Error, 0, XMEMORY_RELEASE);

	Test.Client.Stream = xrtTlsStreamConnect(
		pEngine,
		&Address,
		1u,
		&TransportConfig,
		&ClientConfig,
		&Test.StreamConfig,
		&Test.Events,
		&Test.Client
	);
	testRequire(Test.Client.Stream != NULL,
		"TLS Stream Future abort client creation failed");
	pAbortClientOpen = xrtTlsStreamWaitAsync(
		Test.Client.Stream,
		XTLS_STREAM_WAIT_OPEN
	);
	testTlsStreamFutureWaitCount(
		&Test.Accepted,
		1u,
		"TLS Stream Future abort server was not accepted"
	);
	pAbortServerOpen = xrtTlsStreamWaitAsync(
		Test.Server.Stream,
		XTLS_STREAM_WAIT_OPEN
	);
	testTlsStreamFutureState(
		pAbortClientOpen,
		XFUTURE_RESOLVED,
		"TLS Stream Future abort client Open did not resolve"
	);
	testTlsStreamFutureState(
		pAbortServerOpen,
		XFUTURE_RESOLVED,
		"TLS Stream Future abort server Open did not resolve"
	);
	pAbortBufferedSend = xrtTlsStreamSendAsync(
		Test.Client.Stream,
		AbortPayload,
		sizeof(AbortPayload) - 1u
	);
	testTlsStreamFutureState(
		pAbortBufferedSend,
		XFUTURE_RESOLVED,
		"TLS Stream Future abort prelude send did not resolve"
	);
	testTlsStreamFutureWaitReadable(
		Test.Server.Stream,
		"TLS Stream Future abort prelude was not received"
	);
	pAbortRecv = xrtTlsStreamRecvAsync(Test.Client.Stream, 1u);
	pAbortEnd = xrtTlsStreamWaitAsync(
		Test.Server.Stream,
		XTLS_STREAM_WAIT_END
	);
	pAbortClose = xrtTlsStreamWaitAsync(
		Test.Server.Stream,
		XTLS_STREAM_WAIT_CLOSE
	);
	testRequire(
		(pAbortRecv != NULL) &&
		(pAbortEnd != NULL) &&
		(pAbortClose != NULL) &&
		xrtTlsStreamAbort(Test.Server.Stream),
		"TLS Stream Future abort setup failed"
	);
	testTlsStreamFutureState(
		pAbortRecv,
		XFUTURE_FAILED,
		"TLS Stream Future remote abort receive did not fail"
	);
	testTlsStreamFutureState(
		pAbortEnd,
		XFUTURE_CANCELLED,
		"TLS Stream Future abort End was not cancelled"
	);
	testTlsStreamFutureState(
		pAbortClose,
		XFUTURE_CANCELLED,
		"TLS Stream Future abort Close was not cancelled"
	);
	testTlsStreamFutureWaitCount(
		&Test.Client.Close,
		1u,
		"TLS Stream Future abort client Close callback missing"
	);
	testTlsStreamFutureWaitCount(
		&Test.Server.Close,
		1u,
		"TLS Stream Future abort server Close callback missing"
	);
	pAbortBufferedRecv = xrtTlsStreamRecvAsync(Test.Server.Stream, 0);
	testTlsStreamFutureBytes(
		pAbortBufferedRecv,
		AbortPayload,
		sizeof(AbortPayload) - 1u,
		"TLS Stream Future lost authenticated plaintext after abort"
	);
	testRequire(
		(xrtTlsStreamState(Test.Server.Stream) ==
		 XTLS_STREAM_FAILED) &&
		(xrtErrorKind(xrtTlsStreamError(Test.Server.Stream)) ==
		 XERR_CANCELLED) &&
		(xrtTlsStreamAsyncCount(Test.Server.Stream) == 0) &&
		(xrtTlsStreamAsyncBytes(Test.Server.Stream) == 0),
		"TLS Stream Future abort terminal contract mismatch"
	);
	xrtFutureDestroy(pAbortClientOpen);
	xrtFutureDestroy(pAbortServerOpen);
	xrtFutureDestroy(pAbortBufferedSend);
	xrtFutureDestroy(pAbortBufferedRecv);
	xrtFutureDestroy(pAbortRecv);
	xrtFutureDestroy(pAbortEnd);
	xrtFutureDestroy(pAbortClose);

	testRequire(xrtNetListenerClose(pListener),
		"TLS Stream Future listener close failed");
	testTlsStreamFutureWaitCount(
		&Test.ListenerClose,
		1u,
		"TLS Stream Future listener Close callback missing"
	);
	testRequire(
		xrtAtomic32Load(&Test.ListenerError, XMEMORY_ACQUIRE) == 0,
		"TLS Stream Future listener reported an error"
	);
	xrtTlsStreamDestroy(Test.Client.Stream);
	xrtTlsStreamDestroy(Test.Server.Stream);
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TLS Stream Future engine destroy failed");
	pAfterEngineClose = xrtTlsStreamWaitAsync(
		pRetainedServer,
		XTLS_STREAM_WAIT_CLOSE
	);
	pAfterEngineRecv = xrtTlsStreamRecvAsync(pRetainedServer, 0);
	pAfterEngineSend = xrtTlsStreamSendAsync(
		pRetainedServer,
		"",
		0
	);
	testTlsStreamFutureState(
		pAfterEngineClose,
		XFUTURE_RESOLVED,
		"TLS Stream terminal Close wait used a destroyed Engine"
	);
	testTlsStreamFutureState(
		pAfterEngineRecv,
		XFUTURE_CLOSED,
		"TLS Stream terminal receive used a destroyed Engine"
	);
	testTlsStreamFutureState(
		pAfterEngineSend,
		XFUTURE_CLOSED,
		"TLS Stream terminal send used a destroyed Engine"
	);
	xrtFutureDestroy(pAfterEngineClose);
	xrtFutureDestroy(pAfterEngineRecv);
	xrtFutureDestroy(pAfterEngineSend);
	xrtTlsStreamDestroy(pRetainedClient);
	xrtTlsStreamDestroy(pRetainedServer);
	testRequire(xrtAtomic64Load(
		&Test.AllocAttempts,
		XMEMORY_ACQUIRE
	) != 0, "TLS Stream Future allocator observed no attempts");
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	printf(
		"[PASS] TLS Stream Future %s lifecycle\n",
		TEST_TLS_STREAM_FUTURE_BACKEND_NAME
	);
	return 0;
}
