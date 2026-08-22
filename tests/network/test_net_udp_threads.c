#include "../test.h"



#if !defined(TEST_UDP_BACKEND)
	#define TEST_UDP_BACKEND XNET_PORT_SELECT
	#define TEST_UDP_BACKEND_NAME "select"
#endif



#define TEST_UDP_SEND_THREADS 4u
#define TEST_UDP_SEND_MINIMUM 128u



typedef struct testudpthreads testudpthreads;



typedef struct testudpproducer {
	testudpthreads* Context;
	uint8 Byte;
} testudpproducer;



typedef struct testudpserver {
	xatomic64 Received;
	xatomic32 Failure;
} testudpserver;



typedef struct testudpcloserace {
	xatomic32 Open;
	xatomic32 ReleaseOpen;
	xatomic32 Received;
	xatomic32 Close;
	xatomic32 Failure;
} testudpcloserace;



struct testudpthreads {
	xnetudp* Udp;
	bool Abort;
	bool BlockOpen;
	xatomic32 Open;
	xatomic32 ReleaseOpen;
	xatomic32 Started;
	xatomic32 ProducersClosed;
	xatomic32 Failure;
	xatomic32 Close;
	xatomic64 Attempts;
	xatomic64 Accepted;
	xatomic64 Again;
	xatomic64 Released;
};



/* 在截止时间前等待 32 位计数达到下限。 */
static void testUdpThreadsWait32(
	xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(15000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(iDeadline), sMessage);
		xrtThreadYield();
	}
}



/* 在截止时间前等待 64 位计数达到下限。 */
static void testUdpThreadsWait64(
	xatomic64* pValue,
	uint64 iExpected,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(15000000u);

	while ( xrtAtomic64Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(iDeadline), sMessage);
		xrtThreadYield();
	}
}



/* 阻塞 Open 回调，使回环报文完成包与关闭命令形成稳定的先后关系。 */
static void testUdpCloseRaceOpen(xnetudp* pUdp, ptr pData)
{
	testudpcloserace* pRace = (testudpcloserace*)pData;

	(void)pUdp;
	(void)xrtAtomic32FetchAdd(&pRace->Open, 1, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(
		&pRace->ReleaseOpen,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
}



/* 记录关闭命令建立前已经从端口取出的 UDP 报文。 */
static void testUdpCloseRaceReceive(
	xnetudp* pUdp,
	const xnetudpmessage* pMessage,
	ptr pData
)
{
	testudpcloserace* pRace = (testudpcloserace*)pData;

	(void)pUdp;
	(void)pMessage;
	(void)xrtAtomic32FetchAdd(&pRace->Received, 1, XMEMORY_RELEASE);
}



/* 关闭竞态必须只发布一次正常终态。 */
static void testUdpCloseRaceClose(
	xnetudp* pUdp,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testudpcloserace* pRace = (testudpcloserace*)pData;

	(void)pUdp;
	if ( (Result != XNET_RESULT_OK) || (pError != NULL) ) {
		xrtAtomic32Store(&pRace->Failure, 1, XMEMORY_RELEASE);
	}
	(void)xrtAtomic32FetchAdd(&pRace->Close, 1, XMEMORY_RELEASE);
}



/*
	 * EventBatch 为 1 时先取出一个接收完成，Close 命令随后建立关闭，其余完成包
	 * 即使报告成功，也必须释放槽位并让 UDP 从 CLOSING 收敛到 CLOSED。
 */
static void testUdpCompletionCloseRace(xnetengine* pEngine)
{
	testudpcloserace Race;
	xnetudpconfig Config;
	xnetudpevents Events;
	xnetudpstats Stats;
	xnetaddr Address;
	xnetsocket Sender;
	xnetudp* pUdp;
	const uint8 Data = 0x5a;

	memset(&Race, 0, sizeof(Race));
	memset(&Events, 0, sizeof(Events));
	Events.Open = testUdpCloseRaceOpen;
	Events.Receive = testUdpCloseRaceReceive;
	Events.Close = testUdpCloseRaceClose;
	xrtNetUdpConfigInit(&Config);
	Config.ReceiveConcurrency = 4;
	testRequire(xrtNetAddrLoopback(
		&Address,
		XNET_FAMILY_IPV4,
		0
	), "UDP close race address failed");
	pUdp = xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&Config,
		&Events,
		&Race
	);
	testRequire((pUdp != NULL) && xrtNetUdpLocal(pUdp, &Address),
		"UDP close race bind failed");
	testUdpThreadsWait32(&Race.Open, 1,
		"UDP close race Open callback missing");

	Sender = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM,
		XNET_SOCKET_NONBLOCK
	);
	testRequire(Sender != NULL, "UDP close race sender failed");
	for ( size_t i = 0; i < 32; i++ ) {
		size_t iSent = 0;

		testRequire(
			xrtNetSocketSendTo(
				Sender,
				&Data,
				1,
				&iSent,
				&Address
			) == XNET_RESULT_OK && (iSent == 1),
			"UDP close race datagram send failed"
		);
	}
	xrtSleep(20);
	testRequire(xrtNetUdpClose(pUdp),
		"UDP close race close request failed");
	xrtAtomic32Store(&Race.ReleaseOpen, 1, XMEMORY_RELEASE);
	testUdpThreadsWait32(&Race.Close, 1,
		"UDP close race did not reach CLOSED");
	testRequire(xrtNetUdpStats(pUdp, &Stats),
		"UDP close race stats failed");
	testRequire((Stats.State == XNET_UDP_CLOSED) &&
		(Stats.ActiveReceives == 0) &&
		(xrtAtomic32Load(&Race.Received, XMEMORY_ACQUIRE) <= 1) &&
		(xrtAtomic32Load(&Race.Close, XMEMORY_ACQUIRE) == 1) &&
		(xrtAtomic32Load(&Race.Failure, XMEMORY_ACQUIRE) == 0),
		"UDP close race terminal state mismatch");
	testRequire(xrtNetSocketClose(Sender),
		"UDP close race sender cleanup failed");
	xrtNetUdpDestroy(pUdp);
}



