#include "../test.h"



/* 验证唯一值、稳定规范地址、插入顺序和移动删除。 */
static void testTypedSetBasic(void)
{
	static const int64 arrExpected[] = { 7, 11, 13, 17 };
	xtypedset Set;
	xtypedsetiter Iterator;
	int64 arrValues[] = { 7, 11, 7, 13, 17 };
	int64 iOutput = 0;
	const int64* pStable;
	const int64* pValue;
	bool bNew;

	testRequire(
		xrtTypedSetInit(&Set, xrtTypeInt64()) &&
		xrtTypedSetReserve(&Set, 16u),
		"typed set init or reserve failed"
	);
	testRequire(
		xrtTypedSetAdd(&Set, &arrValues[0]) &&
		xrtTypedSetAdd(&Set, &arrValues[1]) &&
		xrtTypedSetAdd(&Set, &arrValues[2]) &&
		(xrtTypedSetCount(&Set) == 2u),
		"typed set uniqueness mismatch"
	);
	pStable = (const int64*)xrtTypedSetGet(&Set, &arrValues[0]);
	testRequire(
		(pStable != NULL) &&
		(*pStable == 7) &&
		(xrtTypedSetGetOrAdd(&Set, &arrValues[2], &bNew) == pStable) &&
		!bNew &&
		xrtTypedSetAdd(&Set, &arrValues[3]) &&
		xrtTypedSetAdd(&Set, &arrValues[4]) &&
		(xrtTypedSetGet(&Set, &arrValues[0]) == pStable),
		"typed set canonical value or stable address mismatch"
	);
	for ( size_t i = 0u; i < 4u; i++ ) {
		pValue = (const int64*)xrtTypedSetAt(&Set, i);
		testRequire(
			(pValue != NULL) && (*pValue == arrExpected[i]),
			"typed set positional insertion order mismatch"
		);
	}

	testRequire(
		xrtTypedSetIterBegin(&Set, &Iterator),
		"typed set iterator begin failed"
	);
	for ( size_t i = 0u; i < 4u; i++ ) {
		pValue = (const int64*)xrtTypedSetIterNext(&Iterator);
		testRequire(
			(pValue != NULL) && (*pValue == arrExpected[i]),
			"typed set iteration order mismatch"
		);
	}
	testRequire(
		xrtTypedSetIterNext(&Iterator) == NULL,
		"typed set iterator did not finish"
	);
	xrtTypedSetIterEnd(&Iterator);
	testRequire(
		xrtTypedSetIterRBegin(&Set, &Iterator),
		"typed set reverse iterator begin failed"
	);
	for ( size_t i = 0u; i < 4u; i++ ) {
		pValue = (const int64*)xrtTypedSetIterNext(&Iterator);
		testRequire(
			(pValue != NULL) && (*pValue == arrExpected[3u - i]),
			"typed set reverse iteration order mismatch"
		);
	}
	xrtTypedSetIterEnd(&Iterator);

	testRequire(
		xrtTypedSetTake(&Set, &arrValues[1], &iOutput) &&
		(iOutput == 11) &&
		!xrtTypedSetHas(&Set, &arrValues[1]) &&
		xrtTypedSetRemove(&Set, &arrValues[3]),
		"typed set take or remove mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtTypedSetRemove(&Set, &arrValues[1]) &&
		(xrtGetError() == NULL),
		"typed set missing removal reported an error"
	);
	testRequire(
		xrtTypedSetTrim(&Set) && xrtTypedSetClear(&Set) &&
		(xrtTypedSetCount(&Set) == 0u),
		"typed set trim or clear failed"
	);
	xrtTypedSetUnit(&Set);
}



