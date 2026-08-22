#include "../test.h"



/* 等待关闭回调完成 Server 的运行时引用释放。 */
static void testTcpServerFutureOomClose(xnetserver* pServer)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	while ( xrtNetServerState(pServer) != XNET_SERVER_CLOSED ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"TCP server Future OOM close timed out");
		xrtThreadYield();
	}
}



/* 逐个验证等待节点、Promise、取消令牌和监听分配失败都完整回滚。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetserverconfig ServerConfig;
	xnetserverstats Stats;
	xnetengine* pEngine;
	xnetserver* pServer;
	uint32 iFailures = 0;
	bool bSucceeded = false;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP server Future OOM engine start failed");
	xrtNetServerConfigInit(&ServerConfig);
	testRequire(xrtNetAddrLoopback(
		&ServerConfig.Listen.Address,
		XNET_FAMILY_IPV4,
		0
	), "TCP server Future OOM loopback address failed");
	pServer = xrtNetServerStart(
		pEngine,
		&ServerConfig,
		NULL,
		NULL,
		NULL
	);
	testRequire(pServer != NULL,
		"TCP server Future OOM start failed");

	for ( uint64 iOffset = 0; iOffset < 16u; iOffset++ ) {
		xfuture* pFuture;
		bool bTriggered;

		testRequire(xrtMemDebugFailAfter(iOffset),
			"TCP server Future OOM fault setup failed");
		pFuture = xrtNetServerAcceptAsync(pServer);
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( pFuture != NULL ) {
			testRequire(!bTriggered && xrtFutureCancel(pFuture),
				"TCP server Future OOM success cleanup failed");
			xrtFutureDestroy(pFuture);
			bSucceeded = true;
			break;
		}
		iFailures++;
		testRequire(bTriggered &&
			 (xrtErrorKind(xrtGetError()) == XERR_MEMORY),
			"TCP server Future OOM error mismatch");
		xrtClearError();
		testRequire(xrtNetServerStats(pServer, &Stats) &&
			 (Stats.AcceptWaiters == 0),
			"TCP server Future OOM leaked a waiter");
	}
	testRequire(bSucceeded && (iFailures >= 4u),
		"TCP server Future OOM sweep missed an allocation boundary");
	testRequire(xrtNetServerClose(pServer),
		"TCP server Future OOM close failed");
	testTcpServerFutureOomClose(pServer);
	xrtNetServerDestroy(pServer);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP server Future OOM engine destroy failed");
	printf("[PASS] network TCP server Future OOM\n");
	return 0;
}
