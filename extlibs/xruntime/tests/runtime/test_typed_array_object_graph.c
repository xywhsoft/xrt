#include "../test.h"
#include "typed_array_object_graph_fixture.h"



/* 运行类型数组与对象图集成测试。 */
int main(void)
{
	testRequire(
		testTypedArrayObjectGraphFixture() == 0,
		"typed array object graph integration failed"
	);
	xrtClearError();
	printf("[PASS] typed array object graph\n");
	return 0;
}
