#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供 Socket 状态查询和元数据回环接收。 */
int main(void)
{
	xnetsocket Server = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	xnetsocket Client = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, 0);
	xnetaddr Address;
	xnetaddr ClientAddress;
	xnetaddr Remote;
	xnetdgrammeta Meta;
	xnetdgramcontrol Control;
	char sData[4] = { 0 };
	int64 iValue;
	size_t iSize;
	uint32 iMeta;
	int iResult = 1;

	if ( (Server == NULL) || (Client == NULL) ) {
		goto Cleanup;
	}
	if ( !xrtNetSocketGet(Server,
		XNET_OPTION_NONBLOCK, &iValue) || (iValue == 0) ) {
		goto Cleanup;
	}
	iMeta = xrtNetSocketDgramMetaAvailable(Server) &
		(XNET_DGRAM_META_DESTINATION | XNET_DGRAM_META_INTERFACE);
	if ( !xrtNetSocketDgramMetaSet(Server, iMeta) ||
		 !xrtNetSocketSet(Server, XNET_OPTION_NONBLOCK, 0) ||
		 !xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) ||
		 !xrtNetAddrLoopback(&ClientAddress, XNET_FAMILY_IPV4, 0) ||
		 !xrtNetSocketBind(Server, &Address) ||
		 !xrtNetSocketBind(Client, &ClientAddress) ||
		 !xrtNetSocketLocal(Server, &Address) ||
		 !xrtNetSocketLocal(Client, &ClientAddress) ) {
		goto Cleanup;
	}
	memset(&Control, 0, sizeof(Control));
	Control.Flags = XNET_DGRAM_CONTROL_SOURCE;
	Control.Source = ClientAddress;
	Control.Source.Port = 0;
	if ( ((xrtNetSocketDgramControlAvailable(Client) &
		  XNET_DGRAM_CONTROL_SOURCE) == 0) ||
		 (xrtNetSocketSendMsg(Client, "meta", 4,
			&iSize, &Address, &Control) != XNET_RESULT_OK) ||
		 (xrtNetSocketRecvMsg(Server, sData, sizeof(sData),
			&iSize, &Remote, &Meta) != XNET_RESULT_OK) ||
		 (iSize != sizeof(sData)) ||
		 (memcmp(sData, "meta", sizeof(sData)) != 0) ||
		 ((Meta.Flags & iMeta) != iMeta) ) {
		goto Cleanup;
	}
	iResult = 0;

Cleanup:
	if ( Client != NULL ) { (void)xrtNetSocketClose(Client); }
	if ( Server != NULL ) { (void)xrtNetSocketClose(Server); }
	return iResult;
}
