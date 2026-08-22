#include "../test.h"



/* 64 字节对象用于验证分块栈显式过对齐。 */
typedef struct testblockaligned {
	unsigned char Data[64];
} testblockaligned;



/* 验证跨块增长、稳定地址、随机检查和 LIFO 合同。 */
static void testBlockStackBasic(void)
{
	xblockstack tStack;
	int* pFirst;
	int* pSlot;
	int iPopped = 0;

	testRequire(
		xrtBlockStackInitLayout(&tStack, sizeof(int), sizeof(int), 4),
		"block stack layout init failed"
	);
	testRequire(xrtBlockStackReserve(&tStack, 9), "block stack reserve failed");
	testRequire(
		(tStack.Capacity == 12) && (tStack.Blocks.Count == 3),
		"block stack rounded capacity mismatch"
	);

	for ( int i = 0; i < 10; i++ ) {
		testRequire(xrtBlockStackPush(&tStack, &i), "block stack push failed");
	}
	pFirst = (int*)xrtBlockStackGet(&tStack, 0);
	testRequire(pFirst != NULL, "block stack first address missing");
	testRequire(*(int*)xrtBlockStackGet(&tStack, 4) == 4, "block boundary get mismatch");
	testRequire(*(const int*)xrtBlockStackConstPeek(&tStack, 2) == 7, "block stack peek mismatch");

	/* 继续跨块增长不能搬移任何既有活动元素。 */
	for ( int i = 10; i < 40; i++ ) {
		pSlot = (int*)xrtBlockStackAdd(&tStack);
		testRequire(pSlot != NULL, "block stack add failed");
		*pSlot = i;
	}
	testRequire(
		(xrtBlockStackGet(&tStack, 0) == pFirst) && (*pFirst == 0),
		"block stack moved an existing element"
	);

	/* 活动完整元素可以直接复制到新栈顶。 */
	testRequire(xrtBlockStackPush(&tStack, pFirst), "block stack self push failed");
	testRequire(*(int*)xrtBlockStackTop(&tStack) == 0, "block stack self push mismatch");

	testRequire(xrtBlockStackPop(&tStack, &iPopped), "block stack pop failed");
	testRequire((iPopped == 0) && (tStack.Count == 40), "block stack pop result mismatch");

	/* Pop 不隐式释放块；Trim 才按当前深度释放尾部块。 */
	while ( tStack.Count > 5 ) {
		testRequire(xrtBlockStackPop(&tStack, NULL), "block stack discard pop failed");
	}
	testRequire(tStack.Capacity >= 40, "block stack pop unexpectedly released capacity");
	testRequire(xrtBlockStackTrim(&tStack), "block stack trim failed");
	testRequire(
		(tStack.Count == 5) &&
		(tStack.Capacity == 8) &&
		(tStack.Blocks.Count == 2),
		"block stack trim state mismatch"
	);

	xrtBlockStackClear(&tStack);
	testRequire((tStack.Count == 0) && (tStack.Capacity == 8), "block stack clear mismatch");
	testRequire(xrtBlockStackTrim(&tStack), "empty block stack trim failed");
	testRequire(
		(tStack.Capacity == 0) && (tStack.Blocks.Count == 0),
		"empty block stack trim state mismatch"
	);
	xrtBlockStackUnit(&tStack);
}



/* 验证自动块布局、显式对齐和参数边界。 */
static void testBlockStackLayout(void)
{
	xblockstack tStack;
	xblockstack* pCreated;

	testRequire(xrtBlockStackInit(&tStack, sizeof(int)), "default block stack init failed");
	testRequire(
		tStack.BlockItems == XRT_BLOCK_STACK_ITEMS_MAX,
		"default block stack item count mismatch"
	);
	xrtBlockStackUnit(&tStack);

	testRequire(
		xrtBlockStackInitLayout(&tStack, sizeof(testblockaligned), 64, 3),
		"aligned block stack init failed"
	);
	for ( size_t i = 0; i < 10; i++ ) {
		testblockaligned* pItem = (testblockaligned*)xrtBlockStackAdd(&tStack);

		testRequire(pItem != NULL, "aligned block stack add failed");
		testRequire(((uintptr_t)pItem & 63u) == 0, "block stack element alignment mismatch");
	}
	xrtBlockStackUnit(&tStack);

	pCreated = xrtBlockStackCreateLayout(sizeof(int), sizeof(int), 2);
	testRequire(pCreated != NULL, "block stack create failed");
	xrtBlockStackDestroy(pCreated);

	xrtClearError();
	testRequire(
		!xrtBlockStackInitLayout(&tStack, sizeof(int), 3, 4),
		"non-power-of-two block alignment should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "block alignment error mismatch");
	xrtClearError();
	testRequire(
		xrtBlockStackCreateLayout(SIZE_MAX, 1, 2) == NULL,
		"overflowing block layout should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "block layout overflow error mismatch");

	testRequire(
		xrtBlockStackInitLayout(&tStack, sizeof(int), sizeof(int), 2),
		"block stack overflow test init failed"
	);
	xrtClearError();
	testRequire(
		!xrtBlockStackReserve(&tStack, SIZE_MAX),
		"unrepresentable rounded capacity should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"block stack rounded capacity error mismatch"
	);
	testRequire((tStack.Count == 0) && (tStack.Capacity == 0), "failed reserve changed block stack");
	xrtBlockStackUnit(&tStack);
}



/* 验证空栈、空来源和公开摘要损坏均被安全拒绝。 */
static void testBlockStackBoundaries(void)
{
	xblockstack tStack;
	int iValue = 11;

	testRequire(
		xrtBlockStackInitLayout(&tStack, sizeof(int), sizeof(int), 2),
		"block boundary stack init failed"
	);
	testRequire(xrtBlockStackReserve(&tStack, 2), "block boundary reserve failed");

	xrtClearError();
	testRequire(xrtBlockStackTop(&tStack) == NULL, "empty block stack top should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "empty block stack top error mismatch");
	testRequire(xrtBlockStackPush(&tStack, &iValue), "block boundary push failed");

	xrtClearError();
	testRequire(
		!xrtBlockStackPush(&tStack, NULL),
		"NULL block stack source should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"NULL block stack source error mismatch"
	);

	tStack.Capacity++;
	xrtClearError();
	testRequire(xrtBlockStackAdd(&tStack) == NULL, "damaged block stack should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "damaged block stack error mismatch");
	tStack.Capacity--;
	xrtBlockStackUnit(&tStack);
}



/* 运行分块稳定地址栈完整合同测试。 */
int main(void)
{
	testBlockStackBasic();
	testBlockStackLayout();
	testBlockStackBoundaries();
	printf("[PASS] block_stack\n");
	return 0;
}
