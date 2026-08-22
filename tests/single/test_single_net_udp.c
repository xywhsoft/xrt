#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



#if !defined(TEST_SINGLE_UDP_BACKEND)
	#define TEST_SINGLE_UDP_BACKEND XNET_PORT_SELECT
#endif



/* 在有限时间内等待 UDP 进入指定状态。 */
static bool testSingleUdpWaitState(xnetudp* pUdp, xnetudpstate State)
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



/* 在有限时间内从拉取队列取出一个包。 */
static xnetudppacket* testSingleUdpReceive(xnetudp* pUdp)
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



/* 验证单头文件能够完成 UDP 真实回环收发。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetudpconfig UdpConfig;
	xnetengine* pEngine;
	xnetudp* pServer = NULL;
	xnetudp* pClient = NULL;
	xnetudppacket* pPacket;
	xnetsocket Probe;
	xnetaddr Address;
	xnetaddr Local;
	xnetdgramcontrol Control;
	uint32 iMeta = 0;
	int iResult = 1;

	Probe = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_DGRAM, 0);
	if ( Probe == NULL ) {
		return 1;
	}
	iMeta = xrtNetSocketDgramMetaAvailable(Probe) &
		(XNET_DGRAM_META_DESTINATION | XNET_DGRAM_META_INTERFACE);
	(void)xrtNetSocketClose(Probe);

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_SINGLE_UDP_BACKEND;
	EngineConfig.Workers = 1;
	xrtNetUdpConfigInit(&UdpConfig);
	UdpConfig.ReceiveMeta = iMeta;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		return 1;
	}
	(void)xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0);
	pServer = xrtNetUdpBind(pEngine, &Address, 0,
		&UdpConfig, NULL, NULL);
	if ( (pServer == NULL) || !xrtNetUdpLocal(pServer, &Address) ) {
		iResult = 2;
		goto Cleanup;
	}
	pClient = xrtNetUdpConnect(pEngine, &Address, 0,
		NULL, NULL, NULL);
	if ( (pClient == NULL) ||
		 !testSingleUdpWaitState(pServer, XNET_UDP_OPEN) ||
		 !testSingleUdpWaitState(pClient, XNET_UDP_OPEN) ) {
		iResult = 3;
		goto Cleanup;
	}
	if ( !xrtNetUdpLocal(pClient, &Local) ||
		 xrtNetAddrIsUnspecified(&Local) ||
		 ((xrtNetUdpSendControlAvailable(pClient) &
		  XNET_DGRAM_CONTROL_SOURCE) == 0) ) {
		iResult = 4;
		goto Cleanup;
	}
	memset(&Control, 0, sizeof(Control));
	Control.Flags = XNET_DGRAM_CONTROL_SOURCE;
	Control.Source = Local;
	Control.Source.Port = 0;
	if ( xrtNetUdpSendMsg(
		pClient,
		NULL,
		&Control,
		"single-udp",
		10
	) != XNET_RESULT_OK ) {
		iResult = 4;
		goto Cleanup;
	}
	pPacket = testSingleUdpReceive(pServer);
	if ( (pPacket == NULL) ||
		 (xrtNetUdpPacketSize(pPacket) != 10) ||
		 (memcmp(xrtNetUdpPacketData(pPacket),
		  "single-udp", 10) != 0) ||
		 (xrtNetUdpPacketMeta(pPacket) == NULL) ||
		 ((xrtNetUdpPacketMeta(pPacket)->Flags & iMeta) != iMeta) ||
		 (((iMeta & XNET_DGRAM_META_DESTINATION) != 0) &&
		  !xrtNetAddrIsLoopback(
			&xrtNetUdpPacketMeta(pPacket)->Destination)) ||
		 (((iMeta & XNET_DGRAM_META_INTERFACE) != 0) &&
		  (xrtNetUdpPacketMeta(pPacket)->Interface == 0)) ) {
		xrtNetUdpPacketDestroy(pPacket);
		iResult = 5;
		goto Cleanup;
	}
	xrtNetUdpPacketDestroy(pPacket);
	iResult = 0;

Cleanup:
	if ( pClient != NULL ) {
		(void)xrtNetUdpClose(pClient);
		(void)testSingleUdpWaitState(pClient, XNET_UDP_CLOSED);
	}
	if ( pServer != NULL ) {
		(void)xrtNetUdpClose(pServer);
		(void)testSingleUdpWaitState(pServer, XNET_UDP_CLOSED);
	}
	xrtNetUdpDestroy(pClient);
	xrtNetUdpDestroy(pServer);
	if ( !xrtNetEngineDestroy(pEngine) && (iResult == 0) ) {
		iResult = 6;
	}
	return iResult;
}
