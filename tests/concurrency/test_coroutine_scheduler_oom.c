#include "../test.h"



/* 可切换分配器用于覆盖调度器创建和 timer heap 扩容失败。 */
typedef struct testcoschedoom {
	bool Fail;
	int PostRuns;
	int PostDestroyed;
} testcoschedoom;



/* 正常阶段转发到 C 分配器，失败阶段拒绝新分配。 */
static ptr testCoSchedOomAlloc(ptr pContext, size_t iSize)
{
	testcoschedoom* pState = (testcoschedoom*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 正常阶段允许调整内存，失败阶段保持原对象不变。 */
static ptr testCoSchedOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testcoschedoom* pState = (testcoschedoom*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放测试分配器取得的内存。 */
static void testCoSchedOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* READY 协程在关闭后不执行此过程。 */
static ptr testCoSchedOomProc(ptr pData)
{
	return pData;
}



/* 调度器 post 成功执行时记录次数。 */
static void testCoSchedOomPost(xcosched* pSched, ptr pData)
{
	testcoschedoom* pState = (testcoschedoom*)pData;

	(void)pSched;
	pState->PostRuns++;
}



/* Owned post 析构只记录所有权是否被错误消费。 */
static void testCoSchedOomPostDestroy(ptr pData)
{
	testcoschedoom* pState = (testcoschedoom*)pData;

	pState->PostDestroyed++;
}



/* 验证创建和大容量扩容 OOM 均保持调度器可继续使用。 */
int main(void)
{
	testcoschedoom tState;
	xallocator tAllocator;
	xcosched* pSched;
	int iAccepted = 0;

	memset(&tState, 0, sizeof(tState));
	tState.Fail = true;
	memset(&tAllocator, 0, sizeof(tAllocator));
	tAllocator.Context = &tState;
	tAllocator.Alloc = testCoSchedOomAlloc;
	tAllocator.Realloc = testCoSchedOomRealloc;
	tAllocator.Free = testCoSchedOomFree;
	testRequire(xrtSetAllocator(&tAllocator), "scheduler OOM allocator install failed");
	testRequire(xrtCoSchedCreate() == NULL, "scheduler create succeeded under OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "scheduler create OOM error mismatch");

	tState.Fail = false;
	xrtClearError();
	pSched = xrtCoSchedCreate();
	testRequire(pSched != NULL, "scheduler create after OOM failed");

	/* 保留已受理节点直到耗尽缓存，确保命中 post 的真实分配失败路径。 */
	tState.Fail = true;
	for ( ; iAccepted < 4096; iAccepted++ ) {
		xrtClearError();
		if ( !xrtCoSchedPost(pSched, testCoSchedOomPost, &tState) ) {
			break;
		}
	}
	testRequire(iAccepted < 4096, "scheduler post did not reach OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"scheduler post OOM error mismatch");
	xrtClearError();
	testRequire(!xrtCoSchedPostOwned(
		pSched,
		testCoSchedOomPost,
		&tState,
		testCoSchedOomPostDestroy
	), "owned scheduler post succeeded under OOM");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(tState.PostDestroyed == 0), "scheduler post OOM consumed owned data");
	tState.Fail = false;
	testRequire(xrtCoSchedRun(pSched), "scheduler accepted post drain failed");
	testRequire(tState.PostRuns == iAccepted,
		"scheduler accepted post run count mismatch");
	testRequire(xrtCoSchedPostOwned(
		pSched,
		testCoSchedOomPost,
		&tState,
		testCoSchedOomPostDestroy
	), "owned scheduler post recovery failed");
	testRequire(xrtCoSchedRun(pSched), "owned scheduler post recovery drain failed");
	testRequire((tState.PostRuns == (iAccepted + 1)) &&
		(tState.PostDestroyed == 1), "owned scheduler post recovery mismatch");

	for ( int i = 0; i < 128; i++ ) {
		testRequire(
			xrtCoSpawn(pSched, testCoSchedOomProc, NULL, NULL) != NULL,
			"scheduler OOM setup spawn failed"
		);
	}
	testRequire(xrtCoSchedAlive(pSched) == 128, "scheduler OOM setup alive mismatch");

	tState.Fail = true;
	testRequire(
		xrtCoSpawn(pSched, testCoSchedOomProc, NULL, NULL) == NULL,
		"timer heap expansion succeeded under OOM"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "timer heap OOM error mismatch");
	testRequire(xrtCoSchedAlive(pSched) == 128, "timer heap OOM changed alive count");

	tState.Fail = false;
	xrtClearError();
	testRequire(xrtCoSchedClose(pSched), "scheduler OOM close failed");
	testRequire(xrtCoSchedRun(pSched), "scheduler OOM drain failed");
	testRequire(xrtCoSchedDestroy(pSched), "scheduler OOM destroy failed");
	testRequire(xrtCoThreadDetach(), "scheduler OOM runtime detach failed");

	printf("[PASS] coroutine_scheduler_oom\n");
	return 0;
}
