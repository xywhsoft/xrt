#include "../test.h"



#if !defined(TEST_UDP_BACKEND)
	#define TEST_UDP_BACKEND XNET_PORT_SELECT
	#define TEST_UDP_BACKEND_NAME "select"
#endif

#if !defined(TEST_UDP_COMPLETION)
	#define TEST_UDP_COMPLETION 0
#endif



typedef struct testudpedge {
	xatomic32 Open;
	xatomic32 ReleaseOpen;
	xatomic32 Errors;
	xatomic32 Released;
	xatomic32 High;
	xatomic32 Low;
	xatomic32 Drain;
	xatomic32 Close;
	xatomic32 AbortOnRelease;
	xatomic32 AbortReturned;
	xnetudp* Udp;
	bool ExpectAbort;
} testudpedge;



/* 等待一个原子计数达到目标。 */
static void testUdpEdgeWait(
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



/* 等待 UDP 关闭。 */
static void testUdpEdgeWaitClosed(xnetudp* pUdp)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	while ( xrtNetUdpState(pUdp) != XNET_UDP_CLOSED ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"UDP edge close timed out");
		xrtThreadYield();
	}
}



/* 等待拉取队列达到目标包数。 */
static void testUdpEdgeWaitQueued(xnetudp* pUdp, size_t iExpected)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	while ( xrtNetUdpQueued(pUdp) < iExpected ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"UDP edge queue timed out");
		xrtThreadYield();
	}
}



/* 记录可恢复错误。 */
static void testUdpEdgeError(
	xnetudp* pUdp,
	const xerror* pError,
	ptr pData
)
{
	testudpedge* pEdge = (testudpedge*)pData;

	(void)pUdp;
	testRequire(pError != NULL, "UDP edge error callback lost error");
	(void)xrtAtomic32FetchAdd(&pEdge->Errors, 1, XMEMORY_RELEASE);
}



