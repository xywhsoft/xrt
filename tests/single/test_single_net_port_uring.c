#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须保留真实 io_uring 数据报完成路径。 */
int main(void)
{
	#if defined(__linux__)
		xnetportconfig Config;
		xnetportevent Events[2];
		xnetport* pPort = NULL;
		xnetsocket Server = NULL;
		xnetsocket Client = NULL;
		xnetaddr Address;
		char sData[5] = { 0 };
		size_t iCount = 0;
		int iResult = 1;

		xrtNetPortConfigInit(&Config);
		Config.Backend = XNET_PORT_URING;
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
			 !xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) ||
			 !xrtNetSocketBind(Server, &Address) ||
			 !xrtNetSocketLocal(Server, &Address) ||
			 !xrtNetPortRecvFrom(
				pPort,
				Server,
				sData,
				sizeof(sData),
				1,
				NULL
			 ) ||
			 !xrtNetPortSendTo(
				pPort,
				Client,
				"uring",
				5,
				&Address,
				2,
				NULL
			 ) ||
			 (xrtNetPortWait(
				pPort,
				Events,
				2,
				xrtDeadlineAfter(1000000u),
				&iCount
			 ) != XNET_RESULT_OK) ||
			 (iCount != 2) ||
			 (memcmp(sData, "uring", sizeof(sData)) != 0) ) {
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
		return 0;
	#endif
}
