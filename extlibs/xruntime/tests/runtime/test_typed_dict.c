#include "../test.h"



/* 验证文本键边界、规范键副本、默认构造和插入顺序。 */
static void testTypedDictBasic(void)
{
	char arrMutable[] = "mutable";
	char arrBinary[] = { 'a', 0, 'b' };
	xtypeddict Dict;
	xtypeddictiter Iterator;
	xstrview StoredKey;
	xstrview Key;
	int64 iOne = 1;
	int64 iTwo = 2;
	int64 iThree = 3;
	int64 iOutput = 0;
	int64* pValue;
	bool bNew;

	testRequire(
		xrtTypedDictInit(&Dict, xrtTypeInt64()) &&
		xrtTypedDictReserve(&Dict, 16u),
		"typed dictionary init or reserve failed"
	);
	testRequire(
		xrtTypedDictSet(&Dict, XRT_STR_LITERAL("first"), &iOne) &&
		xrtTypedDictSet(
			&Dict,
			(xstrview){ arrMutable, sizeof(arrMutable) - 1u },
			&iTwo
		) &&
		xrtTypedDictSet(
			&Dict,
			(xstrview){ arrBinary, sizeof(arrBinary) },
			&iThree
		),
		"typed dictionary key insertion failed"
	);
	memset(arrMutable, 'x', sizeof(arrMutable) - 1u);
	testRequire(
		xrtTypedDictStoredKey(
			&Dict, XRT_STR_LITERAL("mutable"), &StoredKey
		) &&
		(StoredKey.Size == 7u) &&
		(memcmp(StoredKey.Data, "mutable", 7u) == 0) &&
		(*(const int64*)xrtTypedDictConstGet(
			&Dict, (xstrview){ arrBinary, sizeof(arrBinary) }
		) == 3),
		"typed dictionary key copy or embedded zero mismatch"
	);
	pValue = (int64*)xrtTypedDictGetOrAdd(
		&Dict, XRT_STR_LITERAL("default"), &bNew
	);
	testRequire(
		(pValue != NULL) && bNew && (*pValue == 0) &&
		(xrtTypedDictCount(&Dict) == 4u),
		"typed dictionary default initialization mismatch"
	);
	*pValue = 4;
	pValue = (int64*)xrtTypedDictGetOrAdd(
		&Dict, XRT_STR_LITERAL("default"), &bNew
	);
	testRequire(
		(pValue != NULL) && !bNew && (*pValue == 4),
		"typed dictionary existing default value mismatch"
	);

	testRequire(
		xrtTypedDictIterBegin(&Dict, &Iterator),
		"typed dictionary iterator begin failed"
	);
	pValue = (int64*)xrtTypedDictIterNext(&Iterator, &Key);
	testRequire(
		(pValue != NULL) && (*pValue == 1) &&
		(Key.Size == 5u) && (memcmp(Key.Data, "first", 5u) == 0),
		"typed dictionary insertion order mismatch"
	);
	xrtTypedDictIterEnd(&Iterator);
	testRequire(
		(*(int64*)xrtTypedDictAt(&Dict, 1u, &Key) == 2) &&
		(Key.Size == 7u) &&
		(*(const int64*)xrtTypedDictConstAt(&Dict, 3u, &Key) == 4) &&
		(Key.Size == 7u),
		"typed dictionary positional access mismatch"
	);
	testRequire(
		xrtTypedDictTake(
			&Dict, XRT_STR_LITERAL("mutable"), &iOutput
		) &&
		(iOutput == 2) &&
		!xrtTypedDictHas(&Dict, XRT_STR_LITERAL("mutable")) &&
		xrtTypedDictRemove(&Dict, XRT_STR_LITERAL("first")),
		"typed dictionary take or remove mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtTypedDictRemove(&Dict, XRT_STR_LITERAL("missing")) &&
		(xrtGetError() == NULL),
		"typed dictionary missing removal reported an error"
	);
	testRequire(
		xrtTypedDictTrim(&Dict) && xrtTypedDictClear(&Dict) &&
		(xrtTypedDictCount(&Dict) == 0u),
		"typed dictionary trim or clear failed"
	);
	xrtTypedDictUnit(&Dict);
}



/* 验证深复制、冲突策略、相等性和合并后的迭代失效。 */
static void testTypedDictComposition(void)
{
	xtypeddict Target;
	xtypeddict Source;
	xtypeddict* pSnapshot;
	xtypeddictiter Iterator;
	int64 iOne = 1;
	int64 iTwo = 2;
	int64 iTwenty = 20;
	int64 iThree = 3;

	testRequire(
		xrtTypedDictInit(&Target, xrtTypeInt64()) &&
		xrtTypedDictInit(&Source, xrtTypeInt64()) &&
		xrtTypedDictSet(&Target, XRT_STR_LITERAL("one"), &iOne) &&
		xrtTypedDictSet(&Target, XRT_STR_LITERAL("two"), &iTwo) &&
		xrtTypedDictSet(&Source, XRT_STR_LITERAL("two"), &iTwenty) &&
		xrtTypedDictSet(&Source, XRT_STR_LITERAL("three"), &iThree),
		"typed dictionary composition fixture failed"
	);
	pSnapshot = xrtTypedDictClone(&Target);
	testRequire(
		(pSnapshot != NULL) && xrtTypedDictEquals(&Target, pSnapshot),
		"typed dictionary clone mismatch"
	);
	testRequire(
		xrtTypedDictMerge(&Target, &Source, false) &&
		(*(int64*)xrtTypedDictGet(&Target, XRT_STR_LITERAL("two")) == 2) &&
		(*(int64*)xrtTypedDictGet(&Target, XRT_STR_LITERAL("three")) == 3),
		"typed dictionary preserving merge mismatch"
	);
	testRequire(
		xrtTypedDictIterBegin(&Target, &Iterator) &&
		xrtTypedDictMerge(&Target, &Source, true) &&
		(*(int64*)xrtTypedDictGet(&Target, XRT_STR_LITERAL("two")) == 20) &&
		!xrtTypedDictEquals(&Target, pSnapshot),
		"typed dictionary replacing merge mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtTypedDictIterNext(&Iterator, NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"typed dictionary merge did not invalidate an old iterator"
	);
	xrtTypedDictIterEnd(&Iterator);
	xrtTypedDictDestroy(pSnapshot);
	xrtTypedDictUnit(&Source);
	xrtTypedDictUnit(&Target);
}



/* 验证移动设置同时覆盖新键和已有键，并恢复来源为空值。 */
static void testTypedDictMoveSet(void)
{
	xtypeddict Dict;
	int64 iFirst = 11;
	int64 iSecond = 29;

	testRequire(
		xrtTypedDictInit(&Dict, xrtTypeInt64()) &&
		xrtTypedDictSetTake(
			&Dict, XRT_STR_LITERAL("value"), &iFirst
		) && (iFirst == 0) &&
		(*(int64*)xrtTypedDictGet(
			&Dict, XRT_STR_LITERAL("value")
		) == 11) &&
		xrtTypedDictSetTake(
			&Dict, XRT_STR_LITERAL("value"), &iSecond
		) && (iSecond == 0) &&
		(*(int64*)xrtTypedDictGet(
			&Dict, XRT_STR_LITERAL("value")
		) == 29),
		"typed dictionary moved set mismatch"
	);
	xrtTypedDictUnit(&Dict);
}



/* 运行类型字典常规测试。 */
int main(void)
{
	testTypedDictBasic();
	testTypedDictComposition();
	testTypedDictMoveSet();
	xrtClearError();
	printf("[PASS] typed dictionary\n");
	return 0;
}
