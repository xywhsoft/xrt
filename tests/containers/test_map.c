#include "../test.h"



/* 基础值用于验证零初始化、复制替换和稳定地址。 */
typedef struct testmapvalue {
	int Number;
	uint64 Stamp;
	char Name[16];
} testmapvalue;



/* 拥有值把独立字符串的生命周期移交给映射。 */
typedef struct testmapowned {
	str Text;
} testmapowned;



/* 回调状态记录哈希、释放和访问次数。 */
typedef struct testmapstate {
	size_t HashCount;
	size_t DropCount;
	size_t VisitCount;
	xmap* ReenterMap;
	bool TryDestroy;
	bool TryClear;
	bool TryPolicy;
	bool ReenterDone;
	bool ReenterBlocked;
} testmapstate;



/* 包含映射前后合法存储，用于验证跨入元数据的完整输出区间。 */
typedef struct testmapaliasbox {
	unsigned char Prefix[16];
	xmap Map;
	unsigned char Tail[sizeof(xmap) + 16u];
} testmapaliasbox;



/* 新值初始化状态验证失败原子提交和回调重入门禁。 */
typedef struct testmapinitstate {
	xmap* Map;
	int Value;
	size_t Calls;
	bool Fail;
	bool ReentryBlocked;
} testmapinitstate;



/* 原位初始化一个整数值，并可注入失败或查询重入。 */
static bool testMapInitValue(
	xbytesview Key,
	ptr pValue,
	ptr pUserData
)
{
	testmapinitstate* pState = (testmapinitstate*)pUserData;

	testRequire(
		(Key.Data != NULL) && (Key.Data[Key.Size] == 0),
		"map initializer did not receive the stored key"
	);
	pState->Calls++;
	if ( pState->Map != NULL ) {
		xrtClearError();
		pState->ReentryBlocked =
			(xrtMapCount(pState->Map) == 0u) &&
			(xrtErrorKind(xrtGetError()) == XERR_STATE);
	}
	if ( pState->Fail ) {
		xerror* pError = xrtErrorCreate(
			XERR_VALUE, "test.map.init", 1,
			"the map initializer rejected the value"
		);

		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return false;
	}
	*(int*)pValue = pState->Value;
	return true;
}



/* 为拥有值创建一份独立字符串。 */
static testmapowned testMapOwned(cstr sText)
{
	testmapowned tValue;
	size_t iLength = strlen(sText) + 1u;

	tValue.Text = (str)xrtMalloc(iLength);
	testRequire(tValue.Text != NULL, "map owned value allocation failed");
	memcpy(tValue.Text, sText, iLength);
	return tValue;
}



/* 释放值内部字符串并累加释放次数。 */
static void testMapDrop(xbytesview Key, ptr pValue, ptr pUserData)
{
	testmapowned* pOwned = (testmapowned*)pValue;
	testmapstate* pState = (testmapstate*)pUserData;

	testRequire(Key.Data != NULL, "map drop key is not stored key");
	xrtFree(pOwned->Text);
	pOwned->Text = NULL;
	pState->DropCount++;

	/* 释放器中的同映射生命周期操作必须被拒绝。 */
	if (
		(pState->ReenterMap != NULL) &&
		pState->TryDestroy &&
		!pState->ReenterDone
	) {
		pState->ReenterDone = true;
		xrtClearError();
		xrtMapDestroy(pState->ReenterMap);
		pState->ReenterBlocked = xrtErrorKind(xrtGetError()) == XERR_STATE;
	}
}



/* 释放指针值映射中保存的目标地址。 */
static void testMapPtrDrop(xbytesview Key, ptr pValue, ptr pUserData)
{
	testmapstate* pState = (testmapstate*)pUserData;
	ptr pTarget = *(ptr*)pValue;

	(void)Key;
	xrtFree(pTarget);
	pState->DropCount++;
}



/* 自定义哈希故意制造全碰撞，并记录调用次数。 */
static uint64 testMapCollisionHash(xbytesview Key, ptr pUserData)
{
	testmapstate* pState = (testmapstate*)pUserData;

	(void)Key;
	pState->HashCount++;
	if (
		(pState->ReenterMap != NULL) &&
		pState->TryPolicy &&
		!pState->ReenterDone
	) {
		pState->ReenterDone = true;
		xrtClearError();
		pState->ReenterBlocked =
			!xrtMapSetKeyPolicy(pState->ReenterMap, NULL, NULL, NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_STATE);
	}
	return 7;
}



