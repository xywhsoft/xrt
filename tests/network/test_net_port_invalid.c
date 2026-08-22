#include "../test.h"



/* 端口 API 必须压实空参数、非法掩码和输出初始化。 */
int main(void)
{
	xnetportconfig Config;
	xnetport* pPort;
	xnetsocket Socket;
	xnetportevent Event;
	size_t iCount = 99;

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_SELECT;
	pPort = xrtNetPortCreate(&Config);
	Socket = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	testRequire((pPort != NULL) && (Socket != NULL),
		"invalid port test setup failed");

	testRequire(!xrtNetPortWatch(pPort, Socket,
		1, 0x80000000u, NULL), "unknown watch mask was accepted");
	testRequire(!xrtNetPortWatch(pPort, NULL,
		1, XNET_POLL_READ, NULL), "null watch socket was accepted");
	testRequire(xrtNetPortWait(pPort, &Event, 1,
		XRT_DEADLINE_NEVER, NULL) == XNET_RESULT_ERROR,
		"null wait count was accepted");
	testRequire((xrtNetPortWait(pPort, NULL, 1,
		XRT_DEADLINE_NEVER, &iCount) == XNET_RESULT_ERROR) &&
		(iCount == 0), "null wait events output mismatch");
	testRequire((xrtNetPortWait(pPort, &Event, 0,
		XRT_DEADLINE_NEVER, &iCount) == XNET_RESULT_ERROR) &&
		(iCount == 0), "zero wait capacity output mismatch");

	testRequire(xrtNetPortUnwatch(pPort, Socket),
		"unwatching absent socket failed");
	testRequire(xrtNetSocketClose(Socket),
		"invalid port test socket close failed");
	testRequire(xrtNetPortDestroy(pPort),
		"invalid port test destroy failed");
	return 0;
}
