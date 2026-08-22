#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 示例 Sink 直接消费借用记录，不分配中间消息对象。 */
static xlogresult exampleWrite(
	const xlogrecord* pRecord,
	ptr pUserData
)
{
	FILE* pFile = (FILE*)pUserData;

	fprintf(
		pFile,
		"[%s] %.*s: %.*s\n",
		xrtLogLevelName(pRecord->Level),
		(int)pRecord->Logger.Size,
		pRecord->Logger.Data,
		(int)pRecord->Message.Size,
		pRecord->Message.Data
	);
	return ferror(pFile) == 0
		? XLOG_RESULT_WRITTEN
		: XLOG_RESULT_ERROR;
}



/* 创建 Logger、组合自定义 Sink 并提交结构化记录。 */
int main(void)
{
	xlogsinkconfig Config;
	xlogfield Field;
	xlogger* pLogger;
	xlogsink* pSink;

	memset(&Config, 0, sizeof(Config));
	Config.Name = XRT_STR_LITERAL("stdout");
	Config.Level = XLOG_INFO;
	Config.Write = exampleWrite;
	Config.UserData = stdout;
	pLogger = xrtLogCreate(XRT_STR_LITERAL("example"), XLOG_DEBUG);
	pSink = xrtLogSinkCreate(&Config);
	if (
		(pLogger == NULL) || (pSink == NULL) ||
		!xrtLogAttach(pLogger, pSink)
	) {
		xrtLogSinkFree(pSink);
		xrtLogFree(pLogger);
		return 1;
	}
	Field = xrtLogFieldInt(XRT_STR_LITERAL("request_id"), 42);
	if (
		xrtLogFields(
			pLogger,
			XLOG_INFO,
			XRT_STR_LITERAL("request complete"),
			&Field,
			1u
		) != XLOG_RESULT_WRITTEN
	) {
		xrtLogSinkFree(pSink);
		xrtLogFree(pLogger);
		return 2;
	}
	xrtLogSinkFree(pSink);
	xrtLogFree(pLogger);
	return 0;
}
