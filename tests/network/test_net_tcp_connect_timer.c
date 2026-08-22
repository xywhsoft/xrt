#include "../test.h"



#if !defined(TEST_TCP_BACKEND)
	#define TEST_TCP_BACKEND XNET_PORT_SELECT
	#define TEST_TCP_BACKEND_NAME "select"
#endif



typedef struct testtcpconnecttimer {
	xatomic32 Closed;
	xatomic32 Failure;
} testtcpconnecttimer;



/* 取得一个已释放的本地端口，用于稳定触发连接拒绝。 */
static void testTcpConnectTimerUnusedAddress(xnetaddr* pAddress)
{
	xnetsocket Socket = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		XNET_SOCKET_NONBLOCK
	);

	testRequire(Socket != NULL,
		"TCP connect timer socket open failed");
	testRequire(xrtNetAddrLoopback(
		pAddress,
		XNET_FAMILY_IPV4,
		0
	) && xrtNetSocketBind(Socket, pAddress) &&
		xrtNetSocketLocal(Socket, pAddress),
		"TCP connect timer port allocation failed");
	testRequire(xrtNetSocketClose(Socket),
		"TCP connect timer socket close failed");
}



/* 记录预期的连接失败终态。 */
static void testTcpConnectTimerClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testtcpconnecttimer* pContext = (testtcpconnecttimer*)pData;

	if ( (xrtNetStreamState(pStream) != XNET_STREAM_CLOSED) ||
		 (Result != XNET_RESULT_ERROR) || (pError == NULL) ) {
		xrtAtomic32Store(&pContext->Failure, 1, XMEMORY_RELEASE);
	}
	xrtAtomic32Store(&pContext->Closed, 1, XMEMORY_RELEASE);
}



/* 验证连接提前失败后不保留长超时 Timer 和 Stream 引用。 */
int main(void)
{
	testtcpconnecttimer Context;
	xnetengineconfig EngineConfig;
	xnetstreamconfig StreamConfig;
	xnetstreamevents Events;
	xnetenginestats Stats;
	xnetengine* pEngine;
	xnetstream* pStream;
	xnetaddr Address;
	xdeadline iDeadline;

	memset(&Context, 0, sizeof(Context));
	memset(&Events, 0, sizeof(Events));
	xrtAtomic32Init(&Context.Closed, 0);
	xrtAtomic32Init(&Context.Failure, 0);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TCP_BACKEND;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(pEngine != NULL,
		"TCP connect timer engine create failed");
	testRequire(xrtNetEngineStart(pEngine),
		"TCP connect timer engine start failed");
	testTcpConnectTimerUnusedAddress(&Address);
	xrtNetStreamConfigInit(&StreamConfig);
	StreamConfig.ConnectTimeout = 30000000u;
	Events.Close = testTcpConnectTimerClose;
	pStream = xrtNetStreamConnect(
		pEngine,
		&Address,
		0,
		&StreamConfig,
		&Events,
		&Context
	);
	testRequire(pStream != NULL,
		"TCP connect timer stream create failed");
	iDeadline = xrtDeadlineAfter(5000000u);
	while ( xrtAtomic32Load(&Context.Closed, XMEMORY_ACQUIRE) == 0 ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"TCP connect timer close callback timed out");
		xrtThreadYield();
	}
	testRequire(xrtAtomic32Load(&Context.Failure, XMEMORY_ACQUIRE) == 0,
		"TCP connect timer close result mismatch");
	testRequire(xrtNetEngineStats(pEngine, &Stats),
		"TCP connect timer engine stats failed");
	testRequire(Stats.ActiveTimers == 0,
		"TCP connect failure retained its timeout timer");
	xrtNetStreamDestroy(pStream);
	testRequire(xrtNetEngineStop(pEngine),
		"TCP connect timer engine stop failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP connect timer engine destroy failed");
	printf("[PASS] network TCP %s connect timer cleanup\n",
		TEST_TCP_BACKEND_NAME);
	return 0;
}
