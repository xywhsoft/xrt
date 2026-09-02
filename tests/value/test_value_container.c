#include "../test.h"



/* 读取测试值中的整数并立即断言类型。 */
static int64 testValueReadInt(const xvalue* pValue)
{
	int64 iValue = 0;

	testRequire(xrtValueGetInt(pValue, &iValue), "expected integer value");
	return iValue;
}




/* 测试策略按对象 id 字段定义值身份。 */
static uint64 testValueIdentityHash(const xvalue* pValue, ptr pUserData)
{
	int64 iValue = 0;
	xvalue* pField = xrtValueObjectGet(pValue, XRT_STR_LITERAL("id"));

	(void)pUserData;
	return (pField != NULL) && xrtValueGetInt(pField, &iValue)
		? (uint64)iValue
		: 0;
}




static bool testValueIdentityEqual(
	const xvalue* pLeft,
	const xvalue* pRight,
	ptr pUserData
)
{
	int64 iLeft = 0;
	int64 iRight = 0;
	xvalue* pLeftField = xrtValueObjectGet(pLeft, XRT_STR_LITERAL("id"));
	xvalue* pRightField = xrtValueObjectGet(pRight, XRT_STR_LITERAL("id"));

	(void)pUserData;
	return (pLeftField != NULL) && (pRightField != NULL) &&
		xrtValueGetInt(pLeftField, &iLeft) &&
		xrtValueGetInt(pRightField, &iRight) &&
		(iLeft == iRight);
}




static xvalue* testValueIdentityObject(int64 iId)
{
	xvalue* pValue = xrtValueObject();

	if ( (pValue == NULL) ||
		 !xrtValueTypeIdBind(pValue, UINT64_C(0x1122334455667788)) ||
		 !xrtValueIdentityBind(
			pValue,
			testValueIdentityHash,
			testValueIdentityEqual,
			NULL
		 ) ||
		 !xrtValueObjectSetNew(
			pValue,
			XRT_STR_LITERAL("id"),
			xrtValueInt(iId)
		 ) ) {
		xrtValueRelease(pValue);
		return NULL;
	}
	return pValue;
}



typedef struct testvaluefinalizerstate {
	int Calls;
	int64 Observed;
	bool MutationWorked;
	bool RetainRejected;
	bool CloneRejected;
	bool RebindRejected;
} testvaluefinalizerstate;



/* Finalizer callbacks receive a readable/mutable borrow but cannot resurrect
 * or duplicate the object whose final backing owner is being released. */
static void testValueObjectFinalizer(xvalue* pObject, ptr pUserData)
{
	testvaluefinalizerstate* pState = (testvaluefinalizerstate*)pUserData;
	xvalue* pField = xrtValueObjectGet(pObject, XRT_STR_LITERAL("value"));

	testRequire((pState != NULL) && (pField != NULL),
		"object finalizer did not receive a readable field view");
	pState->Calls += 1;
	pState->Observed = testValueReadInt(pField);
	pState->MutationWorked = xrtValueObjectSetNew(
		pObject,
		XRT_STR_LITERAL("during-finalize"),
		xrtValueInt(1)
	);
	xrtClearError();
	pState->RetainRejected = xrtValueRetain(pObject) == NULL &&
		xrtErrorKind(xrtGetError()) == XERR_STATE;
	xrtClearError();
	pState->CloneRejected = xrtValueClone(pObject) == NULL &&
		xrtErrorKind(xrtGetError()) == XERR_STATE;
	xrtClearError();
	pState->RebindRejected = !xrtValueObjectFinalizerBind(
		pObject,
		testValueObjectFinalizer,
		pUserData
	) && xrtErrorKind(xrtGetError()) == XERR_STATE;
	xrtClearError();
}



/* Verify one backing-owned finalization duty, shared-shell behavior and the
 * identity-preserving rejection of a COW split after binding. */
