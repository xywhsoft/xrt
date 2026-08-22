#include "../test.h"



#if !defined(TEST_TCP_BACKEND)
	#define TEST_TCP_BACKEND XNET_PORT_SELECT
	#define TEST_TCP_BACKEND_NAME "select"
#endif



typedef struct testtcpcontext testtcpcontext;



typedef struct testtcpendpoint {
	testtcpcontext* Context;
	xnetstream* Stream;
	xatomic32 Open;
	xatomic32 ReadBytes;
	xatomic32 End;
	xatomic32 HighWater;
	xatomic32 LowWater;
	xatomic32 Drain;
	xatomic32 Close;
	xatomic32 CloseError;
	xatomic32 PauseAt;
	char Text[256];
	bool Server;
} testtcpendpoint;



struct testtcpcontext {
	testtcpendpoint Client;
	testtcpendpoint Server;
	const xnetstreamevents* StreamEvents;
	xatomic32 Accepted;
	xatomic32 Released;
	xatomic32 ListenerErrors;
	xatomic32 ListenerClose;
	xatomic32 BufferSent;
	xatomic32 IdleChecks;
	xatomic32 IdleLiveBlocks;
};



/* 在测试截止时间前等待原子值达到下限。 */
static void testTcpWait(
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



/* 在 Worker 队列稳定后记录空闲 Stream 实际占用的接收块。 */
static void testTcpIdlePool(xnetworker* pWorker, ptr pData)
{
	testtcpcontext* pContext = (testtcpcontext*)pData;
	xnetbufpoolinfo Info;

	xrtNetBufPoolGet(xrtNetWorkerBufPool(pWorker), &Info);
	(void)xrtAtomic32FetchAdd(
		&pContext->IdleLiveBlocks,
		(uint32)Info.LiveBlocks,
		XMEMORY_RELAXED
	);
	(void)xrtAtomic32FetchAdd(
		&pContext->IdleChecks,
		1,
		XMEMORY_RELEASE
	);
}



/* 在两个 Stream 所属 Worker 稳定后验证没有活跃接收块。 */
static void testTcpRequireIdlePool(
	xnetengine* pEngine,
	testtcpcontext* pContext,
	cstr sMessage
)
{
	xnetworker* pClientWorker = xrtNetStreamWorker(
		pContext->Client.Stream
	);
	xnetworker* pServerWorker = xrtNetStreamWorker(
		pContext->Server.Stream
	);
	uint32 iChecks = (pClientWorker == pServerWorker) ? 1u : 2u;

	xrtAtomic32Store(&pContext->IdleChecks, 0, XMEMORY_RELAXED);
	xrtAtomic32Store(&pContext->IdleLiveBlocks, 0, XMEMORY_RELAXED);
	testRequire(xrtNetEnginePost(
		pEngine,
		xrtNetWorkerIndex(pClientWorker),
		testTcpIdlePool,
		pContext
	), "TCP client idle-pool check post failed");
	if ( pClientWorker != pServerWorker ) {
		testRequire(xrtNetEnginePost(
			pEngine,
			xrtNetWorkerIndex(pServerWorker),
			testTcpIdlePool,
			pContext
		), "TCP server idle-pool check post failed");
	}
	testTcpWait(&pContext->IdleChecks, iChecks,
		"TCP idle-pool checks did not finish");
	testRequire(xrtAtomic32Load(
		&pContext->IdleLiveBlocks,
		XMEMORY_ACQUIRE
	) == 0, sMessage);
}



/* 记录 Stream 已经在所属 Worker 上发布。 */
static void testTcpOpen(xnetstream* pStream, ptr pData)
{
	testtcpendpoint* pEndpoint = (testtcpendpoint*)pData;

	testRequire((pEndpoint != NULL) &&
		 (xrtNetStreamWorker(pStream) != NULL) &&
		 xrtNetWorkerIsCurrent(xrtNetStreamWorker(pStream)),
		"TCP open callback worker mismatch");
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Open,
		1,
		XMEMORY_RELEASE
	);
}