/* ASCII 大小写不敏感比较用于验证可扩展键策略。 */
static bool testMapAsciiEqual(xbytesview Left, xbytesview Right, ptr pUserData)
{
	testmapstate* pState = (testmapstate*)pUserData;

	if (
		(pState != NULL) &&
		(pState->ReenterMap != NULL) &&
		pState->TryClear &&
		!pState->ReenterDone
	) {
		pState->ReenterDone = true;
		xrtClearError();
		xrtMapClear(pState->ReenterMap);
		pState->ReenterBlocked = xrtErrorKind(xrtGetError()) == XERR_STATE;
	}
	if ( Left.Size != Right.Size ) {
		return false;
	}
	for ( size_t i = 0; i < Left.Size; i++ ) {
		unsigned char iLeft = Left.Data[i];
		unsigned char iRight = Right.Data[i];

		if ( (iLeft >= 'A') && (iLeft <= 'Z') ) {
			iLeft = (unsigned char)(iLeft + ('a' - 'A'));
		}
		if ( (iRight >= 'A') && (iRight <= 'Z') ) {
			iRight = (unsigned char)(iRight + ('a' - 'A'));
		}
		if ( iLeft != iRight ) {
			return false;
		}
	}
	return true;
}



/* 访问器可读当前映射，但不能改变键集合或结束生命周期。 */
static bool testMapMutationVisitor(xbytesview Key, ptr pValue, ptr pUserData)
{
	testmapstate* pState = (testmapstate*)pUserData;
	bool bClearBlocked;
	bool bDestroyBlocked;

	testRequire(Key.Data != NULL, "map mutation visitor key missing");
	testRequire(pValue != NULL, "map mutation visitor value missing");
	testRequire(xrtMapCount(pState->ReenterMap) == 2, "map visitor read was rejected");
	testRequire(
		xrtMapHas(pState->ReenterMap, Key),
		"map visitor lookup was rejected"
	);
	xrtClearError();
	xrtMapClear(pState->ReenterMap);
	bClearBlocked = xrtErrorKind(xrtGetError()) == XERR_STATE;
	xrtClearError();
	xrtMapDestroy(pState->ReenterMap);
	bDestroyBlocked = xrtErrorKind(xrtGetError()) == XERR_STATE;
	pState->ReenterBlocked = bClearBlocked && bDestroyBlocked;
	pState->ReenterDone = true;
	pState->VisitCount++;
	return false;
}



/* 访问器验证插入顺序并在第三项主动停止。 */
static bool testMapVisitor(xbytesview Key, ptr pValue, ptr pUserData)
{
	static const cstr arrExpected[] = { "first", "third", "second" };
	testmapstate* pState = (testmapstate*)pUserData;
	int* pNumber = (int*)pValue;
	cstr sExpected = arrExpected[pState->VisitCount];

	testRequire(Key.Size == strlen(sExpected), "map visitor key size mismatch");
	testRequire(memcmp(Key.Data, sExpected, Key.Size) == 0, "map visitor order mismatch");
	testRequire(*pNumber != 0, "map visitor value mismatch");
	pState->VisitCount++;
	return pState->VisitCount < 3;
}



