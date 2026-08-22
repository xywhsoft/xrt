#include "../test.h"



/* 基础值用于验证零初始化、复制替换和稳定地址。 */
typedef struct testmapvalue {
	int Number;
	uint64 Stamp;
	char Name[16];
} testmapvalue;



/* 拥有值把独立字符串的生命期移交给映射。 */
typedef struct testownedvalue {
	str Text;
} testownedvalue;



/* 回调状态记录释放次数和访问顺序。 */
typedef struct testmapstate {
	size_t DropCount;
	size_t VisitCount;
	int64 Previous;
} testmapstate;



/* 回调重入状态记录查询、修改和销毁路径是否符合合同。 */
typedef struct testmapreentrystate {
	xintmap* Map;
	size_t DropCount;
	bool DropTried;
	bool DropDestroyBlocked;
	bool VisitReadAllowed;
	bool VisitRemoveBlocked;
	bool VisitDestroyBlocked;
} testmapreentrystate;



/* 新值初始化状态记录调用次数并允许注入回调失败。 */
typedef struct testmapinitstate {
	size_t Count;
	bool Fail;
} testmapinitstate;



/* 为拥有值创建一份独立字符串。 */
static testownedvalue testIntMapOwned(const char* pText)
{
	testownedvalue tValue;
	size_t iLength = strlen(pText) + 1u;

	tValue.Text = (str)xrtMalloc(iLength);
	testRequire(tValue.Text != NULL, "int map owned value allocation failed");
	memcpy(tValue.Text, pText, iLength);
	return tValue;
}



/* 释放值内部字符串并累加回调计数。 */
static void testIntMapDrop(int64 iKey, ptr pValue, ptr pUserData)
{
	testownedvalue* pOwned = (testownedvalue*)pValue;
	testmapstate* pState = (testmapstate*)pUserData;

	(void)iKey;
	xrtFree(pOwned->Text);
	pState->DropCount++;
}



/* 释放指针值映射中保存的目标地址。 */
static void testIntMapPtrDrop(int64 iKey, ptr pValue, ptr pUserData)
{
	testmapstate* pState = (testmapstate*)pUserData;
	ptr pTarget = *(ptr*)pValue;

	(void)iKey;
	xrtFree(pTarget);
	pState->DropCount++;
}



/* 原位初始化整数值，失败时发布可追踪的下层错误。 */
static bool testIntMapInitValue(int64 iKey, ptr pValue, ptr pUserData)
{
	testmapinitstate* pState = (testmapinitstate*)pUserData;

	pState->Count++;
	if ( pState->Fail ) {
		xerror* pError = xrtErrorCreate(
			XERR_VALUE, "test.int-map.init", 19,
			"the test int map initializer failed"
		);

		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return false;
	}
	*(int*)pValue = (int)iKey;
	return true;
}



/* 访问器验证键严格升序，并在第四项主动停止。 */
static bool testIntMapVisitor(int64 iKey, ptr pValue, ptr pUserData)
{
	testmapstate* pState = (testmapstate*)pUserData;
	testmapvalue* pMapValue = (testmapvalue*)pValue;

	if ( pState->VisitCount != 0 ) {
		testRequire(iKey > pState->Previous, "int map visit order mismatch");
	}
	testRequire(pMapValue->Number != -1, "int map visitor value mismatch");
	pState->Previous = iKey;
	pState->VisitCount++;
	return pState->VisitCount < 4;
}



/* 释放器验证替换期间不能销毁正在修改的映射。 */
static void testIntMapReentryDrop(int64 iKey, ptr pValue, ptr pUserData)
{
	testownedvalue* pOwned = (testownedvalue*)pValue;
	testmapreentrystate* pState = (testmapreentrystate*)pUserData;

	(void)iKey;
	xrtFree(pOwned->Text);
	pState->DropCount++;
	if ( !pState->DropTried ) {
		pState->DropTried = true;
		xrtClearError();
		xrtIntMapDestroy(pState->Map);
		pState->DropDestroyBlocked =
			xrtErrorKind(xrtGetError()) == XERR_STATE;
	}
}



