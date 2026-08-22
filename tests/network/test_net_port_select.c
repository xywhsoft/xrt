#include "../test.h"



#if !defined(TEST_PORT_BACKEND)
	#define TEST_PORT_BACKEND XNET_PORT_SELECT
	#define TEST_PORT_BACKEND_NAME "select"
	#define TEST_PORT_AVAILABLE 1
#endif



#if TEST_PORT_AVAILABLE

/* 等待一次端口事件并要求恰好返回指定数量。 */
static void testPortWait(xnetport* pPort,
	xnetportevent* pEvents, size_t iCapacity, size_t iExpected)
{
	size_t iCount = 0;

	testRequire(xrtNetPortWait(pPort, pEvents, iCapacity,
		xrtDeadlineAfter(1000000), &iCount) == XNET_RESULT_OK,
		"readiness port wait failed");
	testRequire(iCount == iExpected,
		"readiness port event count mismatch");
}



/* 用户事件保持 FIFO，显式唤醒会合并成一个事件。 */
static void testPortPosts(xnetport* pPort)
{
	xnetportevent Events[4];
	int iFirst = 1;
	int iSecond = 2;
	size_t iCount = 7;

	testRequire(xrtNetPortPost(pPort, 11, &iFirst) &&
		xrtNetPortPost(pPort, 12, &iSecond),
		"posting readiness port events failed");
	testRequire(!xrtNetPortPost(pPort, 13, NULL),
		"full readiness port post queue accepted another event");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_AGAIN) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_PORT_POST),
		"readiness port post backpressure error mismatch");
	xrtClearError();
	testRequire(xrtNetPortWake(pPort) &&
		xrtNetPortWake(pPort) && xrtNetPortWake(pPort),
		"coalesced readiness port wake failed");
	testPortWait(pPort, Events, 4, 3);
	testRequire((Events[0].Type == XNET_PORT_EVENT_USER) &&
		(Events[0].Id == 11) && (Events[0].User == &iFirst) &&
		(Events[1].Type == XNET_PORT_EVENT_USER) &&
		(Events[1].Id == 12) && (Events[1].User == &iSecond) &&
		(Events[2].Type == XNET_PORT_EVENT_WAKE),
		"readiness port post ordering or wake coalescing mismatch");
	testRequire(xrtNetPortWait(pPort, Events, 4,
		xrtDeadlineAfter(0), &iCount) == XNET_RESULT_TIMEOUT &&
		(iCount == 0), "empty readiness port did not time out");
}



/* readiness 观察必须 one-shot 报告事件，并保留最新身份和用户上下文。 */
static void testPortReadiness(xnetport* pPort)
{
	xnetsocket Server;
	xnetsocket Client;
	xnetaddr Address;
	xnetaddr Remote;
	xnetportevent Events[2];
	int iServerUser = 1;
	int iClientUser = 2;
	char sData[8] = { 0 };
	size_t iSize;
	size_t iCount = 9;

	Server = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	Client = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	testRequire((Server != NULL) && (Client != NULL),
		"readiness socket setup failed");
	testRequire(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) &&
		xrtNetSocketBind(Server, &Address) &&
		xrtNetSocketLocal(Server, &Address),
		"readiness bind failed");

	testRequire(xrtNetPortWatch(pPort, Server,
		101, XNET_POLL_READ, &iServerUser),
		"readiness read watch failed");
	testRequire(xrtNetPortWatch(pPort, Server,
		102, XNET_POLL_READ, &iClientUser),
		"readiness watch replacement failed");
	testRequire(!xrtNetPortWatch(pPort, Client,
		202, XNET_POLL_WRITE, &iClientUser),
		"readiness watch capacity was not enforced");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_PORT_WATCH),
		"readiness watch capacity error mismatch");
	xrtClearError();

	testRequire((xrtNetSocketSendTo(Client, "ready", 5,
		&iSize, &Address) == XNET_RESULT_OK) && (iSize == 5),
		"readiness send failed");
	testPortWait(pPort, Events, 2, 1);
	testRequire((Events[0].Type == XNET_PORT_EVENT_READY) &&
		((Events[0].Flags & XNET_PORT_EVENT_READ) != 0) &&
		(Events[0].Id == 102) && (Events[0].Socket == Server) &&
		(Events[0].User == &iClientUser),
		"readiness replacement identity mismatch");

	/* 未重新观察时，即使数据仍未读取也不能重复报告。 */
	testRequire(xrtNetPortWait(pPort, Events, 2,
		xrtDeadlineAfter(0), &iCount) == XNET_RESULT_TIMEOUT &&
		(iCount == 0), "readiness one-shot watch repeated without rearm");
	testRequire((xrtNetSocketRecvFrom(Server, sData, sizeof(sData),
		&iSize, &Remote) == XNET_RESULT_OK) && (iSize == 5) &&
		(memcmp(sData, "ready", 5) == 0),
		"readiness receive failed");

	/*
		同时关注两个方向时，已报告方向清除，尚未报告方向保留。
		这压实 one-shot 不会误丢另一方向的公开契约。
	*/
	testRequire(xrtNetPortWatch(pPort, Server,
		303, XNET_POLL_READ | XNET_POLL_WRITE, &iServerUser),
		"readiness combined watch failed");
	testPortWait(pPort, Events, 2, 1);
	testRequire((Events[0].Id == 303) &&
		((Events[0].Flags & XNET_PORT_EVENT_WRITE) != 0) &&
		((Events[0].Flags & XNET_PORT_EVENT_READ) == 0),
		"readiness combined watch first direction mismatch");
	testRequire((xrtNetSocketSendTo(Client, "again", 5,
		&iSize, &Address) == XNET_RESULT_OK) && (iSize == 5),
		"readiness preserved-direction send failed");
	testPortWait(pPort, Events, 2, 1);
	testRequire((Events[0].Id == 303) &&
		((Events[0].Flags & XNET_PORT_EVENT_READ) != 0),
		"readiness watch lost the unreported direction");
	testRequire((xrtNetSocketRecvFrom(Server, sData, sizeof(sData),
		&iSize, &Remote) == XNET_RESULT_OK) && (iSize == 5) &&
		(memcmp(sData, "again", 5) == 0),
		"readiness preserved-direction receive failed");

	/* 写关注立即可用，事件后自动移除，Unwatch 保持幂等。 */
	testRequire(xrtNetPortWatch(pPort, Client,
		202, XNET_POLL_WRITE, &iClientUser),
		"readiness write watch failed");
	testPortWait(pPort, Events, 2, 1);
	testRequire((Events[0].Type == XNET_PORT_EVENT_READY) &&
		((Events[0].Flags & XNET_PORT_EVENT_WRITE) != 0) &&
		(Events[0].Id == 202) && (Events[0].Socket == Client) &&
		(Events[0].User == &iClientUser),
		"readiness write event identity mismatch");
	testRequire(xrtNetPortUnwatch(pPort, Client) &&
		xrtNetPortUnwatch(pPort, Client),
		"readiness idempotent unwatch failed");

	testRequire(xrtNetSocketClose(Client) &&
		xrtNetSocketClose(Server), "readiness socket close failed");
}



