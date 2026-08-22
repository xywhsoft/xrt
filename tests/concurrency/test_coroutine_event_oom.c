#include "../test.h"



/* 可切换分配器验证事件对象创建失败和等待零分配路径。 */
typedef struct testcoeventoom {
	bool Fail;
} testcoeventoom;



/* 正常阶段转发到 C 分配器，失败阶段拒绝分配。 */
static ptr testCoEventOomAlloc(ptr pContext, size_t iSize)
{
	testcoeventoom* pState = (testcoeventoom*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 正常阶段允许调整内存，失败阶段保持原对象不变。 */
static ptr testCoEventOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testcoeventoom* pState = (testcoeventoom*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放测试分配器取得的内存。 */
static void testCoEventOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* OOM 等待用例在协程栈上完成全部注册。 */
typedef struct testcoeventoomwait {
	testcoeventoom* Allocator;
	xcoevent* Event;
	xwaitresult Result;
} testcoeventoomwait;



/* 启用失败分配器后永久等待事件。 */
static ptr testCoEventOomWait(ptr pData)
{
	testcoeventoomwait* pWait = (testcoeventoomwait*)pData;

	pWait->Allocator->Fail = true;
	pWait->Result = xrtCoEventAwait(pWait->Event);
	return pWait;
}



/* 验证 Create OOM 不泄漏，且阻塞等待和唤醒均不需要堆分配。 */
int main(void)
{
	testcoeventoom tState;
	testcoeventoomwait tWait;
	xallocator tAllocator;
	xcoevent tEvent;
	xcosched* pSched;
	xcoro* pCo;

	memset(&tState, 0, sizeof(tState));
	memset(&tWait, 0, sizeof(tWait));
	memset(&tAllocator, 0, sizeof(tAllocator));
	tAllocator.Context = &tState;
	tAllocator.Alloc = testCoEventOomAlloc;
	tAllocator.Realloc = testCoEventOomRealloc;
	tAllocator.Free = testCoEventOomFree;
	testRequire(
		xrtSetAllocator(&tAllocator),
		"coroutine event OOM allocator install failed"
	);

	tState.Fail = true;
	testRequire(
		xrtCoEventCreate(false, false) == NULL,
		"coroutine event Create succeeded under OOM"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"coroutine event Create OOM error mismatch"
	);

	tState.Fail = false;
	xrtClearError();
	testRequire(
		xrtCoEventInit(&tEvent, false, false),
		"coroutine event init after OOM failed"
	);
	pSched = xrtCoSchedCreate();
	testRequire(pSched != NULL, "coroutine event OOM scheduler create failed");
	tWait.Allocator = &tState;
	tWait.Event = &tEvent;
	tWait.Result = XWAIT_ERROR;
	pCo = xrtCoSpawn(pSched, testCoEventOomWait, &tWait, NULL);
	testRequire(pCo != NULL, "coroutine event OOM waiter spawn failed");
	testRequire(
		xrtCoSchedStep(pSched) == XWAIT_OK,
		"coroutine event wait allocated under OOM"
	);
	testRequire(
		xrtCoState(pCo) == XCORO_SUSPENDED,
		"coroutine event OOM waiter did not suspend"
	);
	testRequire(xrtCoEventSet(&tEvent), "coroutine event set allocated under OOM");
	testRequire(xrtCoSchedRun(pSched), "coroutine event OOM scheduler run failed");
	testRequire(tWait.Result == XWAIT_OK, "coroutine event OOM wait result mismatch");

	tState.Fail = false;
	testRequire(xrtCoDestroy(pCo), "coroutine event OOM waiter destroy failed");
	testRequire(xrtCoSchedDestroy(pSched), "coroutine event OOM scheduler destroy failed");
	testRequire(xrtCoEventUnit(&tEvent), "coroutine event OOM unit failed");
	testRequire(xrtCoThreadDetach(), "coroutine event OOM runtime detach failed");

	printf("[PASS] coroutine_event_oom\n");
	return 0;
}
