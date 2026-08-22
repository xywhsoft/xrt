#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须保留真实 IOCP 数据报完成路径。 */
int main(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		xnetportconfig Config;
		xnetportevent Events[2];
		const xnetportevent* pReceive = NULL;
		const xnetportevent* pSend = NULL;
		xnetport* pPort = NULL;
		xnetsocket Server = NULL;
		xnetsocket Client = NULL;
		xnetaddr Address;
		xnetaddr ClientAddress;
		xnetdgramcontrol Control;
		char sData[4] = { 0 };
		size_t iCount = 0;
		int iResult = 1;

		xrtNetPortConfigInit(&Config);
		Config.Backend = XNET_PORT_IOCP;
		pPort = xrtNetPortCreate(&Config);
		Server = xrtNetSocketOpen(XNET_FAMILY_IPV4,
			XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
		Client = xrtNetSocketOpen(XNET_FAMILY_IPV4,
			XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
		if ( (pPort == NULL) || (Server == NULL) || (Client == NULL) ||
			 !xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) ||
			 !xrtNetAddrLoopback(
				&ClientAddress,
				XNET_FAMILY_IPV4,
				0
			 ) ||
			 !xrtNetSocketDgramMetaSet(Server,
				XNET_DGRAM_META_DESTINATION |
				XNET_DGRAM_META_INTERFACE) ||
			 !xrtNetSocketBind(Server, &Address) ||
			 !xrtNetSocketBind(Client, &ClientAddress) ||
			 !xrtNetSocketLocal(Server, &Address) ||
			 !xrtNetSocketLocal(Client, &ClientAddress) ||
			 ((xrtNetSocketDgramControlAvailable(Client) &
			   XNET_DGRAM_CONTROL_SOURCE) == 0) ) {
			goto Cleanup;
		}
		memset(&Control, 0, sizeof(Control));
		Control.Flags = XNET_DGRAM_CONTROL_SOURCE;
		Control.Source = ClientAddress;
		Control.Source.Port = 0;
		if ( !xrtNetPortRecvMsg(pPort, Server,
				sData, sizeof(sData), 1, NULL) ||
			 !xrtNetPortSendMsg(pPort, Client,
				"iocp", 4, &Address, &Control, 2, NULL) ||
			 (xrtNetPortWait(pPort, Events, 2,
				xrtDeadlineAfter(1000000), &iCount) != XNET_RESULT_OK) ||
			 (iCount != 2) ) {
			goto Cleanup;
		}
		for ( size_t i = 0; i < iCount; i++ ) {
			if ( Events[i].Id == 1 ) {
				pReceive = &Events[i];
			} else if ( Events[i].Id == 2 ) {
				pSend = &Events[i];
			}
		}
		if ( (pReceive == NULL) || (pSend == NULL) ||
			 (pReceive->Type != XNET_PORT_EVENT_RECV_MSG) ||
			 (pReceive->Result != XNET_RESULT_OK) ||
			 (pReceive->Bytes != sizeof(sData)) ||
			 (memcmp(sData, "iocp", sizeof(sData)) != 0) ||
			 ((pReceive->Meta.Flags &
				(XNET_DGRAM_META_DESTINATION |
				 XNET_DGRAM_META_INTERFACE)) !=
				(XNET_DGRAM_META_DESTINATION |
				 XNET_DGRAM_META_INTERFACE)) ||
			 (pSend->Type != XNET_PORT_EVENT_SEND_MSG) ||
			 (pSend->Result != XNET_RESULT_OK) ||
			 (pSend->Bytes != sizeof(sData)) ) {
			goto Cleanup;
		}
		iResult = 0;

	Cleanup:
		if ( Client != NULL ) { (void)xrtNetSocketClose(Client); }
		if ( Server != NULL ) { (void)xrtNetSocketClose(Server); }
		if ( pPort != NULL ) { (void)xrtNetPortDestroy(pPort); }
		return iResult;
	#else
		return 0;
	#endif
}
