#define XRT_MODULE_IO_BUFFER
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件 Buffer 适配器必须拉起 IO 与 Buffer 的精确闭包。 */
int main(void)
{
	xbuffer Buffer;
	xwriter* pWriter;

	#if !defined(XRT_FEATURE_IO_BUFFER) || \
		!defined(XRT_FEATURE_IO) || !defined(XRT_FEATURE_BUFFER)
		#error "XRT_MODULE_IO_BUFFER dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_IO_FILE)
		#error "XRT_MODULE_IO_BUFFER pulled file adapters"
	#endif

	if ( !xrtBufferInit(&Buffer) ) {
		return 1;
	}
	pWriter = xrtWriterFromBuffer(&Buffer);
	if ( (pWriter == NULL) ||
		 !xrtWriterWriteFull(pWriter, "abc", 3u, NULL) ||
		 (Buffer.Size != 3u) ||
		 (memcmp(Buffer.Data, "abc", 3u) != 0) ) {
		xrtWriterDestroy(pWriter);
		xrtBufferUnit(&Buffer);
		return 2;
	}
	xrtWriterDestroy(pWriter);
	xrtBufferUnit(&Buffer);
	return 0;
}
