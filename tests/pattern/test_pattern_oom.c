#include "../test.h"

#include "../test_fault_allocator.h"



/* 覆盖 Builder 解析、确定化、打包、缓存引用与捕获匹配。 */
static bool testPatternAttempt(void)
{
	xpatternbuilder* pBuilder = xrtPatternBuilderCreate();
	xpatternspec arrSpec[6];
	xpattern* pPattern = NULL;
	xpatternmatch Match;
	xstrview arrCapture[2];
	bool bComplete = false;

	memset(arrSpec, 0, sizeof(arrSpec));
	arrSpec[0].Pattern = XRT_STR_LITERAL("/a/b/c");
	arrSpec[1].Pattern = XRT_STR_LITERAL("/a/{x}/d");
	arrSpec[2].Pattern = XRT_STR_LITERAL("/api/{group}/{id}");
	arrSpec[3].Pattern = XRT_STR_LITERAL("/static/{*path}");
	arrSpec[4].Pattern = XRT_STR_LITERAL("/asset/file-{id}.json");
	arrSpec[5].Pattern = XRT_STR_LITERAL("/asset/{name}");
	if ( (pBuilder != NULL) &&
		 xrtPatternBuilderAddMany(pBuilder, arrSpec, 6u, NULL) ) {
		pPattern = xrtPatternBuilderCompile(pBuilder);
	}
	if ( (pPattern != NULL) &&
		 (xrtPatternMatch(
			pPattern,
			XRT_STR_LITERAL("/asset/file-42.json"),
			arrCapture,
			2u,
			&Match
		 ) == XPATTERN_MATCH) &&
		 (Match.CaptureCount == 1u) ) {
		bComplete = true;
	}
	xrtPatternRelease(pPattern);
	xrtPatternBuilderFree(pBuilder);
	xrtClearError();
	return bComplete;
}



int main(void)
{
	static testfaultallocator State = { 0, SIZE_MAX, 0, false };
	xallocator Allocator = testFaultAllocator(&State);
	size_t iBaseline;
	size_t iCalls;

	testRequire(xrtSetAllocator(&Allocator), "pattern OOM allocator install failed");
	testRequire(testPatternAttempt(), "pattern OOM warm-up failed");
	testMemoryDebugDrain("pattern OOM warm-up reset failed");
	iBaseline = State.Live;

	State.Calls = 0;
	State.FailAt = SIZE_MAX;
	testRequire(testPatternAttempt(), "pattern OOM baseline failed");
	iCalls = State.Calls;
	testRequire(iCalls != 0, "pattern OOM fixture reached no allocation");
	testMemoryDebugDrain("pattern OOM baseline reset failed");
	testRequire(State.Live == iBaseline, "pattern OOM baseline leaked storage");

	for ( size_t iFail = 1u; iFail <= iCalls; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		State.Hit = false;
		testRequire(
			!testPatternAttempt(),
			"pattern unexpectedly survived injected OOM"
		);
		testRequire(State.Hit, "pattern OOM target was not reached");
		testMemoryDebugDrain("pattern OOM path reset failed");
		if ( State.Live != iBaseline ) {
			fprintf(
				stderr,
				"pattern OOM leak at allocation %zu: live=%zu baseline=%zu\n",
				iFail,
				State.Live,
				iBaseline
			);
		}
		testRequire(State.Live == iBaseline, "pattern OOM path leaked storage");
	}

	State.FailAt = SIZE_MAX;
	testRequire(testPatternAttempt(), "pattern did not recover after OOM");
	testMemoryDebugDrain("pattern OOM recovery reset failed");
	testRequire(State.Live == iBaseline, "pattern OOM recovery leaked storage");
	printf("[PASS] pattern OOM (%zu allocation points)\n", iCalls);
	return 0;
}
