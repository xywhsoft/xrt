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
	xmpmcqueue tQueue;
	xqueueslot pStorage[4];

	testRequire(xrtSetAllocator(&tAllocator), "failed to install MPMC OOM allocator");
	testRequire(!xrtMPMCQueueInit(&tQueue, 8u), "MPMC init should fail under OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "MPMC init OOM error mismatch");
	testRequire(tQueue.Slots == NULL, "failed MPMC init did not preserve zero state");

	tState.Calls = 0;
	tState.Rejects = 0;
	xrtClearError();
	testRequire(xrtMPMCQueueInitBuffer(&tQueue, pStorage, 4u), "external MPMC should survive allocator failure");
	testRequire(tState.Calls == 0u, "external MPMC unexpectedly allocated memory");
	xrtMPMCQueueUnit(&tQueue);

	/* 只拒绝大块，精确命中 Create 的内部序列槽环分配。 */
	tState.Calls = 0;
	tState.RejectAtLeast = 1024u * 1024u;
	tState.Rejects = 0;
	xrtClearError();
	testRequire(xrtMPMCQueueCreate(1024u * 1024u) == NULL, "MPMC create slot allocation should fail");
	testRequire(tState.Rejects != 0u, "MPMC create did not reach target allocation");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "MPMC create OOM error mismatch");
	printf("[PASS] queue_mpmc_oom\n");
	return 0;
}
