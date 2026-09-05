#include <stdio.h>

#include <xrt.h>



/*
 * 范例：network/port_uring —— Linux io_uring 事件端口（直写调用方缓冲）
 * ----------------------------------------------------------------
 * port 层是引擎之下的"事件原语翻译层"：五种系统后端
 *   （IOCP/epoll/kqueue/io_uring/select）暴露统一语义——
 *   one-shot 观察 + 显式重武装，引擎在其上构建回调模型。
 * 模块宏：XRT_MODULE_NET
 * 编译（单头形态，Linux 5.1+）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/network/port_uring/main.c -lws2_32 -liphlpapi
 * 预期输出：（Linux 平台输出直收的数据报）
 */


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
