#include "../test.h"



#define TEST_UDP_FUTURE_OOM_LIMIT 4096u



typedef struct testudpfutureoom {
	xatomic32 Fail;
	xatomic64 Attempts;
} testudpfutureoom;



/* 正常阶段转发系统分配，故障阶段拒绝全部新内存。 */
static ptr testUdpFutureOomAlloc(ptr pData, size_t iSize)
{
	testudpfutureoom* pContext = (testudpfutureoom*)pData;

	(void)xrtAtomic64FetchAdd(&pContext->Attempts, 1, XMEMORY_RELAXED);
	return xrtAtomic32Load(&pContext->Fail, XMEMORY_ACQUIRE) != 0 ?
		NULL : malloc(iSize);
}



/* 正常阶段保持 realloc 语义，故障阶段拒绝扩容。 */
static ptr testUdpFutureOomRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	testudpfutureoom* pContext = (testudpfutureoom*)pData;

	(void)xrtAtomic64FetchAdd(&pContext->Attempts, 1, XMEMORY_RELAXED);
	return xrtAtomic32Load(&pContext->Fail, XMEMORY_ACQUIRE) != 0 ?
		NULL : realloc(pMemory, iSize);
}



/* 释放正常阶段已经取得的底层内存。 */
static void testUdpFutureOomFree(ptr pData, ptr pMemory)
{
	(void)pData;
	free(pMemory);
}



/* 等待 UDP 状态推进到目标。 */
static void testUdpFutureOomState(xnetudp* pUdp, xnetudpstate State)
{
	xdeadline iDeadline = xrtDeadlineAfter(UINT64_C(5000000));

	while ( xrtNetUdpState(pUdp) != State ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"UDP Future OOM state timed out");
		xrtThreadYield();
	}
}



/* 等待一个数据包进入拉取队列，避免故障注入测试在后端异常时永久阻塞。 */
static void testUdpFutureOomQueued(xnetudp* pUdp)
{
	xdeadline iDeadline = xrtDeadlineAfter(UINT64_C(5000000));

	while ( xrtNetUdpQueued(pUdp) == 0 ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"UDP Future OOM receive queue timed out");
		xrtThreadYield();
	}
}