/* 验证空键、二进制键、键副本、零值槽和大规模稳定地址。 */
static void testMapBasic(void)
{
	unsigned char arrBinary[] = { 'a', 0, 'b', 0xFFu };
	char arrMutable[] = "mutable";
	char arrKey[32];
	xmap tMap;
	testmapvalue tValue;
	testmapvalue* pStable;
	testmapvalue* pSlot;
	xbytesview Empty = { NULL, 0 };
	xbytesview Binary = { arrBinary, sizeof(arrBinary) };
	bool bNew;

	testRequire(xrtMapInit(&tMap, sizeof(testmapvalue)), "map init failed");
	testRequire((xrtMapCount(&tMap) == 0) && (xrtMapCapacity(&tMap) == 0), "empty map state mismatch");
	pSlot = (testmapvalue*)xrtMapGetOrAdd(&tMap, Empty, &bNew);
	testRequire((pSlot != NULL) && bNew, "map empty key insert failed");
	for ( size_t i = 0; i < sizeof(testmapvalue); i++ ) {
		testRequire(((bytes)pSlot)[i] == 0, "map new value was not zeroed");
	}
	pSlot->Number = 1;
	testRequire(xrtMapSet(&tMap, Binary, &(testmapvalue){ 2, 3, "binary" }), "map binary key set failed");
	testRequire(
		((testmapvalue*)xrtMapGet(&tMap, Binary))->Number == 2,
		"map binary key get mismatch"
	);

	/* 键必须由映射复制，调用方后续修改源缓冲不能改变索引。 */
	testRequire(
		xrtMapSet(
			&tMap,
			(xbytesview){ (cbytes)arrMutable, sizeof(arrMutable) - 1u },
			&(testmapvalue){ 7, 8, "copied" }
		),
		"map mutable key set failed"
	);
	arrMutable[0] = 'X';
	testRequire(
		((testmapvalue*)xrtMapGet(&tMap, XRT_BYTES_LITERAL("mutable")))->Number == 7,
		"map did not own key copy"
	);

	pStable = (testmapvalue*)xrtMapGet(&tMap, Empty);
	testRequire(pStable != NULL, "map stable value missing");
	for ( int i = 0; i < 20000; i++ ) {
		int iLength = snprintf(arrKey, sizeof(arrKey), "key-%d", i);

		memset(&tValue, 0, sizeof(tValue));
		tValue.Number = i + 10;
		tValue.Stamp = (uint64)i;
		testRequire(
			xrtMapSet(&tMap, (xbytesview){ (cbytes)arrKey, (size_t)iLength }, &tValue),
			"map growth set failed"
		);
	}
	testRequire(xrtMapGet(&tMap, Empty) == pStable, "map rehash moved value address");
	testRequire(xrtMapCount(&tMap) == 20003, "map growth count mismatch");

	/* 旧版百万插入和千万查询边界缩短为日常回归，完整资产仍保留在基线。 */
	for ( int i = 0; i < 200000; i++ ) {
		int iIndex = i % 20000;
		int iLength = snprintf(arrKey, sizeof(arrKey), "key-%d", iIndex);

		pSlot = (testmapvalue*)xrtMapGet(
			&tMap,
			(xbytesview){ (cbytes)arrKey, (size_t)iLength }
		);
		testRequire((pSlot != NULL) && (pSlot->Number == (iIndex + 10)), "map lookup stress mismatch");
	}

	memset(&tValue, 0, sizeof(tValue));
	tValue.Number = 99;
	testRequire(xrtMapSet(&tMap, Binary, &tValue), "map replacement failed");
	pSlot = (testmapvalue*)xrtMapGet(&tMap, Binary);
	testRequire((pSlot != NULL) && (pSlot->Number == 99), "map replacement mismatch");
	testRequire(xrtMapSet(&tMap, Binary, pSlot), "map same-slot set should be no-op");

	xrtClearError();
	testRequire(xrtMapGet(&tMap, XRT_BYTES_LITERAL("missing")) == NULL, "map missing get should be null");
	testRequire(xrtGetError() == NULL, "map missing get reported an error");
	testRequire(xrtMapHas(&tMap, Empty), "map empty key missing");
	testRequire(!xrtMapHas(&tMap, XRT_BYTES_LITERAL("missing")), "map missing key reported present");
	testRequire(
		((const testmapvalue*)xrtMapConstGet(&tMap, Binary))->Number == 99,
		"map const get mismatch"
	);
	xrtMapUnit(&tMap);
}



/* 验证新值初始化只执行一次且失败时不提交键。 */
static void testMapGetOrInit(void)
{
	xmap Map;
	testmapinitstate State = { &Map, 42, 0u, false, false };
	int* pValue;
	bool bNew;

	testRequire(xrtMapInit(&Map, sizeof(int)), "map initializer setup failed");
	pValue = (int*)xrtMapGetOrInit(
		&Map,
		XRT_BYTES_LITERAL("ready"),
		testMapInitValue,
		&State,
		&bNew
	);
	testRequire(
		(pValue != NULL) && bNew && (*pValue == 42) &&
		(State.Calls == 1u) && State.ReentryBlocked &&
		(xrtMapCount(&Map) == 1u),
		"map initializer success contract mismatch"
	);
	State.Fail = true;
	pValue = (int*)xrtMapGetOrInit(
		&Map,
		XRT_BYTES_LITERAL("ready"),
		testMapInitValue,
		&State,
		&bNew
	);
	testRequire(
		(pValue != NULL) && !bNew && (*pValue == 42) &&
		(State.Calls == 1u),
		"map initializer ran for an existing key"
	);
	xrtClearError();
	testRequire(
		xrtMapGetOrInit(
			&Map,
			XRT_BYTES_LITERAL("failed"),
			testMapInitValue,
			&State,
			&bNew
		) == NULL &&
		!bNew &&
		(State.Calls == 2u) &&
		(xrtMapCount(&Map) == 1u) &&
		!xrtMapHas(&Map, XRT_BYTES_LITERAL("failed")) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "test.map.init") == 0),
		"map initializer failure committed a key or lost its error"
	);
	xrtMapUnit(&Map);
}



