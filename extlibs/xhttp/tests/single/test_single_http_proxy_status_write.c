#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头发布必须保留 Proxy-Status 成员规范写出。 */
int main(void)
{
	xhttpstructureditemvalue Item;
	char arrValue[32];
	size_t iSize;

	memset(&Item, 0, sizeof(Item));
	Item.Bare.Type = XHTTP_STRUCTURED_TOKEN;
	Item.Bare.Data = XRT_STR_LITERAL("Proxy");
	return xrtHttpProxyStatusWrite(
		&Item, arrValue, sizeof(arrValue), &iSize
	) && (iSize == 5u) &&
		(memcmp(arrValue, "Proxy", 5u) == 0) ? 0 : 1;
}