/* 验证深复制、完整集合代数和关系判断。 */
static void testTypedSetComposition(void)
{
	xtypedset Left;
	xtypedset Right;
	xtypedset* pClone;
	xtypedset* pUnion;
	xtypedset* pIntersection;
	xtypedset* pDifference;
	xtypedset* pSymmetric;
	int64 iOne = 1;
	int64 iTwo = 2;
	int64 iThree = 3;

	testRequire(
		xrtTypedSetInit(&Left, xrtTypeInt64()) &&
		xrtTypedSetInit(&Right, xrtTypeInt64()) &&
		xrtTypedSetAdd(&Left, &iOne) &&
		xrtTypedSetAdd(&Left, &iTwo) &&
		xrtTypedSetAdd(&Right, &iTwo) &&
		xrtTypedSetAdd(&Right, &iThree),
		"typed set composition fixture failed"
	);
	pClone = xrtTypedSetClone(&Left);
	pUnion = xrtTypedSetUnion(&Left, &Right);
	pIntersection = xrtTypedSetIntersection(&Left, &Right);
	pDifference = xrtTypedSetDifference(&Left, &Right);
	pSymmetric = xrtTypedSetSymmetricDifference(&Left, &Right);
	testRequire(
		(pClone != NULL) && xrtTypedSetEquals(&Left, pClone) &&
		(pUnion != NULL) && (xrtTypedSetCount(pUnion) == 3u) &&
		(pIntersection != NULL) &&
		 (xrtTypedSetCount(pIntersection) == 1u) &&
		 xrtTypedSetHas(pIntersection, &iTwo) &&
		(pDifference != NULL) &&
		 (xrtTypedSetCount(pDifference) == 1u) &&
		 xrtTypedSetHas(pDifference, &iOne) &&
		(pSymmetric != NULL) &&
		 (xrtTypedSetCount(pSymmetric) == 2u) &&
		 xrtTypedSetHas(pSymmetric, &iOne) &&
		 xrtTypedSetHas(pSymmetric, &iThree),
		"typed set algebra mismatch"
	);
	testRequire(
		xrtTypedSetIsSubset(&Left, pUnion, true) &&
		xrtTypedSetIsSuperset(pUnion, &Right, true) &&
		xrtTypedSetIsDisjoint(pDifference, &Right) &&
		!xrtTypedSetEquals(&Left, &Right),
		"typed set relation mismatch"
	);
	testRequire(
		xrtTypedSetMerge(&Left, &Right) &&
		xrtTypedSetEquals(&Left, pUnion),
		"typed set merge mismatch"
	);

	xrtTypedSetDestroy(pSymmetric);
	xrtTypedSetDestroy(pDifference);
	xrtTypedSetDestroy(pIntersection);
	xrtTypedSetDestroy(pUnion);
	xrtTypedSetDestroy(pClone);
	xrtTypedSetUnit(&Right);
	xrtTypedSetUnit(&Left);
}



/* 验证结构修改会让外置迭代器失效。 */
static void testTypedSetIteratorInvalidation(void)
{
	xtypedset Set;
	xtypedsetiter Iterator;
	int64 iOne = 1;
	int64 iTwo = 2;

	testRequire(
		xrtTypedSetInit(&Set, xrtTypeInt64()) &&
		xrtTypedSetAdd(&Set, &iOne) &&
		xrtTypedSetIterBegin(&Set, &Iterator) &&
		xrtTypedSetAdd(&Set, &iTwo),
		"typed set iterator invalidation fixture failed"
	);
	xrtClearError();
	testRequire(
		(xrtTypedSetIterNext(&Iterator) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(strcmp(xrtErrorOperation(xrtGetError()), "iter-next") == 0),
		"typed set iterator did not detect structural modification"
	);
	xrtTypedSetIterEnd(&Iterator);
	xrtTypedSetUnit(&Set);
}



/* 运行类型集合常规测试。 */
int main(void)
{
	testTypedSetBasic();
	testTypedSetComposition();
	testTypedSetIteratorInvalidation();
	xrtClearError();
	printf("[PASS] typed set\n");
	return 0;
}