/* 服务器推送回调只计数，并验证每个数据报保持一字节边界。 */
static void testUdpThreadsReceive(
	xnetudp* pUdp,
	const xnetudpmessage* pMessage,
	ptr pData
)
{
	testudpserver* pServer = (testudpserver*)pData;

	(void)pUdp;
	if ( (pMessage == NULL) || (pMessage->Size != 1) ) {
		xrtAtomic32Store(&pServer->Failure, 1, XMEMORY_RELEASE);
		return;
	}
	(void)xrtAtomic64FetchAdd(&pServer->Received, 1, XMEMORY_RELEASE);
}



/* Abort 场景阻塞 Open，稳定制造尚未挂入 Worker 的发送命令。 */
static void testUdpThreadsOpen(xnetudp* pUdp, ptr pData)
{
	testudpthreads* pContext = (testudpthreads*)pData;

	(void)pUdp;
	(void)xrtAtomic32FetchAdd(&pContext->Open, 1, XMEMORY_RELEASE);
	while ( pContext->BlockOpen &&
		 (xrtAtomic32Load(
			&pContext->ReleaseOpen,
			XMEMORY_ACQUIRE
		 ) == 0) ) {
		xrtThreadYield();
	}
}



/* UDP 并发压力中不应出现可恢复 IO 错误。 */
static void testUdpThreadsError(
	xnetudp* pUdp,
	const xerror* pError,
	ptr pData
)
{
	testudpthreads* pContext = (testudpthreads*)pData;

	(void)pUdp;
	(void)pError;
	xrtAtomic32Store(&pContext->Failure, 1, XMEMORY_RELEASE);
}



/* 验证正常关闭和主动中止使用各自唯一的终态。 */
static void testUdpThreadsClose(
	xnetudp* pUdp,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testudpthreads* pContext = (testudpthreads*)pData;
	xnetresult Expected = pContext->Abort ?
		XNET_RESULT_CANCELLED : XNET_RESULT_OK;

	(void)pUdp;
	if ( (Result != Expected) || (pError != NULL) ) {
		xrtAtomic32Store(&pContext->Failure, 1, XMEMORY_RELEASE);
	}
	(void)xrtAtomic32FetchAdd(&pContext->Close, 1, XMEMORY_RELEASE);
}



/* 每个已受理引用数据报必须恰好释放一次。 */
static void testUdpThreadsRelease(
	ptr pData,
	cbytes pBytes,
	size_t iSize
)
{
	testudpthreads* pContext = (testudpthreads*)pData;

	(void)pBytes;
	if ( iSize != 1 ) {
		xrtAtomic32Store(&pContext->Failure, 1, XMEMORY_RELEASE);
	}
	(void)xrtAtomic64FetchAdd(&pContext->Released, 1, XMEMORY_RELEASE);
}



