#include "../test.h"



/* 64 字节对象用于验证动态栈显式过对齐。 */
typedef struct teststackaligned {
	unsigned char Data[64];
} teststackaligned;



/* 验证动态增长、LIFO、自引用和别名安全。 */
static void testStackBasic(void)
{
	xstack tStack;
	xstack* pCreated;
	int iPopped;
	int* pFirst;

	testRequire(xrtStackInit(&tStack, sizeof(int)), "stack init failed");
	testRequire(xrtStackReserve(&tStack, 8), "stack reserve failed");
	for ( int i = 0; i < 1000; i++ ) {
		testRequire(xrtStackPush(&tStack, &i), "stack push failed");
	}
	testRequire((tStack.Count == 1000) && (*(int*)xrtStackTop(&tStack) == 999), "stack top mismatch");
	testRequire(*(int*)xrtStackGet(&tStack, 7) == 7, "stack get mismatch");
	testRequire(*(const int*)xrtStackConstPeek(&tStack, 2) == 997, "stack peek mismatch");

	/* 裁剪到精确深度，确保下一次自引用 Push 必须经过扩容。 */
	testRequire(xrtStackTrim(&tStack), "stack pre-alias trim failed");
	testRequire(tStack.Capacity == tStack.Count, "stack pre-alias capacity mismatch");
	pFirst = (int*)xrtArrayGet(&tStack, 0);
	testRequire(xrtStackPush(&tStack, pFirst), "stack self push failed");
	testRequire(*(int*)xrtStackTop(&tStack) == 0, "stack self push value mismatch");

	/* Pop 输出不得覆盖栈内另一活动元素。 */
	xrtClearError();
	testRequire(!xrtStackPop(&tStack, xrtArrayGet(&tStack, 1)), "stack alias pop should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "stack alias pop error mismatch");
	testRequire(tStack.Count == 1001, "stack alias pop changed count");
	testRequire(xrtStackPop(&tStack, &iPopped), "stack pop failed");
	testRequire((iPopped == 0) && (tStack.Count == 1000), "stack pop result mismatch");
	testRequire(xrtStackPop(&tStack, NULL), "stack discard pop failed");
	testRequire(*(int*)xrtStackConstTop(&tStack) == 998, "stack const top mismatch");

	xrtStackClear(&tStack);
	testRequire((tStack.Count == 0) && (tStack.Capacity >= 1000), "stack clear mismatch");
	testRequire(xrtStackTrim(&tStack), "stack trim failed");
	testRequire((tStack.Data == NULL) && (tStack.Capacity == 0), "stack trim state mismatch");
	xrtClearError();
	testRequire(xrtStackTop(&tStack) == NULL, "empty stack top should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "empty stack top error mismatch");
	xrtClearError();
	testRequire(xrtStackGet(&tStack, 0) == NULL, "empty stack get should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "empty stack get error mismatch");
	xrtStackUnit(&tStack);

	pCreated = xrtStackCreate(sizeof(int));
	testRequire(pCreated != NULL, "stack create failed");
	xrtStackDestroy(pCreated);
}



/* 验证未初始化槽和显式过对齐入口。 */
static void testStackAddAndAlignment(void)
{
	xstack tStack;
	int* pValue;

	testRequire(xrtStackInit(&tStack, sizeof(int)), "stack add init failed");
	pValue = (int*)xrtStackAdd(&tStack);
	testRequire(pValue != NULL, "stack add failed");
	*pValue = 42;
	testRequire(*(int*)xrtStackTop(&tStack) == 42, "stack add value mismatch");
	xrtStackUnit(&tStack);

	testRequire(
		xrtStackInitAligned(&tStack, sizeof(teststackaligned), 64),
		"aligned stack init failed"
	);
	for ( size_t i = 0; i < 20; i++ ) {
		teststackaligned* pItem = (teststackaligned*)xrtStackAdd(&tStack);

		testRequire(pItem != NULL, "aligned stack add failed");
		testRequire(((uintptr_t)pItem & 63u) == 0, "stack item alignment mismatch");
	}
	xrtStackUnit(&tStack);
}



/* 运行动态栈完整合同测试。 */
int main(void)
{
	testStackBasic();
	testStackAddAndAlignment();
	printf("[PASS] stack\n");
	return 0;
}
