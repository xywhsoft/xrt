#include "../test.h"



typedef struct testudpfuturegate {
	xatomic32 Entered;
	xatomic32 Release;
} testudpfuturegate;



/* 阻塞 UDP Worker，暴露关闭门已设置但状态尚未推进的窗口。 */
static void testUdpFutureGateOpen(xnetudp* pUdp, ptr pData)
{
	testudpfuturegate* pGate = (testudpfuturegate*)pData;

	(void)pUdp;
	(void)xrtAtomic32FetchAdd(
		&pGate->Entered,
		1,
		XMEMORY_RELEASE
	);
	while ( xrtAtomic32Load(
		&pGate->Release,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
}



/* 在测试截止时间内等待一个原子计数达到下限。 */
static void testUdpFutureGateWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(UINT64_C(5000000));

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(iDeadline), sMessage);
		xrtThreadYield();
	}
}



/* 推送回调只用于验证接收 Future 与推送模式互斥。 */
static void testUdpFutureInvalidReceive(
	xnetudp* pUdp,
	const xnetudpmessage* pMessage,
	ptr pData
)
{
	(void)pUdp;
	(void)pMessage;
	(void)pData;
}



/* 要求当前错误准确指向 UDP 拉取状态冲突。 */
static void testUdpFutureInvalidState(cstr sMessage)
{
	const xerror* pError = xrtGetError();

	testRequire((pError != NULL) &&
		 (xrtErrorKind(pError) == XERR_STATE) &&
		 (xrtErrorCode(pError) == XNET_ERROR_UDP_RECEIVE_QUEUE),
		sMessage);
	xrtClearError();
}



