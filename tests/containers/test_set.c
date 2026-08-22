#include "../test.h"



/* 自定义元素用于验证规范元素、碰撞和策略扩展。 */
typedef struct testsetitem {
	int Key;
	char Name[16];
} testsetitem;



/* 生命周期状态记录复制、释放和访问次数。 */
typedef struct testsetstate {
	size_t HashCount;
	size_t CopyCount;
	size_t DropCount;
	size_t VisitCount;
} testsetstate;



/* 回调重入状态分别记录五类回调的访问边界。 */
typedef struct testsetreentrystate {
	xset* Set;
	bool HashTried;
	bool HashBlocked;
	bool EqualTried;
	bool EqualBlocked;
	bool CopyTried;
	bool CopyBlocked;
	bool DropTried;
	bool DropBlocked;
	bool VisitReadAllowed;
	bool VisitRemoveBlocked;
	bool VisitDestroyBlocked;
} testsetreentrystate;



/* 错误复制状态记录不一致规范元素是否得到回滚释放。 */
typedef struct testsetbadcopystate {
	size_t DropCount;
} testsetbadcopystate;



/* 带前后保护区的集合用于验证跨结构输出区间。 */
typedef struct testsetwrapper {
	unsigned char Prefix[64];
	xset Set;
	unsigned char Suffix[64];
} testsetwrapper;



/* 拥有型元素持有一份独立字符串。 */
typedef struct testsetowned {
	int Key;
	str Text;
} testsetowned;



/* 自定义哈希故意制造碰撞并记录调用次数。 */
static uint64 testSetItemHash(const void* pItem, ptr pUserData)
{
	const testsetitem* pValue = (const testsetitem*)pItem;
	testsetstate* pState = (testsetstate*)pUserData;

	pState->HashCount++;
	return (uint64)(pValue->Key & 3);
}



/* 自定义相等器只比较业务键，保留第一次插入的规范元素。 */
static bool testSetItemEqual(
	const void* pLeft,
	const void* pRight,
	ptr pUserData
)
{
	const testsetitem* pA = (const testsetitem*)pLeft;
	const testsetitem* pB = (const testsetitem*)pRight;

	(void)pUserData;
	return pA->Key == pB->Key;
}



/* 拥有型元素按整数业务键计算哈希。 */
static uint64 testSetOwnedHash(const void* pItem, ptr pUserData)
{
	const testsetowned* pValue = (const testsetowned*)pItem;

	(void)pUserData;
	return (uint64)(uint32)pValue->Key;
}



/* 拥有型元素只按整数业务键判断相等。 */
static bool testSetOwnedEqual(
	const void* pLeft,
	const void* pRight,
	ptr pUserData
)
{
	const testsetowned* pA = (const testsetowned*)pLeft;
	const testsetowned* pB = (const testsetowned*)pRight;

	(void)pUserData;
	return pA->Key == pB->Key;
}



/* 拥有型元素复制器建立独立字符串副本。 */
static bool testSetOwnedCopy(ptr pTarget, const void* pSource, ptr pUserData)
{
	testsetowned* pTargetValue = (testsetowned*)pTarget;
	const testsetowned* pSourceValue = (const testsetowned*)pSource;
	testsetstate* pState = (testsetstate*)pUserData;
	size_t iSize = strlen(pSourceValue->Text) + 1u;

	pTargetValue->Text = (str)xrtMalloc(iSize);
	if ( pTargetValue->Text == NULL ) {
		return false;
	}
	pTargetValue->Key = pSourceValue->Key;
	memcpy(pTargetValue->Text, pSourceValue->Text, iSize);
	pState->CopyCount++;
	return true;
}



/* 拥有型元素释放器销毁内部字符串。 */
static void testSetOwnedDrop(ptr pItem, ptr pUserData)
{
	testsetowned* pValue = (testsetowned*)pItem;
	testsetstate* pState = (testsetstate*)pUserData;

	xrtFree(pValue->Text);
	pValue->Text = NULL;
	pState->DropCount++;
}



