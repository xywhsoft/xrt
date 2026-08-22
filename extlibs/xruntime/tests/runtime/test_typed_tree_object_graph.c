#include "../test.h"
#include "typed_tree_object_graph_fixture.h"



/* 运行类型树与对象图集成测试。 */
int main(void)
{
	testRequire(
		testTypedTreeObjectGraphFixture() == 0,
		"typed tree object graph integration failed"
	);
	xrtClearError();
	printf("[PASS] typed tree object graph\n");
	return 0;
}
