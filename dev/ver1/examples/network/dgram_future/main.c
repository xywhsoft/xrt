/*
 * XRT Example - Datagram Future
 * XRT 范例 - 数据报 Future
 *
 * Description / 说明:
 *   EN: Demonstrates waiting for one UDP packet through xrtNetDgramRecvFuture
 *       and reading the returned packet payload.
 *   CN: 演示通过 xrtNetDgramRecvFuture 等待一个 UDP 数据报，
 *       并读取返回的数据包内容。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/network_dgram_future.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/network_dgram_future -lm -lpthread
 */

#include "../../../xrt.c"
#include <stdio.h>
#include <string.h>


int main(void)
{
	xnetengineconfig tCfg;
	xnetdgramconfig tServerCfg;
	xnetdgramconfig tClientCfg;
	xnetengine* pEngine = NULL;
	xdgramsock* pServer = NULL;
	xdgramsock* pClient = NULL;
	xnetfuture* pRecvFuture = NULL;
	xnetdgrampkt* pPacket = NULL;
	xnetaddr tServerAddr;
	char aPacketText[256];
	const xnetaddr* pFrom = NULL;
	xnet_result iRecvStatus = XRT_NET_ERROR;
	int iRet = 0;

	memset(&tCfg, 0, sizeof(tCfg));
	memset(&tServerCfg, 0, sizeof(tServerCfg));
	memset(&tClientCfg, 0, sizeof(tClientCfg));
	memset(&tServerAddr, 0, sizeof(tServerAddr));
	memset(aPacketText, 0, sizeof(aPacketText));
	xrtInit();

	xrtNetEngineConfigInit(&tCfg);
	tCfg.iWorkerCount = 1u;
	xrtNetDgramConfigInit(&tServerCfg);
	xrtNetDgramConfigInit(&tClientCfg);
	xrtNetAddrInitAny(&tServerCfg.tBindAddr, AF_INET, 0u);
	xrtNetAddrInitAny(&tClientCfg.tBindAddr, AF_INET, 0u);

	pEngine = xrtNetEngineCreate(&tCfg);
	if ( pEngine == NULL || xrtNetEngineStart(pEngine) != XRT_NET_OK ) {
		printf("engine = failed\n");
		iRet = 1;
		goto cleanup;
	}

	pServer = xrtNetDgramCreate(pEngine, &tServerCfg, NULL, NULL);
	pClient = xrtNetDgramCreate(pEngine, &tClientCfg, NULL, NULL);
	if ( pServer == NULL || pClient == NULL ) {
		printf("dgram_create = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( xrtNetDgramStart(pServer) != XRT_NET_OK || xrtNetDgramStart(pClient) != XRT_NET_OK ) {
		printf("dgram_start = failed\n");
		iRet = 1;
		goto cleanup;
	}

	pRecvFuture = xrtNetDgramRecvFuture(pServer);
	if ( pRecvFuture == NULL ) {
		printf("recv_future = failed\n");
		iRet = 1;
		goto cleanup;
	}

	if ( xrtNetAddrParse(&tServerAddr, "127.0.0.1", pServer->tLocalAddr.iPort) != XRT_NET_OK ) {
		printf("addr_parse = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( xrtNetDgramSendTo(pClient, &tServerAddr, "future-dgram", 12u) != XRT_NET_OK ) {
		printf("send = failed\n");
		iRet = 1;
		goto cleanup;
	}

	iRecvStatus = xrtNetFutureWait(pRecvFuture, 3000u);
	if ( iRecvStatus != XRT_NET_OK ) {
		printf("recv_wait = failed\n");
		iRet = 1;
		goto cleanup;
	}

	pPacket = (xnetdgrampkt*)xrtNetFutureValue(pRecvFuture);
	if ( pPacket == NULL ) {
		printf("packet = null\n");
		iRet = 1;
		goto cleanup;
	}

	(void)xrtNetDgramPacketPeek(pPacket, aPacketText, sizeof(aPacketText) - 1u);
	pFrom = xrtNetDgramPacketFrom(pPacket);

	printf("recv_status = %d\n", (int)iRecvStatus);
	printf("packet_from = %s\n", pFrom ? xrtNetAddrToStr(pFrom) : "(null)");
	printf("packet_text = %s\n", aPacketText);

cleanup:
	if ( pPacket != NULL ) {
		xrtNetDgramPacketDestroy(pPacket);
	}
	if ( pRecvFuture != NULL ) {
		xrtNetFutureDestroy(pRecvFuture);
	}
	if ( pClient != NULL ) {
		xrtNetDgramStop(pClient);
		xrtNetDgramDestroy(pClient);
	}
	if ( pServer != NULL ) {
		xrtNetDgramStop(pServer);
		xrtNetDgramDestroy(pServer);
	}
	if ( pEngine != NULL ) {
		xrtNetEngineStop(pEngine);
		xrtNetEngineDestroy(pEngine);
	}
	xrtUnit();
	return iRet;
}
