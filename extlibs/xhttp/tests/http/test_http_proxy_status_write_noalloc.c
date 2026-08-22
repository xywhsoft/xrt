#include "../test_allocator.h"

#include <xrt/http_proxy_status.h>



/* Proxy-Status 成员写出必须保持零堆分配。 */
int main(void)
{
	xhttpstructuredparameterentry Parameter;
	xhttpstructureditemvalue Item;
	char arrValue[64];
	size_t iSize;

	memset(&Parameter, 0, sizeof(Parameter));
	Parameter.Key = XRT_STR_LITERAL("error");
	Parameter.Value.Type = XHTTP_STRUCTURED_TOKEN;
	Parameter.Value.Data = XRT_STR_LITERAL("dns_timeout");
	memset(&Item, 0, sizeof(Item));
	Item.Bare.Type = XHTTP_STRUCTURED_TOKEN;
	Item.Bare.Data = XRT_STR_LITERAL("Proxy");
	Item.Parameters = &Parameter;
	Item.ParameterCount = 1u;
	testRequire(
		testInstallFailAllocator(),
		"Proxy-Status writer failure allocator install failed"
	);
	testRequire(
		xrtHttpProxyStatusWrite(
			&Item, arrValue, sizeof(arrValue), &iSize
		) && (iSize == 23u),
		"Proxy-Status writer allocated"
	);
	printf("[PASS] http_proxy_status_write_noalloc\n");
	return 0;
}
