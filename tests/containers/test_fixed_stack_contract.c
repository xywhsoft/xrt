#include "../test.h"



/* 验证固定栈初始化会拒绝无效尺寸和元数据重叠。 */
static void testFixedStackInitContract(void)
{
	union {
		xfixedstack Alignment;
		unsigned char Storage[sizeof(xfixedstack) + 16u];
	} tMemory;
	xfixedstack tStack;
	int iValue = 1;

	xrtClearError();
	testRequire(
		!xrtFixedStackInit(NULL, &iValue, sizeof(iValue), sizeof(iValue)),
		"fixed stack NULL target should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"fixed stack NULL target error mismatch"
	);

	xrtClearError();
	testRequire(
		!xrtFixedStackInit(&tStack, NULL, sizeof(iValue), sizeof(iValue)),
		"fixed stack NULL memory should fail"
	);
	testRequire(
		!xrtFixedStackInit(&tStack, &iValue, 0, sizeof(iValue)),
		"fixed stack short memory should fail"
	);
	testRequire(
		!xrtFixedStackInit(&tStack, &iValue, sizeof(iValue), 0),
		"fixed stack zero item size should fail"
	);

	/* 数据区不得覆盖结构起点或结构尾部。 */
	xrtClearError();
	testRequire(
		!xrtFixedStackInit(
			(xfixedstack*)tMemory.Storage,
			tMemory.Storage,
			sizeof(tMemory.Storage),
			1
		),
		"fixed stack exact metadata alias should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"fixed stack exact metadata alias error mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtFixedStackInit(
			(xfixedstack*)tMemory.Storage,
			tMemory.Storage + sizeof(xfixedstack) - 1u,
			2,
			1
		),
		"fixed stack partial metadata alias should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"fixed stack partial metadata alias error mismatch"
	);
}



/* 验证损坏的公开状态会被所有访问入口拒绝。 */
static void testFixedStackStateContract(void)
{
	xfixedstack tStack;
	int pStorage[2] = { 1, 2 };

	memset(&tStack, 0, sizeof(tStack));
	xrtClearError();
	testRequire(
		xrtFixedStackSpace(&tStack) == 0,
		"zero fixed stack should have no usable space"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"zero fixed stack error mismatch"
	);

	testRequire(
		xrtFixedStackInit(
			&tStack,
			pStorage,
			sizeof(pStorage),
			sizeof(pStorage[0])
		),
		"fixed stack state fixture init failed"
	);
	tStack.Count = tStack.Capacity + 1u;
	xrtClearError();
	testRequire(
		xrtFixedStackTop(&tStack) == NULL,
		"fixed stack invalid count should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"fixed stack invalid count error mismatch"
	);

	tStack.Count = 0;
	tStack.Allocation = &tStack;
	xrtClearError();
	testRequire(
		xrtFixedStackAdd(&tStack) == NULL,
		"fixed stack invalid allocation marker should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"fixed stack invalid allocation marker error mismatch"
	);
	tStack.Allocation = NULL;
	xrtFixedStackUnit(&tStack);
}



/* 验证固定栈访问、别名和满空边界都保持失败原子性。 */
static void testFixedStackOperationContract(void)
{
	xfixedstack tStack;
	int pStorage[3] = { 0, 0, 0 };
	int pValues[3] = { 10, 20, 30 };
	int iOutput = 0;
	size_t iCount;

	testRequire(
		xrtFixedStackInit(
			&tStack,
			pStorage,
			sizeof(pStorage),
			sizeof(pStorage[0])
		),
		"fixed stack operation fixture init failed"
	);
	testRequire(
		xrtFixedStackPush(&tStack, &pValues[0]) &&
		xrtFixedStackPush(&tStack, &pValues[1]),
		"fixed stack operation fixture push failed"
	);

	xrtClearError();
	testRequire(
		xrtFixedStackGet(&tStack, tStack.Count) == NULL,
		"fixed stack end index should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"fixed stack end index error mismatch"
	);
	xrtClearError();
	testRequire(
		xrtFixedStackConstPeek(&tStack, tStack.Count) == NULL,
		"fixed stack excessive depth should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"fixed stack excessive depth error mismatch"
	);

	/* 未使用槽和跨元素来源都不能作为 Push 输入。 */
	iCount = tStack.Count;
	xrtClearError();
	testRequire(
		!xrtFixedStackPush(&tStack, &pStorage[2]),
		"fixed stack inactive source should fail"
	);
	testRequire(
		(tStack.Count == iCount) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"fixed stack inactive source changed state"
	);
	xrtClearError();
	testRequire(
		!xrtFixedStackPush(&tStack, ((unsigned char*)pStorage) + 1u),
		"fixed stack unaligned source should fail"
	);
	testRequire(
		(tStack.Count == iCount) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"fixed stack unaligned source changed state"
	);

	/* Pop 输出不得覆盖固定栈的活动区或保留容量。 */
	xrtClearError();
	testRequire(
		!xrtFixedStackPop(&tStack, &pStorage[2]),
		"fixed stack internal pop output should fail"
	);
	testRequire(
		(tStack.Count == iCount) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"fixed stack internal pop output changed state"
	);

	testRequire(
		xrtFixedStackPush(&tStack, &pValues[2]),
		"fixed stack final push failed"
	);
	iCount = tStack.Count;
	xrtClearError();
	testRequire(
		xrtFixedStackAdd(&tStack) == NULL,
		"full fixed stack add should fail"
	);
	testRequire(
		(tStack.Count == iCount) &&
		(xrtErrorKind(xrtGetError()) == XERR_AGAIN),
		"full fixed stack add changed state"
	);

	testRequire(
		xrtFixedStackPop(&tStack, &iOutput) && (iOutput == 30),
		"fixed stack pop value mismatch"
	);
	testRequire(
		xrtFixedStackPop(&tStack, NULL) &&
		xrtFixedStackPop(&tStack, NULL),
		"fixed stack drain failed"
	);
	xrtClearError();
	testRequire(
		!xrtFixedStackPop(&tStack, &iOutput),
		"empty fixed stack pop should fail"
	);
	testRequire(
		(tStack.Count == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"empty fixed stack pop changed state"
	);
	xrtFixedStackUnit(&tStack);
}



/* 验证拥有型固定栈的尺寸错误在分配前被拒绝。 */
static void testFixedStackCreateContract(void)
{
	xrtClearError();
	testRequire(
		xrtFixedStackCreate(0, sizeof(int)) == NULL,
		"zero-capacity fixed stack should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"zero-capacity fixed stack error mismatch"
	);
	xrtClearError();
	testRequire(
		xrtFixedStackCreate(1, 0) == NULL,
		"zero-width fixed stack should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"zero-width fixed stack error mismatch"
	);
	xrtClearError();
	testRequire(
		xrtFixedStackCreate(SIZE_MAX, 2) == NULL,
		"overflowing fixed stack should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"overflowing fixed stack error mismatch"
	);
}



/* 运行固定容量栈的完整失败契约测试。 */
int main(void)
{
	testFixedStackInitContract();
	testFixedStackStateContract();
	testFixedStackOperationContract();
	testFixedStackCreateContract();
	printf("[PASS] fixed_stack contract\n");
	return 0;
}
