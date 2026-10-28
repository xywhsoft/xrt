#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>



/* 验证单头文件保留完整 Accept-Encoding 协商。 */
int main(void)
{
	xhttpacceptencoding Accept;
	xhttpfield Field = {
		XRT_STR_LITERAL("Content-Encoding"),
		XRT_STR_LITERAL("gzip")
	};
	size_t iSize = 0;
	bool bPass;

	xrtHttpAcceptEncodingInit(&Accept);
	bPass = xrtHttpAcceptEncodingAdd(
		&Accept,
		XRT_STR_LITERAL("gzip;q=0.5, deflate;q=1")
	) && (xrtHttpAcceptEncodingSelect(
		&Accept,
		XHTTP_CODING_IDENTITY |
			XHTTP_CODING_GZIP |
			XHTTP_CODING_DEFLATE,
		XHTTP_CODING_GZIP
	) == XHTTP_CODING_DEFLATE);
	bPass = bPass && xrtHttpContentEncodingWrite(
		&Field, 1, NULL, 0, &iSize
	) && (iSize == 4);
	printf(
		"%s single-http-encoding\n",
		bPass ? "[PASS]" : "[FAIL]"
	);
	return bPass ? 0 : 1;
}
