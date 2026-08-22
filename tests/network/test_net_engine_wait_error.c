#include "../test.h"



typedef struct testenginewaiterror {
	xatomic32 Ready;
	xatomic32 Marker;
} testenginewaiterror;



/* 在 Worker 内故意破坏一个 readiness 观察，用于注入可重复的端口等待错误。 */
static void testEngineWaitErrorBreak(xnetworker* pWorker, ptr pData)
{
	testenginewaiterror* pContext = (testenginewaiterror*)pData;
	xnetport* pPort = xrtNetWorkerPort(pWorker);
	xnetsocket Socket = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM,
		XNET_SOCKET_NONBLOCK
	);
	uint32 iReady = 2;

	if ( (pPort != NULL) && (Socket != NULL) &&
		xrtNetPortWatch(pPort, Socket, 1, XNET_POLL_READ, NULL) ) {
		if ( xrtNetSocketClose(Socket) ) {
			iReady = 1;
		}
		Socket = NULL;
	}
	if ( Socket != NULL ) {
		(void)xrtNetSocketClose(Socket);
	}
	xrtAtomic32Store(&pContext->Ready, iReady, XMEMORY_RELEASE);
}



/* 端口处于终止错误时，Worker 仍必须继续处理关闭和控制命令。 */
static void testEngineWaitErrorMarker(xnetworker* pWorker, ptr pData)
{
	testenginewaiterror* pContext = (testenginewaiterror*)pData;

	(void)pWorker;
	xrtAtomic32Store(&pContext->Marker, 1, XMEMORY_RELEASE);
}



/* 在有限截止时间内等待原子状态，避免错误路径回归造成测试挂死。 */
static void testEngineWaitErrorWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(5000000);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) != iExpected ) {
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtSleepUs(1000);
	}
}



/* 验证等待错误可观测、有界退避，并且不会阻断后续 Worker 命令。 */
int main(void)
{
	xnetengineconfig Config;
	testenginewaiterror Context;
	xnetengine* pEngine;
	xnetworker* pWorker;
	xnetworkerstats Before;
	xnetworkerstats After;
	xdeadline Deadline;

	memset(&Context, 0, sizeof(Context));
	xrtAtomic32Init(&Context.Ready, 0);
	xrtAtomic32Init(&Context.Marker, 0);
	xrtNetEngineConfigInit(&Config);
	Config.Backend = XNET_PORT_SELECT;
	Config.Workers = 1;
	Config.IdleWait = 10000;
	pEngine = xrtNetEngineCreate(&Config);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"wait-error engine start failed");
	pWorker = xrtNetEngineWorker(pEngine, 0);
	testRequire((pWorker != NULL) && xrtNetEnginePost(
		pEngine,
		0,
		testEngineWaitErrorBreak,
		&Context
	), "wait-error injection post failed");
	testEngineWaitErrorWait(
		&Context.Ready,
		1,
		"wait-error injection failed"
	);

	Deadline = xrtDeadlineAfter(5000000);
	for ( ;; ) {
		testRequire(xrtNetWorkerStats(pWorker, &Before),
			"wait-error worker stats failed");
		if ( Before.WaitErrors != 0 ) {
			break;
		}
		testRequire(!xrtDeadlineExpired(Deadline),
			"worker did not observe the injected wait error");
		xrtSleepUs(1000);
	}
	testRequire((Before.LastWaitError == XNET_ERROR_PORT_WAIT) &&
		(Before.LastWaitSystemCode != 0),
		"worker wait-error detail mismatch");

	testRequire(xrtNetEnginePost(
		pEngine,
		0,
		testEngineWaitErrorMarker,
		&Context
	), "wait-error marker post failed");
	testEngineWaitErrorWait(
		&Context.Marker,
		1,
		"terminal port error blocked worker commands"
	);
	xrtSleepUs(50000);
	testRequire(xrtNetWorkerStats(pWorker, &After),
		"wait-error final stats failed");
	testRequire((After.WaitErrors >= Before.WaitErrors) &&
		((After.WaitErrors - Before.WaitErrors) <= 32u),
		"terminal port error entered a hot spin");
	testRequire(xrtNetEngineStop(pEngine) &&
		xrtNetEngineDestroy(pEngine),
		"wait-error engine cleanup failed");
	printf("[PASS] network engine wait error\n");
	return 0;
}
