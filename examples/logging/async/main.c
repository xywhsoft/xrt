#include <xrt.h>

#include <stdio.h>
#include <string.h>



/* 示例目标直接输出原始消息，展示 Async 不依赖特定格式器或持久化层。 */
static xlogresult exampleLogAsyncWrite(
	const xlogrecord* pRecord,
	ptr pUserData
)
{
	FILE* pFile = (FILE*)pUserData;

	if (
		fwrite(
			pRecord->Message.Data,
			1u,
			pRecord->Message.Size,
			pFile
		) != pRecord->Message.Size ||
		fputc('\n', pFile) == EOF
	) {
		return XLOG_RESULT_ERROR;
	}
	return XLOG_RESULT_WRITTEN;
}



/* Flush 由异步栅栏在工作线程上按记录顺序调用。 */
static bool exampleLogAsyncFlush(ptr pUserData)
{
	return fflush((FILE*)pUserData) == 0;
}



/* 包装任意同步 Sink，业务线程只执行有界深拷贝和入队。 */
int main(void)
{
	xlogsinkconfig TargetConfig;
	xlogasyncconfig AsyncConfig;
	xlogrecord Record;
	xlogsink* pTarget;
	xlogsink* pAsync;

	memset(&TargetConfig, 0, sizeof(TargetConfig));
	TargetConfig.Name = XRT_STR_LITERAL("stdout");
	TargetConfig.Level = XLOG_TRACE;
	TargetConfig.Write = exampleLogAsyncWrite;
	TargetConfig.Flush = exampleLogAsyncFlush;
	TargetConfig.UserData = stdout;
	pTarget = xrtLogSinkCreate(&TargetConfig);
	if ( pTarget == NULL ) {
		return 1;
	}
	if ( !xrtLogAsyncConfigInit(&AsyncConfig) ) {
		xrtLogSinkFree(pTarget);
		return 2;
	}
	AsyncConfig.Capacity = 256u;
	AsyncConfig.ByteLimit = 2u * 1024u * 1024u;
	pAsync = xrtLogAsync(pTarget, &AsyncConfig);
	xrtLogSinkFree(pTarget);
	if ( pAsync == NULL ) {
		return 3;
	}
	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = XRT_STR_LITERAL("asynchronous sink ready");
	if ( xrtLogSinkSubmit(pAsync, &Record) != XLOG_RESULT_WRITTEN ) {
		xrtLogSinkFree(pAsync);
		return 4;
	}
	if ( !xrtLogSinkFlush(pAsync) ) {
		xrtLogSinkFree(pAsync);
		return 5;
	}
	xrtLogSinkFree(pAsync);
	return 0;
}
