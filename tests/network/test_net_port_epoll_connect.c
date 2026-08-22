#include "../test.h"



#if defined(__linux__)

/* 取得一个已经释放、当前没有 Listener 的本地端口。 */
static uint16 testEpollUnusedPort(void)
{
	xnetsocket Socket;
	xnetaddr Address;

	Socket = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		XNET_SOCKET_NONBLOCK
	);
	testRequire(Socket != NULL, "epoll unused port socket open failed");
	testRequire(
		xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) &&
		xrtNetSocketBind(Socket, &Address) &&
		xrtNetSocketLocal(Socket, &Address),
		"epoll unused port allocation failed"
	);
	testRequire(
		xrtNetSocketClose(Socket),
		"epoll unused port socket close failed"
	);
	return Address.Port;
}



/* EPOLLERR 不得提前消费 FinishConnect 需要读取的 SO_ERROR。 */
int main(void)
{
	xnetportconfig Config;
	xnetportevent Event;
	xnetsocket Socket;
	xnetaddr Address;
	xnetport* pPort;
	xnetresult Result;
	size_t iCount = 0;

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_EPOLL;
	pPort = xrtNetPortCreate(&Config);
	Socket = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		XNET_SOCKET_NONBLOCK
	);
	testRequire(
		(pPort != NULL) && (Socket != NULL),
		"epoll connect error setup failed"
	);
	testRequire(
		xrtNetAddrLoopback(
			&Address,
			XNET_FAMILY_IPV4,
			testEpollUnusedPort()
		),
		"epoll connect error address failed"
	);

	Result = xrtNetSocketConnect(Socket, &Address);
	if ( Result == XNET_RESULT_AGAIN ) {
		testRequire(
			xrtNetPortWatch(
				pPort,
				Socket,
				1,
				XNET_POLL_WRITE,
				NULL
			),
			"epoll failed-connect watch failed"
		);
		testRequire(
			xrtNetPortWait(
				pPort,
				&Event,
				1,
				xrtDeadlineAfter(3000000),
				&iCount
			) == XNET_RESULT_OK,
			"epoll failed-connect wait failed"
		);
		testRequire(
			(iCount == 1) &&
			(Event.Type == XNET_PORT_EVENT_READY) &&
			((Event.Flags & (XNET_PORT_EVENT_WRITE |
				XNET_PORT_EVENT_ERROR |
				XNET_PORT_EVENT_HANGUP)) != 0),
			"epoll failed-connect readiness mismatch"
		);
		testRequire(
			xrtNetSocketFinishConnect(Socket) == XNET_RESULT_ERROR,
			"epoll consumed the pending connect error"
		);
	} else {
		testRequire(
			Result == XNET_RESULT_ERROR,
			"closed loopback port unexpectedly connected"
		);
	}

	testRequire(
		xrtNetSocketClose(Socket) && xrtNetPortDestroy(pPort),
		"epoll connect error cleanup failed"
	);
	return 0;
}

#else

/* 非 Linux 平台只验证 epoll 模块不会误入运行路径。 */
int main(void)
{
	testRequire(true, "epoll connect test is Linux-only");
	return 0;
}

#endif
