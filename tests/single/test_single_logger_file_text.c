#define XRT_MODULE_LOGGER_FILE_TEXT
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证文本文件 Helper 不拉入 JSON 格式器。 */
int main(void)
{
	xlogfileoptions Options;
	xlogtextconfig Text;
	xlogsink* pSink;

	#if !defined(XRT_FEATURE_LOGGER_FILE_TEXT) || \
		!defined(XRT_FEATURE_LOGGER_FILE) || \
		!defined(XRT_FEATURE_LOGGER_FORMAT_TEXT) || \
		defined(XRT_FEATURE_LOGGER_FORMAT_JSON)
		#error "XRT_MODULE_LOGGER_FILE_TEXT dependency closure is incorrect"
	#endif

	if (
		!xrtLogFileOptionsInit(&Options, "test_single_logger_file_text.log") ||
		!xrtLogTextConfigInit(&Text, XLOG_TEXT_MESSAGE)
	) {
		return 1;
	}
	Options.Mode = XLOG_FILE_TRUNCATE;
	pSink = xrtLogTextFile(&Options, &Text);
	if ( pSink == NULL ) {
		return 2;
	}
	xrtLogSinkFree(pSink);
	if ( !xrtFileDelete("test_single_logger_file_text.log") ) {
		return 3;
	}
	return 0;
}
