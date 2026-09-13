#include "../test.h"

/* 库的逻辑分配故障钩子覆盖池化分配，不把延迟创建的堆 span 误判为泄漏。 */
int main(void)
{
	xmemdebugsnapshot Before, After;
	bool Completed = false;
	char Text[60000];
	size_t Used = 0, Points = 0;
	xvalue* pArray;
	testRequire(xrtMemDebugEnable(true), "enable memory debug");
	for ( size_t i = 0; i < 24; i++ ) {
		Text[Used++] = '"';
		for ( size_t j = 0; j < 2048; j++ ) Text[Used++] = (char)('a' + j % 26);
		Text[Used++] = '"';
		Text[Used++] = '\n';
	}
	pArray = xrtValueArray();
	testRequire(pArray != NULL, "array fixture");
	for ( size_t i = 0; i < 24; i++ ) {
		testRequire(xrtValueArrayAppendNew(pArray, xrtValueString((xstrview){ Text + 1, 2048 })), "array fixture item");
	}
	(void)Used;
	xrtMemDebugSnapshot(&Before);
	for ( uint64 i = 0; i < 4096; i++ ) {
		bool Hit;
		size_t Size = 777;
		str pResult;
		testRequire(xrtMemDebugFailAfter(i), "configure fault");
		pResult = xrtXsonlStringify(pArray, &Size);
		Hit = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( Hit ) {
			testRequire(pResult == NULL && xrtGetError() != NULL && Size == 777, "injected OOM contract");
			Points++;
		} else {
			testRequire(pResult != NULL, "successful recovery");
			Completed = true;
		}
		xrtFree(pResult);
		xrtClearError();
		xrtMemDebugSnapshot(&After);
		testRequire(After.LiveCount == Before.LiveCount &&
			After.LiveBytes == Before.LiveBytes, "OOM leaked logical allocation");
		testRequire(After.DoubleFreeCount == 0 && After.InvalidFreeCount == 0 &&
			After.OverflowCount == 0 && After.UnderflowCount == 0, "OOM memory corruption");
		testRequire(xrtValueCount(pArray) == 24, "OOM mutated source array");
		if ( Completed ) break;
	}
	testRequire(Completed && Points != 0, "fault sweep did not complete");
	xrtValueRelease(pArray);
	testRequire(xrtMemDebugReset(), "OOM left live allocations");
	printf("[PASS] XSONL write OOM (%zu logical points)\n", Points);
	return 0;
}
