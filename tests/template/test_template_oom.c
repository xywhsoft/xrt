#include "../test.h"

#include "../test_fault_allocator.h"



/* 执行覆盖值构造、模板编译、长格式化和结果分配的完整路径。 */
static bool testTemplateAttempt(void)
{
	static const char arrTag[] = "{%value:0200d}";
	char arrSource[4096];
	xvalue* pData = xrtValueObject();
	xtemplate* pTemplate = NULL;
	str sOutput = NULL;
	size_t iSize = 0;
	size_t iExpected = sizeof(arrSource) - (sizeof(arrTag) - 1u) + 200u;
	bool bComplete = false;

	memset(arrSource, 'x', sizeof(arrSource));
	memcpy(arrSource + 100u, arrTag, sizeof(arrTag) - 1u);
	if ( (pData != NULL) && xrtValueObjectSetNew(
		pData,
		XRT_STR_LITERAL("value"),
		xrtValueInt(42)
	) ) {
		pTemplate = xrtTemplateCompile(
			(xstrview){ arrSource, sizeof(arrSource) }
		);
	}
	if ( pTemplate != NULL ) {
		sOutput = xrtTemplateRender(pTemplate, pData, &iSize);
	}
	if ( (sOutput != NULL) && (iSize == iExpected) ) {
		bComplete = true;
	}
	xrtFree(sOutput);
	xrtTemplateRelease(pTemplate);
	xrtValueRelease(pData);
	xrtClearError();
	return bComplete;
}



/* 扫描模板核心路径全部稳定分配点并验证失败后可恢复。 */
int main(void)
{
	static testfaultallocator State = { 0, SIZE_MAX, 0, false };
	xallocator Allocator = testFaultAllocator(&State);
	size_t iBaseline;
	size_t iCalls;

	testRequire(
		xrtSetAllocator(&Allocator),
		"template OOM allocator install failed"
	);
	testRequire(testTemplateAttempt(), "template OOM warm-up failed");
	testMemoryDebugDrain("template OOM memory debug reset failed");
	iBaseline = State.Live;

	State.Calls = 0;
	State.FailAt = SIZE_MAX;
	testRequire(testTemplateAttempt(), "template OOM baseline failed");
	iCalls = State.Calls;
	testRequire(iCalls != 0, "template OOM fixture reached no allocation");
	testMemoryDebugDrain("template OOM baseline reset failed");
	testRequire(
		State.Live == iBaseline,
		"template OOM baseline leaked storage"
	);

	for ( size_t iFail = 1u; iFail <= iCalls; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		State.Hit = false;
		testRequire(
			!testTemplateAttempt(),
			"template unexpectedly survived injected OOM"
		);
		testRequire(State.Hit, "template OOM target was not reached");
		testMemoryDebugDrain("template OOM memory debug reset failed");
		if ( State.Live != iBaseline ) {
			fprintf(
				stderr,
				"template OOM leak at allocation %zu: live=%zu baseline=%zu\n",
				iFail,
				State.Live,
				iBaseline
			);
		}
		testRequire(
			State.Live == iBaseline,
			"template OOM path leaked storage"
		);
	}

	State.FailAt = SIZE_MAX;
	testRequire(
		testTemplateAttempt(),
		"template did not recover after OOM"
	);
	testMemoryDebugDrain("template OOM recovery reset failed");
	testRequire(State.Live == iBaseline, "template recovery leaked storage");
	printf("[PASS] template OOM (%zu allocation points)\n", iCalls);
	return 0;
}
