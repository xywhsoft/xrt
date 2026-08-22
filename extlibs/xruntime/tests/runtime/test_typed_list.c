#include "../test.h"



/* 验证稀疏键、追加、稳定槽、查找和有序迭代。 */
static void testTypedListBasic(void)
{
	static const int64 arrKeys[] = { -4, 3, 10, 20, 21 };
	xtypedlist List;
	xtypedlistiter Iterator;
	int64 iSeven = 7;
	int64 iEleven = 11;
	int64 iThirteen = 13;
	int64 iSeventeen = 17;
	int64 iOutput = 0;
	int64 iKey;
	int64* pStable;
	int64* pValue;

	testRequire(xrtTypedListInit(&List, xrtTypeInt64()), "typed list init failed");
	testRequire(
		xrtTypedListSet(&List, 10, &iThirteen) &&
		xrtTypedListSet(&List, -4, &iEleven) &&
		xrtTypedListSet(&List, 3, &iSeven),
		"typed list sparse set failed"
	);
	pStable = (int64*)xrtTypedListGet(&List, 3);
	testRequire(
		(pStable != NULL) &&
		xrtTypedListSet(&List, 20, pStable) &&
		xrtTypedListAppend(&List, &iSeventeen, &iKey) &&
		(iKey == 21) &&
		(xrtTypedListGet(&List, 3) == pStable) &&
		(xrtTypedListCount(&List) == 5u),
		"typed list internal copy, append, or stable address mismatch"
	);
	testRequire(
		xrtTypedListFind(&List, &iSeven, &iKey) &&
		(iKey == 3) &&
		xrtTypedListContains(&List, &iEleven) &&
		(*(int64*)xrtTypedListAt(&List, 2u, &iKey) == 13) &&
		(iKey == 10) &&
		(*(const int64*)xrtTypedListConstAt(&List, 4u, &iKey) == 17) &&
		(iKey == 21),
		"typed list find or positional access mismatch"
	);

	testRequire(xrtTypedListIterBegin(&List, &Iterator), "typed list iteration begin failed");
	for ( size_t i = 0u; i < sizeof(arrKeys) / sizeof(arrKeys[0]); i++ ) {
		pValue = (int64*)xrtTypedListIterNext(&Iterator, &iKey);
		testRequire(
			(pValue != NULL) && (iKey == arrKeys[i]),
			"typed list iteration order mismatch"
		);
	}
	testRequire(
		xrtTypedListIterNext(&Iterator, &iKey) == NULL,
		"typed list iterator did not end"
	);
	xrtTypedListIterEnd(&Iterator);
	testRequire(
		xrtTypedListIterRBegin(&List, &Iterator),
		"typed list reverse iteration begin failed"
	);
	for ( size_t i = 0u; i < sizeof(arrKeys) / sizeof(arrKeys[0]); i++ ) {
		pValue = (int64*)xrtTypedListIterNext(&Iterator, &iKey);
		testRequire(
			(pValue != NULL) &&
			(iKey == arrKeys[(sizeof(arrKeys) / sizeof(arrKeys[0])) - i - 1u]),
			"typed list reverse iteration order mismatch"
		);
	}
	xrtTypedListIterEnd(&Iterator);
	testRequire(
		xrtTypedListIterFrom(&List, 4, &Iterator) &&
		(xrtTypedListIterNext(&Iterator, &iKey) != NULL) &&
		(iKey == 10),
		"typed list forward bound mismatch"
	);
	xrtTypedListIterEnd(&Iterator);
	testRequire(
		xrtTypedListIterRFrom(&List, 4, &Iterator) &&
		(xrtTypedListIterNext(&Iterator, &iKey) != NULL) &&
		(iKey == 3),
		"typed list reverse bound mismatch"
	);
	xrtTypedListIterEnd(&Iterator);
	testRequire(
		xrtTypedListIterBegin(&List, &Iterator) &&
		xrtTypedListSet(&List, 40, &iSeven),
		"typed list iterator invalidation fixture failed"
	);
	xrtClearError();
	testRequire(
		(xrtTypedListIterNext(&Iterator, &iKey) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"typed list iterator did not detect structural modification"
	);
	xrtTypedListIterEnd(&Iterator);

	testRequire(
		xrtTypedListTake(&List, 3, &iOutput) &&
		(iOutput == 7) &&
		!xrtTypedListHas(&List, 3) &&
		xrtTypedListRemove(&List, -4),
		"typed list take or remove mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtTypedListRemove(&List, INT64_MIN) &&
		(xrtGetError() == NULL),
		"typed list missing removal reported an error"
	);
	xrtTypedListUnit(&List);
}



/* 验证深复制、键冲突策略、比较和失败原子合并。 */
static void testTypedListComposition(void)
{
	xtypedlist Target;
	xtypedlist Source;
	xtypedlist* pClone;
	int64 iOne = 1;
	int64 iTen = 10;
	int64 iHundred = 100;
	int64 iThirty = 30;

	testRequire(
		xrtTypedListInit(&Target, xrtTypeInt64()) &&
		xrtTypedListInit(&Source, xrtTypeInt64()) &&
		xrtTypedListSet(&Target, 1, &iOne) &&
		xrtTypedListSet(&Target, 10, &iTen) &&
		xrtTypedListSet(&Source, 10, &iHundred) &&
		xrtTypedListSet(&Source, 30, &iThirty),
		"typed list composition fixture failed"
	);
	pClone = xrtTypedListClone(&Target);
	testRequire(
		(pClone != NULL) && xrtTypedListEquals(&Target, pClone),
		"typed list clone mismatch"
	);
	testRequire(
		xrtTypedListMerge(&Target, &Source, false) &&
		(*(int64*)xrtTypedListGet(&Target, 10) == 10) &&
		(*(int64*)xrtTypedListGet(&Target, 30) == 30),
		"typed list non-replacing merge mismatch"
	);
	testRequire(
		xrtTypedListMerge(&Target, &Source, true) &&
		(*(int64*)xrtTypedListGet(&Target, 10) == 100) &&
		!xrtTypedListEquals(&Target, pClone),
		"typed list replacing merge mismatch"
	);
	testRequire(
		xrtTypedListClear(&Target) &&
		(xrtTypedListCount(&Target) == 0u),
		"typed list clear failed"
	);
	(void)xrtTypedListTrim(&Target, 0u);
	xrtTypedListDestroy(pClone);
	xrtTypedListUnit(&Source);
	xrtTypedListUnit(&Target);
}



/* 运行类型列表常规测试。 */
int main(void)
{
	testTypedListBasic();
	testTypedListComposition();
	xrtClearError();
	printf("[PASS] typed list\n");
	return 0;
}
