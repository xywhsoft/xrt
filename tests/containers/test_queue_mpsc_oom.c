#include "../test.h"



/* 按分配尺寸注入失败，避免测试依赖堆内部调用顺序。 */
typedef struct testoomstate {
	size_t Calls;
	size_t RejectAtLeast;
	size_t Rejects;
} testoomstate;



/* 拒绝不小于阈值的分配，其余请求转发到 C 分配器。 */
static ptr testOomAlloc(ptr pContext, size_t iSize)
{
	testoomstate* pState = (testoomstate*)pContext;

	pState->Calls++;
	if ( iSize >= pState->RejectAtLeast ) {
		pState->Rejects++;
		return NULL;
	}
	return malloc(iSize);
}



/* 拒绝不小于阈值的重分配，其余请求转发到 C 分配器。 */
static ptr testOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	testoomstate* pState = (testoomstate*)pContext;

	pState->Calls++;
	if ( iSize >= pState->RejectAtLeast ) {
		pState->Rejects++;
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放测试分配器取得的内存。 */
static void testOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证拥有型失败清理和外部槽环的零分配路径。 */
int main(void)
{
	static testoomstate tState = { 0, 1, 0 };
	xallocator tAllocator = { &tState, testOomAlloc, testOomRealloc, testOomFree };
	xmpscqueue tQueue;
	xqueueslot pStorage[4];

	testRequire(xrtSetAllocator(&tAllocator), "failed to install MPSC OOM allocator");
	testRequire(!xrtMPSCQueueInit(&tQueue, 8u), "MPSC init should fail under OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "MPSC init OOM error mismatch");
	testRequire(tQueue.Slots == NULL, "failed MPSC init did not preserve zero state");

	/* 外部槽环初始化在运行期不接触分配器。 */
	tState.Calls = 0;
	tState.Rejects = 0;
	xrtClearError();
	testRequire(
		xrtMPSCQueueInitBuffer(&tQueue, pStorage, 4u),
		"external MPSC should survive allocator failure"
	);
	testRequire(tState.Calls == 0u, "external MPSC unexpectedly allocated memory");
	xrtMPSCQueueUnit(&tQueue);

	/* 只拒绝大块，精确命中 Create 的内部序号槽环分配。 */
	tState.Calls = 0;
	tState.RejectAtLeast = 1024u * 1024u;
	tState.Rejects = 0;
	xrtClearError();
	testRequire(
		xrtMPSCQueueCreate(1024u * 1024u) == NULL,
		"MPSC create slot allocation should fail"
	);
	testRequire(tState.Rejects != 0u, "MPSC create did not reach target allocation");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "MPSC create OOM error mismatch");
	printf("[PASS] queue_mpsc_oom\n");
	return 0;
}
