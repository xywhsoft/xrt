#include "../test.h"

#include "../test_fault_allocator.h"



/* 执行覆盖批量编译、集合构建、matcher 创建和首次匹配的路径。 */
static bool testRegexSetAttempt(const xstrview* arrPattern, xstrview Text)
{
	xregexset* pSet = xrtRegexSetCompile(arrPattern, 2u);
	xregexsetmatcher* pMatcher = NULL;
	bool bComplete = false;

	if ( pSet != NULL ) {
		pMatcher = xrtRegexSetMatcherCreate(pSet);
	}
	if ( (pMatcher != NULL) &&
		 (xrtRegexSetMatcherMatch(pMatcher, Text, 0) == XREGEX_MATCH) &&
		 (xrtRegexSetMatcherCount(pMatcher) == 1u) &&
		 (xrtRegexSetMatcherFirst(pMatcher) == 0) ) {
		bComplete = true;
	}
	xrtRegexSetMatcherFree(pMatcher);
	xrtRegexSetRelease(pSet);
	xrtClearError();
	return bComplete;
}



/* 扫描集合路径的全部稳定分配点并验证失败后可恢复。 */
int main(void)
{
	static testfaultallocator State = { 0, SIZE_MAX, 0, false };
	xallocator Allocator = testFaultAllocator(&State);
	char arrPatternA[1100];
	char arrPatternB[1100];
	char arrText[1100];
	xstrview arrPattern[2];
	xstrview Text;
	size_t iBaseline;
	size_t iCalls;

	memset(arrPatternA, 'a', sizeof(arrPatternA));
	memset(arrPatternB, 'a', sizeof(arrPatternB));
	arrPatternB[sizeof(arrPatternB) - 1u] = 'b';
	memset(arrText, 'a', sizeof(arrText));
	arrPattern[0] = (xstrview){ arrPatternA, sizeof(arrPatternA) };
	arrPattern[1] = (xstrview){ arrPatternB, sizeof(arrPatternB) };
	Text = (xstrview){ arrText, sizeof(arrText) };
	testRequire(xrtSetAllocator(&Allocator), "regex set OOM allocator install failed");
	testRequire(testRegexSetAttempt(arrPattern, Text), "regex set OOM warm-up failed");
	testMemoryDebugDrain("regex set OOM memory debug reset failed");
	iBaseline = State.Live;

	State.Calls = 0;
	State.FailAt = SIZE_MAX;
	testRequire(testRegexSetAttempt(arrPattern, Text), "regex set OOM baseline failed");
	iCalls = State.Calls;
	testRequire(iCalls != 0, "regex set OOM fixture reached no allocation");
	testMemoryDebugDrain("regex set OOM baseline reset failed");
	testRequire(State.Live == iBaseline, "regex set OOM baseline leaked storage");

	for ( size_t iFail = 1u; iFail <= iCalls; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		State.Hit = false;
		testRequire(
			!testRegexSetAttempt(arrPattern, Text),
			"regex set unexpectedly survived injected OOM"
		);
		testRequire(State.Hit, "regex set OOM target was not reached");
		testMemoryDebugDrain("regex set OOM memory debug reset failed");
		if ( State.Live != iBaseline ) {
			fprintf(
				stderr,
				"regex set OOM leak at allocation %zu: live=%zu baseline=%zu\n",
				iFail,
				State.Live,
				iBaseline
			);
		}
		testRequire(State.Live == iBaseline, "regex set OOM path leaked storage");
	}

	State.FailAt = SIZE_MAX;
	testRequire(testRegexSetAttempt(arrPattern, Text), "regex set did not recover after OOM");
	testMemoryDebugDrain("regex set OOM recovery reset failed");
	testRequire(State.Live == iBaseline, "regex set recovery leaked storage");
	printf("[PASS] regex set OOM (%zu allocation points)\n", iCalls);
	return 0;
}
