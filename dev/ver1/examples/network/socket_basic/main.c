/*
 * XRT Example - Socket Basic
 * XRT 范例 - 基础套接字流程
 *
 * Description / 说明:
 *   EN: Demonstrates listener creation, client connect, send and receive on loopback.
 *   CN: 演示监听、连接、发送与接收的基础流程。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/network_socket_basic.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/network_socket_basic -lm -lpthread
 */
#include "../../../xrt.c"
#include "../common.h"


int main()
{
	xnetengineconfig tCfg;
	xnetlistenconfig tListenCfg;
	xnetconnectconfig tConnCfg;
	xnetengine* pEngine = NULL;
	xnetlistener* pListener = NULL;
	xnetstream* pClient = NULL;
	xnetstream* pAccepted = NULL;
	xnetfuture* pAcceptFuture = NULL;
	exnet_stream_ctx tClientCtx = { 0 };
	exnet_stream_ctx tServerCtx = { 0 };

	xrtInit();

	xrtNetEngineConfigInit(&tCfg);
	tCfg.iWorkerCount = 1;
	xrtNetListenConfigInit(&tListenCfg);
	xrtNetConnectConfigInit(&tConnCfg);
	xrtNetAddrInitAny(&tListenCfg.tBindAddr, AF_INET, 0);
	tConnCfg.sHost = "127.0.0.1";

	pEngine = xrtNetEngineCreate(&tCfg);
	xrtNetEngineStart(pEngine);

	pListener = xrtNetListenerCreate(pEngine, &tListenCfg, NULL, &gExNetStreamEvents, NULL);
	xrtNetListenerStart(pListener);

	pAcceptFuture = xrtNetListenerAcceptFuture(pListener);
	pClient = xrtNetStreamCreate(pEngine, &gExNetStreamEvents, &tClientCtx);
	tConnCfg.iPort = pListener->tConfig.tBindAddr.iPort;
	(void)xrtNetStreamConnect(pClient, &tConnCfg);

	(void)xrtNetFutureWait(pAcceptFuture, 3000u);
	pAccepted = (xnetstream*)xrtNetFutureValue(pAcceptFuture);
	xrtNetStreamSetUserData(pAccepted, &tServerCtx);
	tServerCtx.bEchoBack = TRUE;

	xrtSleep(100);
	(void)xrtNetStreamSend(pClient, "socket-basic", 12);

	(void)exNetWaitMin(&tServerCtx.iRecvCount, 1, 1000u);
	(void)exNetWaitMin(&tClientCtx.iRecvCount, 1, 1000u);

	printf("server_recv = %s\n", tServerCtx.aRecv);
	printf("client_recv = %s\n", tClientCtx.aRecv);

	xrtNetStreamClose(pClient, 0);
	xrtNetStreamClose(pAccepted, 0);
	xrtSleep(50);
	xrtNetFutureDestroy(pAcceptFuture);
	xrtNetStreamDestroy(pClient);
	xrtNetStreamDestroy(pAccepted);
	xrtNetListenerStop(pListener);
	xrtNetListenerDestroy(pListener);
	xrtNetEngineStop(pEngine);
	xrtNetEngineDestroy(pEngine);
	xrtUnit();
	return 0;
}
