#define XRT_MODULE_BUFFER_HEX
#define XRT_MODULE_BUFFER_BASE64
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供两个可选编码缓冲构造器。 */
int main(void)
{
	xbuffer* pHex;
	xbuffer* pBase64;

	pHex = xrtBufferFromHex(XRT_STR_LITERAL("000102ff"), 0);
	if ( (pHex == NULL) || (pHex->Size != 4) ||
		 (pHex->Data[3] != UINT8_C(0xff)) ) {
		xrtBufferDestroy(pHex);
		return 1;
	}
	xrtBufferDestroy(pHex);

	pBase64 = xrtBufferFromBase64(XRT_STR_LITERAL("AAEC/w=="), NULL);
	if ( (pBase64 == NULL) || (pBase64->Size != 4) ||
		 (pBase64->Data[3] != UINT8_C(0xff)) ) {
		xrtBufferDestroy(pBase64);
		return 2;
	}
	xrtBufferDestroy(pBase64);
	return 0;
}
