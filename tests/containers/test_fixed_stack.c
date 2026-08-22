#include "../test.h"



/* 验证外部缓冲、固定容量、LIFO 和安全别名合同。 */
static void testFixedStackExternal(void)
{
	xfixedstack tStack;
	int pStorage[4];
	int pValues[] = { 10, 20, 30 };
	int iPopped = 0;
	int* pSlot;

	testRequire(
		xrtFixedStackInit(&tStack, pStorage, sizeof(pStorage), sizeof(int)),
		"fixed stack init failed"
	);
	testRequire((tStack.Count == 0) && (tStack.Capacity == 4), "fixed stack initial state mismatch");
	testRequire(xrtFixedStackSpace(&tStack) == 4, "fixed stack initial space mismatch");
	testRequire(xrtFixedStackPush(&tStack, &pValues[0]), "fixed stack first push failed");
	testRequire(xrtFixedStackPush(&tStack, &pValues[1]), "fixed stack second push failed");
	testRequire(*(int*)xrtFixedStackTop(&tStack) == 20, "fixed stack top mismatch");
	testRequire(*(int*)xrtFixedStackGet(&tStack, 0) == 10, "fixed stack get mismatch");
	testRequire(*(const int*)xrtFixedStackConstPeek(&tStack, 1) == 10, "fixed stack peek mismatch");

	/* 活动元素可以安全复制到新栈顶，来源必须是完整元素。 */
	testRequire(xrtFixedStackPush(&tStack, &pStorage[0]), "fixed stack self push failed");
	testRequire(*(int*)xrtFixedStackTop(&tStack) == 10, "fixed stack self push value mismatch");
	pSlot = (int*)xrtFixedStackAdd(&tStack);
	testRequire(pSlot != NULL, "fixed stack add failed");
	*pSlot = 40;
	testRequire((tStack.Count == 4) && (xrtFixedStackSpace(&tStack) == 0), "fixed stack full state mismatch");

	xrtClearError();
	testRequire(!xrtFixedStackPush(&tStack, &pValues[2]), "fixed stack overflow should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_AGAIN, "fixed stack overflow error mismatch");
	testRequire((tStack.Count == 4) && (*(int*)xrtFixedStackTop(&tStack) == 40), "fixed stack overflow changed state");

	/* 输出写入同一栈缓冲会覆盖其他活动元素，必须拒绝。 */
	xrtClearError();
	testRequire(!xrtFixedStackPop(&tStack, &pStorage[0]), "fixed stack alias pop should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "fixed stack alias pop error mismatch");
	testRequire(tStack.Count == 4, "fixed stack alias pop changed count");
	testRequire(xrtFixedStackPop(&tStack, &iPopped), "fixed stack pop failed");
	testRequire((iPopped == 40) && (tStack.Count == 3), "fixed stack pop result mismatch");
	testRequire(xrtFixedStackPop(&tStack, NULL), "fixed stack discard pop failed");

	xrtFixedStackClear(&tStack);
	testRequire((tStack.Count == 0) && (tStack.Capacity == 4), "fixed stack clear mismatch");
	xrtClearError();
	testRequire(xrtFixedStackTop(&tStack) == NULL, "empty fixed stack top should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "empty fixed stack error mismatch");
	xrtFixedStackUnit(&tStack);
	testRequire((tStack.Data == NULL) && (tStack.Capacity == 0), "fixed stack unit mismatch");
}



/* 验证拥有缓冲的创建入口和尺寸边界。 */
static void testFixedStackCreate(void)
{
	xfixedstack* pStack;
	int iValue = 7;

	pStack = xrtFixedStackCreate(8, sizeof(int));
	testRequire(pStack != NULL, "fixed stack create failed");
	testRequire((pStack->Allocation != NULL) && (pStack->Capacity == 8), "created fixed stack state mismatch");
	testRequire(xrtFixedStackPush(pStack, &iValue), "created fixed stack push failed");
	xrtFixedStackDestroy(pStack);

	xrtClearError();
	testRequire(xrtFixedStackCreate(SIZE_MAX, 2) == NULL, "fixed stack overflow create should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "fixed stack overflow create error mismatch");
	testRequire(
		!xrtFixedStackInit(&(xfixedstack){ 0 }, &iValue, sizeof(iValue), 0),
		"zero item size fixed stack should fail"
	);

	/* 人工构造的环绕地址必须在初始化时被拒绝。 */
	xrtClearError();
	testRequire(
		!xrtFixedStackInit(
			&(xfixedstack){ 0 },
			(ptr)(UINTPTR_MAX - 1u),
			sizeof(int),
			sizeof(int)
		),
		"wrapping fixed stack address should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"wrapping fixed stack address error mismatch"
	);
}



/* 运行固定容量栈完整合同测试。 */
int main(void)
{
	testFixedStackExternal();
	testFixedStackCreate();
	printf("[PASS] fixed_stack\n");
	return 0;
}
