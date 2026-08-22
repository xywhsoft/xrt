#include "../../../dev/bench/bench_common.h"

#define XRUNTIME_MODULE_RUNTIME_OBJECT_GRAPH
#include <xruntime.h>



/* 对象图基准节点只拥有下一节点的一个强引用。 */
typedef struct xbenchgraphnode {
	xrtobject* Next;
} xbenchgraphnode;



/* 初始化一个空图节点。 */
static bool xbenchGraphInit(ptr pValue, const xrttype* pType)
{
	xbenchgraphnode* pNode = (xbenchgraphnode*)pValue;

	(void)pType;
	pNode->Next = NULL;
	return true;
}



/* 释放图节点拥有的强引用。 */
static void xbenchGraphDrop(ptr pValue, const xrttype* pType)
{
	xbenchgraphnode* pNode = (xbenchgraphnode*)pValue;

	(void)pType;
	xrtObjectUnref(pNode->Next);
	pNode->Next = NULL;
}



/* 向收集器报告图节点拥有的唯一强引用。 */
static bool xbenchGraphTrace(
	const void* pValue,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	const xbenchgraphnode* pNode = (const xbenchgraphnode*)pValue;

	(void)pType;
	return (pNode->Next == NULL) || pVisit(pNode->Next, pContext);
}



/* 构造一次无外部根的强引用环并完整收集。 */
static bool xbenchGraphRound(
	xrtobjectgraph* pGraph,
	const xrttype* pType,
	xrtobject** pObjects,
	uint32 iCount
)
{
	xrtobjectgraphresult Result;

	for ( uint32 i = 0u; i < iCount; i++ ) {
		pObjects[i] = xrtObjectCreate(pType);
		if (
			(pObjects[i] == NULL) ||
			!xrtObjectGraphTrack(pGraph, pObjects[i])
		) {
			return false;
		}
	}
	for ( uint32 i = 0u; i < iCount; i++ ) {
		xbenchgraphnode* pNode = (xbenchgraphnode*)xrtObjectData(
			pObjects[i]
		);

		pNode->Next = xrtObjectRef(pObjects[(i + 1u) % iCount]);
		if ( pNode->Next == NULL ) {
			return false;
		}
	}
	for ( uint32 i = 0u; i < iCount; i++ ) {
		xrtObjectUnref(pObjects[i]);
		pObjects[i] = NULL;
	}
	return
		xrtObjectGraphCollect(pGraph, &Result) &&
		(Result.TrackedCount == iCount) &&
		(Result.CollectedCount == iCount) &&
		(xrtObjectGraphCount(pGraph) == 0u);
}



/* 测量对象创建、跟踪、强引用环识别和批量终结的完整路径。 */
int main(int argc, char** argv)
{
	uint32 iRounds = xbenchArgU32(argc, argv, 1, 200u);
	uint32 iNodeCount = xbenchArgU32(argc, argv, 2, 1000u);
	static const xrtinstanceops Ops = {
		.Init = xbenchGraphInit,
		.Drop = xbenchGraphDrop,
		.Trace = xbenchGraphTrace
	};
	xrttype Type = {
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("benchmark_graph_node"),
		.AbiName = XRT_STR_INIT("xrt.benchmark.graph-node"),
		.Size = sizeof(ptr),
		.Align = sizeof(ptr),
		.InstanceSize = sizeof(xbenchgraphnode),
		.InstanceAlign = sizeof(ptr),
		.InstanceOps = &Ops
	};
	xrtobjectgraph* pGraph;
	xrtobject** pObjects;
	xbenchtimer Timer;
	uint64 iElapsed;

	if ( (iRounds == 0u) || (iNodeCount < 2u) ) {
		fprintf(stderr, "benchmark requires rounds and at least two nodes.\n");
		return 1;
	}
	Type.Id = xrtTypeId(Type.AbiName);
	pGraph = xrtObjectGraphCreate();
	pObjects = (xrtobject**)xrtMalloc(
		(size_t)iNodeCount * sizeof(*pObjects)
	);
	if ( (pGraph == NULL) || (pObjects == NULL) ) {
		xrtFree(pObjects);
		xrtObjectGraphDestroy(pGraph);
		return 2;
	}

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0u; i < iRounds; i++ ) {
		if ( !xbenchGraphRound(pGraph, &Type, pObjects, iNodeCount) ) {
			return 3;
		}
	}
	xbenchTimerStop(&Timer);
	iElapsed = xbenchTimerElapsedNs(&Timer);

	printf("xrt runtime object graph benchmark\n");
	xbenchPrintMetricDouble(
		"runtime_graph_objects_per_sec",
		xbenchSafeRate((uint64)iRounds * iNodeCount, iElapsed)
	);
	xbenchPrintMetricU64(
		"checksum",
		(uint64)iRounds * iNodeCount + xrtObjectGraphCount(pGraph)
	);

	xrtFree(pObjects);
	xrtObjectGraphDestroy(pGraph);
	return 0;
}
