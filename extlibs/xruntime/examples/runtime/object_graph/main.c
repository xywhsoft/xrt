#include <stdio.h>
#include <xruntime.h>



typedef struct node {
	xrtobject* Next;
} node;



/* 释放节点拥有的后继引用。 */
static void nodeDrop(ptr pValue, const xrttype* pType)
{
	node* pNode = (node*)pValue;
	(void)pType;

	xrtObjectUnref(pNode->Next);
	pNode->Next = NULL;
}



/* 枚举节点直接拥有的后继引用。 */
static bool nodeTrace(
	const void* pValue,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	const node* pNode = (const node*)pValue;
	(void)pType;

	return (pNode->Next == NULL) || pVisit(pNode->Next, pContext);
}



/* 创建一个自引用对象，并在安全点回收它。 */
int main(void)
{
	static const xrtinstanceops Ops = {
		.Drop = nodeDrop,
		.Trace = nodeTrace
	};
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("example.Node")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("Node"),
		.AbiName = XRT_STR_INIT("example.Node"),
		.Size = sizeof(ptr),
		.Align = sizeof(ptr),
		.InstanceSize = sizeof(node),
		.InstanceAlign = sizeof(ptr),
		.InstanceOps = &Ops
	};
	xrtobjectgraphresult Result;
	xrtobjectgraph* pGraph = xrtObjectGraphCreate();
	xrtobject* pNode = xrtObjectCreate(&Type);

	if ( (pGraph == NULL) || (pNode == NULL) ||
		 !xrtObjectGraphTrack(pGraph, pNode) ) {
		xrtObjectUnref(pNode);
		xrtObjectGraphDestroy(pGraph);
		return 1;
	}
	((node*)xrtObjectData(pNode))->Next = xrtObjectRef(pNode);
	xrtObjectUnref(pNode);
	if ( !xrtObjectGraphCollect(pGraph, &Result) ) {
		xrtObjectGraphDestroy(pGraph);
		return 2;
	}
	printf("tracked=%zu edges=%zu collected=%zu\n",
		Result.TrackedCount, Result.EdgeCount, Result.CollectedCount);
	xrtObjectGraphDestroy(pGraph);
	return 0;
}
