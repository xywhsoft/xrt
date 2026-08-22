#include "../test.h"



/* 验证动态栈生命周期入口继承 Array 的参数与对齐契约。 */
static void testStackInitContract(void)
{
	xstack tStack;
	xstack* pCreated;

	xrtClearError();
	testRequire(
		!xrtStackInit(NULL, sizeof(int)),
		"stack NULL init target should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"stack NULL init target error mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtStackInit(&tStack, 0),
		"stack zero item size should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"stack zero item size error mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtStackInitAligned(&tStack, sizeof(int), 3),
		"stack non-power-of-two alignment should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"stack invalid alignment error mismatch"
	);

	pCreated = xrtStackCreateAligned(64, 64);
	testRequire(pCreated != NULL, "aligned stack create failed");
	testRequire(
		xrtStackAdd(pCreated) != NULL,
		"aligned created stack add failed"
	);
	testRequire(
		(((uintptr_t)pCreated->Data & 63u) == 0) &&
		(pCreated->Alignment == 64),
		"aligned created stack layout mismatch"
	);
	xrtStackDestroy(pCreated);

	xrtClearError();
	testRequire(
		xrtStackCreate(0) == NULL,
		"zero-width stack create should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"zero-width stack create error mismatch"
	);
}



/* 验证未初始化或损坏的 Array 摘要不会被栈包装层接受。 */
static void testStackStateContract(void)
{
	xstack tStack;
	int iValue = 1;

	memset(&tStack, 0, sizeof(tStack));
	xrtClearError();
	testRequire(
		xrtStackAdd(&tStack) == NULL,
		"zero stack add should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"zero stack add error mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtStackReserve(&tStack, 1),
		"zero stack reserve should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"zero stack reserve error mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtStackPush(&tStack, &iValue),
		"zero stack push should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"zero stack push error mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtStackPop(&tStack, &iValue),
		"zero stack pop should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"zero stack pop error mismatch"
	);
	xrtClearError();
	testRequire(
		xrtStackPeek(&tStack, 0) == NULL,
		"zero stack peek should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"zero stack peek error mismatch"
	);

	testRequire(
		xrtStackInit(&tStack, sizeof(int)),
		"damaged stack fixture init failed"
	);
	tStack.Count = 1;
	xrtClearError();
	testRequire(
		xrtStackConstTop(&tStack) == NULL,
		"damaged stack top should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"damaged stack top error mismatch"
	);
	tStack.Count = 0;
	xrtStackUnit(&tStack);
}



/* 验证深度访问和输出别名失败都不修改栈状态。 */
static void testStackOperationContract(void)
{
	xstack tStack;
	int pValues[3] = { 10, 20, 30 };
	size_t iCount;

	testRequire(
		xrtStackInit(&tStack, sizeof(int)),
		"stack operation fixture init failed"
	);
	for ( size_t i = 0; i < 3; i++ ) {
		testRequire(
			xrtStackPush(&tStack, &pValues[i]),
			"stack operation fixture push failed"
		);
	}
	testRequire(
		(*(int*)xrtStackPeek(&tStack, 0) == 30) &&
		(*(int*)xrtStackPeek(&tStack, 2) == 10),
		"stack depth mapping mismatch"
	);

	xrtClearError();
	testRequire(
		xrtStackPeek(&tStack, SIZE_MAX) == NULL,
		"stack excessive depth should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"stack excessive depth error mismatch"
	);

	iCount = tStack.Count;
	xrtClearError();
	testRequire(
		!xrtStackPop(&tStack, xrtStackGet(&tStack, 0)),
		"stack internal pop output should fail"
	);
	testRequire(
		(tStack.Count == iCount) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"stack internal pop output changed state"
	);
	xrtStackUnit(&tStack);
}



/* 运行连续动态栈的完整失败契约测试。 */
int main(void)
{
	testStackInitContract();
	testStackStateContract();
	testStackOperationContract();
	printf("[PASS] stack contract\n");
	return 0;
}