/* 访问器验证插入顺序并在第三项主动停止。 */
static bool testSetVisit(const void* pItem, ptr pUserData)
{
	static const int arrExpected[] = { 1, 3, 2 };
	const int* pValue = (const int*)pItem;
	testsetstate* pState = (testsetstate*)pUserData;

	testRequire(*pValue == arrExpected[pState->VisitCount], "set visit order mismatch");
	pState->VisitCount++;
	return pState->VisitCount < 3;
}



/* 重入哈希器验证同一集合的容量修改被忙状态拒绝。 */
static uint64 testSetReentryHash(const void* pItem, ptr pUserData)
{
	testsetreentrystate* pState = (testsetreentrystate*)pUserData;

	(void)pItem;
	if ( !pState->HashTried ) {
		pState->HashTried = true;
		xrtClearError();
		pState->HashBlocked =
			!xrtSetReserve(pState->Set, 32) &&
			(xrtErrorKind(xrtGetError()) == XERR_STATE);
	}
	return 1;
}



/* 重入相等器验证同一集合不能在查找过程中被清空。 */
static bool testSetReentryEqual(
	const void* pLeft,
	const void* pRight,
	ptr pUserData
)
{
	testsetreentrystate* pState = (testsetreentrystate*)pUserData;

	if ( !pState->EqualTried ) {
		pState->EqualTried = true;
		xrtClearError();
		xrtSetClear(pState->Set);
		pState->EqualBlocked =
			xrtErrorKind(xrtGetError()) == XERR_STATE;
	}
	return *(const int*)pLeft == *(const int*)pRight;
}



/* 重入复制器验证尚未提交的条目不能触发集合销毁。 */
static bool testSetReentryCopy(
	ptr pTarget,
	const void* pSource,
	ptr pUserData
)
{
	testsetreentrystate* pState = (testsetreentrystate*)pUserData;

	if ( !pState->CopyTried ) {
		pState->CopyTried = true;
		xrtClearError();
		xrtSetDestroy(pState->Set);
		pState->CopyBlocked =
			xrtErrorKind(xrtGetError()) == XERR_STATE;
	}
	memcpy(pTarget, pSource, sizeof(int));
	return true;
}



/* 重入释放器验证删除期间不能销毁同一集合。 */
static void testSetReentryDrop(ptr pItem, ptr pUserData)
{
	testsetreentrystate* pState = (testsetreentrystate*)pUserData;

	(void)pItem;
	if ( !pState->DropTried ) {
		pState->DropTried = true;
		xrtClearError();
		xrtSetDestroy(pState->Set);
		pState->DropBlocked =
			xrtErrorKind(xrtGetError()) == XERR_STATE;
	}
}



/* 重入访问器允许查询，但拒绝删除和销毁当前集合。 */
static bool testSetReentryVisitor(const void* pItem, ptr pUserData)
{
	testsetreentrystate* pState = (testsetreentrystate*)pUserData;

	pState->VisitReadAllowed =
		(xrtSetCount(pState->Set) == 1) &&
		xrtSetHas(pState->Set, pItem);

	xrtClearError();
	pState->VisitRemoveBlocked =
		!xrtSetRemove(pState->Set, pItem) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE);

	xrtClearError();
	xrtSetDestroy(pState->Set);
	pState->VisitDestroyBlocked =
		xrtErrorKind(xrtGetError()) == XERR_STATE;
	return false;
}



/* 错误复制器故意改变键，用于验证提交前一致性检查。 */
static bool testSetBadCopy(
	ptr pTarget,
	const void* pSource,
	ptr pUserData
)
{
	(void)pUserData;
	*(int*)pTarget = *(const int*)pSource + 1;
	return true;
}



/* 错误复制回滚时记录规范元素释放次数。 */
static void testSetBadCopyDrop(ptr pItem, ptr pUserData)
{
	testsetbadcopystate* pState = (testsetbadcopystate*)pUserData;

	(void)pItem;
	pState->DropCount++;
}



