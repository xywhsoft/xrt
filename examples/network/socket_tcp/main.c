#include <stdio.h>

#include <xrt.h>



/*
 * 范例：network/socket_tcp —— 裸 TCP Socket 全路径（含半关闭）
 * ----------------------------------------------------------------
 * 演示 API：
 *   TCP Socket 底层路径   监听/连接/接受/收发/半关闭
 * 模块宏：XRT_MODULE_NET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/socket_tcp/main.c -lws2_32 -liphlpapi
 * 预期输出（端口随机）：
 *   bytes=5 data=hello client-port=NNNNN
 *
 * 与 tcp_sync 的分层：socket_tcp 是无引擎的纯系统
 *   调用层；半关闭（shutdown 写端）演示了优雅关闭的
 *   底层形态——业务代码用 xnetstream（见 tcp 范例）。
 */


/* 展示最底层 TCP Socket 的监听、连接、接受、收发和半关闭路径。 */
int main(void)
{
	xnetsocket Listener = NULL;
	xnetsocket Client = NULL;
	xnetsocket Accepted = NULL;
	xnetaddr Address;
	xnetaddr Remote;
	char sData[16] = { 0 };
	size_t iSize;
	int iResult = 1;

	Listener = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM, 0);
	Client = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM, 0);
	if ( (Listener == NULL) || (Client == NULL) ||
		 !xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) ||
		 !xrtNetSocketBind(Listener, &Address) ||
		 !xrtNetSocketLocal(Listener, &Address) ||
		 !xrtNetSocketListen(Listener, 16) ||
		 (xrtNetSocketConnect(Client, &Address) != XNET_RESULT_OK) ||
		 (xrtNetSocketAccept(Listener,
			&Accepted, &Remote) != XNET_RESULT_OK) ||
		 (xrtNetSocketSend(Client, "hello", 5,
			&iSize) != XNET_RESULT_OK) ||
		 (xrtNetSocketRecv(Accepted, sData, sizeof(sData) - 1,
			&iSize) != XNET_RESULT_OK) ) {
		goto Cleanup;
	}

	sData[iSize] = 0;
	printf("bytes=%zu data=%s client-port=%u\n",
		iSize, sData, (unsigned int)Remote.Port);
	if ( !xrtNetSocketShutdown(Client, XNET_SHUTDOWN_WRITE) ) {
		goto Cleanup;
	}
	iResult = 0;

Cleanup:
	if ( Accepted != NULL ) { (void)xrtNetSocketClose(Accepted); }
	if ( Client != NULL ) { (void)xrtNetSocketClose(Client); }
	if ( Listener != NULL ) { (void)xrtNetSocketClose(Listener); }
	return iResult;
}
