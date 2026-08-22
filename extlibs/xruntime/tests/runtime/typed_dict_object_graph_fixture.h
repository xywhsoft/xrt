#ifndef XRT_TEST_TYPED_DICT_OBJECT_GRAPH_FIXTURE_H
#define XRT_TEST_TYPED_DICT_OBJECT_GRAPH_FIXTURE_H



/* 对象负载描述必须使用目标 ABI 的真实结构对齐。 */
#if defined(_MSC_VER)
	#define XRT_TEST_TYPED_DICT_ALIGNOF(Type) __alignof(Type)
#elif defined(__GNUC__) || defined(__clang__) || defined(__TINYC__)
	#define XRT_TEST_TYPED_DICT_ALIGNOF(Type) __alignof__(Type)
#elif defined(__cplusplus)
	#define XRT_TEST_TYPED_DICT_ALIGNOF(Type) alignof(Type)
#else
	#define XRT_TEST_TYPED_DICT_ALIGNOF(Type) _Alignof(Type)
#endif



/* 验证类型字典负载按独立键值槽向对象图报告强引用。 */
static int testTypedDictObjectGraphFixture(void)
{
	const xrttype* arrArguments[1];
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.typed-dict.SelfDict")),
		.Kind = XRT_TYPE_DICT,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE |
			XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_RELOCATABLE,
		.Name = XRT_STR_INIT("SelfDict"),
		.AbiName = XRT_STR_INIT("tests.typed-dict.SelfDict"),
		.Size = sizeof(ptr),
		.Align = sizeof(ptr),
		.InstanceSize = sizeof(xtypeddict),
		.InstanceAlign = XRT_TEST_TYPED_DICT_ALIGNOF(xtypeddict),
		.Ops = xrtObjectValueOps(),
		.InstanceOps = xrtTypedDictInstanceOps(),
		.ArgumentCount = 1u,
		.Arguments = arrArguments
	};
	xrtobjectgraphresult Result;
	xrtobjectgraph* pGraph;
	xrtobject* pObject;
	xtypeddict* pDict;
	xrtweak Weak = { 0 };
	int iResult = 0;

	arrArguments[0] = &Type;
	if ( !xrtTypeValidate(&Type) || !xrtTypedDictTypeValidate(&Type) ) {
		return 1;
	}
	pGraph = xrtObjectGraphCreate();
	pObject = xrtObjectCreate(&Type);
	if ( (pGraph == NULL) || (pObject == NULL) ) {
		xrtObjectUnref(pObject);
		xrtObjectGraphDestroy(pGraph);
		return 2;
	}
	pDict = (xtypeddict*)xrtObjectData(pObject);
	if ( !xrtObjectGraphTrack(pGraph, pObject) ||
		 !xrtWeakInit(&Weak, pObject) ||
		 !xrtTypedDictSet(
			pDict, XRT_STR_LITERAL("left"), &pObject
		) ||
		 !xrtTypedDictSet(
			pDict, XRT_STR_LITERAL("right"), &pObject
		) ||
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

#undef XRT_TEST_TYPED_DICT_ALIGNOF

#endif