/* 阻塞 Open 回调，制造确定性的尚未挂入 Worker 的发送队列。 */
static void testUdpEdgeOpen(xnetudp* pUdp, ptr pData)
{
	testudpedge* pEdge = (testudpedge*)pData;

	(void)pUdp;
	(void)xrtAtomic32FetchAdd(&pEdge->Open, 1, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(
		&pEdge->ReleaseOpen,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
}



/* 记录引用发送释放。 */
static void testUdpEdgeRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	testudpedge* pEdge = (testudpedge*)pContext;

	(void)pData;
	(void)iSize;
	(void)xrtAtomic32FetchAdd(&pEdge->Released, 1, XMEMORY_RELEASE);
	if ( xrtAtomic32Exchange(
		&pEdge->AbortOnRelease,
		0,
		XMEMORY_ACQ_REL
	) != 0 ) {
		xrtAtomic32Store(
			&pEdge->AbortReturned,
			xrtNetUdpAbort(pEdge->Udp) ? 1 : 2,
			XMEMORY_RELEASE
		);
	}
}



/* 记录发送背压边沿。 */
static void testUdpEdgeHigh(
	xnetudp* pUdp,
	size_t iBytes,
	size_t iPackets,
	ptr pData
)
{
	testudpedge* pEdge = (testudpedge*)pData;

	(void)pUdp;
	testRequire((iBytes == 4) && (iPackets == 2),
		"UDP high-water snapshot mismatch");
	(void)xrtAtomic32FetchAdd(&pEdge->High, 1, XMEMORY_RELEASE);
}



/* 记录发送低水位。 */
static void testUdpEdgeLow(
	xnetudp* pUdp,
	size_t iBytes,
	size_t iPackets,
	ptr pData
)
{
	testudpedge* pEdge = (testudpedge*)pData;

	(void)pUdp;
	testRequire((iBytes == 0) && (iPackets == 0),
		"UDP low-water snapshot mismatch");
	(void)xrtAtomic32FetchAdd(&pEdge->Low, 1, XMEMORY_RELEASE);
}



#if !TEST_UDP_COMPLETION

/* readiness 批次的首个低水位回调中止时，后续已发送项仍必须终结。 */
static void testUdpEdgeBatchLow(
	xnetudp* pUdp,
	size_t iBytes,
	size_t iPackets,
	ptr pData
)
{
	testudpedge* pEdge = (testudpedge*)pData;

	testRequire((iBytes == 1) && (iPackets == 1),
		"UDP batch low-water snapshot mismatch");
	(void)xrtAtomic32FetchAdd(&pEdge->Low, 1, XMEMORY_RELEASE);
	xrtAtomic32Store(
		&pEdge->AbortReturned,
		xrtNetUdpAbort(pUdp) ? 1 : 2,
		XMEMORY_RELEASE
	);
}

#endif



/* 记录发送队列排空。 */
static void testUdpEdgeDrain(xnetudp* pUdp, ptr pData)
{
	testudpedge* pEdge = (testudpedge*)pData;

	(void)pUdp;
	(void)xrtAtomic32FetchAdd(&pEdge->Drain, 1, XMEMORY_RELEASE);
}



/* 记录正常关闭。 */
static void testUdpEdgeClose(
	xnetudp* pUdp,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testudpedge* pEdge = (testudpedge*)pData;
	xnetresult Expected = pEdge->ExpectAbort ?
		XNET_RESULT_CANCELLED : XNET_RESULT_OK;

	(void)pUdp;
	testRequire((Result == Expected) && (pError == NULL),
		"UDP edge close result mismatch");
	(void)xrtAtomic32FetchAdd(&pEdge->Close, 1, XMEMORY_RELEASE);
}



/* 从原始 Socket 发送一个数据报。 */
static void testUdpEdgeRawSend(
	xnetsocket Sender,
	const xnetaddr* pRemote,
	const void* pData,
	size_t iSize
)
{
	size_t iSent = 0;

	testRequire((xrtNetSocketSendTo(
		Sender,
		pData,
		iSize,
		&iSent,
		pRemote
	) == XNET_RESULT_OK) && (iSent == iSize),
		"raw UDP edge send failed");
}



/* 验证截断投递与截断错误丢弃。 */
static void testUdpTruncation(
	xnetengine* pEngine,
	xnetsocket Sender
)
{
	testudpedge Edge;
	xnetudpconfig Config;
	xnetudpevents Events;
	xnetudpstats Stats;
	xnetudp* pUdp;
	xnetudppacket* pPacket;
	xnetaddr Address;

	memset(&Edge, 0, sizeof(Edge));
	memset(&Events, 0, sizeof(Events));
	Events.Error = testUdpEdgeError;
	xrtNetUdpConfigInit(&Config);
	Config.ReceiveSize = 4;
	Config.Truncation = XNET_UDP_TRUNCATE_DELIVER;
	testRequire(xrtNetAddrLoopback(
		&Address,
		XNET_FAMILY_IPV4,
		0
	), "UDP truncation address failed");
	pUdp = xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&Config,
		&Events,
		&Edge
	);
	testRequire((pUdp != NULL) && xrtNetUdpLocal(pUdp, &Address),
		"UDP truncation bind failed");
	while ( xrtNetUdpState(pUdp) != XNET_UDP_OPEN ) {
		xrtThreadYield();
	}
	testUdpEdgeRawSend(Sender, &Address, "12345678", 8);
	pPacket = NULL;
	{
		xdeadline iDeadline = xrtDeadlineAfter(5000000u);

		while ( pPacket == NULL ) {
			pPacket = xrtNetUdpReceive(pUdp);
			testRequire(!xrtDeadlineExpired(iDeadline),
				"UDP truncated delivery timed out");
			xrtThreadYield();
		}
	}
	testRequire((xrtNetUdpPacketSize(pPacket) == 4) &&
		 xrtNetUdpPacketTruncated(pPacket) &&
		 (memcmp(xrtNetUdpPacketData(pPacket), "1234", 4) == 0),
		"UDP truncated prefix mismatch");
	xrtNetUdpPacketDestroy(pPacket);
	testRequire(xrtNetUdpStats(pUdp, &Stats) &&
		 (Stats.Truncated == 1) && (Stats.TruncatedDropped == 0),
		"UDP truncated delivery stats mismatch");
	testRequire(xrtNetUdpClose(pUdp), "UDP truncation close failed");
	testUdpEdgeWaitClosed(pUdp);
	xrtNetUdpDestroy(pUdp);

	Config.Truncation = XNET_UDP_TRUNCATE_ERROR;
	Address.Port = 0;
	pUdp = xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&Config,
		&Events,
		&Edge
	);
	testRequire((pUdp != NULL) && xrtNetUdpLocal(pUdp, &Address),
		"UDP truncation error bind failed");
	while ( xrtNetUdpState(pUdp) != XNET_UDP_OPEN ) {
		xrtThreadYield();
	}
	testUdpEdgeRawSend(Sender, &Address, "abcdefgh", 8);
	testUdpEdgeWait(&Edge.Errors, 1, "UDP truncation error missing");
	testRequire(xrtNetUdpStats(pUdp, &Stats) &&
		 (Stats.Truncated == 1) && (Stats.TruncatedDropped == 1) &&
		 (Stats.ReceiveQueued == 0) && (Stats.ReceiveErrors == 1),
		"UDP truncation error stats mismatch");
	testRequire(xrtNetUdpClose(pUdp),
		"UDP truncation error close failed");
	testUdpEdgeWaitClosed(pUdp);
	xrtNetUdpDestroy(pUdp);
}



