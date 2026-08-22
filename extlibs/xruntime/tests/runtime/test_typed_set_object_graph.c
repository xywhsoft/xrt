#include "../test.h"
#include "typed_set_object_graph_fixture.h"



/* 运行类型集合与对象图集成测试。 */
int main(void)
{
	testRequire(
		testTypedSetObjectGraphFixture() == 0,
		"typed set object graph integration failed"
	);
	xrtClearError();
	printf("[PASS] typed set object graph\n");
	return 0;
}
