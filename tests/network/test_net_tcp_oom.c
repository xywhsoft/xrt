#include "../test.h"



#if !defined(TEST_TCP_OOM_BACKEND)
	#define TEST_TCP_OOM_BACKEND XNET_PORT_SELECT
	#define TEST_TCP_OOM_BACKEND_NAME "select"
#endif



typedef struct testtcpoom {
	xatomic32 Fail;
	xatomic64 Attempts;
	xatomic32 Accepted;
	xatomic32 Open;
	xatomic32 Read;
	xatomic32 Close;
	xatomic32 Aborted;
	xatomic32 ExpectAbort;
	xatomic32 Released;
	xatomic32 BufferDone;
	xatomic32 ResumeDone;
	xatomic32 LocalCopyDone;
	xatomic32 ReenterShutdown;
	xatomic32 ShutdownReturned;
	xatomic64 ReenterThread;
	xnetstream* Client;
	xnetstream* Server;
} testtcpoom;



/* 正常阶段转发分配，故障阶段拒绝全部新内存。 */
static ptr testTcpOomAlloc(ptr pData, size_t iSize)
{
	testtcpoom* pContext = (testtcpoom*)pData;

	(void)xrtAtomic64FetchAdd(&pContext->Attempts, 1, XMEMORY_RELAXED);
	if ( (xrtThreadCurrentId() == xrtAtomic64Load(
		&pContext->ReenterThread,
		XMEMORY_ACQUIRE
	)) && (xrtAtomic32Exchange(
		&pContext->ReenterShutdown,
		0,
		XMEMORY_ACQ_REL
	) != 0) ) {
		xrtAtomic32Store(
			&pContext->ShutdownReturned,
			xrtNetStreamShutdownWrite(pContext->Client) ? 1 : 2,
			XMEMORY_RELEASE
		);
	}
	return xrtAtomic32Load(&pContext->Fail, XMEMORY_ACQUIRE) != 0 ?
		NULL : malloc(iSize);
}



/* 正常阶段保持 realloc 语义，故障阶段拒绝扩容。 */
static ptr testTcpOomRealloc(ptr pData, ptr pMemory, size_t iSize)
{
	testtcpoom* pContext = (testtcpoom*)pData;

	(void)xrtAtomic64FetchAdd(&pContext->Attempts, 1, XMEMORY_RELAXED);
	return xrtAtomic32Load(&pContext->Fail, XMEMORY_ACQUIRE) != 0 ?
		NULL : realloc(pMemory, iSize);
}



/* 释放正常阶段已经取得的底层内存。 */
static void testTcpOomFree(ptr pData, ptr pMemory)
{
	(void)pData;
	free(pMemory);
}



/* 在测试截止时间前等待原子计数到达目标。 */
static void testTcpOomWait(
	xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(iDeadline), sMessage);
		xrtThreadYield();
	}
}



/* 记录两端 Open。 */
static void testTcpOomOpen(xnetstream* pStream, ptr pData)
{
	testtcpoom* pContext = (testtcpoom*)pData;

	(void)pStream;
	(void)xrtAtomic32FetchAdd(&pContext->Open, 1, XMEMORY_RELEASE);
}



/* 消费恢复分配后的验证字节。 */
static void testTcpOomRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	testtcpoom* pContext = (testtcpoom*)pData;
	size_t iSize = xrtNetBufSize(pBuffer);

	(void)pStream;
	(void)xrtNetBufConsume(pBuffer, iSize);
	(void)xrtAtomic32FetchAdd(
		&pContext->Read,
		(uint32)iSize,
		XMEMORY_RELEASE
	);
}



/* 记录正常关闭。 */
static void testTcpOomClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testtcpoom* pContext = (testtcpoom*)pData;

	if ( xrtAtomic32Load(
		&pContext->ExpectAbort,
		XMEMORY_ACQUIRE
	) == 0 ) {
		testRequire((Result == XNET_RESULT_OK) && (pError == NULL),
			"TCP OOM recovery close mismatch");
	} else if ( pStream == pContext->Client ) {
		testRequire(
			(Result == XNET_RESULT_CANCELLED) && (pError == NULL),
			"TCP OOM abort result mismatch"
		);
		xrtAtomic32Store(&pContext->Aborted, 1, XMEMORY_RELEASE);
	}
	(void)xrtAtomic32FetchAdd(&pContext->Close, 1, XMEMORY_RELEASE);
}