/* 验证一种拉取队列溢出策略。 */
static void testUdpOverflow(
	xnetengine* pEngine,
	xnetsocket Sender,
	xnetudpoverflow Overflow,
	char iFirst,
	uint64 iNewest,
	uint64 iOldest,
	uint32 iErrors
)
{
	testudpedge Edge;
	xnetudpconfig Config;
	xnetudpevents Events;
	xnetudpstats Stats;
	xnetudp* pUdp;
	xnetudppacket* Packets[2];
	xnetaddr Address;
	size_t iCount;

	memset(&Edge, 0, sizeof(Edge));
	memset(&Events, 0, sizeof(Events));
	Events.Error = testUdpEdgeError;
	xrtNetUdpConfigInit(&Config);
	Config.ReceiveQueueLimit = 2;
	Config.Overflow = Overflow;
	testRequire(xrtNetAddrLoopback(
		&Address,
		XNET_FAMILY_IPV4,
		0
	), "UDP overflow address failed");
	pUdp = xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&Config,
		&Events,
		&Edge
	);
	testRequire((pUdp != NULL) && xrtNetUdpLocal(pUdp, &Address),
		"UDP overflow bind failed");
	while ( xrtNetUdpState(pUdp) != XNET_UDP_OPEN ) {
		xrtThreadYield();
	}
	testUdpEdgeRawSend(Sender, &Address, "1", 1);
	testUdpEdgeRawSend(Sender, &Address, "2", 1);
	testUdpEdgeRawSend(Sender, &Address, "3", 1);
	{
		xdeadline iDeadline = xrtDeadlineAfter(5000000u);

		for ( ;; ) {
			(void)xrtNetUdpStats(pUdp, &Stats);
			if ( Stats.ReceivedPackets == 3 ) {
				break;
			}
			testRequire(!xrtDeadlineExpired(iDeadline),
				"UDP overflow receive timed out");
			xrtThreadYield();
		}
	}
	testUdpEdgeWaitQueued(pUdp, 2);
	{
		xdeadline iDeadline = xrtDeadlineAfter(5000000u);

		for ( ;; ) {
			(void)xrtNetUdpStats(pUdp, &Stats);
			if ( (Stats.DroppedNewest == iNewest) &&
				 (Stats.DroppedOldest == iOldest) &&
				 (Stats.ReceiveErrors == iErrors) &&
				 (xrtAtomic32Load(
					&Edge.Errors,
					XMEMORY_ACQUIRE
				 ) == iErrors) ) {
				break;
			}
			testRequire(!xrtDeadlineExpired(iDeadline),
				"UDP overflow policy timed out");
			xrtThreadYield();
		}
	}
	iCount = xrtNetUdpReceiveBatch(pUdp, Packets, 2);
	testRequire((iCount == 2) &&
		 (*(xrtNetUdpPacketData(Packets[0])) == (uint8)iFirst) &&
		 (*(xrtNetUdpPacketData(Packets[1])) == (uint8)(iFirst + 1)),
		"UDP overflow retained packets mismatch");
	for ( size_t i = 0; i < iCount; i++ ) {
		xrtNetUdpPacketDestroy(Packets[i]);
	}
	testRequire(xrtNetUdpStats(pUdp, &Stats),
		"UDP overflow stats unavailable");
	if ( (Stats.DroppedNewest != iNewest) ||
		 (Stats.DroppedOldest != iOldest) ||
		 (Stats.ReceiveErrors != iErrors) ||
		 (xrtAtomic32Load(&Edge.Errors, XMEMORY_ACQUIRE) != iErrors) ) {
		fprintf(
			stderr,
			"overflow=%d newest=%llu/%llu oldest=%llu/%llu "
			"errors=%llu/%u callbacks=%u\n",
			(int)Overflow,
			(unsigned long long)Stats.DroppedNewest,
			(unsigned long long)iNewest,
			(unsigned long long)Stats.DroppedOldest,
			(unsigned long long)iOldest,
			(unsigned long long)Stats.ReceiveErrors,
			(unsigned)iErrors,
			(unsigned)xrtAtomic32Load(&Edge.Errors, XMEMORY_ACQUIRE)
		);
		testRequire(false, "UDP overflow stats mismatch");
	}
	testRequire(xrtNetUdpClose(pUdp), "UDP overflow close failed");
	testUdpEdgeWaitClosed(pUdp);
	xrtNetUdpDestroy(pUdp);
}