/* 容量为一时，持续 Post 积压也必须给原生 readiness 事件轮转机会。 */
static void testPortPostFairness(xnetport* pPort)
{
	xnetsocket Server;
	xnetsocket Client;
	xnetaddr Address;
	xnetaddr Remote;
	xnetportevent Event;
	char sData[8] = { 0 };
	size_t iSize;

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
	testRequire((Server != NULL) && (Client != NULL),
		"fairness socket setup failed");
	testRequire(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) &&
		xrtNetSocketBind(Server, &Address) &&
		xrtNetSocketLocal(Server, &Address) &&
		xrtNetPortWatch(pPort, Server, 401, XNET_POLL_READ, NULL),
		"fairness readiness setup failed");
	testRequire(xrtNetPortPost(pPort, 501, NULL) &&
		xrtNetPortPost(pPort, 502, NULL) &&
		(xrtNetSocketSendTo(
			Client,
			"ready",
			5,
			&iSize,
			&Address
		) == XNET_RESULT_OK),
		"fairness input setup failed");

	testPortWait(pPort, &Event, 1, 1);
	testRequire((Event.Type == XNET_PORT_EVENT_USER) &&
		(Event.Id == 501), "fairness first post mismatch");
	testPortWait(pPort, &Event, 1, 1);
	testRequire((Event.Type == XNET_PORT_EVENT_READY) &&
		(Event.Id == 401) &&
		((Event.Flags & XNET_PORT_EVENT_READ) != 0),
		"post backlog starved readiness event");
	testPortWait(pPort, &Event, 1, 1);
	testRequire((Event.Type == XNET_PORT_EVENT_USER) &&
		(Event.Id == 502), "fairness second post mismatch");
	testRequire((xrtNetSocketRecvFrom(
		Server,
		sData,
		sizeof(sData),
		&iSize,
		&Remote
	) == XNET_RESULT_OK) && (iSize == 5) &&
		(memcmp(sData, "ready", 5) == 0),
		"fairness datagram receive failed");
	testRequire(xrtNetSocketClose(Client) &&
		xrtNetSocketClose(Server), "fairness socket close failed");
}



/* 执行一个 readiness 后端的完整基础回归。 */
int main(void)
{
	xnetportconfig Config;
	xnetport* pPort;
	uint32 iCapabilities;

	xrtNetPortConfigInit(&Config);
	Config.Backend = TEST_PORT_BACKEND;
	Config.PostLimit = 2;
	Config.WatchLimit = 1;
	pPort = xrtNetPortCreate(&Config);
	testRequire(pPort != NULL, "creating readiness port failed");
	iCapabilities = xrtNetPortCapabilities(pPort);
	testRequire((xrtNetPortBackend(pPort) == TEST_PORT_BACKEND) &&
		(strcmp(xrtNetPortName(pPort), TEST_PORT_BACKEND_NAME) == 0) &&
		((iCapabilities & XNET_PORT_CAP_READINESS) != 0) &&
		((iCapabilities & XNET_PORT_CAP_COMPLETION) == 0) &&
		((iCapabilities & XNET_PORT_CAP_ONESHOT) != 0),
		"readiness port identity or capabilities mismatch");

	testPortPosts(pPort);
	testPortReadiness(pPort);
	testPortPostFairness(pPort);
	testRequire(xrtNetPortBackend(NULL) == XNET_PORT_AUTO,
		"invalid port query unexpectedly succeeded");
	{
		xerror* pPrevious = xrtErrorRef(xrtGetError());

		testRequire((pPrevious != NULL) && xrtNetPortDestroy(pPort),
			"destroying readiness port failed");
		testRequire(xrtGetError() == pPrevious,
			"successful port destroy discarded the previous error");
		xrtClearError();
		xrtErrorFree(pPrevious);
	}
	return 0;
}

#else

/* 不支持目标后端的平台必须返回明确的 unsupported 错误。 */
int main(void)
{
	xnetportconfig Config;

	xrtNetPortConfigInit(&Config);
	Config.Backend = TEST_PORT_BACKEND;
	testRequire(xrtNetPortCreate(&Config) == NULL,
		"unavailable readiness backend was created");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_PORT_CREATE),
		"unavailable readiness backend error mismatch");
	return 0;
}

#endif
