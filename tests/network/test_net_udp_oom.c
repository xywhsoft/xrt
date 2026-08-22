#include "../test.h"



#if !defined(TEST_UDP_OOM_BACKEND)
	#define TEST_UDP_OOM_BACKEND XNET_PORT_SELECT
	#define TEST_UDP_OOM_BACKEND_NAME "select"
#endif



typedef struct testudpoom {
	xatomic32 Fail;
	xatomic64 Attempts;
	xatomic32 Open;
	xatomic32 Errors;
	xatomic32 Close;
	xatomic32 Released;
	xatomic32 ReenterClose;
	xatomic32 CloseReturned;
	xatomic64 ReenterThread;
	xnetudp* Udp;
} testudpoom;



/* 正常阶段转发分配，故障阶段拒绝全部新内存。 */
static ptr testUdpOomAlloc(ptr pData, size_t iSize)
{
	testudpoom* pContext = (testudpoom*)pData;

	(void)xrtAtomic64FetchAdd(&pContext->Attempts, 1, XMEMORY_RELAXED);
	if ( (xrtThreadCurrentId() == xrtAtomic64Load(
		&pContext->ReenterThread,
		XMEMORY_ACQUIRE
	)) && (xrtAtomic32Exchange(
		&pContext->ReenterClose,
		0,
		XMEMORY_ACQ_REL
	) != 0) ) {
		xrtAtomic32Store(
			&pContext->CloseReturned,
			xrtNetUdpClose(pContext->Udp) ? 1 : 2,
			XMEMORY_RELEASE
		);
	}
	return xrtAtomic32Load(&pContext->Fail, XMEMORY_ACQUIRE) != 0 ?
		NULL : malloc(iSize);
}



/* 正常阶段保持 realloc 语义，故障阶段拒绝扩容。 */
static ptr testUdpOomRealloc(ptr pData, ptr pMemory, size_t iSize)
{
	testudpoom* pContext = (testudpoom*)pData;

	(void)xrtAtomic64FetchAdd(&pContext->Attempts, 1, XMEMORY_RELAXED);
	return xrtAtomic32Load(&pContext->Fail, XMEMORY_ACQUIRE) != 0 ?
		NULL : realloc(pMemory, iSize);
}



/* 释放正常阶段已经取得的底层内存。 */
static void testUdpOomFree(ptr pData, ptr pMemory)
{
	(void)pData;
	free(pMemory);
}



/* 在截止时间前等待原子计数达到目标。 */
static void testUdpOomWait(
	xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(iDeadline), sMessage);
		xrtThreadYield();
	}
}



/* 记录 UDP 已经完成初始化。 */
static void testUdpOomOpen(xnetudp* pUdp, ptr pData)
{
	testudpoom* pContext = (testudpoom*)pData;

	(void)pUdp;
	(void)xrtAtomic32FetchAdd(&pContext->Open, 1, XMEMORY_RELEASE);
}



/* 验证拉取包分配失败被报告为可恢复内存错误。 */
static void testUdpOomError(
	xnetudp* pUdp,
	const xerror* pError,
	ptr pData
)
{
	testudpoom* pContext = (testudpoom*)pData;

	(void)pUdp;
	testRequire((pError != NULL) &&
		(xrtErrorKind(pError) == XERR_MEMORY),
		"UDP receive OOM error mismatch");
	(void)xrtAtomic32FetchAdd(&pContext->Errors, 1, XMEMORY_RELEASE);
}



/* 记录 UDP 正常关闭。 */
static void testUdpOomClose(
	xnetudp* pUdp,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testudpoom* pContext = (testudpoom*)pData;

	(void)pUdp;
	testRequire((Result == XNET_RESULT_OK) && (pError == NULL),
		"UDP OOM recovery close mismatch");
	(void)xrtAtomic32FetchAdd(&pContext->Close, 1, XMEMORY_RELEASE);
}



