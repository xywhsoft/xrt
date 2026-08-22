/*
 * XRT Example - TCP Client
 * XRT 范例 - TCP 客户端
 *
 * Description / 说明:
 *   EN: Demonstrates connecting to a loopback TCP server and sending one payload.
 *   CN: 演示连接本地 TCP 服务器并发送一条消息。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/network_tcp_client.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/network_tcp_client -lm -lpthread
 *
 * Note / 注意:
 *   - Start examples/network/tcp_echo_server first to observe the reply.
 */
#include "../../../xrt.c"
#include "../common.h"


int main()
{
	xnetengineconfig tCfg;
	xnetconnectconfig tConnCfg;
	xnetengine* pEngine = NULL;
	xnetstream* pClient = NULL;
	exnet_stream_ctx tClientCtx = { 0 };

	xrtInit();

	strcpy(tClientCtx.aSend, "hello from tcp_client");
	tClientCtx.bSendOnOpen = TRUE;

	xrtNetEngineConfigInit(&tCfg);
	tCfg.iWorkerCount = 1;
	xrtNetConnectConfigInit(&tConnCfg);
	tConnCfg.sHost = "127.0.0.1";
	tConnCfg.iPort = 19091;

	pEngine = xrtNetEngineCreate(&tCfg);
	xrtNetEngineStart(pEngine);
	pClient = xrtNetStreamCreate(pEngine, &gExNetStreamEvents, &tClientCtx);
	(void)xrtNetStreamConnect(pClient, &tConnCfg);

	(void)exNetWaitMin(&tClientCtx.iOpenCount, 1, 1000u);
	(void)exNetWaitMin(&tClientCtx.iRecvCount, 1, 1000u);

	printf("client_recv = %s\n", tClientCtx.aRecv);

	xrtNetStreamClose(pClient, 0);
	xrtSleep(50);
	xrtNetStreamDestroy(pClient);
	xrtNetEngineStop(pEngine);
	xrtNetEngineDestroy(pEngine);
	xrtUnit();
	return 0;
}
