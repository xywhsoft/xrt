#include "../test.h"



#if defined(__linux__)
	#include <errno.h>
#endif



#if !defined(TEST_UDP_SYNC_BACKEND)
	#define TEST_UDP_SYNC_BACKEND XNET_PORT_SELECT
	#define TEST_UDP_SYNC_BACKEND_NAME "select"
#endif



/* 等待 UDP 拉取队列达到指定长度。 */
static void testUdpSyncQueued(xnetudp* pUdp, size_t iCount)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	while ( xrtNetUdpQueued(pUdp) < iCount ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"UDP sync receive queue timed out");
		xrtThreadYield();
	}
}



/* 正常关闭 UDP 并释放调用方引用。 */
static void testUdpSyncClose(xnetudp* pUdp)
{
	if ( xrtNetUdpState(pUdp) != XNET_UDP_CLOSED ) {
		testRequire(xrtNetUdpClose(pUdp),
			"UDP sync close request failed");
		testRequire(xrtNetUdpWait(
			pUdp,
			XNET_UDP_WAIT_CLOSE,
			xrtDeadlineAfter(5000000u),
			NULL
		), "UDP sync close wait failed");
	}
	xrtNetUdpDestroy(pUdp);
}



#if defined(__linux__)
/* 在 Linux io_uring 路径验证阻塞错误接收、超时和结果所有权。 */
static void testUdpSyncErrors(xnetengine* pEngine)
{
	xnetudpconfig Config;
	xnetudpstats Stats;
	xnetudp* pUdp;
	xnetudperrorpacket* pPacket;
	const xnetdgramerror* pError;
	xnetsocket Reserved;
	xnetaddr Target;

	if ( TEST_UDP_SYNC_BACKEND != XNET_PORT_URING ) {
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
		"UDP sync error target setup failed"
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
	testRequire(pUdp != NULL, "UDP sync error object create failed");
	testRequire(xrtNetUdpWait(
		pUdp,
		XNET_UDP_WAIT_OPEN,
		xrtDeadlineAfter(5000000u),
		NULL
	), "UDP sync error object open failed");

	/* 超时只撤销内部 Future，不关闭对象也不遗留错误消费者。 */
	pPacket = xrtNetUdpReceiveErrorWait(
		pUdp,
		xrtDeadlineAfter(1000u),
		NULL
	);
	testRequire(
		(pPacket == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_TIMEOUT) &&
		xrtNetUdpStats(pUdp, &Stats) &&
		(Stats.ErrorWaiters == 0) &&
		(xrtNetUdpState(pUdp) == XNET_UDP_OPEN),
		"UDP sync error timeout contract mismatch"
	);
	xrtClearError();

	testRequire(
		xrtNetUdpSend(pUdp, "error", 5) == XNET_RESULT_OK,
		"UDP sync error trigger send failed"
	);
	pPacket = xrtNetUdpReceiveErrorWait(
		pUdp,
		xrtDeadlineAfter(5000000u),
		NULL
	);
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
		"UDP sync error packet mismatch"
	);
	xrtNetUdpErrorPacketDestroy(pPacket);
	testRequire(
		xrtNetUdpStats(pUdp, &Stats) &&
		(Stats.DatagramErrors == 1) &&
		(Stats.ErrorWaiters == 0) &&
		(Stats.ErrorQueued == 0),
		"UDP sync error statistics mismatch"
	);
	testUdpSyncClose(pUdp);
}
#endif



