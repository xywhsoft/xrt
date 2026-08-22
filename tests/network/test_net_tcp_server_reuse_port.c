#include "../test.h"



/* 等待复用端口启动失败回滚或正常关闭释放全部 Listener。 */
static void testTcpServerReusePortWait(
	xnetengine* pEngine,
	xnetserver* pServer
)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);
	xnetenginestats Stats;

	for ( ;; ) {
		bool bServerDone = (pServer == NULL) ||
			(xrtNetServerState(pServer) == XNET_SERVER_CLOSED);

		testRequire(xrtNetEngineStats(pEngine, &Stats),
			"TCP server reuse-port stats failed");
		if ( bServerDone && (Stats.LiveObjects == 0) ) {
			return;
		}
		testRequire(!xrtDeadlineExpired(iDeadline),
			"TCP server reuse-port cleanup timed out");
		xrtThreadYield();
	}
}



/* 固定复用端口的平台能力与每 Worker Listener 所有权契约。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetserverconfig ServerConfig;
	xnetengine* pEngine;
	xnetserver* pServer;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 3;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP server reuse-port engine start failed");
	xrtNetServerConfigInit(&ServerConfig);
	testRequire(xrtNetAddrLoopback(
		&ServerConfig.Listen.Address,
		XNET_FAMILY_IPV4,
		0
	), "TCP server reuse-port loopback address failed");
	ServerConfig.Mode = XNET_SERVER_REUSE_PORT;
	pServer = xrtNetServerStart(
		pEngine,
		&ServerConfig,
		NULL,
		NULL,
		NULL
	);

	#if defined(_WIN32) || defined(_WIN64)
		testRequire((pServer == NULL) &&
			 (xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED) &&
			 (xrtErrorCode(xrtGetError()) == XNET_ERROR_SERVER_START) &&
			 (xrtErrorCause(xrtGetError()) != NULL),
			"Windows TCP server reuse-port capability mismatch");
		xrtClearError();
		testTcpServerReusePortWait(pEngine, NULL);
	#else
		testRequire((pServer != NULL) &&
			 (xrtNetServerEndpointCount(pServer) == 1u) &&
			 (xrtNetServerListenerCount(pServer) == 3u),
			"POSIX TCP server reuse-port start failed");
		for ( uint32 i = 0; i < 3u; i++ ) {
			xnetlistener* pListener = xrtNetServerListener(pServer, i);

			testRequire((pListener != NULL) &&
				 (xrtNetWorkerIndex(
					xrtNetListenerWorker(pListener)
				  ) == i),
				"TCP server reuse-port Listener affinity mismatch");
			xrtNetListenerDestroy(pListener);
		}
		testRequire(xrtNetServerClose(pServer),
			"TCP server reuse-port close failed");
		testTcpServerReusePortWait(pEngine, pServer);
		xrtNetServerDestroy(pServer);
	#endif

	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP server reuse-port engine destroy failed");
	printf("[PASS] network TCP server reuse-port\n");
	return 0;
}
