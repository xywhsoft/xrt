#include "../fixtures/tls_server.h"



/* 验证 Future 的取消、关闭和消费者互斥，不建立外部连接。 */
int main(void)
{
	xtlslistenerconfig ListenerConfig;
	xtlslistenerstats Stats;
	xnetengineconfig EngineConfig;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	xnetengine* pEngine;
	xtlslistener* pListener;
	xfuture* pCancel;
	xfuture* pClose;

	pContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	testRequire((pContext != NULL) && (pIdentity != NULL),
		"TLS Listener Future fixture creation failed");
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TLS Listener Future engine start failed");
	xrtTlsListenerConfigInit(&ListenerConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenerConfig.Listen.Address,
		XNET_FAMILY_IPV4,
		0
	), "TLS Listener Future address failed");
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
		"TLS Listener Future start failed");
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);

	pCancel = xrtTlsListenerAcceptAsync(pListener);
	testRequire((pCancel != NULL) &&
		(xrtTlsListenerAccept(pListener) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"TLS Listener mixed pull consumers were accepted");
	xrtClearError();
	testRequire(xrtFutureCancel(pCancel) &&
		(xrtFutureWaitFor(
			pCancel,
			UINT64_C(5000000)
		) == XWAIT_OK) &&
		(xrtFutureState(pCancel) == XFUTURE_CANCELLED),
		"TLS Listener Future cancellation failed");
	xrtFutureDestroy(pCancel);
	testRequire(xrtTlsListenerStats(pListener, &Stats) &&
		(Stats.AcceptWaiters == 0),
		"TLS Listener cancelled waiter leaked");

	pClose = xrtTlsListenerAcceptAsync(pListener);
	testRequire((pClose != NULL) && xrtTlsListenerClose(pListener) &&
		(xrtFutureWaitFor(
			pClose,
			UINT64_C(5000000)
		) == XWAIT_OK) &&
		(xrtFutureState(pClose) == XFUTURE_CLOSED),
		"TLS Listener close did not finish accept Future");
	xrtFutureDestroy(pClose);
	while ( xrtTlsListenerState(pListener) != XTLS_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtTlsListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TLS Listener Future engine destroy failed");
	printf("[PASS] TLS Stream Listener Future\n");
	return 0;
}
