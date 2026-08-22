#include "../test.h"



#if !defined(TEST_TCP_BACKEND)
	#define TEST_TCP_BACKEND XNET_PORT_SELECT
	#define TEST_TCP_BACKEND_NAME "select"
#endif



#define TEST_TCP_SLOW_PAYLOAD (8u * 1024u * 1024u)



typedef struct testtcpslow {
	xnetstream* Stream;
	xatomic32 Accepted;
	xatomic32 Open;
	xatomic32 HighWater;
	xatomic32 LowWater;
	xatomic32 Drain;
	xatomic32 Close;
	xatomic32 ListenerError;
} testtcpslow;



/* 在测试截止时间前等待原子计数到达下限。 */
static void testTcpSlowWait(
	xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(30000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(iDeadline), sMessage);
		xrtSleep(1);
	}
}



/* 在 Accept 回调内配置借用 Socket，并接管 Stream 引用。 */
static bool testTcpSlowAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testtcpslow* pContext = (testtcpslow*)pData;
	xnetsocket Socket = xrtNetStreamSocket(pStream);

	(void)pListener;
	testRequire(Socket != NULL, "TCP borrowed stream socket missing");
	testRequire(xrtNetSocketSet(
		Socket,
		XNET_OPTION_SEND_BUFFER,
		4096
	), "TCP stream send-buffer setup failed");
	testRequire(xrtNetStreamSetData(pStream, pContext),
		"TCP slow stream data setup failed");
	pContext->Stream = pStream;
	(void)xrtAtomic32FetchAdd(
		&pContext->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 记录 Stream 已经完成公开。 */
static void testTcpSlowOpen(xnetstream* pStream, ptr pData)
{
	testtcpslow* pContext = (testtcpslow*)pData;

	(void)pStream;
	(void)xrtAtomic32FetchAdd(&pContext->Open, 1, XMEMORY_RELEASE);
}



/* 记录发送队列首次越过高水位。 */
static void testTcpSlowHigh(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	testtcpslow* pContext = (testtcpslow*)pData;

	(void)pStream;
	testRequire(iQueued >= (32u * 1024u),
		"TCP slow high-water value mismatch");
	(void)xrtAtomic32FetchAdd(
		&pContext->HighWater,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录发送队列回落到低水位。 */
static void testTcpSlowLow(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	testtcpslow* pContext = (testtcpslow*)pData;

	(void)pStream;
	testRequire(iQueued <= (8u * 1024u),
		"TCP slow low-water value mismatch");
	(void)xrtAtomic32FetchAdd(
		&pContext->LowWater,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录一次完整排空。 */
static void testTcpSlowDrain(xnetstream* pStream, ptr pData)
{
	testtcpslow* pContext = (testtcpslow*)pData;

	(void)pStream;
	(void)xrtAtomic32FetchAdd(&pContext->Drain, 1, XMEMORY_RELEASE);
}



/* 慢对端测试只接受无错误关闭。 */
static void testTcpSlowClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testtcpslow* pContext = (testtcpslow*)pData;

	(void)pStream;
	testRequire((Result == XNET_RESULT_OK) && (pError == NULL),
		"TCP slow stream closed with an error");
	(void)xrtAtomic32FetchAdd(&pContext->Close, 1, XMEMORY_RELEASE);
}



/* Listener 系统错误必须独立暴露。 */
static void testTcpSlowListenerError(
	xnetlistener* pListener,
	const xerror* pError,
	ptr pData
)
{
	testtcpslow* pContext = (testtcpslow*)pData;

	(void)pListener;
	testRequire(pError != NULL, "TCP slow listener error missing");
	(void)xrtAtomic32FetchAdd(
		&pContext->ListenerError,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证慢速接收方不会造成无界发送或丢失背压事件。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents StreamEvents;
	xnetstreamstats Stats;
	testtcpslow Context;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetsocket Peer;
	xnetaddr Address;
	xdeadline iDeadline;
	char* pPayload;
	char Buffer[16384];
	size_t iReceived = 0;

	memset(&Context, 0, sizeof(Context));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&StreamEvents, 0, sizeof(StreamEvents));
	ListenerEvents.Accept = testTcpSlowAccept;
	ListenerEvents.Error = testTcpSlowListenerError;
	StreamEvents.Open = testTcpSlowOpen;
	StreamEvents.HighWater = testTcpSlowHigh;
	StreamEvents.LowWater = testTcpSlowLow;
	StreamEvents.Drain = testTcpSlowDrain;
	StreamEvents.Close = testTcpSlowClose;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TCP_BACKEND;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP slow engine start failed");

	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TCP slow listener address failed");
	ListenConfig.Stream.WriteHighWater = 32u * 1024u;
	ListenConfig.Stream.WriteLowWater = 8u * 1024u;
	ListenConfig.Stream.WriteLimit = TEST_TCP_SLOW_PAYLOAD;
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		&StreamEvents,
		&Context
	);
	testRequire((pListener != NULL) &&
		 xrtNetListenerLocal(pListener, &Address),
		"TCP slow listener start failed");

	Peer = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		0
	);
	testRequire(Peer != NULL, "TCP slow peer open failed");
	testRequire(xrtNetSocketSet(
		Peer,
		XNET_OPTION_RECEIVE_BUFFER,
		4096
	), "TCP slow peer receive-buffer setup failed");
	testRequire(xrtNetSocketConnect(Peer, &Address) == XNET_RESULT_OK,
		"TCP slow peer connect failed");
	testTcpSlowWait(&Context.Accepted, 1,
		"TCP slow accept callback missing");
	testTcpSlowWait(&Context.Open, 1,
		"TCP slow open callback missing");
	testRequire(xrtNetStreamSocket(Context.Stream) == NULL,
		"TCP stream socket escaped its worker");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE,
		"TCP stream socket thread error mismatch");
	xrtClearError();

	pPayload = (char*)xrtMalloc(TEST_TCP_SLOW_PAYLOAD);
	testRequire(pPayload != NULL, "TCP slow payload allocation failed");
	memset(pPayload, 'S', TEST_TCP_SLOW_PAYLOAD);
	testRequire(xrtNetStreamSend(
		Context.Stream,
		pPayload,
		TEST_TCP_SLOW_PAYLOAD
	) == XNET_RESULT_OK, "TCP slow large send failed");
	xrtFree(pPayload);
	testTcpSlowWait(&Context.HighWater, 1,
		"TCP slow high-water callback missing");

	testRequire(xrtNetSocketSet(Peer, XNET_OPTION_NONBLOCK, 1),
		"TCP slow peer nonblocking setup failed");
	iDeadline = xrtDeadlineAfter(30000000u);
	while ( iReceived < TEST_TCP_SLOW_PAYLOAD ) {
		size_t iRead = 0;
		xnetresult Result = xrtNetSocketRecv(
			Peer,
			Buffer,
			sizeof(Buffer),
			&iRead
		);

		if ( Result == XNET_RESULT_AGAIN ) {
			testRequire(!xrtDeadlineExpired(iDeadline),
				"TCP slow peer receive timed out");
			xrtSleep(1);
			continue;
		}
		testRequire((Result == XNET_RESULT_OK) && (iRead != 0),
			"TCP slow peer receive failed");
		for ( size_t i = 0; i < iRead; i++ ) {
			testRequire(Buffer[i] == 'S',
				"TCP slow payload was corrupted");
		}
		iReceived += iRead;
	}

	testTcpSlowWait(&Context.LowWater, 1,
		"TCP slow low-water callback missing");
	testTcpSlowWait(&Context.Drain, 1,
		"TCP slow drain callback missing");
	testRequire((xrtNetStreamPending(Context.Stream) == 0) &&
		 xrtNetStreamStats(Context.Stream, &Stats) &&
		 (Stats.SentBytes == TEST_TCP_SLOW_PAYLOAD) &&
		 (Stats.PeakQueuedBytes == TEST_TCP_SLOW_PAYLOAD) &&
		 (Stats.QueuedBytes == 0),
		"TCP slow stream statistics mismatch");
	testRequire(xrtAtomic32Load(
		&Context.ListenerError,
		XMEMORY_ACQUIRE
	) == 0, "TCP slow listener reported an error");

	testRequire(xrtNetStreamClose(Context.Stream),
		"TCP slow stream close failed");
	testTcpSlowWait(&Context.Close, 1,
		"TCP slow stream close callback missing");
	testRequire(xrtNetSocketClose(Peer),
		"TCP slow peer close failed");
	testRequire(xrtNetListenerClose(pListener),
		"TCP slow listener close failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtSleep(1);
	}
	xrtNetStreamDestroy(Context.Stream);
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP slow engine destroy failed");
	printf("[PASS] network TCP %s slow peer\n", TEST_TCP_BACKEND_NAME);
	return 0;
}