static void testValueObjectFinalizerLifecycle(void)
{
	testvaluefinalizerstate State = {0};
	xvalue* pObject = xrtValueObjectLifo();
	xvalue* pClone;

	testRequire(
		(pObject != NULL) &&
		xrtValueObjectSetNew(
			pObject,
			XRT_STR_LITERAL("value"),
			xrtValueInt(41)
		) &&
		xrtValueObjectFinalizerBind(
			pObject,
			testValueObjectFinalizer,
			&State
		),
		"object finalizer fixture creation failed"
	);
	pClone = xrtValueClone(pObject);
	testRequire(pClone != NULL, "finalizer-backed object Clone failed");
	xrtClearError();
	testRequire(
		!xrtValueObjectSetNew(
			pObject,
			XRT_STR_LITERAL("split"),
			xrtValueInt(2)
		) && xrtErrorKind(xrtGetError()) == XERR_STATE,
		"finalizer-backed object allowed a duplicate-duty COW split"
	);
	xrtClearError();
	xrtValueRelease(pObject);
	testRequire(State.Calls == 0,
		"object finalizer ran before the last shared backing owner");
	xrtValueRelease(pClone);
	testRequire(
		(State.Calls == 1) && (State.Observed == 41) &&
		State.MutationWorked && State.RetainRejected && State.CloneRejected &&
		State.RebindRejected,
		"object finalizer lifecycle contract mismatch"
	);
}



/* 验证统一动态容器容量查询与 Reserve 使用同一公开边界。 */
static void testValueCapacity(void)
{
	xvalue* pArray = xrtValueArray();
	xvalue* pSet = xrtValueSet();
	xvalue* pObject = xrtValueObject();
	xvalue* pIntMap = xrtValueIntMap();

	testRequire(
		(pArray != NULL) && (pSet != NULL) &&
		(pObject != NULL) && (pIntMap != NULL),
		"capacity fixture creation failed"
	);
	testRequire(
		xrtValueReserve(pArray, 8) && (xrtValueCapacity(pArray) >= 8),
		"array capacity mismatch"
	);
	testRequire(
		xrtValueReserve(pSet, 8) && (xrtValueCapacity(pSet) >= 8),
		"set capacity mismatch"
	);
	testRequire(
		xrtValueReserve(pObject, 8) && (xrtValueCapacity(pObject) >= 8),
		"object capacity mismatch"
	);
	testRequire(
		xrtValueIntMapTrim(pIntMap, 1) == 0,
		"empty int-map trim mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtValueCapacity(pIntMap) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED),
		"int-map capacity did not report unsupported"
	);
	xrtValueRelease(pIntMap);
	xrtValueRelease(pObject);
	xrtValueRelease(pSet);
	xrtValueRelease(pArray);
}



/* 验证数组、负索引和三种所有权写入路径。 */
static void testValueArrayOps(void)
{
	xvalue* pArray = xrtValueArray();
	xvalue* pBorrowed = xrtValueInt(20);
	xvalue* pTaken = xrtValueInt(10);
	xvalue* pResult;

	testRequire((pArray != NULL) && (pBorrowed != NULL) && (pTaken != NULL), "array fixture failed");
	testRequire(xrtValueArrayAppendTake(pArray, &pTaken) && (pTaken == NULL), "array append take failed");
	testRequire(xrtValueArrayAppend(pArray, pBorrowed), "array borrowed append failed");
	testRequire(xrtValueArrayAppendNew(pArray, xrtValueInt(30)), "array append new failed");
	testRequire((xrtValueCount(pArray) == 3) && (testValueReadInt(xrtValueArrayAt(pArray, -1)) == 30), "array count or negative index mismatch");
	testRequire(xrtValueArrayInsertNew(pArray, 1, xrtValueInt(15)), "array insert new failed");
	testRequire(testValueReadInt(xrtValueArrayGet(pArray, 1)) == 15, "array insert result mismatch");
	testRequire(xrtValueArraySetNew(pArray, 2, xrtValueInt(25)), "array set new failed");
	testRequire(testValueReadInt(xrtValueArrayGet(pArray, 2)) == 25, "array set result mismatch");
	testRequire(xrtValueArraySwap(pArray, 0, 3), "array swap failed");
	pResult = xrtValueArrayTake(pArray, 0);
	testRequire((pResult != NULL) && (testValueReadInt(pResult) == 30), "array take mismatch");
	xrtValueRelease(pResult);
	pResult = xrtValueArrayPop(pArray);
	testRequire((pResult != NULL) && (testValueReadInt(pResult) == 10), "array pop mismatch");
	xrtValueRelease(pResult);
	testRequire(xrtValueArrayRemove(pArray, 0, 1) && (xrtValueCount(pArray) == 1), "array remove mismatch");
	testRequire(xrtValueReserve(pArray, 128) && xrtValueTrim(pArray), "array capacity operations failed");
	xrtValueRelease(pBorrowed);
	xrtValueRelease(pArray);
}



