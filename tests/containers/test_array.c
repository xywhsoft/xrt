#include "../test.h"



/* 大元素用于验证交换过程不依赖堆内存或固定元素上限。 */
typedef struct testlargeitem {
	uint32 ID;
	unsigned char Payload[509];
} testlargeitem;



/* 64 字节元素用于验证显式过对齐数组的每个元素地址。 */
typedef struct testaligneditem {
	unsigned char Data[64];
} testaligneditem;



/* 使用无减法溢出的方式比较整数。 */
static int testCompareInt(const void* pLeft, const void* pRight)
{
	int iLeft = *(const int*)pLeft;
	int iRight = *(const int*)pRight;

	return (iLeft > iRight) - (iLeft < iRight);
}



/* 验证数组整数内容与预期完全一致。 */
static void testArrayValues(const xarray* pArray, const int* pValues, size_t iCount)
{
	testRequire(pArray->Count == iCount, "array count mismatch");
	for ( size_t i = 0; i < iCount; i++ ) {
		const int* pValue = (const int*)xrtArrayConstGet(pArray, i);

		testRequire(pValue != NULL, "array value missing");
		testRequire(*pValue == pValues[i], "array value mismatch");
	}
}



/* 验证生命周期、容量、复制写入和安全访问。 */
static void testArrayBasic(void)
{
	xarray tArray;
	xarray* pCreated;
	int pValues[] = { 1, 2, 3, 4 };
	int iValue = 9;
	size_t iCapacity;

	testRequire(xrtArrayInit(&tArray, sizeof(int)), "array init failed");
	testRequire(
		(tArray.Data == NULL) &&
		(tArray.Count == 0) &&
		(tArray.Capacity == 0) &&
		(tArray.ItemSize == sizeof(int)),
		"array initial state mismatch"
	);
	testRequire(xrtArraySort(&tArray, testCompareInt), "empty array sort failed");
	testRequire(
		xrtArrayBSearch(&tArray, &iValue, testCompareInt) == XRT_NPOS,
		"empty array search should not find an item"
	);
	testRequire(xrtArrayReserve(&tArray, 4), "array reserve failed");
	testRequire((tArray.Capacity >= 4) && (tArray.Capacity < 256), "small array growth is excessive");
	iCapacity = tArray.Capacity;
	testRequire(xrtArrayAppend(&tArray, pValues, 4), "array append failed");
	testArrayValues(&tArray, pValues, 4);
	testRequire(xrtArraySet(&tArray, 1, &iValue), "array set failed");
	testRequire(*(int*)xrtArrayGet(&tArray, 1) == 9, "array set value mismatch");
	testRequire(tArray.Capacity == iCapacity, "append unexpectedly changed reserved capacity");

	xrtClearError();
	testRequire(xrtArrayGet(&tArray, 4) == NULL, "out-of-range get should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "get range error mismatch");
	xrtClearError();
	testRequire(!xrtArraySet(&tArray, 4, &iValue), "out-of-range set should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "set range error mismatch");

	xrtArrayClear(&tArray);
	testRequire((tArray.Count == 0) && (tArray.Capacity == iCapacity), "array clear state mismatch");
	testRequire(xrtArrayTrim(&tArray), "empty array trim failed");
	testRequire((tArray.Data == NULL) && (tArray.Capacity == 0), "empty trim did not release storage");
	xrtArrayUnit(&tArray);
	testRequire((tArray.ItemSize == 0) && (tArray.Alignment == 0), "array unit did not reset state");

	pCreated = xrtArrayCreate(sizeof(int));
	testRequire(pCreated != NULL, "array create failed");
	testRequire(xrtArrayPush(pCreated, &iValue), "created array push failed");
	xrtArrayDestroy(pCreated);
}



/* 验证旧版已有的批量插入、删除、交换和排序能力。 */
static void testArrayOperations(void)
{
	xarray tArray;
	int pInitial[] = { 10, 20, 30, 40 };
	int pInserted[] = { 21, 22 };
	int pExpectedInsert[] = { 10, 20, 21, 22, 30, 40 };
	int pExpectedRemove[] = { 10, 22, 30, 40 };
	int pExpectedSwap[] = { 40, 22, 30, 10 };
	int pExpectedReverse[] = { 10, 30, 22, 40 };
	int pExpectedSort[] = { 10, 22, 30, 40 };
	int iPopped = 0;
	int iKey = 30;

	testRequire(xrtArrayInit(&tArray, sizeof(int)), "operation array init failed");
	testRequire(xrtArrayAppend(&tArray, pInitial, 4), "initial append failed");
	testRequire(xrtArrayInsert(&tArray, 2, pInserted, 2), "middle insert failed");
	testArrayValues(&tArray, pExpectedInsert, 6);
	testRequire(xrtArrayRemove(&tArray, 1, 2), "range remove failed");
	testArrayValues(&tArray, pExpectedRemove, 4);

	/* 精确删除合同不再像旧版一样静默截断到末尾。 */
	testRequire(!xrtArrayRemove(&tArray, 3, 2), "oversized remove should fail");
	testRequire(tArray.Count == 4, "failed remove changed count");
	testRequire(xrtArraySwap(&tArray, 0, 3), "array swap failed");
	testArrayValues(&tArray, pExpectedSwap, 4);
	testRequire(xrtArrayReverse(&tArray), "array reverse failed");
	testArrayValues(&tArray, pExpectedReverse, 4);
	testRequire(xrtArraySort(&tArray, testCompareInt), "array sort failed");
	testArrayValues(&tArray, pExpectedSort, 4);
	testRequire(xrtArrayFind(&tArray, &iKey) == 2, "array byte find mismatch");
	testRequire(xrtArrayFindBy(&tArray, &iKey, testCompareInt) == 2, "array comparator find mismatch");
	testRequire(xrtArrayBSearch(&tArray, &iKey, testCompareInt) == 2, "array binary search mismatch");
	xrtClearError();
	testRequire(
		!xrtArrayPop(&tArray, xrtArrayGet(&tArray, 0)),
		"array pop should reject an internal output alias"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "array pop alias error mismatch");
	testRequire(tArray.Count == 4, "array pop alias changed count");

	testRequire(xrtArrayRemoveSwap(&tArray, 1), "array swap remove failed");
	testRequire((tArray.Count == 3) && (*(int*)xrtArrayGet(&tArray, 1) == 40), "swap remove value mismatch");
	testRequire(xrtArrayPop(&tArray, &iPopped), "array pop failed");
	testRequire((iPopped == 30) && (tArray.Count == 2), "array pop result mismatch");
	xrtArrayUnit(&tArray);
}



/* 验证直接取得未初始化槽后写入的底层高效路径。 */
static void testArraySpaces(void)
{
	xarray tArray;
	int* pSpace;
	int pExpected[] = { 1, 2, 3, 4, 5 };

	testRequire(xrtArrayInit(&tArray, sizeof(int)), "space array init failed");
	pSpace = (int*)xrtArrayAdd(&tArray, 3);
	testRequire(pSpace != NULL, "array add space failed");
	pSpace[0] = 1;
	pSpace[1] = 4;
	pSpace[2] = 5;
	pSpace = (int*)xrtArrayInsertSpace(&tArray, 1, 2);
	testRequire(pSpace != NULL, "array insert space failed");
	pSpace[0] = 2;
	pSpace[1] = 3;
	testArrayValues(&tArray, pExpected, 5);

	xrtClearError();
	testRequire(xrtArrayAdd(&tArray, 0) == NULL, "zero add should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "zero add error mismatch");
	testRequire(xrtArrayAppend(&tArray, NULL, 0), "zero append should be a no-op");
	testRequire(xrtArrayInsert(&tArray, tArray.Count, NULL, 0), "zero insert should be a no-op");
	xrtArrayUnit(&tArray);
}



/* 验证自追加和自插入在扩容及中段移动时保持原数据。 */
static void testArrayAlias(void)
{
	xarray tArray;
	int pInitial[] = { 1, 2, 3, 4 };
	int pExpectedPrefix[] = { 1, 2, 1, 2, 3, 4 };
	int pExpectedSuffix[] = { 1, 2, 1, 2, 3, 4, 3, 4 };
	int pExpectedInsert[] = { 1, 2, 2, 3, 3, 4 };
	int pExpectedAppend[] = { 1, 2, 2, 3, 3, 4, 1, 2, 2, 3, 3, 4 };
	int* pInactive;

	testRequire(xrtArrayInit(&tArray, sizeof(int)), "alias array init failed");
	testRequire(xrtArrayAppend(&tArray, pInitial, 4), "alias initial append failed");
	testRequire(
		xrtArrayInsert(&tArray, 2, xrtArrayGet(&tArray, 0), 2),
		"self insert from prefix failed"
	);
	testArrayValues(&tArray, pExpectedPrefix, 6);
	testRequire(
		xrtArrayInsert(&tArray, tArray.Count, xrtArrayGet(&tArray, 4), 2),
		"self insert from suffix failed"
	);
	testArrayValues(&tArray, pExpectedSuffix, 8);
	xrtArrayClear(&tArray);
	testRequire(xrtArrayAppend(&tArray, pInitial, 4), "alias reset append failed");
	testRequire(
		xrtArrayInsert(&tArray, 2, xrtArrayGet(&tArray, 1), 2),
		"self insert across insertion point failed"
	);
	testArrayValues(&tArray, pExpectedInsert, 6);
	testRequire(
		xrtArrayAppend(&tArray, tArray.Data, tArray.Count),
		"self append failed"
	);
	testArrayValues(&tArray, pExpectedAppend, 12);

	/* 保留区不是活动元素，不能作为复制来源。 */
	testRequire(xrtArrayReserve(&tArray, tArray.Count + 8), "alias reserve failed");
	pInactive = (int*)tArray.Data + tArray.Count;
	testRequire(!xrtArrayPush(&tArray, pInactive), "inactive capacity source should fail");
	testRequire(tArray.Count == 12, "invalid alias changed array count");
	xrtArrayUnit(&tArray);
}



/* 穷举短数组的来源区间、插入位点和扩容状态。 */
static void testArrayAliasMatrix(void)
{
	int pInitial[8];
	int pExpected[16];

	for ( size_t i = 0; i < 8u; i++ ) {
		pInitial[i] = (int)(i + 1u);
	}

	for ( size_t iLength = 1u; iLength <= 8u; iLength++ ) {
		for ( size_t iSource = 0; iSource < iLength; iSource++ ) {
			for (
				size_t iCopyCount = 1u;
				iCopyCount <= (iLength - iSource);
				iCopyCount++
			) {
				for ( size_t iInsert = 0; iInsert <= iLength; iInsert++ ) {
					for ( size_t iGrowth = 0; iGrowth < 2u; iGrowth++ ) {
						xarray tArray;
						const int* pSource;

						memcpy(
							pExpected,
							pInitial,
							iInsert * sizeof(int)
						);
						memcpy(
							pExpected + iInsert,
							pInitial + iSource,
							iCopyCount * sizeof(int)
						);
						memcpy(
							pExpected + iInsert + iCopyCount,
							pInitial + iInsert,
							(iLength - iInsert) * sizeof(int)
						);

						testRequire(
							xrtArrayInit(&tArray, sizeof(int)),
							"alias matrix init failed"
						);
						testRequire(
							xrtArrayAppend(&tArray, pInitial, iLength),
							"alias matrix setup failed"
						);
						if ( iGrowth != 0u ) {
							testRequire(
								xrtArrayTrim(&tArray),
								"alias matrix trim failed"
							);
						} else {
							testRequire(
								xrtArrayReserve(
									&tArray,
									iLength + iCopyCount + 8u
								),
								"alias matrix reserve failed"
							);
						}

						pSource = (const int*)xrtArrayConstGet(
							&tArray,
							iSource
						);
						testRequire(
							xrtArrayInsert(
								&tArray,
								iInsert,
								pSource,
								iCopyCount
							),
							"alias matrix insert failed"
						);
						testArrayValues(
							&tArray,
							pExpected,
							iLength + iCopyCount
						);
						xrtArrayUnit(&tArray);
					}
				}
			}
		}
	}
}



/* 验证损坏的公开布局在进入地址运算前被拒绝。 */
static void testArrayInvalidState(void)
{
	xarray tArray;
	bytes pData;
	ptr pAllocation;
	size_t iCapacity;
	int iValue = 7;

	testRequire(xrtArrayInit(&tArray, sizeof(int)), "state array init failed");
	testRequire(xrtArrayPush(&tArray, &iValue), "state array setup failed");
	pData = tArray.Data;
	pAllocation = tArray.Allocation;
	iCapacity = tArray.Capacity;

	tArray.Capacity = SIZE_MAX;
	xrtClearError();
	testRequire(!xrtArrayReserve(&tArray, 1), "overflowed public capacity should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "capacity state error mismatch");
	tArray.Capacity = iCapacity;

	tArray.Data = pData + sizeof(int);
	xrtClearError();
	testRequire(xrtArrayGet(&tArray, 0) == NULL, "detached public data should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "data state error mismatch");
	tArray.Data = pData;
	tArray.Allocation = pAllocation;
	xrtArrayUnit(&tArray);
}



/* 验证安全 resize、精确 trim 和溢出边界。 */
static void testArrayResize(void)
{
	xarray tArray;
	int iValue = 7;

	testRequire(xrtArrayInit(&tArray, sizeof(int)), "resize array init failed");
	testRequire(xrtArrayPush(&tArray, &iValue), "resize initial push failed");
	testRequire(xrtArrayResize(&tArray, 5), "array grow resize failed");
	for ( size_t i = 1; i < 5; i++ ) {
		testRequire(*(int*)xrtArrayGet(&tArray, i) == 0, "new resize item is not zero");
	}
	testRequire(xrtArrayResize(&tArray, 2), "array shrink resize failed");
	testRequire(tArray.Capacity >= 5, "resize shrink unexpectedly released capacity");
	testRequire(xrtArrayTrim(&tArray), "array trim failed");
	testRequire(tArray.Capacity == 2, "array trim capacity mismatch");

	xrtClearError();
	testRequire(!xrtArrayReserve(&tArray, SIZE_MAX), "overflow reserve should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "overflow reserve error mismatch");
	testRequire((tArray.Count == 2) && (tArray.Capacity == 2), "overflow changed array state");
	xrtArrayUnit(&tArray);
}



/* 验证显式过对齐和大元素无分配交换。 */
static void testArrayAlignmentAndLargeSwap(void)
{
	xarray tAligned;
	xarray tLarge;
	testaligneditem* pItems;
	testlargeitem tLeft;
	testlargeitem tRight;

	testRequire(
		xrtArrayInitAligned(&tAligned, sizeof(testaligneditem), 64),
		"aligned array init failed"
	);
	testRequire(xrtArrayResize(&tAligned, 5), "aligned array resize failed");
	pItems = (testaligneditem*)tAligned.Data;
	for ( size_t i = 0; i < tAligned.Count; i++ ) {
		testRequire(((uintptr_t)&pItems[i] & 63u) == 0, "array element alignment mismatch");
	}
	testRequire(xrtArrayTrim(&tAligned), "aligned array trim failed");
	testRequire(((uintptr_t)tAligned.Data & 63u) == 0, "trim lost array alignment");
	if ( tAligned.Allocation != tAligned.Data ) {
		testRequire(
			!xrtArrayPush(&tAligned, tAligned.Allocation),
			"aligned prefix padding should not be an element source"
		);
	} else {
		testRequire(
			!xrtArrayPush(&tAligned, tAligned.Data + (tAligned.Count * tAligned.ItemSize)),
			"aligned suffix padding should not be an element source"
		);
	}
	testRequire(tAligned.Count == 5, "padding source changed aligned array count");
	xrtArrayUnit(&tAligned);

	testRequire(!xrtArrayInitAligned(&tAligned, 12, 16), "misaligned stride should fail");
	testRequire(!xrtArrayInitAligned(&tAligned, 64, 24), "non-power-of-two alignment should fail");

	memset(&tLeft, 0x11, sizeof(tLeft));
	memset(&tRight, 0x77, sizeof(tRight));
	tLeft.ID = 11;
	tRight.ID = 77;
	testRequire(xrtArrayInit(&tLarge, sizeof(testlargeitem)), "large array init failed");
	testRequire(xrtArrayPush(&tLarge, &tLeft), "large left push failed");
	testRequire(xrtArrayPush(&tLarge, &tRight), "large right push failed");
	testRequire(xrtArraySwap(&tLarge, 0, 1), "large item swap failed");
	testRequire(((testlargeitem*)xrtArrayGet(&tLarge, 0))->ID == 77, "large left swap mismatch");
	testRequire(((testlargeitem*)xrtArrayGet(&tLarge, 1))->ID == 11, "large right swap mismatch");
	xrtArrayUnit(&tLarge);
}



/* 运行动态数组完整合同测试。 */
int main(void)
{
	testArrayBasic();
	testArrayOperations();
	testArraySpaces();
	testArrayAlias();
	testArrayAliasMatrix();
	testArrayInvalidState();
	testArrayResize();
	testArrayAlignmentAndLargeSwap();
	printf("[PASS] array\n");
	return 0;
}
