#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留拥有型 Header 的增删改查和直接写出。 */
int main(void)
{
	xhttpheaders* pHeaders = xrtHttpHeadersParse(
		XRT_STR_LITERAL("Host: example.test"), NULL, NULL
	);
	const xhttpfield* pHost;
	char Output[64];
	str sBuilt = NULL;
	size_t iSize;
	int iResult = 0;

	if ( (pHeaders == NULL) ||
		(xrtHttpHeadersGetUnique(
			pHeaders,
			XRT_STR_LITERAL("host"),
			&pHost
		) != XHTTP_NEXT_ITEM) ||
		(pHost->Value.Size != sizeof("example.test") - 1u) ||
		(xrtHttpHeadersGetAll(
			pHeaders, XRT_STR_LITERAL("host"), NULL, 0
		) != 1) || !xrtHttpHeadersWrite(
		pHeaders, Output, sizeof(Output), &iSize
	) || (iSize != 22) ||
		(memcmp(Output, "Host: example.test\r\n\r\n", iSize) != 0) ) {
		iResult = 1;
	}
	sBuilt = xrtHttpHeadersBuild(pHeaders, &iSize);
	if ( (sBuilt == NULL) || (iSize != 22) ||
		(memcmp(sBuilt, "Host: example.test\r\n\r\n", iSize + 1u) != 0) ) {
		iResult = 1;
	}
	xrtFree(sBuilt);
	xrtHttpHeadersDestroy(pHeaders);
	return iResult;
}
