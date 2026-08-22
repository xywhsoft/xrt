#include "../test.h"



#if defined(__linux__)
	#include <errno.h>
#endif



#if !defined(TEST_UDP_FUTURE_BACKEND)
	#define TEST_UDP_FUTURE_BACKEND XNET_PORT_SELECT
	#define TEST_UDP_FUTURE_BACKEND_NAME "select"
#endif

#define TEST_UDP_FUTURE_CLOSE_WAITS 64



typedef struct testudpblock {
	xatomic32 Entered;
	xatomic32 Release;
} testudpblock;



/* 等待 Future 在截止时间前完成，并要求达到指定终态。 */
static void testUdpFutureWait(
	xfuture* pFuture,
	xfuturestate State,
	cstr sMessage
)
{
	testRequire((pFuture != NULL) &&
		 (xrtFutureWaitFor(pFuture, UINT64_C(5000000)) == XWAIT_OK) &&
		 (xrtFutureState(pFuture) == State), sMessage);
}



/* 等待 UDP 拉取队列达到指定长度。 */
static void testUdpFutureWaitQueued(xnetudp* pUdp, size_t iCount)
{
	xdeadline iDeadline = xrtDeadlineAfter(UINT64_C(5000000));

	while ( xrtNetUdpQueued(pUdp) < iCount ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"UDP Future receive queue timed out");
		xrtThreadYield();
	}
}



/* 等待消费式接收 Future 数量到达目标。 */
static void testUdpFutureWaitWaiters(xnetudp* pUdp, size_t iCount)
{
	xdeadline iDeadline = xrtDeadlineAfter(UINT64_C(5000000));
	xnetudpstats Stats;

	for ( ;; ) {
		testRequire(xrtNetUdpStats(pUdp, &Stats),
			"UDP Future waiter stats failed");
		if ( Stats.ReceiveWaiters == iCount ) {
			return;
		}
		testRequire(!xrtDeadlineExpired(iDeadline),
			"UDP Future waiter count timed out");
		xrtThreadYield();
	}
}



/* 阻塞所属 Worker，稳定制造尚未归还的发送预算。 */
static void testUdpFutureBlockOpen(xnetudp* pUdp, ptr pData)
{
	testudpblock* pBlock = (testudpblock*)pData;

	(void)pUdp;
	xrtAtomic32Store(&pBlock->Entered, 1, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(&pBlock->Release, XMEMORY_ACQUIRE) == 0 ) {
		xrtThreadYield();
	}
}



