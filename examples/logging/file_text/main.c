/*
 * 范例：logging/file_text —— 文本格式文件日志（可插拔格式预设）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtLogTextConfigInit   文本格式配置（预设 = 字段开关组合）
 *   XLOG_TEXT_SIMPLE       预设：时间 级别 名字 - 消息（控制台同款）
 *   xrtLogAddTextFile      一行挂载文本格式滚动文件 Sink
 * 模块宏：XRT_MODULE_LOGGER
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/logging/file_text/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   wrote example_logger_text.log
 * （文件内容每行形如 "2026-...Z INFO example - text file sink ready"）
 *
 * 格式预设体系：SIMPLE（人读友好）之外还有
 *   XLOG_TEXT_LEVEL / _MESSAGE 等单字段开关自由组合
 *   （见 format_text_buffer 范例），预置与自定义同构。
 */

#include <xrt.h>

#include <stdio.h>



int main(void)
{
	xlogfileoptions Options;
	xlogtextconfig Text;
	xlogger* pLogger;

	if (
		!xrtLogFileOptionsInit(&Options, "example_logger_text.log") ||
		!xrtLogTextConfigInit(&Text, XLOG_TEXT_SIMPLE)
	) {
		return 1;
	}
	Options.MaxBytes = 1024u * 1024u;
	Options.BackupCount = 3u;

	pLogger = xrtLogCreate(XRT_STR_LITERAL("example"), XLOG_INFO);
	if (
		(pLogger == NULL) ||
		!xrtLogAddTextFile(pLogger, &Options, &Text) ||
		(xrtLog(
			pLogger,
			XLOG_INFO,
			XRT_STR_LITERAL("text file sink ready")
		) != XLOG_RESULT_WRITTEN)
	) {
		xrtLogFree(pLogger);
		return 2;
	}
	xrtLogFree(pLogger);
	printf("wrote example_logger_text.log\n");
	return 0;
}
