#include "../test.h"

#include <stdio.h>



#define TEST_LOG_TEXT_FILE_PATH "test_logger_file_text.log"



/* 读取文本文件并比较完整内容。 */
static void testLogTextFileEqual(cstr sExpected)
{
	FILE* pFile = fopen(TEST_LOG_TEXT_FILE_PATH, "rb");
	char arrOutput[512];
	size_t iSize;
	size_t iExpected = strlen(sExpected);

	testRequire(pFile != NULL, "Logger text file read open failed");
	iSize = fread(arrOutput, 1u, sizeof(arrOutput), pFile);
	testRequire(!ferror(pFile), "Logger text file read failed");
	testRequire(fclose(pFile) == 0, "Logger text file read close failed");
	testRequire(
		(iSize == iExpected) &&
		(memcmp(arrOutput, sExpected, iExpected) == 0),
		"Logger text file layout changed"
	);
}



/* 验证文本配置被复制，且直接 Sink 和一行 Logger Helper 语义一致。 */
int main(void)
{
	xlogfileoptions Options;
	xlogtextconfig Text;
	xlogrecord Record;
	xlogsink* pSink;
	xlogger* pLogger;

	if ( xrtPathExists(TEST_LOG_TEXT_FILE_PATH) ) {
		testRequire(
			xrtFileDelete(TEST_LOG_TEXT_FILE_PATH),
			"Logger text file cleanup failed"
		);
	}
	testRequire(
		xrtLogFileOptionsInit(&Options, TEST_LOG_TEXT_FILE_PATH) &&
		xrtLogTextConfigInit(&Text, XLOG_TEXT_MESSAGE),
		"Logger text file options failed"
	);
	Options.Mode = XLOG_FILE_TRUNCATE;
	pSink = xrtLogTextFile(&Options, &Text);
	testRequire(pSink != NULL, "Logger text file creation failed");
	testRequire(
		xrtLogTextConfigInit(&Text, XLOG_TEXT_FULL),
		"Logger text config mutation fixture failed"
	);
	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = XRT_STR_LITERAL("direct");
	testRequire(
		xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_WRITTEN,
		"Logger text file direct submit failed"
	);
	xrtLogSinkFree(pSink);

	Options.Mode = XLOG_FILE_APPEND;
	testRequire(
		xrtLogTextConfigInit(&Text, XLOG_TEXT_MESSAGE),
		"Logger text helper config failed"
	);
	pLogger = xrtLogCreate(XRT_STR_LITERAL("text-file"), XLOG_INFO);
	testRequire(
		(pLogger != NULL) &&
		xrtLogAddTextFile(pLogger, &Options, &Text) &&
		(xrtLog(
			pLogger,
			XLOG_INFO,
			XRT_STR_LITERAL("helper")
		) == XLOG_RESULT_WRITTEN),
		"Logger text file helper failed"
	);
	xrtLogFree(pLogger);
	testLogTextFileEqual("direct\nhelper\n");
	testRequire(
		xrtFileDelete(TEST_LOG_TEXT_FILE_PATH),
		"Logger text file final cleanup failed"
	);
	printf("[PASS] Logger text file\n");
	return 0;
}
