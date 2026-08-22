#include <stdio.h>

#include <xrt.h>



/* 展示拥有型 Header 的重复字段、Set 与原始块写出。 */
int main(void)
{
	xhttpheaders* pHeaders = xrtHttpHeadersParse(
		XRT_STR_LITERAL("Set-Cookie: a=1; Path=/\r\n"),
		NULL,
		NULL
	);
	const xhttpfield* pContentType;
	char Output[256];
	size_t iSize;

	if ( (pHeaders == NULL) || !xrtHttpHeadersAdd(
		pHeaders,
		XRT_STR_LITERAL("Set-Cookie"),
		XRT_STR_LITERAL("b=2; Path=/")
	) || !xrtHttpHeadersSet(
		pHeaders,
		XRT_STR_LITERAL("Content-Type"),
		XRT_STR_LITERAL("application/json")
	) || (xrtHttpHeadersGetUnique(
		pHeaders,
		XRT_STR_LITERAL("Content-Type"),
		&pContentType
	) != XHTTP_NEXT_ITEM) ||
		(pContentType->Value.Size !=
		 sizeof("application/json") - 1u) ||
		!xrtHttpHeadersWrite(
		pHeaders, Output, sizeof(Output), &iSize
	) ) {
		xrtHttpHeadersDestroy(pHeaders);
		return 1;
	}
	fwrite(Output, 1, iSize, stdout);
	xrtHttpHeadersDestroy(pHeaders);
	return 0;
}
