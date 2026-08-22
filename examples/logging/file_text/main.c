#include <xrt.h>

#include <stdio.h>



/* 使用一行 Helper 为 Logger 附加常用文本文件输出。 */
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
