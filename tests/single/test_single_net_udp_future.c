#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件包含 UDP 打开、真实收发、Packet 所有权和关闭 Future。 */
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
	xfuture* pReceive;
	xfuture* pServerClose;
	xfuture* pClientClose;
	xnetudppacket* pPacket;
	static const char sPayload[] = "single UDP Future";

	if ( xrtNetUdpReceiveErrorAsync(NULL) != NULL ) {
		return 1;
	}
	xrtClearError();
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		return 2;
	}
	xrtNetUdpConfigInit(&UdpConfig);
	if ( !xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) ) {
		return 3;
	}
	pServer = xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&UdpConfig,
		NULL,
		NULL
	);
	if ( (pServer == NULL) || !xrtNetUdpLocal(pServer, &Address) ) {
		return 4;
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
	if ( pClient == NULL ) {
		return 5;
	}
	pClientOpen = xrtNetUdpWaitAsync(pClient, XNET_UDP_WAIT_OPEN);
	if ( (pServerOpen == NULL) || (pClientOpen == NULL) ||
		 (xrtFutureWaitFor(pServerOpen, UINT64_C(3000000)) != XWAIT_OK) ||
		 (xrtFutureState(pServerOpen) != XFUTURE_RESOLVED) ||
		 (xrtFutureWaitFor(pClientOpen, UINT64_C(3000000)) != XWAIT_OK) ||
		 (xrtFutureState(pClientOpen) != XFUTURE_RESOLVED) ) {
		return 6;
	}
	pReceive = xrtNetUdpReceiveAsync(pServer);
	if ( (pReceive == NULL) || (xrtNetUdpSend(
		pClient,
		sPayload,
		sizeof(sPayload) - 1u
	) != XNET_RESULT_OK) ) {
		return 7;
	}
	if ( (xrtFutureWaitFor(pReceive, UINT64_C(3000000)) != XWAIT_OK) ||
		 (xrtFutureState(pReceive) != XFUTURE_RESOLVED) ) {
		return 8;
	}
	pPacket = xrtNetUdpPacketRef(
		(xnetudppacket*)xrtFutureValue(pReceive)
	);
	xrtFutureDestroy(pReceive);
	if ( (pPacket == NULL) ||
		 (xrtNetUdpPacketSize(pPacket) != (sizeof(sPayload) - 1u)) ||
		 (memcmp(
			xrtNetUdpPacketData(pPacket),
			sPayload,
			 sizeof(sPayload) - 1u
		 ) != 0) ) {
		return 9;
	}
	xrtNetUdpPacketDestroy(pPacket);

	pServerClose = xrtNetUdpWaitAsync(pServer, XNET_UDP_WAIT_CLOSE);
	pClientClose = xrtNetUdpWaitAsync(pClient, XNET_UDP_WAIT_CLOSE);
	if ( (pServerClose == NULL) || (pClientClose == NULL) ||
		 !xrtNetUdpClose(pServer) || !xrtNetUdpClose(pClient) ||
		 (xrtFutureWaitFor(pServerClose, UINT64_C(3000000)) != XWAIT_OK) ||
		 (xrtFutureState(pServerClose) != XFUTURE_RESOLVED) ||
		 (xrtFutureWaitFor(pClientClose, UINT64_C(3000000)) != XWAIT_OK) ||
		 (xrtFutureState(pClientClose) != XFUTURE_RESOLVED) ) {
		return 10;
	}
	xrtFutureDestroy(pServerOpen);
	xrtFutureDestroy(pClientOpen);
	xrtFutureDestroy(pServerClose);
	xrtFutureDestroy(pClientClose);
	xrtNetUdpDestroy(pServer);
	xrtNetUdpDestroy(pClient);
	return xrtNetEngineDestroy(pEngine) ? 0 : 11;
}
