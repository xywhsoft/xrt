#ifndef XRT_TEST_TYPED_SET_OBJECT_GRAPH_FIXTURE_H
#define XRT_TEST_TYPED_SET_OBJECT_GRAPH_FIXTURE_H



/* 对象负载描述必须使用目标 ABI 的真实结构对齐。 */
#if defined(_MSC_VER)
	#define XRT_TEST_TYPED_SET_ALIGNOF(Type) __alignof(Type)
#elif defined(__GNUC__) || defined(__clang__) || defined(__TINYC__)
	#define XRT_TEST_TYPED_SET_ALIGNOF(Type) __alignof__(Type)
#elif defined(__cplusplus)
	#define XRT_TEST_TYPED_SET_ALIGNOF(Type) alignof(Type)
#else
	#define XRT_TEST_TYPED_SET_ALIGNOF(Type) _Alignof(Type)
#endif



/* 验证类型集合负载只向对象图报告唯一规范强引用槽位。 */
static int testTypedSetObjectGraphFixture(void)
{
	const xrttype* arrArguments[1];
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.typed-set.SelfSet")),
		.Kind = XRT_TYPE_SET,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE |
			XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_RELOCATABLE,
		.Name = XRT_STR_INIT("SelfSet"),
		.AbiName = XRT_STR_INIT("tests.typed-set.SelfSet"),
		.Size = sizeof(ptr),
		.Align = sizeof(ptr),
		.InstanceSize = sizeof(xtypedset),
		.InstanceAlign = XRT_TEST_TYPED_SET_ALIGNOF(xtypedset),
		.Ops = xrtObjectValueOps(),
		.InstanceOps = xrtTypedSetInstanceOps(),
		.ArgumentCount = 1u,
		.Arguments = arrArguments
	};
	xrtobjectgraphresult Result;
	xrtobjectgraph* pGraph;
	xrtobject* pObject;
	xtypedset* pSet;
	xrtweak Weak = { 0 };
	int iResult = 0;

	arrArguments[0] = &Type;
	if ( !xrtTypeValidate(&Type) || !xrtTypedSetTypeValidate(&Type) ) {
		return 1;
	}
	pGraph = xrtObjectGraphCreate();
	pObject = xrtObjectCreate(&Type);
	if ( (pGraph == NULL) || (pObject == NULL) ) {
		xrtObjectUnref(pObject);
		xrtObjectGraphDestroy(pGraph);
		return 2;
	}
	pSet = (xtypedset*)xrtObjectData(pObject);
	if ( !xrtObjectGraphTrack(pGraph, pObject) ||
		 !xrtWeakInit(&Weak, pObject) ||
		 !xrtTypedSetAdd(pSet, &pObject) ||
		 !xrtTypedSetAdd(pSet, &pObject) ||
		 (xrtTypedSetCount(pSet) != 1u) ||
		 (xrtObjectRefCount(pObject) != 2u) ) {
		iResult = 3;
	}
	if ( iResult == 0 ) {
		xrtObjectUnref(pObject);
		pObject = NULL;
		if ( !xrtObjectGraphCollect(pGraph, &Result) ||
			 (Result.TrackedCount != 1u) ||
			 (Result.EdgeCount != 1u) ||
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

#undef XRT_TEST_TYPED_SET_ALIGNOF

#endif
