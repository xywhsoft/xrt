#include "../test.h"



#if defined(__linux__)

#define TEST_URING_STRESS_COUNT 512u
#define TEST_URING_STRESS_MAGIC UINT32_C(0x58555247)



typedef struct testuringpacket {
	uint32 Magic;
	uint32 Index;
} testuringpacket;



/* 预投递大量 UDP 接收并分批发送，验证批量 CQ、ID 和调用方缓冲直写。 */
static void testUringDatagrams(void)
{
	xnetportconfig Config;
	xnetportevent Events[64];
	testuringpacket Sent[TEST_URING_STRESS_COUNT];
	testuringpacket Received[TEST_URING_STRESS_COUNT];
	bool Seen[TEST_URING_STRESS_COUNT];
	xnetport* pPort;
	xnetsocket Server;
	xnetsocket Client;
	xnetaddr Address;
	size_t iReceiveCount = 0;
	size_t iSendCount = 0;
	size_t iSubmitted = 0;
	xdeadline Deadline = xrtDeadlineAfter(10000000u);

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_URING;
	Config.OperationLimit = TEST_URING_STRESS_COUNT * 2u;
	pPort = xrtNetPortCreate(&Config);
	Server = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM,
		XNET_SOCKET_NONBLOCK
	);
	Client = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM,
		XNET_SOCKET_NONBLOCK
	);
	testRequire(
		(pPort != NULL) && (Server != NULL) && (Client != NULL),
		"io_uring stress setup failed"
	);
	testRequire(
		xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) &&
		xrtNetSocketBind(Server, &Address) &&
		xrtNetSocketLocal(Server, &Address),
		"io_uring stress bind failed"
	);
	memset(Received, 0, sizeof(Received));
	memset(Seen, 0, sizeof(Seen));

	for ( size_t i = 0; i < TEST_URING_STRESS_COUNT; i++ ) {
		Sent[i].Magic = TEST_URING_STRESS_MAGIC;
		Sent[i].Index = (uint32)i;
		testRequire(
			xrtNetPortRecvFrom(
				pPort,
				Server,
				&Received[i],
				sizeof(Received[i]),
				(uint64)i + 1u,
				&Received[i]
			),
			"io_uring stress receive submit failed"
		);
	}
	while ( iSubmitted < TEST_URING_STRESS_COUNT ) {
		size_t iBatchEnd = iSubmitted + 64u;

		if ( iBatchEnd > TEST_URING_STRESS_COUNT ) {
			iBatchEnd = TEST_URING_STRESS_COUNT;
		}
		for ( size_t i = iSubmitted; i < iBatchEnd; i++ ) {
			testRequire(
				xrtNetPortSendTo(
					pPort,
					Client,
					&Sent[i],
					sizeof(Sent[i]),
					&Address,
					(uint64)TEST_URING_STRESS_COUNT + (uint64)i + 1u,
					&Sent[i]
				),
				"io_uring stress send submit failed"
			);
		}

		while ( (iReceiveCount < iBatchEnd) ||
			(iSendCount < iBatchEnd) ) {
			size_t iCount = 0;
			xnetresult Result;

			Result = xrtNetPortWait(
				pPort,
				Events,
				64,
				Deadline,
				&iCount
			);
			if ( Result != XNET_RESULT_OK ) {
				const xerror* pError = xrtGetError();

				fprintf(
					stderr,
					"[DIAG] io_uring datagram stress: result=%d recv=%zu "
					"send=%zu submitted=%zu error=%s system=%d\n",
					(int)Result,
					iReceiveCount,
					iSendCount,
					iBatchEnd,
					(pError != NULL) ? xrtErrorMessage(pError) : "none",
					(pError != NULL) ? (int)xrtErrorSystemCode(pError) : 0
				);
			}
			testRequire(Result == XNET_RESULT_OK, "io_uring stress wait failed");
			for ( size_t i = 0; i < iCount; i++ ) {
				xnetportevent* pEvent = &Events[i];

				testRequire(
					(pEvent->Result == XNET_RESULT_OK) &&
					(pEvent->Bytes == sizeof(testuringpacket)),
					"io_uring stress terminal result mismatch"
				);
				if ( pEvent->Type == XNET_PORT_EVENT_RECV_FROM ) {
					testuringpacket* pPacket =
						(testuringpacket*)pEvent->User;

					testRequire(
						(pPacket->Magic == TEST_URING_STRESS_MAGIC) &&
						(pPacket->Index < TEST_URING_STRESS_COUNT) &&
						!Seen[pPacket->Index],
						"io_uring stress receive identity mismatch"
					);
					Seen[pPacket->Index] = true;
					iReceiveCount++;
				} else {
					testRequire(
						pEvent->Type == XNET_PORT_EVENT_SEND_TO,
						"io_uring stress event type mismatch"
					);
					iSendCount++;
				}
			}
		}
		iSubmitted = iBatchEnd;
	}

	testRequire(
		xrtNetSocketClose(Client) &&
		xrtNetSocketClose(Server) &&
		xrtNetPortDestroy(pPort),
		"io_uring datagram stress cleanup failed"
	);
}



