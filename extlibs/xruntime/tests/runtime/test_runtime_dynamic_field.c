#include "../test.h"



/* 验证动态字段的完整查询、所有权、顺序和便捷集合契约。 */
static void testDynamicFieldOperations(void)
{
	xrtdynamicfields* pFields = xrtDynamicFieldsCreate();
	xrtdynamicfields* pClone;
	xvalue* pSource = xrtValueArray();
	xvalue* pMoved = xrtValueString(XRT_STR_LITERAL("xlang"));
	xvalue* pCopy;
	xvalue* pTaken;
	xvalue* pKeys;
	xvalue* pValues;
	xvalue* pItems;
	const xvalue* pStored;
	xstrview StoredName;

	testRequire(
		(pFields != NULL) &&
		xrtTypeValidate(xrtDynamicFieldsType()) &&
		xrtTypedDictTypeValidate(xrtDynamicFieldsType()) &&
		xrtValueArrayAppendNew(pSource, xrtValueInt(1)) &&
		xrtDynamicFieldsSet(
			pFields, XRT_STR_LITERAL("items"), pSource
		) &&
		xrtValueArrayAppendNew(pSource, xrtValueInt(2)),
		"dynamic field setup failed"
	);
	pStored = xrtDynamicFieldsGet(
		pFields, XRT_STR_LITERAL("items")
	);
	testRequire(
		(pStored != NULL) && (xrtValueCount(pStored) == 1u),
		"dynamic field Set did not isolate the source graph"
	);
	pCopy = xrtDynamicFieldsCopy(
		pFields, XRT_STR_LITERAL("items")
	);
	testRequire(
		(pCopy != NULL) &&
		xrtValueArrayAppendNew(pCopy, xrtValueInt(3)) &&
		(xrtValueCount(pCopy) == 2u) &&
		(xrtValueCount(pStored) == 1u),
		"dynamic field Copy did not return independent ownership"
	);
	testRequire(
		xrtDynamicFieldsSetTake(
			pFields, XRT_STR_LITERAL("name"), &pMoved
		) && (pMoved == NULL) &&
		xrtDynamicFieldsStoredName(
			pFields, XRT_STR_LITERAL("name"), &StoredName
		) && (StoredName.Size == 4u) &&
		(memcmp(StoredName.Data, "name", 4u) == 0),
		"dynamic field moved insertion or stored name failed"
	);
	pKeys = xrtDynamicFieldsKeys(pFields);
	pValues = xrtDynamicFieldsValues(pFields);
	pItems = xrtDynamicFieldsItems(pFields);
	testRequire(
		(pKeys != NULL) && (pValues != NULL) && (pItems != NULL) &&
		(xrtValueCount(pKeys) == 2u) &&
		(xrtValueCount(pValues) == 2u) &&
		(xrtValueCount(pItems) == 2u) &&
		(xrtValueCount(xrtValueArrayGet(pItems, 0u)) == 2u),
		"dynamic field convenience collections mismatch"
	);
	pClone = xrtDynamicFieldsClone(pFields);
	testRequire(
		(pClone != NULL) &&
		(xrtDynamicFieldsCount(pClone) == 2u) &&
		xrtDynamicFieldsSetNew(
			pClone, XRT_STR_LITERAL("extra"), xrtValueBool(true)
		) &&
		(xrtDynamicFieldsCount(pFields) == 2u) &&
		xrtDynamicFieldsMerge(pFields, pClone, false) &&
		(xrtDynamicFieldsCount(pFields) == 3u),
		"dynamic field clone or merge failed"
	);
	pTaken = xrtDynamicFieldsTake(
		pFields, XRT_STR_LITERAL("name")
	);
	testRequire(
		(pTaken != NULL) &&
		!xrtDynamicFieldsHas(pFields, XRT_STR_LITERAL("name")) &&
		xrtDynamicFieldsRemove(pFields, XRT_STR_LITERAL("extra")) &&
		(xrtDynamicFieldsCount(pFields) == 1u) &&
		xrtDynamicFieldsClear(pFields) &&
		(xrtDynamicFieldsCount(pFields) == 0u),
		"dynamic field take, remove, or clear failed"
	);

	xrtValueRelease(pTaken);
	xrtValueRelease(pItems);
	xrtValueRelease(pValues);
	xrtValueRelease(pKeys);
	xrtValueRelease(pCopy);
	xrtValueRelease(pSource);
	xrtDynamicFieldsUnref(pClone);
	xrtDynamicFieldsUnref(pFields);
}



