/*
 * XRT Example - UDP Echo
 * XRT 范例 - UDP Echo
 *
 * Description / 说明:
 *   EN: Demonstrates UDP send and receive on loopback with an echo server.
 *   CN: 演示本地回环上的 UDP 发送、接收与回显。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/network_udp_echo.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/network_udp_echo -lm -lpthread
 */
#include "../../../xrt.c"
#include "../common.h"


int main()
{
	xnetengineconfig tCfg;
	xnetdgramconfig tServerCfg;
	xnetdgramconfig tClientCfg;
	xnetengine* pEngine = NULL;
	xdgramsock* pServer = NULL;
	xdgramsock* pClient = NULL;
	exnet_dgram_ctx tServerCtx = { 0 };
	exnet_dgram_ctx tClientCtx = { 0 };
	xnetaddr tServerAddr;

	xrtInit();

	xrtNetEngineConfigInit(&tCfg);
	tCfg.iWorkerCount = 1;
	xrtNetDgramConfigInit(&tServerCfg);
	xrtNetDgramConfigInit(&tClientCfg);
	xrtNetAddrInitAny(&tServerCfg.tBindAddr, AF_INET, 19093);
	xrtNetAddrInitAny(&tClientCfg.tBindAddr, AF_INET, 0);
	tServerCtx.bEchoBack = TRUE;

	pEngine = xrtNetEngineCreate(&tCfg);
	xrtNetEngineStart(pEngine);
	pServer = xrtNetDgramCreate(pEngine, &tServerCfg, &gExNetDgramEvents, &tServerCtx);
	pClient = xrtNetDgramCreate(pEngine, &tClientCfg, &gExNetDgramEvents, &tClientCtx);
	xrtNetDgramStart(pServer);
	xrtNetDgramStart(pClient);

	(void)xrtNetAddrParse(&tServerAddr, "127.0.0.1", 19093);
	(void)xrtNetDgramSendTo(pClient, &tServerAddr, "udp-echo", 8);

	(void)exNetWaitMin(&tServerCtx.iRecvCount, 1, 1000u);
	(void)exNetWaitMin(&tClientCtx.iRecvCount, 1, 1000u);

	printf("server_recv = %s\n", tServerCtx.aRecv);
	printf("client_recv = %s\n", tClientCtx.aRecv);

	xrtNetDgramStop(pClient);
	xrtNetDgramStop(pServer);
	xrtNetDgramDestroy(pClient);
	xrtNetDgramDestroy(pServer);
	xrtNetEngineStop(pEngine);
	xrtNetEngineDestroy(pEngine);
	xrtUnit();
	return 0;
}
