#include "../test.h"



#if !defined(TEST_UDP_BACKEND)
	#define TEST_UDP_BACKEND XNET_PORT_SELECT
	#define TEST_UDP_BACKEND_NAME "select"
#endif

#if !defined(TEST_UDP_COMPLETION)
	#define TEST_UDP_COMPLETION 0
#endif

#define TEST_UDP_SEND_COUNT 8u
#define TEST_UDP_SEND_CONCURRENCY 4u



/* 在截止时间内等待 UDP 进入指定状态。 */
static void testUdpConcurrencyWaitState(
	xnetudp* pUdp,
	xnetudpstate State
)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	while ( xrtNetUdpState(pUdp) != State ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"UDP send concurrency state timed out");
		xrtThreadYield();
	}
}



/* 在截止时间内拉取一个数据报。 */
static xnetudppacket* testUdpConcurrencyReceive(xnetudp* pUdp)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);
	xnetudppacket* pPacket;

	for ( ;; ) {
		pPacket = xrtNetUdpReceive(pUdp);
		if ( pPacket != NULL ) {
			return pPacket;
		}
		testRequire(!xrtDeadlineExpired(iDeadline),
			"UDP concurrent receive timed out");
		xrtThreadYield();
	}
}



/* 验证 completion 发送槽并发、数据报完整性和关闭收敛。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetudpconfig ServerConfig;
	xnetudpconfig ClientConfig;
	xnetengine* pEngine;
	xnetudp* pServer;
	xnetudp* pClient;
	xnetdgramsend Sends[TEST_UDP_SEND_COUNT];
	xnetudpstats Stats;
	xnetaddr Address;
	uint8 Payloads[TEST_UDP_SEND_COUNT];
	uint32 iSeen = 0;
	size_t iAccepted = 0;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_UDP_BACKEND;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"UDP send concurrency engine start failed");

	xrtNetUdpConfigInit(&ServerConfig);
	xrtNetUdpConfigInit(&ClientConfig);
	ClientConfig.SendConcurrency = TEST_UDP_SEND_CONCURRENCY;
	testRequire(xrtNetAddrLoopback(
		&Address,
		XNET_FAMILY_IPV4,
		0
	), "UDP send concurrency loopback failed");
	pServer = xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&ServerConfig,
		NULL,
		NULL
	);
	testRequire((pServer != NULL) && xrtNetUdpLocal(pServer, &Address),
		"UDP send concurrency bind failed");
	pClient = xrtNetUdpConnect(
		pEngine,
		&Address,
		0,
		&ClientConfig,
		NULL,
		NULL
	);
	testRequire(pClient != NULL, "UDP send concurrency connect failed");
	testUdpConcurrencyWaitState(pServer, XNET_UDP_OPEN);
	testUdpConcurrencyWaitState(pClient, XNET_UDP_OPEN);

	for ( size_t i = 0; i < TEST_UDP_SEND_COUNT; i++ ) {
		Payloads[i] = (uint8)i;
		Sends[i].Remote = NULL;
		Sends[i].Data = &Payloads[i];
		Sends[i].Size = 1;
	}
	testRequire((xrtNetUdpSendBatch(
		pClient,
		Sends,
		TEST_UDP_SEND_COUNT,
		&iAccepted
	) == XNET_RESULT_OK) && (iAccepted == TEST_UDP_SEND_COUNT),
		"UDP concurrent batch submit failed");

	for ( size_t i = 0; i < TEST_UDP_SEND_COUNT; i++ ) {
		xnetudppacket* pPacket = testUdpConcurrencyReceive(pServer);
		uint8 iValue;

		testRequire(xrtNetUdpPacketSize(pPacket) == 1,
			"UDP concurrent datagram size mismatch");
		iValue = *xrtNetUdpPacketData(pPacket);
		testRequire((iValue < TEST_UDP_SEND_COUNT) &&
			 ((iSeen & (1u << iValue)) == 0),
			"UDP concurrent datagram identity mismatch");
		iSeen |= 1u << iValue;
		xrtNetUdpPacketDestroy(pPacket);
	}
	testRequire(iSeen == ((1u << TEST_UDP_SEND_COUNT) - 1u),
		"UDP concurrent datagram set mismatch");

	{
		xdeadline iDeadline = xrtDeadlineAfter(5000000u);

		for ( ;; ) {
			testRequire(xrtNetUdpStats(pClient, &Stats),
				"UDP concurrent stats failed");
			if ( (Stats.SentPackets == TEST_UDP_SEND_COUNT) &&
				 (Stats.QueuedPackets == 0) &&
				 (Stats.ActiveSends == 0) ) {
				break;
			}
			testRequire(!xrtDeadlineExpired(iDeadline),
				"UDP concurrent sends did not drain");
			xrtThreadYield();
		}
	}
	#if TEST_UDP_COMPLETION
		testRequire(Stats.PeakActiveSends >= TEST_UDP_SEND_CONCURRENCY,
			"UDP completion send concurrency was not reached");
	#else
		testRequire(Stats.PeakActiveSends == 0,
			"UDP readiness backend allocated completion sends");
	#endif

	testRequire(xrtNetUdpClose(pClient) && xrtNetUdpClose(pServer),
		"UDP send concurrency close failed");
	testUdpConcurrencyWaitState(pClient, XNET_UDP_CLOSED);
	testUdpConcurrencyWaitState(pServer, XNET_UDP_CLOSED);
	testRequire(xrtNetUdpStats(pClient, &Stats) &&
		 (Stats.ActiveSends == 0) && (Stats.QueuedPackets == 0),
		"UDP concurrent final stats mismatch");
	xrtNetUdpDestroy(pClient);
	xrtNetUdpDestroy(pServer);
	testRequire(xrtNetEngineDestroy(pEngine),
		"UDP send concurrency engine destroy failed");
	printf("[PASS] network UDP send concurrency %s\n",
		TEST_UDP_BACKEND_NAME);
	return 0;
}
