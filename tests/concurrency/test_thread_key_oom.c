#include "../test.h"



/* 测试分配器可以在指定底层调用之后持续返回内存不足。 */
typedef struct testkeyoom {
	size_t Count;
	size_t AllowThrough;
} testkeyoom;



/* 在允许范围内分配底层存储，超过故障边界后持续失败。 */
static ptr testThreadKeyOomAlloc(ptr pContext, size_t iSize)
{
	testkeyoom* pState = (testkeyoom*)pContext;

	pState->Count++;
	return pState->Count <= pState->AllowThrough ?
		malloc(iSize) : NULL;
}



/* 测试分配器保持标准重分配语义。 */
static ptr testThreadKeyOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	(void)pContext;
	return realloc(pMemory, iSize);
}



/* 释放测试分配器实际拥有的底层 span。 */
static void testThreadKeyOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证线程状态和值槽分配失败都不会接管用户值或破坏键。 */
int main(void)
{
	static testkeyoom tState = { 0, SIZE_MAX };
	xallocator tAllocator = {
		&tState,
		testThreadKeyOomAlloc,
		testThreadKeyOomRealloc,
		testThreadKeyOomFree
	};
	xthreadkey* pKey;
	ptr pWarm;
	int iValue = 7;

	testRequire(xrtSetAllocator(&tAllocator), "failed to install thread key OOM allocator");

	/* 先建立线程堆缓存，并避开线程键使用的小尺寸类。 */
	pWarm = xrtMalloc(1024u);
	testRequire(pWarm != NULL, "thread key heap warm-up failed");
	xrtFree(pWarm);

	/* 只允许线程键对象所属 span 建立成功。 */
	tState.AllowThrough = tState.Count + 1u;
	pKey = xrtThreadKeyCreate(NULL);
	testRequire(pKey != NULL, "thread key object allocation failed too early");

	/* 不再允许建立新尺寸类 span，验证线程状态分配失败。 */
	xrtClearError();
	testRequire(!xrtThreadKeySet(pKey, &iValue), "thread key slot succeeded under OOM");
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"thread key state OOM error mismatch"
	);
	testRequire(xrtThreadKeyGet(pKey) == NULL, "failed thread key state remained installed");

	/* 只允许状态尺寸类建立成功，让值槽尺寸类失败并验证状态回滚。 */
	xrtClearError();
	tState.AllowThrough = tState.Count + 1u;
	testRequire(!xrtThreadKeySet(pKey, &iValue), "thread key slot succeeded under OOM");
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"thread key slot OOM error mismatch"
	);
	testRequire(xrtThreadKeyGet(pKey) == NULL, "failed thread key slot remained installed");

	/* 解除故障后必须可以正常设置并取走同一个值。 */
	xrtClearError();
	tState.AllowThrough = SIZE_MAX;
	testRequire(xrtThreadKeySet(pKey, &iValue), "thread key recovery after OOM failed");
	testRequire(xrtThreadKeyGet(pKey) == &iValue, "thread key recovery value mismatch");
	testRequire(xrtThreadKeyTake(pKey) == &iValue, "thread key take after OOM failed");
	testRequire(xrtThreadKeyDestroy(pKey), "thread key destroy failed after slot OOM");

	printf("[PASS] thread key OOM\n");
	return 0;
}