/* 验证稀疏整数键保持负键和有序迭代。 */
static void testValueIntMapOps(void)
{
	xvalue* pMap = xrtValueIntMap();
	xvalueiter tIterator;
	xvaluekey Key;
	xvalue* pItem;
	int64 arrExpected[] = { -7, 2, 100 };
	size_t i = 0;

	testRequire(pMap != NULL, "int map create failed");
	testRequire(xrtValueIntMapSetNew(pMap, 100, xrtValueInt(3)), "int map high key failed");
	testRequire(xrtValueIntMapSetNew(pMap, -7, xrtValueInt(1)), "int map negative key failed");
	testRequire(xrtValueIntMapSetNew(pMap, 2, xrtValueInt(2)), "int map middle key failed");
	testRequire(xrtValueIntMapHas(pMap, -7) && (testValueReadInt(xrtValueIntMapGet(pMap, -7)) == 1), "int map negative lookup mismatch");
	testRequire(xrtValueIterBegin(pMap, &tIterator), "int map iterator begin failed");
	while ( (pItem = xrtValueIterNext(&tIterator, &Key)) != NULL ) {
		testRequire((Key.Type == XVALUE_KEY_INT) && (Key.Integer == arrExpected[i]), "int map iterator key mismatch");
		testRequire(testValueReadInt(pItem) == (int64)(i + 1), "int map iterator value mismatch");
		i++;
	}
	xrtValueIterEnd(&tIterator);
	testRequire(i == 3, "int map iterator count mismatch");
	pItem = xrtValueIntMapTake(pMap, 2);
	testRequire((pItem != NULL) && (testValueReadInt(pItem) == 2), "int map take failed");
	xrtValueRelease(pItem);
	testRequire(xrtValueIntMapRemove(pMap, 100), "int map remove failed");
	testRequire(xrtValueTrim(pMap), "int map trim failed");
	xrtValueRelease(pMap);
}



/* 验证拥有式迭代器保持逆序快照，并允许空指针销毁。 */
static void testValueOwnedIterator(void)
{
	xvalue* pArray = xrtValueArray();
	xvalueiter* pIterator;
	xvaluekey Key;
	xvalue* pItem;
	int64 arrExpected[] = { 30, 20, 10 };
	size_t i = 0;

	testRequire(pArray != NULL, "owned iterator fixture failed");
	testRequire(
		xrtValueArrayAppendNew(pArray, xrtValueInt(10)) &&
		xrtValueArrayAppendNew(pArray, xrtValueInt(20)) &&
		xrtValueArrayAppendNew(pArray, xrtValueInt(30)),
		"owned iterator fixture append failed"
	);
	pIterator = xrtValueIterRCreate(pArray);
	testRequire(pIterator != NULL, "owned reverse iterator create failed");

	/* 创建后修改外壳不能改变迭代器已经持有的 backing 快照。 */
	testRequire(
		xrtValueArrayAppendNew(pArray, xrtValueInt(40)),
		"owned iterator source mutation failed"
	);
	while ( (pItem = xrtValueIterNext(pIterator, &Key)) != NULL ) {
		testRequire(
			(Key.Type == XVALUE_KEY_INDEX) &&
			(Key.Index == (2 - i)) &&
			(testValueReadInt(pItem) == arrExpected[i]),
			"owned reverse iterator value mismatch"
		);
		i++;
	}
	testRequire(i == 3, "owned reverse iterator count mismatch");
	xrtValueIterDestroy(pIterator);
	xrtValueIterDestroy(NULL);
	xrtValueRelease(pArray);
}



