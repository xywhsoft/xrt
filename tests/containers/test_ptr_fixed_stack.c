#include "../test.h"



/* 验证固定指针栈的容量、空值、类型和对象所有权合同。 */
int main(void)
{
	xptrfixedstack tStack;
	xptrfixedstack* pCreated;
	xfixedstack tWrongStack;
	ptr pStorage[3];
	unsigned char pWrongStorage[sizeof(ptr) * 4u];
	int pValues[] = { 10, 20, 30 };
	ptr pValue = (ptr)(uintptr_t)1;

	testRequire(
		xrtPtrFixedStackInit(&tStack, pStorage, 3),
		"fixed pointer stack init failed"
	);
	testRequire(xrtPtrFixedStackSpace(&tStack) == 3, "fixed pointer stack space mismatch");
	testRequire(xrtPtrFixedStackPush(&tStack, &pValues[0]), "fixed pointer first push failed");
	testRequire(xrtPtrFixedStackPush(&tStack, NULL), "fixed pointer NULL push failed");
	testRequire(xrtPtrFixedStackPush(&tStack, &pValues[2]), "fixed pointer third push failed");
	testRequire(xrtPtrFixedStackGet(&tStack, 0) == &pValues[0], "fixed pointer get mismatch");
	testRequire(xrtPtrFixedStackTop(&tStack) == &pValues[2], "fixed pointer top mismatch");

	xrtClearError();
	testRequire(
		!xrtPtrFixedStackPush(&tStack, &pValues[1]),
		"full fixed pointer stack should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_AGAIN, "fixed pointer full error mismatch");
	testRequire(xrtPtrFixedStackPop(&tStack, &pValue), "fixed pointer pop failed");
	testRequire(pValue == &pValues[2], "fixed pointer pop value mismatch");

	xrtClearError();
	testRequire(xrtPtrFixedStackPeek(&tStack, 0) == NULL, "fixed pointer NULL peek mismatch");
	testRequire(xrtGetError() == NULL, "valid fixed pointer NULL reported error");
	testRequire(xrtPtrFixedStackPop(&tStack, &pValue), "fixed pointer NULL pop failed");
	testRequire(pValue == NULL, "fixed pointer NULL pop value mismatch");
	testRequire(xrtPtrFixedStackPop(&tStack, NULL), "fixed pointer discard pop failed");

	xrtClearError();
	testRequire(!xrtPtrFixedStackPop(&tStack, &pValue), "empty fixed pointer pop should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "empty fixed pointer error mismatch");
	xrtPtrFixedStackClear(&tStack);
	xrtPtrFixedStackUnit(&tStack);

	pCreated = xrtPtrFixedStackCreate(4);
	testRequire(pCreated != NULL, "fixed pointer stack create failed");
	xrtPtrFixedStackDestroy(pCreated);

	/* 类型化薄层必须拒绝元素宽度不等于指针的 FixedStack。 */
	testRequire(
		xrtFixedStackInit(
			&tWrongStack,
			pWrongStorage,
			sizeof(pWrongStorage),
			sizeof(ptr) * 2u
		),
		"wrong-width fixed stack init failed"
	);
	xrtClearError();
	testRequire(
		!xrtPtrFixedStackPush((xptrfixedstack*)&tWrongStack, &pValues[0]),
		"fixed pointer stack should reject wrong item size"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"fixed pointer wrong item size error mismatch"
	);
	xrtFixedStackUnit(&tWrongStack);

	xrtClearError();
	testRequire(
		!xrtPtrFixedStackInit(&tStack, pStorage, SIZE_MAX),
		"fixed pointer capacity overflow should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"fixed pointer capacity overflow error mismatch"
	);
	printf("[PASS] ptr_fixed_stack\n");
	return 0;
}