/* 验证接收字节上限、多包淘汰和不可容纳新包的保留策略。 */
static void testUdpReceiveByteLimit(
	xnetengine* pEngine,
	xnetsocket Sender
)
{
	testudpedge Edge;
	xnetudpconfig Config;
	xnetudpstats Stats;
	xnetudp* pUdp;
	xnetudppacket* pPacket;
	xnetaddr Address;
	xdeadline iDeadline;

	memset(&Edge, 0, sizeof(Edge));
	xrtNetUdpConfigInit(&Config);
	Config.ReceiveQueueLimit = 4;
	Config.ReceiveQueueByteLimit = 4;
	Config.Overflow = XNET_UDP_DROP_OLDEST;
	testRequire(xrtNetAddrLoopback(
		&Address,
		XNET_FAMILY_IPV4,
		0
	), "UDP receive byte limit address failed");
	pUdp = xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&Config,
		NULL,
		&Edge
	);
	testRequire((pUdp != NULL) && xrtNetUdpLocal(pUdp, &Address),
		"UDP receive byte limit bind failed");
	while ( xrtNetUdpState(pUdp) != XNET_UDP_OPEN ) {
		xrtThreadYield();
	}
	testUdpEdgeRawSend(Sender, &Address, "aa", 2);
	testUdpEdgeRawSend(Sender, &Address, "bb", 2);
	testUdpEdgeRawSend(Sender, &Address, "cccc", 4);
	testUdpEdgeRawSend(Sender, &Address, "12345", 5);
	iDeadline = xrtDeadlineAfter(5000000u);
	for ( ;; ) {
		testRequire(xrtNetUdpStats(pUdp, &Stats),
			"UDP receive byte limit stats failed");
		if ( (Stats.ReceivedPackets == 4) &&
			(Stats.DroppedOldest == 2) &&
			(Stats.DroppedNewest == 1) &&
			(Stats.ReceiveQueued == 1) &&
			(Stats.ReceiveQueuedBytes == 4) ) {
			break;
		}
		testRequire(!xrtDeadlineExpired(iDeadline),
			"UDP receive byte limit timed out");
		xrtThreadYield();
	}
	testRequire((Stats.DroppedOldest == 2) &&
		(Stats.DroppedNewest == 1) &&
		(Stats.ReceiveQueued == 1) &&
		(Stats.ReceiveQueuedBytes == 4) &&
		(Stats.PeakReceiveQueuedBytes == 4) &&
		(xrtNetUdpQueuedBytes(pUdp) == 4),
		"UDP receive byte limit accounting mismatch");
	pPacket = xrtNetUdpReceive(pUdp);
	testRequire((pPacket != NULL) &&
		(xrtNetUdpPacketSize(pPacket) == 4) &&
		(memcmp(xrtNetUdpPacketData(pPacket), "cccc", 4) == 0) &&
		(xrtNetUdpQueued(pUdp) == 0) &&
		(xrtNetUdpQueuedBytes(pUdp) == 0),
		"UDP receive byte limit retained packet mismatch");
	xrtNetUdpPacketDestroy(pPacket);
	testRequire(xrtNetUdpClose(pUdp),
		"UDP receive byte limit close failed");
	testUdpEdgeWaitClosed(pUdp);
	xrtNetUdpDestroy(pUdp);
}



