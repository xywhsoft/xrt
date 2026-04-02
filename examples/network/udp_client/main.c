/*
 * XRT Example - UDP Client
 * XRT 范例 - UDP 客户端
 *
 * Description / 说明:
 *   EN: Demonstrates sending a UDP packet to a loopback server.
 *   CN: 演示向本地 UDP 服务器发送一个数据报。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/network_udp_client.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/network_udp_client -lm -lpthread
 *
 * Note / 注意:
 *   - Start examples/network/udp_echo first to observe the echoed response.
 */
#include "../../../xrt.c"


int main()
{
	xnetengineconfig tCfg;
	xnetdgramconfig tClientCfg;
	xnetengine* pEngine = NULL;
	xdgramsock* pClient = NULL;
	xnetaddr tServerAddr;

	xrtInit();

	xrtNetEngineConfigInit(&tCfg);
	tCfg.iWorkerCount = 1;
	xrtNetDgramConfigInit(&tClientCfg);
	xrtNetAddrInitAny(&tClientCfg.tBindAddr, AF_INET, 0);

	pEngine = xrtNetEngineCreate(&tCfg);
	xrtNetEngineStart(pEngine);
	pClient = xrtNetDgramCreate(pEngine, &tClientCfg, NULL, NULL);
	xrtNetDgramStart(pClient);
	(void)xrtNetAddrParse(&tServerAddr, "127.0.0.1", 19093);
	(void)xrtNetDgramSendTo(pClient, &tServerAddr, "hello from udp_client", 21);
	printf("udp_send_done = TRUE\n");

	xrtNetDgramStop(pClient);
	xrtNetDgramDestroy(pClient);
	xrtNetEngineStop(pEngine);
	xrtNetEngineDestroy(pEngine);
	xrtUnit();
	return 0;
}
