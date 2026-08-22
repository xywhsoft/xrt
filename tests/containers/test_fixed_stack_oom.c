#include "../test.h"



/* 按分配尺寸注入失败，避免测试依赖堆内部调用顺序。 */
typedef struct testoomstate {
	size_t CallCount;
	size_t RejectAtLeast;
	size_t RejectCount;
} testoomstate;



/* 拒绝不小于阈值的分配，其余请求转发到 C 分配器。 */
static ptr testFailAlloc(ptr pContext, size_t iSize)
{
	testoomstate* pState = (testoomstate*)pContext;

	pState->CallCount++;
	if ( iSize >= pState->RejectAtLeast ) {
		pState->RejectCount++;
		return NULL;
	}
	return malloc(iSize);
}



/* 拒绝不小于阈值的重分配，其余请求转发到 C 分配器。 */
static ptr testFailRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	testoomstate* pState = (testoomstate*)pContext;

	pState->CallCount++;
	if ( iSize >= pState->RejectAtLeast ) {
		pState->RejectCount++;
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 失败分配器释放入口保持完整接口。 */
static void testFailFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证创建首个分配失败时返回稳定内存错误。 */
int main(void)
{
	static testoomstate tState = { 0, 1, 0 };
	xallocator tAllocator = { &tState, testFailAlloc, testFailRealloc, testFailFree };
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		xmemdebugsnapshot tBefore;
		xmemdebugsnapshot tAfter;
	#endif

	testRequire(xrtSetAllocator(&tAllocator), "failed to install fixed stack OOM allocator");
	testRequire(xrtFixedStackCreate(8, sizeof(int)) == NULL, "fixed stack create should fail under OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "fixed stack OOM error mismatch");

	/* 只拒绝大块，精确命中 Create 的内部元素缓冲分配。 */
	tState.CallCount = 0;
	tState.RejectAtLeast = 1024u * 1024u;
	tState.RejectCount = 0;
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		xrtMemDebugSnapshot(&tBefore);
	#endif
	xrtClearError();
	testRequire(
		xrtFixedStackCreate(1024u * 1024u, sizeof(int)) == NULL,
		"fixed stack buffer allocation should fail"
	);
	testRequire(
		tState.RejectCount != 0u,
		"fixed stack create did not reach target allocation"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "fixed stack buffer OOM error mismatch");
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		xrtMemDebugSnapshot(&tAfter);
		testRequire(
			(tAfter.LiveCount == tBefore.LiveCount) &&
			(tAfter.LiveBytes == tBefore.LiveBytes),
			"fixed stack buffer OOM retained logical metadata"
		);
	#endif
	printf("[PASS] fixed_stack OOM\n");
	return 0;
}
