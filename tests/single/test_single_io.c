#define XRT_MODULE_IO
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件只选择 IO 时必须提供内存适配器且不拉入 Buffer 或文件。 */
int main(void)
{
	unsigned char arrOutput[4] = { 0 };
	xreader* pReader;
	xwriter* pWriter;
	uint64 iCopied = 0;

	#if !defined(XRT_FEATURE_IO)
		#error "XRT_MODULE_IO did not enable IO"
	#endif
	#if defined(XRT_FEATURE_IO_BUFFER) || defined(XRT_FEATURE_IO_FILE)
		#error "XRT_MODULE_IO pulled optional adapters"
	#endif

	pReader = xrtReaderFromMemory(XRT_BYTES_LITERAL("data"));
	pWriter = xrtWriterFromMemory(arrOutput, sizeof(arrOutput));
	if ( (pReader == NULL) || (pWriter == NULL) ) {
		xrtReaderDestroy(pReader);
		xrtWriterDestroy(pWriter);
		return 1;
	}
	if ( !xrtReaderCopy(pReader, pWriter, &iCopied) ||
		 (iCopied != sizeof(arrOutput)) ||
		 (memcmp(arrOutput, "data", sizeof(arrOutput)) != 0) ) {
		xrtReaderDestroy(pReader);
		xrtWriterDestroy(pWriter);
		return 2;
	}
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);
	return 0;
}