/* 验证基础增删查、稳定地址、扩缩容和外置迭代器。 */
static void testSetBasic(void)
{
	xset tSet;
	xsetiter tIterator;
	const int* pStable;
	const int* pValue;
	bool bNew;
	int iValue;

	testRequire(xrtSetInit(&tSet, sizeof(int)), "set init failed");
	testRequire((xrtSetCount(&tSet) == 0) && (xrtSetCapacity(&tSet) == 0), "empty set state mismatch");
	iValue = 7;
	pStable = (const int*)xrtSetGetOrAdd(&tSet, &iValue, &bNew);
	testRequire((pStable != NULL) && bNew && (*pStable == 7), "set first insert failed");
	testRequire(xrtSetGetOrAdd(&tSet, &iValue, &bNew) == pStable && !bNew, "set duplicate mismatch");

	for ( int i = 0; i < 20000; i++ ) {
		iValue = i + 100;
		testRequire(xrtSetAdd(&tSet, &iValue), "set growth add failed");
	}
	testRequire(xrtSetGet(&tSet, &(int){ 7 }) == pStable, "set rehash moved element");
	testRequire(xrtSetCount(&tSet) == 20001, "set growth count mismatch");
	for ( int i = 0; i < 200000; i++ ) {
		iValue = (i % 20000) + 100;
		testRequire(xrtSetHas(&tSet, &iValue), "set lookup stress mismatch");
	}

	testRequire(xrtSetIterBegin(&tSet, &tIterator), "set iterator begin failed");
	pValue = (const int*)xrtSetIterNext(&tIterator);
	testRequire(pValue == pStable, "set iterator first mismatch");
	testRequire(xrtSetReserve(&tSet, 50000), "set reserve failed");
	testRequire(xrtSetIterNext(&tIterator) != NULL, "set reserve invalidated iterator");
	testRequire(xrtSetTrim(&tSet), "set trim failed");
	testRequire(xrtSetIterNext(&tIterator) != NULL, "set trim invalidated iterator");
	xrtSetIterEnd(&tIterator);

	testRequire(xrtSetTake(&tSet, &(int){ 7 }, &iValue), "set take failed");
	testRequire((iValue == 7) && !xrtSetHas(&tSet, &iValue), "set take result mismatch");
	testRequire(xrtSetRemove(&tSet, &(int){ 100 }), "set remove failed");
	testRequire(!xrtSetRemove(&tSet, &(int){ -1 }), "set missing remove should fail");
	xrtSetClear(&tSet);
	testRequire(xrtSetCount(&tSet) == 0, "set clear count mismatch");
	testRequire(xrtSetTrim(&tSet) && (xrtSetCapacity(&tSet) == 0), "empty set trim mismatch");
	xrtSetUnit(&tSet);
}



/* 验证自定义键策略、规范元素和结构修改失效规则。 */
static void testSetPolicyAndOrder(void)
{
	xset tSet;
	xsetiter tIterator;
	testsetstate tState = { 0, 0, 0, 0 };
	testsetitem tFirst = { 5, "first" };
	testsetitem tEqual = { 5, "equal" };
	const testsetitem* pStored;
	int iFirst = 1;
	int iSecond = 2;
	int iThird = 3;

	testRequire(xrtSetInit(&tSet, sizeof(testsetitem)), "policy set init failed");
	testRequire(
		xrtSetSetKeyPolicy(&tSet, testSetItemHash, testSetItemEqual, &tState),
		"set key policy failed"
	);
	testRequire(xrtSetAdd(&tSet, &tFirst), "policy set first add failed");
	testRequire(xrtSetAdd(&tSet, &tEqual), "policy set duplicate add failed");
	pStored = (const testsetitem*)xrtSetGet(&tSet, &tEqual);
	testRequire(
		(pStored != NULL) &&
		(strcmp(pStored->Name, "first") == 0) &&
		(xrtSetCount(&tSet) == 1) &&
		(tState.HashCount != 0),
		"set canonical element mismatch"
	);
	xrtClearError();
	testRequire(!xrtSetSetKeyPolicy(&tSet, NULL, NULL, NULL), "non-empty set accepted policy change");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "set policy state error mismatch");
	xrtSetUnit(&tSet);

	testRequire(xrtSetInit(&tSet, sizeof(int)), "ordered set init failed");
	testRequire(xrtSetAdd(&tSet, &iFirst), "ordered set first add failed");
	testRequire(xrtSetAdd(&tSet, &iSecond), "ordered set second add failed");
	testRequire(xrtSetAdd(&tSet, &iThird), "ordered set third add failed");
	testRequire(xrtSetRemove(&tSet, &iSecond), "ordered set remove failed");
	testRequire(xrtSetAdd(&tSet, &iSecond), "ordered set reinsert failed");
	testRequire(xrtSetVisit(&tSet, testSetVisit, &tState) == 3, "set visitor count mismatch");
	testRequire(xrtSetIterRBegin(&tSet, &tIterator), "set reverse iterator begin failed");
	testRequire(*(const int*)xrtSetIterNext(&tIterator) == 2, "set reverse order mismatch");
	testRequire(xrtSetAdd(&tSet, &(int){ 4 }), "set iterator mutation setup failed");
	xrtClearError();
	testRequire(xrtSetIterNext(&tIterator) == NULL, "mutated set iterator should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "set iterator state error mismatch");
	xrtSetUnit(&tSet);
}