#if defined(__linux__)
/* 在 Linux io_uring 路径验证错误条件、消费 Future、取消和所有权。 */
static void testUdpFutureErrors(xnetengine* pEngine)
{
	xnetudpconfig Config;
	xnetudpstats Stats;
	xnetudp* pUdp;
	xnetudperrorpacket* pPacket;
	xnetudperrorpacket* pRetained;
	const xnetdgramerror* pError;
	xnetsocket Reserved;
	xnetaddr Target;
	xfuture* pOpen;
	xfuture* pReady;
	xfuture* pCancelled;
	xfuture* pReceive;
	xfuture* pClose;

	if ( TEST_UDP_FUTURE_BACKEND != XNET_PORT_URING ) {
		return;
	}
	Reserved = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM,
		XNET_SOCKET_NONBLOCK
	);
	testRequire(
		(Reserved != NULL) &&
		xrtNetAddrLoopback(&Target, XNET_FAMILY_IPV4, 0) &&
		xrtNetSocketBind(Reserved, &Target) &&
		xrtNetSocketLocal(Reserved, &Target) &&
		xrtNetSocketClose(Reserved),
		"UDP Future error target setup failed"
	);

	xrtNetUdpConfigInit(&Config);
	Config.ReceiveErrors = true;
	Config.ErrorSize = 64;
	Config.ErrorQueueLimit = 1;
	Config.ErrorQueueByteLimit = 64;
	Config.PathMtu = XNET_PMTU_DISCOVER;
	pUdp = xrtNetUdpConnect(
		pEngine,
		&Target,
		0,
		&Config,
		NULL,
		NULL
	);
	testRequire(pUdp != NULL, "UDP Future error object create failed");
	pOpen = xrtNetUdpWaitAsync(pUdp, XNET_UDP_WAIT_OPEN);
	testUdpFutureWait(
		pOpen,
		XFUTURE_RESOLVED,
		"UDP Future error object open failed"
	);
	xrtFutureDestroy(pOpen);

	/* 条件 Future 不消费错误，并与消费 Future 明确互斥。 */
	pReady = xrtNetUdpWaitAsync(pUdp, XNET_UDP_WAIT_ERROR);
	testRequire((pReady != NULL) && !xrtFutureDone(pReady),
		"UDP error readiness Future completed without an error");
	testRequire(
		xrtNetUdpReceiveErrorAsync(pUdp) == NULL &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"UDP error Future ignored readiness conflict"
	);
	xrtClearError();
	testRequire(xrtFutureCancel(pReady),
		"UDP error readiness cancellation failed");
	testUdpFutureWait(
		pReady,
		XFUTURE_CANCELLED,
		"UDP error readiness did not cancel"
	);
	xrtFutureDestroy(pReady);

	/* 取消消费只撤销本次等待，且不能在统计中留下节点。 */
	pCancelled = xrtNetUdpReceiveErrorAsync(pUdp);
	testRequire(
		(pCancelled != NULL) &&
		xrtNetUdpStats(pUdp, &Stats) &&
		(Stats.ErrorWaiters == 1),
		"UDP error consuming waiter was not registered"
	);
	testRequire(xrtFutureCancel(pCancelled),
		"UDP error consuming cancellation failed");
	testUdpFutureWait(
		pCancelled,
		XFUTURE_CANCELLED,
		"UDP error consuming Future did not cancel"
	);
	xrtFutureDestroy(pCancelled);
	testRequire(
		xrtNetUdpStats(pUdp, &Stats) && (Stats.ErrorWaiters == 0),
		"UDP cancelled error Future retained a waiter"
	);

	pReceive = xrtNetUdpReceiveErrorAsync(pUdp);
	testRequire(
		(pReceive != NULL) &&
		(xrtNetUdpReceiveError(pUdp) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"direct UDP error receive raced a consuming Future"
	);
	xrtClearError();
	testRequire(
		xrtNetUdpSend(pUdp, "error", 5) == XNET_RESULT_OK,
		"UDP Future error trigger send failed"
	);
	testUdpFutureWait(
		pReceive,
		XFUTURE_RESOLVED,
		"UDP datagram error Future did not resolve"
	);
	pPacket = (xnetudperrorpacket*)xrtFutureValue(pReceive);
	pError = xrtNetUdpErrorPacketInfo(pPacket);
	testRequire(
		(pError != NULL) &&
		(pError->SystemCode == ECONNREFUSED) &&
		(pError->Kind == XERR_IO) &&
		(pError->Origin == XNET_DGRAM_ERROR_ICMP) &&
		((pError->Flags & XNET_DGRAM_ERROR_REMOTE) != 0) &&
		xrtNetAddrEqual(&pError->Remote, &Target) &&
		(xrtNetUdpErrorPacketSize(pPacket) == 5) &&
		(memcmp(xrtNetUdpErrorPacketData(pPacket), "error", 5) == 0),
		"UDP datagram error Future result mismatch"
	);
	pRetained = xrtNetUdpErrorPacketRef(pPacket);
	testRequire(pRetained == pPacket,
		"UDP Future error packet retain failed");
	xrtFutureDestroy(pReceive);
	testRequire(
		(xrtNetUdpErrorPacketSize(pRetained) == 5) &&
		(memcmp(xrtNetUdpErrorPacketData(pRetained), "error", 5) == 0),
		"UDP error packet did not outlive its Future"
	);
	xrtNetUdpErrorPacketDestroy(pRetained);
	testRequire(
		xrtNetUdpStats(pUdp, &Stats) &&
		(Stats.DatagramErrors == 1) &&
		(Stats.DatagramErrorsDropped == 0) &&
		(Stats.ErrorWaiters == 0) &&
		(Stats.ErrorQueued == 0) &&
		(xrtNetUdpQueuedErrors(pUdp) == 0),
		"UDP error Future statistics mismatch"
	);

	/* 正常关闭必须终结尚未满足的错误消费链并清零 waiter 统计。 */
	pReceive = xrtNetUdpReceiveErrorAsync(pUdp);
	pClose = xrtNetUdpWaitAsync(pUdp, XNET_UDP_WAIT_CLOSE);
	testRequire(
		(pReceive != NULL) && (pClose != NULL) && xrtNetUdpClose(pUdp),
		"UDP error Future close setup failed");
	testUdpFutureWait(
		pReceive,
		XFUTURE_CLOSED,
		"UDP pending error Future did not close"
	);
	testUdpFutureWait(
		pClose,
		XFUTURE_RESOLVED,
		"UDP error Future close failed"
	);
	xrtFutureDestroy(pReceive);
	xrtFutureDestroy(pClose);
	testRequire(
		xrtNetUdpStats(pUdp, &Stats) && (Stats.ErrorWaiters == 0),
		"UDP close retained an error waiter"
	);
	xrtNetUdpDestroy(pUdp);
}
#endif