/* 验证对象首次插入顺序、二进制字符串键和替换语义。 */
static void testValueObjectOps(void)
{
	char arrBinaryKey[] = { 'a', '\0', 'b' };
	xvalue* pObject = xrtValueObject();
	xvalueiter tIterator;
	xvaluekey Key;
	xstrview KeyAt = { 0 };
	xvalue* pItem;
	const char* arrExpected[] = { "beta", "alpha" };
	size_t arrExpectedSize[] = { 4, 5 };
	size_t i = 0;

	testRequire(pObject != NULL, "object create failed");
	testRequire(xrtValueObjectSetNew(pObject, XRT_STR_LITERAL("beta"), xrtValueInt(2)), "object beta set failed");
	testRequire(xrtValueObjectSetNew(pObject, XRT_STR_LITERAL("alpha"), xrtValueInt(1)), "object alpha set failed");
	testRequire(xrtValueObjectSetNew(pObject, XRT_STR_LITERAL("beta"), xrtValueInt(22)), "object replace failed");
	testRequire(xrtValueObjectSetNew(pObject, (xstrview){ arrBinaryKey, 3 }, xrtValueInt(3)), "object binary key failed");
	testRequire(testValueReadInt(xrtValueObjectGet(pObject, XRT_STR_LITERAL("beta"))) == 22, "object replace value mismatch");
	testRequire((testValueReadInt(xrtValueObjectAt(pObject, 0, &KeyAt)) == 22) && (KeyAt.Size == 4) && (memcmp(KeyAt.Data, "beta", 4) == 0), "object ordered access mismatch");
	xrtClearError();
	testRequire((xrtValueObjectAt(pObject, 3, &KeyAt) == NULL) && (xrtErrorKind(xrtGetError()) == XERR_RANGE), "object ordered access range mismatch");
	xrtClearError();
	testRequire(xrtValueIterBegin(pObject, &tIterator), "object iterator begin failed");
	while ( (pItem = xrtValueIterNext(&tIterator, &Key)) != NULL ) {
		testRequire(Key.Type == XVALUE_KEY_STRING, "object iterator key type mismatch");
		if ( i < 2 ) {
			testRequire((Key.String.Size == arrExpectedSize[i]) && (memcmp(Key.String.Data, arrExpected[i], Key.String.Size) == 0), "object insertion order mismatch");
		} else {
			testRequire((Key.String.Size == 3) && (memcmp(Key.String.Data, arrBinaryKey, 3) == 0), "object binary key iteration mismatch");
		}
		i++;
	}
	xrtValueIterEnd(&tIterator);
	testRequire(i == 3, "object iterator count mismatch");
	pItem = xrtValueObjectTake(pObject, XRT_STR_LITERAL("alpha"));
	testRequire((pItem != NULL) && (testValueReadInt(pItem) == 1), "object take mismatch");
	xrtValueRelease(pItem);
	testRequire(xrtValueObjectRemove(pObject, XRT_STR_LITERAL("beta")), "object remove failed");
	xrtValueRelease(pObject);
}



/* 验证集合的规范值、数值跨类型等价和不可哈希拒绝。 */
static void testValueSetOps(void)
{
	xvalue* pSet = xrtValueSet();
	xvalue* pInt = xrtValueInt(42);
	xvalue* pFloat = xrtValueFloat(42.0);
	xvalue* pArray = xrtValueArray();
	xvalue* pStored;

	testRequire((pSet != NULL) && (pInt != NULL) && (pFloat != NULL) && (pArray != NULL), "value set fixture failed");
	testRequire(xrtValueSetAdd(pSet, pInt) && xrtValueSetAdd(pSet, pFloat), "value set numeric add failed");
	testRequire((xrtValueCount(pSet) == 1) && xrtValueSetHas(pSet, pFloat), "value set numeric equality mismatch");
	xrtClearError();
	testRequire(!xrtValueSetAdd(pSet, pArray), "value set accepted container");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_TYPE, "unhashable set error mismatch");
	pStored = xrtValueSetTake(pSet, pFloat);
	testRequire((pStored == pInt) && (xrtValueCount(pSet) == 0), "value set canonical take mismatch");
	xrtValueRelease(pStored);
	xrtValueRelease(pArray);
	xrtValueRelease(pFloat);
	xrtValueRelease(pInt);
	xrtValueRelease(pSet);
}