/* 验证发送释放回调中的 Abort 立即封闭后续队列。 */
static void testUdpReleaseAbort(
	xnetengine* pEngine,
	xnetsocket Receiver,
	const xnetaddr* pReceiverAddress,
	uint32 iConcurrency,
	uint32 iExpected
)
{
	testudpedge Edge;
	xnetudpconfig Config;
	xnetudpevents Events;
	xnetudpstats Stats;
	xnetudp* pUdp;
	xnetaddr Remote;
	xnetresult Result;
	char iByte = 0;
	size_t iReceived = 0;
	uint32 iSeen = 0;

	memset(&Edge, 0, sizeof(Edge));
	memset(&Events, 0, sizeof(Events));
	Edge.ExpectAbort = true;
	Events.Open = testUdpEdgeOpen;
	Events.Close = testUdpEdgeClose;
	xrtNetUdpConfigInit(&Config);
	Config.SendConcurrency = iConcurrency;
	pUdp = xrtNetUdpConnect(
		pEngine,
		pReceiverAddress,
		0,
		&Config,
		&Events,
		&Edge
	);
	testRequire(pUdp != NULL, "UDP release Abort open failed");
	Edge.Udp = pUdp;
	testUdpEdgeWait(&Edge.Open, 1,
		"UDP release Abort Open missing");
	testRequire(xrtNetUdpSendRef(
		pUdp,
		"1",
		1,
		testUdpEdgeRelease,
		&Edge
	) == XNET_RESULT_OK, "UDP release Abort first send failed");
	testRequire(xrtNetUdpSendRef(
		pUdp,
		"2",
		1,
		testUdpEdgeRelease,
		&Edge
	) == XNET_RESULT_OK, "UDP release Abort second send failed");
	xrtAtomic32Store(&Edge.AbortOnRelease, 1, XMEMORY_RELEASE);
	xrtAtomic32Store(&Edge.ReleaseOpen, 1, XMEMORY_RELEASE);
	testUdpEdgeWait(&Edge.AbortReturned, 1,
		"UDP release callback Abort did not return");
	testUdpEdgeWait(&Edge.Released, 2,
		"UDP release callback Abort lost ownership");
	testUdpEdgeWait(&Edge.Close, 1,
		"UDP release callback Abort did not close");
	testRequire(xrtAtomic32Load(
		&Edge.AbortReturned,
		XMEMORY_ACQUIRE
	) == 1, "UDP release callback Abort failed");
	for ( uint32 i = 0; i < iExpected; i++ ) {
		xdeadline iDeadline = xrtDeadlineAfter(5000000u);

		do {
			Result = xrtNetSocketRecvFrom(
				Receiver,
				&iByte,
				1,
				&iReceived,
				&Remote
			);
			if ( Result == XNET_RESULT_AGAIN ) {
				testRequire(!xrtDeadlineExpired(iDeadline),
					"UDP release callback datagram timed out");
				xrtThreadYield();
			}
		} while ( Result == XNET_RESULT_AGAIN );
		testRequire((Result == XNET_RESULT_OK) && (iReceived == 1) &&
			(iByte >= '1') && (iByte <= '2'),
			"UDP release callback datagram mismatch");
		iSeen |= 1u << (uint32)(iByte - '1');
	}
	testRequire(iSeen == ((1u << iExpected) - 1u),
		"UDP release callback lost an already submitted datagram");
	Result = xrtNetSocketRecvFrom(
		Receiver,
		&iByte,
		1,
		&iReceived,
		&Remote
	);
	testRequire((Result == XNET_RESULT_AGAIN) &&
		xrtNetUdpStats(pUdp, &Stats) &&
		(Stats.SentPackets == iExpected) &&
		(Stats.QueuedPackets == 0) &&
		(Stats.QueuedBytes == 0),
		"UDP release callback Abort sent a later datagram");
	xrtNetUdpDestroy(pUdp);
}