/* 接管服务端 Stream 引用。 */
static bool testTcpOomAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testtcpoom* pContext = (testtcpoom*)pData;

	(void)pListener;
	testRequire(xrtNetStreamSetData(pStream, pContext),
		"TCP OOM accepted data setup failed");
	pContext->Server = pStream;
	(void)xrtAtomic32FetchAdd(
		&pContext->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 失败的引用发送不得调用释放过程。 */
static void testTcpOomRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	testtcpoom* pTest = (testtcpoom*)pContext;

	(void)pData;
	(void)iSize;
	(void)xrtAtomic32FetchAdd(
		&pTest->Released,
		1,
		XMEMORY_RELEASE
	);
}



/* 在 Stream Worker 内验证缓冲接管 OOM 不转移块链或泄漏预算。 */
static void testTcpOomBuffer(
	xnetworker* pWorker,
	ptr pData
)
{
	testtcpoom* pContext = (testtcpoom*)pData;
	xnetbuf Buffer;
	static const char iByte = 'B';

	testRequire(xrtNetBufInit(
		&Buffer,
		xrtNetWorkerBufPool(pWorker)
	), "TCP buffer OOM setup failed");
	for ( size_t i = 0; i < 128; i++ ) {
		testRequire(xrtNetBufAppendBorrow(
			&Buffer,
			&iByte,
			1
		), "TCP buffer OOM source construction failed");
	}
	xrtClearError();
	xrtAtomic32Store(&pContext->Fail, 1, XMEMORY_RELEASE);
	testRequire(xrtNetStreamSendBuffer(
		pContext->Client,
		&Buffer
	) == XNET_RESULT_ERROR &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtNetBufSize(&Buffer) == 128) &&
		(xrtNetBufSpanCount(&Buffer) == 128) &&
		(xrtNetStreamPending(pContext->Client) == 0),
		"TCP buffer send OOM changed ownership or budget");
	xrtAtomic32Store(&pContext->Fail, 0, XMEMORY_RELEASE);
	xrtClearError();
	xrtNetBufClear(&Buffer);
	(void)xrtAtomic32FetchAdd(
		&pContext->BufferDone,
		1,
		XMEMORY_RELEASE
	);
}



/* 在 Worker 命令仍占用队列节点时验证 Resume 完全不依赖分配器。 */
static void testTcpOomResume(
	xnetworker* pWorker,
	ptr pData
)
{
	testtcpoom* pContext = (testtcpoom*)pData;
	uint64 iAttempts;

	(void)pWorker;
	xrtNetStreamPause(pContext->Server);
	xrtAtomic32Store(&pContext->Fail, 1, XMEMORY_RELEASE);
	iAttempts = xrtAtomic64Load(
		&pContext->Attempts,
		XMEMORY_ACQUIRE
	);
	testRequire(
		xrtNetStreamResume(pContext->Server) &&
		xrtNetStreamResume(pContext->Server) &&
		(xrtAtomic64Load(
			&pContext->Attempts,
			XMEMORY_ACQUIRE
		 ) == iAttempts),
		"TCP Resume allocated or failed during OOM"
	);
	xrtAtomic32Store(&pContext->Fail, 0, XMEMORY_RELEASE);
	(void)xrtAtomic32FetchAdd(
		&pContext->ResumeDone,
		1,
		XMEMORY_RELEASE
	);
}



