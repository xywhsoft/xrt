#include "../test.h"



/* 向整数键类型树写入一个键值。 */
static bool testTypedTreeSetInt(
	xtypedtree* pTree,
	int32 iKey,
	int64 iValue
)
{
	return xrtTypedTreeSet(pTree, &iKey, &iValue);
}



/* 验证有序查询、上下界、正反迭代、移动和删除。 */
static void testTypedTreeBasic(void)
{
	xtypedtree Tree;
	xtypedtreeiter Iterator;
	const void* pStoredKey;
	int32 arrForward[] = { 1, 2, 3, 4 };
	int32 arrReverse[] = { 4, 3, 2, 1 };
	int32 iOne = 1;
	int32 iTwo = 2;
	int32 iThree = 3;
	int32 iFour = 4;
	int32 iZero = 0;
	int64 iMoved = 40;
	int64 iOutput = 0;
	int64* pValue;
	bool bNew;

	testRequire(
		xrtTypedTreeInit(&Tree, xrtTypeInt32(), xrtTypeInt64()) &&
		testTypedTreeSetInt(&Tree, iThree, 30) &&
		testTypedTreeSetInt(&Tree, iOne, 10) &&
		testTypedTreeSetInt(&Tree, iTwo, 20),
		"typed tree insertion failed"
	);
	testRequire(
		(xrtTypedTreeCount(&Tree) == 3u) &&
		(*(const int64*)xrtTypedTreeConstGet(&Tree, &iTwo) == 20) &&
		xrtTypedTreeHas(&Tree, &iThree) &&
		(*(const int32*)xrtTypedTreeStoredKey(&Tree, &iTwo) == 2),
		"typed tree lookup mismatch"
	);
	pValue = (int64*)xrtTypedTreeGetOrAdd(&Tree, &iFour, &bNew);
	testRequire(
		(pValue != NULL) && bNew && (*pValue == 0),
		"typed tree default insertion mismatch"
	);
	*pValue = 40;
	pValue = (int64*)xrtTypedTreeGetOrAdd(&Tree, &iFour, &bNew);
	testRequire(
		(pValue != NULL) && !bNew && (*pValue == 40),
		"typed tree existing default lookup mismatch"
	);

	pValue = (int64*)xrtTypedTreeFirst(&Tree, &pStoredKey);
	testRequire(
		(pValue != NULL) && (*pValue == 10) &&
		(*(const int32*)pStoredKey == 1),
		"typed tree first item mismatch"
	);
	pValue = (int64*)xrtTypedTreeLast(&Tree, &pStoredKey);
	testRequire(
		(pValue != NULL) && (*pValue == 40) &&
		(*(const int32*)pStoredKey == 4),
		"typed tree last item mismatch"
	);
	pValue = (int64*)xrtTypedTreeLowerBound(
		&Tree, &iZero, &pStoredKey
	);
	testRequire(
		(pValue != NULL) && (*(const int32*)pStoredKey == 1),
		"typed tree lower bound below range mismatch"
	);
	pValue = (int64*)xrtTypedTreeLowerBound(
		&Tree, &iTwo, &pStoredKey
	);
	testRequire(
		(pValue != NULL) && (*pValue == 20) &&
		(*(const int32*)pStoredKey == 2),
		"typed tree inclusive lower bound mismatch"
	);
	pValue = (int64*)xrtTypedTreeUpperBound(
		&Tree, &iTwo, &pStoredKey
	);
	testRequire(
		(pValue != NULL) && (*pValue == 30) &&
		(*(const int32*)pStoredKey == 3),
		"typed tree strict upper bound mismatch"
	);

	testRequire(
		xrtTypedTreeIterBegin(&Tree, &Iterator),
		"typed tree forward iterator could not start"
	);
	for ( size_t i = 0u; i < 4u; i++ ) {
		pValue = (int64*)xrtTypedTreeIterNext(&Iterator, &pStoredKey);
		testRequire(
			(pValue != NULL) &&
			(*(const int32*)pStoredKey == arrForward[i]),
			"typed tree forward order mismatch"
		);
	}
	xrtClearError();
	testRequire(
		(xrtTypedTreeIterNext(&Iterator, NULL) == NULL) &&
		(xrtGetError() == NULL),
		"typed tree natural iterator end reported an error"
	);
	testRequire(
		xrtTypedTreeIterRBegin(&Tree, &Iterator),
		"typed tree reverse iterator could not start"
	);
	for ( size_t i = 0u; i < 4u; i++ ) {
		pValue = (int64*)xrtTypedTreeIterNext(&Iterator, &pStoredKey);
		testRequire(
			(pValue != NULL) &&
			(*(const int32*)pStoredKey == arrReverse[i]),
			"typed tree reverse order mismatch"
		);
	}
	xrtTypedTreeIterEnd(&Iterator);
	testRequire(
		xrtTypedTreeIterFrom(&Tree, &iTwo, &Iterator) &&
		(*(int64*)xrtTypedTreeIterNext(&Iterator, &pStoredKey) == 20) &&
		(*(const int32*)pStoredKey == 2),
		"typed tree forward boundary iterator mismatch"
	);
	xrtTypedTreeIterEnd(&Iterator);
	testRequire(
		xrtTypedTreeIterRFrom(&Tree, &iTwo, &Iterator) &&
		(*(int64*)xrtTypedTreeIterNext(&Iterator, &pStoredKey) == 20) &&
		(*(const int32*)pStoredKey == 2),
		"typed tree reverse boundary iterator mismatch"
	);
	xrtTypedTreeIterEnd(&Iterator);

	testRequire(
		xrtTypedTreeSetTake(&Tree, &iFour, &iMoved) &&
		(iMoved == 0) &&
		(*(int64*)xrtTypedTreeGet(&Tree, &iFour) == 40) &&
		xrtTypedTreeTake(&Tree, &iFour, &iOutput) &&
		(iOutput == 40) &&
		!xrtTypedTreeHas(&Tree, &iFour),
		"typed tree move set or take mismatch"
	);
	testRequire(
		xrtTypedTreeRemove(&Tree, &iOne) &&
		(xrtTypedTreeCount(&Tree) == 2u),
		"typed tree removal mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtTypedTreeRemove(&Tree, &iFour) &&
		(xrtGetError() == NULL),
		"typed tree missing removal reported an error"
	);
	testRequire(
		xrtTypedTreeClear(&Tree) &&
		(xrtTypedTreeCount(&Tree) == 0u),
		"typed tree clear mismatch"
	);
	(void)xrtTypedTreeTrim(&Tree, 0u);
	xrtTypedTreeUnit(&Tree);
}



