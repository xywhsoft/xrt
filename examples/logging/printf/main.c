#include <stdio.h>
#include <string.h>
#include <xrt.h>



/* 把 Logger 已格式化的消息交给示例 Sink。 */
static xlogresult exampleLogPrintfWrite(
	const xlogrecord* pRecord,
	ptr pData
)
{
	(void)pData;
	printf(
		"%.*s\n",
		(int)pRecord->Message.Size,
		pRecord->Message.Data
	);
	return XLOG_RESULT_WRITTEN;
}



/* 使用 printf 风格便利入口提交一条结构化日志记录。 */
int main(void)
{
	xlogsinkconfig Config;
	xlogger* pLogger;
	xlogsink* pSink;
	xlogresult Result = XLOG_RESULT_ERROR;

	memset(&Config, 0, sizeof(Config));
	Config.Name = XRT_STR_LITERAL("console");
	Config.Level = XLOG_TRACE;
	Config.Write = exampleLogPrintfWrite;
	pLogger = xrtLogCreate(XRT_STR_LITERAL("example"), XLOG_TRACE);
	pSink = xrtLogSinkCreate(&Config);
	if ( (pLogger != NULL) && (pSink != NULL) &&
		xrtLogAttach(pLogger, pSink) ) {
		Result = xrtLogPrintf(
			pLogger,
			XLOG_INFO,
			"request=%u status=%u",
			42u,
			200u
		);
	}
	xrtLogSinkFree(pSink);
	xrtLogFree(pLogger);
	return (Result == XLOG_RESULT_WRITTEN) ? 0 : 1;
}
