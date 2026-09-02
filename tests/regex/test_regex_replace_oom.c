#include "../test.h"

#include "../test_fault_allocator.h"



/* 执行覆盖编译、模板解析、matcher、捕获展开和输出增长的替换路径。 */
static bool testRegexReplaceAttempt(xstrview Pattern, xstrview Text)
{
	xregex* pRegex = xrtRegexCompile(Pattern);
	xstrbuf Output;
	bool bComplete = false;

	xrtStrBufInit(&Output);
	if ( (pRegex != NULL) && xrtRegexReplaceTo(
		pRegex,
		Text,
		XRT_STR_LITERAL("$0$0"),
		SIZE_MAX,
		&Output,
		NULL
	) && (Output.Size == (Text.Size * 2u)) ) {
		bComplete = true;
	}
	xrtStrBufFree(&Output);
	xrtRegexRelease(pRegex);
	xrtClearError();
	return bComplete;
}



/* 扫描替换路径全部稳定分配点并验证失败后可恢复。 */
int main(void)
{
	static testfaultallocator State = { 0, SIZE_MAX, 0, false };
	xallocator Allocator = testFaultAllocator(&State);
	char arrPattern[1102];
	char arrText[1100];
	xstrview Pattern;
	xstrview Text;
	size_t iBaseline;
	size_t iCalls;

	arrPattern[0] = '(';
	memset(arrPattern + 1, 'a', sizeof(arrText));
	arrPattern[sizeof(arrPattern) - 1u] = ')';
	memset(arrText, 'a', sizeof(arrText));
	Pattern = (xstrview){ arrPattern, sizeof(arrPattern) };
	Text = (xstrview){ arrText, sizeof(arrText) };
	testRequire(xrtSetAllocator(&Allocator), "regex replace OOM allocator install failed");
	testRequire(testRegexReplaceAttempt(Pattern, Text), "regex replace OOM warm-up failed");
	testMemoryDebugDrain("regex replace OOM memory debug reset failed");
	iBaseline = State.Live;

	State.Calls = 0;
	State.FailAt = SIZE_MAX;
	testRequire(testRegexReplaceAttempt(Pattern, Text), "regex replace OOM baseline failed");
	iCalls = State.Calls;
	testRequire(iCalls != 0, "regex replace OOM fixture reached no allocation");
	testMemoryDebugDrain("regex replace OOM baseline reset failed");
	testRequire(State.Live == iBaseline, "regex replace OOM baseline leaked storage");

	for ( size_t iFail = 1u; iFail <= iCalls; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		State.Hit = false;
		testRequire(
			!testRegexReplaceAttempt(Pattern, Text),
			"regex replace unexpectedly survived injected OOM"
		);
		testRequire(State.Hit, "regex replace OOM target was not reached");
		testMemoryDebugDrain("regex replace OOM memory debug reset failed");
		if ( State.Live != iBaseline ) {
			fprintf(
				stderr,
				"regex replace OOM leak at allocation %zu: live=%zu baseline=%zu\n",
				iFail,
				State.Live,
				iBaseline
			);
		}
		testRequire(State.Live == iBaseline, "regex replace OOM path leaked storage");
	}

	State.FailAt = SIZE_MAX;
	testRequire(testRegexReplaceAttempt(Pattern, Text), "regex replace did not recover after OOM");
	testMemoryDebugDrain("regex replace OOM recovery reset failed");
	testRequire(State.Live == iBaseline, "regex replace recovery leaked storage");
	printf("[PASS] regex replace OOM (%zu allocation points)\n", iCalls);
	return 0;
}