/* 验证资源生命周期、克隆、移交和释放次数。 */
static void testSetLifecycle(void)
{
	xset tSet;
	xset* pClone;
	testsetstate tState = { 0, 0, 0, 0 };
	testsetowned tOne = { 1, "one" };
	testsetowned tSame = { 1, "same" };
	testsetowned tTwo = { 2, "two" };
	testsetowned tTaken;
	const testsetowned* pStored;

	testRequire(xrtSetInit(&tSet, sizeof(testsetowned)), "lifecycle set init failed");
	testRequire(
		xrtSetSetKeyPolicy(&tSet, testSetOwnedHash, testSetOwnedEqual, &tState),
		"owned set key policy failed"
	);
	testRequire(
		xrtSetSetLifecycle(&tSet, testSetOwnedCopy, testSetOwnedDrop, &tState),
		"owned set lifecycle failed"
	);
	testRequire(xrtSetAdd(&tSet, &tOne), "owned set first add failed");
	testRequire(xrtSetAdd(&tSet, &tSame), "owned set duplicate add failed");
	testRequire(xrtSetAdd(&tSet, &tTwo), "owned set second add failed");
	testRequire((tState.CopyCount == 2) && (tState.DropCount == 0), "owned set duplicate lifecycle mismatch");
	pStored = (const testsetowned*)xrtSetGet(&tSet, &tSame);
	testRequire(strcmp(pStored->Text, "one") == 0, "owned set canonical resource mismatch");

	pClone = xrtSetClone(&tSet);
	testRequire(pClone != NULL, "owned set clone failed");
	testRequire((tState.CopyCount == 4) && xrtSetEqual(&tSet, pClone), "owned set clone mismatch");
	memset(&tTaken, 0, sizeof(tTaken));
	testRequire(xrtSetTake(&tSet, &tOne, &tTaken), "owned set take failed");
	testRequire((tState.DropCount == 0) && (strcmp(tTaken.Text, "one") == 0), "owned set take lifecycle mismatch");
	xrtFree(tTaken.Text);
	testRequire(xrtSetRemove(&tSet, &tTwo), "owned set remove failed");
	testRequire(tState.DropCount == 1, "owned set remove did not drop");
	xrtSetUnit(&tSet);
	testRequire(tState.DropCount == 1, "empty owned set unit changed drops");
	xrtSetDestroy(pClone);
	testRequire(tState.DropCount == 3, "owned set clone destroy mismatch");
}



