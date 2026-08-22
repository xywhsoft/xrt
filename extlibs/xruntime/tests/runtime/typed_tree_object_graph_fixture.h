#ifndef XRT_TEST_TYPED_TREE_OBJECT_GRAPH_FIXTURE_H
#define XRT_TEST_TYPED_TREE_OBJECT_GRAPH_FIXTURE_H



/* 对象负载描述必须使用目标 ABI 的真实结构对齐。 */
#if defined(_MSC_VER)
	#define XRT_TEST_TYPED_TREE_ALIGNOF(Type) __alignof(Type)
#elif defined(__GNUC__) || defined(__clang__) || defined(__TINYC__)
	#define XRT_TEST_TYPED_TREE_ALIGNOF(Type) __alignof__(Type)
#elif defined(__cplusplus)
	#define XRT_TEST_TYPED_TREE_ALIGNOF(Type) alignof(Type)
#else
	#define XRT_TEST_TYPED_TREE_ALIGNOF(Type) _Alignof(Type)
#endif



/* 验证类型树按独立键和值槽向对象图报告强引用。 */
static int testTypedTreeObjectGraphFixture(void)
{
	const xrttype* arrArguments[2];
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.typed-tree.SelfTree")),
		.Kind = XRT_TYPE_DICT,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE |
			XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_RELOCATABLE,
		.Name = XRT_STR_INIT("SelfTree"),
		.AbiName = XRT_STR_INIT("tests.typed-tree.SelfTree"),
		.Size = sizeof(ptr),
		.Align = sizeof(ptr),
		.InstanceSize = sizeof(xtypedtree),
		.InstanceAlign = XRT_TEST_TYPED_TREE_ALIGNOF(xtypedtree),
		.Ops = xrtObjectValueOps(),
		.InstanceOps = xrtTypedTreeInstanceOps(),
		.ArgumentCount = 2u,
		.Arguments = arrArguments
	};
	xrtobjectgraphresult Result;
	xrtobjectgraph* pGraph;
	xrtobject* pObject;
	xtypedtree* pTree;
	xrtweak Weak = { 0 };
	int iResult = 0;

	arrArguments[0] = &Type;
	arrArguments[1] = &Type;
	if ( !xrtTypeValidate(&Type) || !xrtTypedTreeTypeValidate(&Type) ) {
		return 1;
	}
	pGraph = xrtObjectGraphCreate();
	pObject = xrtObjectCreate(&Type);
	if ( (pGraph == NULL) || (pObject == NULL) ) {
		xrtObjectUnref(pObject);
		xrtObjectGraphDestroy(pGraph);
		return 2;
	}
	pTree = (xtypedtree*)xrtObjectData(pObject);
	if ( !xrtObjectGraphTrack(pGraph, pObject) ||
		 !xrtWeakInit(&Weak, pObject) ||
		 !xrtTypedTreeSet(pTree, &pObject, &pObject) ||
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

#undef XRT_TEST_TYPED_TREE_ALIGNOF

#endif
