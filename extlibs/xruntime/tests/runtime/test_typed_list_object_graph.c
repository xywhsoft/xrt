#include "../test.h"
#include "typed_list_object_graph_fixture.h"



/* 运行类型列表与对象图集成测试。 */
int main(void)
{
	testRequire(
		testTypedListObjectGraphFixture() == 0,
		"typed list object graph integration failed"
	);
	xrtClearError();
	printf("[PASS] typed list object graph\n");
	return 0;
}
