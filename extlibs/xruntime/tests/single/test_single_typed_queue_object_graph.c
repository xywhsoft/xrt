#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"

#include "../runtime/typed_queue_object_graph_fixture.h"



/* 验证单头文件中的三种类型队列对象图追踪。 */
int main(void)
{
	return testTypedQueueObjectGraphFixture();
}
