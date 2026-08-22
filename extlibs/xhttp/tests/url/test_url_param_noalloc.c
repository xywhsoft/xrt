#include "../test_allocator.h"



/* HTTP 参数 URI-reference 验证必须保持零堆分配。 */
int main(void)
{
	xhttpparam Param;

	memset(&Param, 0, sizeof(Param));
	Param.Value = XRT_STR_LITERAL(
		"https:\\/\\/user@[2001:db8::1]:443\\/a?q=1#f"
	);
	Param.Flags = XHTTP_PARAM_HAS_VALUE | XHTTP_PARAM_QUOTED;
	testRequire(testInstallFailAllocator(),
		"URL parameter failure allocator install failed");
	testRequire(xrtUrlParamValid(&Param),
		"URL parameter validation allocated");
	printf("[PASS] url_param_noalloc\n");
	return 0;
}
