#include "../test.h"
#include "../test_thread.h"



#define TEST_GRAPH_THREAD_COUNT 6u
#define TEST_GRAPH_THREAD_OBJECTS 192u
#define TEST_GRAPH_THREAD_ROUNDS 40u



typedef struct testgraphthreadcontext {
	xrtobjectgraph* Graph;
	xrtobject** Objects;
	size_t ThreadIndex;
	bool Remove;
} testgraphthreadcontext;



/* 并发执行幂等跟踪，或分片摘除互不相同的对象。 */
static int testGraphThreadRun(ptr pData)
{
	testgraphthreadcontext* pContext = (testgraphthreadcontext*)pData;

	if ( !pContext->Remove ) {
		for ( size_t iRound = 0; iRound < TEST_GRAPH_THREAD_ROUNDS; iRound++ ) {
			for ( size_t i = 0; i < TEST_GRAPH_THREAD_OBJECTS; i++ ) {
				if ( !xrtObjectGraphTrack(
						pContext->Graph, pContext->Objects[i]
					) ) {
					return 1;
				}
			}
		}
		return 0;
	}
	for ( size_t i = pContext->ThreadIndex;
		  i < TEST_GRAPH_THREAD_OBJECTS;
		  i += TEST_GRAPH_THREAD_COUNT ) {
		if ( !xrtObjectGraphUntrack(
				pContext->Graph, pContext->Objects[i]
			) ) {
			return 2;
		}
	}
	return 0;
}



/* 启动一次并发跟踪或摘除阶段并检查全部线程。 */
static void testGraphThreadStage(
	xrtobjectgraph* pGraph,
	xrtobject** pObjects,
	bool bRemove
)
{
	testgraphthreadcontext arrContext[TEST_GRAPH_THREAD_COUNT];
	testthread arrThread[TEST_GRAPH_THREAD_COUNT];

	for ( size_t i = 0; i < TEST_GRAPH_THREAD_COUNT; i++ ) {
		arrContext[i].Graph = pGraph;
		arrContext[i].Objects = pObjects;
		arrContext[i].ThreadIndex = i;
		arrContext[i].Remove = bRemove;
		arrThread[i].Proc = testGraphThreadRun;
		arrThread[i].Data = &arrContext[i];
	}
	testThreadsStart(arrThread, TEST_GRAPH_THREAD_COUNT);
	testThreadsJoin(arrThread, TEST_GRAPH_THREAD_COUNT);
	for ( size_t i = 0; i < TEST_GRAPH_THREAD_COUNT; i++ ) {
		testRequire(arrThread[i].Result == 0,
			"object graph concurrent membership operation failed");
	}
}



/* 验证同一图的并发幂等跟踪和分片摘除保持成员表完整。 */
int main(void)
{
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.GraphThread")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("GraphThread"),
		.AbiName = XRT_STR_INIT("tests.runtime.GraphThread"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(int64),
		.InstanceAlign = TEST_ALIGNOF(int64)
	};
	xrtobject* arrObject[TEST_GRAPH_THREAD_OBJECTS];
	xrtobjectgraph* pGraph = xrtObjectGraphCreate();

	testRequire(pGraph != NULL, "threaded object graph create failed");
	for ( size_t i = 0; i < TEST_GRAPH_THREAD_OBJECTS; i++ ) {
		arrObject[i] = xrtObjectCreate(&Type);
		testRequire(arrObject[i] != NULL,
			"threaded object graph fixture failed");
	}
	testGraphThreadStage(pGraph, arrObject, false);
	testRequire(
		xrtObjectGraphCount(pGraph) == TEST_GRAPH_THREAD_OBJECTS,
		"concurrent idempotent graph track count mismatch"
	);
	testGraphThreadStage(pGraph, arrObject, true);
	testRequire(xrtObjectGraphCount(pGraph) == 0u,
		"concurrent graph untrack count mismatch");
	for ( size_t i = 0; i < TEST_GRAPH_THREAD_OBJECTS; i++ ) {
		xrtObjectUnref(arrObject[i]);
	}
	xrtObjectGraphDestroy(pGraph);
	xrtClearError();
	printf("[PASS] runtime object graph threads\n");
	return 0;
}
