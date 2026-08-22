#include "../test.h"



/* 等待启动回滚释放全部 Engine 对象。 */
static void testTcpServerOomRollback(xnetengine* pEngine)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);
	xnetenginestats Stats;

	for ( ;; ) {
		testRequire(xrtNetEngineStats(pEngine, &Stats),
			"TCP server OOM stats failed");
		if ( Stats.LiveObjects == 0 ) {
			return;
		}
		testRequire(!xrtDeadlineExpired(iDeadline),
			"TCP server OOM rollback leaked an Engine object");
		xrtThreadYield();
	}
}



/* 验证第二个 Listener 分配失败时，已建端点被完整回滚。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetserverconfig ServerConfig;
	xnetlistenconfig Additional;
	xnetengine* pEngine;
	xnetserver* pServer;
	bool bTriggered;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP server OOM engine start failed");
	xrtNetServerConfigInit(&ServerConfig);
	ServerConfig.Listen.AcceptConcurrency = 1;
	Additional = ServerConfig.Listen;
	Additional.Affinity = 1;
	ServerConfig.Additional = &Additional;
	ServerConfig.AdditionalCount = 1;

	/* Server、首个 Listener 与 AcceptSlots 成功后拒绝下一次逻辑分配。 */
	testRequire(
		xrtMemDebugFailAfter(3u),
		"TCP server OOM injection setup failed"
	);
	pServer = xrtNetServerStart(
		pEngine,
		&ServerConfig,
		NULL,
		NULL,
		NULL
	);
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	testRequire(pServer == NULL,
		"TCP server survived its Listener OOM point");
	testRequire(bTriggered,
		"TCP server did not reach its Listener OOM point");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"TCP server OOM error kind mismatch");
	xrtClearError();
	testTcpServerOomRollback(pEngine);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP server OOM engine destroy failed");
	printf("[PASS] network TCP server OOM rollback\n");
	return 0;
}
