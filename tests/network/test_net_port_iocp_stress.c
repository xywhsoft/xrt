#include "../test.h"



#if defined(_WIN32) || defined(_WIN64)

#define TEST_IOCP_STRESS_COUNT 512u
#define TEST_IOCP_STRESS_MAGIC 0x58494F43u



/* 每个数据报携带可乱序验证的稳定序号。 */
typedef struct testiocppacket {
	uint32 Magic;
	uint32 Index;
} testiocppacket;



/* 预投递大量收发并验证每个操作只产生一个完整终态。 */
int main(void)
{
	xnetportconfig Config;
	xnetportevent Events[64];
	testiocppacket Sent[TEST_IOCP_STRESS_COUNT];
	testiocppacket Received[TEST_IOCP_STRESS_COUNT];
	bool Seen[TEST_IOCP_STRESS_COUNT];
	xnetport* pPort;
	xnetsocket Server;
	xnetsocket Client;
	xnetaddr Address;
	size_t iReceiveCount = 0;
	size_t iSendCount = 0;
	xdeadline Deadline = xrtDeadlineAfter(10000000);

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_IOCP;
	Config.OperationLimit = TEST_IOCP_STRESS_COUNT * 2u;
	pPort = xrtNetPortCreate(&Config);
	Server = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	Client = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	testRequire((pPort != NULL) && (Server != NULL) && (Client != NULL),
		"IOCP stress setup failed");
	testRequire(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) &&
		xrtNetSocketBind(Server, &Address) &&
		xrtNetSocketLocal(Server, &Address),
		"IOCP stress bind failed");
	memset(Received, 0, sizeof(Received));
	memset(Seen, 0, sizeof(Seen));

	for ( size_t i = 0; i < TEST_IOCP_STRESS_COUNT; i++ ) {
		Sent[i].Magic = TEST_IOCP_STRESS_MAGIC;
		Sent[i].Index = (uint32)i;
		testRequire(xrtNetPortRecvFrom(pPort, Server,
			&Received[i], sizeof(Received[i]),
			(uint64)i + 1u, &Received[i]),
			"IOCP stress receive submit failed");
	}
	for ( size_t i = 0; i < TEST_IOCP_STRESS_COUNT; i++ ) {
		testRequire(xrtNetPortSendTo(pPort, Client,
			&Sent[i], sizeof(Sent[i]), &Address,
			(uint64)TEST_IOCP_STRESS_COUNT + (uint64)i + 1u,
			&Sent[i]), "IOCP stress send submit failed");
	}

	while ( (iReceiveCount < TEST_IOCP_STRESS_COUNT) ||
		(iSendCount < TEST_IOCP_STRESS_COUNT) ) {
		size_t iCount = 0;

		testRequire(xrtNetPortWait(pPort, Events, 64,
			Deadline, &iCount) == XNET_RESULT_OK,
			"IOCP stress wait failed");
		for ( size_t i = 0; i < iCount; i++ ) {
			xnetportevent* pEvent = &Events[i];

			testRequire((pEvent->Result == XNET_RESULT_OK) &&
				(pEvent->Bytes == sizeof(testiocppacket)),
				"IOCP stress terminal result mismatch");
			if ( pEvent->Type == XNET_PORT_EVENT_RECV_FROM ) {
				testiocppacket* pPacket =
					(testiocppacket*)pEvent->User;

				testRequire((pPacket->Magic == TEST_IOCP_STRESS_MAGIC) &&
					(pPacket->Index < TEST_IOCP_STRESS_COUNT) &&
					!Seen[pPacket->Index],
					"IOCP stress receive data duplicated or corrupted");
				Seen[pPacket->Index] = true;
				iReceiveCount++;
			} else {
				testRequire(pEvent->Type == XNET_PORT_EVENT_SEND_TO,
					"IOCP stress returned an unexpected event");
				iSendCount++;
			}
		}
	}
	for ( size_t i = 0; i < TEST_IOCP_STRESS_COUNT; i++ ) {
		testRequire(Seen[i], "IOCP stress lost a datagram");
	}

	testRequire(xrtNetSocketClose(Client) &&
		xrtNetSocketClose(Server) &&
		xrtNetPortDestroy(pPort), "IOCP stress cleanup failed");
	return 0;
}

#else

/* 非 Windows 构建由基础 IOCP 用例验证 unavailable 契约。 */
int main(void)
{
	return 0;
}

#endif
