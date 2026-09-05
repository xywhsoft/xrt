/*
 * 范例：network/udp_sync —— 阻塞式 UDP 接收
 * ----------------------------------------------------------------
 * 演示 API：
 *   UDP 同步接收面（deadline 内等待状态/拉包）
 * 模块宏：XRT_MODULE_NET_UDP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/udp_sync/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   received: hello
 *
 * 与 tcp_sync 对称的 UDP 阻塞面。
 */


#include <stdio.h>
#include <xrt.h>



/* 使用现有 Engine 完成一次本地 UDP 阻塞接收。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetudpconfig UdpConfig;
	xnetengine* pEngine;
	xnetudp* pServer;
	xnetudp* pClient;
	xnetudppacket* pPacket;
	xnetaddr Address;
	bool bReceived;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 2;
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
	pClient = xrtNetUdpConnect(
		pEngine,
		&Address,
		1,
		&UdpConfig,
		NULL,
		NULL
	);
	if ( (pClient == NULL) ||
		 (xrtNetUdpSend(pClient, "hello", 5) != XNET_RESULT_OK) ) {
		return 3;
	}
	pPacket = xrtNetUdpReceiveWait(
		pServer,
		xrtDeadlineAfter(3000000u),
		NULL
	);
	bReceived = pPacket != NULL;
	if ( pPacket != NULL ) {
		printf(
			"received: %.*s\n",
			(int)xrtNetUdpPacketSize(pPacket),
			(cstr)xrtNetUdpPacketData(pPacket)
		);
	}
	xrtNetUdpPacketDestroy(pPacket);
	(void)xrtNetUdpAbort(pClient);
	(void)xrtNetUdpAbort(pServer);
	while ( (xrtNetUdpState(pClient) != XNET_UDP_CLOSED) ||
		 (xrtNetUdpState(pServer) != XNET_UDP_CLOSED) ) {
		xrtThreadYield();
	}
	xrtNetUdpDestroy(pClient);
	xrtNetUdpDestroy(pServer);
	return xrtNetEngineDestroy(pEngine) && bReceived ? 0 : 4;
}