/* 验证复制、接管、合并和克隆都隔离完整嵌套 Value 图。 */
static void testDynamicFieldNestedIsolation(void)
{
	xrtdynamicfields* pFields = xrtDynamicFieldsCreate();
	xrtdynamicfields* pSourceFields = xrtDynamicFieldsCreate();
	xrtdynamicfields* pMergedFields = xrtDynamicFieldsCreate();
	xrtdynamicfields* pClonedFields;
	xvalue* pSource = xrtValueArray();
	xvalue* pChild = xrtValueArray();
	xvalue* pMoved = xrtValueArray();
	xvalue* pMovedChild = xrtValueArray();
	xvalue* pNested = xrtValueArray();
	xvalue* pNestedChild = xrtValueArray();
	xvalue* pTaken;
	xvalue* pTakenChild;
	const xvalue* pStored;
	const xvalue* pStoredChild;

	testRequire(
		(pFields != NULL) && (pSourceFields != NULL) &&
		(pMergedFields != NULL) && (pSource != NULL) &&
		(pChild != NULL) && (pMoved != NULL) &&
		(pMovedChild != NULL) && (pNested != NULL) &&
		(pNestedChild != NULL) &&
		xrtValueArrayAppendNew(pChild, xrtValueInt(1)) &&
		xrtValueArrayAppend(pSource, pChild) &&
		xrtDynamicFieldsSet(
			pFields, XRT_STR_LITERAL("copy"), pSource
		) &&
		xrtValueArrayAppendNew(pChild, xrtValueInt(2)),
		"dynamic field nested copy fixture failed"
	);
	pStored = xrtDynamicFieldsGet(
		pFields, XRT_STR_LITERAL("copy")
	);
	pStoredChild = xrtValueArrayGet(pStored, 0u);
	testRequire(
		(pStoredChild != NULL) && (xrtValueCount(pStoredChild) == 1u),
		"dynamic field Set shared a nested source value"
	);

	testRequire(
		xrtValueArrayAppendNew(pMovedChild, xrtValueInt(3)) &&
		xrtValueArrayAppend(pMoved, pMovedChild) &&
		xrtDynamicFieldsSetTake(
			pFields, XRT_STR_LITERAL("move"), &pMoved
		) && (pMoved == NULL) &&
		xrtValueArrayAppendNew(pMovedChild, xrtValueInt(4)),
		"dynamic field nested move fixture failed"
	);
	pStored = xrtDynamicFieldsGet(
		pFields, XRT_STR_LITERAL("move")
	);
	pStoredChild = xrtValueArrayGet(pStored, 0u);
	testRequire(
		(pStoredChild != NULL) && (xrtValueCount(pStoredChild) == 1u),
		"dynamic field SetTake shared a nested source value"
	);

	testRequire(
		xrtValueArrayAppendNew(pNestedChild, xrtValueInt(5)) &&
		xrtValueArrayAppendTake(pNested, &pNestedChild) &&
		xrtDynamicFieldsSetNew(
			pSourceFields, XRT_STR_LITERAL("nested"), pNested
		) &&
		xrtDynamicFieldsMerge(pMergedFields, pSourceFields, true),
		"dynamic field nested merge fixture failed"
	);
	pClonedFields = xrtDynamicFieldsClone(pSourceFields);
	pTaken = xrtDynamicFieldsTake(
		pSourceFields, XRT_STR_LITERAL("nested")
	);
	pTakenChild = pTaken != NULL ? xrtValueArrayGet(pTaken, 0u) : NULL;
	testRequire(
		(pClonedFields != NULL) && (pTakenChild != NULL) &&
		xrtValueArrayAppendNew(pTakenChild, xrtValueInt(6)),
		"dynamic field nested clone mutation fixture failed"
	);
	pStored = xrtDynamicFieldsGet(
		pMergedFields, XRT_STR_LITERAL("nested")
	);
	pStoredChild = xrtValueArrayGet(pStored, 0u);
	testRequire(
		(pStoredChild != NULL) && (xrtValueCount(pStoredChild) == 1u),
		"dynamic field Merge shared a nested source value"
	);
	pStored = xrtDynamicFieldsGet(
		pClonedFields, XRT_STR_LITERAL("nested")
	);
	pStoredChild = xrtValueArrayGet(pStored, 0u);
	testRequire(
		(pStoredChild != NULL) && (xrtValueCount(pStoredChild) == 1u),
		"dynamic field Clone shared a nested source value"
	);

	xrtValueRelease(pTaken);
	xrtValueRelease(pNestedChild);
	xrtValueRelease(pMovedChild);
	xrtValueRelease(pMoved);
	xrtValueRelease(pChild);
	xrtValueRelease(pSource);
	xrtDynamicFieldsUnref(pClonedFields);
	xrtDynamicFieldsUnref(pMergedFields);
	xrtDynamicFieldsUnref(pSourceFields);
	xrtDynamicFieldsUnref(pFields);
}



