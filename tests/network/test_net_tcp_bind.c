#include "../test.h"



/* 等待 Listener 完成全部在途 Accept 的取消。 */
static void testTcpBindWaitClosed(xnetlistener* pListener)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"TCP listener close timed out");
		xrtSleep(1);
	}
}



/* 默认监听策略必须拒绝第二个活动绑定，避免地址被意外抢占。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetlistenconfig FirstConfig;
	xnetlistenconfig SecondConfig;
	xnetengine* pEngine;
	xnetlistener* pFirst;
	xnetlistener* pSecond;
	xnetaddr Address;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(pEngine != NULL, "TCP bind engine creation failed");
	testRequire(xrtNetEngineStart(pEngine), "TCP bind engine start failed");

	xrtNetListenConfigInit(&FirstConfig);
	testRequire(xrtNetAddrLoopback(
		&FirstConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TCP first bind address failed");
	pFirst = xrtNetListen(pEngine, &FirstConfig, NULL, NULL, NULL);
	testRequire(pFirst != NULL, "TCP first listener creation failed");
	testRequire(xrtNetListenerLocal(pFirst, &Address),
		"TCP first listener local address failed");

	xrtNetListenConfigInit(&SecondConfig);
	SecondConfig.Address = Address;
	pSecond = xrtNetListen(pEngine, &SecondConfig, NULL, NULL, NULL);
	testRequire(pSecond == NULL,
		"TCP default listener allowed a duplicate active bind");
	testRequire((xrtGetError() != NULL) &&
		 (xrtErrorCode(xrtGetError()) == XNET_ERROR_SOCKET_BIND),
		"TCP duplicate bind error mismatch");
	xrtClearError();

	testRequire(xrtNetListenerClose(pFirst),
		"TCP first listener close failed");
	testTcpBindWaitClosed(pFirst);
	xrtNetListenerDestroy(pFirst);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP bind engine destroy failed");
	printf("[PASS] network TCP exclusive active bind\n");
	return 0;
}