/* 访问器允许查询同一映射，但拒绝修改或销毁它。 */
static bool testIntMapReentryVisitor(int64 iKey, ptr pValue, ptr pUserData)
{
	testmapvalue* pMapValue = (testmapvalue*)pValue;
	testmapreentrystate* pState = (testmapreentrystate*)pUserData;

	pMapValue->Number++;
	pState->VisitReadAllowed =
		(xrtIntMapCount(pState->Map) == 1) &&
		(xrtIntMapGet(pState->Map, iKey) == pValue);

	xrtClearError();
	pState->VisitRemoveBlocked =
		!xrtIntMapRemove(pState->Map, iKey) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE);

	xrtClearError();
	xrtIntMapDestroy(pState->Map);
	pState->VisitDestroyBlocked =
		xrtErrorKind(xrtGetError()) == XERR_STATE;
	return false;
}



/* 验证极值键、稀疏键、零值槽和复制替换合同。 */
static void testIntMapBasic(void)
{
	static const int64 arrKeys[] = {
		INT64_MIN,
		-500000,
		-1,
		0,
		1000000,
		INT64_MAX
	};
	xintmap tMap;
	testmapvalue tValue;
	testmapvalue* pStable;
	testmapvalue* pSlot;
	bool bNew;

	testRequire(xrtIntMapInit(&tMap, sizeof(testmapvalue)), "int map init failed");
	for ( size_t i = 0; i < (sizeof(arrKeys) / sizeof(arrKeys[0])); i++ ) {
		pSlot = (testmapvalue*)xrtIntMapGetOrAdd(&tMap, arrKeys[i], &bNew);
		testRequire((pSlot != NULL) && bNew, "int map unique get-or-add failed");
		for ( size_t n = 0; n < sizeof(testmapvalue); n++ ) {
			testRequire(((bytes)pSlot)[n] == 0, "int map new value was not zeroed");
		}
		pSlot->Number = (int)i + 1;
		pSlot->Stamp = (uint64)i + 100u;
	}
	testRequire(xrtIntMapCount(&tMap) == 6, "int map sparse count mismatch");

	pStable = (testmapvalue*)xrtIntMapGet(&tMap, 0);
	testRequire(pStable != NULL, "int map stable value missing");
	pSlot = (testmapvalue*)xrtIntMapGetOrAdd(&tMap, 0, &bNew);
	testRequire((pSlot == pStable) && !bNew, "int map duplicate get-or-add mismatch");
	for ( int64 i = 1; i <= 20000; i++ ) {
		tValue.Number = (int)i;
		tValue.Stamp = (uint64)i;
		memset(tValue.Name, 0, sizeof(tValue.Name));
		testRequire(xrtIntMapSet(&tMap, i, &tValue), "int map growth set failed");
	}
	testRequire(xrtIntMapGet(&tMap, 0) == pStable, "int map value address was not stable");

	memset(&tValue, 0, sizeof(tValue));
	tValue.Number = 77;
	tValue.Stamp = 9001;
	memcpy(tValue.Name, "replace", 8);
	testRequire(xrtIntMapSet(&tMap, -1, &tValue), "int map replace failed");
	pSlot = (testmapvalue*)xrtIntMapGet(&tMap, -1);
	testRequire(
		(pSlot != NULL) &&
		(pSlot->Number == 77) &&
		(pSlot->Stamp == 9001) &&
		(strcmp(pSlot->Name, "replace") == 0),
		"int map replace value mismatch"
	);
	testRequire(xrtIntMapSet(&tMap, -1, pSlot), "int map same-slot set should be a no-op");

	/* 同一池的另一个值槽不能作为浅拷贝来源。 */
	xrtClearError();
	testRequire(
		!xrtIntMapSet(&tMap, -1, xrtIntMapGet(&tMap, 1)),
		"int map should reject another internal value as source"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "int map alias error mismatch");
	testRequire(((testmapvalue*)xrtIntMapGet(&tMap, -1))->Number == 77, "int map alias failure changed value");

	xrtClearError();
	testRequire(xrtIntMapGet(&tMap, -9) == NULL, "int map missing get should return null");
	testRequire(xrtGetError() == NULL, "int map missing get should not report an error");
	testRequire(xrtIntMapHas(&tMap, INT64_MAX), "int map max key missing");
	testRequire(!xrtIntMapHas(&tMap, -9), "int map missing key reported present");
	testRequire(
		((const testmapvalue*)xrtIntMapConstGet(&tMap, -1))->Number == 77,
		"int map const get mismatch"
	);

	xrtIntMapUnit(&tMap);
}