/* 验证自定义碰撞策略、等价键和内部规范键查询。 */
static void testMapKeyPolicy(void)
{
	xmap tMap;
	testmapstate tState = { 0 };
	xbytesview Stored;
	int iValue = 10;
	int iReplacement = 20;
	bool bNew;

	testRequire(xrtMapInit(&tMap, sizeof(int)), "custom map init failed");
	testRequire(
		xrtMapSetKeyPolicy(&tMap, testMapCollisionHash, testMapAsciiEqual, &tState),
		"custom map policy setup failed"
	);
	testRequire(xrtMapSet(&tMap, XRT_BYTES_LITERAL("Alpha"), &iValue), "custom map first set failed");
	testRequire(xrtMapSet(&tMap, XRT_BYTES_LITERAL("Beta"), &iValue), "custom map collision set failed");
	testRequire(
		*(int*)xrtMapGet(&tMap, XRT_BYTES_LITERAL("alpha")) == 10,
		"custom map equivalent lookup failed"
	);
	testRequire(
		xrtMapGetOrAdd(&tMap, XRT_BYTES_LITERAL("ALPHA"), &bNew) != NULL && !bNew,
		"custom map equivalent get-or-add inserted duplicate"
	);
	testRequire(xrtMapSet(&tMap, XRT_BYTES_LITERAL("aLpHa"), &iReplacement), "custom map replace failed");
	testRequire((xrtMapCount(&tMap) == 2) && (tState.HashCount != 0), "custom map state mismatch");
	testRequire(
		xrtMapStoredKey(&tMap, XRT_BYTES_LITERAL("ALPHA"), &Stored),
		"custom map stored key lookup failed"
	);
	testRequire(
		(Stored.Size == 5) && (memcmp(Stored.Data, "Alpha", 5) == 0) && (Stored.Data[5] == 0),
		"custom map did not preserve canonical key"
	);

	xrtClearError();
	testRequire(
		!xrtMapSetKeyPolicy(&tMap, NULL, NULL, NULL),
		"non-empty map accepted key policy replacement"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "map key policy state error mismatch");
	xrtMapClear(&tMap);
	testRequire(xrtMapSetKeyPolicy(&tMap, NULL, NULL, NULL), "map default policy restore failed");
	testRequire(xrtMapSet(&tMap, XRT_BYTES_LITERAL("A"), &iValue), "restored map first set failed");
	testRequire(xrtMapSet(&tMap, XRT_BYTES_LITERAL("a"), &iValue), "restored map exact set failed");
	testRequire(xrtMapCount(&tMap) == 2, "restored exact policy merged keys");
	xrtMapUnit(&tMap);
}



