#include "../test.h"
#include "../test_fault_allocator.h"



#define TEST_LOG_FILE_OOM_PATH "test_logger_file_oom.log"



/* OOM 格式器直接把消息写入动态记录缓冲。 */
static bool testLogFileOomFormat(
	const xlogrecord* pRecord,
	xlogwriteproc pWrite,
	ptr pWriteData,
	ptr pUserData
)
{
	(void)pUserData;
	return pWrite(
		(xbytesview){
			(cbytes)pRecord->Message.Data,
			pRecord->Message.Size
		},
		pWriteData
	);
}



/* 验证记录缓冲 OOM 不写部分文件，并可在解除故障后继续使用。 */
int main(void)
{
	static testfaultallocator State = { 0, SIZE_MAX, 0, false };
	xallocator Allocator = testFaultAllocator(&State);
	xlogfileoptions Options;
	xlogfileconfig Config;
	xlogfilestats Stats;
	xlogrecord Record;
	xlogsink* pSink;
	char arrLarge[4096];

	testRequire(
		xrtSetAllocator(&Allocator),
		"Logger file OOM allocator setup failed"
	);
	if ( xrtPathExists(TEST_LOG_FILE_OOM_PATH) ) {
		testRequire(
			xrtFileDelete(TEST_LOG_FILE_OOM_PATH),
			"Logger file OOM cleanup failed"
		);
	}
	testRequire(
		xrtLogFileOptionsInit(&Options, TEST_LOG_FILE_OOM_PATH),
		"Logger file OOM options failed"
	);
	Options.Mode = XLOG_FILE_TRUNCATE;
	Options.RecordLimit = sizeof(arrLarge) * 2u;
	memset(&Config, 0, sizeof(Config));
	Config.Options = Options;
	Config.Format = testLogFileOomFormat;
	pSink = xrtLogFile(&Config);
	testRequire(pSink != NULL, "Logger file OOM creation failed");
	memset(arrLarge, 'x', sizeof(arrLarge));
	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = (xstrview){ arrLarge, sizeof(arrLarge) };
	State.FailAt = State.Calls + 1u;
	State.Hit = false;
	testRequire(
		xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_ERROR,
		"Logger file buffer OOM did not fail"
	);
	testRequire(
		State.Hit &&
		(xrtErrorFind(
			xrtGetError(),
			"xrt.log",
			XLOG_ERROR_FILE_FORMAT
		) != NULL) &&
		(xrtErrorIs(xrtGetError(), XERR_MEMORY) != NULL),
		"Logger file OOM error chain changed"
	);
	xrtClearError();
	testRequire(
		xrtLogFileStats(pSink, &Stats) &&
		(Stats.CurrentBytes == 0u) &&
		(Stats.WrittenBytes == 0u) &&
		(Stats.Records == 0u),
		"Logger file OOM wrote partial data"
	);
	State.FailAt = SIZE_MAX;
	Record.Message = XRT_STR_LITERAL("ok");
	testRequire(
		xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_WRITTEN,
		"Logger file did not recover after OOM"
	);
	xrtLogSinkFree(pSink);
	testRequire(
		xrtFileDelete(TEST_LOG_FILE_OOM_PATH),
		"Logger file OOM final cleanup failed"
	);
	testMemoryDebugDrain("Logger file OOM memory debug reset failed");
	printf("[PASS] Logger file OOM\n");
	return 0;
}
