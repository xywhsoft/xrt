#include "../test.h"

#include <stdio.h>



#define TEST_LOG_JSON_FILE_PATH "test_logger_file_json.log"



/* 读取 JSON 文件并比较完整内容。 */
static void testLogJsonFileEqual(cstr sExpected)
{
	FILE* pFile = fopen(TEST_LOG_JSON_FILE_PATH, "rb");
	char arrOutput[512];
	size_t iSize;
	size_t iExpected = strlen(sExpected);

	testRequire(pFile != NULL, "Logger JSON file read open failed");
	iSize = fread(arrOutput, 1u, sizeof(arrOutput), pFile);
	testRequire(!ferror(pFile), "Logger JSON file read failed");
	testRequire(fclose(pFile) == 0, "Logger JSON file read close failed");
	testRequire(
		(iSize == iExpected) &&
		(memcmp(arrOutput, sExpected, iExpected) == 0),
		"Logger JSON file layout changed"
	);
}



/* 验证 JSON 配置被复制，且直接 Sink 和一行 Logger Helper 语义一致。 */
int main(void)
{
	xlogfileoptions Options;
	xlogjsonconfig Json;
	xlogrecord Record;
	xlogsink* pSink;
	xlogger* pLogger;

	if ( xrtPathExists(TEST_LOG_JSON_FILE_PATH) ) {
		testRequire(
			xrtFileDelete(TEST_LOG_JSON_FILE_PATH),
			"Logger JSON file cleanup failed"
		);
	}
	testRequire(
		xrtLogFileOptionsInit(&Options, TEST_LOG_JSON_FILE_PATH) &&
		xrtLogJsonConfigInit(&Json),
		"Logger JSON file options failed"
	);
	Options.Mode = XLOG_FILE_TRUNCATE;
	Json.Flags = XLOG_JSON_MESSAGE | XLOG_JSON_NEWLINE;
	pSink = xrtLogJsonFile(&Options, &Json);
	testRequire(pSink != NULL, "Logger JSON file creation failed");
	Json.Flags = XLOG_JSON_LEVEL;
	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = XRT_STR_LITERAL("direct");
	testRequire(
		xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_WRITTEN,
		"Logger JSON file direct submit failed"
	);
	xrtLogSinkFree(pSink);

	Options.Mode = XLOG_FILE_APPEND;
	testRequire(
		xrtLogJsonConfigInit(&Json),
		"Logger JSON helper config failed"
	);
	Json.Flags = XLOG_JSON_MESSAGE | XLOG_JSON_NEWLINE;
	pLogger = xrtLogCreate(XRT_STR_LITERAL("json-file"), XLOG_INFO);
	testRequire(
		(pLogger != NULL) &&
		xrtLogAddJsonFile(pLogger, &Options, &Json) &&
		(xrtLog(
			pLogger,
			XLOG_INFO,
			XRT_STR_LITERAL("helper")
		) == XLOG_RESULT_WRITTEN),
		"Logger JSON file helper failed"
	);
	xrtLogFree(pLogger);
	testLogJsonFileEqual(
		"{\"message\":\"direct\"}\n"
		"{\"message\":\"helper\"}\n"
	);
	testRequire(
		xrtFileDelete(TEST_LOG_JSON_FILE_PATH),
		"Logger JSON file final cleanup failed"
	);
	printf("[PASS] Logger JSON file\n");
	return 0;
}
