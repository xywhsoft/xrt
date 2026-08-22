#include "../fixtures/tls_server.h"



/* 验证同步等待的超时映射和关闭终态。 */
int main(void)
{
	xtlslistenerconfig ListenerConfig;
	xnetengineconfig EngineConfig;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	xnetengine* pEngine;
	xtlslistener* pListener;
	xtlsstream* pStream;

	pContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	testRequire((pContext != NULL) && (pIdentity != NULL),
		"TLS Listener sync fixture creation failed");
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TLS Listener sync engine start failed");
	xrtTlsListenerConfigInit(&ListenerConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenerConfig.Listen.Address,
		XNET_FAMILY_IPV4,
		0
	), "TLS Listener sync address failed");
	ListenerConfig.Tls.Context = pContext;
	ListenerConfig.Tls.Identity = pIdentity;
	pListener = xrtTlsListenerStart(
		pEngine,
		&ListenerConfig,
		NULL,
		NULL,
		NULL
	);
	testRequire(pListener != NULL,
		"TLS Listener sync start failed");
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);

	pStream = xrtTlsListenerAcceptWait(
		pListener,
		xrtDeadlineAfter(UINT64_C(1000)),
		NULL
	);
	testRequire((pStream == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_TIMEOUT),
		"TLS Listener sync timeout mapping failed");
	xrtClearError();
	testRequire(xrtTlsListenerClose(pListener),
		"TLS Listener sync close failed");
	while ( xrtTlsListenerState(pListener) != XTLS_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtTlsListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TLS Listener sync engine destroy failed");
	printf("[PASS] TLS Stream Listener sync\n");
	return 0;
}
