#include "../test.h"

#include "../test_fault_allocator.h"



/* 执行覆盖定义索引、表达式、raw 和结果扩容的组合层路径。 */
static bool testTemplateComposeAttempt(void)
{
	static const char sSource[] =
		"{#define:'row'}[{$name}]{#end}"
		"{#for:1:64}{#include:'row'}{#end}"
		"{#raw}{$ignored}{#end}";
	char arrSource[8192];
	xvalue* pRoot = xrtValueObject();
	xtemplate* pTemplate = NULL;
	str sOutput = NULL;
	bool bComplete = false;

	if ( pRoot == NULL ) {
		goto cleanup;
	}
	memcpy(arrSource, sSource, sizeof(sSource) - 1u);
	memset(
		arrSource + sizeof(sSource) - 1u,
		'x',
		sizeof(arrSource) - (sizeof(sSource) - 1u)
	);
	if ( !xrtValueObjectSetNew(
		pRoot,
		XRT_STR_LITERAL("name"),
		xrtValueString(XRT_STR_LITERAL("Alice"))
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
	xrtValueRelease(pRoot);
	xrtClearError();
	return bComplete;
}



/* 扫描组合层稳定分配点并验证失败清理与后续恢复。 */
int main(void)
{
	static testfaultallocator State = { 0, SIZE_MAX, 0, false };
	xallocator Allocator = testFaultAllocator(&State);
	size_t iBaseline;
	size_t iCalls;

	testRequire(
		xrtSetAllocator(&Allocator),
		"template compose OOM allocator install failed"
	);
	testRequire(
		testTemplateComposeAttempt(),
		"template compose OOM warm-up failed"
	);
	testMemoryDebugDrain("template compose OOM memory debug reset failed");
	iBaseline = State.Live;

	State.Calls = 0;
	State.FailAt = SIZE_MAX;
	testRequire(
		testTemplateComposeAttempt(),
		"template compose OOM baseline failed"
	);
	iCalls = State.Calls;
	testRequire(iCalls != 0, "template compose reached no allocation");
	testMemoryDebugDrain("template compose OOM baseline reset failed");
	testRequire(
		State.Live == iBaseline,
		"template compose OOM baseline leaked storage"
	);

	for ( size_t iFail = 1u; iFail <= iCalls; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		State.Hit = false;
		testRequire(
			!testTemplateComposeAttempt(),
			"template compose unexpectedly survived injected OOM"
		);
		testRequire(
			State.Hit,
			"template compose OOM target was not reached"
		);
		testMemoryDebugDrain(
			"template compose OOM memory debug reset failed"
		);
		testRequire(
			State.Live == iBaseline,
			"template compose OOM path leaked storage"
		);
	}

	State.FailAt = SIZE_MAX;
	testRequire(
		testTemplateComposeAttempt(),
		"template compose did not recover after OOM"
	);
	testMemoryDebugDrain("template compose OOM recovery reset failed");
	testRequire(State.Live == iBaseline, "template compose recovery leaked storage");
	printf(
		"[PASS] template compose OOM (%zu allocation points)\n",
		iCalls
	);
	return 0;
}
