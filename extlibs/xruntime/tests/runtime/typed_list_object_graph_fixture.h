#ifndef XRT_TEST_TYPED_LIST_OBJECT_GRAPH_FIXTURE_H
#define XRT_TEST_TYPED_LIST_OBJECT_GRAPH_FIXTURE_H



/* 验证类型列表负载能够向对象图精确报告稀疏强引用槽位。 */
static int testTypedListObjectGraphFixture(void)
{
	const xrttype* arrArguments[1];
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.typed-list.SelfList")),
		.Kind = XRT_TYPE_LIST,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE |
			XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_RELOCATABLE,
		.Name = XRT_STR_INIT("SelfList"),
		.AbiName = XRT_STR_INIT("tests.typed-list.SelfList"),
		.Size = sizeof(ptr),
		.Align = sizeof(ptr),
		.InstanceSize = sizeof(xtypedlist),
		.InstanceAlign = sizeof(ptr),
		.Ops = xrtObjectValueOps(),
		.InstanceOps = xrtTypedListInstanceOps(),
		.ArgumentCount = 1u,
		.Arguments = arrArguments
	};
	xrtobjectgraphresult Result;
	xrtobjectgraph* pGraph;
	xrtobject* pObject;
	xtypedlist* pList;
	xrtweak Weak = { 0 };
	int iResult = 0;

	arrArguments[0] = &Type;
	if ( !xrtTypeValidate(&Type) || !xrtTypedListTypeValidate(&Type) ) {
		return 1;
	}
	pGraph = xrtObjectGraphCreate();
	pObject = xrtObjectCreate(&Type);
	if ( (pGraph == NULL) || (pObject == NULL) ) {
		xrtObjectUnref(pObject);
		xrtObjectGraphDestroy(pGraph);
		return 2;
	}
	pList = (xtypedlist*)xrtObjectData(pObject);
	if ( !xrtObjectGraphTrack(pGraph, pObject) ||
		 !xrtWeakInit(&Weak, pObject) ||
		 !xrtTypedListSet(pList, -7, &pObject) ||
		 !xrtTypedListSet(pList, 19, &pObject) ||
		 (xrtObjectRefCount(pObject) != 3u) ) {
		iResult = 3;
	}
	if ( iResult == 0 ) {
		xrtObjectUnref(pObject);
		pObject = NULL;
		if ( !xrtObjectGraphCollect(pGraph, &Result) ||
			 (Result.TrackedCount != 1u) ||
			 (Result.EdgeCount != 2u) ||
			 (Result.RootCount != 0u) ||
			 (Result.CollectedCount != 1u) ||
			 !xrtWeakExpired(&Weak) ||
			 (xrtObjectGraphCount(pGraph) != 0u) ) {
			iResult = 4;
		}
	}
	xrtObjectUnref(pObject);
	xrtWeakUnit(&Weak);
	xrtObjectGraphDestroy(pGraph);
	return iResult;
}

#endif