/* 验证所有用户回调均不能重入破坏当前集合。 */
static void testSetCallbackReentry(void)
{
	testsetreentrystate tState = { 0 };
	xset* pSet;
	int iFirst = 1;
	int iSecond = 2;

	pSet = xrtSetCreate(sizeof(int));
	testRequire(pSet != NULL, "set callback reentry create failed");
	tState.Set = pSet;
	testRequire(
		xrtSetSetKeyPolicy(
			pSet,
			testSetReentryHash,
			testSetReentryEqual,
			&tState
		),
		"set callback reentry key policy failed"
	);
	testRequire(
		xrtSetSetLifecycle(
			pSet,
			testSetReentryCopy,
			testSetReentryDrop,
			&tState
		),
		"set callback reentry lifecycle failed"
	);
	testRequire(xrtSetAdd(pSet, &iFirst), "set callback reentry first add failed");
	testRequire(xrtSetAdd(pSet, &iSecond), "set callback reentry second add failed");
	testRequire(xrtSetRemove(pSet, &iFirst), "set callback reentry remove failed");
	testRequire(
		xrtSetVisit(
			pSet,
			testSetReentryVisitor,
			&tState
		) == 1,
		"set callback reentry visit count mismatch"
	);

	testRequire(tState.HashTried && tState.HashBlocked, "set hash callback reentry was allowed");
	testRequire(tState.EqualTried && tState.EqualBlocked, "set equal callback reentry was allowed");
	testRequire(tState.CopyTried && tState.CopyBlocked, "set copy callback reentry was allowed");
	testRequire(tState.DropTried && tState.DropBlocked, "set drop callback reentry was allowed");
	testRequire(tState.VisitReadAllowed, "set visitor query was rejected");
	testRequire(tState.VisitRemoveBlocked, "set visitor removed current item");
	testRequire(tState.VisitDestroyBlocked, "set visitor destroyed current set");
	testRequire(
		(xrtSetCount(pSet) == 1) && xrtSetHas(pSet, &iSecond),
		"set callback reentry changed final state"
	);

	xrtClearError();
	xrtSetDestroy(pSet);
}



/* 验证自定义复制器不能建立与来源键不一致的规范元素。 */
static void testSetCopyConsistency(void)
{
	testsetbadcopystate tState = { 0 };
	xset tSet;
	int iValue = 7;

	testRequire(xrtSetInit(&tSet, sizeof(int)), "set bad copy init failed");
	testRequire(
		xrtSetSetLifecycle(
			&tSet,
			testSetBadCopy,
			testSetBadCopyDrop,
			&tState
		),
		"set bad copy lifecycle failed"
	);
	xrtClearError();
	testRequire(!xrtSetAdd(&tSet, &iValue), "set accepted inconsistent copied key");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "set bad copy error mismatch");
	testRequire(
		(xrtSetCount(&tSet) == 0) && (tState.DropCount == 1),
		"set bad copy did not roll back canonical item"
	);
	xrtClearError();
	xrtSetUnit(&tSet);
}



/* 验证并集、交集、差集、对称差和集合关系。 */
static void testSetAlgebra(void)
{
	xset tLeft;
	xset tRight;
	xset* pUnion;
	xset* pIntersection;
	xset* pDifference;
	xset* pSymmetric;

	testRequire(xrtSetInit(&tLeft, sizeof(int)), "left algebra set init failed");
	testRequire(xrtSetInit(&tRight, sizeof(int)), "right algebra set init failed");
	for ( int i = 1; i <= 3; i++ ) {
		testRequire(xrtSetAdd(&tLeft, &i), "left algebra add failed");
	}
	for ( int i = 3; i <= 5; i++ ) {
		testRequire(xrtSetAdd(&tRight, &i), "right algebra add failed");
	}
	pUnion = xrtSetUnion(&tLeft, &tRight);
	pIntersection = xrtSetIntersection(&tLeft, &tRight);
	pDifference = xrtSetDifference(&tLeft, &tRight);
	pSymmetric = xrtSetSymmetricDifference(&tLeft, &tRight);
	testRequire(
		(pUnion != NULL) &&
		(pIntersection != NULL) &&
		(pDifference != NULL) &&
		(pSymmetric != NULL),
		"set algebra allocation failed"
	);
	testRequire((xrtSetCount(pUnion) == 5) && xrtSetHas(pUnion, &(int){ 5 }), "set union mismatch");
	testRequire((xrtSetCount(pIntersection) == 1) && xrtSetHas(pIntersection, &(int){ 3 }), "set intersection mismatch");
	testRequire((xrtSetCount(pDifference) == 2) && !xrtSetHas(pDifference, &(int){ 3 }), "set difference mismatch");
	testRequire((xrtSetCount(pSymmetric) == 4) && !xrtSetHas(pSymmetric, &(int){ 3 }), "set symmetric difference mismatch");
	testRequire(xrtSetIsSubset(&tLeft, pUnion, true), "set proper subset mismatch");
	testRequire(xrtSetIsSuperset(pUnion, &tRight, true), "set proper superset mismatch");
	testRequire(!xrtSetIsDisjoint(&tLeft, &tRight), "set overlap reported disjoint");
	testRequire(
		xrtSetIsDisjoint(pDifference, &tRight),
		"set disjoint relation mismatch"
	);
	testRequire(xrtSetMerge(&tLeft, &tRight), "set merge failed");
	testRequire(xrtSetEqual(&tLeft, pUnion), "set merge result mismatch");

	xrtSetDestroy(pSymmetric);
	xrtSetDestroy(pDifference);
	xrtSetDestroy(pIntersection);
	xrtSetDestroy(pUnion);
	xrtSetUnit(&tRight);
	xrtSetUnit(&tLeft);
}



