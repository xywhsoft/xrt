#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"
#include "../runtime/typed_array_object_graph_fixture.h"



/* 验证单头文件中的类型数组对象图集成路径。 */
int main(void)
{
	return testTypedArrayObjectGraphFixture();
}