/* 持续竞争引用发送，直到 Close 或 Abort 原子封闭入口。 */
static int32 testUdpThreadsProducer(ptr pData)
{
	testudpproducer* pProducer = (testudpproducer*)pData;
	testudpthreads* pContext = pProducer->Context;

	(void)xrtAtomic32FetchAdd(&pContext->Started, 1, XMEMORY_RELEASE);
	for ( ;; ) {
		xnetresult Result;

		(void)xrtAtomic64FetchAdd(
			&pContext->Attempts,
			1,
			XMEMORY_RELAXED
		);
		Result = xrtNetUdpSendRef(
			pContext->Udp,
			&pProducer->Byte,
			1,
			testUdpThreadsRelease,
			pContext
		);
		if ( Result == XNET_RESULT_OK ) {
			(void)xrtAtomic64FetchAdd(
				&pContext->Accepted,
				1,
				XMEMORY_RELEASE
			);
			continue;
		}
		if ( Result == XNET_RESULT_AGAIN ) {
			(void)xrtAtomic64FetchAdd(
				&pContext->Again,
				1,
				XMEMORY_RELAXED
			);
			xrtThreadYield();
			continue;
		}
		if ( Result == XNET_RESULT_CLOSED ) {
			(void)xrtAtomic32FetchAdd(
				&pContext->ProducersClosed,
				1,
				XMEMORY_RELEASE
			);
			return 0;
		}
		xrtAtomic32Store(&pContext->Failure, 1, XMEMORY_RELEASE);
		return 1;
	}
}



/* 运行一种并发终止模式，并核对预算、统计和所有权终态。 */
static void testUdpThreadsRun(
	xnetengine* pEngine,
	const xnetaddr* pServerAddress,
	testudpserver* pServer,
	bool bAbort
)
{
	testudpthreads Context;
	testudpproducer Producers[TEST_UDP_SEND_THREADS];
	xthread* Threads[TEST_UDP_SEND_THREADS];
	xnetudpconfig Config;
	xnetudpevents Events;
	xnetudpstats Stats;
	uint64 iReceivedBase;
	uint64 iReceived;
	uint64 iAccepted;
	uint64 iAgain;

	memset(&Context, 0, sizeof(Context));
	Context.Abort = bAbort;
	Context.BlockOpen = bAbort;
	memset(&Events, 0, sizeof(Events));
	Events.Open = testUdpThreadsOpen;
	Events.Error = testUdpThreadsError;
	Events.Close = testUdpThreadsClose;
	xrtNetUdpConfigInit(&Config);
	Config.SendHighWater = 128;
	Config.SendLowWater = 32;
	Config.SendLimit = 256;
	Config.SendPacketLimit = 256;
	iReceivedBase = xrtAtomic64Load(&pServer->Received, XMEMORY_ACQUIRE);
	Context.Udp = xrtNetUdpConnect(
		pEngine,
		pServerAddress,
		0,
		&Config,
		&Events,
		&Context
	);
	testRequire(Context.Udp != NULL,
		"UDP threaded client create failed");
	testUdpThreadsWait32(&Context.Open, 1,
		"UDP threaded open callback missing");

	for ( uint32 i = 0; i < TEST_UDP_SEND_THREADS; i++ ) {
		Producers[i].Context = &Context;
		Producers[i].Byte = (uint8)('A' + i);
		Threads[i] = xrtThreadCreate(
			testUdpThreadsProducer,
			&Producers[i],
			0
		);
		testRequire(Threads[i] != NULL,
			"UDP send producer create failed");
	}
	testUdpThreadsWait32(
		&Context.Started,
		TEST_UDP_SEND_THREADS,
		"UDP send producers did not start"
	);
	testUdpThreadsWait64(
		&Context.Accepted,
		TEST_UDP_SEND_MINIMUM,
		"UDP send producers made no progress"
	);
	if ( bAbort ) {
		testRequire(xrtNetUdpAbort(Context.Udp),
			"UDP concurrent abort request failed");
		xrtAtomic32Store(&Context.ReleaseOpen, 1, XMEMORY_RELEASE);
	} else {
		testRequire(xrtNetUdpClose(Context.Udp),
			"UDP concurrent close request failed");
	}

	for ( uint32 i = 0; i < TEST_UDP_SEND_THREADS; i++ ) {
		testRequire(xrtThreadWait(Threads[i]) == XWAIT_OK,
			"UDP send producer wait failed");
		testRequire(xrtThreadExitCode(Threads[i]) == 0,
			"UDP send producer returned failure");
		xrtThreadDestroy(Threads[i]);
	}
	testUdpThreadsWait32(&Context.Close, 1,
		"UDP threaded close callback missing");
	iAccepted = xrtAtomic64Load(&Context.Accepted, XMEMORY_ACQUIRE);
	iAgain = xrtAtomic64Load(&Context.Again, XMEMORY_ACQUIRE);
	testUdpThreadsWait64(&Context.Released, iAccepted,
		"UDP accepted reference was not released");
	if ( !bAbort ) {
		/* UDP 不保证端到端交付；本测试只要求回环接收路径确实工作。 */
		testUdpThreadsWait64(
			&pServer->Received,
			iReceivedBase + 1,
			"UDP normal close delivered no datagram"
		);
	}
	testRequire(xrtNetUdpStats(Context.Udp, &Stats),
		"UDP threaded stats failed");
	iReceived = xrtAtomic64Load(
		&pServer->Received,
		XMEMORY_ACQUIRE
	) - iReceivedBase;
	testRequire(
		(xrtAtomic32Load(&Context.Failure, XMEMORY_ACQUIRE) == 0) &&
		(xrtAtomic32Load(
			&Context.ProducersClosed,
			XMEMORY_ACQUIRE
		 ) == TEST_UDP_SEND_THREADS) &&
		(xrtAtomic64Load(&Context.Released, XMEMORY_ACQUIRE) ==
		 iAccepted) &&
		(Stats.SendRejected == iAgain) &&
		(Stats.QueuedBytes == 0) &&
		(Stats.QueuedPackets == 0) &&
		(Stats.SendErrors == 0),
		"UDP threaded ownership, budget, or error mismatch");
	if ( bAbort ) {
		testRequire(Stats.SentPackets == 0,
			"UDP abort let a blocked send command pass");
	} else {
		testRequire((Stats.SentPackets == iAccepted) &&
			(iReceived <= iAccepted),
			"UDP normal close lost a local send or duplicated delivery");
	}
	xrtNetUdpDestroy(Context.Udp);
}



