#include "../test.h"



/* 验证旧版覆盖过的定宽标量不会因栈封装改变宽度。 */
static void testTypedStackWidths(void)
{
	xtypedstack Stack;
	int8 iSmall = -7;
	int8 iSmallOut = 0;
	uint16 iWide = 60000u;
	uint16 iWideOut = 0u;
	float fReal = 1.25f;
	float fRealOut = 0.0f;

	testRequire(
		xrtTypedStackInit(&Stack, xrtTypeInt8()) &&
		xrtTypedStackPush(&Stack, &iSmall) &&
		xrtTypedStackPop(&Stack, &iSmallOut) &&
		(iSmallOut == iSmall),
		"typed stack int8 width mismatch"
	);
	xrtTypedStackUnit(&Stack);
	testRequire(
		xrtTypedStackInit(&Stack, xrtTypeUInt16()) &&
		xrtTypedStackPush(&Stack, &iWide) &&
		xrtTypedStackPop(&Stack, &iWideOut) &&
		(iWideOut == iWide),
		"typed stack uint16 width mismatch"
	);
	xrtTypedStackUnit(&Stack);
	testRequire(
		xrtTypedStackInit(&Stack, xrtTypeFloat32()) &&
		xrtTypedStackPush(&Stack, &fReal) &&
		xrtTypedStackPop(&Stack, &fRealOut) &&
		(fRealOut == fReal),
		"typed stack float32 width mismatch"
	);
	xrtTypedStackUnit(&Stack);
}



/* 验证基础后进先出操作、自引用压入、克隆和丢弃式弹出。 */
static void testTypedStackOperations(void)
{
	xtypedstack Stack;
	xtypedstack* pClone;
	int64 iFirst = 11;
	int64 iSecond = 22;
	int64 iOutput = 0;
	const int64* pTop;

	testRequire(
		xrtTypedStackInit(&Stack, xrtTypeInt64()) &&
		xrtTypedStackReserve(&Stack, 2u) &&
		xrtTypedStackPush(&Stack, &iFirst) &&
		xrtTypedStackPush(&Stack, &iSecond),
		"typed stack fixture failed"
	);
	pTop = (const int64*)xrtTypedStackConstTop(&Stack);
	testRequire(
		(pTop != NULL) && (*pTop == iSecond) &&
		xrtTypedStackPush(&Stack, pTop) &&
		(xrtTypedStackCount(&Stack) == 3u) &&
		(*(const int64*)xrtTypedStackConstPeek(&Stack, 1u) == iSecond),
		"typed stack top, peek, or self push failed"
	);
	pClone = xrtTypedStackClone(&Stack);
	testRequire(
		(pClone != NULL) && xrtTypedStackEquals(&Stack, pClone),
		"typed stack clone failed"
	);
	testRequire(
		xrtTypedStackPop(&Stack, &iOutput) && (iOutput == iSecond) &&
		xrtTypedStackPop(&Stack, NULL) &&
		(xrtTypedStackCount(&Stack) == 1u) &&
		(*(const int64*)xrtTypedStackTop(&Stack) == iFirst),
		"typed stack pop failed"
	);
	testRequire(
		!xrtTypedStackEquals(&Stack, pClone) &&
		xrtTypedStackTrim(&Stack) &&
		(xrtTypedStackCapacity(&Stack) == 1u),
		"typed stack trim or equality failed"
	);
	xrtTypedStackClear(&Stack);
	testRequire(
		(xrtTypedStackCount(&Stack) == 0u) &&
		!xrtTypedStackPop(&Stack, NULL),
		"typed stack empty behavior mismatch"
	);
	xrtTypedStackDestroy(pClone);
	xrtTypedStackUnit(&Stack);
}



/* 运行类型栈常规与旧资产回归测试。 */
int main(void)
{
	testTypedStackWidths();
	testTypedStackOperations();
	xrtClearError();
	printf("[PASS] typed stack\n");
	return 0;
}
