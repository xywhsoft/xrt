#include "../test.h"



/* XSON 读取故障注入器记录底层调用、命中点和仍存活的原始块。 */
typedef struct testxsonreadallocator {
	size_t Calls;
	size_t FailAt;
	size_t Live;
	bool Hit;
} testxsonreadallocator;



/* 在指定底层分配序号失败，其余请求交给 C 运行库。 */
static ptr testXsonReadAlloc(ptr pContext, size_t iSize)
{
	testxsonreadallocator* pState = (testxsonreadallocator*)pContext;
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
static ptr testXsonReadRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testxsonreadallocator* pState = (testxsonreadallocator*)pContext;
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
static void testXsonReadFree(ptr pContext, ptr pMemory)
{
	testxsonreadallocator* pState = (testxsonreadallocator*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	testRequire(pState->Live != 0, "XSON read OOM live counter underflow");
	pState->Live--;
	free(pMemory);
}



/* 构造经过反转义、Base64、整数映射、集合和容器扩容的合法 XSON。 */
static size_t testXsonReadBuild(char* sText, size_t iCapacity)
{
	static const char sPrefix[] =
		"{\"escaped\":\"";
	static const char sMiddle[] =
		"\",\"blob\":bytes(\"AAECAwQFBgcICQ==\"),"
		"\"map\":intmap{-7:\"x\",9:time(\"2026-07-31T08:00:00Z\")},"
		"\"set\":set[0,1,2,3,4,5,6,7],\"plain\":\"";
	static const char sSuffix[] = "\"}";
	size_t iSize = 0;

	testRequire(iCapacity > 6000u, "XSON read OOM fixture is too small");
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
static bool testXsonReadAttempt(xstrview Text)
{
	xvalue* pValue = xrtXsonParse(Text);
	bool bComplete = pValue != NULL;

	xrtValueRelease(pValue);
	xrtClearError();
	return bComplete;
}



/* 扫描所有稳定底层分配点，要求失败可恢复且没有原始块泄漏。 */
int main(void)
{
	static testxsonreadallocator State = { 0, SIZE_MAX, 0, false };
	xallocator Allocator;
	char Text[8192];
	xstrview Xson;
	size_t iBaseline;
	size_t iCalls;

	Allocator.Context = &State;
	Allocator.Alloc = testXsonReadAlloc;
	Allocator.Realloc = testXsonReadRealloc;
	Allocator.Free = testXsonReadFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"XSON read OOM allocator install failed"
	);
	Xson.Data = Text;
	Xson.Size = testXsonReadBuild(Text, sizeof(Text));

	/* 预热尺寸类，再以成功路径定义当前稳定扫描区间。 */
	testRequire(testXsonReadAttempt(Xson), "XSON read OOM warm-up failed");
	testMemoryDebugDrain("XSON read OOM memory debug reset failed");
	iBaseline = State.Live;
	State.Calls = 0;
	State.FailAt = SIZE_MAX;
	testRequire(testXsonReadAttempt(Xson), "XSON read OOM baseline failed");
	iCalls = State.Calls;
	testRequire(iCalls != 0, "XSON read OOM fixture reached no allocation");
	testMemoryDebugDrain("XSON read OOM baseline reset failed");
	testRequire(State.Live == iBaseline, "XSON read baseline leaked storage");

	for ( size_t iFail = 1u; iFail <= iCalls; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		State.Hit = false;
		testRequire(
			!testXsonReadAttempt(Xson),
			"XSON read unexpectedly survived injected OOM"
		);
		testRequire(State.Hit, "XSON read OOM target was not reached");
		testMemoryDebugDrain("XSON read OOM memory debug reset failed");
		testRequire(State.Live == iBaseline, "XSON read OOM leaked storage");
	}

	State.FailAt = SIZE_MAX;
	testRequire(testXsonReadAttempt(Xson), "XSON read did not recover after OOM");
	testMemoryDebugDrain("XSON read OOM recovery reset failed");
	testRequire(State.Live == iBaseline, "XSON read recovery leaked storage");
	printf("[PASS] XSON read OOM\n");
	return 0;
}
