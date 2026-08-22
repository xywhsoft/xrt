#include "../test.h"



/* 测试分配器可以在指定底层调用之后持续返回内存不足。 */
typedef struct testcancelwatchoom {
	size_t Count;
	size_t AllowThrough;
} testcancelwatchoom;



/* 在允许范围内分配底层存储，超过故障边界后持续失败。 */
static ptr testCancelWatchOomAlloc(ptr pContext, size_t iSize)
{
	testcancelwatchoom* pState = (testcancelwatchoom*)pContext;

	pState->Count++;
	return pState->Count <= pState->AllowThrough ?
		malloc(iSize) : NULL;
}



/* 测试分配器保持标准重分配语义。 */
static ptr testCancelWatchOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	(void)pContext;
	return realloc(pMemory, iSize);
}



/* 释放测试分配器实际拥有的底层 span。 */
static void testCancelWatchOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* OOM 用例中的回调不应被注册或执行。 */
static void testCancelWatchOomCallback(ptr pData)
{
	int* pCount = (int*)pData;

	(*pCount)++;
}



/* 验证监听分配失败会归还令牌引用且不留下链表节点。 */
int main(void)
{
	static testcancelwatchoom tState = { 0, SIZE_MAX };
	xallocator tAllocator = {
		&tState,
		testCancelWatchOomAlloc,
		testCancelWatchOomRealloc,
		testCancelWatchOomFree
	};
	xcancel* pCancel;
	xcancelwatch* pWatch;
	ptr pWarm;
	int iCount = 0;

	testRequire(xrtSetAllocator(&tAllocator), "failed to install cancel watch OOM allocator");

	/* 先建立线程堆缓存，并避开取消对象使用的尺寸类。 */
	pWarm = xrtMalloc(1024u);
	testRequire(pWarm != NULL, "cancel watch heap warm-up failed");
	xrtFree(pWarm);

	/* 只允许取消令牌对象所属 span 建立成功。 */
	tState.AllowThrough = tState.Count + 1u;
	pCancel = xrtCancelCreate();
	testRequire(pCancel != NULL, "cancel token allocation failed too early");

	/* 后续底层分配全部失败，使故障确定落在监听对象。 */
	tState.AllowThrough = tState.Count;
	xrtClearError();
	testRequire(
		xrtCancelWatch(pCancel, testCancelWatchOomCallback, &iCount) == NULL,
		"cancel watch succeeded under OOM"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "cancel watch OOM error mismatch");
	testRequire(xrtCancelRequest(pCancel), "cancel request failed after watch OOM");
	testRequire(iCount == 0, "failed cancel watch left a callback behind");

	/* 解除故障后，已取消令牌必须立即触发新监听一次。 */
	xrtClearError();
	tState.AllowThrough = SIZE_MAX;
	pWatch = xrtCancelWatch(pCancel, testCancelWatchOomCallback, &iCount);
	testRequire(pWatch != NULL, "cancel watch recovery after OOM failed");
	testRequire(iCount == 1, "recovered cancel watch callback count mismatch");
	xrtCancelUnwatch(pWatch);
	xrtCancelDestroy(pCancel);

	printf("[PASS] cancel watch OOM\n");
	return 0;
}
