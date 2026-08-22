#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



typedef struct testsingledialfuture {
	xnetstream* Server;
	xatomic32 Accepted;
} testsingledialfuture;



/* 返回单头文件 Dial Future 使用的 IPv4 回环地址。 */
static xnetaddrlist* testSingleDialFutureLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)sHost;
	(void)Family;
	(void)pData;
	if ( !xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) ) {
		return NULL;
	}
	return xrtNetAddrListCreate(&Address, 1);
}



/* 接管服务端 Stream，并发布 Accept 已完成。 */
static bool testSingleDialFutureAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testsingledialfuture* pState =
		(testsingledialfuture*)pData;

	(void)pListener;
	pState->Server = pStream;
	xrtAtomic32Store(&pState->Accepted, 1, XMEMORY_RELEASE);
	return true;
}



/* 验证单头文件 Future 成功值持有一个已经 OPEN 的 Stream。 */
int main(void)
{
	testsingledialfuture State;
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	xnetstream* pClient;
	xnetaddr Address;
	xfuture* pFuture;
	xdeadline iDeadline;

	memset(&State, 0, sizeof(State));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	ListenerEvents.Accept = testSingleDialFutureAccept;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		return 1;
	}
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Lookup = testSingleDialFutureLookup;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	if ( pResolver == NULL ) {
		return 2;
	}
	xrtNetListenConfigInit(&ListenConfig);
	(void)xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	);
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		NULL,
		&State
	);
	if ( (pListener == NULL) ||
		 !xrtNetListenerLocal(pListener, &Address) ) {
		return 3;
	}
	pFuture = xrtNetDialAsync(
		pEngine,
		pResolver,
		"single-future.test",
		Address.Port,
		NULL,
		NULL,
		NULL
	);
	if ( (pFuture == NULL) ||
		 (xrtFutureWaitFor(pFuture, 3000000u) != XWAIT_OK) ||
		 (xrtFutureState(pFuture) != XFUTURE_RESOLVED) ) {
		return 4;
	}
	pClient = xrtNetStreamRef(
		(xnetstream*)xrtFutureValue(pFuture)
	);
	xrtFutureDestroy(pFuture);
	if ( (pClient == NULL) ||
		 (xrtNetStreamState(pClient) != XNET_STREAM_OPEN) ) {
		return 5;
	}
	iDeadline = xrtDeadlineAfter(3000000u);
	while ( xrtAtomic32Load(&State.Accepted, XMEMORY_ACQUIRE) == 0 ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			return 6;
		}
		xrtThreadYield();
	}
	if ( !xrtNetStreamClose(pClient) ||
		 !xrtNetStreamClose(State.Server) ) {
		return 7;
	}
	while ( (xrtNetStreamState(pClient) != XNET_STREAM_CLOSED) ||
		 (xrtNetStreamState(State.Server) != XNET_STREAM_CLOSED) ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			return 8;
		}
		xrtThreadYield();
	}
	(void)xrtNetListenerClose(pListener);
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetStreamDestroy(pClient);
	xrtNetStreamDestroy(State.Server);
	xrtNetListenerDestroy(pListener);
	if ( !xrtNetResolverDestroy(pResolver) ) {
		return 9;
	}
	return xrtNetEngineDestroy(pEngine) ? 0 : 10;
}
