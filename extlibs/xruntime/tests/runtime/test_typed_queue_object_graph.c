#include "../test.h"
#include "typed_queue_object_graph_fixture.h"



/* 运行三种类型队列与对象图的集成测试。 */
int main(void)
{
	testRequire(
		testTypedQueueObjectGraphFixture() == 0,
		"typed queue object graph integration failed"
	);
	xrtClearError();
	printf("[PASS] typed queue object graph\n");
	return 0;
}
