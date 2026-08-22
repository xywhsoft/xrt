#define XRT_MODULE_LOGGER_PRINTF
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头 printf Sink 验证格式化结果。 */
static xlogresult testSingleLogPrintfWrite(
	const xlogrecord* pRecord,
	ptr pUserData
)
{
	(void)pUserData;
	return
		(pRecord->Message.Size == 8u) &&
		(memcmp(pRecord->Message.Data, "value=12", 8u) == 0)
		? XLOG_RESULT_WRITTEN
		: XLOG_RESULT_ERROR;
}



/* 验证 printf 单头模块启用字符串格式化闭包。 */
int main(void)
{
	xlogsinkconfig Config;
	xlogger* pLogger;
	xlogsink* pSink;
	int iResult = 0;

	#if !defined(XRT_FEATURE_LOGGER_PRINTF) || \
		!defined(XRT_FEATURE_LOGGER_CORE) || \
		!defined(XRT_FEATURE_STRING_FORMAT)
		#error "XRT_MODULE_LOGGER_PRINTF did not enable its dependency closure"
	#endif

	memset(&Config, 0, sizeof(Config));
	Config.Name = XRT_STR_LITERAL("printf");
	Config.Level = XLOG_TRACE;
	Config.Write = testSingleLogPrintfWrite;
	pLogger = xrtLogCreate(XRT_STR_LITERAL("printf"), XLOG_TRACE);
	pSink = xrtLogSinkCreate(&Config);
	if (
		(pLogger == NULL) || (pSink == NULL) ||
		!xrtLogAttach(pLogger, pSink) ||
		(xrtLogPrintf(pLogger, XLOG_INFO, "value=%d", 12) !=
		 XLOG_RESULT_WRITTEN)
	) {
		iResult = 1;
	}
	xrtLogSinkFree(pSink);
	xrtLogFree(pLogger);
	return iResult;
}