/* 大量并发 accept 取消验证控制 CQE 不吞占公开批容量或重复终态。 */
static void testUringCancellations(void)
{
	xnetportconfig Config;
	xnetportevent Events[64];
	bool Seen[TEST_URING_STRESS_COUNT];
	xnetport* pPort;
	xnetsocket Listener;
	xnetaddr Address;
	size_t iCompleted = 0;
	xdeadline Deadline = xrtDeadlineAfter(10000000u);

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_URING;
	Config.OperationLimit = TEST_URING_STRESS_COUNT;
	pPort = xrtNetPortCreate(&Config);
	Listener = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		XNET_SOCKET_NONBLOCK
	);
	testRequire(
		(pPort != NULL) && (Listener != NULL) &&
		xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) &&
		xrtNetSocketBind(Listener, &Address) &&
		xrtNetSocketListen(Listener, 16),
		"io_uring cancellation stress setup failed"
	);
	memset(Seen, 0, sizeof(Seen));

	for ( size_t i = 0; i < TEST_URING_STRESS_COUNT; i++ ) {
		testRequire(
			xrtNetPortAccept(pPort, Listener, (uint64)i + 1u, NULL),
			"io_uring cancellation stress accept failed"
		);
	}
	for ( size_t i = 0; i < TEST_URING_STRESS_COUNT; i++ ) {
		testRequire(
			xrtNetPortCancel(pPort, (uint64)i + 1u),
			"io_uring cancellation stress request failed"
		);
	}

	while ( iCompleted < TEST_URING_STRESS_COUNT ) {
		size_t iCount = 0;

		testRequire(
			xrtNetPortWait(
				pPort,
				Events,
				64,
				Deadline,
				&iCount
			) == XNET_RESULT_OK,
			"io_uring cancellation stress wait failed"
		);
		for ( size_t i = 0; i < iCount; i++ ) {
			size_t iIndex = (size_t)Events[i].Id - 1u;

			testRequire(
				(Events[i].Type == XNET_PORT_EVENT_ACCEPT) &&
				(Events[i].Result == XNET_RESULT_CANCELLED) &&
				(iIndex < TEST_URING_STRESS_COUNT) &&
				!Seen[iIndex],
				"io_uring cancellation stress terminal mismatch"
			);
			Seen[iIndex] = true;
			iCompleted++;
		}
	}

	testRequire(
		xrtNetSocketClose(Listener) &&
		xrtNetPortDestroy(pPort),
		"io_uring cancellation stress cleanup failed"
	);
}



/* 执行完成和取消两类高密度 ring 压力。 */
int main(void)
{
	testUringDatagrams();
	testUringCancellations();
	return 0;
}

#else

/* 非 Linux 平台不执行原生 ring 压力。 */
int main(void)
{
	testRequire(true, "non-Linux io_uring stress placeholder");
	return 0;
}

#endif
