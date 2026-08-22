#ifndef XRT_TEST_TYPED_ARRAY_OBJECT_GRAPH_FIXTURE_H
#define XRT_TEST_TYPED_ARRAY_OBJECT_GRAPH_FIXTURE_H



/* 验证类型数组负载能够向对象图精确报告重复强引用槽位。 */
static int testTypedArrayObjectGraphFixture(void)
{
	const xrttype* arrArguments[1];
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.typed-array.SelfArray")),
		.Kind = XRT_TYPE_ARRAY,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE |
			XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_RELOCATABLE,
		.Name = XRT_STR_INIT("SelfArray"),
		.AbiName = XRT_STR_INIT("tests.typed-array.SelfArray"),
		.Size = sizeof(ptr),
		.Align = sizeof(ptr),
		.InstanceSize = sizeof(xtypedarray),
		.InstanceAlign = sizeof(ptr),
		.Ops = xrtObjectValueOps(),
		.InstanceOps = xrtTypedArrayInstanceOps(),
		.ArgumentCount = 1u,
		.Arguments = arrArguments
	};
	xrtobjectgraphresult Result;
	xrtobjectgraph* pGraph;
	xrtobject* pObject;
	xtypedarray* pArray;
	xrtweak Weak = { 0 };
	int iResult = 0;

	arrArguments[0] = &Type;
	if ( !xrtTypeValidate(&Type) || !xrtTypedArrayTypeValidate(&Type) ) {
		return 1;
	}
	pGraph = xrtObjectGraphCreate();
	pObject = xrtObjectCreate(&Type);
	if ( (pGraph == NULL) || (pObject == NULL) ) {
		xrtObjectUnref(pObject);
		xrtObjectGraphDestroy(pGraph);
		return 2;
	}
	pArray = (xtypedarray*)xrtObjectData(pObject);
	if ( !xrtObjectGraphTrack(pGraph, pObject) ||
		 !xrtWeakInit(&Weak, pObject) ||
		 !xrtTypedArrayPush(pArray, &pObject) ||
		 !xrtTypedArrayPush(pArray, &pObject) ||
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
