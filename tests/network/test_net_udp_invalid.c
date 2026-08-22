#include "../test.h"
#include "../../src/internal/xrt_udp.h"



/* 推送模式占位回调，用于验证拉取 API 不会混用。 */
static void testUdpInvalidReceive(
	xnetudp* pUdp,
	const xnetudpmessage* pMessage,
	ptr pData
)
{
	(void)pUdp;
	(void)pMessage;
	(void)pData;
}



/* 等待 UDP 关闭。 */
static void testUdpInvalidWaitClosed(xnetudp* pUdp)
{
	xdeadline iDeadline = xrtDeadlineAfter(3000000u);

	while ( xrtNetUdpState(pUdp) != XNET_UDP_CLOSED ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"invalid UDP close timed out");
		xrtThreadYield();
	}
}



/* 验证配置、空指针、地址、模式和 Worker 限制。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetudpconfig UdpConfig;
	xnetudpevents Events;
	xnetudpstats Stats;
	xnetengine* pEngine;
	xnetudp* pUdp;
	size_t iUdpSizeLimit = 648u;

	/* 核心对象、默认单槽及可裁剪 Future 状态分别受固定预算约束。 */
	#if defined(XRT_FEATURE_NET_UDP_FUTURE)
		iUdpSizeLimit += 80u;
	#endif
	if ( sizeof(xnetudp) > iUdpSizeLimit ) {
		fprintf(
			stderr,
			"[udp-size] actual=%zu limit=%zu future=%d\n",
			sizeof(xnetudp),
			iUdpSizeLimit,
			#if defined(XRT_FEATURE_NET_UDP_FUTURE)
				1
			#else
				0
			#endif
		);
	}
	testRequire(sizeof(xnetudp) <= iUdpSizeLimit,
		"UDP fixed object is too large");
	xnetudp* pPush;
	xnetaddr Address;
	xnetaddr Other;
	xnetspan Span;
	size_t iAccepted = 9;

	xrtNetUdpConfigInit(&UdpConfig);
	testRequire((UdpConfig.ReceiveSize != 0) &&
		 (UdpConfig.ReceiveConcurrency != 0) &&
		 (UdpConfig.ReceiveBatch != 0) &&
		 (UdpConfig.ErrorSize != 0) &&
		 (UdpConfig.SendPacketLimit != 0) &&
		 (UdpConfig.SendConcurrency == 1),
		"invalid UDP defaults");
	testRequire(xrtNetUdpRef(NULL) == NULL,
		"UDP ref accepted NULL");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"UDP NULL ref error mismatch");
	xrtClearError();
	testRequire((xrtNetUdpSend(NULL, NULL, 0) == XNET_RESULT_ERROR) &&
		 (xrtNetUdpSendVec(NULL, NULL, 0) == XNET_RESULT_ERROR) &&
		 (xrtNetUdpSendRef(NULL, NULL, 0, NULL, NULL) ==
		  XNET_RESULT_ERROR) &&
		 (xrtNetUdpSendTake(NULL, NULL, 0) == XNET_RESULT_ERROR),
		"UDP send accepted NULL object");
	xrtClearError();
	testRequire(!xrtNetUdpClose(NULL) && !xrtNetUdpAbort(NULL) &&
		 !xrtNetUdpStats(NULL, &Stats),
		"UDP control accepted NULL object");
	xrtClearError();
	testRequire((xrtNetUdpState(NULL) == XNET_UDP_CLOSED) &&
		 (xrtNetUdpWorker(NULL) == NULL) &&
		 (xrtNetUdpData(NULL) == NULL) &&
		 (xrtNetUdpError(NULL) == NULL) &&
		 (xrtNetUdpPending(NULL) == 0) &&
		 (xrtNetUdpQueued(NULL) == 0) &&
		 (xrtNetUdpQueuedErrors(NULL) == 0) &&
		 (xrtNetUdpQueuedErrorBytes(NULL) == 0) &&
		 (xrtNetUdpPathMtu(NULL) == 0) &&
		 !xrtNetUdpConnected(NULL),
		"UDP NULL query defaults mismatch");

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(pEngine != NULL, "invalid UDP engine create failed");
	testRequire(xrtNetAddrLoopback(
		&Address,
		XNET_FAMILY_IPV4,
		0
	), "invalid UDP loopback failed");
	testRequire(xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		NULL,
		NULL,
		NULL
	) == NULL, "stopped engine accepted UDP");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_CLOSED,
		"stopped UDP error mismatch");
	xrtClearError();
	testRequire(xrtNetEngineStart(pEngine),
		"invalid UDP engine start failed");

	UdpConfig.ReceiveSize = 0;
	testRequire(xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&UdpConfig,
		NULL,
		NULL
	) == NULL, "UDP accepted zero receive size");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		 (xrtErrorCode(xrtGetError()) == XNET_ERROR_UDP_CONFIG),
		"invalid UDP config error mismatch");
	xrtClearError();
	xrtNetUdpConfigInit(&UdpConfig);
	UdpConfig.SendConcurrency = 0;
	testRequire(xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&UdpConfig,
		NULL,
		NULL
	) == NULL, "UDP accepted zero send concurrency");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		 (xrtErrorCode(xrtGetError()) == XNET_ERROR_UDP_CONFIG),
		"invalid UDP send concurrency error mismatch");
	xrtClearError();
	xrtNetUdpConfigInit(&UdpConfig);
	UdpConfig.ErrorSize = 0;
	testRequire(xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&UdpConfig,
		NULL,
		NULL
	) == NULL, "UDP accepted zero error receive size");
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_UDP_CONFIG),
		"invalid UDP error size mismatch"
	);
	xrtClearError();
	xrtNetUdpConfigInit(&UdpConfig);
	UdpConfig.PathMtu = (xnetpmtumode)99;
	testRequire(xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&UdpConfig,
		NULL,
		NULL
	) == NULL, "UDP accepted invalid path MTU mode");
	xrtClearError();
	xrtNetUdpConfigInit(&UdpConfig);
	UdpConfig.SendConcurrency = 65;
	testRequire(xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&UdpConfig,
		NULL,
		NULL
	) == NULL, "UDP accepted excessive send concurrency");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		 (xrtErrorCode(xrtGetError()) == XNET_ERROR_UDP_CONFIG),
		"excessive UDP send concurrency error mismatch");
	xrtClearError();
	xrtNetUdpConfigInit(&UdpConfig);
	UdpConfig.ReceiveMeta = XNET_DGRAM_META_TRUNCATED;
	testRequire(xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&UdpConfig,
		NULL,
		NULL
	) == NULL, "UDP accepted a metadata result flag as configuration");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_UDP_CONFIG),
		"invalid UDP metadata config error mismatch");
	xrtClearError();
	xrtNetUdpConfigInit(&UdpConfig);
	UdpConfig.ExclusiveAddress = true;
	UdpConfig.ReuseAddress = true;
	testRequire(xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&UdpConfig,
		NULL,
		NULL
	) == NULL, "UDP accepted exclusive reuse conflict");
	xrtClearError();
	xrtNetUdpConfigInit(&UdpConfig);
	pUdp = xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&UdpConfig,
		NULL,
		NULL
	);
	testRequire((pUdp != NULL) && xrtNetUdpLocal(pUdp, &Address),
		"invalid UDP bind setup failed");
	testRequire(
		(xrtNetUdpReceiveError(pUdp) == NULL) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"UDP error pull accepted a disabled error queue"
	);
	xrtClearError();
	testRequire(xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&UdpConfig,
		NULL,
		NULL
	) == NULL, "UDP default allowed a duplicate active bind");
	testRequire(xrtErrorCode(xrtGetError()) == XNET_ERROR_SOCKET_BIND,
		"duplicate UDP bind error mismatch");
	xrtClearError();

	testRequire(xrtNetUdpSend(pUdp, "x", 1) == XNET_RESULT_ERROR,
		"unconnected UDP send accepted no remote");
	testRequire(xrtErrorCode(xrtGetError()) == XNET_ERROR_UDP_SEND,
		"unconnected UDP send error mismatch");
	xrtClearError();
	Other = Address;
	Other.Port = 1;
	testRequire(xrtNetUdpSendTo(
		pUdp,
		&Other,
		"x",
		XNET_UDP_PAYLOAD_MAX + 1u
	) == XNET_RESULT_ERROR, "UDP accepted oversized payload");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		 (xrtNetUdpPending(pUdp) == 0),
		"oversized UDP send changed budget");
	xrtClearError();
	Span.Data = NULL;
	Span.Size = 1;
	testRequire(xrtNetUdpSendVecTo(
		pUdp,
		&Other,
		&Span,
		1
	) == XNET_RESULT_ERROR, "UDP vector accepted NULL data");
	xrtClearError();
	testRequire(xrtNetUdpSendBatch(
		pUdp,
		NULL,
		1,
		&iAccepted
	) == XNET_RESULT_ERROR && (iAccepted == 0),
		"UDP batch accepted invalid items");
	xrtClearError();
	testRequire(xrtNetUdpSendBatch(
		pUdp,
		NULL,
		0,
		NULL
	) == XNET_RESULT_ERROR,
		"UDP batch accepted a missing count output");
	xrtClearError();
	testRequire(!xrtNetUdpPeer(pUdp, &Other),
		"unconnected UDP returned a peer");
	xrtClearError();
	testRequire((xrtNetUdpSocket(pUdp) == NULL) &&
		 !xrtNetUdpSetData(pUdp, pUdp) &&
		 !xrtNetUdpJoin(pUdp, &Other, &Address) &&
		 !xrtNetUdpLeave(pUdp, &Other, &Address) &&
		 !xrtNetUdpMulticastLoop(pUdp, true) &&
		 !xrtNetUdpMulticastHopLimit(pUdp, 1) &&
		 !xrtNetUdpMulticastInterface(pUdp, &Address),
		"UDP Worker-only API succeeded off Worker");
	xrtClearError();

	memset(&Events, 0, sizeof(Events));
	Events.Receive = testUdpInvalidReceive;
	Address.Port = 0;
	pPush = xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		NULL,
		&Events,
		NULL
	);
	testRequire(pPush != NULL, "push UDP setup failed");
	testRequire(xrtNetUdpReceive(pPush) == NULL,
		"push UDP exposed pull receive");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE,
		"push/pull UDP mode error mismatch");
	xrtClearError();

	Other = Address;
	Other.Port = 0;
	testRequire(xrtNetUdpConnect(
		pEngine,
		&Other,
		0,
		NULL,
		NULL,
		NULL
	) == NULL, "connected UDP accepted port zero");
	xrtClearError();
	testRequire(xrtNetUdpClose(pPush) && xrtNetUdpClose(pUdp),
		"invalid UDP cleanup close failed");
	testUdpInvalidWaitClosed(pPush);
	testUdpInvalidWaitClosed(pUdp);
	xrtNetUdpDestroy(pPush);
	xrtNetUdpDestroy(pUdp);
	testRequire(xrtNetEngineDestroy(pEngine),
		"invalid UDP engine destroy failed");
	printf("[PASS] network UDP invalid inputs\n");
	return 0;
}