/* 验证合并保留旧地址，并只在真正新增元素时使迭代器失效。 */
static void testSetMergeStability(void)
{
	xset tTarget;
	xset tDuplicate;
	xset tGrowth;
	xsetiter tIterator;
	const int* pStable;
	int iValue;

	testRequire(xrtSetInit(&tTarget, sizeof(int)), "set stable merge target init failed");
	testRequire(xrtSetInit(&tDuplicate, sizeof(int)), "set stable merge duplicate init failed");
	testRequire(xrtSetInit(&tGrowth, sizeof(int)), "set stable merge growth init failed");
	for ( iValue = 1; iValue <= 2; iValue++ ) {
		testRequire(xrtSetAdd(&tTarget, &iValue), "set stable merge target add failed");
	}
	iValue = 2;
	testRequire(xrtSetAdd(&tDuplicate, &iValue), "set stable merge duplicate add failed");
	for ( iValue = 2; iValue <= 3; iValue++ ) {
		testRequire(xrtSetAdd(&tGrowth, &iValue), "set stable merge growth add failed");
	}
	pStable = (const int*)xrtSetGet(&tTarget, &(int){ 1 });
	testRequire(pStable != NULL, "set stable merge item missing");

	testRequire(xrtSetIterBegin(&tTarget, &tIterator), "set no-op merge iterator begin failed");
	testRequire(xrtSetMerge(&tTarget, &tDuplicate), "set duplicate-only merge failed");
	testRequire(
		(xrtSetGet(&tTarget, &(int){ 1 }) == pStable) &&
		(xrtSetIterNext(&tIterator) != NULL),
		"set duplicate-only merge changed address or iterator"
	);
	xrtSetIterEnd(&tIterator);

	testRequire(xrtSetIterBegin(&tTarget, &tIterator), "set growth merge iterator begin failed");
	testRequire(xrtSetMerge(&tTarget, &tGrowth), "set growth merge failed");
	testRequire(
		(xrtSetGet(&tTarget, &(int){ 1 }) == pStable) &&
		(xrtSetCount(&tTarget) == 3),
		"set growth merge moved old item or lost new item"
	);
	xrtClearError();
	testRequire(xrtSetIterNext(&tIterator) == NULL, "set growth merge kept stale iterator");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "set growth merge iterator error mismatch");

	xrtSetUnit(&tGrowth);
	xrtSetUnit(&tDuplicate);
	xrtSetUnit(&tTarget);
}



