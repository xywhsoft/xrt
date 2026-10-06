#include <xrt/detail/wait.h>
#include "../test.h"



#if !defined(TEST_TCP_SYNC_BACKEND)
	#define TEST_TCP_SYNC_BACKEND XNET_PORT_SELECT
	#define TEST_TCP_SYNC_BACKEND_NAME "select"
#endif



typedef struct testtcpsyncworker {
	xnetstream* Stream;
	xatomic32 Done;
	xatomic32 Rejected;
} testtcpsyncworker;



/* 等待 Stream 完成关闭后释放调用方引用。 */
static void testTcpSyncClose(xnetstream* pStream)
{
	if ( xrtNetStreamState(pStream) != XNET_STREAM_CLOSED ) {
		testRequire(xrtNetStreamClose(pStream),
			"TCP sync close request failed");
		testRequire(__xrtNetStreamWait(
			pStream,
			XNET_STREAM_WAIT_CLOSE,
			__xrtWaitAfter(5000),
			NULL
		), "TCP sync close wait failed");
	}
	xrtNetStreamDestroy(pStream);
}



/* Worker 不能阻塞等待只能由自身事件循环推进的 Stream。 */
static void testTcpSyncWorker(xnetworker* pWorker, ptr pData)
{
	testtcpsyncworker* pContext = (testtcpsyncworker*)pData;
	bool bResult;

	(void)pWorker;
	bResult = __xrtNetStreamWait(
		pContext->Stream,
		XNET_STREAM_WAIT_READ,
		INFINITY,
		NULL
	);
	if ( !bResult && (xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		 (xrtErrorCode(xrtGetError()) == XNET_ERROR_STREAM_READ) ) {
		xrtAtomic32Store(
			&pContext->Rejected,
			1,
			XMEMORY_RELEASE
		);
	}
	xrtClearError();
	xrtAtomic32Store(&pContext->Done, 1, XMEMORY_RELEASE);
}



/* 验证无隐藏 Engine 的 TCP 阻塞等待、Accept 和拥有型接收结果。 */
int main(void)
{
	testtcpsyncworker Worker;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetstream* pClient;
	xnetstream* pServer;
	xnetbytes* pBytes;
	xnetbytes* pHeldBytes;
	xcancel* pCancel;
	xbytesview View;
	xnetaddr Address;
	double iDeadline;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TCP_SYNC_BACKEND;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP sync engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TCP sync listener address failed");
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		NULL,
		NULL,
		NULL
	);
	testRequire((pListener != NULL) &&
		xrtNetListenerLocal(pListener, &Address),
		"TCP sync listener create failed");
	pClient = xrtNetStreamConnect(
		pEngine,
		&Address,
		1,
		NULL,
		NULL,
		NULL
	);
	testRequire(pClient != NULL, "TCP sync client create failed");
	pServer = __xrtNetListenerAcceptWait(
		pListener,
		__xrtWaitAfter(5000),
		NULL
	);
	testRequire(pServer != NULL, "TCP sync accept failed");
	testRequire(__xrtNetStreamWait(
		pClient,
		XNET_STREAM_WAIT_OPEN,
		__xrtWaitAfter(5000),
		NULL
	), "TCP sync client open failed");
	testRequire(__xrtNetStreamWait(
		pClient,
		XNET_STREAM_WAIT_WRITE,
		__xrtWaitAfter(5000),
		NULL
	), "TCP sync writable deadline wait failed");

	/* 超时和取消只撤销本次接收，不改变 Stream。 */
	testRequire(__xrtNetStreamRecv(
		pServer,
		0,
		__xrtWaitAfter(1),
		NULL
	) == NULL, "TCP sync receive unexpectedly ignored timeout");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_TIMEOUT) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_STREAM_READ) &&
		(xrtNetStreamState(pServer) == XNET_STREAM_OPEN),
		"TCP sync receive timeout error mismatch");
	xrtClearError();
	pCancel = xrtCancelCreate();
	testRequire((pCancel != NULL) && xrtCancelRequest(pCancel),
		"TCP sync cancel setup failed");
	testRequire(__xrtNetStreamRecv(
		pServer,
		0,
		INFINITY,
		pCancel
	) == NULL, "TCP sync receive ignored cancellation");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_CANCELLED) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_STREAM_READ),
		"TCP sync receive cancellation error mismatch");
	xrtCancelDestroy(pCancel);
	xrtClearError();

	/* 最小可读字节等待必须观察缓冲增长，不能被已有的不完整前缀满足。 */
	testRequire(xrtNetStreamSend(
		pClient,
		"abc",
		3
	) == XNET_RESULT_OK, "TCP available prefix send failed");
	testRequire(!__xrtNetStreamWaitAvailable(
		pServer,
		4,
		__xrtWaitAfter(1),
		NULL
	), "TCP available wait accepted an incomplete prefix");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_TIMEOUT) &&
		(xrtNetStreamState(pServer) == XNET_STREAM_OPEN),
		"TCP available wait timeout mismatch");
	xrtClearError();
	testRequire(xrtNetStreamSend(
		pClient,
		"d",
		1
	) == XNET_RESULT_OK, "TCP available suffix send failed");
	testRequire(__xrtNetStreamWaitAvailable(
		pServer,
		4,
		__xrtWaitAfter(5000),
		NULL
	), "TCP available wait did not observe buffer growth");
	pBytes = __xrtNetStreamRecv(
		pServer,
		4,
		__xrtWaitAfter(5000),
		NULL
	);
	testRequire(pBytes != NULL, "TCP available payload receive failed");
	View = xrtNetBytesView(pBytes);
	testRequire((View.Size == 4) &&
		(memcmp(View.Data, "abcd", 4) == 0),
		"TCP available payload mismatch");
	xrtNetBytesDestroy(pBytes);

	/* Worker 自等待必须在挂起事件循环前被拒绝。 */
	memset(&Worker, 0, sizeof(Worker));
	Worker.Stream = pServer;
	testRequire(xrtNetEnginePost(
		pEngine,
		xrtNetWorkerIndex(xrtNetStreamWorker(pServer)),
		testTcpSyncWorker,
		&Worker
	), "TCP sync worker rejection task failed");
	iDeadline = __xrtWaitAfter(5000);
	while ( xrtAtomic32Load(&Worker.Done, XMEMORY_ACQUIRE) == 0 ) {
		testRequire(!__xrtWaitExpired(iDeadline),
			"TCP sync worker rejection timed out");
		xrtThreadYield();
	}
	testRequire(xrtAtomic32Load(
		&Worker.Rejected,
		XMEMORY_ACQUIRE
	) != 0, "TCP sync worker self-wait was not rejected");

	/* 字节结果只复制一次，并可独立于内部 Future 保留引用。 */
	testRequire(xrtNetStreamSend(
		pClient,
		"sync-bytes",
		10
	) == XNET_RESULT_OK, "TCP sync send failed");
	pBytes = __xrtNetStreamRecv(
		pServer,
		0,
		__xrtWaitAfter(5000),
		NULL
	);
	testRequire(pBytes != NULL, "TCP sync receive failed");
	pHeldBytes = xrtNetBytesRef(pBytes);
	testRequire(pHeldBytes == pBytes,
		"TCP sync byte result retain failed");
	xrtNetBytesDestroy(pBytes);
	View = xrtNetBytesView(pHeldBytes);
	testRequire((View.Size == 10) &&
		(memcmp(View.Data, "sync-bytes", 10) == 0),
		"TCP sync byte result payload mismatch");
	xrtNetBytesDestroy(pHeldBytes);

	testTcpSyncClose(pClient);
	testTcpSyncClose(pServer);
	testRequire(xrtNetListenerClose(pListener),
		"TCP sync listener close failed");
	iDeadline = __xrtWaitAfter(5000);
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		testRequire(!__xrtWaitExpired(iDeadline),
			"TCP sync listener close timed out");
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP sync engine destroy failed");
	printf("[PASS] TCP sync facade (%s)\n", TEST_TCP_SYNC_BACKEND_NAME);
	return 0;
}
