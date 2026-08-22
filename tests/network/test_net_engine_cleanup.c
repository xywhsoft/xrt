#include "../test.h"
#include "../../src/internal/xrt_net_engine.h"



typedef struct testenginecleanup {
	xnetbuf Buffer;
	xatomic32 Held;
	xatomic32 Released;
} testenginecleanup;



/* 等待 Worker 完成指定的清理测试步骤。 */
static void testEngineCleanupWait(
	const xatomic32* pValue,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) == 0 ) {
		testRequire(!xrtDeadlineExpired(iDeadline), sMessage);
		xrtThreadYield();
	}
}



/* 从 Worker 共享池保留一个块，模拟尚未归还的内部缓冲。 */
static void testEngineCleanupHold(xnetworker* pWorker, ptr pData)
{
	testenginecleanup* pContext = (testenginecleanup*)pData;
	xnetbufpool* pPool = xrtNetWorkerBufPool(pWorker);

	testRequire(pPool != NULL, "cleanup worker buffer pool lookup failed");
	testRequire(xrtNetBufInit(&pContext->Buffer, pPool),
		"cleanup worker buffer init failed");
	testRequire(xrtNetBufAppend(&pContext->Buffer, "held", 4),
		"cleanup worker buffer append failed");
	xrtAtomic32Store(&pContext->Held, 1, XMEMORY_RELEASE);
}



/* 在重启后的同一 Worker 上归还旧运行周期保留的块。 */
static void testEngineCleanupRelease(xnetworker* pWorker, ptr pData)
{
	testenginecleanup* pContext = (testenginecleanup*)pData;

	testRequire(xrtNetWorkerBufPool(pWorker) != NULL,
		"cleanup restarted worker buffer pool lookup failed");
	xrtNetBufClear(&pContext->Buffer);
	xrtAtomic32Store(&pContext->Released, 1, XMEMORY_RELEASE);
}



/* 验证清理失败可见、对象保留且重启后能够完成资源归还。 */
int main(void)
{
	testenginecleanup Context;
	xnetengineconfig Config;
	xnetengine* pEngine;

	memset(&Context, 0, sizeof(Context));
	xrtAtomic32Init(&Context.Held, 0);
	xrtAtomic32Init(&Context.Released, 0);
	xrtNetEngineConfigInit(&Config);
	Config.Workers = 1;
	pEngine = xrtNetEngineCreate(&Config);
	testRequire(pEngine != NULL, "cleanup engine create failed");
	testRequire(xrtNetEngineStart(pEngine), "cleanup engine start failed");
	testRequire(xrtNetEnginePost(
		pEngine,
		0,
		testEngineCleanupHold,
		&Context
	), "cleanup hold post failed");
	testEngineCleanupWait(&Context.Held,
		"cleanup hold callback did not execute");

	/* Stop 必须报告池仍被占用，并保留可恢复的停止态 Engine。 */
	testRequire(!xrtNetEngineStop(pEngine),
		"cleanup engine stop ignored a live pool block");
	testRequire((xrtGetError() != NULL) &&
		 (xrtErrorCode(xrtGetError()) == XNET_ERROR_POOL_BUSY),
		"cleanup engine stop error mismatch");
	testRequire(xrtNetEngineState(pEngine) == XNET_ENGINE_STOPPED,
		"cleanup engine stop failure state mismatch");
	xrtClearError();

	/* Destroy 同样不能释放仍被 Buffer 引用的 Engine 与池。 */
	testRequire(!xrtNetEngineDestroy(pEngine),
		"cleanup engine destroy ignored a live pool block");
	testRequire((xrtGetError() != NULL) &&
		 (xrtErrorCode(xrtGetError()) == XNET_ERROR_POOL_BUSY),
		"cleanup engine destroy error mismatch");
	testRequire(xrtNetEngineState(pEngine) == XNET_ENGINE_STOPPED,
		"cleanup engine destroy failure state mismatch");
	xrtClearError();

	/* 重启复用保留池，归还旧块后必须可以完整停止和销毁。 */
	testRequire(xrtNetEngineStart(pEngine),
		"cleanup engine recovery start failed");
	testRequire(xrtNetEnginePost(
		pEngine,
		0,
		testEngineCleanupRelease,
		&Context
	), "cleanup release post failed");
	testEngineCleanupWait(&Context.Released,
		"cleanup release callback did not execute");
	testRequire(xrtNetEngineStop(pEngine),
		"cleanup engine recovery stop failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"cleanup engine recovery destroy failed");
	printf("[PASS] network engine cleanup recovery\n");
	return 0;
}
