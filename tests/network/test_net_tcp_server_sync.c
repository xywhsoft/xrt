#include "../test.h"



typedef struct testtcpserversyncworker {
	xnetserver* Server;
	xatomic32 Done;
	xatomic32 Rejected;
} testtcpserversyncworker;



/* 任意 Engine Worker 都不能阻塞整组 Server 的 Accept 推进。 */
static void testTcpServerSyncWorker(xnetworker* pWorker, ptr pData)
{
	testtcpserversyncworker* pContext =
		(testtcpserversyncworker*)pData;
	xnetstream* pStream;

	(void)pWorker;
	pStream = xrtNetServerAcceptWait(
		pContext->Server,
		XRT_DEADLINE_NEVER,
		NULL
	);
	if ( (pStream == NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		 (xrtErrorCode(xrtGetError()) == XNET_ERROR_SERVER_ACCEPT) ) {
		xrtAtomic32Store(
			&pContext->Rejected,
			1,
			XMEMORY_RELEASE
		);
	}
	xrtClearError();
	xrtAtomic32Store(&pContext->Done, 1, XMEMORY_RELEASE);
}



/* 等待 Worker 检查或异步关闭完成。 */
static void testTcpServerSyncWait(
	testtcpserversyncworker* pWorker,
	xnetserver* pServer,
	xnetstream* pClient,
	xnetstream* pAccepted
)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	for ( ;; ) {
		bool bWorkerDone = (pWorker == NULL) ||
			(xrtAtomic32Load(
				&pWorker->Done,
				XMEMORY_ACQUIRE
			 ) != 0);
		bool bServerDone = (pServer == NULL) ||
			(xrtNetServerState(pServer) == XNET_SERVER_CLOSED);
		bool bClientDone = (pClient == NULL) ||
			(xrtNetStreamState(pClient) == XNET_STREAM_CLOSED);
		bool bAcceptedDone = (pAccepted == NULL) ||
			(xrtNetStreamState(pAccepted) == XNET_STREAM_CLOSED);

		if ( bWorkerDone && bServerDone && bClientDone &&
			 bAcceptedDone ) {
			return;
		}
		testRequire(!xrtDeadlineExpired(iDeadline),
			"TCP server sync wait timed out");
		xrtThreadYield();
	}
}



/* 验证同步聚合 Accept 的成功、超时、取消和 Worker 防阻塞。 */
int main(void)
{
	testtcpserversyncworker Worker;
	xnetengineconfig EngineConfig;
	xnetserverconfig ServerConfig;
	xnetengine* pEngine;
	xnetserver* pServer;
	xnetstream* pClient;
	xnetstream* pAccepted;
	xcancel* pCancel;
	xnetaddr Local;

	memset(&Worker, 0, sizeof(Worker));
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP server sync engine start failed");
	xrtNetServerConfigInit(&ServerConfig);
	testRequire(xrtNetAddrLoopback(
		&ServerConfig.Listen.Address,
		XNET_FAMILY_IPV4,
		0
	), "TCP server sync loopback address failed");
	pServer = xrtNetServerStart(
		pEngine,
		&ServerConfig,
		NULL,
		NULL,
		NULL
	);
	testRequire((pServer != NULL) &&
		 xrtNetServerLocal(pServer, 0, &Local),
		"TCP server sync start failed");

	testRequire(xrtNetServerAcceptWait(
		pServer,
		xrtDeadlineAfter(1000u),
		NULL
	) == NULL, "TCP server sync Accept ignored timeout");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_TIMEOUT) &&
		 (xrtErrorCode(xrtGetError()) == XNET_ERROR_SERVER_ACCEPT),
		"TCP server sync timeout error mismatch");
	xrtClearError();
	pCancel = xrtCancelCreate();
	testRequire((pCancel != NULL) && xrtCancelRequest(pCancel),
		"TCP server sync cancel setup failed");
	testRequire(xrtNetServerAcceptWait(
		pServer,
		XRT_DEADLINE_NEVER,
		pCancel
	) == NULL, "TCP server sync Accept ignored cancellation");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_CANCELLED) &&
		 (xrtErrorCode(xrtGetError()) == XNET_ERROR_SERVER_ACCEPT),
		"TCP server sync cancellation error mismatch");
	xrtCancelDestroy(pCancel);
	xrtClearError();

	Worker.Server = pServer;
	testRequire(xrtNetEnginePost(
		pEngine,
		1,
		testTcpServerSyncWorker,
		&Worker
	), "TCP server sync Worker check post failed");
	testTcpServerSyncWait(&Worker, NULL, NULL, NULL);
	testRequire(xrtAtomic32Load(
		&Worker.Rejected,
		XMEMORY_ACQUIRE
	) != 0, "TCP server Worker blocking Accept was not rejected");

	pClient = xrtNetStreamConnect(
		pEngine,
		&Local,
		1,
		NULL,
		NULL,
		NULL
	);
	testRequire(pClient != NULL,
		"TCP server sync client connect failed");
	pAccepted = xrtNetServerAcceptWait(
		pServer,
		xrtDeadlineAfter(5000000u),
		NULL
	);
	testRequire(pAccepted != NULL,
		"TCP server sync Accept failed");

	testRequire(xrtNetServerClose(pServer),
		"TCP server sync close failed");
	(void)xrtNetStreamAbort(pAccepted);
	(void)xrtNetStreamAbort(pClient);
	testTcpServerSyncWait(NULL, pServer, pClient, pAccepted);
	xrtNetStreamDestroy(pAccepted);
	xrtNetStreamDestroy(pClient);
	xrtNetServerDestroy(pServer);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP server sync engine destroy failed");
	printf("[PASS] network TCP server sync\n");
	return 0;
}