#if !TEST_UDP_COMPLETION

/* 验证 readiness 原生批次在回调中止后仍终结全部已发送前缀。 */
static void testUdpBatchAbort(
	xnetengine* pEngine,
	xnetsocket Receiver,
	const xnetaddr* pReceiverAddress
)
{
	testudpedge Edge;
	xnetudpconfig Config;
	xnetudpevents Events;
	xnetudpstats Stats;
	xnetdgramsend Sends[2];
	xnetudp* pUdp;
	xnetaddr Remote;
	xnetresult Result;
	size_t iAccepted = 0;
	size_t iReceived = 0;
	uint32 iSeen = 0;
	char iByte = 0;

	memset(&Edge, 0, sizeof(Edge));
	memset(&Events, 0, sizeof(Events));
	Edge.ExpectAbort = true;
	Events.Open = testUdpEdgeOpen;
	Events.LowWater = testUdpEdgeBatchLow;
	Events.Close = testUdpEdgeClose;
	xrtNetUdpConfigInit(&Config);
	Config.SendConcurrency = 2;
	Config.SendHighWater = 2;
	Config.SendLowWater = 1;
	pUdp = xrtNetUdpConnect(
		pEngine,
		pReceiverAddress,
		0,
		&Config,
		&Events,
		&Edge
	);
	testRequire(pUdp != NULL, "UDP batch Abort open failed");
	Edge.Udp = pUdp;
	testUdpEdgeWait(&Edge.Open, 1, "UDP batch Abort Open missing");
	Sends[0] = (xnetdgramsend){ NULL, "1", 1 };
	Sends[1] = (xnetdgramsend){ NULL, "2", 1 };
	testRequire((xrtNetUdpSendBatch(
		pUdp,
		Sends,
		2,
		&iAccepted
	) == XNET_RESULT_OK) && (iAccepted == 2),
		"UDP batch Abort submit failed");
	xrtAtomic32Store(&Edge.ReleaseOpen, 1, XMEMORY_RELEASE);
	testUdpEdgeWait(&Edge.AbortReturned, 1,
		"UDP batch low-water Abort did not return");
	testUdpEdgeWait(&Edge.Low, 1, "UDP batch low-water event missing");
	testUdpEdgeWait(&Edge.Close, 1, "UDP batch Abort did not close");
	testRequire(xrtAtomic32Load(
		&Edge.AbortReturned,
		XMEMORY_ACQUIRE
	) == 1, "UDP batch low-water Abort failed");

	for ( uint32 i = 0; i < 2; i++ ) {
		xdeadline iDeadline = xrtDeadlineAfter(5000000u);

		do {
			Result = xrtNetSocketRecvFrom(
				Receiver,
				&iByte,
				1,
				&iReceived,
				&Remote
			);
			if ( Result == XNET_RESULT_AGAIN ) {
				testRequire(!xrtDeadlineExpired(iDeadline),
					"UDP batch Abort datagram timed out");
				xrtThreadYield();
			}
		} while ( Result == XNET_RESULT_AGAIN );
		testRequire((Result == XNET_RESULT_OK) && (iReceived == 1) &&
			(iByte >= '1') && (iByte <= '2'),
			"UDP batch Abort datagram mismatch");
		iSeen |= 1u << (uint32)(iByte - '1');
	}
	testRequire((iSeen == 3) && xrtNetUdpStats(pUdp, &Stats) &&
		(Stats.SentPackets == 2) && (Stats.QueuedPackets == 0) &&
		(Stats.QueuedBytes == 0),
		"UDP batch Abort lost an already sent item");
	xrtNetUdpDestroy(pUdp);
}

