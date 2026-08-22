#include "../test.h"



/* JSON 读取故障注入器记录底层调用、命中点和仍存活的原始块。 */
typedef struct testjsonreadallocator {
	size_t Calls;
	size_t FailAt;
	size_t Live;
	bool Hit;
} testjsonreadallocator;



/* 在指定底层分配序号失败，其余请求交给 C 运行库。 */
static ptr testJsonReadAlloc(ptr pContext, size_t iSize)
{
	testjsonreadallocator* pState = (testjsonreadallocator*)pContext;
	ptr pMemory;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		pState->Hit = true;
		return NULL;
	}
	pMemory = malloc(iSize);
	if ( pMemory != NULL ) {
		pState->Live++;
	}
	return pMemory;
}



/* 重分配失败时保留原块，只在从空指针创建块时增加存活计数。 */
static ptr testJsonReadRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testjsonreadallocator* pState = (testjsonreadallocator*)pContext;
	ptr pResult;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		pState->Hit = true;
		return NULL;
	}
	pResult = realloc(pMemory, iSize);
	if ( (pResult != NULL) && (pMemory == NULL) ) {
		pState->Live++;
	}
	return pResult;
}



/* 释放底层块并维护存活计数。 */
static void testJsonReadFree(ptr pContext, ptr pMemory)
{
	testjsonreadallocator* pState = (testjsonreadallocator*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	testRequire(pState->Live != 0, "JSON read OOM live counter underflow");
	pState->Live--;
	free(pMemory);
}



/* 构造同时经过反转义缓冲、长字符串和容器分配的合法 JSON。 */
static size_t testJsonReadBuild(char* sText, size_t iCapacity)
{
	static const char sPrefix[] = "{\"escaped\":\"";
	static const char sMiddle[] = "\",\"plain\":\"";
	static const char sSuffix[] = "\",\"items\":[0,1,2,3,4,5,6,7]}";
	size_t iSize = 0;

	testRequire(iCapacity > 6000u, "JSON read OOM fixture is too small");
	memcpy(sText + iSize, sPrefix, sizeof(sPrefix) - 1u);
	iSize += sizeof(sPrefix) - 1u;
	for ( size_t i = 0; i < 400u; i++ ) {
		memcpy(sText + iSize, "\\u4E00", 6u);
		iSize += 6u;
	}
	memcpy(sText + iSize, sMiddle, sizeof(sMiddle) - 1u);
	iSize += sizeof(sMiddle) - 1u;
	memset(sText + iSize, 'x', 2048u);
	iSize += 2048u;
	memcpy(sText + iSize, sSuffix, sizeof(sSuffix) - 1u);
	iSize += sizeof(sSuffix) - 1u;
	return iSize;
}



/* 执行一次完整 DOM 读取，并在成功或失败后释放全部可见状态。 */
static bool testJsonReadAttempt(xstrview Text)
{
	xvalue* pValue = xrtJsonParse(Text);
	bool bComplete = pValue != NULL;

	xrtValueRelease(pValue);
	xrtClearError();
	return bComplete;
}



/* 扫描所有稳定底层分配点，要求失败可恢复且没有原始块泄漏。 */
int main(void)
{
	static testjsonreadallocator State = { 0, SIZE_MAX, 0, false };
	xallocator Allocator;
	char Text[8192];
	xstrview Json;
	size_t iBaseline;
	size_t iCalls;

	Allocator.Context = &State;
	Allocator.Alloc = testJsonReadAlloc;
	Allocator.Realloc = testJsonReadRealloc;
	Allocator.Free = testJsonReadFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"JSON read OOM allocator install failed"
	);
	Json.Data = Text;
	Json.Size = testJsonReadBuild(Text, sizeof(Text));

	/* 预热小对象尺寸类，并确认纯验证成功路径不需要动态内存。 */
	testRequire(testJsonReadAttempt(Json), "JSON read OOM warm-up failed");
	testMemoryDebugDrain("JSON read OOM memory debug reset failed");
	iBaseline = State.Live;
	State.Calls = 0;
	State.FailAt = 1u;
	State.Hit = false;
	testRequire(
		xrtJsonValid(XRT_STR_LITERAL("{\"ok\":[1,true,null]}")),
		"JSON allocation-free validation failed"
	);
	testRequire(
		(State.Calls == 0) && !State.Hit,
		"JSON validation allocated on a valid input"
	);

	/* 成功路径的底层调用数定义当前稳定扫描区间。 */
	State.Calls = 0;
	State.FailAt = SIZE_MAX;
	testRequire(testJsonReadAttempt(Json), "JSON read OOM baseline failed");
	iCalls = State.Calls;
	testRequire(iCalls != 0, "JSON read OOM fixture reached no allocation");
	testMemoryDebugDrain("JSON read OOM baseline reset failed");
	testRequire(State.Live == iBaseline, "JSON read baseline leaked storage");

	for ( size_t iFail = 1u; iFail <= iCalls; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		State.Hit = false;
		testRequire(
			!testJsonReadAttempt(Json),
			"JSON read unexpectedly survived injected OOM"
		);
		testRequire(State.Hit, "JSON read OOM target was not reached");
		testMemoryDebugDrain("JSON read OOM memory debug reset failed");
		testRequire(State.Live == iBaseline, "JSON read OOM leaked storage");
	}

	State.FailAt = SIZE_MAX;
	testRequire(testJsonReadAttempt(Json), "JSON read did not recover after OOM");
	testMemoryDebugDrain("JSON read OOM recovery reset failed");
	testRequire(State.Live == iBaseline, "JSON read recovery leaked storage");
	printf("[PASS] JSON read OOM\n");
	return 0;
}
