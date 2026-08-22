#define XRT_MODULE_LOGGER_ASYNC
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头目标统计异步工作线程收到的完整记录。 */
static xlogresult testSingleLogAsyncWrite(
	const xlogrecord* pRecord,
	ptr pUserData
)
{
	size_t* pCount = (size_t*)pUserData;

	if (
		(pRecord->Message.Size != 6u) ||
		(memcmp(pRecord->Message.Data, "single", 6u) != 0)
	) {
		return XLOG_RESULT_ERROR;
	}
	(*pCount)++;
	return XLOG_RESULT_WRITTEN;
}



/* 验证 Async 单头闭包只拉入所需同步与线程底座。 */
int main(void)
{
	xlogsinkconfig TargetConfig;
	xlogasyncstats Stats;
	xlogrecord Record;
	xlogsink* pTarget;
	xlogsink* pAsync;
	size_t iCount = 0;

	#if !defined(XRT_FEATURE_LOGGER_ASYNC) || \
		!defined(XRT_FEATURE_LOGGER_CORE) || \
		!defined(XRT_FEATURE_COND) || \
		!defined(XRT_FEATURE_EVENT) || \
		!defined(XRT_FEATURE_THREAD) || \
		defined(XRT_FEATURE_BUFFER) || \
		defined(XRT_FEATURE_FILE) || \
		defined(XRT_FEATURE_LOGGER_FORMAT_TEXT) || \
		defined(XRT_FEATURE_LOGGER_FORMAT_JSON)
		#error "XRT_MODULE_LOGGER_ASYNC dependency closure is incorrect"
	#endif

	memset(&TargetConfig, 0, sizeof(TargetConfig));
	TargetConfig.Level = XLOG_TRACE;
	TargetConfig.Write = testSingleLogAsyncWrite;
	TargetConfig.UserData = &iCount;
	pTarget = xrtLogSinkCreate(&TargetConfig);
	if ( pTarget == NULL ) {
		return 1;
	}
	pAsync = xrtLogAsync(pTarget, NULL);
	if ( pAsync == NULL ) {
		xrtLogSinkFree(pTarget);
		return 2;
	}
	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = XRT_STR_LITERAL("single");
	if ( xrtLogSinkSubmit(pAsync, &Record) != XLOG_RESULT_WRITTEN ) {
		return 3;
	}
	if ( !xrtLogSinkFlush(pAsync) ) {
		return 4;
	}
	if (
		!xrtLogAsyncStats(pAsync, &Stats) ||
		(iCount != 1u) ||
		(Stats.Written != 1u)
	) {
		return 5;
	}
	xrtLogSinkFree(pAsync);
	xrtLogSinkFree(pTarget);
	return 0;
}