#endif



/* 验证硬发送预算和关闭排空尚未挂入 Worker 的命令。 */
static void testUdpSendBudget(
	xnetengine* pEngine,
	xnetsocket Receiver,
	const xnetaddr* pReceiverAddress
)
{
	testudpedge Edge;
	xnetudpconfig Config;
	xnetudpevents Events;
	xnetudpstats Stats;
	xnetudp* pUdp;
	xnetaddr Remote;
	xnetaddr Local;
	xnetdgramcontrol Control;
	char sData[4];
	size_t iReceived = 0;

	memset(&Edge, 0, sizeof(Edge));
	memset(&Events, 0, sizeof(Events));
	Events.Open = testUdpEdgeOpen;
	Events.HighWater = testUdpEdgeHigh;
	Events.LowWater = testUdpEdgeLow;
	Events.Drain = testUdpEdgeDrain;
	Events.Close = testUdpEdgeClose;
	xrtNetUdpConfigInit(&Config);
	Config.SendHighWater = 4;
	Config.SendLowWater = 0;
	Config.SendLimit = 4;
	Config.SendPacketLimit = 2;
	pUdp = xrtNetUdpConnect(
		pEngine,
		pReceiverAddress,
		0,
		&Config,
		&Events,
		&Edge
	);
	testRequire(pUdp != NULL, "UDP send budget open failed");
	testUdpEdgeWait(&Edge.Open, 1, "UDP send budget Open missing");
	testRequire(xrtNetUdpLocal(pUdp, &Local) &&
		!xrtNetAddrIsUnspecified(&Local) &&
		((xrtNetUdpSendControlAvailable(pUdp) &
		 XNET_DGRAM_CONTROL_SOURCE) != 0),
		"UDP send budget source control is unavailable");
	memset(&Control, 0, sizeof(Control));
	Control.Flags = XNET_DGRAM_CONTROL_SOURCE;
	Control.Source = Local;
	Control.Source.Port = 0;
	testRequire(xrtNetUdpSend(pUdp, "aa", 2) == XNET_RESULT_OK,
		"UDP send budget first send failed");
	testRequire(xrtNetUdpSendMsgRef(
		pUdp,
		NULL,
		&Control,
		"bb",
		2,
		testUdpEdgeRelease,
		&Edge
	) == XNET_RESULT_OK, "UDP send budget reference failed");
	testRequire(xrtNetUdpSendMsgRef(
		pUdp,
		NULL,
		&Control,
		"c",
		1,
		testUdpEdgeRelease,
		&Edge
	) == XNET_RESULT_AGAIN, "UDP packet hard limit was not enforced");
	testRequire(xrtNetAddrAny(
		&Control.Source,
		XNET_FAMILY_IPV4,
		0
	), "UDP caller control mutation failed");
	testRequire((xrtNetUdpPending(pUdp) == 4) &&
		 xrtNetUdpStats(pUdp, &Stats) &&
		 (Stats.QueuedPackets == 2) && (Stats.SendRejected == 1) &&
		 (xrtAtomic32Load(&Edge.Released, XMEMORY_ACQUIRE) == 0),
		"UDP rejected send changed budget or ownership");
	testRequire(xrtNetUdpClose(pUdp),
		"UDP send budget close request failed");
	xrtAtomic32Store(&Edge.ReleaseOpen, 1, XMEMORY_RELEASE);
	testUdpEdgeWaitClosed(pUdp);
	testUdpEdgeWait(&Edge.Released, 1,
		"UDP accepted reference was not released");
	testUdpEdgeWait(&Edge.High, 1, "UDP high-water event missing");
	testUdpEdgeWait(&Edge.Low, 1, "UDP low-water event missing");
	testUdpEdgeWait(&Edge.Drain, 1, "UDP drain event missing");
	testUdpEdgeWait(&Edge.Close, 1, "UDP close event missing");

	for ( size_t i = 0; i < 2; i++ ) {
		xdeadline iDeadline = xrtDeadlineAfter(5000000u);
		size_t iPart = 0;

		for ( ;; ) {
			xnetresult Result = xrtNetSocketRecvFrom(
				Receiver,
				sData + iReceived,
				2,
				&iPart,
				&Remote
			);

			if ( Result == XNET_RESULT_OK ) {
				iReceived += iPart;
				break;
			}
			testRequire(Result == XNET_RESULT_AGAIN,
				"UDP send budget raw receive failed");
			testRequire(!xrtDeadlineExpired(iDeadline),
				"UDP send budget raw receive timed out");
			xrtThreadYield();
		}
	}
	testRequire((iReceived == 4) && (memcmp(sData, "aabb", 4) == 0),
		"UDP graceful close did not drain accepted sends");
	testRequire(xrtNetUdpStats(pUdp, &Stats) &&
		 (Stats.SentPackets == 2) && (Stats.SentBytes == 4) &&
		 (Stats.QueuedPackets == 0) && (Stats.QueuedBytes == 0) &&
		 (Stats.PeakQueuedPackets == 2) &&
		 (Stats.PeakQueuedBytes == 4),
		"UDP send budget final stats mismatch");
	xrtNetUdpDestroy(pUdp);
}



