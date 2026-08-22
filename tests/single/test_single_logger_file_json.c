#define XRT_MODULE_LOGGER_FILE_JSON
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证 JSON 文件 Helper 不拉入文本格式器。 */
int main(void)
{
	xlogfileoptions Options;
	xlogjsonconfig Json;
	xlogsink* pSink;

	#if !defined(XRT_FEATURE_LOGGER_FILE_JSON) || \
		!defined(XRT_FEATURE_LOGGER_FILE) || \
		!defined(XRT_FEATURE_LOGGER_FORMAT_JSON) || \
		defined(XRT_FEATURE_LOGGER_FORMAT_TEXT)
		#error "XRT_MODULE_LOGGER_FILE_JSON dependency closure is incorrect"
	#endif

	if (
		!xrtLogFileOptionsInit(&Options, "test_single_logger_file_json.log") ||
		!xrtLogJsonConfigInit(&Json)
	) {
		return 1;
	}
	Options.Mode = XLOG_FILE_TRUNCATE;
	pSink = xrtLogJsonFile(&Options, &Json);
	if ( pSink == NULL ) {
		return 2;
	}
	xrtLogSinkFree(pSink);
	if ( !xrtFileDelete("test_single_logger_file_json.log") ) {
		return 3;
	}
	return 0;
}
