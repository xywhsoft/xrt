/*
 * 范例：network/tcp_dial_sync —— 同步主机名连接
 * ----------------------------------------------------------------
 * 演示 API：
 *   阻塞式 Resolver + 连接（调用方 Engine）
 * 模块宏：XRT_MODULE_NET_TCP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/tcp_dial_sync/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   connected: yes
 *
 * 工具/脚本场景的最少代码："连一个主机名"。
 */


#include <stdio.h>
#include <string.h>
#include <xrt.h>



static xnetaddr ExampleAddress;



/* 示例解析器把固定名称映射到本地 Listener。 */
static xnetaddrlist* resolveExample(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "local.example") != 0 ) {
		return NULL;
	}
	Address = ExampleAddress;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1);
}



/* 使用调用方 Engine 和 Resolver 完成一次阻塞主机名连接。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	xnetstream* pClient;
	xnetstream* pServer;
	bool bConnected;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		return 1;
	}
	xrtNetListenConfigInit(&ListenConfig);
	(void)xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	);
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	if ( (pListener == NULL) || !xrtNetListenerLocal(
		pListener,
		&ExampleAddress
	) ) {
		return 2;
	}
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1;
	ResolverConfig.Lookup = resolveExample;
	ResolverConfig.CacheEntries = 0;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	if ( pResolver == NULL ) {
		return 3;
	}
	pClient = xrtNetConnect(
		pEngine,
		pResolver,
		"local.example",
		ExampleAddress.Port,
		NULL,
		NULL,
		NULL,
		xrtDeadlineAfter(3000000u),
		NULL
	);
	pServer = xrtNetListenerAcceptWait(
		pListener,
		xrtDeadlineAfter(3000000u),
		NULL
	);
	bConnected = (pClient != NULL) && (pServer != NULL);
	printf("connected: %s\n",
		bConnected ? "yes" : "no");
	if ( pClient != NULL ) {
		(void)xrtNetStreamAbort(pClient);
	}
	if ( pServer != NULL ) {
		(void)xrtNetStreamAbort(pServer);
	}
	(void)xrtNetListenerClose(pListener);
	while ( ((pClient != NULL) &&
		  (xrtNetStreamState(pClient) != XNET_STREAM_CLOSED)) ||
		 ((pServer != NULL) &&
		  (xrtNetStreamState(pServer) != XNET_STREAM_CLOSED)) ||
		 (xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED) ) {
		xrtThreadYield();
	}
	xrtNetStreamDestroy(pClient);
	xrtNetStreamDestroy(pServer);
	xrtNetListenerDestroy(pListener);
	(void)xrtNetResolverDestroy(pResolver);
	return xrtNetEngineDestroy(pEngine) && bConnected ? 0 : 4;
}
