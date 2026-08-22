#include "../test_allocator.h"

#include <xrt/http_proxy_status.h>



/* RFC 9532 别名验证、迭代和直接解码必须保持零分配。 */
int main(void)
{
	xhttpproxyaliascursor Cursor;
	xstrview Alias;
	char arrOutput[64];
	size_t iSize;

	testRequire(
		testInstallFailAllocator(),
		"proxy alias failure allocator install failed"
	);
	xrtHttpProxyAliasCursorInit(&Cursor);
	testRequire(
		xrtHttpProxyAliasesValid(XRT_STR_LITERAL(
			"comma%2Cname.example,dot%5C.label.example"
		)) && (xrtHttpProxyAliasNext(
			XRT_STR_LITERAL(
				"comma%2Cname.example,dot%5C.label.example"
			), &Cursor, &Alias
		) == XHTTP_NEXT_ITEM) &&
		xrtHttpProxyAliasRead(
			Alias, arrOutput, sizeof(arrOutput), &iSize
		),
		"proxy alias parser allocated"
	);
	printf("[PASS] http_proxy_alias_noalloc\n");
	return 0;
}
