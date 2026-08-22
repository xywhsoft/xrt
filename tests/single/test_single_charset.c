#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须提供显式端序转码。 */
int main(void)
{
	bytes pText;
	size_t iSize = 0;

	pText = xrtTranscode((xbytesview){ (cbytes)"A", 1 }, XENCODING_UTF8,
		XENCODING_UTF16_BE, XUTF_STRICT, false, &iSize);
	if ( (pText == NULL) || (iSize != 2) ||
		 (pText[0] != 0) || (pText[1] != 'A') ) {
		xrtFree(pText);
		return 1;
	}
	xrtFree(pText);
	return 0;
}
