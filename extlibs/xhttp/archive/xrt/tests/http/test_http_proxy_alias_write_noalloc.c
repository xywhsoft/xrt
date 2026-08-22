#include "../test_allocator.h"

#include <xrt/http_proxy_status.h>



/* RFC 9532 别名直接写出和长度查询必须保持零分配。 */
int main(void)
{
	static const xstrview Aliases[] = {
		XRT_STR_INIT("comma,name.example"),
		XRT_STR_INIT("dot\\.label.example")
	};
	char arrOutput[96];
	size_t iSize;

	testRequire(
		testInstallFailAllocator(),
		"proxy alias writer failure allocator install failed"
	);
	testRequire(
		xrtHttpProxyAliasesWrite(
			Aliases, 2u, NULL, 0, &iSize
		) && xrtHttpProxyAliasesWrite(
			Aliases, 2u, arrOutput,
			sizeof(arrOutput), &iSize
		),
		"proxy alias writer allocated"
	);
	printf("[PASS] http_proxy_alias_write_noalloc\n");
	return 0;
}
