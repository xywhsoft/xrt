/*
 * XRT Example - Listener Future
 * XRT 范例 - 监听器 Future
 *
 * Description / 说明:
 *   EN: Demonstrates waiting for a loopback TCP accept operation through
 *       xrtNetListenerAcceptFuture and then exchanging payloads.
 *   CN: 演示通过 xrtNetListenerAcceptFuture 等待本地 TCP accept，
 *       然后继续交换数据。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/network_listener_future.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/network_listener_future -lm -lpthread
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
	exnet_stream_ctx tClientCtx;
	exnet_stream_ctx tServerCtx;
	xnet_result iAcceptStatus = XRT_NET_ERROR;
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
		printf("prepare = failed\n");
		iRet = 1;
		goto cleanup;
	}

	tConnCfg.iPort = pListener->tConfig.tBindAddr.iPort;
	if ( xrtNetStreamConnect(pClient, &tConnCfg) != XRT_NET_OK ) {
		printf("connect = failed\n");
		iRet = 1;
		goto cleanup;
	}

	iAcceptStatus = xrtNetFutureWait(pAcceptFuture, 3000u);
	if ( iAcceptStatus != XRT_NET_OK ) {
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
	tServerCtx.bEchoBack = TRUE;
	xrtNetStreamSetUserData(pAccepted, &tServerCtx);

	xrtSleep(100u);
	if ( xrtNetStreamSend(pClient, "listener-future", 15u) != XRT_NET_OK ) {
		printf("client_send = failed\n");
		iRet = 1;
		goto cleanup;
	}

	if ( !exNetWaitMin(&tServerCtx.iRecvCount, 1, 1000u) || !exNetWaitMin(&tClientCtx.iRecvCount, 1, 1000u) ) {
		printf("exchange = timeout\n");
		iRet = 1;
		goto cleanup;
	}

	printf("accept_status = %d\n", (int)iAcceptStatus);
	printf("accepted_remote = %s\n", xrtNetAddrToStr(xrtNetStreamRemoteAddr(pAccepted)));
	printf("server_recv = %s\n", tServerCtx.aRecv);
	printf("client_recv = %s\n", tClientCtx.aRecv);

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
