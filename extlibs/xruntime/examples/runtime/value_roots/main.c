#include <stdio.h>
#include <xruntime.h>



/* 示例对象通过 Value 槽持有语言对象引用。 */
typedef struct examplevalueroot {
	xvalue* Value;
} examplevalueroot;



/* 初始化示例对象的空 Value 槽。 */
static bool exampleValueRootInit(ptr pValue, const xrttype* pType)
{
	examplevalueroot* pPayload = (examplevalueroot*)pValue;
	(void)pType;

	pPayload->Value = NULL;
	return true;
}



/* 释放示例对象拥有的 Value 图。 */
static void exampleValueRootDrop(ptr pValue, const xrttype* pType)
{
	examplevalueroot* pPayload = (examplevalueroot*)pValue;
	(void)pType;

	xrtValueRelease(pPayload->Value);
	pPayload->Value = NULL;
}



/* 让对象图看见示例对象 Value 图中的强引用。 */
static bool exampleValueRootTrace(
	const void* pValue,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	const examplevalueroot* pPayload = (const examplevalueroot*)pValue;
	(void)pType;

	return (pPayload->Value == NULL) ||
		xrtValueTraceRuntimeObjects(pPayload->Value, pVisit, pContext);
}



/* 在安全点把宿主 Value 变量作为根传给对象图收集器。 */
int main(void)
{
	static const xrtinstanceops Ops = {
		.Init = exampleValueRootInit,
		.Drop = exampleValueRootDrop,
		.Trace = exampleValueRootTrace
	};
	xrttype Type = {
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("ValueRootExample"),
		.AbiName = XRT_STR_INIT("example.runtime.ValueRoot"),
		.Size = sizeof(ptr),
		.Align = sizeof(ptr),
		.InstanceSize = sizeof(examplevalueroot),
		.InstanceAlign = sizeof(ptr),
		.InstanceOps = &Ops
	};
	xrtobjectgraph* pGraph = xrtObjectGraphCreate();
	xrtobject* pObject;
	examplevalueroot* pPayload;
	xvalue* pStackValue;
	xrtobjectgraphresult Result;
	int iResult = 1;

	Type.Id = xrtTypeId(Type.AbiName);
	pObject = xrtObjectCreate(&Type);
	if ( (pGraph == NULL) || (pObject == NULL) ||
		 !xrtObjectGraphTrack(pGraph, pObject) ) {
		xrtObjectUnref(pObject);
		xrtObjectGraphDestroy(pGraph);
		return 1;
	}
	pPayload = (examplevalueroot*)xrtObjectData(pObject);
	pPayload->Value = xrtValueRuntimeObject(pObject);
	pStackValue = xrtValueRetain(pPayload->Value);
	xrtObjectUnref(pObject);
	if ( (pStackValue != NULL) &&
		 xrtObjectGraphCollectValueRoot(pGraph, pStackValue, &Result) ) {
		printf("roots=%zu collected=%zu\n",
			Result.RootCount, Result.CollectedCount);
		iResult = (Result.RootCount == 1u) &&
			(Result.CollectedCount == 0u) ? 0 : 1;
	}
	xrtValueRelease(pStackValue);
	(void)xrtObjectGraphCollect(pGraph, NULL);
	xrtObjectGraphDestroy(pGraph);
	return iResult;
}
