#include "../test.h"



/* 验证内建标量的增删改查、自引用插入、追加和克隆。 */
static void testTypedArrayIntegers(void)
{
	xtypedarray Array;
	int64 iTen = 10;
	int64 iTwenty = 20;
	int64 iThirty = 30;
	int64 iOutput = -1;
	const int64* pInternal;
	xtypedarray* pClone;
	xtypedarray* pConcat;

	testRequire(
		xrtTypedArrayInit(&Array, xrtTypeInt64()),
		"typed integer array init failed"
	);
	testRequire(
		xrtTypedArrayReserve(&Array, 8u) &&
		xrtTypedArrayPush(&Array, &iTen) &&
		xrtTypedArrayPush(&Array, &iTwenty),
		"typed integer array push failed"
	);
	pInternal = (const int64*)xrtTypedArrayConstGet(&Array, 0u);
	testRequire(
		(pInternal != NULL) && xrtTypedArrayPush(&Array, pInternal),
		"typed array self push failed"
	);
	pInternal = (const int64*)xrtTypedArrayConstGet(&Array, 2u);
	testRequire(
		xrtTypedArrayInsert(&Array, 1u, pInternal),
		"typed array self insert failed"
	);
	testRequire(
		(xrtTypedArrayCount(&Array) == 4u) &&
		(xrtTypedArrayData(&Array) == xrtTypedArrayGet(&Array, 0u)) &&
		(xrtTypedArrayConstData(&Array) == xrtTypedArrayConstGet(&Array, 0u)) &&
		(*(const int64*)xrtTypedArrayConstGet(&Array, 0u) == 10) &&
		(*(const int64*)xrtTypedArrayConstGet(&Array, 1u) == 10) &&
		(*(const int64*)xrtTypedArrayConstGet(&Array, 2u) == 20) &&
		(*(const int64*)xrtTypedArrayConstGet(&Array, 3u) == 10),
		"typed array insertion order mismatch"
	);
	((int64*)xrtTypedArrayData(&Array))[0] = 20;
	testRequire(
		((const int64*)xrtTypedArrayConstData(&Array))[0] == 20,
		"typed array contiguous data borrow mismatch"
	);
	((int64*)xrtTypedArrayData(&Array))[0] = 10;
	testRequire(
		xrtTypedArraySet(&Array, 2u, &iThirty) &&
		(xrtTypedArrayFind(&Array, &iThirty) == 2u) &&
		xrtTypedArrayContains(&Array, &iTen),
		"typed array set or search failed"
	);
	testRequire(
		xrtTypedArrayTake(&Array, 1u, &iOutput) && (iOutput == 10) &&
		xrtTypedArrayPop(&Array, &iOutput) && (iOutput == 10),
		"typed array take or pop failed"
	);
	testRequire(
		xrtTypedArrayResize(&Array, 4u) &&
		(*(const int64*)xrtTypedArrayConstGet(&Array, 2u) == 0) &&
		(*(const int64*)xrtTypedArrayConstGet(&Array, 3u) == 0),
		"typed array resize initialization failed"
	);
	testRequire(
		xrtTypedArraySwap(&Array, 0u, 1u) &&
		(*(const int64*)xrtTypedArrayConstGet(&Array, 0u) == 30) &&
		(*(const int64*)xrtTypedArrayConstGet(&Array, 1u) == 10),
		"typed array swap result mismatch"
	);

	/* 越界交换必须失败，并且不能改变元素数量或内容。 */
	xrtClearError();
	testRequire(
		!xrtTypedArraySwap(&Array, 0u, xrtTypedArrayCount(&Array)) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) == XTYPED_ARRAY_ERROR_RANGE) &&
		(xrtTypedArrayCount(&Array) == 4u) &&
		(*(const int64*)xrtTypedArrayConstGet(&Array, 0u) == 30) &&
		(*(const int64*)xrtTypedArrayConstGet(&Array, 1u) == 10),
		"typed array invalid swap contract mismatch"
	);
	xrtClearError();
	testRequire(
		xrtTypedArrayReverse(&Array) &&
		(*(const int64*)xrtTypedArrayConstGet(&Array, 0u) == 0) &&
		(*(const int64*)xrtTypedArrayConstGet(&Array, 1u) == 0) &&
		(*(const int64*)xrtTypedArrayConstGet(&Array, 2u) == 10) &&
		(*(const int64*)xrtTypedArrayConstGet(&Array, 3u) == 30),
		"typed array reverse result mismatch"
	);
	testRequire(
		xrtTypedArrayResize(&Array, 1u) &&
		xrtTypedArrayAppend(&Array, &Array) &&
		(xrtTypedArrayCount(&Array) == 2u),
		"typed array shrink or self append failed"
	);
	pClone = xrtTypedArrayClone(&Array);
	testRequire(
		(pClone != NULL) &&
		(xrtTypedArrayItemType(pClone) == xrtTypeInt64()) &&
		(xrtTypedArrayCount(pClone) == 2u),
		"typed array clone failed"
	);
	testRequire(
		xrtTypedArrayTrim(pClone) &&
		(xrtTypedArrayCapacity(pClone) == xrtTypedArrayCount(pClone)),
		"typed array trim failed"
	);
	pConcat = xrtTypedArrayConcat(&Array, pClone);
	testRequire(
		(pConcat != NULL) &&
		(xrtTypedArrayCount(pConcat) == 4u) &&
		xrtTypedArrayEquals(&Array, pClone) &&
		!xrtTypedArrayEquals(&Array, pConcat),
		"typed array concat or equals failed"
	);
	xrtTypedArrayClear(&Array);
	testRequire(xrtTypedArrayCount(&Array) == 0u, "typed array clear failed");
	xrtTypedArrayDestroy(pConcat);
	xrtTypedArrayDestroy(pClone);
	xrtTypedArrayUnit(&Array);
}



/* 运行类型数组常规测试。 */
int main(void)
{
	testTypedArrayIntegers();
	xrtClearError();
	printf("[PASS] typed array\n");
	return 0;
}
