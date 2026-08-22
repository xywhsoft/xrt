#include <stdio.h>

#include <xrt.h>



/* 展示 io_uring 直接向调用方缓冲异步接收数据报。 */
int main(void)
{
	#if defined(__linux__)
		xnetportconfig Config;
		xnetportevent Events[2];
		xnetport* pPort = NULL;
		xnetsocket Server = NULL;
		xnetsocket Client = NULL;
		xnetaddr Address;
		char sData[16] = { 0 };
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
				sizeof(sData) - 1u,
				1,
				NULL
			 ) ||
			 !xrtNetPortSendTo(
				pPort,
				Client,
				"completion",
				10,
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
			 (iCount != 2) ) {
			goto Cleanup;
		}

		for ( size_t i = 0; i < iCount; i++ ) {
			if ( Events[i].Type == XNET_PORT_EVENT_RECV_FROM ) {
				sData[Events[i].Bytes] = 0;
				printf(
					"backend=%s bytes=%zu data=%s\n",
					xrtNetPortName(pPort),
					Events[i].Bytes,
					sData
				);
			}
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
		puts("io_uring is available on Linux only.");
		return 0;
	#endif
}