/* 验证新值回调只在缺失键调用，并在失败时不提交条目。 */
static void testIntMapGetOrInit(void)
{
	xintmap Map;
	testmapinitstate State = { 0 };
	int* pValue;
	bool bNew;

	testRequire(xrtIntMapInit(&Map, sizeof(int)), "int map init callback fixture failed");
	pValue = (int*)xrtIntMapGetOrInit(
		&Map, 41, testIntMapInitValue, &State, &bNew
	);
	testRequire(
		(pValue != NULL) && bNew && (*pValue == 41) && (State.Count == 1u),
		"int map new value initializer mismatch"
	);
	State.Fail = true;
	pValue = (int*)xrtIntMapGetOrInit(
		&Map, 41, testIntMapInitValue, &State, &bNew
	);
	testRequire(
		(pValue != NULL) && !bNew && (*pValue == 41) && (State.Count == 1u),
		"int map existing key called initializer"
	);
	xrtClearError();
	testRequire(
		(xrtIntMapGetOrInit(
			&Map, 42, testIntMapInitValue, &State, &bNew
		) == NULL) &&
		!bNew &&
		(xrtIntMapCount(&Map) == 1u) &&
		!xrtIntMapHas(&Map, 42) &&
		(State.Count == 2u) &&
		(xrtGetError() != NULL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "test.int-map.init") == 0),
		"int map initializer failure committed visible state"
	);
	xrtClearError();
	testRequire(
		!xrtIntMapSet(&Map, 42, &Map) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"int map accepted a source overlapping its structure"
	);
	xrtIntMapUnit(&Map);
}



