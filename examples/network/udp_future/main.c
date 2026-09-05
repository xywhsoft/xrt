/*
 * 范例：network/udp_future —— Future 化 UDP 收发
 * ----------------------------------------------------------------
 * 演示 API：
 *   UDP 包拉取 Future
 * 模块宏：XRT_MODULE_NET_UDP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/udp_future/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   server received: hello UDP Future
 *   client received: received
 *
 * 与 tcp_future 同构的 UDP 版：无连接服务端 +
 *   连接式客户端经 Future 完成请求响应。
 */


#include <stdio.h>
#include <string.h>
#include <xrt.h>



/* 等待一个 UDP Future 成功解析。 */
static bool exampleUdpFutureWait(xfuture* pFuture)
{
	return (pFuture != NULL) &&
		(xrtFutureWaitFor(pFuture, UINT64_C(3000000)) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED);
}



/* 使用拉取 Future 完成一次无连接服务端与连接式客户端的请求响应。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetudpconfig UdpConfig;
	xnetengine* pEngine;
	xnetudp* pServer;
	xnetudp* pClient;
	xnetaddr Address;
	xfuture* pServerOpen;
	xfuture* pClientOpen;
	xfuture* pRequest;
	xfuture* pReply;
	xfuture* pServerClose;
	xfuture* pClientClose;
	xnetudppacket* pPacket;
	static const char sRequest[] = "hello UDP Future";
	static const char sReply[] = "received";

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		return 1;
	}
	xrtNetUdpConfigInit(&UdpConfig);
	(void)xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0);
	pServer = xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&UdpConfig,
		NULL,
		NULL
	);
	if ( (pServer == NULL) || !xrtNetUdpLocal(pServer, &Address) ) {
		return 2;
	}
	pServerOpen = xrtNetUdpWaitAsync(pServer, XNET_UDP_WAIT_OPEN);
	pClient = xrtNetUdpConnect(
		pEngine,
		&Address,
		0,
		&UdpConfig,
		NULL,
		NULL
	);
	pClientOpen = xrtNetUdpWaitAsync(pClient, XNET_UDP_WAIT_OPEN);
	if ( !exampleUdpFutureWait(pServerOpen) ||
		 !exampleUdpFutureWait(pClientOpen) ) {
		return 3;
	}

	pRequest = xrtNetUdpReceiveAsync(pServer);
	if ( (pRequest == NULL) || (xrtNetUdpSend(
		pClient,
		sRequest,
		sizeof(sRequest) - 1u
	) != XNET_RESULT_OK) || !exampleUdpFutureWait(pRequest) ) {
		return 4;
	}
	pPacket = (xnetudppacket*)xrtFutureValue(pRequest);
	printf("server received: %.*s\n",
		(int)xrtNetUdpPacketSize(pPacket),
		(const char*)xrtNetUdpPacketData(pPacket));

	pReply = xrtNetUdpReceiveAsync(pClient);
	if ( (pReply == NULL) || (xrtNetUdpSendTo(
		pServer,
		xrtNetUdpPacketRemote(pPacket),
		sReply,
		sizeof(sReply) - 1u
	) != XNET_RESULT_OK) || !exampleUdpFutureWait(pReply) ) {
		return 5;
	}
	pPacket = (xnetudppacket*)xrtFutureValue(pReply);
	printf("client received: %.*s\n",
		(int)xrtNetUdpPacketSize(pPacket),
		(const char*)xrtNetUdpPacketData(pPacket));

	pServerClose = xrtNetUdpWaitAsync(pServer, XNET_UDP_WAIT_CLOSE);
	pClientClose = xrtNetUdpWaitAsync(pClient, XNET_UDP_WAIT_CLOSE);
	if ( (pServerClose == NULL) || (pClientClose == NULL) ||
		 !xrtNetUdpClose(pServer) || !xrtNetUdpClose(pClient) ||
		 !exampleUdpFutureWait(pServerClose) ||
		 !exampleUdpFutureWait(pClientClose) ) {
		return 6;
	}
	xrtFutureDestroy(pServerOpen);
	xrtFutureDestroy(pClientOpen);
	xrtFutureDestroy(pRequest);
	xrtFutureDestroy(pReply);
	xrtFutureDestroy(pServerClose);
	xrtFutureDestroy(pClientClose);
	xrtNetUdpDestroy(pServer);
	xrtNetUdpDestroy(pClient);
	return xrtNetEngineDestroy(pEngine) ? 0 : 7;
}
