#include "../test.h"



/* 等待失败回滚释放全部 Listener Engine 占用。 */
static void testTcpServerRollback(xnetengine* pEngine)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);
	xnetenginestats Stats;

	for ( ;; ) {
		testRequire(xrtNetEngineStats(pEngine, &Stats),
			"TCP server rollback stats failed");
		if ( Stats.LiveObjects == 0 ) {
			return;
		}
		testRequire(!xrtDeadlineExpired(iDeadline),
			"TCP server rollback leaked a Listener");
		xrtThreadYield();
	}
}



/* 验证 Server 参数、错误域和多端点失败回滚。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetserverconfig Config;
	xnetlistenconfig Additional;
	xnetserverstats Stats;
	xnetengine* pEngine;

	xrtNetServerConfigInit(&Config);
	testRequire((Config.Listen.Address.Family == XNET_FAMILY_IPV4) &&
		 (Config.AcceptQueueLimit != 0) &&
		 (Config.Mode == XNET_SERVER_SHARED) &&
		 (Config.Additional == NULL) &&
		 (Config.AdditionalCount == 0),
		"TCP server defaults are invalid");
	testRequire((xrtNetServerRef(NULL) == NULL) &&
		 (xrtNetServerAccept(NULL) == NULL) &&
		 !xrtNetServerClose(NULL) &&
		 (xrtNetServerState(NULL) == XNET_SERVER_CLOSED) &&
		 (xrtNetServerEndpointCount(NULL) == 0) &&
		 (xrtNetServerListenerCount(NULL) == 0) &&
		 (xrtNetServerData(NULL) == NULL) &&
		 !xrtNetServerStats(NULL, &Stats),
		"TCP server NULL contract mismatch");
	xrtClearError();

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(pEngine != NULL,
		"TCP server invalid engine create failed");
	testRequire(xrtNetServerStart(
		pEngine,
		&Config,
		NULL,
		NULL,
		NULL
	) == NULL, "stopped Engine accepted a TCP server");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_CLOSED) &&
		 (xrtErrorCode(xrtGetError()) == XNET_ERROR_SERVER_START),
		"stopped TCP server error mismatch");
	xrtClearError();
	testRequire(xrtNetEngineStart(pEngine),
		"TCP server invalid engine start failed");

	xrtNetServerConfigInit(&Config);
	Config.AdditionalCount = 1;
	testRequire(xrtNetServerStart(
		pEngine,
		&Config,
		NULL,
		NULL,
		NULL
	) == NULL, "TCP server accepted a NULL endpoint range");
	testRequire(xrtErrorCode(xrtGetError()) == XNET_ERROR_SERVER_CONFIG,
		"TCP server NULL range error mismatch");
	xrtClearError();

	xrtNetServerConfigInit(&Config);
	Config.Mode = (xnetservermode)99;
	testRequire(xrtNetServerStart(
		pEngine,
		&Config,
		NULL,
		NULL,
		NULL
	) == NULL, "TCP server accepted an unknown mode");
	testRequire(xrtErrorCode(xrtGetError()) == XNET_ERROR_SERVER_CONFIG,
		"TCP server mode error mismatch");
	xrtClearError();

	xrtNetServerConfigInit(&Config);
	Config.AcceptQueueLimit = 0;
	testRequire(xrtNetServerStart(
		pEngine,
		&Config,
		NULL,
		NULL,
		NULL
	) == NULL, "TCP server accepted a zero queue limit");
	xrtClearError();

	xrtNetServerConfigInit(&Config);
	xrtNetListenConfigInit(&Additional);
	Config.Listen.Address.Port = 10001;
	Additional.Address.Port = 10002;
	Config.Additional = &Additional;
	Config.AdditionalCount = 1;
	Config.SharedPort = true;
	testRequire(xrtNetServerStart(
		pEngine,
		&Config,
		NULL,
		NULL,
		NULL
	) == NULL, "TCP server accepted conflicting shared ports");
	testRequire(xrtErrorCode(xrtGetError()) == XNET_ERROR_SERVER_CONFIG,
		"TCP server shared port error mismatch");
	xrtClearError();

	/* 两个相同 IPv4 端点共享动态端口，第二次绑定必须触发整组回滚。 */
	xrtNetServerConfigInit(&Config);
	Additional = Config.Listen;
	Config.Additional = &Additional;
	Config.AdditionalCount = 1;
	Config.SharedPort = true;
	testRequire(xrtNetServerStart(
		pEngine,
		&Config,
		NULL,
		NULL,
		NULL
	) == NULL, "TCP server duplicate endpoint unexpectedly started");
	testRequire((xrtErrorCode(xrtGetError()) == XNET_ERROR_SERVER_START) &&
		 (xrtErrorCause(xrtGetError()) != NULL),
		"TCP server rollback error lost its Listener cause");
	xrtClearError();
	testTcpServerRollback(pEngine);

	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP server invalid engine destroy failed");
	printf("[PASS] network TCP server invalid inputs\n");
	return 0;
}
