#include "../test.h"



#if !defined(TEST_PORT_BACKEND)
	#define TEST_PORT_BACKEND XNET_PORT_EPOLL
	#if defined(__linux__)
		#define TEST_PORT_AVAILABLE 1
	#else
		#define TEST_PORT_AVAILABLE 0
	#endif
#endif

/* 数百个独立 Socket 足以压实哈希观察表、批量提取和 one-shot 移除。 */
#define TEST_READINESS_WATCH_COUNT 256u



#if TEST_PORT_AVAILABLE



/* 大批 UDP readiness 不得丢失、重复或串错 Socket 身份。 */
int main(void)
{
	xnetsocket Servers[TEST_READINESS_WATCH_COUNT];
	xnetaddr Addresses[TEST_READINESS_WATCH_COUNT];
	bool Seen[TEST_READINESS_WATCH_COUNT];
	xnetportevent Events[32];
	xnetportconfig Config;
	xnetport* pPort;
	xnetsocket Client;
	xdeadline Deadline;
	size_t iCompleted = 0;

	memset(Servers, 0, sizeof(Servers));
	memset(Addresses, 0, sizeof(Addresses));
	memset(Seen, 0, sizeof(Seen));
	xrtNetPortConfigInit(&Config);
	Config.Backend = TEST_PORT_BACKEND;
	Config.WatchLimit = TEST_READINESS_WATCH_COUNT;
	pPort = xrtNetPortCreate(&Config);
	Client = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM,
		XNET_SOCKET_NONBLOCK
	);
	testRequire((pPort != NULL) && (Client != NULL),
		"readiness stress setup failed");

	for ( size_t i = 0; i < TEST_READINESS_WATCH_COUNT; i++ ) {
		Servers[i] = xrtNetSocketOpen(
			XNET_FAMILY_IPV4,
			XNET_SOCKET_DGRAM,
			XNET_SOCKET_NONBLOCK
		);
		testRequire((Servers[i] != NULL) &&
			xrtNetAddrLoopback(
				&Addresses[i],
				XNET_FAMILY_IPV4,
				0
			) &&
			xrtNetSocketBind(Servers[i], &Addresses[i]) &&
			xrtNetSocketLocal(Servers[i], &Addresses[i]) &&
			xrtNetPortWatch(
				pPort,
				Servers[i],
				(uint64)i + 1u,
				XNET_POLL_READ,
				Servers[i]
			),
			"readiness stress watch setup failed");
	}
	testRequire(!xrtNetPortWatch(
		pPort,
		Client,
		TEST_READINESS_WATCH_COUNT + 1u,
		XNET_POLL_READ,
		Client
	), "readiness stress watch limit was not enforced");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_PORT_WATCH),
		"readiness stress watch limit error mismatch");
	xrtClearError();

	for ( size_t i = 0; i < TEST_READINESS_WATCH_COUNT; i++ ) {
		unsigned char iByte = (unsigned char)i;
		size_t iSent = 0;

		testRequire((xrtNetSocketSendTo(
			Client,
			&iByte,
			1,
			&iSent,
			&Addresses[i]
		) == XNET_RESULT_OK) && (iSent == 1),
			"readiness stress datagram send failed");
	}

	Deadline = xrtDeadlineAfter(5000000u);
	while ( iCompleted < TEST_READINESS_WATCH_COUNT ) {
		size_t iCount = 0;

		testRequire(xrtNetPortWait(
			pPort,
			Events,
			sizeof(Events) / sizeof(Events[0]),
			Deadline,
			&iCount
		) == XNET_RESULT_OK, "readiness stress wait failed");
		testRequire(iCount != 0,
			"readiness stress wait made no progress");
		for ( size_t i = 0; i < iCount; i++ ) {
			size_t iIndex = (size_t)Events[i].Id - 1u;
			unsigned char iByte = 0;
			size_t iReceived = 0;

			testRequire((Events[i].Type == XNET_PORT_EVENT_READY) &&
				((Events[i].Flags & XNET_PORT_EVENT_READ) != 0) &&
				(iIndex < TEST_READINESS_WATCH_COUNT) &&
				(Events[i].Socket == Servers[iIndex]) &&
				(Events[i].User == Servers[iIndex]) &&
				!Seen[iIndex],
				"readiness stress event identity duplicated or corrupted");
			testRequire((xrtNetSocketRecvFrom(
				Servers[iIndex],
				&iByte,
				1,
				&iReceived,
				NULL
			) == XNET_RESULT_OK) &&
				(iReceived == 1) &&
				(iByte == (unsigned char)iIndex),
				"readiness stress datagram payload mismatch");
			Seen[iIndex] = true;
			iCompleted++;
		}
	}

	for ( size_t i = 0; i < TEST_READINESS_WATCH_COUNT; i++ ) {
		testRequire(Seen[i] && xrtNetSocketClose(Servers[i]),
			"readiness stress server cleanup failed");
	}
	testRequire(xrtNetSocketClose(Client),
		"readiness stress client cleanup failed");
	testRequire(xrtNetPortDestroy(pPort),
		"readiness stress port cleanup failed");
	return 0;
}

#else

/* 不支持目标 readiness 后端的平台只验证空分支可独立构建。 */
int main(void)
{
	(void)testRequire;
	return 0;
}

#endif
