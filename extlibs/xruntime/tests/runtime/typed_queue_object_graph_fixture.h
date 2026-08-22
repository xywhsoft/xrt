#ifndef XRT_TEST_TYPED_QUEUE_OBJECT_GRAPH_FIXTURE_H
#define XRT_TEST_TYPED_QUEUE_OBJECT_GRAPH_FIXTURE_H



/* 对象负载描述必须使用目标 ABI 的真实结构对齐。 */
#if defined(_MSC_VER)
	#define XRT_TEST_TYPED_QUEUE_ALIGNOF(Type) __alignof(Type)
#elif defined(__GNUC__) || defined(__clang__) || defined(__TINYC__)
	#define XRT_TEST_TYPED_QUEUE_ALIGNOF(Type) __alignof__(Type)
#elif defined(__cplusplus)
	#define XRT_TEST_TYPED_QUEUE_ALIGNOF(Type) alignof(Type)
#else
	#define XRT_TEST_TYPED_QUEUE_ALIGNOF(Type) _Alignof(Type)
#endif



/* 对象队列夹具使用的具体并发角色。 */
typedef enum testtypedqueueobjectkind {
	TEST_TYPED_QUEUE_OBJECT_SPSC = 1,
	TEST_TYPED_QUEUE_OBJECT_MPSC,
	TEST_TYPED_QUEUE_OBJECT_MPMC
} testtypedqueueobjectkind;



/* 按并发角色返回实例负载大小。 */
static size_t testTypedQueueObjectSize(testtypedqueueobjectkind Kind)
{
	switch ( Kind ) {
		case TEST_TYPED_QUEUE_OBJECT_SPSC:
			return sizeof(xtypedspscqueue);
		case TEST_TYPED_QUEUE_OBJECT_MPSC:
			return sizeof(xtypedmpscqueue);
		default:
			return sizeof(xtypedmpmcqueue);
	}
}



/* 按并发角色返回实例负载对齐。 */
static size_t testTypedQueueObjectAlign(testtypedqueueobjectkind Kind)
{
	switch ( Kind ) {
		case TEST_TYPED_QUEUE_OBJECT_SPSC:
			return XRT_TEST_TYPED_QUEUE_ALIGNOF(xtypedspscqueue);
		case TEST_TYPED_QUEUE_OBJECT_MPSC:
			return XRT_TEST_TYPED_QUEUE_ALIGNOF(xtypedmpscqueue);
		default:
			return XRT_TEST_TYPED_QUEUE_ALIGNOF(xtypedmpmcqueue);
	}
}



/* 按并发角色返回实例操作表。 */
static const xrtinstanceops* testTypedQueueObjectOps(
	testtypedqueueobjectkind Kind
)
{
	switch ( Kind ) {
		case TEST_TYPED_QUEUE_OBJECT_SPSC:
			return xrtTypedSPSCQueueInstanceOps();
		case TEST_TYPED_QUEUE_OBJECT_MPSC:
			return xrtTypedMPSCQueueInstanceOps();
		default:
			return xrtTypedMPMCQueueInstanceOps();
	}
}



/* 验证具体对象类型描述与并发角色匹配。 */
static bool testTypedQueueObjectTypeValidate(
	testtypedqueueobjectkind Kind,
	const xrttype* pType
)
{
	switch ( Kind ) {
		case TEST_TYPED_QUEUE_OBJECT_SPSC:
			return xrtTypedSPSCQueueTypeValidate(pType);
		case TEST_TYPED_QUEUE_OBJECT_MPSC:
			return xrtTypedMPSCQueueTypeValidate(pType);
		default:
			return xrtTypedMPMCQueueTypeValidate(pType);
	}
}



/* 把对象自身复制压入指定并发角色的类型队列。 */
static xqueueresult testTypedQueueObjectPush(
	testtypedqueueobjectkind Kind,
	ptr pQueue,
	xrtobject** pObject
)
{
	switch ( Kind ) {
		case TEST_TYPED_QUEUE_OBJECT_SPSC:
			return xrtTypedSPSCQueueTryPush(
				(xtypedspscqueue*)pQueue, pObject
			);
		case TEST_TYPED_QUEUE_OBJECT_MPSC:
			return xrtTypedMPSCQueueTryPush(
				(xtypedmpscqueue*)pQueue, pObject
			);
		default:
			return xrtTypedMPMCQueueTryPush(
				(xtypedmpmcqueue*)pQueue, pObject
			);
	}
}