/* 验证输入和完整输出区间都不能覆盖集合拥有的内存。 */
static void testSetAliasSafety(void)
{
	testsetwrapper tWrapper = { 0 };
	unsigned char arrFirst[64] = { 1 };
	unsigned char arrSecond[64] = { 2 };
	const void* pFirst;
	const void* pSecond;
	ptr pCrossing;
	size_t iBucketBytes;

	testRequire(xrtSetInit(&tWrapper.Set, sizeof(arrFirst)), "set alias init failed");
	pFirst = xrtSetGetOrAdd(&tWrapper.Set, arrFirst, NULL);
	pSecond = xrtSetGetOrAdd(&tWrapper.Set, arrSecond, NULL);
	testRequire((pFirst != NULL) && (pSecond != NULL), "set alias setup failed");

	pCrossing = (bytes)&tWrapper.Set - 16u;
	xrtClearError();
	testRequire(
		!xrtSetTake(&tWrapper.Set, arrFirst, pCrossing),
		"set take accepted output crossing set structure"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "set structure alias error mismatch");

	xrtClearError();
	testRequire(
		!xrtSetTake(
			&tWrapper.Set,
			arrFirst,
			(bytes)pSecond - 8u
		),
		"set take accepted output crossing another entry"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "set entry alias error mismatch");

	iBucketBytes = tWrapper.Set.BucketCount * sizeof(xsetentry*);
	xrtClearError();
	testRequire(
		!xrtSetTake(
			&tWrapper.Set,
			arrFirst,
			(bytes)tWrapper.Set.Buckets + iBucketBytes - 8u
		),
		"set take accepted output crossing bucket array"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "set bucket alias error mismatch");

	xrtClearError();
	testRequire(
		!xrtSetAdd(&tWrapper.Set, &tWrapper.Set.Count),
		"set accepted metadata as item source"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "set metadata source error mismatch");

	xrtClearError();
	testRequire(
		!xrtSetAdd(&tWrapper.Set, tWrapper.Set.Buckets),
		"set accepted bucket array as item source"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "set bucket source error mismatch");
	testRequire(
		(xrtSetCount(&tWrapper.Set) == 2) &&
		(xrtSetGet(&tWrapper.Set, arrFirst) == pFirst) &&
		(xrtSetGet(&tWrapper.Set, arrSecond) == pSecond),
		"set alias rejection changed collection"
	);
	xrtClearError();
	xrtSetUnit(&tWrapper.Set);
}



/* 验证参数、对齐、溢出和危险输出别名。 */
static void testSetErrors(void)
{
	xset tSet;
	xset* pCreated;
	const void* pStored;
	bool bNew = true;

	testRequire(xrtSetInitAligned(&tSet, 64, 64), "aligned set init failed");
	pStored = xrtSetGetOrAdd(&tSet, (unsigned char[64]){ 1 }, &bNew);
	testRequire(
		(pStored != NULL) && bNew && (((uintptr_t)pStored & 63u) == 0),
		"set item alignment mismatch"
	);
	xrtClearError();
	testRequire(!xrtSetTake(&tSet, pStored, (ptr)pStored), "set take accepted internal output");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "set take alias error mismatch");
	xrtSetUnit(&tSet);

	pCreated = xrtSetCreate(sizeof(int));
	testRequire(pCreated != NULL, "set create failed");
	xrtSetDestroy(pCreated);
	testRequire(!xrtSetInit(&tSet, 0), "zero-sized set should fail");
	testRequire(!xrtSetInitAligned(&tSet, sizeof(int), 24), "invalid set alignment should fail");
	xrtClearError();
	testRequire(!xrtSetInitAligned(&tSet, SIZE_MAX, 16), "overflowing set layout should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "set layout overflow error mismatch");

	testRequire(xrtSetInit(&tSet, sizeof(int)), "error set init failed");
	xrtClearError();
	testRequire(!xrtSetSetKeyPolicy(&tSet, testSetItemHash, NULL, NULL), "set accepted incomplete key policy");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "set incomplete policy error mismatch");
	xrtClearError();
	testRequire(!xrtSetSetLifecycle(&tSet, testSetOwnedCopy, NULL, NULL), "set accepted incomplete lifecycle");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "set incomplete lifecycle error mismatch");
	bNew = true;
	xrtClearError();
	testRequire(xrtSetGetOrAdd(NULL, &(int){ 1 }, &bNew) == NULL, "null set should fail");
	testRequire(!bNew && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT), "null set error mismatch");
	xrtSetUnit(&tSet);
}



/* 运行通用集合的全部契约测试。 */
int main(void)
{
	testSetBasic();
	testSetPolicyAndOrder();
	testSetLifecycle();
	testSetCallbackReentry();
	testSetCopyConsistency();
	testSetAlgebra();
	testSetMergeStability();
	testSetAliasSafety();
	testSetErrors();
	printf("[PASS] set\n");
	return 0;
}