/* 验证单包、批量、取消、可读、精确可写、排空和关闭 Future。 */
int main(void)
{
	testudpblock Block;
	xnetengineconfig EngineConfig;
	xnetudpconfig UdpConfig;
	xnetudpevents Events;
	xnetudpstats Stats;
	xnetenginestats EngineStats;
	xnetengine* pEngine;
	xnetudp* pServer;
	xnetudp* pClient;
	xnetudp* pBlocked;
	xnetaddr Address;
	xfuture* pServerOpen;
	xfuture* pClientOpen;
	xfuture* pCacheOpen;
	xfuture* pReceive;
	xfuture* pFirstReceive;
	xfuture* pSecondReceive;
	xfuture* pBatchFuture;
	xfuture* pReadable;
	xfuture* pWritable;
	xfuture* pDrain;
	xfuture* pClose;
	xfuture* pLateClose;
	xfuture* pLateClose2;
	xfuture* pServerClose[TEST_UDP_FUTURE_CLOSE_WAITS];
	xnetudppacket* pPacket;
	xnetudppacket* pRetained;
	xnetudppacket* pTaken;
	uint64 iNodeHits;
	xnetudpbatch* pBatch;
	char sBatch[3] = {'A', 'B', 'C'};

	memset(&Block, 0, sizeof(Block));
	memset(&Events, 0, sizeof(Events));
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_UDP_FUTURE_BACKEND;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"UDP Future engine start failed");
	#if defined(__linux__)
		testUdpFutureErrors(pEngine);
	#endif

	xrtNetUdpConfigInit(&UdpConfig);
	testRequire(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0),
		"UDP Future loopback address failed");
	pServer = xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&UdpConfig,
		NULL,
		NULL
	);
	testRequire((pServer != NULL) && xrtNetUdpLocal(pServer, &Address),
		"UDP Future server bind failed");
	pServerOpen = xrtNetUdpWaitAsync(pServer, XNET_UDP_WAIT_OPEN);
	pClient = xrtNetUdpConnect(
		pEngine,
		&Address,
		1,
		&UdpConfig,
		NULL,
		NULL
	);
	testRequire(pClient != NULL, "UDP Future client connect failed");
	pClientOpen = xrtNetUdpWaitAsync(pClient, XNET_UDP_WAIT_OPEN);
	testUdpFutureWait(pServerOpen, XFUTURE_RESOLVED,
		"UDP server open Future failed");
	testUdpFutureWait(pClientOpen, XFUTURE_RESOLVED,
		"UDP client open Future failed");
	xrtFutureDestroy(pServerOpen);
	xrtFutureDestroy(pClientOpen);
	testRequire(xrtNetEngineStats(pEngine, &EngineStats),
		"UDP Future node cache initial stats failed");
	iNodeHits = EngineStats.NodeCacheHits;
	pCacheOpen = xrtNetUdpWaitAsync(pClient, XNET_UDP_WAIT_OPEN);
	testRequire((pCacheOpen != NULL) &&
		 (xrtFutureWaitFor(pCacheOpen, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pCacheOpen) == XFUTURE_RESOLVED) &&
		 xrtNetEngineStats(pEngine, &EngineStats) &&
		 (EngineStats.NodeCacheHits > iNodeHits) &&
		 (EngineStats.NodeCachedBytes <= EngineConfig.NodeCacheBytes),
		"UDP Future waiter did not reuse the Worker node cache");

	/* 长期条件等待与消费式接收使用独立 FIFO，收包路径不能扫描关闭等待。 */
	for ( size_t i = 0; i < TEST_UDP_FUTURE_CLOSE_WAITS; i++ ) {
		pServerClose[i] = xrtNetUdpWaitAsync(
			pServer,
			XNET_UDP_WAIT_CLOSE
		);
		testRequire((pServerClose[i] != NULL) &&
			 !xrtFutureDone(pServerClose[i]),
			"UDP server close Future completed before close");
	}

	/* 等待超时不取消 Future；显式取消只撤销当前接收，随后可以继续使用 UDP。 */
	pReceive = xrtNetUdpReceiveAsync(pServer);
	testRequire(pReceive != NULL, "UDP receive cancellation Future failed");
	testRequire((xrtFutureWaitFor(
		pReceive,
		UINT64_C(1000)
	) == XWAIT_TIMEOUT) && !xrtFutureDone(pReceive),
		"UDP receive Future timeout changed the operation state");
	testRequire(xrtFutureCancel(pReceive),
		"UDP receive Future cancellation request failed");
	testUdpFutureWait(pReceive, XFUTURE_CANCELLED,
		"UDP receive Future did not confirm cancellation");
	xrtFutureDestroy(pReceive);
	testRequire(xrtNetUdpStats(pServer, &Stats) &&
		 (Stats.ReceiveWaiters == 0),
		"UDP cancelled receive retained a waiter");

	/* 调用方提前释放 Future 不释放内部操作；到包后结果会自动清理。 */
	pReceive = xrtNetUdpReceiveAsync(pServer);
	testRequire(pReceive != NULL,
		"UDP abandoned receive Future create failed");
	xrtFutureDestroy(pReceive);
	testRequire(xrtNetUdpSend(pClient, "abandoned", 9) == XNET_RESULT_OK,
		"UDP abandoned receive send failed");
	testUdpFutureWaitWaiters(pServer, 0);
	testRequire(xrtNetUdpQueued(pServer) == 0,
		"UDP abandoned Future left a consumed packet queued");

	pReceive = xrtNetUdpReceiveAsync(pServer);
	testRequire((pReceive != NULL) &&
		 (xrtNetUdpSend(pClient, "one", 3) == XNET_RESULT_OK),
		"UDP single receive Future setup failed");
	testUdpFutureWait(pReceive, XFUTURE_RESOLVED,
		"UDP single receive Future failed");
	pPacket = (xnetudppacket*)xrtFutureValue(pReceive);
	testRequire((pPacket != NULL) &&
		 (xrtNetUdpPacketSize(pPacket) == 3) &&
		 (memcmp(xrtNetUdpPacketData(pPacket), "one", 3) == 0),
		"UDP single receive Future payload mismatch");
	pRetained = xrtNetUdpPacketRef(pPacket);
	xrtFutureDestroy(pReceive);
	testRequire((pRetained != NULL) &&
		 (memcmp(xrtNetUdpPacketData(pRetained), "one", 3) == 0),
		"UDP packet reference did not outlive Future");
	xrtNetUdpPacketDestroy(pRetained);

	/* 多个消费式 Future 必须按注册顺序与到达的数据包一一配对。 */
	pFirstReceive = xrtNetUdpReceiveAsync(pServer);
	pSecondReceive = xrtNetUdpReceiveAsync(pServer);
	testRequire((pFirstReceive != NULL) && (pSecondReceive != NULL) &&
		 (xrtNetUdpSend(pClient, "first", 5) == XNET_RESULT_OK) &&
		 (xrtNetUdpSend(pClient, "second", 6) == XNET_RESULT_OK),
		"UDP multiple receive Future setup failed");
	testUdpFutureWait(pFirstReceive, XFUTURE_RESOLVED,
		"UDP first receive Future failed");
	testUdpFutureWait(pSecondReceive, XFUTURE_RESOLVED,
		"UDP second receive Future failed");
	pPacket = (xnetudppacket*)xrtFutureValue(pFirstReceive);
	testRequire((pPacket != NULL) &&
		 (xrtNetUdpPacketSize(pPacket) == 5) &&
		 (memcmp(xrtNetUdpPacketData(pPacket), "first", 5) == 0),
		"UDP first receive Future FIFO mismatch");
	pPacket = (xnetudppacket*)xrtFutureValue(pSecondReceive);
	testRequire((pPacket != NULL) &&
		 (xrtNetUdpPacketSize(pPacket) == 6) &&
		 (memcmp(xrtNetUdpPacketData(pPacket), "second", 6) == 0),
		"UDP second receive Future FIFO mismatch");
	xrtFutureDestroy(pFirstReceive);
	xrtFutureDestroy(pSecondReceive);

	/* 批量 Future 必须一次取走当前前缀，并允许逐包转移所有权。 */
	for ( size_t i = 0; i < 3; i++ ) {
		testRequire(xrtNetUdpSend(pClient, &sBatch[i], 1) ==
			XNET_RESULT_OK, "UDP batch Future send failed");
	}
	testUdpFutureWaitQueued(pServer, 3);
	pBatchFuture = xrtNetUdpReceiveBatchAsync(pServer, 8);
	testUdpFutureWait(pBatchFuture, XFUTURE_RESOLVED,
		"UDP batch receive Future failed");
	pBatch = (xnetudpbatch*)xrtFutureValue(pBatchFuture);
	testRequire((pBatch != NULL) && (xrtNetUdpBatchCount(pBatch) == 3),
		"UDP batch receive count mismatch");
	for ( size_t i = 0; i < 3; i++ ) {
		pPacket = xrtNetUdpBatchPacket(pBatch, i);
		testRequire((pPacket != NULL) &&
			 (*(xrtNetUdpPacketData(pPacket)) == sBatch[i]),
			"UDP batch receive order mismatch");
	}
	pTaken = xrtNetUdpBatchTake(pBatch, 1);
	xrtFutureDestroy(pBatchFuture);
	testRequire((pTaken != NULL) &&
		 (*(xrtNetUdpPacketData(pTaken)) == 'B'),
		"UDP batch packet ownership transfer failed");
	xrtNetUdpPacketDestroy(pTaken);

	/* 可读等待不消费报文，随后直接拉取必须仍取得同一数据包。 */
	pReadable = xrtNetUdpWaitAsync(pServer, XNET_UDP_WAIT_RECEIVE);
	testRequire((pReadable != NULL) &&
		 (xrtNetUdpSend(pClient, "ready", 5) == XNET_RESULT_OK),
		"UDP readable Future setup failed");
	testUdpFutureWait(pReadable, XFUTURE_RESOLVED,
		"UDP readable Future failed");
	xrtFutureDestroy(pReadable);
	pPacket = xrtNetUdpReceive(pServer);
	testRequire((pPacket != NULL) &&
		 (xrtNetUdpPacketSize(pPacket) == 5) &&
		 (memcmp(xrtNetUdpPacketData(pPacket), "ready", 5) == 0),
		"UDP readable Future consumed the packet");
	xrtNetUdpPacketDestroy(pPacket);

	/* 阻塞 Worker 后填满预算，验证按报文大小的可写等待和排空等待。 */
	Events.Open = testUdpFutureBlockOpen;
	xrtNetUdpConfigInit(&UdpConfig);
	UdpConfig.SendHighWater = 8;
	UdpConfig.SendLowWater = 0;
	UdpConfig.SendLimit = 8;
	UdpConfig.SendPacketLimit = 1;
	pBlocked = xrtNetUdpConnect(
		pEngine,
		&Address,
		1,
		&UdpConfig,
		&Events,
		&Block
	);
	testRequire(pBlocked != NULL, "UDP blocked client create failed");
	while ( xrtAtomic32Load(&Block.Entered, XMEMORY_ACQUIRE) == 0 ) {
		xrtThreadYield();
	}
	testRequire(xrtNetUdpSend(pBlocked, "12345678", 8) ==
		XNET_RESULT_OK, "UDP blocked budget fill failed");
	pWritable = xrtNetUdpWritableAsync(pBlocked, 1);
	pDrain = xrtNetUdpWaitAsync(pBlocked, XNET_UDP_WAIT_DRAIN);
	testRequire((pWritable != NULL) && (pDrain != NULL) &&
		 !xrtFutureDone(pWritable) && !xrtFutureDone(pDrain),
		"UDP writable or drain Future completed before budget release");
	xrtAtomic32Store(&Block.Release, 1, XMEMORY_RELEASE);
	testUdpFutureWait(pWritable, XFUTURE_RESOLVED,
		"UDP writable Future failed after budget release");
	testUdpFutureWait(pDrain, XFUTURE_RESOLVED,
		"UDP drain Future failed after budget release");
	xrtFutureDestroy(pWritable);
	xrtFutureDestroy(pDrain);

	pClose = xrtNetUdpWaitAsync(pBlocked, XNET_UDP_WAIT_CLOSE);
	testRequire((pClose != NULL) && xrtNetUdpClose(pBlocked),
		"UDP close Future setup failed");
	testUdpFutureWait(pClose, XFUTURE_RESOLVED,
		"UDP close Future failed");
	xrtFutureDestroy(pClose);
	xrtNetUdpDestroy(pBlocked);

	pClose = xrtNetUdpWaitAsync(pClient, XNET_UDP_WAIT_CLOSE);
	testRequire((pClose != NULL) && xrtNetUdpClose(pClient) &&
		 xrtNetUdpClose(pServer), "UDP Future final close failed");
	testUdpFutureWait(pClose, XFUTURE_RESOLVED,
		"UDP client final close Future failed");
	xrtFutureDestroy(pClose);
	while ( xrtNetUdpState(pServer) != XNET_UDP_CLOSED ) {
		xrtThreadYield();
	}
	for ( size_t i = 0; i < TEST_UDP_FUTURE_CLOSE_WAITS; i++ ) {
		testUdpFutureWait(
			pServerClose[i],
			XFUTURE_RESOLVED,
			"UDP server close Future failed"
		);
		xrtFutureDestroy(pServerClose[i]);
	}
	testRequire(xrtNetUdpStats(pServer, &Stats) &&
		 (Stats.ReceiveWaiters == 0),
		"UDP Future final waiter count mismatch");
	xrtFutureDestroy(pCacheOpen);
	testRequire(xrtNetEngineDestroy(pEngine),
		"UDP Future engine destroy failed");
	pLateClose = xrtNetUdpWaitAsync(pClient, XNET_UDP_WAIT_CLOSE);
	testRequire((pLateClose != NULL) &&
		 (xrtFutureWaitFor(pLateClose, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pLateClose) == XFUTURE_RESOLVED),
		"UDP late close Future used a destroyed Engine");
	pLateClose2 = xrtNetUdpWaitAsync(pClient, XNET_UDP_WAIT_CLOSE);
	testRequire((pLateClose2 != NULL) &&
		 (xrtFutureWaitFor(pLateClose2, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pLateClose2) == XFUTURE_RESOLVED),
		"UDP repeated late Future used a destroyed Engine");
	xrtFutureDestroy(pLateClose);
	xrtFutureDestroy(pLateClose2);
	xrtNetUdpDestroy(pClient);
	xrtNetUdpDestroy(pServer);
	printf("[PASS] network UDP Future %s\n",
		TEST_UDP_FUTURE_BACKEND_NAME);
	return 0;
}
