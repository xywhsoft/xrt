#include "../test.h"



/* 验证指针栈薄封装、空指针值和目标所有权合同。 */
int main(void)
{
	xptrstack tStack;
	xptrstack* pCreated;
	xstack tWrongStack;
	int pValues[] = { 10, 20, 30 };
	ptr pValue = (ptr)(uintptr_t)1;

	testRequire(xrtPtrStackInit(&tStack), "pointer stack init failed");
	testRequire(xrtPtrStackReserve(&tStack, 4), "pointer stack reserve failed");
	testRequire(xrtPtrStackPush(&tStack, &pValues[0]), "pointer stack first push failed");
	testRequire(xrtPtrStackPush(&tStack, NULL), "pointer stack NULL push failed");
	testRequire(xrtPtrStackPush(&tStack, &pValues[2]), "pointer stack third push failed");
	testRequire(xrtPtrStackTop(&tStack) == &pValues[2], "pointer stack top mismatch");
	testRequire(xrtPtrStackGet(&tStack, 0) == &pValues[0], "pointer stack get mismatch");
	xrtClearError();
	testRequire(xrtPtrStackPeek(&tStack, 1) == NULL, "pointer stack NULL peek mismatch");
	testRequire(xrtGetError() == NULL, "valid pointer stack NULL peek reported error");
	testRequire(xrtPtrStackPop(&tStack, &pValue), "pointer stack pop failed");
	testRequire(pValue == &pValues[2], "pointer stack pop value mismatch");

	xrtClearError();
	testRequire(xrtPtrStackTop(&tStack) == NULL, "pointer stack stored NULL mismatch");
	testRequire(xrtGetError() == NULL, "valid pointer stack NULL reported error");
	testRequire(xrtPtrStackPop(&tStack, &pValue), "pointer stack NULL pop failed");
	testRequire(pValue == NULL, "pointer stack NULL pop value mismatch");
	testRequire(xrtPtrStackPop(&tStack, NULL), "pointer stack discard pop failed");
	xrtClearError();
	testRequire(!xrtPtrStackPop(&tStack, &pValue), "empty pointer stack pop should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "empty pointer stack error mismatch");

	testRequire(xrtPtrStackTrim(&tStack), "pointer stack trim failed");
	xrtPtrStackClear(&tStack);
	xrtPtrStackUnit(&tStack);
	pCreated = xrtPtrStackCreate();
	testRequire(pCreated != NULL, "pointer stack create failed");
	xrtPtrStackDestroy(pCreated);

	/* 类型化薄封装必须拒绝其他元素宽度，不能越界读取局部指针。 */
	testRequire(
		xrtStackInit(&tWrongStack, sizeof(ptr) * 2u),
		"wrong-width stack init failed"
	);
	xrtClearError();
	testRequire(
		!xrtPtrStackPush((xptrstack*)&tWrongStack, &pValues[0]),
		"pointer stack should reject wrong item size"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"pointer stack wrong item size error mismatch"
	);
	testRequire(tWrongStack.Count == 0, "pointer stack wrong item size changed stack");
	xrtStackUnit(&tWrongStack);
	printf("[PASS] ptr_stack\n");
	return 0;
}
