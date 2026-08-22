#include "../test.h"



/* 等待 Stream 与 Server 完成异步终止。 */
static void testTcpServerFutureClose(
	xnetserver* pServer,
	xnetstream* pClient,
	xnetstream* pAccepted
)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	while ( (xrtNetServerState(pServer) != XNET_SERVER_CLOSED) ||
		 (xrtNetStreamState(pClient) != XNET_STREAM_CLOSED) ||
		 (xrtNetStreamState(pAccepted) != XNET_STREAM_CLOSED) ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"TCP server Future objects did not close");
		xrtThreadYield();
	}
}



/* 验证聚合 Accept Future 的成功、取消、关闭和消费互斥。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetserverconfig ServerConfig;
	xnetserverstats Stats;
	xnetengine* pEngine;
	xnetserver* pServer;
	xnetstream* pClient;
	xnetstream* pAccepted;
	xfuture* pAccept;
	xfuture* pCancel;
	xfuture* pClose;
	xfutureresult Result;
	xnetaddr Local;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP server Future engine start failed");
	xrtNetServerConfigInit(&ServerConfig);
	testRequire(xrtNetAddrLoopback(
		&ServerConfig.Listen.Address,
		XNET_FAMILY_IPV4,
		0
	), "TCP server Future loopback address failed");
	pServer = xrtNetServerStart(
		pEngine,
		&ServerConfig,
		NULL,
		NULL,
		NULL
	);
	testRequire((pServer != NULL) &&
		 xrtNetServerLocal(pServer, 0, &Local),
		"TCP server Future start failed");

	pAccept = xrtNetServerAcceptAsync(pServer);
	testRequire(pAccept != NULL,
		"TCP server Accept Future create failed");
	testRequire((xrtNetServerAccept(pServer) == NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_STATE),
		"TCP server mixed pull consumers were accepted");
	xrtClearError();
	pClient = xrtNetStreamConnect(
		pEngine,
		&Local,
		1,
		NULL,
		NULL,
		NULL
	);
	testRequire(pClient != NULL,
		"TCP server Future client connect failed");
	testRequire((xrtFutureWaitFor(pAccept, 5000000u) == XWAIT_OK) &&
		 xrtFutureResult(pAccept, &Result) &&
		 (Result.State == XFUTURE_RESOLVED) &&
		 (Result.Value != NULL),
		"TCP server Accept Future did not resolve");
	pAccepted = xrtNetStreamRef((xnetstream*)Result.Value);
	testRequire(pAccepted != NULL,
		"TCP server Accept Future Stream ref failed");
	xrtFutureDestroy(pAccept);

	pCancel = xrtNetServerAcceptAsync(pServer);
	testRequire((pCancel != NULL) && xrtFutureCancel(pCancel) &&
		 (xrtFutureWaitFor(pCancel, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pCancel) == XFUTURE_CANCELLED),
		"TCP server Accept Future cancellation failed");
	xrtFutureDestroy(pCancel);
	testRequire(xrtNetServerStats(pServer, &Stats) &&
		 (Stats.AcceptWaiters == 0),
		"TCP server cancelled waiter leaked");

	pClose = xrtNetServerAcceptAsync(pServer);
	testRequire((pClose != NULL) && xrtNetServerClose(pServer) &&
		 (xrtFutureWaitFor(pClose, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pClose) == XFUTURE_CLOSED),
		"TCP server close did not finish its Accept Future");
	xrtFutureDestroy(pClose);
	(void)xrtNetStreamAbort(pAccepted);
	(void)xrtNetStreamAbort(pClient);
	testTcpServerFutureClose(pServer, pClient, pAccepted);

	xrtNetStreamDestroy(pAccepted);
	xrtNetStreamDestroy(pClient);
	xrtNetServerDestroy(pServer);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP server Future engine destroy failed");
	printf("[PASS] network TCP server Future\n");
	return 0;
}
