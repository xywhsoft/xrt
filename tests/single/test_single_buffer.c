#define XRT_MODULE_BUFFER
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件只选择 buffer 根模块时必须自动带入数组依赖。 */
int main(void)
{
	xbuffer tBuffer;
	const unsigned char pExpected[] = {
		'a', 'b', 'c', 0, 0, 'z'
	};

	#if !defined(XRT_FEATURE_BUFFER) || !defined(XRT_FEATURE_ARRAY)
		#error "XRT_MODULE_BUFFER did not enable its dependency closure"
	#endif

	if ( !xrtBufferInit(&tBuffer) ) {
		return 1;
	}
	if ( !xrtBufferAppend(&tBuffer, XRT_BYTES_LITERAL("abc")) ||
		 !xrtBufferWrite(&tBuffer, 5, XRT_BYTES_LITERAL("z")) ) {
		xrtBufferUnit(&tBuffer);
		return 2;
	}
	if ( (tBuffer.Size != sizeof(pExpected)) ||
		 (memcmp(tBuffer.Data, pExpected, sizeof(pExpected)) != 0) ) {
		xrtBufferUnit(&tBuffer);
		return 3;
	}
	xrtBufferUnit(&tBuffer);
	return 0;
}
