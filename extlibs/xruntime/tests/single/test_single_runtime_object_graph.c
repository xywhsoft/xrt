#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



typedef struct singlering {
	xrtobject* Next;
} singlering;



/* 释放单头测试对象持有的后继。 */
static void singleRingDrop(ptr pValue, const xrttype* pType)
{
	singlering* pRing = (singlering*)pValue;
	(void)pType;

	xrtObjectUnref(pRing->Next);
	pRing->Next = NULL;
}



/* 枚举单头测试对象持有的后继。 */
static bool singleRingTrace(
	const void* pValue,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	const singlering* pRing = (const singlering*)pValue;
	(void)pType;

	return (pRing->Next == NULL) || pVisit(pRing->Next, pContext);
}



/* 验证单头文件中的对象图循环回收路径。 */
int main(void)
{
	static const xrtinstanceops Ops = {
		.Drop = singleRingDrop,
		.Trace = singleRingTrace
	};
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("single.ObjectGraph")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("ObjectGraph"),
		.AbiName = XRT_STR_INIT("single.ObjectGraph"),
		.Size = sizeof(ptr),
		.Align = sizeof(ptr),
		.InstanceSize = sizeof(singlering),
		.InstanceAlign = sizeof(ptr),
		.InstanceOps = &Ops
	};
	xrtobjectgraphresult Result;
	xrtobjectgraph* pGraph = xrtObjectGraphCreate();
	xrtobject* pObject = xrtObjectCreate(&Type);
	singlering* pRing;

	if ( (pGraph == NULL) || (pObject == NULL) ||
		 !xrtObjectGraphTrack(pGraph, pObject) ) {
		xrtObjectUnref(pObject);
		xrtObjectGraphDestroy(pGraph);
		return 1;
	}
	pRing = (singlering*)xrtObjectData(pObject);
	pRing->Next = xrtObjectRef(pObject);
	xrtObjectUnref(pObject);
	if ( !xrtObjectGraphCollect(pGraph, &Result) ||
		 (Result.CollectedCount != 1u) ) {
		xrtObjectGraphDestroy(pGraph);
		return 2;
	}
	xrtObjectGraphDestroy(pGraph);
	return 0;
}
