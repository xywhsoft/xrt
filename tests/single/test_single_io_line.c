#define XRT_MODULE_IO_LINE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件 Line Reader 必须只拉起 IO 与动态 Buffer 闭包。 */
int main(void)
{
	xreader* pReader;
	xlinereader* pLines;
	xlineview Line;

	#if !defined(XRT_FEATURE_IO_LINE) || \
		!defined(XRT_FEATURE_IO) || !defined(XRT_FEATURE_BUFFER)
		#error "XRT_MODULE_IO_LINE dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_IO_FILE)
		#error "XRT_MODULE_IO_LINE pulled file adapters"
	#endif

	pReader = xrtReaderFromMemory(XRT_BYTES_LITERAL("one\r\ntwo"));
	pLines = xrtLineReaderTake(&pReader, 8u);
	if ( (pLines == NULL) || (pReader != NULL) ||
		 (xrtLineReaderNext(pLines, &Line) != XLINE_NEXT_LINE) ||
		 (Line.End != XLINE_END_CRLF) ||
		 (Line.Text.Size != 3u) ||
		 (memcmp(Line.Text.Data, "one", 3u) != 0) ||
		 (xrtLineReaderNext(pLines, &Line) != XLINE_NEXT_LINE) ||
		 (Line.End != XLINE_END_NONE) ||
		 (Line.Text.Size != 3u) ||
		 (memcmp(Line.Text.Data, "two", 3u) != 0) ||
		 (xrtLineReaderNext(pLines, &Line) != XLINE_NEXT_END) ) {
		xrtLineReaderDestroy(pLines);
		return 1;
	}
	return xrtLineReaderDestroy(pLines) ? 0 : 2;
}
