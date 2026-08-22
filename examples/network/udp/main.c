#include <stdio.h>
#include <string.h>
#include <xrt.h>



/* 在截止时间内等待 UDP 状态转换。 */
static bool exampleUdpWaitState(xnetudp* pUdp, xnetudpstate State)
{
	xdeadline iDeadline = xrtDeadlineAfter(3000000u);

	while ( xrtNetUdpState(pUdp) != State ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}



/* 在截止时间内拉取一个 UDP 包。 */
static xnetudppacket* exampleUdpReceive(xnetudp* pUdp)
{
	xdeadline iDeadline = xrtDeadlineAfter(3000000u);
	xnetudppacket* pPacket;

	for ( ;; ) {
		pPacket = xrtNetUdpReceive(pUdp);
		if ( pPacket != NULL ) {
			return pPacket;
		}
		if ( xrtDeadlineExpired(iDeadline) ) {
			return NULL;
		}
		xrtThreadYield();
	}
}



/* 建立绑定端和连接端，完成一次 UDP 回环收发。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetudpconfig UdpConfig;
	xnetengine* pEngine = NULL;
	xnetudp* pServer = NULL;
	xnetudp* pClient = NULL;
	xnetudppacket* pPacket = NULL;
	xnetaddr Address;
	xnetdgramcontrol Control;
	str sEndpoint = NULL;
	int iResult = 1;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 2;
	xrtNetUdpConfigInit(&UdpConfig);
	UdpConfig.ReceiveMeta = XNET_DGRAM_META_DESTINATION;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		goto Cleanup;
	}
	(void)xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0);
	pServer = xrtNetUdpBind(pEngine, &Address, 0,
		&UdpConfig, NULL, NULL);
	if ( (pServer == NULL) || !xrtNetUdpLocal(pServer, &Address) ) {
		iResult = 2;
		goto Cleanup;
	}
	sEndpoint = xrtNetAddrEndpointString(&Address);
	if ( sEndpoint == NULL ) {
		iResult = 3;
		goto Cleanup;
	}
	printf("UDP server: %s\n", sEndpoint);
	xrtFree(sEndpoint);
	sEndpoint = NULL;
	pClient = xrtNetUdpConnect(pEngine, &Address, 1,
		NULL, NULL, NULL);
	if ( (pClient == NULL) ||
		 !exampleUdpWaitState(pServer, XNET_UDP_OPEN) ||
		 !exampleUdpWaitState(pClient, XNET_UDP_OPEN) ) {
		iResult = 4;
		goto Cleanup;
	}
	if ( xrtNetUdpSend(pClient, "hello UDP", 9) !=
		 XNET_RESULT_OK ) {
		iResult = 5;
		goto Cleanup;
	}
	pPacket = exampleUdpReceive(pServer);
	if ( pPacket == NULL ) {
		iResult = 6;
		goto Cleanup;
	}
	printf("received: %.*s\n",
		(int)xrtNetUdpPacketSize(pPacket),
		(cstr)xrtNetUdpPacketData(pPacket));
	if ( ((xrtNetUdpPacketMeta(pPacket)->Flags &
		  XNET_DGRAM_META_DESTINATION) == 0) ||
		 ((xrtNetUdpSendControlAvailable(pServer) &
		  XNET_DGRAM_CONTROL_SOURCE) == 0) ) {
		iResult = 7;
		goto Cleanup;
	}
	memset(&Control, 0, sizeof(Control));
	Control.Flags = XNET_DGRAM_CONTROL_SOURCE;
	Control.Source = xrtNetUdpPacketMeta(pPacket)->Destination;
	Control.Source.Port = 0;
	if ( xrtNetUdpSendMsg(
		pServer,
		xrtNetUdpPacketRemote(pPacket),
		&Control,
		"reply",
		5
	) != XNET_RESULT_OK ) {
		iResult = 8;
		goto Cleanup;
	}
	xrtNetUdpPacketDestroy(pPacket);
	pPacket = exampleUdpReceive(pClient);
	if ( (pPacket == NULL) || (xrtNetUdpPacketSize(pPacket) != 5) ||
		 (memcmp(xrtNetUdpPacketData(pPacket), "reply", 5) != 0) ) {
		iResult = 9;
		goto Cleanup;
	}
	printf("response: %.*s\n",
		(int)xrtNetUdpPacketSize(pPacket),
		(cstr)xrtNetUdpPacketData(pPacket));
	iResult = 0;

Cleanup:
	xrtNetUdpPacketDestroy(pPacket);
	xrtFree(sEndpoint);
	if ( pClient != NULL ) {
		(void)xrtNetUdpClose(pClient);
		(void)exampleUdpWaitState(pClient, XNET_UDP_CLOSED);
	}
	if ( pServer != NULL ) {
		(void)xrtNetUdpClose(pServer);
		(void)exampleUdpWaitState(pServer, XNET_UDP_CLOSED);
	}
	xrtNetUdpDestroy(pClient);
	xrtNetUdpDestroy(pServer);
	if ( (pEngine != NULL) && !xrtNetEngineDestroy(pEngine) &&
		 (iResult == 0) ) {
		iResult = 10;
	}
	return iResult;
}
