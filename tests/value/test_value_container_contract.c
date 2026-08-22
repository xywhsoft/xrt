#include "../test.h"



/* 容器契约测试共用的句柄状态。 */
typedef struct testvaluecontainerstate {
	xvalue* Parent;
	xvalue* Item;
	size_t DropCount;
	bool HashEntered;
	bool HashReentryRejected;
	bool CloneReentryRejected;
	bool WriteReentryRejected;
} testvaluecontainerstate;



/* 释放测试句柄，并验证释放回调不能重入父容器。 */
static void testValueContainerDrop(ptr pHandle, ptr pUserData)
{
	testvaluecontainerstate* pState =
		(testvaluecontainerstate*)pUserData;
	xvalue* pClone;

	pState->DropCount++;
	if ( pState->Parent != NULL ) {
		xrtClearError();
		pClone = xrtValueClone(pState->Parent);
		pState->CloneReentryRejected =
			(pClone == NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_STATE);
		xrtValueRelease(pClone);

		xrtClearError();
		pState->WriteReentryRejected = !xrtValueArrayAppendNew(
			pState->Parent,
			xrtValueInt(99)
		) && (xrtErrorKind(xrtGetError()) == XERR_STATE);
	}
	xrtFree(pHandle);
}



/* 为 Set 测试计算稳定句柄哈希，并尝试重入父集合。 */
static uint64 testValueContainerHash(ptr pHandle, ptr pUserData)
{
	testvaluecontainerstate* pState =
		(testvaluecontainerstate*)pUserData;

	if ( !pState->HashEntered ) {
		pState->HashEntered = true;
		xrtClearError();
		pState->HashReentryRejected = !xrtValueSetHas(
			pState->Parent,
			pState->Item
		) && (xrtErrorKind(xrtGetError()) == XERR_STATE);
	}
	return (uint64)*(const int*)pHandle;
}



/* 按整数内容比较两个测试句柄。 */
static bool testValueContainerEqual(
	ptr pLeft,
	ptr pRight,
	ptr pUserData
)
{
	(void)pUserData;
	return *(const int*)pLeft == *(const int*)pRight;
}



/* 创建由指定策略拥有的整数句柄值。 */
static xvalue* testValueContainerHandle(
	int iValue,
	const xvaluehandleops* pOps,
	testvaluecontainerstate* pState
)
{
	int* pHandle = (int*)xrtMalloc(sizeof(int));
	xvalue* pValue;

	if ( pHandle == NULL ) {
		return NULL;
	}
	*pHandle = iValue;
	pValue = xrtValueHandleTake((ptr*)&pHandle, pOps, pState);
	if ( pValue == NULL ) {
		xrtFree(pHandle);
	}
	return pValue;
}



/* 验证同一值覆盖同一槽不会泄漏借用或移交引用。 */
static void testValueContainerSameValue(void)
{
	static const xvaluehandleops tOps = {
		NULL,
		testValueContainerDrop,
		testValueContainerHash,
		testValueContainerEqual
	};
	testvaluecontainerstate tIntMapState = { 0 };
	testvaluecontainerstate tObjectState = { 0 };
	xvalue* pIntMap = xrtValueIntMap();
	xvalue* pObject = xrtValueObject();
	xvalue* pIntMapItem = testValueContainerHandle(
		11,
		&tOps,
		&tIntMapState
	);
	xvalue* pObjectItem = testValueContainerHandle(
		22,
		&tOps,
		&tObjectState
	);
	xvalue* pTaken;

	testRequire(
		(pIntMap != NULL) && (pObject != NULL) &&
		(pIntMapItem != NULL) && (pObjectItem != NULL),
		"same-value fixture failed"
	);
	testRequire(
		xrtValueIntMapSet(pIntMap, 7, pIntMapItem) &&
		xrtValueIntMapSet(pIntMap, 7, pIntMapItem),
		"same-value int map borrowed set failed"
	);
	pTaken = xrtValueRetain(pIntMapItem);
	testRequire(
		(pTaken != NULL) &&
		xrtValueIntMapSetTake(pIntMap, 7, &pTaken) &&
		(pTaken == NULL),
		"same-value int map take failed"
	);

	testRequire(
		xrtValueObjectSet(
			pObject,
			XRT_STR_LITERAL("item"),
			pObjectItem
		) &&
		xrtValueObjectSet(
			pObject,
			XRT_STR_LITERAL("item"),
			pObjectItem
		),
		"same-value object borrowed set failed"
	);
	pTaken = xrtValueRetain(pObjectItem);
	testRequire(
		(pTaken != NULL) &&
		xrtValueObjectSetTake(
			pObject,
			XRT_STR_LITERAL("item"),
			&pTaken
		) &&
		(pTaken == NULL),
		"same-value object take failed"
	);

	xrtValueRelease(pIntMapItem);
	xrtValueRelease(pObjectItem);
	xrtValueRelease(pIntMap);
	xrtValueRelease(pObject);
	testRequire(
		(tIntMapState.DropCount == 1) &&
		(tObjectState.DropCount == 1),
		"same-value replacement leaked a value reference"
	);
}



