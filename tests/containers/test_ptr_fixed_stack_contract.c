#include "../test.h"



/* 验证固定指针栈初始化的参数、尺寸和重叠边界。 */
static void testPtrFixedStackInitContract(void)
{
	union {
		xptrfixedstack Alignment;
		unsigned char Storage[sizeof(xptrfixedstack) + sizeof(ptr)];
	} tMemory;
	xptrfixedstack tStack;
	ptr pStorage[2];

	xrtClearError();
	testRequire(
		!xrtPtrFixedStackInit(NULL, pStorage, SIZE_MAX),
		"fixed pointer stack NULL target should fail first"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"fixed pointer stack NULL target error mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtPtrFixedStackInit(&tStack, NULL, SIZE_MAX),
		"fixed pointer stack NULL memory should fail first"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"fixed pointer stack NULL memory error mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtPtrFixedStackInit(&tStack, pStorage, 0),
		"fixed pointer stack zero capacity should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"fixed pointer stack zero capacity error mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtPtrFixedStackInit(&tStack, pStorage, SIZE_MAX),
		"fixed pointer stack overflowing capacity should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"fixed pointer stack overflowing capacity error mismatch"
	);

	/* 指针数组同样不得覆盖类型摘要结构。 */
	xrtClearError();
	testRequire(
		!xrtPtrFixedStackInit(
			(xptrfixedstack*)tMemory.Storage,
			(ptr*)tMemory.Storage,
			1
		),
		"fixed pointer stack metadata alias should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"fixed pointer stack metadata alias error mismatch"
	);

	xrtClearError();
	testRequire(
		xrtPtrFixedStackCreate(0) == NULL,
		"zero-capacity fixed pointer stack create should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"zero-capacity fixed pointer stack create error mismatch"
	);
}



/* 验证合法空指针值可由范围成功状态与失败状态准确区分。 */
static void testPtrFixedStackNullContract(void)
{
	xptrfixedstack tStack;
	ptr pStorage[2];
	ptr pOutput = (ptr)(uintptr_t)1;

	testRequire(
		xrtPtrFixedStackInit(&tStack, pStorage, 2),
		"fixed pointer NULL fixture init failed"
	);
	testRequire(
		xrtPtrFixedStackPush(&tStack, NULL),
		"fixed pointer NULL fixture push failed"
	);

	xrtClearError();
	testRequire(
		xrtPtrFixedStackGet(&tStack, 0) == NULL,
		"fixed pointer NULL get mismatch"
	);
	testRequire(
		xrtGetError() == NULL,
		"fixed pointer NULL get reported an error"
	);
	testRequire(
		xrtPtrFixedStackPeek(&tStack, 0) == NULL,
		"fixed pointer NULL peek mismatch"
	);
	testRequire(
		xrtGetError() == NULL,
		"fixed pointer NULL peek reported an error"
	);
	testRequire(
		xrtPtrFixedStackTop(&tStack) == NULL,
		"fixed pointer NULL top mismatch"
	);
	testRequire(
		xrtGetError() == NULL,
		"fixed pointer NULL top reported an error"
	);
	testRequire(
		xrtPtrFixedStackPop(&tStack, &pOutput) && (pOutput == NULL),
		"fixed pointer NULL pop mismatch"
	);

	xrtClearError();
	testRequire(
		xrtPtrFixedStackGet(&tStack, 0) == NULL,
		"empty fixed pointer get should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"empty fixed pointer get error mismatch"
	);
	xrtPtrFixedStackUnit(&tStack);
}



/* 验证全部类型化入口都会拒绝错误元素宽度。 */
static void testPtrFixedStackTypeContract(void)
{
	xfixedstack tWrong;
	unsigned char pStorage[sizeof(ptr) * 4u];
	ptr pOutput = (ptr)(uintptr_t)1;

	testRequire(
		xrtFixedStackInit(
			&tWrong,
			pStorage,
			sizeof(pStorage),
			sizeof(ptr) * 2u
		),
		"wrong-width fixed pointer fixture init failed"
	);
	testRequire(
		xrtFixedStackAdd(&tWrong) != NULL,
		"wrong-width fixed pointer fixture add failed"
	);

	xrtClearError();
	xrtPtrFixedStackClear((xptrfixedstack*)&tWrong);
	testRequire(
		(tWrong.Count == 1) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"fixed pointer clear accepted wrong item size"
	);
	xrtClearError();
	testRequire(
		(xrtPtrFixedStackSpace((xptrfixedstack*)&tWrong) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"fixed pointer space accepted wrong item size"
	);
	xrtClearError();
	testRequire(
		(xrtPtrFixedStackGet((xptrfixedstack*)&tWrong, 0) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"fixed pointer get accepted wrong item size"
	);
	xrtClearError();
	testRequire(
		!xrtPtrFixedStackPush((xptrfixedstack*)&tWrong, NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"fixed pointer push accepted wrong item size"
	);
	xrtClearError();
	testRequire(
		!xrtPtrFixedStackPop((xptrfixedstack*)&tWrong, &pOutput) &&
		(pOutput == (ptr)(uintptr_t)1) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"fixed pointer pop accepted wrong item size"
	);
	xrtClearError();
	testRequire(
		(xrtPtrFixedStackPeek((xptrfixedstack*)&tWrong, 0) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"fixed pointer peek accepted wrong item size"
	);
	xrtClearError();
	testRequire(
		(xrtPtrFixedStackTop((xptrfixedstack*)&tWrong) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"fixed pointer top accepted wrong item size"
	);

	xrtFixedStackUnit(&tWrong);
}



/* 验证类型化 Pop 继承底层固定缓冲别名保护。 */
static void testPtrFixedStackAliasContract(void)
{
	xptrfixedstack tStack;
	ptr pStorage[2];
	int iValue = 7;

	testRequire(
		xrtPtrFixedStackInit(&tStack, pStorage, 2),
		"fixed pointer alias fixture init failed"
	);
	testRequire(
		xrtPtrFixedStackPush(&tStack, &iValue),
		"fixed pointer alias fixture push failed"
	);
	xrtClearError();
	testRequire(
		!xrtPtrFixedStackPop(&tStack, &pStorage[1]),
		"fixed pointer internal pop output should fail"
	);
	testRequire(
		(tStack.Count == 1) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"fixed pointer internal pop output changed state"
	);
	xrtPtrFixedStackUnit(&tStack);
}



/* 运行固定指针栈的完整失败契约测试。 */
int main(void)
{
	testPtrFixedStackInitContract();
	testPtrFixedStackNullContract();
	testPtrFixedStackTypeContract();
	testPtrFixedStackAliasContract();
	printf("[PASS] ptr_fixed_stack contract\n");
	return 0;
}
