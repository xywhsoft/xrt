#include "../test.h"



/* 验证创建和两种插入在 OOM 后保持来源与字段表不变。 */
static void testDynamicFieldInsertOom(void)
{
	xrtdynamicfields* pFields;
	xvalue* pCopySource;
	xvalue* pMoveSource;
	bool bTriggered;

	testRequire(xrtMemDebugFailAfter(0),
		"dynamic field creation OOM injection failed");
	pFields = xrtDynamicFieldsCreate();
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	testRequire(
		(pFields == NULL) && bTriggered &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"dynamic field creation OOM was not reported"
	);
	xrtClearError();
	pFields = xrtDynamicFieldsCreate();
	pCopySource = xrtValueString(XRT_STR_LITERAL("copy"));
	pMoveSource = xrtValueString(XRT_STR_LITERAL("move"));
	testRequire(
		(pFields != NULL) && (pCopySource != NULL) &&
		(pMoveSource != NULL),
		"dynamic field insertion OOM fixture failed"
	);
	testRequire(xrtMemDebugFailAfter(0),
		"dynamic field copy OOM injection failed");
	testRequire(
		!xrtDynamicFieldsSet(
			pFields, XRT_STR_LITERAL("copy"), pCopySource
		),
		"dynamic field copy insertion survived OOM"
	);
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	testRequire(
		bTriggered && (xrtDynamicFieldsCount(pFields) == 0u) &&
		(xrtValueType(pCopySource) == XVALUE_STRING),
		"dynamic field copy insertion OOM changed visible state"
	);
	xrtClearError();
	testRequire(xrtMemDebugFailAfter(0),
		"dynamic field move OOM injection failed");
	testRequire(
		!xrtDynamicFieldsSetTake(
			pFields, XRT_STR_LITERAL("move"), &pMoveSource
		),
		"dynamic field moved insertion survived OOM"
	);
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	testRequire(
		bTriggered && (pMoveSource != NULL) &&
		(xrtDynamicFieldsCount(pFields) == 0u),
		"dynamic field moved insertion OOM consumed its source"
	);
	xrtValueRelease(pMoveSource);
	xrtValueRelease(pCopySource);
	xrtDynamicFieldsUnref(pFields);
}



/* 扫描克隆故障点并验证来源始终保持完整。 */
static void testDynamicFieldCloneOom(void)
{
	xrtdynamicfields* pFields = xrtDynamicFieldsCreate();
	xrtdynamicfields* pClone;
	xvalue* pNested = xrtValueArray();
	bool bFailure = false;
	bool bSuccess = false;
	bool bTriggered;

	testRequire(
		(pFields != NULL) && (pNested != NULL) &&
		xrtDynamicFieldsSetNew(
			pFields, XRT_STR_LITERAL("one"), xrtValueInt(1)
		),
		"dynamic field clone OOM fixture failed"
	);
	for ( size_t i = 0u; i < 128u; i++ ) {
		testRequire(
			xrtValueArrayAppendNew(pNested, xrtValueInt((int64)i)),
			"dynamic field clone OOM nested fixture failed"
		);
	}
	testRequire(
		xrtDynamicFieldsSetNew(
			pFields, XRT_STR_LITERAL("two"), pNested
		),
		"dynamic field clone OOM fixture failed"
	);
	for ( uint64 i = 0u; i < 4096u; i++ ) {
		xrtClearError();
		testRequire(xrtMemDebugFailAfter(i),
			"dynamic field clone OOM injection failed");
		pClone = xrtDynamicFieldsClone(pFields);
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( pClone == NULL ) {
			bFailure = true;
			testRequire(
				bTriggered &&
				(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
				(xrtDynamicFieldsCount(pFields) == 2u),
				"dynamic field clone OOM changed its source"
			);
		} else {
			bSuccess = true;
			testRequire(!bTriggered &&
				xrtDynamicFieldsCount(pClone) == 2u,
				"dynamic field clone after OOM scan mismatched");
			xrtDynamicFieldsUnref(pClone);
			break;
		}
	}
	testRequire(bFailure && bSuccess,
		"dynamic field clone OOM scan missed failure or success");
	xrtDynamicFieldsUnref(pFields);
}



/* 运行动态字段分配失败测试。 */
int main(void)
{
	testDynamicFieldInsertOom();
	testDynamicFieldCloneOom();
	xrtClearError();
	printf("[PASS] runtime dynamic field OOM\n");
	return 0;
}
