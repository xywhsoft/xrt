#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



/* 单头测试对象用一个 Value 槽形成自身强引用环。 */
typedef struct testsinglevalueroot {
	xvalue* Value;
} testsinglevalueroot;



/* 初始化单头测试对象的空 Value 槽。 */
static bool testSingleValueRootInit(ptr pValue, const xrttype* pType)
{
	testsinglevalueroot* pPayload = (testsinglevalueroot*)pValue;
	(void)pType;

	pPayload->Value = NULL;
	return true;
}



/* 释放单头测试对象拥有的 Value。 */
static void testSingleValueRootDrop(ptr pValue, const xrttype* pType)
{
	testsinglevalueroot* pPayload = (testsinglevalueroot*)pValue;
	(void)pType;

	xrtValueRelease(pPayload->Value);
	pPayload->Value = NULL;
}



/* 枚举单头测试对象 Value 图中的运行时对象引用。 */
static bool testSingleValueRootTrace(
	const void* pValue,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	const testsinglevalueroot* pPayload =
		(const testsinglevalueroot*)pValue;
	(void)pType;

	return (pPayload->Value == NULL) ||
		xrtValueTraceRuntimeObjects(pPayload->Value, pVisit, pContext);
}



/* 验证单头文件包含 Value 根收集的完整依赖闭包。 */
int main(void)
{
	static const xrtinstanceops Ops = {
		.Init = testSingleValueRootInit,
		.Drop = testSingleValueRootDrop,
		.Trace = testSingleValueRootTrace
	};
	xrttype Type = {
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("SingleValueRoot"),
		.AbiName = XRT_STR_INIT("tests.single.ValueRoot"),
		.Size = sizeof(ptr),
		.Align = sizeof(ptr),
		.InstanceSize = sizeof(testsinglevalueroot),
		.InstanceAlign = sizeof(ptr),
		.InstanceOps = &Ops
	};
	xrtobjectgraph* pGraph = NULL;
	xrtobject* pObject = NULL;
	testsinglevalueroot* pPayload;
	xvalue* pRoot = NULL;
	xrtobjectgraphresult Result;
	int iResult = 0;

	Type.Id = xrtTypeId(Type.AbiName);
	pGraph = xrtObjectGraphCreate();
	pObject = xrtObjectCreate(&Type);
	if ( (pGraph == NULL) || (pObject == NULL) ||
		 !xrtObjectGraphTrack(pGraph, pObject) ) {
		iResult = 1;
		goto Cleanup;
	}
	pPayload = (testsinglevalueroot*)xrtObjectData(pObject);
	pPayload->Value = xrtValueRuntimeObject(pObject);
	pRoot = xrtValueRetain(pPayload->Value);
	xrtObjectUnref(pObject);
	pObject = NULL;
	if ( (pRoot == NULL) ||
		 !xrtObjectGraphCollectValueRoot(pGraph, pRoot, &Result) ||
		 (Result.CollectedCount != 0u) ) {
		iResult = 2;
		goto Cleanup;
	}
	xrtValueRelease(pRoot);
	pRoot = NULL;
	if ( !xrtObjectGraphCollect(pGraph, &Result) ||
		 (Result.CollectedCount != 1u) ) {
		iResult = 3;
	}

Cleanup:
	xrtValueRelease(pRoot);
	xrtObjectUnref(pObject);
	xrtObjectGraphDestroy(pGraph);
	return iResult;
}
