#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



static xnetaddr testTcpDialSyncAddress;



/* 把单头测试主机解析到本地 Listener。 */
static xnetaddrlist* testSingleTcpDialSyncLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "single.test") != 0 ) {
		return NULL;
	}
	Address = testTcpDialSyncAddress;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1);
}



/* 验证单头文件包含阻塞 Resolver/TCP Dial 组合。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xnetdialconfig DialConfig;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	xnetstream* pClient;
	xnetstream* pServer;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
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
	if ( (pListener == NULL) ||
		 !xrtNetListenerLocal(pListener, &testTcpDialSyncAddress) ) {
		return 2;
	}
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1;
	ResolverConfig.CacheEntries = 0;
	ResolverConfig.Lookup = testSingleTcpDialSyncLookup;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	if ( pResolver == NULL ) {
		return 3;
	}
	xrtNetDialConfigInit(&DialConfig);
	DialConfig.Family = XNET_FAMILY_IPV4;
	pClient = xrtNetConnect(
		pEngine,
		pResolver,
		"single.test",
		testTcpDialSyncAddress.Port,
		&DialConfig,
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
	if ( (pClient == NULL) || (pServer == NULL) ) {
		return 4;
	}
	(void)xrtNetStreamAbort(pClient);
	(void)xrtNetStreamAbort(pServer);
	(void)xrtNetListenerClose(pListener);
	while ( (xrtNetStreamState(pClient) != XNET_STREAM_CLOSED) ||
		 (xrtNetStreamState(pServer) != XNET_STREAM_CLOSED) ||
		 (xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED) ) {
		xrtThreadYield();
	}
	xrtNetStreamDestroy(pClient);
	xrtNetStreamDestroy(pServer);
	xrtNetListenerDestroy(pListener);
	if ( !xrtNetResolverDestroy(pResolver) ) {
		return 5;
	}
	return xrtNetEngineDestroy(pEngine) ? 0 : 6;
}