/* 验证深复制、冲突策略、相等性和提交后的迭代失效。 */
static void testTypedTreeComposition(void)
{
	xtypedtree Target;
	xtypedtree Source;
	xtypedtree* pSnapshot;
	xtypedtreeiter Iterator;
	int32 iOne = 1;
	int32 iTwo = 2;
	int32 iThree = 3;

	testRequire(
		xrtTypedTreeInit(&Target, xrtTypeInt32(), xrtTypeInt64()) &&
		xrtTypedTreeInit(&Source, xrtTypeInt32(), xrtTypeInt64()) &&
		testTypedTreeSetInt(&Target, iOne, 10) &&
		testTypedTreeSetInt(&Target, iTwo, 20) &&
		testTypedTreeSetInt(&Source, iTwo, 200) &&
		testTypedTreeSetInt(&Source, iThree, 300),
		"typed tree composition fixture failed"
	);
	pSnapshot = xrtTypedTreeClone(&Target);
	testRequire(
		(pSnapshot != NULL) && xrtTypedTreeEquals(&Target, pSnapshot),
		"typed tree clone mismatch"
	);
	testRequire(
		xrtTypedTreeMerge(&Target, &Source, false) &&
		(*(int64*)xrtTypedTreeGet(&Target, &iTwo) == 20) &&
		(*(int64*)xrtTypedTreeGet(&Target, &iThree) == 300),
		"typed tree preserving merge mismatch"
	);
	testRequire(
		xrtTypedTreeIterBegin(&Target, &Iterator) &&
		xrtTypedTreeMerge(&Target, &Source, true) &&
		(*(int64*)xrtTypedTreeGet(&Target, &iTwo) == 200) &&
		!xrtTypedTreeEquals(&Target, pSnapshot),
		"typed tree replacing merge mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtTypedTreeIterNext(&Iterator, NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"typed tree merge did not invalidate an old iterator"
	);
	xrtTypedTreeIterEnd(&Iterator);
	xrtTypedTreeDestroy(pSnapshot);
	xrtTypedTreeUnit(&Source);
	xrtTypedTreeUnit(&Target);
}



/* 运行类型树常规测试。 */
int main(void)
{
	testTypedTreeBasic();
	testTypedTreeComposition();
	xrtClearError();
	printf("[PASS] typed tree\n");
	return 0;
}
