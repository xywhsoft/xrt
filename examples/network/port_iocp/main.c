#include <stdio.h>
#include <string.h>

#include <xrt.h>



/*
 * 范例：network/port_iocp —— Windows IOCP 事件端口（直写调用方缓冲）
 * ----------------------------------------------------------------
 * 演示 API：对应后端的事件端口原语（观察/重武装/直收缓冲）
 * port 层是引擎之下的"事件原语翻译层"：五种系统后端
 *   （IOCP/epoll/kqueue/io_uring/select）暴露统一语义——
 *   one-shot 观察 + 显式重武装，引擎在其上构建回调模型。
 * 模块宏：XRT_MODULE_NET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/network/port_iocp/main.c -lws2_32 -liphlpapi
 * 预期输出：（输出直收的数据报内容）
 */


/* 展示 IOCP 直接向调用方缓冲异步接收数据报。 */
int main(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		xnetportconfig Config;
		xnetportevent Events[2];
		xnetport* pPort = NULL;
		xnetsocket Server = NULL;
		xnetsocket Client = NULL;
		xnetaddr Address;
		xnetaddr ClientAddress;
		xnetdgramcontrol Control;
		char sData[16] = { 0 };
		size_t iCount = 0;
		bool bSent = false;
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
		if ( !xrtNetPortRecvFrom(pPort, Server,
				sData, sizeof(sData) - 1, 1, NULL) ||
			 !xrtNetPortSendMsg(pPort, Client,
				"completion", 10, &Address, &Control, 2, NULL) ||
			 (xrtNetPortWait(pPort, Events, 2,
				xrtDeadlineAfter(1000000), &iCount) != XNET_RESULT_OK) ||
			 (iCount != 2) ) {
			goto Cleanup;
		}

		for ( size_t i = 0; i < iCount; i++ ) {
			if ( Events[i].Type == XNET_PORT_EVENT_RECV_FROM ) {
				sData[Events[i].Bytes] = 0;
				printf("backend=%s bytes=%zu data=%s\n",
					xrtNetPortName(pPort), Events[i].Bytes, sData);
			} else if ( (Events[i].Type == XNET_PORT_EVENT_SEND_MSG) &&
				(Events[i].Result == XNET_RESULT_OK) ) {
				bSent = true;
			}
		}
		if ( !bSent ) {
			goto Cleanup;
		}
		iResult = 0;

	Cleanup:
		if ( Client != NULL ) { (void)xrtNetSocketClose(Client); }
		if ( Server != NULL ) { (void)xrtNetSocketClose(Server); }
		if ( pPort != NULL ) { (void)xrtNetPortDestroy(pPort); }
		return iResult;
	#else
		puts("IOCP is available on Windows only.");
		return 0;
	#endif
}