/* 预热 Worker 缓冲后，普通复制发送不得再依赖全局发送节点分配。 */
static void testTcpOomLocalCopy(
	xnetworker* pWorker,
	ptr pData
)
{
	testtcpoom* pContext = (testtcpoom*)pData;
	xnetbuf BufferOne;
	xnetbuf BufferTwo;
	xnetspan Spans[2] = {
		{ (cbytes)"V", 1 },
		{ (cbytes)"V", 1 }
	};
	uint64 iAttempts;

	testRequire(xrtNetBufInit(
		&BufferOne,
		xrtNetWorkerBufPool(pWorker)
	), "TCP local copy warmup init failed");
	testRequire(xrtNetBufInit(
		&BufferTwo,
		xrtNetWorkerBufPool(pWorker)
	), "TCP local vector warmup init failed");
	testRequire(xrtNetBufAppend(&BufferOne, "W", 1),
		"TCP local copy warmup append failed");
	testRequire(xrtNetBufAppend(&BufferTwo, "W", 1),
		"TCP local vector warmup append failed");
	xrtNetBufClear(&BufferOne);
	xrtNetBufClear(&BufferTwo);
	iAttempts = xrtAtomic64Load(
		&pContext->Attempts,
		XMEMORY_ACQUIRE
	);
	xrtAtomic32Store(&pContext->Fail, 1, XMEMORY_RELEASE);
	testRequire(
		xrtNetStreamSend(pContext->Client, "L", 1) == XNET_RESULT_OK,
		"TCP worker-local copy send used global allocation"
	);
	testRequire(
		xrtNetStreamSendVec(pContext->Client, Spans, 2) ==
		 XNET_RESULT_OK,
		"TCP worker-local vector send used global allocation"
	);
	testRequire(
		xrtAtomic64Load(
			&pContext->Attempts,
			XMEMORY_ACQUIRE
		) == iAttempts,
		"TCP worker-local copy send attempted global allocation"
	);
	xrtAtomic32Store(&pContext->Fail, 0, XMEMORY_RELEASE);
	(void)xrtAtomic32FetchAdd(
		&pContext->LocalCopyDone,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证发送 OOM 的预算、所有权和连接恢复。 */
int main(void)
{
	testtcpoom Context;
	xallocator Allocator;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetstreamconfig StreamConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents StreamEvents;
	xnetstreamstats Stats;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetstream* pClient;
	xnetaddr Address;
	xnetspan Spans[2];
	xnetref* pRefs;
	char* pTaken;
	char* pRejectedRef;
	char CopyPayload[2048];
	uint64 iAttempts;

	memset(&Context, 0, sizeof(Context));
	xrtAtomic32Init(&Context.Fail, 0);
	xrtAtomic64Init(&Context.Attempts, 0);
	xrtAtomic32Init(&Context.ResumeDone, 0);
	xrtAtomic32Init(&Context.LocalCopyDone, 0);
	xrtAtomic32Init(&Context.ReenterShutdown, 0);
	xrtAtomic32Init(&Context.ShutdownReturned, 0);
	xrtAtomic64Init(&Context.ReenterThread, 0);
	Allocator.Context = &Context;
	Allocator.Alloc = testTcpOomAlloc;
	Allocator.Realloc = testTcpOomRealloc;
	Allocator.Free = testTcpOomFree;
	testRequire(xrtSetAllocator(&Allocator),
		"TCP OOM allocator install failed");

	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&StreamEvents, 0, sizeof(StreamEvents));
	ListenerEvents.Accept = testTcpOomAccept;
	StreamEvents.Open = testTcpOomOpen;
	StreamEvents.Read = testTcpOomRead;
	StreamEvents.Close = testTcpOomClose;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TCP_OOM_BACKEND;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP OOM engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TCP OOM loopback address failed");
	ListenConfig.AcceptConcurrency = 1;
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		&StreamEvents,
		&Context
	);
	testRequire((pListener != NULL) &&
		 xrtNetListenerLocal(pListener, &Address),
		"TCP OOM listener create failed");
	xrtNetStreamConfigInit(&StreamConfig);
	StreamConfig.WriteHighWater = 4096;
	StreamConfig.WriteLowWater = 1024;
	StreamConfig.WriteLimit = 8192;
	pClient = xrtNetStreamConnect(
		pEngine,
		&Address,
		0,
		&StreamConfig,
		&StreamEvents,
		&Context
	);
	testRequire(pClient != NULL, "TCP OOM client create failed");
	Context.Client = pClient;
	testTcpOomWait(&Context.Accepted, 1,
		"TCP OOM accept callback missing");
	testTcpOomWait(&Context.Open, 2, "TCP OOM open callbacks missing");
	testRequire(xrtNetEnginePost(
		pEngine,
		xrtNetWorkerIndex(xrtNetStreamWorker(pClient)),
		testTcpOomResume,
		&Context
	), "TCP Resume OOM task post failed");
	testTcpOomWait(&Context.ResumeDone, 1,
		"TCP Resume OOM task did not finish");
	testRequire(xrtNetEnginePost(
		pEngine,
		xrtNetWorkerIndex(xrtNetStreamWorker(pClient)),
		testTcpOomLocalCopy,
		&Context
	), "TCP local copy OOM task post failed");
	testTcpOomWait(&Context.LocalCopyDone, 1,
		"TCP local copy OOM task did not finish");
	testTcpOomWait(&Context.Read, 3,
		"TCP local copy did not reach the peer");

	/* 预热后，跨线程小包应同时复用发送节点和 Engine 命令节点。 */
	testRequire(xrtNetStreamSend(pClient, "W", 1) == XNET_RESULT_OK,
		"TCP small-node cache warmup failed");
	testTcpOomWait(&Context.Read, 4,
		"TCP small-node cache warmup did not reach the peer");
	iAttempts = xrtAtomic64Load(&Context.Attempts, XMEMORY_ACQUIRE);
	xrtAtomic32Store(&Context.Fail, 1, XMEMORY_RELEASE);
	testRequire(
		(xrtNetStreamSend(pClient, "N", 1) == XNET_RESULT_OK) &&
		(xrtAtomic64Load(&Context.Attempts, XMEMORY_ACQUIRE) == iAttempts),
		"TCP cached small send attempted global allocation"
	);
	xrtAtomic32Store(&Context.Fail, 0, XMEMORY_RELEASE);
	testTcpOomWait(&Context.Read, 5,
		"TCP cached small send did not reach the peer");

	testRequire(xrtNetEnginePost(
		pEngine,
		xrtNetWorkerIndex(xrtNetStreamWorker(pClient)),
		testTcpOomBuffer,
		&Context
	), "TCP buffer OOM task post failed");
	testTcpOomWait(&Context.BufferDone, 1,
		"TCP buffer OOM task did not finish");

	pTaken = (char*)xrtMalloc(8193);
	pRejectedRef = (char*)xrtMalloc(8193);
	pRefs = (xnetref*)malloc(1024 * sizeof(xnetref));
	testRequire((pTaken != NULL) && (pRejectedRef != NULL) &&
		 (pRefs != NULL),
		"TCP OOM rejected payload allocation failed");
	pTaken[0] = 'T';
	memset(CopyPayload, 'C', sizeof(CopyPayload));
	Spans[0].Data = (cbytes)CopyPayload;
	Spans[0].Size = sizeof(CopyPayload) / 2;
	Spans[1].Data = (cbytes)CopyPayload + Spans[0].Size;
	Spans[1].Size = sizeof(CopyPayload) - Spans[0].Size;
	for ( size_t i = 0; i < 1024; i++ ) {
		pRefs[i] = (xnetref){
			(cbytes)CopyPayload + i,
			1,
			testTcpOomRelease,
			&Context
		};
	}
	xrtAtomic32Store(&Context.Fail, 1, XMEMORY_RELEASE);
	testRequire(xrtNetStreamSend(
		pClient,
		CopyPayload,
		sizeof(CopyPayload)
	) ==
		 XNET_RESULT_ERROR, "TCP copy send survived OOM");
	testRequire(xrtNetStreamSendVec(pClient, Spans, 2) ==
		 XNET_RESULT_ERROR, "TCP vector send survived OOM");
	testRequire(xrtNetStreamSendRefs(pClient, pRefs, 1024) ==
		 XNET_RESULT_ERROR, "TCP reference batch survived OOM");
	testRequire(xrtNetStreamSendRef(
		pClient,
		pRejectedRef,
		8193,
		testTcpOomRelease,
		&Context
	) == XNET_RESULT_AGAIN,
		"TCP reference send exceeded its hard limit");
	testRequire(xrtNetStreamSendTake(pClient, pTaken, 8193) ==
		 XNET_RESULT_AGAIN, "TCP taken send exceeded its hard limit");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		 (xrtNetStreamPending(pClient) == 0) &&
		 (xrtAtomic32Load(
			&Context.Released,
			XMEMORY_ACQUIRE
		 ) == 0) && (xrtNetStreamState(pClient) == XNET_STREAM_OPEN),
		"TCP send OOM changed budget, ownership, or state");
	xrtClearError();

	xrtAtomic32Store(&Context.Fail, 0, XMEMORY_RELEASE);
	xrtFree(pTaken);
	xrtFree(pRejectedRef);
	free(pRefs);
	testRequire(xrtNetStreamSend(pClient, "R", 1) ==
		 XNET_RESULT_OK, "TCP send did not recover after OOM");
	testTcpOomWait(&Context.Read, 6,
		"TCP peer did not receive data after OOM recovery");
	testRequire(xrtNetStreamStats(pClient, &Stats) &&
		 (Stats.SendRejected == 2) && (Stats.QueuedBytes == 0),
		"TCP OOM recovery stats mismatch");

	/* 分配器重入生命周期入口不得等待当前发送提交者。 */
	xrtAtomic64Store(
		&Context.ReenterThread,
		xrtThreadCurrentId(),
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(&Context.ReenterShutdown, 1, XMEMORY_RELEASE);
	testRequire(xrtNetStreamSend(
		pClient,
		CopyPayload,
		sizeof(CopyPayload)
	) == XNET_RESULT_OK, "TCP reentrant shutdown rejected prior send");
	testRequire(xrtAtomic32Load(
		&Context.ShutdownReturned,
		XMEMORY_ACQUIRE
	) == 1, "TCP reentrant shutdown did not return from allocator");
	{
		xdeadline iDeadline = xrtDeadlineAfter(5000000u);

		do {
			testRequire(xrtNetStreamStats(pClient, &Stats),
				"TCP reentrant shutdown stats failed");
			testRequire(!xrtDeadlineExpired(iDeadline),
				"TCP reentrant shutdown did not reach the Worker");
			xrtThreadYield();
		} while ( !Stats.WriteEnded );
	}

	xrtAtomic32Store(&Context.Fail, 1, XMEMORY_RELEASE);
	testRequire(xrtNetStreamShutdownWrite(pClient) &&
		 xrtNetStreamClose(pClient) &&
		 xrtNetStreamClose(Context.Server),
		"TCP lifecycle request allocated during OOM");
	testTcpOomWait(&Context.Close, 2,
		"TCP OOM recovery close callbacks missing");
	xrtNetStreamDestroy(pClient);
	xrtNetStreamDestroy(Context.Server);

	xrtAtomic32Store(&Context.Fail, 0, XMEMORY_RELEASE);
	pClient = xrtNetStreamConnect(
		pEngine,
		&Address,
		0,
		&StreamConfig,
		&StreamEvents,
		&Context
	);
	testRequire(pClient != NULL, "TCP OOM abort client create failed");
	Context.Client = pClient;
	testTcpOomWait(&Context.Accepted, 2,
		"TCP OOM abort accept callback missing");
	testTcpOomWait(&Context.Open, 4,
		"TCP OOM abort open callbacks missing");

	xrtAtomic32Store(&Context.ExpectAbort, 1, XMEMORY_RELEASE);
	xrtAtomic32Store(&Context.Fail, 1, XMEMORY_RELEASE);
	testRequire(xrtNetStreamAbort(pClient) &&
		 xrtNetStreamClose(Context.Server),
		"TCP abort allocated during OOM");
	testTcpOomWait(&Context.Aborted, 1,
		"TCP OOM abort callback missing");
	testTcpOomWait(&Context.Close, 4,
		"TCP OOM abort peer close callback missing");
	xrtNetStreamDestroy(pClient);
	xrtNetStreamDestroy(Context.Server);

	xrtAtomic32Store(&Context.Fail, 0, XMEMORY_RELEASE);
	testRequire(xrtNetListenerClose(pListener),
		"TCP OOM listener close failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP OOM engine destroy failed");
	testRequire(xrtAtomic32Load(
		&Context.Close,
		XMEMORY_ACQUIRE
	) == 4, "TCP close callback was emitted more than once");
	testRequire(xrtAtomic64Load(
		&Context.Attempts,
		XMEMORY_ACQUIRE
	) != 0, "TCP OOM allocator observed no attempts");
	printf(
		"[PASS] network TCP OOM (%s)\n",
		TEST_TCP_OOM_BACKEND_NAME
	);
	return 0;
}
