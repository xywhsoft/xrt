#define XRT_MODULE_LOGGER_CORE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <string.h>



/* 单头测试 Sink 统计接收到的记录。 */
static xlogresult testSingleLogWrite(
	const xlogrecord* pRecord,
	ptr pUserData
)
{
	size_t* pCount = (size_t*)pUserData;

	if (
		(pRecord->Level != XLOG_INFO) ||
		(pRecord->Message.Size != 6u) ||
		(memcmp(pRecord->Message.Data, "single", 6u) != 0)
	) {
		return XLOG_RESULT_ERROR;
	}
	(*pCount)++;
	return XLOG_RESULT_WRITTEN;
}



/* 验证 Logger 单头模块拉起精确依赖并可以独立工作。 */
int main(void)
{
	xlogsinkconfig Config;
	xlogger* pLogger;
	xlogsink* pSink;
	size_t iCount = 0;

	#if !defined(XRT_FEATURE_LOGGER_CORE) || \
		!defined(XRT_FEATURE_ATOMIC) || \
		!defined(XRT_FEATURE_MUTEX) || \
		!defined(XRT_FEATURE_TIME)
		#error "XRT_MODULE_LOGGER_CORE did not enable its dependency closure"
	#endif

	memset(&Config, 0, sizeof(Config));
	Config.Name = XRT_STR_LITERAL("single");
	Config.Level = XLOG_TRACE;
	Config.Write = testSingleLogWrite;
	Config.UserData = &iCount;
	pLogger = xrtLogCreate(XRT_STR_LITERAL("single"), XLOG_TRACE);
	pSink = xrtLogSinkCreate(&Config);
	if (
		(pLogger == NULL) || (pSink == NULL) ||
		!xrtLogAttach(pLogger, pSink) ||
		(xrtLog(pLogger, XLOG_INFO, XRT_STR_LITERAL("single")) !=
		 XLOG_RESULT_WRITTEN) ||
		(iCount != 1u)
	) {
		return 1;
	}
	xrtLogSinkFree(pSink);
	xrtLogFree(pLogger);
	return 0;
}
