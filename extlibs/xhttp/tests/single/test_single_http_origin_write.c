#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头发布必须保留 Origin 规范写出。 */
int main(void)
{
	xhttporigin Origin;
	char sOutput[32];
	size_t iSize;

	return xrtHttpOriginParse(
		XRT_STR_LITERAL("HTTPS://Example.Test:443"), &Origin
	) && xrtHttpOriginWrite(
		&Origin, sOutput, sizeof(sOutput), &iSize
	) && (iSize == 20u) &&
		(memcmp(sOutput, "https://example.test", 20u) == 0) ? 0 : 1;
}
