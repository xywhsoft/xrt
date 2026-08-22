#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



#if !defined(TEST_SINGLE_PORT_BACKEND)
	#define TEST_SINGLE_PORT_BACKEND XNET_PORT_EPOLL
	#if defined(__linux__)
		#define TEST_SINGLE_PORT_AVAILABLE 1
	#else
		#define TEST_SINGLE_PORT_AVAILABLE 0
	#endif
#endif



/* 单头文件必须保留目标后端的真实 one-shot readiness 路径。 */
int main(void)
{
	#if TEST_SINGLE_PORT_AVAILABLE
		xnetportconfig Config;
		xnetportevent Event;
		xnetport* pPort = NULL;
		xnetsocket Server = NULL;
		xnetsocket Client = NULL;
		xnetaddr Address;
		char sData[4];
		size_t iSize = 0;
		size_t iCount = 0;
		int iResult = 1;

		xrtNetPortConfigInit(&Config);
		Config.Backend = TEST_SINGLE_PORT_BACKEND;
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
		if ( (pPort == NULL) || (Server == NULL) || (Client == NULL) ||
			 !xrtNetAddrLoopback(
				&Address,
				XNET_FAMILY_IPV4,
				0
			 ) ||
			 !xrtNetSocketBind(Server, &Address) ||
			 !xrtNetSocketLocal(Server, &Address) ||
			 !xrtNetPortWatch(
				pPort,
				Server,
				1,
				XNET_POLL_READ,
				NULL
			 ) ||
			 (xrtNetSocketSendTo(
				Client,
				"epoll",
				4,
				&iSize,
				&Address
			 ) != XNET_RESULT_OK) ||
			 (xrtNetPortWait(
				pPort,
				&Event,
				1,
				xrtDeadlineAfter(1000000u),
				&iCount
			 ) != XNET_RESULT_OK) ||
			 (iCount != 1) ||
			 ((Event.Flags & XNET_PORT_EVENT_READ) == 0) ||
			 (xrtNetSocketRecvFrom(
				Server,
				sData,
				sizeof(sData),
				&iSize,
				NULL
			 ) != XNET_RESULT_OK) ||
			 (iSize != sizeof(sData)) ||
			 (memcmp(sData, "epoll", sizeof(sData)) != 0) ) {
			goto Cleanup;
		}
		iResult = 0;

	Cleanup:
		if ( Client != NULL ) {
			(void)xrtNetSocketClose(Client);
		}
		if ( Server != NULL ) {
			(void)xrtNetSocketClose(Server);
		}
		if ( pPort != NULL ) {
			(void)xrtNetPortDestroy(pPort);
		}
		return iResult;
	#else
		xnetportconfig Config;

		xrtNetPortConfigInit(&Config);
		Config.Backend = TEST_SINGLE_PORT_BACKEND;
		return (xrtNetPortCreate(&Config) == NULL) &&
			(xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED) ?
			0 : 1;
	#endif
}
