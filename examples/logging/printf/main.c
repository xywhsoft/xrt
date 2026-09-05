/*
 * 范例：logging/printf —— printf 风格便利层：格式化即提交
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtLogPrintf   printf 格式串 + 可变参数 → 格式化并提交记录
 * 模块宏：XRT_MODULE_LOGGER（依赖 STRING_FORMAT）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/logging/printf/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   request=42 status=200
 *
 * 便利层定位：xrtLog 要求现成视图；Printf 接受格式串——
 *   调用点最省事，内部走 xrtFormatV 渲染后构造记录。
 * 注意 Sink 收到的 Message 已是渲染后的完整文本：
 *   格式化发生在 Logger 侧（一次），Sink 侧永远只做输出。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>



/* Sink：消息已格式化完毕，直接打印即可。 */
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

		/* printf 风格提交：格式化 + 记录构造一步完成。 */
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
