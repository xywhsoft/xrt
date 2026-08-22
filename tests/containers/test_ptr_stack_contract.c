#include "../test.h"



/* 验证空结构、越界访问和合法空指针值的区别。 */
static void testPtrStackContractBoundaries(void)
{
	xptrstack tStack;
	ptr pOutput = (ptr)(uintptr_t)1;

	xrtClearError();
	testRequire(
		!xrtPtrStackInit(NULL),
		"NULL pointer stack init should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"NULL pointer stack init error mismatch"
	);

	testRequire(xrtPtrStackInit(&tStack), "pointer stack boundary init failed");
	xrtClearError();
	testRequire(
		xrtPtrStackGet(&tStack, SIZE_MAX) == NULL,
		"oversized pointer stack index should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"oversized pointer stack index error mismatch"
	);

	xrtClearError();
	testRequire(
		xrtPtrStackPeek(&tStack, SIZE_MAX) == NULL,
		"oversized pointer stack depth should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"oversized pointer stack depth error mismatch"
	);

	testRequire(
		xrtPtrStackPush(&tStack, NULL),
		"pointer stack NULL push failed"
	);
	xrtClearError();
	testRequire(
		xrtPtrStackGet(&tStack, 0) == NULL,
		"pointer stack stored NULL get mismatch"
	);
	testRequire(
		xrtGetError() == NULL,
		"pointer stack stored NULL get reported error"
	);
	testRequire(
		xrtPtrStackPop(&tStack, &pOutput) && (pOutput == NULL),
		"pointer stack stored NULL pop mismatch"
	);

	xrtPtrStackUnit(&tStack);
}



/* 验证所有类型化入口都拒绝非指针宽度动态栈。 */
static void testPtrStackContractType(void)
{
	xstack tWrong;
	unsigned char pValue[sizeof(ptr) * 2u] = { 0 };
	size_t iCount;

	testRequire(
		xrtStackInit(&tWrong, sizeof(pValue)),
		"wrong-width pointer stack fixture init failed"
	);
	testRequire(
		xrtStackPush(&tWrong, pValue),
		"wrong-width pointer stack fixture push failed"
	);
	iCount = tWrong.Count;

	xrtClearError();
	xrtPtrStackClear((xptrstack*)&tWrong);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(tWrong.Count == iCount),
		"pointer stack clear accepted wrong item size"
	);

	xrtClearError();
	testRequire(
		!xrtPtrStackReserve((xptrstack*)&tWrong, 8),
		"pointer stack reserve accepted wrong item size"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"pointer stack reserve wrong-width error mismatch"
	);

	xrtClearError();
	testRequire(
		!xrtPtrStackTrim((xptrstack*)&tWrong),
		"pointer stack trim accepted wrong item size"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"pointer stack trim wrong-width error mismatch"
	);

	xrtClearError();
	testRequire(
		xrtPtrStackGet((const xptrstack*)&tWrong, 0) == NULL,
		"pointer stack get accepted wrong item size"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"pointer stack get wrong-width error mismatch"
	);

	xrtClearError();
	testRequire(
		!xrtPtrStackPush((xptrstack*)&tWrong, NULL),
		"pointer stack push accepted wrong item size"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"pointer stack push wrong-width error mismatch"
	);

	xrtClearError();
	testRequire(
		!xrtPtrStackPop((xptrstack*)&tWrong, NULL),
		"pointer stack pop accepted wrong item size"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"pointer stack pop wrong-width error mismatch"
	);

	xrtClearError();
	testRequire(
		xrtPtrStackPeek((const xptrstack*)&tWrong, 0) == NULL,
		"pointer stack peek accepted wrong item size"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"pointer stack peek wrong-width error mismatch"
	);

	xrtClearError();
	testRequire(
		xrtPtrStackTop((const xptrstack*)&tWrong) == NULL,
		"pointer stack top accepted wrong item size"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"pointer stack top wrong-width error mismatch"
	);
	testRequire(
		tWrong.Count == iCount,
		"wrong-width pointer stack operation changed source stack"
	);

	xrtStackUnit(&tWrong);
}



/* 验证 Pop 拒绝覆盖仍活动的内部指针槽。 */
static void testPtrStackContractAlias(void)
{
	xptrstack tStack;
	int pValues[] = { 10, 20 };
	ptr* pInternal;

	testRequire(xrtPtrStackInit(&tStack), "pointer stack alias init failed");
	testRequire(
		xrtPtrStackPush(&tStack, &pValues[0]) &&
		xrtPtrStackPush(&tStack, &pValues[1]),
		"pointer stack alias fixture push failed"
	);
	pInternal = (ptr*)xrtStackGet(&tStack, 0);
	testRequire(pInternal != NULL, "pointer stack alias slot missing");

	xrtClearError();
	testRequire(
		!xrtPtrStackPop(&tStack, pInternal),
		"pointer stack internal pop output should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"pointer stack internal pop output error mismatch"
	);
	testRequire(
		(tStack.Count == 2) &&
		(xrtPtrStackGet(&tStack, 0) == &pValues[0]) &&
		(xrtPtrStackTop(&tStack) == &pValues[1]),
		"pointer stack internal pop output changed state"
	);

	xrtPtrStackUnit(&tStack);
}



/* 运行动态指针栈边界、类型和别名契约测试。 */
int main(void)
{
	testPtrStackContractBoundaries();
	testPtrStackContractType();
	testPtrStackContractAlias();
	printf("[PASS] ptr_stack contract\n");
	return 0;
}
