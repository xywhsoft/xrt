#include "../test.h"



#if !defined(TEST_UDP_BACKEND)
	#define TEST_UDP_BACKEND XNET_PORT_SELECT
	#define TEST_UDP_BACKEND_NAME "select"
#endif



typedef struct testudpmulticast {
	xatomic32 Done;
	xatomic32 Success;
} testudpmulticast;



/* 在 UDP 所属 Worker 中验证全部高层多播包装。 */
static void testUdpMulticastOpen(xnetudp* pUdp, ptr pData)
{
	testudpmulticast* pTest = (testudpmulticast*)pData;
	xnetaddr Group;
	xnetaddr Interface;
	bool bSuccess;

	bSuccess = (xrtNetUdpSocket(pUdp) != NULL) &&
		xrtNetAddrParse(&Group, "239.255.42.100", 0) &&
		xrtNetAddrLoopback(&Interface, XNET_FAMILY_IPV4, 0) &&
		xrtNetUdpJoin(pUdp, &Group, &Interface) &&
		xrtNetUdpMulticastLoop(pUdp, false) &&
		xrtNetUdpMulticastLoop(pUdp, true) &&
		xrtNetUdpMulticastHopLimit(pUdp, 0) &&
		xrtNetUdpMulticastHopLimit(pUdp, 255) &&
		xrtNetUdpMulticastInterface(pUdp, &Interface) &&
		xrtNetUdpMulticastInterface(pUdp, NULL) &&
		xrtNetUdpLeave(pUdp, &Group, &Interface);
	xrtAtomic32Store(&pTest->Success,
		bSuccess ? 1u : 0u, XMEMORY_RELEASE);
	xrtAtomic32Store(&pTest->Done, 1u, XMEMORY_RELEASE);
}



/* 等待 Worker 完成多播配置。 */
static void testUdpMulticastWait(testudpmulticast* pTest)
{
	xdeadline iDeadline = xrtDeadlineAfter(3000000u);

	while ( xrtAtomic32Load(&pTest->Done, XMEMORY_ACQUIRE) == 0 ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"UDP multicast worker test timed out");
		xrtThreadYield();
	}
}



/* 等待 UDP 正常关闭。 */
static void testUdpMulticastWaitClosed(xnetudp* pUdp)
{
	xdeadline iDeadline = xrtDeadlineAfter(3000000u);

	while ( xrtNetUdpState(pUdp) != XNET_UDP_CLOSED ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"UDP multicast close timed out");
		xrtThreadYield();
	}
}



/* 验证高层 UDP 多播包装的 Worker 契约。 */
int main(void)
{
	testudpmulticast Test;
	xnetengineconfig EngineConfig;
	xnetudpevents Events;
	xnetengine* pEngine;
	xnetudp* pUdp;
	xnetaddr Address;

	memset(&Test, 0, sizeof(Test));
	memset(&Events, 0, sizeof(Events));
	Events.Open = testUdpMulticastOpen;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_UDP_BACKEND;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"UDP multicast engine start failed");
	testRequire(xrtNetAddrAny(&Address, XNET_FAMILY_IPV4, 0),
		"UDP multicast bind address failed");
	pUdp = xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		NULL,
		&Events,
		&Test
	);
	testRequire(pUdp != NULL, "UDP multicast bind failed");
	testUdpMulticastWait(&Test);
	testRequire(xrtAtomic32Load(
		&Test.Success,
		XMEMORY_ACQUIRE
	) != 0, "UDP multicast Worker API failed");
	testRequire(xrtNetUdpClose(pUdp),
		"UDP multicast close failed");
	testUdpMulticastWaitClosed(pUdp);
	xrtNetUdpDestroy(pUdp);
	testRequire(xrtNetEngineDestroy(pEngine),
		"UDP multicast engine destroy failed");
	printf("[PASS] network UDP multicast %s\n",
		TEST_UDP_BACKEND_NAME);
	return 0;
}
