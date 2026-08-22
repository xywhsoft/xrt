#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件正文读取路径。 */
int main(void)
{
	xhttpbody* pBody = xrtHttpBodyCopy(
		(xbytesview){ (cbytes)"body", 4 }
	);
	xhttpbodyreader* pReader;
	char Output[4];
	size_t iSize = 0;
	bool bPass;

	if ( pBody == NULL ) {
		return 1;
	}
	pReader = xrtHttpBodyOpen(pBody);
	bPass = (pReader != NULL) &&
		(xrtHttpBodyRead(
			pReader, Output, sizeof(Output), &iSize
		) == XHTTP_BODY_DATA) &&
		(iSize == sizeof(Output)) &&
		(memcmp(Output, "body", sizeof(Output)) == 0);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	return bPass ? 0 : 1;
}