/* 失败受理的引用发送不得调用释放过程。 */
static void testUdpOomRelease(
	ptr pData,
	cbytes pBytes,
	size_t iSize
)
{
	testudpoom* pContext = (testudpoom*)pData;

	(void)pBytes;
	(void)iSize;
	(void)xrtAtomic32FetchAdd(&pContext->Released, 1, XMEMORY_RELEASE);
}



/* 非阻塞发送一个原始数据报，并为调度抖动保留截止时间。 */
static void testUdpOomRawSend(
	xnetsocket Socket,
	const xnetaddr* pRemote,
	const void* pData,
	size_t iSize
)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);
	xnetresult Result;
	size_t iSent;

	do {
		Result = xrtNetSocketSendTo(
			Socket,
			pData,
			iSize,
			&iSent,
			pRemote
		);
		if ( Result == XNET_RESULT_AGAIN ) {
			testRequire(!xrtDeadlineExpired(iDeadline),
				"UDP OOM raw send timed out");
			xrtThreadYield();
		}
	} while ( Result == XNET_RESULT_AGAIN );
	testRequire((Result == XNET_RESULT_OK) && (iSent == iSize),
		"UDP OOM raw send failed");
}



/* 非阻塞接收一个原始数据报，并验证其连续载荷。 */
static void testUdpOomRawReceive(
	xnetsocket Socket,
	const void* pExpected,
	size_t iExpected
)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);
	xnetaddr Remote;
	xnetresult Result;
	char sData[4096];
	size_t iReceived;

	do {
		Result = xrtNetSocketRecvFrom(
			Socket,
			sData,
			sizeof(sData),
			&iReceived,
			&Remote
		);
		if ( Result == XNET_RESULT_AGAIN ) {
			testRequire(!xrtDeadlineExpired(iDeadline),
				"UDP OOM raw receive timed out");
			xrtThreadYield();
		}
	} while ( Result == XNET_RESULT_AGAIN );
	testRequire((Result == XNET_RESULT_OK) &&
		(iReceived == iExpected) &&
		(memcmp(sData, pExpected, iExpected) == 0),
		"UDP OOM raw receive mismatch");
}



