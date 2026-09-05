#include <stdio.h>

#include <xrt.h>



/*
 * 范例：network/port_select —— select 兜底事件端口
 * ----------------------------------------------------------------
 * 演示 API：对应后端的事件端口原语（观察/重武装/直收缓冲）
 * port 层是引擎之下的"事件原语翻译层"：五种系统后端
 *   （IOCP/epoll/kqueue/io_uring/select）暴露统一语义——
 *   one-shot 观察 + 显式重武装，引擎在其上构建回调模型。
 * 模块宏：XRT_MODULE_NET
 * 编译（单头形态，全平台）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/network/port_select/main.c -lws2_32 -liphlpapi
 * 预期输出：（输出观察结果）
 */


/* 展示 select fallback 的 one-shot 可读观察和显式重新观察模型。 */
int main(void)
{
	xnetportconfig Config;
	xnetportevent Event;
	xnetport* pPort = NULL;
	xnetsocket Server = NULL;
	xnetsocket Client = NULL;
	xnetaddr Address;
	char sData[16] = { 0 };
	size_t iSize;
	size_t iCount;
	int iResult = 1;

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_SELECT;
	pPort = xrtNetPortCreate(&Config);
	Server = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	Client = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	if ( (pPort == NULL) || (Server == NULL) || (Client == NULL) ||
		 !xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) ||
		 !xrtNetSocketBind(Server, &Address) ||
		 !xrtNetSocketLocal(Server, &Address) ||
		 !xrtNetPortWatch(pPort, Server, 1, XNET_POLL_READ, NULL) ||
		 (xrtNetSocketSendTo(Client, "event", 5,
			&iSize, &Address) != XNET_RESULT_OK) ||
		 (xrtNetPortWait(pPort, &Event, 1,
			xrtDeadlineAfter(1000000), &iCount) != XNET_RESULT_OK) ||
		 (iCount != 1) ||
		 ((Event.Flags & XNET_PORT_EVENT_READ) == 0) ||
		 (xrtNetSocketRecvFrom(Server, sData, sizeof(sData) - 1,
			&iSize, NULL) != XNET_RESULT_OK) ) {
		goto Cleanup;
	}

	sData[iSize] = 0;
	printf("backend=%s data=%s\n", xrtNetPortName(pPort), sData);
	iResult = 0;

Cleanup:
	if ( Client != NULL ) { (void)xrtNetSocketClose(Client); }
	if ( Server != NULL ) { (void)xrtNetSocketClose(Server); }
	if ( pPort != NULL ) { (void)xrtNetPortDestroy(pPort); }
	return iResult;
}
