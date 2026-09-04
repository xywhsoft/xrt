#include "../test.h"
#include "../test_thread.h"



#define TEST_PATTERN_THREAD_COUNT 6
#define TEST_PATTERN_ITERATIONS 50000



typedef struct test_pattern_thread_context {
	xpattern* Pattern;
	size_t Index;
} test_pattern_thread_context;



/* 所有线程共享同一个不可变快照，捕获缓冲由线程独占。 */
static int testPatternThreadRun(ptr pData)
{
	test_pattern_thread_context* pContext =
		(test_pattern_thread_context*)pData;
	char arrText[64];
	int iSize = snprintf(
		arrText,
		sizeof(arrText),
		"/api/resource%zu/value",
		pContext->Index
	);
	xstrview Text = { arrText, (size_t)iSize };

	for ( size_t i = 0; i < TEST_PATTERN_ITERATIONS; i++ ) {
		xpatternmatch Match;
		xstrview Capture;

		if ( xrtPatternMatch(
			pContext->Pattern,
			Text,
			&Capture,
			1u,
			&Match
		) != XPATTERN_MATCH ) {
			return 1;
		}
		if ( (Match.Value != (ptr)(uintptr_t)(pContext->Index + 1u)) ||
			 (Capture.Size != 5u) ||
			 (memcmp(Capture.Data, "value", 5u) != 0) ) {
			return 2;
		}
	}
	return 0;
}



int main(void)
{
	char arrPattern[TEST_PATTERN_THREAD_COUNT][64];
	xpatternspec arrSpec[TEST_PATTERN_THREAD_COUNT];
	xpattern* pPattern;
	test_pattern_thread_context arrContext[TEST_PATTERN_THREAD_COUNT];
	testthread arrThread[TEST_PATTERN_THREAD_COUNT];

	memset(arrSpec, 0, sizeof(arrSpec));
	memset(arrThread, 0, sizeof(arrThread));
	for ( size_t i = 0; i < TEST_PATTERN_THREAD_COUNT; i++ ) {
		int iSize = snprintf(
			arrPattern[i],
			sizeof(arrPattern[i]),
			"/api/resource%zu/{id}",
			i
		);

		arrSpec[i].Pattern = (xstrview){ arrPattern[i], (size_t)iSize };
		arrSpec[i].Value = (ptr)(uintptr_t)(i + 1u);
	}
	pPattern = xrtPatternCompileMany(arrSpec, TEST_PATTERN_THREAD_COUNT);
	testRequire(pPattern != NULL, "thread pattern compile failed");
	for ( size_t i = 0; i < TEST_PATTERN_THREAD_COUNT; i++ ) {
		arrContext[i].Pattern = pPattern;
		arrContext[i].Index = i;
		arrThread[i].Proc = testPatternThreadRun;
		arrThread[i].Data = &arrContext[i];
	}
	testThreadsStart(arrThread, TEST_PATTERN_THREAD_COUNT);
	testThreadsJoin(arrThread, TEST_PATTERN_THREAD_COUNT);
	for ( size_t i = 0; i < TEST_PATTERN_THREAD_COUNT; i++ ) {
		testRequire(arrThread[i].Result == 0, "concurrent pattern match failed");
	}
	xrtPatternRelease(pPattern);
	printf("[PASS] pattern threads\n");
	return 0;
}
