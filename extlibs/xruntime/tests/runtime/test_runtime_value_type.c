#include "../test.h"



static int gRuntimeValueTypeDrops;



/* 记录运行时 Value 类型测试对象的最终释放。 */
static void testRuntimeValueTypeDrop(ptr pValue, const xrttype* pType)
{
	(void)pValue;
	(void)pType;
	gRuntimeValueTypeDrops++;
}



/* 统计 Value 所有权图报告的对象强引用边。 */
static bool testRuntimeValueTypeVisit(xrtobject* pObject, ptr pContext)
{
	size_t* pCount = (size_t*)pContext;

	testRequire(pObject != NULL, "runtime Value trace visited a null object");
	(*pCount)++;
	return true;
}



/* 验证 Value 类型描述的深复制、移动、释放和对象边去重。 */
int main(void)
{
	xrtinstanceops ObjectOps = { .Drop = testRuntimeValueTypeDrop };
	xrttype ObjectType = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.ValueTypeObject")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("ValueTypeObject"),
		.AbiName = XRT_STR_INIT("tests.runtime.ValueTypeObject"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(int64),
		.InstanceAlign = TEST_ALIGNOF(int64),
		.InstanceOps = &ObjectOps
	};
	const xrttype* pValueType = xrtTypeValue();
	xrtobject* pObject = xrtObjectCreate(&ObjectType);
	xvalue* pFirst = xrtValueRuntimeObject(pObject);
	xvalue* pSecond = xrtValueRuntimeObject(pObject);
	xvalue* pSource = xrtValueArray();
	xvalue* pCopy = xrtValueNull();
	xvalue* pMoved = xrtValueNull();
	xvalue* pEmpty = NULL;
	size_t iEdges = 0u;

	testRequire(
		xrtTypeValidate(pValueType) &&
		xrtTypeIsCopyable(pValueType) &&
		xrtTypeIsRelocatable(pValueType),
		"runtime Value type descriptor is invalid"
	);
	testRequire(
		(pObject != NULL) && (pFirst != NULL) && (pSecond != NULL) &&
		(pSource != NULL) &&
		xrtValueArrayAppend(pSource, pFirst) &&
		xrtValueArrayAppend(pSource, pFirst) &&
		xrtValueArrayAppend(pSource, pSecond),
		"runtime Value type graph fixture failed"
	);
	testRequire(
		xrtValueTraceRuntimeObjects(
			pSource, testRuntimeValueTypeVisit, &iEdges
		) && (iEdges == 2u),
		"runtime Value trace did not preserve actual handle ownership"
	);
	testRequire(
		xrtTypeCloneValue(pValueType, &pCopy, &pSource) &&
		(pCopy != pSource) && xrtValueEqual(pCopy, pSource),
		"runtime Value type deep clone mismatch"
	);
	iEdges = 0u;
	testRequire(
		xrtTypeTraceValue(
			pValueType, &pCopy, testRuntimeValueTypeVisit, &iEdges
		) && (iEdges == 2u),
		"runtime Value type trace mismatch"
	);
	iEdges = 0u;
	testRequire(
		xrtTypeTraceValue(
			pValueType, &pEmpty, testRuntimeValueTypeVisit, &iEdges
		) && (iEdges == 0u),
		"empty runtime Value slot was reported as a trace failure"
	);
	testRequire(
		xrtTypeMoveValue(pValueType, &pMoved, &pCopy) &&
		(pMoved != NULL) && (xrtValueType(pCopy) == XVALUE_NULL),
		"runtime Value type move mismatch"
	);

	xrtTypeDropValue(pValueType, &pMoved);
	xrtTypeDropValue(pValueType, &pCopy);
	xrtValueRelease(pSource);
	xrtValueRelease(pSecond);
	xrtValueRelease(pFirst);
	xrtObjectUnref(pObject);
	testRequire(gRuntimeValueTypeDrops == 1,
		"runtime Value type leaked or double-dropped its object");
	xrtClearError();
	printf("[PASS] runtime Value type\n");
	return 0;
}