/* 系统性验证 UDP 截断、溢出和硬发送预算。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetengine* pEngine;
	xnetsocket Sender;
	xnetsocket Receiver;
	xnetaddr ReceiverAddress;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_UDP_BACKEND;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"UDP edge engine start failed");
	Sender = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM,
		XNET_SOCKET_NONBLOCK
	);
	Receiver = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM,
		XNET_SOCKET_NONBLOCK
	);
	testRequire((Sender != NULL) && (Receiver != NULL),
		"UDP edge raw sockets failed");
	testRequire(xrtNetAddrLoopback(
		&ReceiverAddress,
		XNET_FAMILY_IPV4,
		0
	) && xrtNetSocketBind(Receiver, &ReceiverAddress) &&
		 xrtNetSocketLocal(Receiver, &ReceiverAddress),
		"UDP edge raw receiver bind failed");

	testUdpTruncation(pEngine, Sender);
	testUdpOverflow(
		pEngine,
		Sender,
		XNET_UDP_DROP_NEWEST,
		'1',
		1,
		0,
		0
	);
	testUdpOverflow(
		pEngine,
		Sender,
		XNET_UDP_DROP_OLDEST,
		'2',
		0,
		1,
		0
	);
	testUdpOverflow(
		pEngine,
		Sender,
		XNET_UDP_DROP_ERROR,
		'1',
		1,
		0,
		1
	);
	testUdpReceiveByteLimit(pEngine, Sender);
	testUdpReleaseAbort(pEngine, Receiver, &ReceiverAddress, 1, 1);
	#if !TEST_UDP_COMPLETION
		testUdpBatchAbort(pEngine, Receiver, &ReceiverAddress);
	#endif
	testUdpSendBudget(pEngine, Receiver, &ReceiverAddress);

	testRequire(xrtNetSocketClose(Receiver) &&
		 xrtNetSocketClose(Sender) && xrtNetEngineDestroy(pEngine),
		"UDP edge cleanup failed");
	printf("[PASS] network UDP edges %s\n", TEST_UDP_BACKEND_NAME);
	return 0;
}