/* 验证插入顺序、反向迭代、容量变更和结构修改失效。 */
static void testMapOrderAndCapacity(void)
{
	static const cstr arrForward[] = { "first", "third", "second" };
	xmap tMap;
	xmapiter tIterator;
	xbytesview Key;
	testmapstate tState = { 0 };
	int iFirst = 1;
	int iSecond = 2;
	int iThird = 3;
	int iFourth = 4;
	int* pStable;
	int* pValue;
	size_t iReserved;

	testRequire(xrtMapInit(&tMap, sizeof(int)), "ordered map init failed");
	testRequire(xrtMapSet(&tMap, XRT_BYTES_LITERAL("first"), &iFirst), "ordered map first set failed");
	testRequire(xrtMapSet(&tMap, XRT_BYTES_LITERAL("second"), &iSecond), "ordered map second set failed");
	testRequire(xrtMapSet(&tMap, XRT_BYTES_LITERAL("third"), &iThird), "ordered map third set failed");
	pStable = (int*)xrtMapGet(&tMap, XRT_BYTES_LITERAL("first"));
	testRequire(xrtMapRemove(&tMap, XRT_BYTES_LITERAL("second")), "ordered map remove failed");
	testRequire(xrtMapSet(&tMap, XRT_BYTES_LITERAL("second"), &iSecond), "ordered map reinsert failed");

	testRequire(xrtMapIterBegin(&tMap, &tIterator), "map forward iterator begin failed");
	for ( size_t i = 0; i < 3; i++ ) {
		pValue = (int*)xrtMapIterNext(&tIterator, &Key);
		testRequire(pValue != NULL, "map forward iterator ended early");
		testRequire(
			(Key.Size == strlen(arrForward[i])) &&
			(memcmp(Key.Data, arrForward[i], Key.Size) == 0),
			"map forward insertion order mismatch"
		);
	}
	testRequire(xrtMapIterNext(&tIterator, &Key) == NULL, "map forward iterator did not end");
	testRequire((Key.Data == NULL) && (Key.Size == 0), "map ended iterator did not clear key");

	testRequire(xrtMapIterRBegin(&tMap, &tIterator), "map reverse iterator begin failed");
	for ( size_t i = 3; i != 0; i-- ) {
		pValue = (int*)xrtMapIterNext(&tIterator, &Key);
		testRequire(pValue != NULL, "map reverse iterator ended early");
		testRequire(
			(Key.Size == strlen(arrForward[i - 1u])) &&
			(memcmp(Key.Data, arrForward[i - 1u], Key.Size) == 0),
			"map reverse insertion order mismatch"
		);
	}
	xrtMapIterEnd(&tIterator);

	/* 纯桶扩缩不改变键集合，因此不移动值地址，也不使迭代器失效。 */
	testRequire(xrtMapIterBegin(&tMap, &tIterator), "map reserve iterator begin failed");
	testRequire(xrtMapIterNext(&tIterator, &Key) == pStable, "map reserve iterator first mismatch");
	testRequire(xrtMapReserve(&tMap, 1000), "map reserve failed");
	iReserved = xrtMapCapacity(&tMap);
	testRequire(iReserved >= 1000, "map reserve capacity mismatch");
	testRequire(xrtMapGet(&tMap, XRT_BYTES_LITERAL("first")) == pStable, "map reserve moved value");
	testRequire(xrtMapIterNext(&tIterator, &Key) != NULL, "map reserve invalidated iterator");
	testRequire(xrtMapTrim(&tMap), "map trim failed");
	testRequire(xrtMapCapacity(&tMap) < iReserved, "map trim did not shrink buckets");
	testRequire(xrtMapGet(&tMap, XRT_BYTES_LITERAL("first")) == pStable, "map trim moved value");
	testRequire(xrtMapIterNext(&tIterator, &Key) != NULL, "map trim invalidated iterator");

	testRequire(xrtMapIterBegin(&tMap, &tIterator), "map mutation iterator begin failed");
	pValue = (int*)xrtMapIterNext(&tIterator, &Key);
	testRequire(pValue != NULL, "map mutation iterator first missing");
	(*pValue)++;
	testRequire(xrtMapIterNext(&tIterator, &Key) != NULL, "map value mutation invalidated iterator");
	testRequire(xrtMapSet(&tMap, XRT_BYTES_LITERAL("fourth"), &iFourth), "map structural set failed");
	xrtClearError();
	testRequire(xrtMapIterNext(&tIterator, &Key) == NULL, "map mutated iterator should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "map mutation error mismatch");

	testRequire(xrtMapVisit(&tMap, testMapVisitor, &tState) == 3, "map visitor stop mismatch");
	testRequire(tState.VisitCount == 3, "map visitor count mismatch");
	xrtMapUnit(&tMap);
}



/* 验证释放器、替换、Remove、Take、Clear 和桶复用。 */
static void testMapOwnership(void)
{
	xmap tMap;
	testmapstate tState = { 0 };
	testmapowned tValue;
	testmapowned tTaken;
	size_t iCapacity;

	testRequire(xrtMapInit(&tMap, sizeof(testmapowned)), "owned map init failed");
	testRequire(xrtMapSetDrop(&tMap, testMapDrop, &tState), "owned map drop setup failed");
	tValue = testMapOwned("first");
	testRequire(xrtMapSet(&tMap, XRT_BYTES_LITERAL("key"), &tValue), "owned map first set failed");
	tValue = testMapOwned("second");
	testRequire(xrtMapSet(&tMap, XRT_BYTES_LITERAL("key"), &tValue), "owned map replacement failed");
	testRequire(tState.DropCount == 1, "owned map replacement did not drop old value");

	memset(&tTaken, 0, sizeof(tTaken));
	testRequire(xrtMapTake(&tMap, XRT_BYTES_LITERAL("key"), &tTaken), "owned map take failed");
	testRequire(
		(tState.DropCount == 1) && (strcmp(tTaken.Text, "second") == 0),
		"owned map take called drop or copied wrong value"
	);
	xrtFree(tTaken.Text);

	for ( int i = 0; i < 100; i++ ) {
		char arrKey[24];
		int iLength = snprintf(arrKey, sizeof(arrKey), "owned-%d", i);

		tValue = testMapOwned("entry");
		testRequire(
			xrtMapSet(&tMap, (xbytesview){ (cbytes)arrKey, (size_t)iLength }, &tValue),
			"owned map batch set failed"
		);
	}
	testRequire(xrtMapRemove(&tMap, XRT_BYTES_LITERAL("owned-5")), "owned map remove failed");
	testRequire(tState.DropCount == 2, "owned map remove did not drop value");
	testRequire(!xrtMapRemove(&tMap, XRT_BYTES_LITERAL("missing")), "owned map missing remove should fail");
	xrtClearError();
	testRequire(!xrtMapSetDrop(&tMap, testMapDrop, &tState), "non-empty map accepted drop replacement");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "map drop state error mismatch");

	iCapacity = xrtMapCapacity(&tMap);
	xrtMapClear(&tMap);
	testRequire((xrtMapCount(&tMap) == 0) && (tState.DropCount == 101), "owned map clear mismatch");
	testRequire(xrtMapCapacity(&tMap) == iCapacity, "map clear discarded reusable buckets");
	testRequire(xrtMapTrim(&tMap), "empty map trim failed");
	testRequire(xrtMapCapacity(&tMap) == 0, "empty map trim retained buckets");
	xrtMapUnit(&tMap);
}