/* 验证多线程 Send 与正常关闭、主动中止之间的单终态契约。 */
int main(void)
{
	testudpserver Server;
	xnetengineconfig EngineConfig;
	xnetudpconfig ServerConfig;
	xnetudpevents ServerEvents;
	xnetengine* pEngine;
	xnetudp* pUdpServer;
	xnetaddr ServerAddress;

	memset(&Server, 0, sizeof(Server));
	memset(&ServerEvents, 0, sizeof(ServerEvents));
	ServerEvents.Receive = testUdpThreadsReceive;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_UDP_BACKEND;
	EngineConfig.Workers = 1;
	EngineConfig.EventBatch = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"UDP threaded engine start failed");
	testUdpCompletionCloseRace(pEngine);
	xrtNetUdpConfigInit(&ServerConfig);
	ServerConfig.ReceiveConcurrency = 4;
	testRequire(xrtNetAddrLoopback(
		&ServerAddress,
		XNET_FAMILY_IPV4,
		0
	), "UDP threaded server address failed");
	pUdpServer = xrtNetUdpBind(
		pEngine,
		&ServerAddress,
		0,
		&ServerConfig,
		&ServerEvents,
		&Server
	);
	testRequire((pUdpServer != NULL) &&
		xrtNetUdpLocal(pUdpServer, &ServerAddress),
		"UDP threaded server create failed");
	while ( xrtNetUdpState(pUdpServer) != XNET_UDP_OPEN ) {
		xrtThreadYield();
	}

	testUdpThreadsRun(pEngine, &ServerAddress, &Server, false);
	testUdpThreadsRun(pEngine, &ServerAddress, &Server, true);
	testRequire(xrtAtomic32Load(
		&Server.Failure,
		XMEMORY_ACQUIRE
	) == 0, "UDP threaded server callback failed");
	testRequire(xrtNetUdpClose(pUdpServer),
		"UDP threaded server close failed");
	while ( xrtNetUdpState(pUdpServer) != XNET_UDP_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetUdpDestroy(pUdpServer);
	testRequire(xrtNetEngineDestroy(pEngine),
		"UDP threaded engine destroy failed");
	printf("[PASS] network UDP %s send-close-abort threads\n",
		TEST_UDP_BACKEND_NAME);
	return 0;
}