/* 验证 Edit 只接受子容器，失败不会改变父值内容。 */
static void testValueContainerEditType(void)
{
	xvalue* pArray = xrtValueArray();
	xvalue* pIntMap = xrtValueIntMap();
	xvalue* pObject = xrtValueObject();

	testRequire(
		(pArray != NULL) && (pIntMap != NULL) && (pObject != NULL) &&
		xrtValueArrayAppendNew(pArray, xrtValueInt(1)) &&
		xrtValueIntMapSetNew(pIntMap, 1, xrtValueInt(2)) &&
		xrtValueObjectSetNew(
			pObject,
			XRT_STR_LITERAL("value"),
			xrtValueInt(3)
		),
		"edit type fixture failed"
	);

	xrtClearError();
	testRequire(
		!xrtValueArrayAppendTake(
			pArray,
			(xvalue**)(void*)pArray
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtValueCount(pArray) == 1),
		"array take accepted source slot over target shell"
	);
	xrtClearError();
	testRequire(
		(xrtValueArrayEdit(pArray, 0) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_TYPE),
		"array edit accepted scalar child"
	);
	xrtClearError();
	testRequire(
		(xrtValueIntMapEdit(pIntMap, 1) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_TYPE),
		"int map edit accepted scalar child"
	);
	xrtClearError();
	testRequire(
		(xrtValueObjectEdit(
			pObject,
			XRT_STR_LITERAL("value")
		) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_TYPE),
		"object edit accepted scalar child"
	);
	testRequire(
		(xrtValueCount(pArray) == 1) &&
		(xrtValueCount(pIntMap) == 1) &&
		(xrtValueCount(pObject) == 1),
		"failed scalar edit changed parent content"
	);
	xrtClearError();
	testRequire(
		!xrtValueReserve(pIntMap, 16) &&
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED),
		"int map reserve contract mismatch"
	);

	xrtValueRelease(pObject);
	xrtValueRelease(pIntMap);
	xrtValueRelease(pArray);
}



/* 验证负索引解析覆盖读写路径和全部边界。 */
static void testValueContainerResolve(void)
{
	xvalue* pArray = xrtValueArray();
	size_t iResolved = 99;
	int64 iValue = 0;

	testRequire(
		(pArray != NULL) &&
		xrtValueArrayAppendNew(pArray, xrtValueInt(10)) &&
		xrtValueArrayAppendNew(pArray, xrtValueInt(20)) &&
		xrtValueArrayAppendNew(pArray, xrtValueInt(30)),
		"array resolve fixture failed"
	);
	testRequire(
		xrtValueArrayResolve(pArray, -1, &iResolved) &&
		(iResolved == 2) &&
		xrtValueArraySetNew(pArray, iResolved, xrtValueInt(31)) &&
		xrtValueGetInt(xrtValueArrayAt(pArray, -1), &iValue) &&
		(iValue == 31),
		"negative array write path failed"
	);
	testRequire(
		xrtValueArrayResolve(pArray, -3, &iResolved) &&
		(iResolved == 0),
		"negative array first index mismatch"
	);

	iResolved = 99;
	xrtClearError();
	testRequire(
		!xrtValueArrayResolve(pArray, -4, &iResolved) &&
		(iResolved == 99) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"negative array underflow changed output"
	);
	xrtClearError();
	testRequire(
		!xrtValueArrayResolve(pArray, INT64_MIN, &iResolved) &&
		(iResolved == 99) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"minimum negative index changed output"
	);
	xrtClearError();
	testRequire(
		!xrtValueArrayResolve(
			pArray,
			0,
			(size_t*)(void*)pArray
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtValueCount(pArray) == 3),
		"array resolve accepted output over value shell"
	);

	xrtValueRelease(pArray);
}



