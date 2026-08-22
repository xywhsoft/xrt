#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件包含 UDP 阻塞等待和拥有型接收。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetudpconfig UdpConfig;
	xnetengine* pEngine;
	xnetudp* pServer;
	xnetudp* pClient;
	xnetudppacket* pPacket;
	xnetaddr Address;

	if ( xrtNetUdpReceiveErrorWait(
		NULL,
		XRT_DEADLINE_NEVER,
		NULL
	) != NULL ) {
		return 1;
	}
	xrtClearError();
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		return 2;
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
		return 3;
	}
	pClient = xrtNetUdpConnect(
		pEngine,
		&Address,
		1,
		&UdpConfig,
		NULL,
		NULL
	);
	if ( (pClient == NULL) || !xrtNetUdpWait(
		pClient,
		XNET_UDP_WAIT_OPEN,
		xrtDeadlineAfter(3000000u),
		NULL
	) || (xrtNetUdpSend(pClient, "udp", 3) != XNET_RESULT_OK) ) {
		return 4;
	}
	pPacket = xrtNetUdpReceiveWait(
		pServer,
		xrtDeadlineAfter(3000000u),
		NULL
	);
	if ( (pPacket == NULL) || (xrtNetUdpPacketSize(pPacket) != 3) ||
		 (memcmp(xrtNetUdpPacketData(pPacket), "udp", 3) != 0) ) {
		return 5;
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
	return xrtNetEngineDestroy(pEngine) ? 0 : 6;
}
