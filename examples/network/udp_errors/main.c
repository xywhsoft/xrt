/*
 * 范例：network/udp_errors —— 异步错误接收与 PMTU 策略
 * ----------------------------------------------------------------
 * 演示 API：
 *   拥有型异步错误队列 / PMTU 探测策略
 * 模块宏：XRT_MODULE_NET_UDP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/udp_errors/main.c -lws2_32 -liphlpapi
 * 预期输出（Windows 无错误队列时）：
 *   datagram error queue is unavailable on this platform
 *
 * 向关闭端口发包触发 ICMP 不可达——错误作为数据
 *   异步送达（而非阻塞报错）；平台不支持时优雅报告，
 *   能力探测范式与 file/fifo 一致。
 */


#include <stdio.h>

#include <xrt.h>



/* 在本机关闭端口上演示 PMTU 策略和拥有型异步错误接收。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetudpconfig UdpConfig;
	xnetengine* pEngine = NULL;
	xnetudp* pUdp = NULL;
	xnetudperrorpacket* pPacket = NULL;
	const xnetdgramerror* pError;
	xnetsocket Probe = NULL;
	xnetsocket Reserved = NULL;
	xnetaddr Target;
	uint32 iCapabilities;
	int iResult = 1;

	Probe = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_DGRAM, 0);
	if ( Probe == NULL ) {
		goto Cleanup;
	}
	iCapabilities = xrtNetSocketDgramCapabilities(Probe);
	(void)xrtNetSocketClose(Probe);
	Probe = NULL;
	if ( (iCapabilities & XNET_DGRAM_CAP_ERROR_QUEUE) == 0 ) {
		printf("datagram error queue is unavailable on this platform\n");
		iResult = 0;
		goto Cleanup;
	}

	Reserved = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_DGRAM, 0);
	if ( (Reserved == NULL) ||
		 !xrtNetAddrLoopback(&Target, XNET_FAMILY_IPV4, 0) ||
		 !xrtNetSocketBind(Reserved, &Target) ||
		 !xrtNetSocketLocal(Reserved, &Target) ) {
		iResult = 2;
		goto Cleanup;
	}
	(void)xrtNetSocketClose(Reserved);
	Reserved = NULL;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		iResult = 3;
		goto Cleanup;
	}
	xrtNetUdpConfigInit(&UdpConfig);
	UdpConfig.ReceiveErrors = true;
	UdpConfig.PathMtu = XNET_PMTU_DISCOVER;
	pUdp = xrtNetUdpConnect(
		pEngine,
		&Target,
		0,
		&UdpConfig,
		NULL,
		NULL
	);
	if ( (pUdp == NULL) || !xrtNetUdpWait(
		pUdp,
		XNET_UDP_WAIT_OPEN,
		xrtDeadlineAfter(3000000u),
		NULL
	) ) {
		iResult = 4;
		goto Cleanup;
	}
	if ( xrtNetUdpSend(pUdp, "probe", 5) != XNET_RESULT_OK ) {
		iResult = 5;
		goto Cleanup;
	}
	pPacket = xrtNetUdpReceiveErrorWait(
		pUdp,
		xrtDeadlineAfter(3000000u),
		NULL
	);
	if ( pPacket == NULL ) {
		iResult = 6;
		goto Cleanup;
	}
	pError = xrtNetUdpErrorPacketInfo(pPacket);
	printf(
		"origin=%d system=%d mtu=%zu payload=%zu\n",
		(int)pError->Origin,
		pError->SystemCode,
		pError->PathMtu,
		xrtNetUdpErrorPacketSize(pPacket)
	);
	xrtNetUdpErrorPacketDestroy(pPacket);
	pPacket = NULL;
	if ( !xrtNetUdpClose(pUdp) || !xrtNetUdpWait(
		pUdp,
		XNET_UDP_WAIT_CLOSE,
		xrtDeadlineAfter(3000000u),
		NULL
	) ) {
		iResult = 7;
		goto Cleanup;
	}
	xrtNetUdpDestroy(pUdp);
	pUdp = NULL;
	iResult = xrtNetEngineDestroy(pEngine) ? 0 : 8;
	pEngine = NULL;

Cleanup:
	xrtNetUdpErrorPacketDestroy(pPacket);
	if ( pUdp != NULL ) {
		(void)xrtNetUdpAbort(pUdp);
		(void)xrtNetUdpWait(
			pUdp,
			XNET_UDP_WAIT_CLOSE,
			xrtDeadlineAfter(3000000u),
			NULL
		);
		xrtNetUdpDestroy(pUdp);
	}
	if ( pEngine != NULL ) {
		(void)xrtNetEngineDestroy(pEngine);
	}
	if ( Reserved != NULL ) {
		(void)xrtNetSocketClose(Reserved);
	}
	if ( Probe != NULL ) {
		(void)xrtNetSocketClose(Probe);
	}
	return iResult;
}
