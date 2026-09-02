#include "../test.h"

#include "../test_fault_allocator.h"



/* 执行覆盖编译、两遍 matcher 遍历和单块列表分配的拆分路径。 */
static bool testRegexSplitAttempt(xstrview Text)
{
	xregex* pRegex = xrtRegexCompile(XRT_STR_LITERAL("a"));
	xstrlist* pList = NULL;
	bool bComplete = false;

	if ( pRegex != NULL ) {
		pList = xrtRegexSplit(pRegex, Text);
	}
	if ( (pList != NULL) && (pList->Count == (Text.Size + 1u)) ) {
		bComplete = true;
	}
	xrtStrListFree(pList);
	xrtRegexRelease(pRegex);
	xrtClearError();
	return bComplete;
}



/* 扫描拆分路径全部稳定分配点并验证失败后可恢复。 */
int main(void)
{
	static testfaultallocator State = { 0, SIZE_MAX, 0, false };
	xallocator Allocator = testFaultAllocator(&State);
	char arrText[1100];
	xstrview Text;
	size_t iBaseline;
	size_t iCalls;

	memset(arrText, 'a', sizeof(arrText));
	Text = (xstrview){ arrText, sizeof(arrText) };
	testRequire(xrtSetAllocator(&Allocator), "regex split OOM allocator install failed");
	testRequire(testRegexSplitAttempt(Text), "regex split OOM warm-up failed");
	testMemoryDebugDrain("regex split OOM memory debug reset failed");
	iBaseline = State.Live;

	State.Calls = 0;
	State.FailAt = SIZE_MAX;
	testRequire(testRegexSplitAttempt(Text), "regex split OOM baseline failed");
	iCalls = State.Calls;
	testRequire(iCalls != 0, "regex split OOM fixture reached no allocation");
	testMemoryDebugDrain("regex split OOM baseline reset failed");
	testRequire(State.Live == iBaseline, "regex split OOM baseline leaked storage");

	for ( size_t iFail = 1u; iFail <= iCalls; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		State.Hit = false;
		testRequire(
			!testRegexSplitAttempt(Text),
			"regex split unexpectedly survived injected OOM"
		);
		testRequire(State.Hit, "regex split OOM target was not reached");
		testMemoryDebugDrain("regex split OOM memory debug reset failed");
		if ( State.Live != iBaseline ) {
			fprintf(
				stderr,
				"regex split OOM leak at allocation %zu: live=%zu baseline=%zu\n",
				iFail,
				State.Live,
				iBaseline
			);
		}
		testRequire(State.Live == iBaseline, "regex split OOM path leaked storage");
	}

	State.FailAt = SIZE_MAX;
	testRequire(testRegexSplitAttempt(Text), "regex split did not recover after OOM");
	testMemoryDebugDrain("regex split OOM recovery reset failed");
	testRequire(State.Live == iBaseline, "regex split recovery leaked storage");
	printf("[PASS] regex split OOM (%zu allocation points)\n", iCalls);
	return 0;
}
