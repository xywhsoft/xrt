#include "../test.h"



/* JSON 写出故障注入器记录底层调用、命中点和仍存活的原始块。 */
typedef struct testjsonwriteallocator {
	size_t Calls;
	size_t FailAt;
	size_t Live;
	bool Hit;
} testjsonwriteallocator;



/* 在指定底层分配序号失败，其余请求交给 C 运行库。 */
static ptr testJsonWriteAlloc(ptr pContext, size_t iSize)
{
	testjsonwriteallocator* pState = (testjsonwriteallocator*)pContext;
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
static ptr testJsonWriteRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testjsonwriteallocator* pState = (testjsonwriteallocator*)pContext;
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
static void testJsonWriteFree(ptr pContext, ptr pMemory)
{
	testjsonwriteallocator* pState = (testjsonwriteallocator*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	testRequire(pState->Live != 0, "JSON write OOM live counter underflow");
	pState->Live--;
	free(pMemory);
}



/* 深层增量写入器的 sink 立即消费借用字节且不再分配。 */
static bool testJsonWriteSink(xbytesview Data, ptr pUserData)
{
	(void)Data;
	(void)pUserData;
	return true;
}



/* 同时覆盖内存输出扩容、结果移交和增量容器帧扩容。 */
static bool testJsonWriteAttempt(const xvalue* pValue)
{
	xjsonwriteconfig Config;
	xjsonwriter* pWriter = NULL;
	str sText = NULL;
	size_t iSize = SIZE_MAX;
	bool bComplete = false;

	sText = xrtJsonStringify(pValue, false, &iSize);
	if ( sText == NULL ) {
		testRequire(iSize == SIZE_MAX, "JSON stringify OOM changed result size");
		goto done;
	}
	xrtFree(sText);
	sText = NULL;

	xrtJsonWriteConfigInit(&Config);
	pWriter = xrtJsonWriterCreateSink(&Config, testJsonWriteSink, NULL);
	if ( pWriter == NULL ) {
		goto done;
	}
	for ( size_t i = 0; i < 256u; i++ ) {
		if ( !xrtJsonWriterArray(pWriter) ) {
			goto done;
		}
	}
	if ( !xrtJsonWriterNull(pWriter) ) {
		goto done;
	}
	for ( size_t i = 0; i < 256u; i++ ) {
		if ( !xrtJsonWriterEnd(pWriter) ) {
			goto done;
		}
	}
	if ( !xrtJsonWriterFinish(pWriter) ) {
		goto done;
	}
	bComplete = true;

done:
	xrtFree(sText);
	xrtJsonWriterFree(pWriter);
	xrtClearError();
	return bComplete;
}



/* 扫描所有稳定底层分配点，要求失败可恢复且没有原始块泄漏。 */
int main(void)
{
	static testjsonwriteallocator State = { 0, SIZE_MAX, 0, false };
	xallocator Allocator;
	char Text[4096];
	xvalue* pValue;
	size_t iBaseline;
	size_t iCalls;

	Allocator.Context = &State;
	Allocator.Alloc = testJsonWriteAlloc;
	Allocator.Realloc = testJsonWriteRealloc;
	Allocator.Free = testJsonWriteFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"JSON write OOM allocator install failed"
	);
	memset(Text, 'x', sizeof(Text));
	pValue = xrtValueString((xstrview){ Text, sizeof(Text) });
	testRequire(pValue != NULL, "JSON write OOM fixture create failed");

	/* 预热小对象尺寸类，再以成功路径定义稳定扫描区间。 */
	testRequire(testJsonWriteAttempt(pValue), "JSON write OOM warm-up failed");
	xrtValueRelease(pValue);
	testMemoryDebugDrain("JSON write OOM memory debug reset failed");
	iBaseline = State.Live;

	State.FailAt = SIZE_MAX;
	pValue = xrtValueString((xstrview){ Text, sizeof(Text) });
	testRequire(pValue != NULL, "JSON write OOM baseline fixture create failed");
	State.Calls = 0;
	testRequire(testJsonWriteAttempt(pValue), "JSON write OOM baseline failed");
	iCalls = State.Calls;
	testRequire(iCalls != 0, "JSON write OOM fixture reached no allocation");
	xrtValueRelease(pValue);
	testMemoryDebugDrain("JSON write OOM baseline reset failed");
	testRequire(State.Live == iBaseline, "JSON write baseline leaked storage");

	for ( size_t iFail = 1u; iFail <= iCalls; iFail++ ) {
		State.FailAt = SIZE_MAX;
		pValue = xrtValueString((xstrview){ Text, sizeof(Text) });
		testRequire(pValue != NULL, "JSON write OOM fixture recreate failed");
		State.Calls = 0;
		State.FailAt = iFail;
		State.Hit = false;
		testRequire(
			!testJsonWriteAttempt(pValue),
			"JSON write unexpectedly survived injected OOM"
		);
		testRequire(State.Hit, "JSON write OOM target was not reached");
		xrtValueRelease(pValue);
		testMemoryDebugDrain("JSON write OOM memory debug reset failed");
		testRequire(State.Live == iBaseline, "JSON write OOM leaked storage");
	}

	State.FailAt = SIZE_MAX;
	pValue = xrtValueString((xstrview){ Text, sizeof(Text) });
	testRequire(pValue != NULL, "JSON write recovery fixture create failed");
	testRequire(
		testJsonWriteAttempt(pValue),
		"JSON write did not recover after OOM"
	);
	xrtValueRelease(pValue);
	testMemoryDebugDrain("JSON write OOM recovery reset failed");
	testRequire(State.Live == iBaseline, "JSON write recovery leaked storage");
	xrtClearError();
	printf("[PASS] JSON write OOM\n");
	return 0;
}
