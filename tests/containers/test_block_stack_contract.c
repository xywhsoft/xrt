#include "../test.h"



/* 验证生命周期参数、错误优先级和默认块策略。 */
static void testBlockStackContractLayout(void)
{
	xblockstack tStack;

	xrtClearError();
	testRequire(
		!xrtBlockStackInit(NULL, sizeof(int)),
		"NULL block stack init should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"NULL block stack init error mismatch"
	);

	xrtClearError();
	testRequire(
		!xrtBlockStackInit(&tStack, 0),
		"zero-width block stack init should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"zero-width block stack init error mismatch"
	);

	/* 空结构优先报告参数错误，不继续检查其余布局参数。 */
	xrtClearError();
	testRequire(
		!xrtBlockStackInitLayout(NULL, SIZE_MAX, 1, 2),
		"NULL block stack layout should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"NULL block stack layout error precedence mismatch"
	);

	xrtClearError();
	testRequire(
		!xrtBlockStackInitLayout(&tStack, sizeof(int), sizeof(int), 0),
		"zero block item count should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"zero block item count error mismatch"
	);

	xrtClearError();
	testRequire(
		xrtBlockStackCreate(0) == NULL,
		"zero-width block stack create should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"zero-width block stack create error mismatch"
	);

	/* 默认策略应限制小元素块深度，并按 16 KiB 目标缩减大元素块。 */
	testRequire(
		xrtBlockStackInit(&tStack, 64),
		"small default block stack init failed"
	);
	testRequire(
		tStack.BlockItems == XRT_BLOCK_STACK_ITEMS_MAX,
		"small default block item count mismatch"
	);
	xrtBlockStackUnit(&tStack);

	testRequire(
		xrtBlockStackInit(&tStack, 1024),
		"medium default block stack init failed"
	);
	testRequire(
		tStack.BlockItems == 16,
		"medium default block item count mismatch"
	);
	xrtBlockStackUnit(&tStack);

	testRequire(
		xrtBlockStackInit(&tStack, XRT_BLOCK_STACK_BYTES_DEFAULT + 1u),
		"large default block stack init failed"
	);
	testRequire(
		tStack.BlockItems == 1,
		"large default block item count mismatch"
	);
	xrtBlockStackUnit(&tStack);
}



/* 验证空栈、越界访问和损坏分配摘要均被确定性拒绝。 */
static void testBlockStackContractState(void)
{
	xblockstack tStack;

	testRequire(
		xrtBlockStackInitLayout(&tStack, 1, 1, 1),
		"block stack state test init failed"
	);

	xrtClearError();
	testRequire(
		!xrtBlockStackPop(&tStack, NULL),
		"empty block stack pop should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"empty block stack pop error mismatch"
	);

	xrtClearError();
	testRequire(
		xrtBlockStackGet(&tStack, SIZE_MAX) == NULL,
		"oversized block stack index should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"oversized block stack index error mismatch"
	);

	xrtClearError();
	testRequire(
		xrtBlockStackConstPeek(&tStack, SIZE_MAX) == NULL,
		"oversized block stack depth should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"oversized block stack depth error mismatch"
	);

	/* 乘法溢出的公开摘要不得进入块分配路径。 */
	tStack.ItemSize = (SIZE_MAX / 2u) + 1u;
	tStack.BlockItems = 2;
	xrtClearError();
	testRequire(
		xrtBlockStackAdd(&tStack) == NULL,
		"overflowing damaged block layout should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"overflowing damaged block layout error mismatch"
	);
	testRequire(
		(tStack.Count == 0) &&
		(tStack.Capacity == 0) &&
		(tStack.Blocks.Count == 0),
		"overflowing damaged block layout changed state"
	);

	/* 即使元素乘法本身成立，管理头加数据区溢出也必须被拒绝。 */
	tStack.ItemSize = SIZE_MAX;
	tStack.BlockItems = 1;
	xrtClearError();
	testRequire(
		!xrtBlockStackReserve(&tStack, 1),
		"overhead-overflowing damaged block layout should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"overhead-overflowing damaged block layout error mismatch"
	);

	tStack.ItemSize = 1;
	tStack.BlockItems = 1;
	xrtBlockStackUnit(&tStack);
}



/* 验证访问块和裁剪块的内部地址损坏不会造成部分提交。 */
static void testBlockStackContractBlockState(void)
{
	xblockstack tStack;
	ptr* pBlocks;
	ptr pFirstBlock;
	ptr pSecondBlock;
	unsigned char iValue = 23;

	testRequire(
		xrtBlockStackInitLayout(&tStack, 1, 1, 2),
		"block stack block-state init failed"
	);
	testRequire(
		xrtBlockStackReserve(&tStack, 4),
		"block stack block-state reserve failed"
	);
	testRequire(
		xrtBlockStackPush(&tStack, &iValue),
		"block stack block-state push failed"
	);
	pBlocks = (ptr*)tStack.Blocks.Data;
	pFirstBlock = pBlocks[0];
	pSecondBlock = pBlocks[1];

	/* 被访问块的空地址必须作为损坏状态拒绝。 */
	pBlocks[0] = NULL;
	xrtClearError();
	testRequire(
		xrtBlockStackGet(&tStack, 0) == NULL,
		"damaged active block should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"damaged active block error mismatch"
	);
	pBlocks[0] = pFirstBlock;

	/* Trim 先验证全部尾块，发现损坏时不得释放任何已验证块。 */
	pBlocks[1] = NULL;
	xrtClearError();
	testRequire(
		!xrtBlockStackTrim(&tStack),
		"damaged trailing block trim should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"damaged trailing block trim error mismatch"
	);
	testRequire(
		(tStack.Count == 1) &&
		(tStack.Capacity == 4) &&
		(tStack.Blocks.Count == 2) &&
		(pBlocks[0] == pFirstBlock),
		"failed damaged trailing block trim changed state"
	);
	pBlocks[1] = pSecondBlock;

	xrtBlockStackUnit(&tStack);
}



/* 运行分块栈参数、状态和失败原子性契约测试。 */
int main(void)
{
	testBlockStackContractLayout();
	testBlockStackContractState();
	testBlockStackContractBlockState();
	printf("[PASS] block_stack contract\n");
	return 0;
}
