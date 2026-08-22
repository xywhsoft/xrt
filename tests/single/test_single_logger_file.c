#define XRT_MODULE_LOGGER_FILE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头测试格式器只输出消息本身。 */
static bool testSingleLogFileFormat(
	const xlogrecord* pRecord,
	xlogwriteproc pWrite,
	ptr pWriteData,
	ptr pUserData
)
{
	(void)pUserData;
	return pWrite(
		(xbytesview){
			(cbytes)pRecord->Message.Data,
			pRecord->Message.Size
		},
		pWriteData
	);
}



/* 验证通用文件 Sink 不拉入文本或 JSON 格式器。 */
int main(void)
{
	xlogfileoptions Options;
	xlogfileconfig Config;
	xlogsink* pSink;

	#if !defined(XRT_FEATURE_LOGGER_FILE) || \
		!defined(XRT_FEATURE_BUFFER) || \
		!defined(XRT_FEATURE_FILE) || \
		defined(XRT_FEATURE_LOGGER_FORMAT_TEXT) || \
		defined(XRT_FEATURE_LOGGER_FORMAT_JSON)
		#error "XRT_MODULE_LOGGER_FILE dependency closure is incorrect"
	#endif

	if ( !xrtLogFileOptionsInit(&Options, "test_single_logger_file.log") ) {
		return 1;
	}
	Options.Mode = XLOG_FILE_TRUNCATE;
	memset(&Config, 0, sizeof(Config));
	Config.Options = Options;
	Config.Format = testSingleLogFileFormat;
	pSink = xrtLogFile(&Config);
	if ( pSink == NULL ) {
		return 2;
	}
	xrtLogSinkFree(pSink);
	if ( !xrtFileDelete("test_single_logger_file.log") ) {
		return 3;
	}
	return 0;
}