/* 验证无隐藏 Engine 的 UDP 等待、单包接收和批量所有权。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetudpconfig UdpConfig;
	xnetengine* pEngine;
	xnetudp* pServer;
	xnetudp* pClient;
	xnetudppacket* pPacket;
	xnetudppacket* pTaken;
	xnetudpbatch* pBatch;
	xnetudpbatch* pHeldBatch;
	xcancel* pCancel;
	xnetaddr Address;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_UDP_SYNC_BACKEND;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"UDP sync engine start failed");
	testRequire(
		xrtNetUdpReceiveErrorWait(NULL, XRT_DEADLINE_NEVER, NULL) == NULL &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"UDP sync error receive accepted null UDP"
	);
	xrtClearError();
	#if defined(__linux__)
		testUdpSyncErrors(pEngine);
	#endif
	xrtNetUdpConfigInit(&UdpConfig);
	testRequire(xrtNetAddrLoopback(
		&Address,
		XNET_FAMILY_IPV4,
		0
	), "UDP sync loopback address failed");
	pServer = xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&UdpConfig,
		NULL,
		NULL
	);
	testRequire((pServer != NULL) && xrtNetUdpLocal(pServer, &Address),
		"UDP sync server bind failed");
	pClient = xrtNetUdpConnect(
		pEngine,
		&Address,
		1,
		&UdpConfig,
		NULL,
		NULL
	);
	testRequire(pClient != NULL, "UDP sync client connect failed");
	testRequire(xrtNetUdpWait(
		pServer,
		XNET_UDP_WAIT_OPEN,
		xrtDeadlineAfter(5000000u),
		NULL
	) && xrtNetUdpWait(
		pClient,
		XNET_UDP_WAIT_OPEN,
		xrtDeadlineAfter(5000000u),
		NULL
	), "UDP sync open wait failed");
	testRequire(xrtNetUdpWritable(
		pClient,
		16,
		xrtDeadlineAfter(5000000u),
		NULL
	), "UDP sync writable wait failed");

	/* 超时和取消只撤销本次接收，不关闭 UDP。 */
	testRequire(xrtNetUdpReceiveWait(
		pServer,
		xrtDeadlineAfter(1000u),
		NULL
	) == NULL, "UDP sync receive unexpectedly ignored timeout");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_TIMEOUT) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_UDP_RECEIVE) &&
		(xrtNetUdpState(pServer) == XNET_UDP_OPEN),
		"UDP sync receive timeout error mismatch");
	xrtClearError();
	pCancel = xrtCancelCreate();
	testRequire((pCancel != NULL) && xrtCancelRequest(pCancel),
		"UDP sync cancel setup failed");
	testRequire(xrtNetUdpReceiveWait(
		pServer,
		XRT_DEADLINE_NEVER,
		pCancel
	) == NULL, "UDP sync receive ignored cancellation");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_CANCELLED) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_UDP_RECEIVE),
		"UDP sync receive cancellation error mismatch");
	xrtCancelDestroy(pCancel);
	xrtClearError();

	/* 单包结果在内部 Future 销毁后仍由调用方独占。 */
	testRequire(xrtNetUdpSend(
		pClient,
		"packet",
		6
	) == XNET_RESULT_OK, "UDP sync single send failed");
	pPacket = xrtNetUdpReceiveWait(
		pServer,
		xrtDeadlineAfter(5000000u),
		NULL
	);
	testRequire((pPacket != NULL) &&
		(xrtNetUdpPacketSize(pPacket) == 6) &&
		(memcmp(xrtNetUdpPacketData(pPacket), "packet", 6) == 0),
		"UDP sync single receive payload mismatch");
	xrtNetUdpPacketDestroy(pPacket);

	/* 批量结果可引用并逐项转移数据包所有权。 */
	testRequire((xrtNetUdpSend(pClient, "A", 1) == XNET_RESULT_OK) &&
		(xrtNetUdpSend(pClient, "B", 1) == XNET_RESULT_OK) &&
		(xrtNetUdpSend(pClient, "C", 1) == XNET_RESULT_OK),
		"UDP sync batch send failed");
	testUdpSyncQueued(pServer, 3);
	pBatch = xrtNetUdpReceiveBatchWait(
		pServer,
		3,
		xrtDeadlineAfter(5000000u),
		NULL
	);
	testRequire((pBatch != NULL) &&
		(xrtNetUdpBatchCount(pBatch) == 3),
		"UDP sync batch receive failed");
	pHeldBatch = xrtNetUdpBatchRef(pBatch);
	testRequire(pHeldBatch == pBatch,
		"UDP sync batch retain failed");
	xrtNetUdpBatchDestroy(pBatch);
	pTaken = xrtNetUdpBatchTake(pHeldBatch, 1);
	testRequire((pTaken != NULL) &&
		(xrtNetUdpPacketSize(pTaken) == 1) &&
		(xrtNetUdpPacketData(pTaken)[0] == 'B'),
		"UDP sync batch take mismatch");
	xrtNetUdpPacketDestroy(pTaken);
	xrtNetUdpBatchDestroy(pHeldBatch);

	testUdpSyncClose(pClient);
	testUdpSyncClose(pServer);
	testRequire(xrtNetEngineDestroy(pEngine),
		"UDP sync engine destroy failed");
	printf("[PASS] UDP sync facade (%s)\n", TEST_UDP_SYNC_BACKEND_NAME);
	return 0;
}