/* 验证显式值身份容器在直接哈希、Clone 和 Set 中共享同一策略。 */
static void testValueIdentitySetOps(void)
{
	xvalue* pSet = xrtValueSet();
	xvalue* pLeft = testValueIdentityObject(7);
	xvalue* pSame = testValueIdentityObject(7);
	xvalue* pOther = testValueIdentityObject(8);
	xvalue* pClone = xrtValueClone(pLeft);
	uint64 iLeftHash = 0;
	uint64 iSameHash = 0;

	testRequire(
		(pSet != NULL) && (pLeft != NULL) && (pSame != NULL) &&
		(pOther != NULL) && (pClone != NULL),
		"value identity fixture failed"
	);
	testRequire(
		xrtValueHash(pLeft, &iLeftHash) &&
		xrtValueHash(pSame, &iSameHash) &&
		(iLeftHash == iSameHash),
		"value identity hash mismatch"
	);
	testRequire(
		xrtValueSetAdd(pSet, pLeft) &&
		xrtValueSetAdd(pSet, pSame) &&
		xrtValueSetAdd(pSet, pOther) &&
		(xrtValueCount(pSet) == 2),
		"value identity set did not combine equal containers"
	);
	testRequire(
		xrtValueSetHas(pSet, pSame) && xrtValueSetHas(pSet, pClone),
		"value identity set lookup or Clone propagation failed"
	);
	xrtValueRelease(pClone);
	xrtValueRelease(pOther);
	xrtValueRelease(pSame);
	xrtValueRelease(pLeft);
	xrtValueRelease(pSet);
}



/* 验证引用计数容器拒绝直接和间接环且不产生部分写入。 */
static void testValueCycleGuard(void)
{
	xvalue* pRoot = xrtValueArray();
	xvalue* pChild = xrtValueObject();

	testRequire((pRoot != NULL) && (pChild != NULL), "cycle fixture failed");
	testRequire(xrtValueArrayAppend(pRoot, pChild), "cycle child append failed");
	xrtClearError();
	testRequire(!xrtValueObjectSet(pChild, XRT_STR_LITERAL("root"), pRoot), "indirect cycle should fail");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_VALUE) && (xrtValueCount(pChild) == 0), "cycle failure changed target");
	xrtClearError();
	testRequire(!xrtValueArrayAppend(pRoot, pRoot), "direct cycle should fail");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_VALUE) && (xrtValueCount(pRoot) == 1), "direct cycle changed target");
	xrtValueRelease(pChild);
	xrtValueRelease(pRoot);
}



/* 验证共享 backing 分离后允许 DAG 分支，但仍拒绝反向闭环。 */
static void testValueCowCycleGuard(void)
{
	xvalue* pLeft = xrtValueArray();
	xvalue* pRight = xrtValueClone(pLeft);
	xvalue* pBranch;

	testRequire(
		(pLeft != NULL) && (pRight != NULL),
		"COW cycle fixture failed"
	);
	testRequire(
		xrtValueArrayAppend(pLeft, pRight),
		"acyclic shared branch should succeed"
	);
	xrtClearError();
	testRequire(
		!xrtValueArrayAppend(pRight, pLeft),
		"reverse shared branch should form a cycle"
	);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(xrtValueCount(pRight) == 0),
		"reverse cycle failure changed target"
	);
	pBranch = xrtValueClone(pLeft);
	xrtClearError();
	testRequire(
		!xrtValueArrayAppend(pBranch, pBranch),
		"cloned self cycle should fail"
	);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(xrtValueCount(pBranch) == 1),
		"cloned self cycle failure changed target"
	);
	xrtValueRelease(pBranch);
	xrtValueRelease(pRight);
	xrtValueRelease(pLeft);
}



/* 运行动态值容器回归。 */
int main(void)
{
	testValueCapacity();
	testValueObjectFinalizerLifecycle();
	testValueArrayOps();
	testValueIntMapOps();
	testValueOwnedIterator();
	testValueObjectOps();
	testValueSetOps();
	testValueIdentitySetOps();
	testValueCycleGuard();
	testValueCowCycleGuard();
	printf("[PASS] value container\n");
	return 0;
}