/* 验证一种类型队列按每个已占用值槽向对象图报告强引用。 */
static int testTypedQueueObjectGraphOne(testtypedqueueobjectkind Kind)
{
	const xrttype* arrArguments[1];
	xtypedqueuemeta Meta = { 4u };
	xrttype Type = {
		.Kind = XRT_TYPE_LIST,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE |
			XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_RELOCATABLE,
		.Size = sizeof(ptr),
		.Align = XRT_TEST_TYPED_QUEUE_ALIGNOF(ptr),
		.Ops = xrtObjectValueOps(),
		.ArgumentCount = 1u,
		.Arguments = arrArguments,
		.Metadata = &Meta
	};
	xrtobjectgraphresult Result;
	xrtobjectgraph* pGraph;
	xrtobject* pObject;
	xrtweak Weak = { 0 };
	int iResult = 0;

	switch ( Kind ) {
		case TEST_TYPED_QUEUE_OBJECT_SPSC:
			Type.Id = xrtTypeId(XRT_STR_LITERAL("tests.typed-queue.SelfSPSC"));
			Type.Name = (xstrview)XRT_STR_INIT("SelfSPSC");
			Type.AbiName = (xstrview)XRT_STR_INIT("tests.typed-queue.SelfSPSC");
			break;
		case TEST_TYPED_QUEUE_OBJECT_MPSC:
			Type.Id = xrtTypeId(XRT_STR_LITERAL("tests.typed-queue.SelfMPSC"));
			Type.Name = (xstrview)XRT_STR_INIT("SelfMPSC");
			Type.AbiName = (xstrview)XRT_STR_INIT("tests.typed-queue.SelfMPSC");
			break;
		default:
			Type.Id = xrtTypeId(XRT_STR_LITERAL("tests.typed-queue.SelfMPMC"));
			Type.Name = (xstrview)XRT_STR_INIT("SelfMPMC");
			Type.AbiName = (xstrview)XRT_STR_INIT("tests.typed-queue.SelfMPMC");
			break;
	}
	Type.InstanceSize = testTypedQueueObjectSize(Kind);
	Type.InstanceAlign = testTypedQueueObjectAlign(Kind);
	Type.InstanceOps = testTypedQueueObjectOps(Kind);
	arrArguments[0] = &Type;
	if ( !xrtTypeValidate(&Type) ||
		 !testTypedQueueObjectTypeValidate(Kind, &Type) ) {
		return 1;
	}
	pGraph = xrtObjectGraphCreate();
	pObject = xrtObjectCreate(&Type);
	if ( (pGraph == NULL) || (pObject == NULL) ) {
		xrtObjectUnref(pObject);
		xrtObjectGraphDestroy(pGraph);
		return 2;
	}
	if ( !xrtObjectGraphTrack(pGraph, pObject) ||
		 !xrtWeakInit(&Weak, pObject) ||
		 (testTypedQueueObjectPush(
			Kind, xrtObjectData(pObject), &pObject
		 ) != XQUEUE_OK) ||
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



/* 依次验证 SPSC、MPSC 和 MPMC 队列对象的自引用收集。 */
static int testTypedQueueObjectGraphFixture(void)
{
	int iResult = testTypedQueueObjectGraphOne(
		TEST_TYPED_QUEUE_OBJECT_SPSC
	);

	if ( iResult == 0 ) {
		iResult = testTypedQueueObjectGraphOne(
			TEST_TYPED_QUEUE_OBJECT_MPSC
		);
	}
	if ( iResult == 0 ) {
		iResult = testTypedQueueObjectGraphOne(
			TEST_TYPED_QUEUE_OBJECT_MPMC
		);
	}
	return iResult;
}

#undef XRT_TEST_TYPED_QUEUE_ALIGNOF

#endif
