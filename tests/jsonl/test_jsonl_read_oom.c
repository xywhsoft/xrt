#include "../test.h"

/* 库的逻辑分配故障钩子覆盖池化分配，不把延迟创建的堆 span 误判为泄漏。 */
int main(void)
{
	xmemdebugsnapshot Before, After;
	bool Completed = false;
	char Text[60000];
	size_t Used = 0, Points = 0;

	testRequire(xrtMemDebugEnable(true), "enable memory debug");
	for ( size_t i = 0; i < 24; i++ ) {
		Text[Used++] = '"';
		for ( size_t j = 0; j < 2048; j++ ) Text[Used++] = (char)('a' + j % 26);
		Text[Used++] = '"';
		Text[Used++] = '\n';
	}

	xrtMemDebugSnapshot(&Before);
	for ( uint64 i = 0; i < 4096; i++ ) {
		bool Hit;
		xvalue* pResult;
		testRequire(xrtMemDebugFailAfter(i), "configure fault");
		pResult = xrtJsonlParse((xstrview){ Text, Used });
		Hit = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( Hit ) {
			testRequire(pResult == NULL && xrtGetError() != NULL, "injected OOM contract");
			Points++;
		} else {
			testRequire(pResult != NULL, "successful recovery");
			Completed = true;
		}
		xrtValueRelease(pResult);
		xrtClearError();
		xrtMemDebugSnapshot(&After);
		testRequire(After.LiveCount == Before.LiveCount &&
			After.LiveBytes == Before.LiveBytes, "OOM leaked logical allocation");
		testRequire(After.DoubleFreeCount == 0 && After.InvalidFreeCount == 0 &&
			After.OverflowCount == 0 && After.UnderflowCount == 0, "OOM memory corruption");

		if ( Completed ) break;
	}
	testRequire(Completed && Points != 0, "fault sweep did not complete");

	testRequire(xrtMemDebugReset(), "OOM left live allocations");
	printf("[PASS] JSONL read OOM (%zu logical points)\n", Points);
	return 0;
}
