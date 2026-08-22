#include "../test.h"
#include "typed_dict_object_graph_fixture.h"



/* 运行类型字典与对象图集成测试。 */
int main(void)
{
	testRequire(
		testTypedDictObjectGraphFixture() == 0,
		"typed dictionary object graph integration failed"
	);
	xrtClearError();
	printf("[PASS] typed dictionary object graph\n");
	return 0;
}