/* 验证发送与拉取接收 OOM 不破坏预算、所有权和对象可恢复性。 */
int main(void)
{
	testudpoom Context;
	xallocator Allocator;
	xnetengineconfig EngineConfig;
	xnetudpconfig UdpConfig;
	xnetudpevents Events;
	xnetudpstats Stats;
	xnetresult CopyResult;
	xnetresult VecResult;
	xnetengine* pEngine;
	xnetudp* pUdp;
	xnetudppacket* pPacket;
	xnetsocket Peer;
	xnetaddr PeerAddress;
	xnetaddr LocalAddress;
	xnetspan Spans[2];
	char* pTaken;
	char* pRejectedRef;
	char CopyPayload[2048];
	char InboundPayload[2048];
	uint64 iAttempts;

	memset(&Context, 0, sizeof(Context));
	xrtAtomic32Init(&Context.Fail, 0);
	xrtAtomic64Init(&Context.Attempts, 0);
	xrtAtomic32Init(&Context.ReenterClose, 0);
	xrtAtomic32Init(&Context.CloseReturned, 0);
	xrtAtomic64Init(&Context.ReenterThread, 0);
	Allocator.Context = &Context;
	Allocator.Alloc = testUdpOomAlloc;
	Allocator.Realloc = testUdpOomRealloc;
	Allocator.Free = testUdpOomFree;
	testRequire(xrtSetAllocator(&Allocator),
		"UDP OOM allocator install failed");

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_UDP_OOM_BACKEND;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"UDP OOM engine start failed");

	Peer = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM,
		XNET_SOCKET_NONBLOCK
	);
	testRequire((Peer != NULL) && xrtNetAddrLoopback(
		&PeerAddress,
		XNET_FAMILY_IPV4,
		0
	) && xrtNetSocketBind(Peer, &PeerAddress) &&
		xrtNetSocketLocal(Peer, &PeerAddress),
		"UDP OOM peer setup failed");

	memset(&Events, 0, sizeof(Events));
	Events.Open = testUdpOomOpen;
	Events.Error = testUdpOomError;
	Events.Close = testUdpOomClose;
	xrtNetUdpConfigInit(&UdpConfig);
	UdpConfig.SendHighWater = 4096;
	UdpConfig.SendLowWater = 0;
	UdpConfig.SendLimit = 4096;
	UdpConfig.SendPacketLimit = 2;
	pUdp = xrtNetUdpConnect(
		pEngine,
		&PeerAddress,
		0,
		&UdpConfig,
		&Events,
		&Context
	);
	testRequire((pUdp != NULL) && xrtNetUdpLocal(pUdp, &LocalAddress),
		"UDP OOM object create failed");
	Context.Udp = pUdp;
	testUdpOomWait(&Context.Open, 1, "UDP OOM open callback missing");

	/* 先取得调用方所有权内存，再进入确定性故障阶段。 */
	pTaken = (char*)xrtMalloc(4097);
	pRejectedRef = (char*)xrtMalloc(4097);
	testRequire((pTaken != NULL) && (pRejectedRef != NULL),
		"UDP OOM payload allocation failed");
	memset(CopyPayload, 'C', sizeof(CopyPayload));
	memset(InboundPayload, 'I', sizeof(InboundPayload));
	Spans[0] = (xnetspan){
		(cbytes)CopyPayload,
		sizeof(CopyPayload) / 2
	};
	Spans[1] = (xnetspan){
		(cbytes)CopyPayload + Spans[0].Size,
		sizeof(CopyPayload) - Spans[0].Size
	};

	/* 预热后，跨线程小包应同时复用发送节点和 Engine 命令节点。 */
	testRequire(xrtNetUdpSend(pUdp, "W", 1) == XNET_RESULT_OK,
		"UDP small-node cache warmup failed");
	testUdpOomRawReceive(Peer, "W", 1);
	iAttempts = xrtAtomic64Load(&Context.Attempts, XMEMORY_ACQUIRE);
	xrtAtomic32Store(&Context.Fail, 1, XMEMORY_RELEASE);
	testRequire(
		(xrtNetUdpSend(pUdp, "N", 1) == XNET_RESULT_OK) &&
		(xrtAtomic64Load(&Context.Attempts, XMEMORY_ACQUIRE) == iAttempts),
		"UDP cached small send attempted global allocation"
	);
	xrtAtomic32Store(&Context.Fail, 0, XMEMORY_RELEASE);
	testUdpOomRawReceive(Peer, "N", 1);

	xrtAtomic32Store(&Context.Fail, 1, XMEMORY_RELEASE);
	CopyResult = xrtNetUdpSend(
		pUdp,
		CopyPayload,
		sizeof(CopyPayload)
	);
	VecResult = xrtNetUdpSendVec(pUdp, Spans, 2);
	(void)xrtNetUdpStats(pUdp, &Stats);
	if ( (CopyResult != XNET_RESULT_ERROR) ||
		 (VecResult != XNET_RESULT_ERROR) ) {
		fprintf(
			stderr,
			"UDP OOM send results copy=%d vec=%d queued=%zu packets=%zu "
			"rejected=%llu\n",
			(int)CopyResult,
			(int)VecResult,
			Stats.QueuedBytes,
			Stats.QueuedPackets,
			(unsigned long long)Stats.SendRejected
		);
	}
	testRequire(CopyResult == XNET_RESULT_ERROR,
		"UDP copy send survived OOM");
	testRequire(VecResult == XNET_RESULT_ERROR,
		"UDP vector send survived OOM");
	testRequire(xrtNetUdpSendRef(
		pUdp,
		pRejectedRef,
		4097,
		testUdpOomRelease,
		&Context
	) == XNET_RESULT_AGAIN,
		"UDP reference send exceeded its hard limit");
	testRequire(xrtNetUdpSendTake(pUdp, pTaken, 4097) == XNET_RESULT_AGAIN,
		"UDP taken send exceeded its hard limit");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		xrtNetUdpStats(pUdp, &Stats) &&
		(Stats.QueuedBytes == 0) &&
		(Stats.QueuedPackets == 0) &&
		(Stats.SendRejected == 2) &&
		(xrtAtomic32Load(&Context.Released, XMEMORY_ACQUIRE) == 0) &&
		(xrtNetUdpState(pUdp) == XNET_UDP_OPEN),
		"UDP send OOM changed budget, ownership, or state");
	xrtClearError();

	/* 拉取包分配失败必须丢弃单包、报告错误并继续接收。 */
	testUdpOomRawSend(
		Peer,
		&LocalAddress,
		InboundPayload,
		sizeof(InboundPayload)
	);
	testUdpOomWait(&Context.Errors, 1,
		"UDP receive OOM error callback missing");
	testRequire(xrtNetUdpStats(pUdp, &Stats) &&
		(Stats.ReceivedPackets == 1) &&
		(Stats.DroppedNewest == 1) &&
		(Stats.ReceiveErrors == 1) &&
		(Stats.ReceiveQueued == 0) &&
		(xrtNetUdpState(pUdp) == XNET_UDP_OPEN),
		"UDP receive OOM stats or state mismatch");

	/* 恢复分配后，同一个 UDP 必须恢复双向收发。 */
	xrtAtomic32Store(&Context.Fail, 0, XMEMORY_RELEASE);
	xrtFree(pTaken);
	xrtFree(pRejectedRef);
	testUdpOomRawSend(Peer, &LocalAddress, "G", 1);
	{
		xdeadline iDeadline = xrtDeadlineAfter(5000000u);

		while ( xrtNetUdpQueued(pUdp) == 0 ) {
			testRequire(!xrtDeadlineExpired(iDeadline),
				"UDP receive did not recover after OOM");
			xrtThreadYield();
		}
	}
	pPacket = xrtNetUdpReceive(pUdp);
	testRequire((pPacket != NULL) &&
		(xrtNetUdpPacketSize(pPacket) == 1) &&
		(*(xrtNetUdpPacketData(pPacket)) == 'G'),
		"UDP recovered packet mismatch");
	xrtNetUdpPacketDestroy(pPacket);
	testRequire(xrtNetUdpSend(pUdp, "R", 1) == XNET_RESULT_OK,
		"UDP send did not recover after OOM");
	testUdpOomRawReceive(Peer, "R", 1);

	/* 分配器重入生命周期入口不得等待当前发送提交者。 */
	xrtAtomic64Store(
		&Context.ReenterThread,
		xrtThreadCurrentId(),
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(&Context.ReenterClose, 1, XMEMORY_RELEASE);
	testRequire(xrtNetUdpSend(
		pUdp,
		CopyPayload,
		sizeof(CopyPayload)
	) == XNET_RESULT_OK, "UDP reentrant close rejected prior send");
	testRequire(xrtAtomic32Load(
		&Context.CloseReturned,
		XMEMORY_ACQUIRE
	) == 1, "UDP reentrant close did not return from allocator");
	testUdpOomRawReceive(Peer, CopyPayload, sizeof(CopyPayload));

	testRequire(xrtNetUdpClose(pUdp),
		"UDP reentrant close idempotence failed");
	testUdpOomWait(&Context.Close, 1,
		"UDP OOM recovery close callback missing");
	testRequire(xrtNetUdpStats(pUdp, &Stats) &&
		(Stats.QueuedBytes == 0) && (Stats.QueuedPackets == 0),
		"UDP OOM close retained send budget");
	xrtNetUdpDestroy(pUdp);
	testRequire(xrtNetSocketClose(Peer) &&
		xrtNetEngineDestroy(pEngine),
		"UDP OOM cleanup failed");
	testRequire(xrtAtomic64Load(
		&Context.Attempts,
		XMEMORY_ACQUIRE
	) != 0, "UDP OOM allocator observed no attempts");
	printf("[PASS] network UDP OOM (%s)\n",
		TEST_UDP_OOM_BACKEND_NAME);
	return 0;
}
