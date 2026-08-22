#define XRT_MODULE_IO_FILE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件文件适配器必须完成写入、关闭和重新读取。 */
int main(void)
{
	static const char sPath[] = "test-single-io-file.bin";
	unsigned char arrData[3];
	xwriter* pWriter;
	xreader* pReader;

	#if !defined(XRT_FEATURE_IO_FILE) || \
		!defined(XRT_FEATURE_IO) || !defined(XRT_FEATURE_FILE)
		#error "XRT_MODULE_IO_FILE dependency closure is incomplete"
	#endif

	(void)xrtFileDelete(sPath);
	xrtClearError();
	pWriter = xrtWriterOpen(sPath);
	if ( (pWriter == NULL) ||
		 !xrtWriterWriteFull(pWriter, "abc", 3u, NULL) ||
		 !xrtWriterDestroy(pWriter) ) {
		return 1;
	}
	pReader = xrtReaderOpen(sPath);
	if ( (pReader == NULL) ||
		 !xrtReaderReadFull(pReader, arrData, sizeof(arrData), NULL) ||
		 (memcmp(arrData, "abc", sizeof(arrData)) != 0) ||
		 !xrtReaderDestroy(pReader) ) {
		(void)xrtFileDelete(sPath);
		return 2;
	}
	if ( !xrtFileDelete(sPath) ) {
		return 3;
	}
	return 0;
}
