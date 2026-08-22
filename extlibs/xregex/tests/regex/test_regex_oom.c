#include "../test.h"

#include "../test_fault_allocator.h"


/* 执行覆盖编译、matcher 创建、搜索和 full match 的完整路径。 */
static bool testRegexAttempt(xstrview Pattern, xstrview Text)
{
	str sEscaped = xrtRegexEscape(Pattern, NULL);
	xregex* pRegex = sEscaped != NULL ?
		xrtRegexCompile(xrtStrViewN(sEscaped, Pattern.Size)) : NULL;
	xregexmatcher* pMatcher = NULL;
	bool bComplete = false;

	if ( pRegex != NULL ) {
		pMatcher = xrtRegexMatcherCreate(pRegex);
	}
	if ( (pMatcher != NULL) &&
		 (xrtRegexMatcherFind(pMatcher, Text, 0) == XREGEX_MATCH) &&
		 (xrtRegexMatcherFull(pMatcher, Text) == XREGEX_MATCH) ) {
		bComplete = true;
	}
	xrtRegexMatcherFree(pMatcher);
	xrtRegexRelease(pRegex);
	xrtFree(sEscaped);
	xrtClearError();
	return bComplete;
}



/* 扫描全部稳定分配点，要求失败可恢复且没有底层块泄漏。 */
int main(void)
{
	static testfaultallocator State = { 0, SIZE_MAX, 0, false };
	xallocator Allocator;
	char arrPattern[1100];
	char arrText[1100];
	xstrview Pattern;
	xstrview Text;
	size_t iBaseline;
	size_t iCalls;

	Allocator = testFaultAllocator(&State);
	memset(arrPattern, 'a', sizeof(arrPattern));
	memset(arrText, 'a', sizeof(arrText));
	Pattern = (xstrview){ arrPattern, sizeof(arrPattern) };
	Text = (xstrview){ arrText, sizeof(arrText) };
	testRequire(xrtSetAllocator(&Allocator), "regex OOM allocator install failed");
	testRequire(testRegexAttempt(Pattern, Text), "regex OOM warm-up failed");
	testMemoryDebugDrain("regex OOM memory debug reset failed");
	iBaseline = State.Live;

	State.Calls = 0;
	State.FailAt = SIZE_MAX;
	testRequire(testRegexAttempt(Pattern, Text), "regex OOM baseline failed");
	iCalls = State.Calls;
	testRequire(iCalls != 0, "regex OOM fixture reached no allocation");
	testMemoryDebugDrain("regex OOM baseline reset failed");
	testRequire(State.Live == iBaseline, "regex OOM baseline leaked storage");

	for ( size_t iFail = 1u; iFail <= iCalls; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		State.Hit = false;
		testRequire(
			!testRegexAttempt(Pattern, Text),
			"regex unexpectedly survived injected OOM"
		);
		testRequire(State.Hit, "regex OOM target was not reached");
		testMemoryDebugDrain("regex OOM memory debug reset failed");
		testRequire(State.Live == iBaseline, "regex OOM path leaked storage");
	}

	State.FailAt = SIZE_MAX;
	testRequire(testRegexAttempt(Pattern, Text), "regex did not recover after OOM");
	testMemoryDebugDrain("regex OOM recovery reset failed");
	testRequire(State.Live == iBaseline, "regex recovery leaked storage");
	printf("[PASS] regex OOM (%zu allocation points)\n", iCalls);
	return 0;
}