/* 验证 Ref 路径保留 Value 身份，而 Value 转换继续隔离完整图。 */
static void testDynamicFieldSharedValues(void)
{
	xrtdynamicfields* pFields = xrtDynamicFieldsCreate();
	xrtdynamicfields* pImported;
	xvalue* pSource = xrtValueArray();
	xvalue* pReference;
	xvalue* pMoved = xrtValueString(XRT_STR_LITERAL("moved"));
	xvalue* pObject;
	const xvalue* pStored;
	const xvalue* pExported;

	testRequire(
		(pFields != NULL) && (pSource != NULL) &&
		xrtValueArrayAppendNew(pSource, xrtValueInt(1)) &&
		xrtDynamicFieldsSetRef(
			pFields, XRT_STR_LITERAL("items"), pSource
		),
		"dynamic field shared setup failed"
	);
	pReference = xrtDynamicFieldsGetRef(
		pFields, XRT_STR_LITERAL("items")
	);
	testRequire(
		(pReference == pSource) &&
		xrtValueArrayAppendNew(pSource, xrtValueInt(2)) &&
		(xrtValueCount(xrtDynamicFieldsGet(
			pFields, XRT_STR_LITERAL("items")
		)) == 2u) &&
		xrtDynamicFieldsSetRefTake(
			pFields, XRT_STR_LITERAL("moved"), &pMoved
		) && (pMoved == NULL) &&
		xrtDynamicFieldsSetRefNew(
			pFields, XRT_STR_LITERAL("ready"), xrtValueBool(true)
		),
		"dynamic field Ref ownership contract failed"
	);
	pObject = xrtDynamicFieldsToValue(pFields);
	pExported = pObject != NULL ? xrtValueObjectGet(
		pObject, XRT_STR_LITERAL("items")
	) : NULL;
	testRequire(
		(pObject != NULL) && (pExported != NULL) &&
		(pExported != pSource) &&
		xrtValueArrayAppendNew(pSource, xrtValueInt(3)) &&
		(xrtValueCount(pExported) == 2u),
		"dynamic field Value export shared its source graph"
	);
	pImported = xrtDynamicFieldsFromValue(pObject);
	pStored = pImported != NULL ? xrtDynamicFieldsGet(
		pImported, XRT_STR_LITERAL("items")
	) : NULL;
	testRequire(
		(pImported != NULL) && (pStored != NULL) &&
		(xrtValueCount(pStored) == 2u) &&
		(xrtDynamicFieldsCount(pImported) == 3u),
		"dynamic field Value import mismatch"
	);

	xrtDynamicFieldsUnref(pImported);
	xrtValueRelease(pObject);
	xrtValueRelease(pMoved);
	xrtValueRelease(pReference);
	xrtValueRelease(pSource);
	xrtDynamicFieldsUnref(pFields);
}