/* 验证参数、模式混用和直接/异步多消费者冲突。 */
int main(void)
{
	testudpfuturegate Gate;
	xnetengineconfig EngineConfig;
	xnetudpconfig UdpConfig;
	xnetudpevents Events;
	xnetengine* pEngine;
	xnetudp* pPull;
	xnetudp* pPush;
	xnetudp* pGate;
	xnetaddr Address;
	xfuture* pFuture;
	xfuture* GateFutures[3];
	xnetudppacket* Packets[1];

	memset(&Gate, 0, sizeof(Gate));

	testRequire((xrtNetUdpWaitAsync(NULL, XNET_UDP_WAIT_OPEN) == NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"UDP Future accepted null UDP");
	xrtClearError();
	testRequire((xrtNetUdpReceiveAsync(NULL) == NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"UDP receive Future accepted null UDP");
	xrtClearError();
	testRequire((xrtNetUdpReceiveErrorAsync(NULL) == NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"UDP error Future accepted null UDP");
	xrtClearError();
	testRequire((xrtNetUdpReceiveBatchAsync(NULL, 0) == NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"UDP batch Future accepted zero capacity");
	xrtClearError();
	testRequire((xrtNetUdpPacketRef(NULL) == NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"UDP packet reference accepted null");
	xrtClearError();
	testRequire((xrtNetUdpBatchPacket(NULL, 0) == NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"UDP batch access accepted null");
	xrtClearError();

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"UDP invalid Future engine start failed");
	xrtNetUdpConfigInit(&UdpConfig);
	testRequire(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0),
		"UDP invalid Future address failed");
	pPull = xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&UdpConfig,
		NULL,
		NULL
	);
	testRequire(pPull != NULL, "UDP invalid Future pull object failed");
	testRequire(xrtNetUdpWaitAsync(
		pPull,
		XNET_UDP_WAIT_ERROR
	) == NULL, "UDP error readiness accepted a disabled error queue");
	testUdpFutureInvalidState(
		"UDP disabled error readiness mismatch"
	);
	testRequire(xrtNetUdpReceiveErrorAsync(pPull) == NULL,
		"UDP error Future accepted a disabled error queue");
	testUdpFutureInvalidState("UDP disabled error Future mismatch");

	testRequire(xrtNetUdpWaitAsync(
		pPull,
		(xnetudpwait)99
	) == NULL, "UDP Future accepted invalid wait kind");
	testRequire(xrtNetUdpReceiveBatchAsync(pPull, 257) == NULL,
		"UDP batch Future accepted excessive capacity");
	testRequire(xrtNetUdpWritableAsync(
		pPull,
		(size_t)XNET_UDP_PAYLOAD_MAX + 1
	) == NULL, "UDP writable Future accepted excessive datagram");
	xrtClearError();

	/* Readiness 和消费式接收不能同时登记。 */
	pFuture = xrtNetUdpWaitAsync(pPull, XNET_UDP_WAIT_RECEIVE);
	testRequire(pFuture != NULL, "UDP readable Future create failed");
	testRequire(xrtNetUdpReceiveAsync(pPull) == NULL,
		"UDP consuming Future mixed with readable Future");
	testUdpFutureInvalidState(
		"UDP readable/consuming conflict error mismatch");
	testRequire(xrtFutureCancel(pFuture),
		"UDP readable Future cancellation failed");
	xrtFutureDestroy(pFuture);

	pFuture = xrtNetUdpReceiveAsync(pPull);
	testRequire(pFuture != NULL, "UDP consuming Future create failed");
	testRequire(xrtNetUdpWaitAsync(
		pPull,
		XNET_UDP_WAIT_RECEIVE
	) == NULL, "UDP readable Future mixed with consuming Future");
	testUdpFutureInvalidState(
		"UDP consuming/readable conflict error mismatch");
	testRequire(xrtNetUdpReceive(pPull) == NULL,
		"direct UDP receive raced an asynchronous waiter");
	testUdpFutureInvalidState("UDP direct receive conflict error mismatch");
	testRequire(xrtNetUdpReceiveBatch(pPull, Packets, 1) == 0,
		"direct UDP batch raced an asynchronous waiter");
	testUdpFutureInvalidState("UDP direct batch conflict error mismatch");
	testRequire(xrtFutureCancel(pFuture),
		"UDP consuming Future cancellation failed");
	testRequire(xrtFutureWaitFor(pFuture, UINT64_C(1000000)) == XWAIT_OK,
		"UDP consuming Future cancellation wait failed");
	xrtFutureDestroy(pFuture);

	/* 推送 UDP 仍可等待状态，但不能建立任何拉取接收 Future。 */
	memset(&Events, 0, sizeof(Events));
	Events.Receive = testUdpFutureInvalidReceive;
	pPush = xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&UdpConfig,
		&Events,
		NULL
	);
	testRequire(pPush != NULL, "UDP invalid Future push object failed");
	pFuture = xrtNetUdpWaitAsync(pPush, XNET_UDP_WAIT_OPEN);
	testRequire((pFuture != NULL) &&
		 (xrtFutureWaitFor(pFuture, UINT64_C(1000000)) == XWAIT_OK) &&
		 (xrtFutureState(pFuture) == XFUTURE_RESOLVED),
		"UDP push object open Future failed");
	xrtFutureDestroy(pFuture);
	testRequire(xrtNetUdpReceiveAsync(pPush) == NULL,
		"UDP receive Future accepted push object");
	testUdpFutureInvalidState("UDP push receive conflict error mismatch");
	testRequire(xrtNetUdpWaitAsync(
		pPush,
		XNET_UDP_WAIT_RECEIVE
	) == NULL, "UDP readable Future accepted push object");
	testUdpFutureInvalidState("UDP push readable conflict error mismatch");

	/* 已受理 Abort 必须封闭旧 OPEN 状态上的全部成功条件。 */
	memset(&Events, 0, sizeof(Events));
	Events.Open = testUdpFutureGateOpen;
	pGate = xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&UdpConfig,
		&Events,
		&Gate
	);
	testRequire(pGate != NULL, "UDP Future gate object failed");
	testUdpFutureGateWait(
		&Gate.Entered,
		1,
		"UDP Future gate callback did not start"
	);
	testRequire(xrtNetUdpAbort(pGate),
		"UDP Future gate abort failed");
	GateFutures[0] = xrtNetUdpWaitAsync(pGate, XNET_UDP_WAIT_OPEN);
	GateFutures[1] = xrtNetUdpWritableAsync(pGate, 1);
	GateFutures[2] = xrtNetUdpReceiveAsync(pGate);
	for ( size_t i = 0; i < 3; i++ ) {
		testRequire((GateFutures[i] != NULL) &&
			(xrtFutureState(GateFutures[i]) == XFUTURE_PENDING),
			"UDP Future crossed an accepted abort gate");
	}
	xrtAtomic32Store(&Gate.Release, 1, XMEMORY_RELEASE);
	for ( size_t i = 0; i < 3; i++ ) {
		testRequire(
			xrtFutureWaitFor(
				GateFutures[i],
				UINT64_C(5000000)
			) == XWAIT_OK &&
			(xrtFutureState(GateFutures[i]) == XFUTURE_CANCELLED),
			"UDP Future abort terminal mismatch"
		);
		xrtFutureDestroy(GateFutures[i]);
	}
	testRequire(xrtNetUdpState(pGate) == XNET_UDP_CLOSED,
		"UDP Future gate object did not close");
	xrtNetUdpDestroy(pGate);

	testRequire(xrtNetUdpClose(pPush) && xrtNetUdpClose(pPull),
		"UDP invalid Future close failed");
	while ( (xrtNetUdpState(pPush) != XNET_UDP_CLOSED) ||
		 (xrtNetUdpState(pPull) != XNET_UDP_CLOSED) ) {
		xrtThreadYield();
	}
	xrtNetUdpDestroy(pPush);
	xrtNetUdpDestroy(pPull);
	testRequire(xrtNetEngineDestroy(pEngine),
		"UDP invalid Future engine destroy failed");
	printf("[PASS] network UDP Future invalid inputs\n");
	return 0;
}
