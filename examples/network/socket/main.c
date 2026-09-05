#include <stdio.h>
#include <string.h>

#include <xrt.h>



/*
 * 范例：network/socket —— 裸 UDP Socket：绑定/收发/元数据
 * ----------------------------------------------------------------
 * 演示 API：
 *   UDP Socket 底层路径   绑定、发送、接收、对端查询
 * 模块宏：XRT_MODULE_NET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/socket/main.c -lws2_32 -liphlpapi
 * 预期输出（端口随机）：
 *   bytes=6 data=packet source-port=NNNNN meta=0x...
 *
 * 最底层视角：不经引擎的同步 UDP socket——
 *   收到的是数据报边界（不是流）+ 来源地址 +
 *   附带元数据（TOS/收包时间等）。常规业务用
 *   udp 范例的引擎版（事件驱动 + 池化缓冲）。
 */


/* 展示最底层 UDP Socket 的绑定、发送、接收和地址查询路径。 */
int main(void)
{
	xnetsocket Server;
	xnetsocket Client;
	xnetaddr Address;
	xnetaddr ClientAddress;
	xnetaddr Remote;
	xnetdgrammeta Meta;
	xnetdgramcontrol Control;
	char sData[32] = { 0 };
	size_t iSize;
	uint32 iMeta;

	Server = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, 0);
	Client = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, 0);
	if ( (Server == NULL) || (Client == NULL) ) {
		if ( Client != NULL ) { (void)xrtNetSocketClose(Client); }
		if ( Server != NULL ) { (void)xrtNetSocketClose(Server); }
		return 1;
	}
	iMeta = xrtNetSocketDgramMetaAvailable(Server);
	if ( !xrtNetSocketDgramMetaSet(Server, iMeta) ||
		 !xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) ||
		 !xrtNetAddrLoopback(&ClientAddress, XNET_FAMILY_IPV4, 0) ||
		 !xrtNetSocketBind(Server, &Address) ||
		 !xrtNetSocketBind(Client, &ClientAddress) ||
		 !xrtNetSocketLocal(Server, &Address) ||
		 !xrtNetSocketLocal(Client, &ClientAddress) ||
		 ((xrtNetSocketDgramControlAvailable(Client) &
		   XNET_DGRAM_CONTROL_SOURCE) == 0) ) {
		if ( Client != NULL ) { (void)xrtNetSocketClose(Client); }
		if ( Server != NULL ) { (void)xrtNetSocketClose(Server); }
		return 1;
	}
	memset(&Control, 0, sizeof(Control));
	Control.Flags = XNET_DGRAM_CONTROL_SOURCE;
	Control.Source = ClientAddress;
	Control.Source.Port = 0;
	if ( (xrtNetSocketSendMsg(Client, "packet", 6,
			&iSize, &Address, &Control) != XNET_RESULT_OK) ||
		 (xrtNetSocketRecvMsg(Server, sData, sizeof(sData) - 1,
			&iSize, &Remote, &Meta) != XNET_RESULT_OK) ) {
		if ( Client != NULL ) { (void)xrtNetSocketClose(Client); }
		if ( Server != NULL ) { (void)xrtNetSocketClose(Server); }
		return 1;
	}

	sData[iSize] = 0;
	printf("bytes=%zu data=%s source-port=%u meta=0x%08x\n",
		iSize, sData, (unsigned int)Remote.Port,
		(unsigned int)Meta.Flags);
	(void)xrtNetSocketClose(Client);
	(void)xrtNetSocketClose(Server);
	return 0;
}
