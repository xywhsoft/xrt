#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



typedef struct testsingledial {
	xnetstream* Client;
	xnetstream* Server;
	xatomic32 Done;
} testsingledial;



/* 返回单头文件 Dial 使用的 IPv4 回环解析结果。 */
static xnetaddrlist* testSingleDialLookup(
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



/* 接管单头文件 Listener 接受的服务端 Stream。 */
static bool testSingleDialAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testsingledial* pState = (testsingledial*)pData;

	(void)pListener;
	pState->Server = pStream;
	return true;
}



/* 接管成功 Stream，并发布 Dial 唯一终态。 */
static void testSingleDialDone(
	xnetdial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	testsingledial* pState = (testsingledial*)pData;

	(void)pDial;
	if ( (Result == XNET_RESULT_OK) &&
		 (pStream != NULL) && (pError == NULL) ) {
		pState->Client = pStream;
		xrtAtomic32Store(&pState->Done, 1, XMEMORY_RELEASE);
	}
}



/* 验证单头文件能够执行 Resolver、Dial、Accept 和完整关闭。 */
int main(void)
{
	testsingledial State;
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	xnetdial* pDial;
	xnetaddr Address;
	xdeadline iDeadline;

	memset(&State, 0, sizeof(State));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	ListenerEvents.Accept = testSingleDialAccept;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		return 1;
	}
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Lookup = testSingleDialLookup;
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
	pDial = xrtNetDial(
		pEngine,
		pResolver,
		"single.test",
		Address.Port,
		NULL,
		NULL,
		NULL,
		testSingleDialDone,
		&State
	);
	if ( pDial == NULL ) {
		return 4;
	}
	iDeadline = xrtDeadlineAfter(3000000u);
	while ( (xrtAtomic32Load(&State.Done, XMEMORY_ACQUIRE) == 0) ||
		 (State.Server == NULL) ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			return 5;
		}
		xrtThreadYield();
	}
	if ( !xrtNetStreamClose(State.Client) ||
		 !xrtNetStreamClose(State.Server) ) {
		return 6;
	}
	while ( (xrtNetStreamState(State.Client) != XNET_STREAM_CLOSED) ||
		 (xrtNetStreamState(State.Server) != XNET_STREAM_CLOSED) ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			return 7;
		}
		xrtThreadYield();
	}
	(void)xrtNetListenerClose(pListener);
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetStreamDestroy(State.Client);
	xrtNetStreamDestroy(State.Server);
	xrtNetDialDestroy(pDial);
	xrtNetListenerDestroy(pListener);
	if ( !xrtNetResolverDestroy(pResolver) ) {
		return 8;
	}
	return xrtNetEngineDestroy(pEngine) ? 0 : 9;
}