/* 验证迭代键输出不能覆盖迭代器或容器外壳。 */
static void testValueContainerIteratorAlias(void)
{
	xvalue* pArray = xrtValueArray();
	xvalueiter tIterator;
	xvaluekey Key;
	xvalue* pItem;

	testRequire(
		(pArray != NULL) &&
		xrtValueArrayAppendNew(pArray, xrtValueInt(1)),
		"iterator alias fixture failed"
	);
	xrtClearError();
	testRequire(
		!xrtValueIterBegin(pArray, (xvalueiter*)(void*)pArray) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtValueCount(pArray) == 1),
		"iterator begin accepted value-shell overlap"
	);

	testRequire(
		xrtValueIterBegin(pArray, &tIterator),
		"iterator alias begin failed"
	);
	xrtClearError();
	testRequire(
		(xrtValueIterNext(
			&tIterator,
			(xvaluekey*)(void*)&tIterator
		) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"iterator next accepted iterator overlap"
	);
	pItem = xrtValueIterNext(&tIterator, &Key);
	testRequire(
		(pItem != NULL) &&
		(Key.Type == XVALUE_KEY_INDEX) &&
		(Key.Index == 0),
		"iterator alias failure advanced snapshot"
	);
	xrtValueIterEnd(&tIterator);
	xrtValueRelease(pArray);
}



/* 验证子值释放回调不能重入正在修改的父数组。 */
static void testValueContainerDropReentry(void)
{
	static const xvaluehandleops tOps = {
		NULL,
		testValueContainerDrop,
		NULL,
		NULL
	};
	testvaluecontainerstate tState = { 0 };
	xvalue* pArray = xrtValueArray();
	xvalue* pItem = testValueContainerHandle(7, &tOps, &tState);

	testRequire(
		(pArray != NULL) && (pItem != NULL) &&
		xrtValueArrayAppendTake(pArray, &pItem),
		"drop reentry fixture failed"
	);
	tState.Parent = pArray;
	xrtClearError();
	testRequire(
		xrtValueArrayRemove(pArray, 0, 1),
		"drop reentry remove failed"
	);
	testRequire(
		(tState.DropCount == 1) &&
		tState.CloneReentryRejected &&
		tState.WriteReentryRejected &&
		(xrtValueCount(pArray) == 0),
		"drop callback reentered parent array"
	);
	tState.Parent = NULL;
	xrtValueRelease(pArray);
}



/* 验证句柄 Hash 从第一次调用起就在父集合重入门禁内。 */
static void testValueContainerHashReentry(void)
{
	static const xvaluehandleops tOps = {
		NULL,
		testValueContainerDrop,
		testValueContainerHash,
		testValueContainerEqual
	};
	testvaluecontainerstate tState = { 0 };
	xvalue* pSet = xrtValueSet();
	xvalue* pItem = testValueContainerHandle(19, &tOps, &tState);

	testRequire(
		(pSet != NULL) && (pItem != NULL),
		"hash reentry fixture failed"
	);
	tState.Parent = pSet;
	tState.Item = pItem;
	testRequire(
		xrtValueSetAdd(pSet, pItem) &&
		tState.HashEntered &&
		tState.HashReentryRejected &&
		(xrtValueCount(pSet) == 1),
		"hash callback escaped parent set reentry guard"
	);
	tState.Parent = NULL;
	tState.Item = NULL;
	xrtValueRelease(pItem);
	xrtValueRelease(pSet);
	testRequire(
		tState.DropCount == 1,
		"hash reentry fixture leaked handle"
	);
}



/* 验证共享 DAG 的环检测按唯一 backing 数量线性推进。 */
static void testValueContainerSharedDag(void)
{
	xvalue* pNode = xrtValueArray();
	xvalue* pTarget = xrtValueArray();

	testRequire(
		(pNode != NULL) && (pTarget != NULL) &&
		xrtValueArrayAppendNew(pNode, xrtValueInt(1)),
		"shared DAG fixture failed"
	);
	for ( size_t i = 0; i < 64; i++ ) {
		xvalue* pParent = xrtValueArray();

		testRequire(
			(pParent != NULL) &&
			xrtValueArrayAppend(pParent, pNode) &&
			xrtValueArrayAppend(pParent, pNode),
			"shared DAG level failed"
		);
		xrtValueRelease(pNode);
		pNode = pParent;
	}
	testRequire(
		xrtValueArrayAppend(pTarget, pNode) &&
		(xrtValueCount(pTarget) == 1),
		"shared DAG traversal failed"
	);
	xrtValueRelease(pNode);
	xrtValueRelease(pTarget);
}



/* 运行 Value 容器所有权、别名和重入契约回归。 */
int main(void)
{
	testValueContainerSameValue();
	testValueContainerEditType();
	testValueContainerResolve();
	testValueContainerIteratorAlias();
	testValueContainerDropReentry();
	testValueContainerHashReentry();
	testValueContainerSharedDag();
	printf("[PASS] value container contract\n");
	return 0;
}
