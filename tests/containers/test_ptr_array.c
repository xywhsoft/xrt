#include "../test.h"



/* 指针排序比较器接收指向数组指针元素的地址。 */
static int testComparePointerInt(const void* pLeft, const void* pRight)
{
	const int* pLeftValue = *(const int* const*)pLeft;
	const int* pRightValue = *(const int* const*)pRight;

	return (*pLeftValue > *pRightValue) - (*pLeftValue < *pRightValue);
}



/* 验证批量自引用路径和错误类型不会被指针便利层改变。 */
static void testPointerArrayAliasAndType(void)
{
	xptrarray tArray;
	xarray tWrongType;
	int pNumbers[] = { 10, 20, 30, 40 };
	ptr pInitial[] = {
		&pNumbers[0],
		&pNumbers[1],
		&pNumbers[2],
		&pNumbers[3]
	};
	ptr pExpected[] = {
		&pNumbers[0],
		&pNumbers[1],
		&pNumbers[1],
		&pNumbers[2],
		&pNumbers[3],
		&pNumbers[2],
		&pNumbers[3],
		&pNumbers[1],
		&pNumbers[2]
	};
	ptr const* pSource;

	testRequire(xrtPtrArrayInit(&tArray), "pointer alias init failed");
	testRequire(
		xrtPtrArrayAppend(&tArray, pInitial, 4),
		"pointer alias setup failed"
	);
	testRequire(
		xrtPtrArrayAppend(
			&tArray,
			xrtPtrArrayConstData(&tArray) + 1,
			2
		),
		"pointer self append failed"
	);
	pSource = xrtPtrArrayConstData(&tArray) + 1;
	testRequire(
		xrtPtrArrayInsertMany(&tArray, 2, pSource, 3),
		"pointer crossing self insert failed"
	);
	testRequire(tArray.Count == 9, "pointer alias count mismatch");
	for ( size_t i = 0; i < tArray.Count; i++ ) {
		testRequire(
			xrtPtrArrayGet(&tArray, i) == pExpected[i],
			"pointer alias value mismatch"
		);
	}
	testRequire(
		xrtPtrArrayAppend(&tArray, NULL, 0),
		"zero pointer append should be a no-op"
	);
	testRequire(
		xrtPtrArrayInsertMany(&tArray, tArray.Count, NULL, 0),
		"zero pointer insert should be a no-op"
	);
	xrtPtrArrayUnit(&tArray);

	/* 通用数组不能误用为指针数组，否则元素步长会破坏复制边界。 */
	testRequire(
		xrtArrayInit(&tWrongType, sizeof(uint16)),
		"wrong pointer array type setup failed"
	);
	xrtClearError();
	testRequire(
		!xrtPtrArrayPush((xptrarray*)&tWrongType, &pNumbers[0]),
		"wrong pointer array item size should fail"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"wrong pointer array type error mismatch"
	);
	testRequire(tWrongType.Count == 0, "wrong pointer array type changed state");
	xrtArrayUnit(&tWrongType);
}



/* 验证指针数组完整薄封装合同。 */
int main(void)
{
	xptrarray tArray;
	xptrarray* pCreated;
	int pNumbers[] = { 40, 10, 30, 20, 50, 60 };
	ptr pInitial[] = { &pNumbers[0], &pNumbers[1], &pNumbers[2] };
	ptr pMore[] = { &pNumbers[3], &pNumbers[4] };
	ptr pValue;
	ptr* pData;
	size_t iCapacity;

	testRequire(xrtPtrArrayInit(&tArray), "pointer array init failed");
	testRequire(xrtPtrArrayReserve(&tArray, 3), "pointer array reserve failed");
	iCapacity = tArray.Capacity;
	testRequire(xrtPtrArrayAppend(&tArray, pInitial, 3), "pointer array append failed");
	testRequire(xrtPtrArrayGet(&tArray, 0) == &pNumbers[0], "pointer get mismatch");
	testRequire(xrtPtrArraySet(&tArray, 1, NULL), "pointer set NULL failed");
	xrtClearError();
	testRequire(xrtPtrArrayGet(&tArray, 1) == NULL, "valid NULL pointer mismatch");
	testRequire(xrtGetError() == NULL, "valid NULL pointer reported an error");
	testRequire(xrtPtrArrayFind(&tArray, NULL) == 1, "NULL pointer find mismatch");
	testRequire(xrtPtrArraySet(&tArray, 1, &pNumbers[1]), "pointer restore failed");

	testRequire(xrtPtrArrayInsert(&tArray, 1, &pNumbers[5]), "pointer insert failed");
	testRequire(xrtPtrArrayInsertMany(&tArray, 2, pMore, 2), "pointer insert many failed");
	testRequire(tArray.Count == 6, "pointer insert count mismatch");
	testRequire(xrtPtrArrayGet(&tArray, 2) == &pNumbers[3], "pointer insert many value mismatch");
	testRequire(xrtPtrArrayRemove(&tArray, 1, 2), "pointer remove failed");
	testRequire(tArray.Count == 4, "pointer remove count mismatch");
	testRequire(xrtPtrArrayPush(&tArray, &pNumbers[5]), "pointer push failed");

	pData = xrtPtrArrayData(&tArray);
	testRequire((pData != NULL) && (pData[0] == &pNumbers[0]), "pointer data view mismatch");
	testRequire(xrtPtrArraySwap(&tArray, 0, 4), "pointer swap failed");
	testRequire(xrtPtrArrayReverse(&tArray), "pointer reverse failed");
	testRequire(xrtPtrArraySort(&tArray, testComparePointerInt), "pointer sort failed");
	for ( size_t i = 1; i < tArray.Count; i++ ) {
		int* pPrevious = (int*)xrtPtrArrayGet(&tArray, i - 1u);
		int* pCurrent = (int*)xrtPtrArrayGet(&tArray, i);

		testRequire(*pPrevious <= *pCurrent, "pointer sort order mismatch");
	}
	testRequire(xrtPtrArrayFind(&tArray, &pNumbers[2]) != XRT_NPOS, "pointer find failed");
	testRequire(xrtPtrArrayRemoveSwap(&tArray, 0), "pointer remove swap failed");
	testRequire(xrtPtrArrayPop(&tArray, &pValue), "pointer pop failed");
	testRequire(pValue != NULL, "pointer pop value missing");

	testRequire(xrtPtrArrayResize(&tArray, 8), "pointer resize failed");
	for ( size_t i = 3; i < 8; i++ ) {
		testRequire(xrtPtrArrayGet(&tArray, i) == NULL, "new pointer slot is not NULL");
	}
	testRequire(xrtPtrArrayResize(&tArray, 2), "pointer shrink failed");
	testRequire(xrtPtrArrayTrim(&tArray), "pointer trim failed");
	testRequire(tArray.Capacity == 2, "pointer trim capacity mismatch");
	xrtPtrArrayClear(&tArray);
	testRequire((tArray.Count == 0) && (tArray.Capacity == 2), "pointer clear mismatch");
	testRequire(iCapacity >= 3, "pointer initial capacity mismatch");
	xrtPtrArrayUnit(&tArray);

	pCreated = xrtPtrArrayCreate();
	testRequire(pCreated != NULL, "pointer array create failed");
	testRequire(xrtPtrArrayPush(pCreated, &pNumbers[0]), "created pointer push failed");
	xrtPtrArrayDestroy(pCreated);
	testPointerArrayAliasAndType();
	printf("[PASS] ptr_array\n");
	return 0;
}