/* 验证首尾、边界查询、正反迭代和结构修改失效。 */
static void testIntMapOrderAndIteration(void)
{
	static const int64 arrKeys[] = { 40, -10, 0, 20, 10, 90 };
	static const int64 arrSorted[] = { -10, 0, 10, 20, 40, 90 };
	xintmap tMap;
	xintmapiter tForward;
	xintmapiter tSecond;
	xintmapiter tReverse;
	testmapstate tState = { 0, 0, 0 };
	testmapvalue tValue;
	testmapvalue* pValue;
	int64 iKey;

	testRequire(xrtIntMapInit(&tMap, sizeof(testmapvalue)), "ordered int map init failed");
	for ( size_t i = 0; i < (sizeof(arrKeys) / sizeof(arrKeys[0])); i++ ) {
		memset(&tValue, 0, sizeof(tValue));
		tValue.Number = (int)arrKeys[i];
		testRequire(xrtIntMapSet(&tMap, arrKeys[i], &tValue), "ordered int map set failed");
	}

	pValue = (testmapvalue*)xrtIntMapFirst(&tMap, &iKey);
	testRequire((pValue != NULL) && (iKey == -10), "int map first mismatch");
	pValue = (testmapvalue*)xrtIntMapLast(&tMap, &iKey);
	testRequire((pValue != NULL) && (iKey == 90), "int map last mismatch");
	pValue = (testmapvalue*)xrtIntMapLowerBound(&tMap, 11, &iKey);
	testRequire((pValue != NULL) && (iKey == 20), "int map lower bound mismatch");
	pValue = (testmapvalue*)xrtIntMapLowerBound(&tMap, 20, &iKey);
	testRequire((pValue != NULL) && (iKey == 20), "int map exact lower bound mismatch");
	pValue = (testmapvalue*)xrtIntMapUpperBound(&tMap, 20, &iKey);
	testRequire((pValue != NULL) && (iKey == 40), "int map upper bound mismatch");
	iKey = 123;
	testRequire(xrtIntMapUpperBound(&tMap, 90, &iKey) == NULL, "int map upper bound end mismatch");
	testRequire(iKey == 0, "int map missing bound did not clear output key");

	testRequire(xrtIntMapIterBegin(&tMap, &tForward), "int map forward begin failed");
	testRequire(xrtIntMapIterBegin(&tMap, &tSecond), "int map second begin failed");
	for ( size_t i = 0; i < (sizeof(arrSorted) / sizeof(arrSorted[0])); i++ ) {
		pValue = (testmapvalue*)xrtIntMapIterNext(&tForward, &iKey);
		testRequire((pValue != NULL) && (iKey == arrSorted[i]), "int map forward order mismatch");
		pValue->Stamp++;
	}
	testRequire(xrtIntMapIterNext(&tForward, &iKey) == NULL, "int map forward iterator did not end");
	testRequire(iKey == 0, "int map ended iterator did not clear key");
	testRequire(xrtIntMapIterNext(&tSecond, &iKey) != NULL, "int map iterator states collided");
	xrtIntMapIterEnd(&tSecond);

	testRequire(xrtIntMapIterRBegin(&tMap, &tReverse), "int map reverse begin failed");
	for ( size_t i = (sizeof(arrSorted) / sizeof(arrSorted[0])); i != 0; i-- ) {
		pValue = (testmapvalue*)xrtIntMapIterNext(&tReverse, &iKey);
		testRequire((pValue != NULL) && (iKey == arrSorted[i - 1u]), "int map reverse order mismatch");
	}
	xrtIntMapIterEnd(&tReverse);

	/* 范围迭代从 AVL 中直接定位包含边界，不扫描前缀。 */
	testRequire(xrtIntMapIterFrom(&tMap, 11, &tForward), "int map range begin failed");
	for ( size_t i = 3; i < (sizeof(arrSorted) / sizeof(arrSorted[0])); i++ ) {
		pValue = (testmapvalue*)xrtIntMapIterNext(&tForward, &iKey);
		testRequire((pValue != NULL) && (iKey == arrSorted[i]), "int map range order mismatch");
	}
	testRequire(xrtIntMapIterNext(&tForward, &iKey) == NULL, "int map range iterator did not end");
	testRequire(xrtIntMapIterRFrom(&tMap, 39, &tReverse), "int map reverse range begin failed");
	for ( size_t i = 4; i != 0; i-- ) {
		pValue = (testmapvalue*)xrtIntMapIterNext(&tReverse, &iKey);
		testRequire((pValue != NULL) && (iKey == arrSorted[i - 1u]), "int map reverse range order mismatch");
	}
	testRequire(xrtIntMapIterNext(&tReverse, &iKey) == NULL, "int map reverse range did not end");
	testRequire(xrtIntMapIterFrom(&tMap, 91, &tForward), "int map empty range begin failed");
	testRequire(xrtIntMapIterNext(&tForward, &iKey) == NULL, "int map empty range should end");
	testRequire(xrtIntMapIterRFrom(&tMap, -11, &tReverse), "int map empty reverse range begin failed");
	testRequire(xrtIntMapIterNext(&tReverse, &iKey) == NULL, "int map empty reverse range should end");

	testRequire(xrtIntMapVisit(&tMap, testIntMapVisitor, &tState) == 4, "int map visitor stop mismatch");
	testRequire(tState.VisitCount == 4, "int map visitor count mismatch");

	/* 直接修改值不改变索引，插入新键则必须使迭代器失效。 */
	testRequire(xrtIntMapIterBegin(&tMap, &tForward), "int map mutation begin failed");
	pValue = (testmapvalue*)xrtIntMapIterNext(&tForward, &iKey);
	testRequire(pValue != NULL, "int map mutation first value missing");
	pValue->Number++;
	testRequire(xrtIntMapIterNext(&tForward, &iKey) != NULL, "value mutation invalidated int map iterator");
	memset(&tValue, 0, sizeof(tValue));
	tValue.Number = 70;
	testRequire(xrtIntMapSet(&tMap, 70, &tValue), "int map structural mutation failed");
	xrtClearError();
	testRequire(xrtIntMapIterNext(&tForward, &iKey) == NULL, "mutated int map iterator should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "int map mutation error mismatch");

	xrtIntMapUnit(&tMap);
}



/* 验证释放器、替换、Remove、Take、Clear 和池页裁剪。 */
static void testIntMapOwnership(void)
{
	xintmap tMap;
	testmapstate tState = { 0 };
	testownedvalue tValue;
	testownedvalue tTaken;

	testRequire(xrtIntMapInit(&tMap, sizeof(testownedvalue)), "owned int map init failed");
	testRequire(xrtIntMapSetDrop(&tMap, testIntMapDrop, &tState), "owned int map drop setup failed");
	tValue = testIntMapOwned("first");
	testRequire(xrtIntMapSet(&tMap, 1, &tValue), "owned int map first set failed");
	tValue = testIntMapOwned("second");
	testRequire(xrtIntMapSet(&tMap, 1, &tValue), "owned int map replace failed");
	testRequire(tState.DropCount == 1, "owned int map replace did not drop old value");

	memset(&tTaken, 0, sizeof(tTaken));
	testRequire(xrtIntMapTake(&tMap, 1, &tTaken), "owned int map take failed");
	testRequire(
		(tState.DropCount == 1) && (strcmp(tTaken.Text, "second") == 0),
		"owned int map take called drop or copied wrong value"
	);
	xrtFree(tTaken.Text);

	for ( int64 i = 0; i < 600; i++ ) {
		tValue = testIntMapOwned("entry");
		testRequire(xrtIntMapSet(&tMap, i, &tValue), "owned int map batch set failed");
	}
	testRequire(xrtIntMapRemove(&tMap, 5), "owned int map remove failed");
	testRequire(tState.DropCount == 2, "owned int map remove did not drop value");
	testRequire(!xrtIntMapRemove(&tMap, 5000), "owned int map missing remove should fail");
	xrtClearError();
	testRequire(
		!xrtIntMapSetDrop(&tMap, testIntMapDrop, &tState),
		"non-empty int map should reject drop replacement"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "int map drop replacement error mismatch");

	xrtIntMapClear(&tMap);
	testRequire((xrtIntMapCount(&tMap) == 0) && (tState.DropCount == 601), "owned int map clear mismatch");
	testRequire(xrtIntMapTrim(&tMap, 0) != 0, "int map trim did not release empty pages");
	xrtIntMapUnit(&tMap);
}



/* 验证释放和访问回调均不能破坏外层整数映射操作。 */
static void testIntMapCallbackReentry(void)
{
	testmapreentrystate tDropState = { 0 };
	testmapreentrystate tVisitState = { 0 };
	xintmap* pMap;
	testownedvalue tOwned;
	testmapvalue tValue = { 0 };
	testmapvalue* pStored;

	pMap = xrtIntMapCreate(sizeof(testownedvalue));
	testRequire(pMap != NULL, "int map drop reentry create failed");
	tDropState.Map = pMap;
	testRequire(
		xrtIntMapSetDrop(
			pMap,
			testIntMapReentryDrop,
			&tDropState
		),
		"int map drop reentry setup failed"
	);
	tOwned = testIntMapOwned("first");
	testRequire(xrtIntMapSet(pMap, 1, &tOwned), "int map drop reentry first set failed");
	tOwned = testIntMapOwned("second");
	testRequire(xrtIntMapSet(pMap, 1, &tOwned), "int map drop reentry replace failed");
	testRequire(tDropState.DropTried, "int map replacement did not exercise destroy reentry");
	testRequire(tDropState.DropDestroyBlocked, "int map drop callback destroyed current map");
	testRequire(
		(xrtIntMapCount(pMap) == 1) &&
		(strcmp(((testownedvalue*)xrtIntMapGet(pMap, 1))->Text, "second") == 0),
		"int map drop reentry damaged replacement"
	);
	xrtClearError();
	xrtIntMapDestroy(pMap);
	testRequire(tDropState.DropCount == 2, "int map drop reentry final count mismatch");

	pMap = xrtIntMapCreate(sizeof(testmapvalue));
	testRequire(pMap != NULL, "int map visit reentry create failed");
	tValue.Number = 10;
	testRequire(xrtIntMapSet(pMap, 7, &tValue), "int map visit reentry set failed");
	tVisitState.Map = pMap;
	testRequire(
		xrtIntMapVisit(
			pMap,
			testIntMapReentryVisitor,
			&tVisitState
		) == 1,
		"int map visit reentry count mismatch"
	);
	testRequire(tVisitState.VisitReadAllowed, "int map visitor query was rejected");
	testRequire(tVisitState.VisitRemoveBlocked, "int map visitor removed current value");
	testRequire(tVisitState.VisitDestroyBlocked, "int map visitor destroyed current map");
	pStored = (testmapvalue*)xrtIntMapGet(pMap, 7);
	testRequire(
		(xrtIntMapCount(pMap) == 1) &&
		(pStored != NULL) &&
		(pStored->Number == 11),
		"int map visitor changed structure or lost value mutation"
	);
	xrtClearError();
	xrtIntMapDestroy(pMap);
}



/* 验证指针值便利层能区分缺失键与已保存的空指针。 */
static void testIntMapPointers(void)
{
	xintmap tMap;
	xintmap tWrong;
	testmapstate tState = { 0, 0, 0 };
	ptr pFirst;
	ptr pSecond;
	ptr pTaken = (ptr)(uintptr_t)1;

	testRequire(xrtIntMapInit(&tMap, sizeof(ptr)), "pointer int map init failed");
	testRequire(xrtIntMapSetDrop(&tMap, testIntMapPtrDrop, &tState), "pointer int map drop setup failed");
	testRequire(xrtIntMapSetPtr(&tMap, 0, NULL), "pointer int map null set failed");
	testRequire(xrtIntMapHas(&tMap, 0), "pointer int map null key missing");
	testRequire(xrtIntMapGetPtr(&tMap, 0) == NULL, "pointer int map null value mismatch");

	pFirst = xrtMalloc(16);
	pSecond = xrtMalloc(16);
	testRequire((pFirst != NULL) && (pSecond != NULL), "pointer int map target allocation failed");
	testRequire(xrtIntMapSetPtr(&tMap, 1, pFirst), "pointer int map first set failed");
	testRequire(xrtIntMapSetPtr(&tMap, 1, pFirst), "pointer int map equal replacement failed");
	testRequire(tState.DropCount == 0, "pointer int map equal replacement dropped target");
	testRequire(xrtIntMapSetPtr(&tMap, 1, pSecond), "pointer int map replacement failed");
	testRequire(tState.DropCount == 1, "pointer int map replacement did not drop old target");
	testRequire(xrtIntMapTakePtr(&tMap, 1, &pTaken), "pointer int map take failed");
	testRequire((pTaken == pSecond) && (tState.DropCount == 1), "pointer int map take mismatch");
	xrtFree(pTaken);

	pTaken = (ptr)(uintptr_t)1;
	testRequire(!xrtIntMapTakePtr(&tMap, 99, &pTaken), "pointer int map missing take should fail");
	testRequire(pTaken == NULL, "pointer int map missing take did not clear output");

	testRequire(xrtIntMapInit(&tWrong, sizeof(ptr) + 1u), "wrong pointer int map init failed");
	xrtClearError();
	testRequire(!xrtIntMapSetPtr(&tWrong, 1, NULL), "wrong-sized int map pointer set should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "wrong-sized pointer map error mismatch");
	xrtIntMapUnit(&tWrong);

	xrtIntMapUnit(&tMap);
	testRequire(tState.DropCount == 2, "pointer int map unit did not drop stored null value");
}



/* 验证过对齐值、堆创建和非法布局参数。 */
static void testIntMapAlignmentAndErrors(void)
{
	xintmap tMap;
	xintmap* pCreated;
	ptr pValue;
	int64 iKey = 123;
	bool bNew = true;

	testRequire(xrtIntMapInitAligned(&tMap, 64, 64), "aligned int map init failed");
	pValue = xrtIntMapGetOrAdd(&tMap, 7, &bNew);
	testRequire(
		(pValue != NULL) && bNew && (((uintptr_t)pValue & 63u) == 0),
		"int map value alignment mismatch"
	);
	xrtIntMapUnit(&tMap);

	pCreated = xrtIntMapCreate(sizeof(uint64));
	testRequire(pCreated != NULL, "int map create failed");
	xrtIntMapDestroy(pCreated);

	testRequire(!xrtIntMapInit(&tMap, 0), "zero-sized int map should fail");
	testRequire(!xrtIntMapInitAligned(&tMap, sizeof(int), 24), "invalid int map alignment should fail");
	xrtClearError();
	testRequire(
		!xrtIntMapInitAligned(&tMap, SIZE_MAX, 16),
		"overflowing int map layout should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "int map overflow error mismatch");

	testRequire(xrtIntMapInit(&tMap, sizeof(int)), "empty int map init failed");
	testRequire(xrtIntMapFirst(&tMap, &iKey) == NULL, "empty int map first should be null");
	testRequire(iKey == 0, "empty int map first did not clear key");
	bNew = true;
	xrtClearError();
	testRequire(xrtIntMapGetOrAdd(NULL, 1, &bNew) == NULL, "null int map should fail");
	testRequire(!bNew, "failed int map get-or-add reported new value");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "null int map error mismatch");
	xrtIntMapUnit(&tMap);
}



/* 运行整数映射全部合同测试。 */
int main(void)
{
	testIntMapBasic();
	testIntMapGetOrInit();
	testIntMapOrderAndIteration();
	testIntMapOwnership();
	testIntMapCallbackReentry();
	testIntMapPointers();
	testIntMapAlignmentAndErrors();
	printf("[PASS] int_map\n");
	return 0;
}