/* 验证构造 OOM 不消费数据、不遗留等待节点，并能完整恢复。 */
int main(void)
{
	testudpfutureoom Context;
	xallocator Allocator;
	xnetengineconfig EngineConfig;
	xnetudpconfig UdpConfig;
	xnetudpstats Stats;
	xnetengine* pEngine;
	xnetudp* pServer;
	xnetudp* pClient;
	xnetaddr Address;
	xfuture** pPending;
	xfuture* pFailed;
	xfuture* pRecovered;
	xnetudppacket* pPacket;
	size_t iPending = 0;

	memset(&Context, 0, sizeof(Context));
	xrtAtomic32Init(&Context.Fail, 0);
	xrtAtomic64Init(&Context.Attempts, 0);
	Allocator.Context = &Context;
	Allocator.Alloc = testUdpFutureOomAlloc;
	Allocator.Realloc = testUdpFutureOomRealloc;
	Allocator.Free = testUdpFutureOomFree;
	testRequire(xrtSetAllocator(&Allocator),
		"UDP Future OOM allocator install failed");
	pPending = (xfuture**)malloc(
		TEST_UDP_FUTURE_OOM_LIMIT * sizeof(*pPending)
	);
	testRequire(pPending != NULL,
		"UDP Future OOM system allocation failed");

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"UDP Future OOM engine start failed");
	xrtNetUdpConfigInit(&UdpConfig);
	testRequire(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0),
		"UDP Future OOM address failed");
	pServer = xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&UdpConfig,
		NULL,
		NULL
	);
	testRequire((pServer != NULL) && xrtNetUdpLocal(pServer, &Address),
		"UDP Future OOM server failed");
	pClient = xrtNetUdpConnect(
		pEngine,
		&Address,
		0,
		&UdpConfig,
		NULL,
		NULL
	);
	testRequire(pClient != NULL, "UDP Future OOM client failed");
	testUdpFutureOomState(pServer, XNET_UDP_OPEN);
	testUdpFutureOomState(pClient, XNET_UDP_OPEN);

	/* 任一等待构造分配失败都必须完整回滚已建立的 Promise 和监听。 */
	xrtAtomic32Store(&Context.Fail, 1, XMEMORY_RELEASE);
	while ( iPending < TEST_UDP_FUTURE_OOM_LIMIT ) {
		xfuture* pFuture = xrtNetUdpReceiveAsync(pServer);

		if ( pFuture == NULL ) {
			break;
		}
		pPending[iPending++] = pFuture;
	}
	testRequire(iPending < TEST_UDP_FUTURE_OOM_LIMIT,
		"UDP Future creation did not reach OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"UDP Future creation OOM error mismatch");
	xrtClearError();
	xrtAtomic32Store(&Context.Fail, 0, XMEMORY_RELEASE);
	for ( size_t i = 0; i < iPending; i++ ) {
		testRequire(xrtFutureCancel(pPending[i]) &&
			 (xrtFutureWaitFor(
				pPending[i],
				UINT64_C(5000000)
			 ) == XWAIT_OK) &&
			 (xrtFutureState(pPending[i]) == XFUTURE_CANCELLED),
			"UDP Future OOM retained a malformed waiter");
		xrtFutureDestroy(pPending[i]);
	}
	testRequire(xrtNetUdpStats(pServer, &Stats) &&
		 (Stats.ReceiveWaiters == 0),
		"UDP Future OOM leaked receive waiters");

	/* 批量构造失败发生在取包前，已有报文必须完整留在队列。 */
	testRequire(xrtNetUdpSend(pClient, "Q", 1) == XNET_RESULT_OK,
		"UDP Future OOM setup send failed");
	testUdpFutureOomQueued(pServer);
	xrtAtomic32Store(&Context.Fail, 1, XMEMORY_RELEASE);
	pFailed = xrtNetUdpReceiveBatchAsync(pServer, 256);
	testRequire((pFailed == NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		 (xrtNetUdpQueued(pServer) == 1),
		"UDP batch Future OOM consumed a queued packet");
	xrtClearError();
	xrtAtomic32Store(&Context.Fail, 0, XMEMORY_RELEASE);

	pRecovered = xrtNetUdpReceiveAsync(pServer);
	testRequire((pRecovered != NULL) &&
		 (xrtFutureWaitFor(
			pRecovered,
			UINT64_C(5000000)
		 ) == XWAIT_OK) &&
		 (xrtFutureState(pRecovered) == XFUTURE_RESOLVED),
		"UDP receive Future did not recover after OOM");
	pPacket = (xnetudppacket*)xrtFutureValue(pRecovered);
	testRequire((pPacket != NULL) &&
		 (xrtNetUdpPacketSize(pPacket) == 1) &&
		 (*(xrtNetUdpPacketData(pPacket)) == 'Q') &&
		 (xrtNetUdpQueued(pServer) == 0),
		"UDP receive Future recovery packet mismatch");
	xrtFutureDestroy(pRecovered);

	testRequire(xrtNetUdpClose(pClient) && xrtNetUdpClose(pServer),
		"UDP Future OOM close failed");
	testUdpFutureOomState(pClient, XNET_UDP_CLOSED);
	testUdpFutureOomState(pServer, XNET_UDP_CLOSED);
	xrtNetUdpDestroy(pClient);
	xrtNetUdpDestroy(pServer);
	testRequire(xrtNetEngineDestroy(pEngine),
		"UDP Future OOM engine destroy failed");
	testRequire(xrtAtomic64Load(
		&Context.Attempts,
		XMEMORY_ACQUIRE
	) != 0, "UDP Future OOM allocator observed no attempts");
	free(pPending);
	printf("[PASS] network UDP Future OOM\n");
	return 0;
}
