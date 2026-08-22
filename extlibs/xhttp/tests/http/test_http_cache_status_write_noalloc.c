#include "../test_allocator.h"

#include <xrt/http_cache_status.h>



/* Cache-Status 成员写出必须保持零堆分配。 */
int main(void)
{
	xhttpstructuredparameterentry Parameter;
	xhttpstructureditemvalue Item;
	char arrValue[64];
	size_t iSize;

	memset(&Parameter, 0, sizeof(Parameter));
	Parameter.Key = XRT_STR_LITERAL("hit");
	Parameter.Value.Type = XHTTP_STRUCTURED_BOOLEAN;
	Parameter.Value.Number = 1;
	memset(&Item, 0, sizeof(Item));
	Item.Bare.Type = XHTTP_STRUCTURED_TOKEN;
	Item.Bare.Data = XRT_STR_LITERAL("Cache");
	Item.Parameters = &Parameter;
	Item.ParameterCount = 1u;
	testRequire(
		testInstallFailAllocator(),
		"Cache-Status writer failure allocator install failed"
	);
	testRequire(
		xrtHttpCacheStatusWrite(
			&Item, arrValue, sizeof(arrValue), &iSize
		) && (iSize == 9u),
		"Cache-Status writer allocated"
	);
	printf("[PASS] http_cache_status_write_noalloc\n");
	return 0;
}
