#include <xrt.h>

#include <stdio.h>



/* 使用一行 Helper 为 Logger 附加 JSON Lines 文件输出。 */
int main(void)
{
	xlogfileoptions Options;
	xlogger* pLogger;
	xlogfield Field;

	if ( !xrtLogFileOptionsInit(&Options, "example_logger_json.log") ) {
		return 1;
	}
	Options.MaxBytes = 1024u * 1024u;
	Options.BackupCount = 3u;
	pLogger = xrtLogCreate(XRT_STR_LITERAL("example"), XLOG_INFO);
	Field = xrtLogFieldUInt(XRT_STR_LITERAL("request_id"), 42u);
	if (
		(pLogger == NULL) ||
		!xrtLogAddJsonFile(pLogger, &Options, NULL) ||
		(xrtLogFields(
			pLogger,
			XLOG_INFO,
			XRT_STR_LITERAL("JSON file sink ready"),
			&Field,
			1u
		) != XLOG_RESULT_WRITTEN)
	) {
		xrtLogFree(pLogger);
		return 2;
	}
	xrtLogFree(pLogger);
	printf("wrote example_logger_json.log\n");
	return 0;
}