/* 验证动态字段作为独立图节点能够回收自引用环。 */
static void testDynamicFieldObjectGraph(void)
{
	xrtobjectgraph* pGraph = xrtObjectGraphCreate();
	xrtdynamicfields* pFields = xrtDynamicFieldsCreate();
	xrtweak Weak = { 0 };
	xrtobjectgraphresult Result;

	testRequire(
		(pGraph != NULL) && (pFields != NULL) &&
		xrtWeakInit(&Weak, pFields) &&
		xrtObjectGraphTrack(pGraph, pFields) &&
		xrtDynamicFieldsSetNew(
			pFields,
			XRT_STR_LITERAL("self"),
			xrtValueRuntimeObject(pFields)
		),
		"dynamic field graph fixture failed"
	);
	xrtDynamicFieldsUnref(pFields);
	testRequire(
		xrtObjectGraphCollect(pGraph, &Result) &&
		(Result.CollectedCount == 1u) &&
		xrtWeakExpired(&Weak),
		"dynamic field self cycle was not collected"
	);
	xrtWeakUnit(&Weak);
	xrtObjectGraphDestroy(pGraph);
}



/* 验证字段写入不会把字段外仍共享的 Value 引用误判为图内引用。 */
static void testDynamicFieldExternalValueRoot(void)
{
	xrtobjectgraph* pGraph = xrtObjectGraphCreate();
	xrtdynamicfields* pFields = xrtDynamicFieldsCreate();
	xvalue* pValue;
	xvalue* pAlias;
	xrtweak Weak = { 0 };
	xrtobjectgraphresult Result;

	testRequire(
		(pGraph != NULL) && (pFields != NULL) &&
		xrtWeakInit(&Weak, pFields) &&
		xrtObjectGraphTrack(pGraph, pFields),
		"dynamic field external Value root fixture failed"
	);
	pValue = xrtValueRuntimeObject(pFields);
	pAlias = xrtValueRetain(pValue);
	testRequire(
		(pValue != NULL) && (pAlias != NULL) &&
		xrtDynamicFieldsSetTake(
			pFields, XRT_STR_LITERAL("self"), &pValue
		) && (pValue == NULL),
		"dynamic field external Value root insertion failed"
	);
	xrtDynamicFieldsUnref(pFields);
	testRequire(
		xrtObjectGraphCollect(pGraph, &Result) &&
		(Result.CollectedCount == 0u) && !xrtWeakExpired(&Weak),
		"dynamic field collection ignored an external Value root"
	);
	xrtValueRelease(pAlias);
	testRequire(
		xrtObjectGraphCollect(pGraph, &Result) &&
		(Result.CollectedCount == 1u) && xrtWeakExpired(&Weak),
		"dynamic field cycle survived after its external Value root was released"
	);
	xrtWeakUnit(&Weak);
	xrtObjectGraphDestroy(pGraph);
}



/* 运行动态字段常规与对象图测试。 */
int main(void)
{
	testDynamicFieldOperations();
	testDynamicFieldNestedIsolation();
	testDynamicFieldSharedValues();
	testDynamicFieldObjectGraph();
	testDynamicFieldExternalValueRoot();
	xrtClearError();
	printf("[PASS] runtime dynamic field\n");
	return 0;
}
