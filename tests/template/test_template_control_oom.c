#include "../test.h"

#include "../test_fault_allocator.h"



/* 执行覆盖表达式、分支、循环快照和大结果的完整控制层路径。 */
static bool testTemplateControlAttempt(void)
{
	static const char sPrefix[] =
		"{#if:active and (count = 7)}ok{#else}bad{#end}"
		"{#foreach:items}{%loop.value}{#end}";
	char arrSource[5000];
	xvalue* pRoot = xrtValueObject();
	xvalue* pItems = xrtValueArray();
	xtemplate* pTemplate = NULL;
	str sOutput = NULL;
	bool bComplete = false;

	memcpy(arrSource, sPrefix, sizeof(sPrefix) - 1u);
	memset(
		arrSource + sizeof(sPrefix) - 1u,
		'x',
		sizeof(arrSource) - (sizeof(sPrefix) - 1u)
	);
	if ( (pRoot == NULL) || (pItems == NULL) ) {
		goto cleanup;
	}
	for ( size_t i = 0; i < 512u; i++ ) {
		if ( !xrtValueArrayAppendNew(pItems, xrtValueInt((int64)i)) ) {
			goto cleanup;
		}
	}
	if ( !xrtValueObjectSetNew(
		pRoot,
		XRT_STR_LITERAL("active"),
		xrtValueBool(true)
	) || !xrtValueObjectSetNew(
		pRoot,
		XRT_STR_LITERAL("count"),
		xrtValueInt(7)
	) || !xrtValueObjectSetTake(
		pRoot,
		XRT_STR_LITERAL("items"),
		&pItems
	) ) {
		goto cleanup;
	}
	pTemplate = xrtTemplateCompile(
		(xstrview){ arrSource, sizeof(arrSource) }
	);
	if ( pTemplate != NULL ) {
		sOutput = xrtTemplateRender(pTemplate, pRoot, NULL);
	}
	bComplete = sOutput != NULL;

cleanup:
	xrtFree(sOutput);
	xrtTemplateRelease(pTemplate);
	xrtValueRelease(pItems);
	xrtValueRelease(pRoot);
	xrtClearError();
	return bComplete;
}



/* 扫描控制层稳定分配点并验证失败清理与后续恢复。 */
int main(void)
{
	static testfaultallocator State = { 0, SIZE_MAX, 0, false };
	xallocator Allocator = testFaultAllocator(&State);
	size_t iBaseline;
	size_t iCalls;

	testRequire(
		xrtSetAllocator(&Allocator),
		"template control OOM allocator install failed"
	);
	testRequire(
		testTemplateControlAttempt(),
		"template control OOM warm-up failed"
	);
	testMemoryDebugDrain("template control OOM memory debug reset failed");
	iBaseline = State.Live;

	State.Calls = 0;
	State.FailAt = SIZE_MAX;
	testRequire(
		testTemplateControlAttempt(),
		"template control OOM baseline failed"
	);
	iCalls = State.Calls;
	testRequire(
		iCalls != 0,
		"template control OOM fixture reached no allocation"
	);
	testMemoryDebugDrain("template control OOM baseline reset failed");
	testRequire(
		State.Live == iBaseline,
		"template control OOM baseline leaked storage"
	);

	for ( size_t iFail = 1u; iFail <= iCalls; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		State.Hit = false;
		testRequire(
			!testTemplateControlAttempt(),
			"template control unexpectedly survived injected OOM"
		);
		testRequire(
			State.Hit,
			"template control OOM target was not reached"
		);
		testMemoryDebugDrain(
			"template control OOM memory debug reset failed"
		);
		if ( State.Live != iBaseline ) {
			fprintf(
				stderr,
				"template control OOM leak at allocation %zu: live=%zu baseline=%zu\n",
				iFail,
				State.Live,
				iBaseline
			);
		}
		testRequire(
			State.Live == iBaseline,
			"template control OOM path leaked storage"
		);
	}

	State.FailAt = SIZE_MAX;
	testRequire(
		testTemplateControlAttempt(),
		"template control did not recover after OOM"
	);
	testMemoryDebugDrain("template control OOM recovery reset failed");
	testRequire(State.Live == iBaseline, "template control recovery leaked storage");
	printf(
		"[PASS] template control OOM (%zu allocation points)\n",
		iCalls
	);
	return 0;
}
