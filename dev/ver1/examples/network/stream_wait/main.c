/*
 * XRT Example - Stream Wait
 * XRT 范例 - 网络流等待
 *
 * Description / 说明:
 *   EN: Demonstrates stream readable wait-source and drain waiting on a
 *       loopback TCP pair.
 *   CN: 演示在本地 TCP 对端之间使用 stream readable wait-source 和 drain 等待。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/network_stream_wait.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/network_stream_wait -lm -lpthread
 */

#include "../../../xrt.c"
#include "../common.h"
#include <stdio.h>
#include <string.h>


int main(void)
{
	xnetengineconfig tCfg;
	xnetlistenconfig tListenCfg;
	xnetconnectconfig tConnCfg;
	xnetengine* pEngine = NULL;
	xnetlistener* pListener = NULL;
	xnetstream* pClient = NULL;
	xnetstream* pAccepted = NULL;
	xnetfuture* pAcceptFuture = NULL;
	xnetwaitsrc tReadableSrc;
	exnet_stream_ctx tClientCtx;
	exnet_stream_ctx tServerCtx;
	xnet_result iReadableStatus = XRT_NET_ERROR;
	xnet_result iDrainStatus = XRT_NET_ERROR;
	int iRet = 0;

	memset(&tCfg, 0, sizeof(tCfg));
	memset(&tListenCfg, 0, sizeof(tListenCfg));
	memset(&tConnCfg, 0, sizeof(tConnCfg));
	memset(&tClientCtx, 0, sizeof(tClientCtx));
	memset(&tServerCtx, 0, sizeof(tServerCtx));
	xrtInit();

	xrtNetEngineConfigInit(&tCfg);
	tCfg.iWorkerCount = 1u;
	xrtNetListenConfigInit(&tListenCfg);
	xrtNetConnectConfigInit(&tConnCfg);
	xrtNetAddrInitAny(&tListenCfg.tBindAddr, AF_INET, 0u);
	tConnCfg.sHost = "127.0.0.1";

	pEngine = xrtNetEngineCreate(&tCfg);
	if ( pEngine == NULL || xrtNetEngineStart(pEngine) != XRT_NET_OK ) {
		printf("engine = failed\n");
		iRet = 1;
		goto cleanup;
	}

	pListener = xrtNetListenerCreate(pEngine, &tListenCfg, NULL, &gExNetStreamEvents, NULL);
	if ( pListener == NULL || xrtNetListenerStart(pListener) != XRT_NET_OK ) {
		printf("listener = failed\n");
		iRet = 1;
		goto cleanup;
	}

	pAcceptFuture = xrtNetListenerAcceptFuture(pListener);
	pClient = xrtNetStreamCreate(pEngine, &gExNetStreamEvents, &tClientCtx);
	if ( pAcceptFuture == NULL || pClient == NULL ) {
		printf("stream_prepare = failed\n");
		iRet = 1;
		goto cleanup;
	}

	tConnCfg.iPort = pListener->tConfig.tBindAddr.iPort;
	if ( xrtNetStreamConnect(pClient, &tConnCfg) != XRT_NET_OK ) {
		printf("connect = failed\n");
		iRet = 1;
		goto cleanup;
	}

	if ( xrtNetFutureWait(pAcceptFuture, 3000u) != XRT_NET_OK ) {
		printf("accept_wait = failed\n");
		iRet = 1;
		goto cleanup;
	}

	pAccepted = (xnetstream*)xrtNetFutureValue(pAcceptFuture);
	if ( pAccepted == NULL ) {
		printf("accepted = null\n");
		iRet = 1;
		goto cleanup;
	}
	xrtNetStreamSetUserData(pAccepted, &tServerCtx);

	xrtSleep(100u);
	xrtNetStreamPauseRead(pClient);
	tReadableSrc = xrtNetWaitSourceStream(pClient, XNET_STREAM_WAIT_READABLE);

	if ( xrtNetStreamSend(pAccepted, "future-readable", 15u) != XRT_NET_OK ) {
		printf("server_send = failed\n");
		iRet = 1;
		goto cleanup;
	}

	iReadableStatus = xrtNetWaitSourceWaitTimeout(&tReadableSrc, 1000u);
	xrtNetStreamResumeRead(pClient);
	if ( iReadableStatus != XRT_NET_OK || !exNetWaitMin(&tClientCtx.iRecvCount, 1, 1000u) ) {
		printf("readable_wait = failed\n");
		iRet = 1;
		goto cleanup;
	}

	if ( xrtNetStreamSend(pClient, "drain-check", 11u) != XRT_NET_OK ) {
		printf("client_send = failed\n");
		iRet = 1;
		goto cleanup;
	}

	iDrainStatus = xrtNetStreamWaitTimeoutEx(pClient, XNET_STREAM_WAIT_DRAIN, 1000u);
	if ( iDrainStatus != XRT_NET_OK || !exNetWaitMin(&tServerCtx.iRecvCount, 1, 1000u) ) {
		printf("drain_wait = failed\n");
		iRet = 1;
		goto cleanup;
	}

	printf("readable_status = %d\n", (int)iReadableStatus);
	printf("drain_status = %d\n", (int)iDrainStatus);
	printf("client_recv = %s\n", tClientCtx.aRecv);
	printf("server_recv = %s\n", tServerCtx.aRecv);

cleanup:
	if ( pClient != NULL ) {
		xrtNetStreamClose(pClient, 0u);
	}
	if ( pAccepted != NULL ) {
		xrtNetStreamClose(pAccepted, 0u);
	}
	xrtSleep(50u);
	if ( pAcceptFuture != NULL ) {
		xrtNetFutureDestroy(pAcceptFuture);
	}
	if ( pClient != NULL ) {
		xrtNetStreamDestroy(pClient);
	}
	if ( pAccepted != NULL ) {
		xrtNetStreamDestroy(pAccepted);
	}
	if ( pListener != NULL ) {
		xrtNetListenerStop(pListener);
		xrtNetListenerDestroy(pListener);
	}
	if ( pEngine != NULL ) {
		xrtNetEngineStop(pEngine);
		xrtNetEngineDestroy(pEngine);
	}
	xrtUnit();
	return iRet;
}
