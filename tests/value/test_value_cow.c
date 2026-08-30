#include "../test.h"



/* 读取整数测试值。 */
static int64 testValueCowInt(const xvalue* pValue)
{
	int64 iValue = 0;

	testRequire(xrtValueGetInt(pValue, &iValue), "COW expected integer");
	return iValue;
}



/* 验证四种 backing 的 O(1) 克隆和首次写入分离。 */
static void testValueCowRoots(void)
{
	xvalue* pArray = xrtValueArray();
	xvalue* pArrayCopy;
	xvalue* pMap = xrtValueIntMap();
	xvalue* pMapCopy;
	xvalue* pObject = xrtValueObject();
	xvalue* pObjectCopy;
	xvalue* pSet = xrtValueSet();
	xvalue* pSetCopy;
	xvalue* pTwo = xrtValueInt(2);

	testRequire(xrtValueArrayAppendNew(pArray, xrtValueInt(1)), "COW array fixture failed");
	testRequire(
		xrtValueTypeIdBind(pArray, UINT64_C(0x1020304050607080)),
		"COW array type identity bind failed"
	);
	pArrayCopy = xrtValueClone(pArray);
	testRequire(
		xrtValueTypeId(pArrayCopy) == UINT64_C(0x1020304050607080),
		"COW clone lost type identity"
	);
	testRequire(xrtValueArrayAppendNew(pArrayCopy, xrtValueInt(2)), "COW array mutation failed");
	testRequire((xrtValueCount(pArray) == 1) && (xrtValueCount(pArrayCopy) == 2), "COW array source changed");

	testRequire(xrtValueIntMapSetNew(pMap, 1, xrtValueInt(1)), "COW int map fixture failed");
	pMapCopy = xrtValueClone(pMap);
	testRequire(xrtValueIntMapSetNew(pMapCopy, 2, xrtValueInt(2)), "COW int map mutation failed");
	testRequire(!xrtValueIntMapHas(pMap, 2) && xrtValueIntMapHas(pMapCopy, 2), "COW int map source changed");

	testRequire(xrtValueObjectSetNew(pObject, XRT_STR_LITERAL("a"), xrtValueInt(1)), "COW object fixture failed");
	pObjectCopy = xrtValueClone(pObject);
	testRequire(xrtValueObjectSetNew(pObjectCopy, XRT_STR_LITERAL("b"), xrtValueInt(2)), "COW object mutation failed");
	testRequire(!xrtValueObjectHas(pObject, XRT_STR_LITERAL("b")) && xrtValueObjectHas(pObjectCopy, XRT_STR_LITERAL("b")), "COW object source changed");

	testRequire(xrtValueSetAddNew(pSet, xrtValueInt(1)), "COW set fixture failed");
	pSetCopy = xrtValueClone(pSet);
	testRequire(xrtValueSetAdd(pSetCopy, pTwo), "COW set mutation failed");
	testRequire(!xrtValueSetHas(pSet, pTwo) && xrtValueSetHas(pSetCopy, pTwo), "COW set source changed");

	xrtValueRelease(pTwo);
	xrtValueRelease(pSetCopy);
	xrtValueRelease(pSet);
	xrtValueRelease(pObjectCopy);
	xrtValueRelease(pObject);
	xrtValueRelease(pMapCopy);
	xrtValueRelease(pMap);
	xrtValueRelease(pArrayCopy);
	xrtValueRelease(pArray);
}



/* 验证 Edit 只分离需要修改的嵌套路径。 */
static void testValueCowNested(void)
{
	xvalue* pRoot = xrtValueArray();
	xvalue* pChild = xrtValueArray();
	xvalue* pCopy;
	xvalue* pMutable;

	testRequire(xrtValueArrayAppendNew(pChild, xrtValueInt(31)), "nested child fixture failed");
	testRequire(xrtValueArrayAppendTake(pRoot, &pChild), "nested root fixture failed");
	pCopy = xrtValueClone(pRoot);
	pMutable = xrtValueArrayEdit(pCopy, 0);
	testRequire((pMutable != NULL) && xrtValueArraySetNew(pMutable, 0, xrtValueInt(32)), "nested edit failed");
	testRequire(testValueCowInt(xrtValueArrayGet(xrtValueArrayGet(pRoot, 0), 0)) == 31, "nested edit changed source");
	testRequire(testValueCowInt(xrtValueArrayGet(xrtValueArrayGet(pCopy, 0), 0)) == 32, "nested edit did not change copy");
	xrtValueRelease(pCopy);
	xrtValueRelease(pRoot);
}



/* 验证快照迭代器使后续写入分离并继续读取旧顺序。 */
static void testValueIteratorSnapshot(void)
{
	xvalue* pObject = xrtValueObject();
	xvalueiter tIterator;
	xvaluekey Key;
	xvalue* pItem;

	testRequire(xrtValueObjectSetNew(pObject, XRT_STR_LITERAL("old"), xrtValueInt(1)), "snapshot fixture failed");
	testRequire(xrtValueIterBegin(pObject, &tIterator), "snapshot iterator begin failed");
	testRequire(xrtValueObjectSetNew(pObject, XRT_STR_LITERAL("new"), xrtValueInt(2)), "snapshot mutation failed");
	pItem = xrtValueIterNext(&tIterator, &Key);
	testRequire((pItem != NULL) && (Key.String.Size == 3) && (memcmp(Key.String.Data, "old", 3) == 0), "snapshot old item mismatch");
	testRequire(xrtValueIterNext(&tIterator, &Key) == NULL, "snapshot observed later mutation");
	xrtValueIterEnd(&tIterator);
	testRequire(xrtValueCount(pObject) == 2, "snapshot mutation count mismatch");
	xrtValueRelease(pObject);
}



/* 验证逆序快照拥有 backing，可在原容器释放后继续遍历。 */
static void testValueIteratorLifetime(void)
{
	xvalue* pArray = xrtValueArray();
	xvalueiter tIterator;
	xvaluekey Key;
	xvalue* pItem;
	int64 arrExpected[] = { 3, 2, 1 };

	testRequire(
		xrtValueArrayAppendNew(pArray, xrtValueInt(1)) &&
		xrtValueArrayAppendNew(pArray, xrtValueInt(2)) &&
		xrtValueArrayAppendNew(pArray, xrtValueInt(3)),
		"reverse snapshot fixture failed"
	);
	testRequire(
		xrtValueIterRBegin(pArray, &tIterator),
		"reverse snapshot begin failed"
	);
	xrtValueRelease(pArray);
	for ( size_t i = 0; i < 3; i++ ) {
		pItem = xrtValueIterNext(&tIterator, &Key);
		testRequire(
			(pItem != NULL) && (Key.Type == XVALUE_KEY_INDEX) &&
			(Key.Index == (2u - i)) &&
			(testValueCowInt(pItem) == arrExpected[i]),
			"reverse snapshot item mismatch"
		);
	}
	testRequire(
		xrtValueIterNext(&tIterator, &Key) == NULL,
		"reverse snapshot end mismatch"
	);
	xrtValueIterEnd(&tIterator);
}



/* 运行 Value COW 与快照回归。 */
int main(void)
{
	testValueCowRoots();
	testValueCowNested();
	testValueIteratorSnapshot();
	testValueIteratorLifetime();
	printf("[PASS] value COW\n");
	return 0;
}