/* 消费全部接收缓冲；服务端把字节原样回送。 */
static void testTcpRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	testtcpendpoint* pEndpoint = (testtcpendpoint*)pData;
	uint32 iOffset = xrtAtomic32Load(
		&pEndpoint->ReadBytes,
		XMEMORY_RELAXED
	);
	size_t iAvailable = xrtNetBufSize(pBuffer);
	size_t iRead;

	testRequire((iOffset <= sizeof(pEndpoint->Text)) &&
		 (iAvailable <= (sizeof(pEndpoint->Text) - iOffset)),
		"TCP test receive buffer overflow");
	iRead = xrtNetBufPeek(
		pBuffer,
		0,
		pEndpoint->Text + iOffset,
		iAvailable
	);
	testRequire(iRead == iAvailable,
		"TCP read callback did not inspect its full buffer");
	if ( pEndpoint->Server ) {
		testRequire(xrtNetStreamSendBuffer(
			pStream,
			pBuffer
		) == XNET_RESULT_OK && xrtNetBufEmpty(pBuffer),
			"TCP zero-copy buffer echo failed");
	} else {
		testRequire(xrtNetBufConsume(pBuffer, iRead) == iRead,
			"TCP client read did not consume its buffer");
	}
	if ( (xrtAtomic32Load(
		&pEndpoint->PauseAt,
		XMEMORY_ACQUIRE
	) != 0) && ((iOffset + (uint32)iRead) >= xrtAtomic32Load(
		&pEndpoint->PauseAt,
		XMEMORY_RELAXED
	)) ) {
		xrtNetStreamPause(pStream);
	}
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->ReadBytes,
		(uint32)iRead,
		XMEMORY_RELEASE
	);
}



/* 记录对端写半关闭。 */
static void testTcpEnd(xnetstream* pStream, ptr pData)
{
	testtcpendpoint* pEndpoint = (testtcpendpoint*)pData;

	(void)pStream;
	(void)xrtAtomic32FetchAdd(&pEndpoint->End, 1, XMEMORY_RELEASE);
}



