/*
 * XRT Example - TCP Echo Server
 * XRT 范例 - TCP Echo 服务器
 *
 * Description / 说明:
 *   EN: Demonstrates a loopback TCP echo server using xrtNet listener and stream callbacks.
 *   CN: 演示使用 xrtNet 监听器与流回调实现 TCP Echo 服务器。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/network_tcp_echo_server.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/network_tcp_echo_server -lm -lpthread
 */
#include "../../../xrt.c"
#include "../common.h"


int main()
{
	xnetengineconfig tCfg;
	xnetlistenconfig tListenCfg;
	xnetengine* pEngine = NULL;
	xnetlistener* pListener = NULL;
	exnet_stream_ctx tServerCtx = { 0 };

	xrtInit();

	xrtNetEngineConfigInit(&tCfg);
	tCfg.iWorkerCount = 1;
	xrtNetListenConfigInit(&tListenCfg);
	xrtNetAddrInitAny(&tListenCfg.tBindAddr, AF_INET, 19091);

	tServerCtx.bEchoBack = TRUE;

	pEngine = xrtNetEngineCreate(&tCfg);
	xrtNetEngineStart(pEngine);
	pListener = xrtNetListenerCreate(pEngine, &tListenCfg, NULL, &gExNetStreamEvents, &tServerCtx);
	xrtNetListenerStart(pListener);

	printf("echo_server_bound = %u\n", (unsigned)pListener->tConfig.tBindAddr.iPort);
	printf("run_time_ms = 3000\n");
	xrtSleep(3000);

	xrtNetListenerStop(pListener);
	xrtNetListenerDestroy(pListener);
	xrtNetEngineStop(pEngine);
	xrtNetEngineDestroy(pEngine);
	xrtUnit();
	return 0;
}