/* 验证指针便利层区分缺失键和已保存空指针。 */
static void testMapPointers(void)
{
	xmap tMap;
	xmap tWrong;
	testmapstate tState = { 0 };
	ptr pFirst;
	ptr pSecond;
	ptr pTaken = (ptr)(uintptr_t)1;

	testRequire(xrtMapInit(&tMap, sizeof(ptr)), "pointer map init failed");
	testRequire(xrtMapSetDrop(&tMap, testMapPtrDrop, &tState), "pointer map drop setup failed");
	testRequire(xrtMapSetPtr(&tMap, XRT_BYTES_LITERAL("null"), NULL), "pointer map null set failed");
	testRequire(xrtMapHas(&tMap, XRT_BYTES_LITERAL("null")), "pointer map null key missing");
	testRequire(xrtMapGetPtr(&tMap, XRT_BYTES_LITERAL("null")) == NULL, "pointer map null value mismatch");

	pFirst = xrtMalloc(16);
	pSecond = xrtMalloc(16);
	testRequire((pFirst != NULL) && (pSecond != NULL), "pointer map target allocation failed");
	testRequire(xrtMapSetPtr(&tMap, XRT_BYTES_LITERAL("ptr"), pFirst), "pointer map first set failed");
	testRequire(xrtMapSetPtr(&tMap, XRT_BYTES_LITERAL("ptr"), pFirst), "pointer map equal set failed");
	testRequire(tState.DropCount == 0, "pointer map equal set dropped target");
	testRequire(xrtMapSetPtr(&tMap, XRT_BYTES_LITERAL("ptr"), pSecond), "pointer map replacement failed");
	testRequire(tState.DropCount == 1, "pointer map replacement did not drop old target");
	testRequire(xrtMapTakePtr(&tMap, XRT_BYTES_LITERAL("ptr"), &pTaken), "pointer map take failed");
	testRequire((pTaken == pSecond) && (tState.DropCount == 1), "pointer map take mismatch");
	xrtFree(pTaken);

	pTaken = (ptr)(uintptr_t)1;
	testRequire(!xrtMapTakePtr(&tMap, XRT_BYTES_LITERAL("missing"), &pTaken), "pointer map missing take should fail");
	testRequire(pTaken == NULL, "pointer map missing take did not clear output");

	testRequire(xrtMapInit(&tWrong, sizeof(ptr) + 1u), "wrong pointer map init failed");
	xrtClearError();
	testRequire(!xrtMapSetPtr(&tWrong, XRT_BYTES_LITERAL("key"), NULL), "wrong pointer map set should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "wrong pointer map error mismatch");
	xrtMapUnit(&tWrong);

	xrtMapUnit(&tMap);
	testRequire(tState.DropCount == 2, "pointer map unit did not drop stored null value");
}



/* 验证用户回调不能重入破坏正在执行的映射操作。 */
static void testMapReentry(void)
{
	xmap tDropMap;
	xmap tPolicyMap;
	xmap tVisitMap;
	testmapowned tOwned;
	testmapstate tDropState = { 0 };
	testmapstate tPolicyState = { 0 };
	testmapstate tVisitState = { 0 };
	int iFirst = 10;
	int iSecond = 20;

	testRequire(
		xrtMapInit(&tDropMap, sizeof(testmapowned)),
		"map drop reentry init failed"
	);
	tDropState.ReenterMap = &tDropMap;
	tDropState.TryDestroy = true;
	testRequire(
		xrtMapSetDrop(&tDropMap, testMapDrop, &tDropState),
		"map drop reentry setup failed"
	);
	tOwned = testMapOwned("reentry");
	testRequire(
		xrtMapSet(&tDropMap, XRT_BYTES_LITERAL("owned"), &tOwned),
		"map drop reentry value setup failed"
	);
	testRequire(
		xrtMapRemove(&tDropMap, XRT_BYTES_LITERAL("owned")),
		"map drop reentry outer remove failed"
	);
	testRequire(tDropState.ReenterDone, "map drop reentry was not exercised");
	testRequire(tDropState.ReenterBlocked, "map drop reentry destroy was allowed");
	xrtClearError();
	testRequire(xrtMapCount(&tDropMap) == 0, "map drop reentry damaged map");
	xrtMapUnit(&tDropMap);

	testRequire(xrtMapInit(&tPolicyMap, sizeof(int)), "map policy reentry init failed");
	tPolicyState.ReenterMap = &tPolicyMap;
	tPolicyState.TryPolicy = true;
	testRequire(
		xrtMapSetKeyPolicy(
			&tPolicyMap,
			testMapCollisionHash,
			testMapAsciiEqual,
			&tPolicyState
		),
		"map policy reentry setup failed"
	);
	testRequire(
		xrtMapSet(&tPolicyMap, XRT_BYTES_LITERAL("Alpha"), &iFirst),
		"map hash reentry outer set failed"
	);
	testRequire(tPolicyState.ReenterDone, "map hash reentry was not exercised");
	testRequire(tPolicyState.ReenterBlocked, "map hash reentry policy change was allowed");
	xrtClearError();
	testRequire(
		*(int*)xrtMapGet(&tPolicyMap, XRT_BYTES_LITERAL("alpha")) == 10,
		"map hash reentry changed key policy"
	);

	tPolicyState.TryPolicy = false;
	tPolicyState.TryClear = true;
	tPolicyState.ReenterDone = false;
	tPolicyState.ReenterBlocked = false;
	xrtClearError();
	testRequire(
		*(int*)xrtMapGet(&tPolicyMap, XRT_BYTES_LITERAL("ALPHA")) == 10,
		"map equal reentry outer lookup failed"
	);
	testRequire(tPolicyState.ReenterDone, "map equal reentry was not exercised");
	testRequire(tPolicyState.ReenterBlocked, "map equal reentry clear was allowed");
	xrtClearError();
	testRequire(xrtMapCount(&tPolicyMap) == 1, "map equal reentry changed key set");
	xrtMapUnit(&tPolicyMap);

	testRequire(xrtMapInit(&tVisitMap, sizeof(int)), "map visitor reentry init failed");
	testRequire(
		xrtMapSet(&tVisitMap, XRT_BYTES_LITERAL("first"), &iFirst) &&
		xrtMapSet(&tVisitMap, XRT_BYTES_LITERAL("second"), &iSecond),
		"map visitor reentry setup failed"
	);
	tVisitState.ReenterMap = &tVisitMap;
	testRequire(
		xrtMapVisit(&tVisitMap, testMapMutationVisitor, &tVisitState) == 1,
		"map visitor reentry count mismatch"
	);
	testRequire(tVisitState.ReenterDone, "map visitor mutation was not exercised");
	testRequire(tVisitState.ReenterBlocked, "map visitor structural mutation was allowed");
	xrtClearError();
	testRequire(xrtMapCount(&tVisitMap) == 2, "map visitor mutation changed key set");
	xrtMapUnit(&tVisitMap);
}



/* 验证复制来源和 Take 输出不能触及映射拥有的任何内存区间。 */
static void testMapAliases(void)
{
	testmapaliasbox tBox;
	xmap tMap;
	unsigned char arrLargeValue[sizeof(xmap) + 1u];
	int iValue = 1;
	int* pFirst;
	int* pSecond;
	size_t iCapacity;

	memset(&tBox, 0, sizeof(tBox));
	memset(arrLargeValue, 0x5A, sizeof(arrLargeValue));
	testRequire(
		xrtMapInit(&tBox.Map, sizeof(arrLargeValue)),
		"map metadata alias init failed"
	);
	testRequire(
		xrtMapSet(
			&tBox.Map,
			XRT_BYTES_LITERAL("large"),
			arrLargeValue
		),
		"map metadata alias setup failed"
	);
	xrtClearError();
	testRequire(
		!xrtMapTake(
			&tBox.Map,
			XRT_BYTES_LITERAL("large"),
			&tBox.Prefix[sizeof(tBox.Prefix) - 1u]
		),
		"map take accepted output range crossing map metadata"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "map metadata alias error");
	testRequire(
		xrtMapHas(&tBox.Map, XRT_BYTES_LITERAL("large")),
		"map metadata alias removed target"
	);
	xrtMapUnit(&tBox.Map);

	testRequire(xrtMapInit(&tMap, sizeof(int)), "map entry alias init failed");
	testRequire(
		xrtMapSet(&tMap, XRT_BYTES_LITERAL("first"), &iValue) &&
		xrtMapSet(&tMap, XRT_BYTES_LITERAL("second"), &iValue),
		"map entry alias setup failed"
	);
	pFirst = (int*)xrtMapGet(&tMap, XRT_BYTES_LITERAL("first"));
	pSecond = (int*)xrtMapGet(&tMap, XRT_BYTES_LITERAL("second"));
	xrtClearError();
	testRequire(
		!xrtMapSet(
			&tMap,
			XRT_BYTES_LITERAL("first"),
			(bytes)pFirst + 1u
		),
		"map set accepted an interior target source"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "map set target alias error");
	xrtClearError();
	testRequire(
		!xrtMapSet(&tMap, XRT_BYTES_LITERAL("third"), &tMap.Count),
		"map set accepted map metadata as source"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "map set metadata alias error");
	xrtClearError();
	testRequire(
		!xrtMapTake(&tMap, XRT_BYTES_LITERAL("first"), pSecond),
		"map take accepted another owned value as output"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "map take entry alias error");
	testRequire(
		(xrtMapCount(&tMap) == 2) &&
		(*pFirst == 1) &&
		(*pSecond == 1),
		"map entry alias changed values or key set"
	);
	xrtMapUnit(&tMap);

	testRequire(xrtMapInit(&tMap, sizeof(ptr)), "map bucket alias init failed");
	for ( int i = 0; i < 12; i++ ) {
		char arrKey[16];
		int iLength = snprintf(arrKey, sizeof(arrKey), "key-%d", i);
		ptr pPointer = (ptr)(uintptr_t)(i + 1);

		testRequire(
			xrtMapSet(
				&tMap,
				(xbytesview){ (cbytes)arrKey, (size_t)iLength },
				&pPointer
			),
			"map bucket alias setup failed"
		);
	}
	iCapacity = xrtMapCapacity(&tMap);
	testRequire(iCapacity == 12, "map bucket alias threshold mismatch");
	xrtClearError();
	testRequire(
		!xrtMapSet(
			&tMap,
			XRT_BYTES_LITERAL("growth"),
			tMap.Buckets
		),
		"map set accepted bucket memory as growth source"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "map bucket alias error");
	testRequire(
		(xrtMapCount(&tMap) == 12) &&
		(xrtMapCapacity(&tMap) == iCapacity) &&
		!xrtMapHas(&tMap, XRT_BYTES_LITERAL("growth")),
		"map bucket alias committed growth"
	);
	xrtMapUnit(&tMap);
}



/* 验证过对齐值、堆创建、布局溢出和危险输出别名。 */
static void testMapAlignmentAndErrors(void)
{
	xmap tMap;
	xmap* pCreated;
	ptr pValue;
	int iValue = 5;
	bool bNew = true;

	testRequire(xrtMapInitAligned(&tMap, 64, 64), "aligned map init failed");
	pValue = xrtMapGetOrAdd(&tMap, XRT_BYTES_LITERAL("aligned"), &bNew);
	testRequire(
		(pValue != NULL) && bNew && (((uintptr_t)pValue & 63u) == 0),
		"map value alignment mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtMapTake(&tMap, XRT_BYTES_LITERAL("aligned"), pValue),
		"map take accepted output inside removed entry"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "map take alias error mismatch");
	testRequire(xrtMapHas(&tMap, XRT_BYTES_LITERAL("aligned")), "map take alias removed key");
	xrtMapUnit(&tMap);

	pCreated = xrtMapCreate(sizeof(uint64));
	testRequire(pCreated != NULL, "map create failed");
	xrtMapDestroy(pCreated);

	testRequire(!xrtMapInit(&tMap, 0), "zero-sized map should fail");
	testRequire(!xrtMapInitAligned(&tMap, sizeof(int), 24), "invalid map alignment should fail");
	xrtClearError();
	testRequire(!xrtMapInitAligned(&tMap, SIZE_MAX, 16), "overflowing map layout should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "map layout overflow error mismatch");

	testRequire(xrtMapInit(&tMap, sizeof(int)), "error map init failed");
	xrtClearError();
	testRequire(!xrtMapReserve(&tMap, SIZE_MAX), "overflowing map reserve should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "map reserve overflow error mismatch");
	testRequire(xrtMapSet(&tMap, XRT_BYTES_LITERAL("key"), &iValue), "error map set failed");
	xrtClearError();
	testRequire(
		!xrtMapSetKeyPolicy(&tMap, testMapCollisionHash, NULL, NULL),
		"map accepted incomplete key policy"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "non-empty map policy error precedence mismatch");
	xrtMapClear(&tMap);
	xrtClearError();
	testRequire(
		!xrtMapSetKeyPolicy(&tMap, testMapCollisionHash, NULL, NULL),
		"empty map accepted incomplete key policy"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "incomplete map policy error mismatch");

	bNew = true;
	xrtClearError();
	testRequire(xrtMapGetOrAdd(NULL, XRT_BYTES_LITERAL("key"), &bNew) == NULL, "null map should fail");
	testRequire(!bNew, "failed map get-or-add reported new value");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "null map error mismatch");
	xrtMapUnit(&tMap);
}



/* 运行字节键映射全部合同测试。 */
int main(void)
{
	testMapBasic();
	testMapGetOrInit();
	testMapKeyPolicy();
	testMapOrderAndCapacity();
	testMapOwnership();
	testMapPointers();
	testMapReentry();
	testMapAliases();
	testMapAlignmentAndErrors();
	printf("[PASS] map\n");
	return 0;
}