/* 记录发送队列首次越过高水位。 */
static void testTcpHighWater(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	testtcpendpoint* pEndpoint = (testtcpendpoint*)pData;

	(void)pStream;
	testRequire(iQueued >= 1, "TCP high-water value mismatch");
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->HighWater,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录发送队列回落到低水位。 */
static void testTcpLowWater(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	testtcpendpoint* pEndpoint = (testtcpendpoint*)pData;

	(void)pStream;
	testRequire(iQueued == 0, "TCP low-water value mismatch");
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->LowWater,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录发送队列排空。 */
static void testTcpDrain(xnetstream* pStream, ptr pData)
{
	testtcpendpoint* pEndpoint = (testtcpendpoint*)pData;

	(void)pStream;
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Drain,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证正常关闭没有附带结构化错误。 */
static void testTcpClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testtcpendpoint* pEndpoint = (testtcpendpoint*)pData;

	testRequire(xrtNetWorkerIsCurrent(xrtNetStreamWorker(pStream)),
		"TCP close callback worker mismatch");
	if ( (Result != XNET_RESULT_OK) || (pError != NULL) ) {
		(void)xrtAtomic32FetchAdd(
			&pEndpoint->CloseError,
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



/* 接管已接受 Stream 的调用方引用并设置其独立用户数据。 */
static bool testTcpAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testtcpcontext* pContext = (testtcpcontext*)pData;

	(void)pListener;
	testRequire(xrtNetStreamSetEvents(
		pStream,
		pContext->StreamEvents,
		&pContext->Server
	), "accepted TCP stream event takeover failed");
	pContext->Server.Stream = pStream;
	(void)xrtAtomic32FetchAdd(
		&pContext->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* Listener 错误必须单独统计，不能伪装成用户拒绝。 */
static void testTcpListenerError(
	xnetlistener* pListener,
	const xerror* pError,
	ptr pData
)
{
	testtcpcontext* pContext = (testtcpcontext*)pData;

	(void)pListener;
	testRequire(pError != NULL, "TCP listener error object missing");
	(void)xrtAtomic32FetchAdd(
		&pContext->ListenerErrors,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 Listener 唯一关闭回调。 */
static void testTcpListenerClose(xnetlistener* pListener, ptr pData)
{
	testtcpcontext* pContext = (testtcpcontext*)pData;

	(void)pListener;
	(void)xrtAtomic32FetchAdd(
		&pContext->ListenerClose,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录零复制引用离开发送队列。 */
static void testTcpRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	testtcpcontext* pTest = (testtcpcontext*)pContext;

	testRequire((pData != NULL) && (iSize != 0),
		"TCP send reference release arguments mismatch");
	(void)xrtAtomic32FetchAdd(
		&pTest->Released,
		1,
		XMEMORY_RELEASE
	);
}



/* 在客户端 Worker 上以三个独立块验证缓冲链接管和顺序。 */
static void testTcpSendBuffer(
	xnetworker* pWorker,
	ptr pData
)
{
	testtcpcontext* pContext = (testtcpcontext*)pData;
	xnetbuf Buffer;
	xnetbuf Rejected;
	xnetbuf Reserved;
	xnetwspan Write;
	static const char Data[] = "LMN";

	testRequire(xrtNetBufInit(
		&Rejected,
		xrtNetWorkerBufPool(pWorker)
	) && xrtNetBufAppend(&Rejected, "OVER", 4),
		"TCP rejected buffer send setup failed");
	testRequire(xrtNetStreamSendBuffer(
		pContext->Client.Stream,
		&Rejected
	) == XNET_RESULT_AGAIN && (xrtNetBufSize(&Rejected) == 4),
		"TCP buffer send crossed its hard write limit");
	xrtNetBufClear(&Rejected);
	testRequire(xrtNetBufInit(
		&Reserved,
		xrtNetWorkerBufPool(pWorker)
	) && xrtNetBufReserve(&Reserved, 1, &Write),
		"TCP reserved buffer send setup failed");
	xrtClearError();
	testRequire(xrtNetStreamSendBuffer(
		pContext->Client.Stream,
		&Reserved
	) == XNET_RESULT_ERROR && (Reserved.Reserved != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"TCP buffer send accepted an active write reservation");
	testRequire(xrtNetBufCancel(&Reserved),
		"TCP reserved buffer cleanup failed");
	xrtNetBufClear(&Reserved);
	xrtClearError();
	testRequire(xrtNetBufInit(
		&Buffer,
		xrtNetWorkerBufPool(pWorker)
	), "TCP buffer send setup failed");
	for ( size_t i = 0; i < sizeof(Data) - 1; i++ ) {
		bool bAdded;

		if ( (i + 1) == (sizeof(Data) - 1) ) {
			bAdded = xrtNetBufAppendRef(
				&Buffer,
				Data + i,
				1,
				testTcpRelease,
				pContext
			);
		} else {
			bAdded = xrtNetBufAppendBorrow(
				&Buffer,
				Data + i,
				1
			);
		}

		testRequire(bAdded, "TCP buffer send block setup failed");
	}
	testRequire(xrtNetStreamSendBuffer(
		pContext->Client.Stream,
		&Buffer
	) == XNET_RESULT_OK && xrtNetBufEmpty(&Buffer),
		"TCP multi-block buffer send failed");
	xrtNetBufClear(&Buffer);
	(void)xrtAtomic32FetchAdd(
		&pContext->BufferSent,
		1,
		XMEMORY_RELEASE
	);
}



/* 覆盖 select fallback 的完整 TCP 生命周期和关键边界。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetstreamconfig StreamConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents StreamEvents;
	testtcpcontext Context;
	xnetlistenerstats ListenerStats;
	xnetstreamstats ClientStats;
	xnetstreamstats ServerStats;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetaddr Address;
	xnetaddr ClientLocal;
	xnetaddr ClientRemote;
	xnetbuf OffWorker;
	xnetspan Spans[3];
	xnetref Refs[3];
	char* pTaken;
	char iVecOne = 'V';
	char iVecTwo = 'E';
	char Burst[256];
	static const char sRejected[] = "fail";
	static const char sReferenced[] = "B";
	static const char sRefOne[] = "R";
	static const char sRefTwo[] = "S";

	memset(&Context, 0, sizeof(Context));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&StreamEvents, 0, sizeof(StreamEvents));
	Context.Client.Context = &Context;
	Context.Server.Context = &Context;
	Context.Server.Server = true;
	ListenerEvents.Accept = testTcpAccept;
	ListenerEvents.Error = testTcpListenerError;
	ListenerEvents.Close = testTcpListenerClose;
	StreamEvents.Open = testTcpOpen;
	StreamEvents.Read = testTcpRead;
	StreamEvents.End = testTcpEnd;
	StreamEvents.HighWater = testTcpHighWater;
	StreamEvents.LowWater = testTcpLowWater;
	StreamEvents.Drain = testTcpDrain;
	StreamEvents.Close = testTcpClose;
	Context.StreamEvents = &StreamEvents;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TCP_BACKEND;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP select engine start failed");

	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TCP loopback address setup failed");
	ListenConfig.AcceptConcurrency = 4;
	ListenConfig.Stream.ReadSize = 64;
	ListenConfig.Stream.ReadLimit = 256;
	ListenConfig.Stream.ReadMode = XNET_STREAM_READ_PROBE;
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		&StreamEvents,
		&Context
	);
	testRequire(pListener != NULL, "TCP loopback listener failed");
	testRequire(xrtNetListenerLocal(pListener, &Address) &&
		 (Address.Port != 0), "TCP dynamic listener port missing");

	xrtNetStreamConfigInit(&StreamConfig);
	StreamConfig.ReadSize = 256;
	StreamConfig.ReadLimit = 256;
	StreamConfig.WriteHighWater = 1;
	StreamConfig.WriteLowWater = 0;
	StreamConfig.WriteLimit = 3;
	Context.Client.Stream = xrtNetStreamConnect(
		pEngine,
		&Address,
		1,
		&StreamConfig,
		&StreamEvents,
		&Context.Client
	);
	testRequire(Context.Client.Stream != NULL,
		"TCP loopback connect creation failed");
	testTcpWait(&Context.Accepted, 1, "TCP accept callback missing");
	testTcpWait(&Context.Client.Open, 1, "TCP client open callback missing");
	testTcpWait(&Context.Server.Open, 1, "TCP server open callback missing");
	testTcpRequireIdlePool(pEngine, &Context,
		"idle TCP streams retained receive blocks");

	/* 恰好填满一次接收后，自适应模式必须回到零缓冲探测。 */
	memset(Burst, 'X', sizeof(Burst));
	testRequire(xrtNetStreamSend(
		Context.Server.Stream,
		Burst,
		sizeof(Burst)
	) == XNET_RESULT_OK, "TCP exact-block burst send failed");
	testTcpWait(&Context.Client.ReadBytes, (uint32)sizeof(Burst),
		"TCP exact-block burst was not received");
	testTcpWait(&Context.Server.Drain, 1,
		"TCP exact-block burst did not drain");
	testRequire(memcmp(Context.Client.Text, Burst, sizeof(Burst)) == 0,
		"TCP exact-block burst payload mismatch");
	testTcpRequireIdlePool(pEngine, &Context,
		"adaptive TCP stream retained a receive block after exact burst");
	xrtAtomic32Store(&Context.Client.ReadBytes, 0, XMEMORY_RELEASE);
	memset(Context.Client.Text, 0, sizeof(Context.Client.Text));

	testRequire(xrtNetStreamLocal(
		Context.Client.Stream,
		&ClientLocal
	) && xrtNetStreamRemote(
		Context.Client.Stream,
		&ClientRemote
	) && (ClientLocal.Port != 0) &&
		 (ClientRemote.Port == Address.Port),
		"TCP client addresses mismatch");
	testRequire(xrtNetBufInit(&OffWorker, NULL) &&
		xrtNetBufAppend(&OffWorker, "X", 1),
		"TCP off-worker buffer setup failed");
	xrtClearError();
	testRequire(xrtNetStreamSendBuffer(
		Context.Client.Stream,
		&OffWorker
	) == XNET_RESULT_ERROR && (xrtNetBufSize(&OffWorker) == 1) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"TCP buffer send crossed its worker ownership boundary");
	xrtNetBufClear(&OffWorker);
	xrtClearError();

	pTaken = (char*)xrtMalloc(1);
	testRequire(pTaken != NULL,
		"TCP empty taken send allocation failed");
	testRequire(
		(xrtNetStreamSendTake(
			Context.Client.Stream,
			pTaken,
			0
		 ) == XNET_RESULT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtNetStreamPending(Context.Client.Stream) == 0),
		"TCP empty taken send transferred ambiguous ownership"
	);
	xrtFree(pTaken);
	xrtClearError();
	testRequire(
		(xrtNetStreamSendRef(
			Context.Client.Stream,
			sReferenced,
			0,
			testTcpRelease,
			&Context
		 ) == XNET_RESULT_OK) &&
		(xrtAtomic32Load(
			&Context.Released,
			XMEMORY_ACQUIRE
		 ) == 0),
		"TCP empty reference send transferred ownership"
	);

	testRequire(xrtNetStreamSendRef(
		Context.Client.Stream,
		sRejected,
		sizeof(sRejected) - 1,
		testTcpRelease,
		&Context
	) == XNET_RESULT_AGAIN, "TCP hard write limit was not enforced");
	testRequire(xrtAtomic32Load(
		&Context.Released,
		XMEMORY_ACQUIRE
	) == 0, "rejected TCP reference transferred ownership");
	Refs[0] = (xnetref){
		(cbytes)sRejected,
		2,
		testTcpRelease,
		&Context
	};
	Refs[1] = (xnetref){
		(cbytes)sRejected + 2,
		2,
		testTcpRelease,
		&Context
	};
	testRequire(xrtNetStreamSendRefs(
		Context.Client.Stream,
		Refs,
		2
	) == XNET_RESULT_AGAIN, "TCP reference batch exceeded hard limit");
	testRequire(xrtAtomic32Load(
		&Context.Released,
		XMEMORY_ACQUIRE
	) == 0, "rejected TCP reference batch transferred ownership");
	testRequire(xrtNetStreamSend(
		Context.Client.Stream,
		"A",
		1
	) == XNET_RESULT_OK, "TCP copy send failed");
	testRequire(xrtNetStreamSendRef(
		Context.Client.Stream,
		sReferenced,
		1,
		testTcpRelease,
		&Context
	) == XNET_RESULT_OK, "TCP reference send failed");
	pTaken = (char*)xrtMalloc(1);
	testRequire(pTaken != NULL, "TCP taken send allocation failed");
	pTaken[0] = 'C';
	testRequire(xrtNetStreamSendTake(
		Context.Client.Stream,
		pTaken,
		1
	) == XNET_RESULT_OK, "TCP taken send failed");
	testTcpWait(&Context.Server.ReadBytes, 3,
		"TCP server did not receive ownership payloads");
	testTcpWait(&Context.Client.ReadBytes, 3,
		"TCP client did not receive echoed payloads");
	testTcpWait(&Context.Released, 1,
		"TCP reference release callback missing");
	testTcpWait(&Context.Client.HighWater, 1,
		"TCP high-water callback missing");
	testTcpWait(&Context.Client.LowWater, 1,
		"TCP low-water callback missing");
	testTcpWait(&Context.Client.Drain, 1,
		"TCP drain callback missing");
	testRequire(memcmp(Context.Server.Text, "ABC", 3) == 0 &&
		 memcmp(Context.Client.Text, "ABC", 3) == 0,
		"TCP payload ordering mismatch");
	Spans[0].Data = (cbytes)&iVecOne;
	Spans[0].Size = 1;
	Spans[1].Data = NULL;
	Spans[1].Size = 0;
	Spans[2].Data = (cbytes)&iVecTwo;
	Spans[2].Size = 1;
	testRequire(xrtNetStreamSendVec(
		Context.Client.Stream,
		Spans,
		3
	) == XNET_RESULT_OK, "TCP vector copy send failed");
	iVecOne = 'x';
	iVecTwo = 'x';
	testTcpWait(&Context.Server.ReadBytes, 5,
		"TCP server did not receive vector payload");
	testTcpWait(&Context.Client.ReadBytes, 5,
		"TCP client did not receive vector echo");
	testRequire(memcmp(Context.Client.Text, "ABCVE", 5) == 0,
		"TCP vector send did not copy its input");

	Refs[0] = (xnetref){
		(cbytes)sRefOne,
		1,
		testTcpRelease,
		&Context
	};
	Refs[1] = (xnetref){ NULL, 0, NULL, NULL };
	Refs[2] = (xnetref){
		(cbytes)sRefTwo,
		1,
		testTcpRelease,
		&Context
	};
	testRequire(xrtNetStreamSendRefs(
		Context.Client.Stream,
		Refs,
		3
	) == XNET_RESULT_OK, "TCP reference batch send failed");
	testTcpWait(&Context.Server.ReadBytes, 7,
		"TCP server did not receive reference batch");
	testTcpWait(&Context.Client.ReadBytes, 7,
		"TCP client did not receive reference batch echo");
	testTcpWait(&Context.Released, 3,
		"TCP reference batch release callbacks missing");
	testRequire(memcmp(Context.Client.Text, "ABCVERS", 7) == 0,
		"TCP reference batch ordering mismatch");

	xrtAtomic32Store(&Context.Server.PauseAt, 8, XMEMORY_RELEASE);
	testRequire(xrtNetStreamSend(
		Context.Client.Stream,
		"P",
		1
	) == XNET_RESULT_OK, "TCP paused-read payload send failed");
	testTcpWait(&Context.Server.ReadBytes, 8,
		"TCP pause marker did not reach server");
	testTcpWait(&Context.Client.ReadBytes, 8,
		"TCP pause marker echo did not reach client");
	testRequire(xrtNetStreamSend(
		Context.Client.Stream,
		"Q",
		1
	) == XNET_RESULT_OK, "TCP post-pause payload send failed");
	for ( uint32 i = 0; i < 10000; i++ ) {
		xrtThreadYield();
	}
	testRequire(xrtAtomic32Load(
		&Context.Server.ReadBytes,
		XMEMORY_ACQUIRE
	) == 8, "TCP pause allowed a new read callback");
	xrtAtomic32Store(&Context.Server.PauseAt, 0, XMEMORY_RELEASE);
	testRequire(xrtNetStreamResume(Context.Server.Stream),
		"TCP read resume failed");
	testTcpWait(&Context.Server.ReadBytes, 9,
		"TCP resumed server did not receive payload");
	testTcpWait(&Context.Client.ReadBytes, 9,
		"TCP resumed echo did not reach client");
	testRequire(xrtNetEnginePost(
		pEngine,
		xrtNetWorkerIndex(xrtNetStreamWorker(Context.Client.Stream)),
		testTcpSendBuffer,
		&Context
	), "TCP buffer send task post failed");
	testTcpWait(&Context.BufferSent, 1,
		"TCP buffer send task did not finish");
	testTcpWait(&Context.Server.ReadBytes, 12,
		"TCP server did not receive the multi-block buffer");
	testTcpWait(&Context.Client.ReadBytes, 12,
		"TCP client did not receive the multi-block echo");
	testTcpWait(&Context.Released, 4,
		"TCP buffer send reference release callback missing");

	testRequire(xrtNetStreamShutdownWrite(Context.Client.Stream),
		"TCP write shutdown request failed");
	testTcpWait(&Context.Server.End, 1,
		"TCP peer did not observe write shutdown");
	testRequire(xrtNetStreamSend(
		Context.Server.Stream,
		"Z",
		1
	) == XNET_RESULT_OK, "TCP read side did not survive half-close");
	testTcpWait(&Context.Client.ReadBytes, 13,
		"TCP half-close response missing");
	testRequire(memcmp(Context.Client.Text, "ABCVERSPQLMNZ", 13) == 0,
		"TCP half-close response ordering mismatch");

	testRequire(xrtNetStreamClose(Context.Server.Stream),
		"TCP server close request failed");
	testTcpWait(&Context.Server.Close, 1,
		"TCP server close callback missing");
	testTcpWait(&Context.Client.End, 1,
		"TCP client did not observe server close");
	testRequire(xrtNetStreamClose(Context.Client.Stream),
		"TCP client close request failed");
	testTcpWait(&Context.Client.Close, 1,
		"TCP client close callback missing");
	testRequire(xrtNetStreamError(Context.Client.Stream) == NULL &&
		 (xrtAtomic32Load(
			&Context.Client.CloseError,
			XMEMORY_ACQUIRE
		 ) == 0) && (xrtAtomic32Load(
			&Context.Server.CloseError,
			XMEMORY_ACQUIRE
		 ) == 0), "normal TCP close reported an error");

	testRequire(xrtNetStreamStats(
		Context.Client.Stream,
		&ClientStats
	) && xrtNetStreamStats(
		Context.Server.Stream,
		&ServerStats
	), "TCP stream stats failed");
	testRequire((ClientStats.State == XNET_STREAM_CLOSED) &&
		 (ClientStats.ReceivedBytes == 269) &&
		 (ClientStats.SentBytes == 12) &&
		 (ClientStats.SendRejected == 3) &&
		 (ClientStats.QueuedBytes == 0) &&
		 (ClientStats.PeakQueuedBytes <= 3) &&
		 (ServerStats.ReceivedBytes == 12) &&
		 (ServerStats.SentBytes == 269),
		"TCP stream stats mismatch");

	testRequire(xrtNetListenerClose(pListener),
		"TCP listener close request failed");
	testTcpWait(&Context.ListenerClose, 1,
		"TCP listener close callback missing");
	testRequire(xrtNetListenerStats(pListener, &ListenerStats) &&
		 (ListenerStats.State == XNET_LISTENER_CLOSED) &&
		 (ListenerStats.Accepted == 1) &&
		 (ListenerStats.Rejected == 0) &&
		 (ListenerStats.Errors == 0) &&
		 (ListenerStats.ActiveAccepts == 0) &&
		 (ListenerStats.ActiveDispatches == 0) &&
		 (xrtAtomic32Load(
			&Context.ListenerErrors,
			XMEMORY_ACQUIRE
		 ) == 0), "TCP listener stats mismatch");

	xrtNetStreamDestroy(Context.Client.Stream);
	xrtNetStreamDestroy(Context.Server.Stream);
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP select engine destroy failed");
	printf("[PASS] network TCP %s lifecycle\n", TEST_TCP_BACKEND_NAME);
	return 0;
}
