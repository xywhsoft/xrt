#include "../test.h"

#include <stdio.h>



/* 文件测试路径集中管理，避免遗漏滚动备份清理。 */
#define TEST_LOG_FILE_PATH "test_logger_file.log"
#define TEST_LOG_FILE_BACKUP1 "test_logger_file.log.1"
#define TEST_LOG_FILE_BACKUP2 "test_logger_file.log.2"
#define TEST_LOG_FILE_EXTERNAL "test_logger_file.external.log"
#define TEST_LOG_FILE_UTF8 \
	"test_logger_" "\xE6\x97\xA5\xE5\xBF\x97" ".log"



/* 格式器所有权测试通过外部计数观察释放次数。 */
typedef struct testlogfileowned {
	size_t* Drops;
} testlogfileowned;



/* 递归错误处理器保存同一 Sink 和嵌套提交结果。 */
typedef struct testlogfileerror {
	xlogsink* Sink;
	xlogrecord Record;
	size_t Calls;
	xlogresult Nested;
} testlogfileerror;



/* 清理一个可能不存在的测试文件。 */
static void testLogFileRemove(cstr sPath)
{
	if ( xrtPathExists(sPath) ) {
		testRequire(xrtFileDelete(sPath), "Logger file cleanup failed");
	}
}



/* 清理当前文件、两个备份和外部轮转文件。 */
static void testLogFileCleanup(void)
{
	testLogFileRemove(TEST_LOG_FILE_PATH);
	testLogFileRemove(TEST_LOG_FILE_BACKUP1);
	testLogFileRemove(TEST_LOG_FILE_BACKUP2);
	testLogFileRemove(TEST_LOG_FILE_EXTERNAL);
}



/* 使用 stdio 写入已有文件夹具。 */
static void testLogFileWriteFixture(cstr sPath, cstr sText)
{
	FILE* pFile = fopen(sPath, "wb");

	testRequire(pFile != NULL, "Logger file fixture open failed");
	testRequire(
		fwrite(sText, 1u, strlen(sText), pFile) == strlen(sText),
		"Logger file fixture write failed"
	);
	testRequire(fclose(pFile) == 0, "Logger file fixture close failed");
}



/* 读取完整测试文件到固定缓冲。 */
static size_t testLogFileRead(
	cstr sPath,
	char* sOutput,
	size_t iCapacity
)
{
	xfile File = xrtOpen(sPath, XFILE_READ);
	size_t iSize = 0;

	testRequire(File != NULL, "Logger file read open failed");
	testRequire(iCapacity != 0u, "Logger file read capacity is zero");
	while ( iSize < (iCapacity - 1u) ) {
		size_t iRead = 0;

		testRequire(
			xrtRead(
				File,
				sOutput + iSize,
				(iCapacity - 1u) - iSize,
				&iRead
			),
			"Logger file read failed"
		);
		if ( iRead == 0u ) {
			break;
		}
		iSize += iRead;
	}
	testRequire(xrtClose(File), "Logger file read close failed");
	sOutput[iSize] = 0;
	return iSize;
}



/* 比较完整文件内容。 */
static void testLogFileEqual(cstr sPath, cstr sExpected, cstr sMessage)
{
	char arrOutput[4096];
	size_t iSize = testLogFileRead(sPath, arrOutput, sizeof(arrOutput));
	size_t iExpected = strlen(sExpected);

	testRequire(
		(iSize == iExpected) &&
		(memcmp(arrOutput, sExpected, iExpected) == 0),
		sMessage
	);
}



/* 最小格式器分两段写出消息和换行。 */
static bool testLogFileFormat(
	const xlogrecord* pRecord,
	xlogwriteproc pWrite,
	ptr pWriteData,
	ptr pUserData
)
{
	(void)pUserData;
	return
		pWrite(
			(xbytesview){
				(cbytes)pRecord->Message.Data,
				pRecord->Message.Size
			},
			pWriteData
		) &&
		pWrite(XRT_BYTES_LITERAL("\n"), pWriteData);
}



/* 拥有型格式器复用相同布局。 */
static bool testLogFileOwnedFormat(
	const xlogrecord* pRecord,
	xlogwriteproc pWrite,
	ptr pWriteData,
	ptr pUserData
)
{
	(void)pUserData;
	return testLogFileFormat(pRecord, pWrite, pWriteData, NULL);
}



/* 故意忽略 Writer 失败，验证文件核心仍会拒绝截断记录。 */
static bool testLogFileIgnoringFormat(
	const xlogrecord* pRecord,
	xlogwriteproc pWrite,
	ptr pWriteData,
	ptr pUserData
)
{
	(void)pUserData;
	(void)pWrite(
		(xbytesview){
			(cbytes)pRecord->Message.Data,
			pRecord->Message.Size
		},
		pWriteData
	);
	return true;
}



/* 释放拥有型格式器数据并累计恰好一次。 */
static void testLogFileOwnedDrop(ptr pUserData)
{
	testlogfileowned* pOwned = (testlogfileowned*)pUserData;

	(*pOwned->Drops)++;
	xrtFree(pOwned);
}



/* 创建使用最小格式器的文件 Sink。 */
static xlogsink* testLogFileCreate(xlogfileoptions* pOptions)
{
	xlogfileconfig Config;

	memset(&Config, 0, sizeof(Config));
	Config.Options = *pOptions;
	Config.Format = testLogFileFormat;
	return xrtLogFile(&Config);
}



/* 创建一条最小借用记录。 */
static xlogrecord testLogFileRecord(xstrview Message)
{
	xlogrecord Record;

	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = Message;
	return Record;
}



/* 验证默认值、追加、截断、持久化和统计。 */
static void testLogFileBasic(void)
{
	xlogfileoptions Options;
	xlogfilestats Stats;
	xlogrecord Record;
	xlogsink* pSink;

	testLogFileCleanup();
	testLogFileWriteFixture(TEST_LOG_FILE_PATH, "old\n");
	testRequire(
		xrtLogFileOptionsInit(&Options, TEST_LOG_FILE_PATH) &&
		(Options.Level == XLOG_INFO) &&
		(Options.Mode == XLOG_FILE_APPEND) &&
		(Options.Sync == XLOG_FILE_SYNC_MANUAL) &&
		(Options.RecordLimit == XLOG_FILE_RECORD_LIMIT_DEFAULT) &&
		(Options.BufferLimit == XLOG_FILE_BUFFER_LIMIT_DEFAULT),
		"Logger file defaults changed"
	);
	Options.Sync = XLOG_FILE_SYNC_RECORD;
	pSink = testLogFileCreate(&Options);
	testRequire(pSink != NULL, "Logger append file creation failed");
	testRequire(
		strcmp(xrtLogFilePath(pSink), TEST_LOG_FILE_PATH) == 0,
		"Logger file path changed"
	);
	Record = testLogFileRecord(XRT_STR_LITERAL("new"));
	testRequire(
		xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_WRITTEN,
		"Logger append file submit failed"
	);
	testRequire(
		xrtLogFileStats(pSink, &Stats) &&
		(Stats.CurrentBytes == 8u) &&
		(Stats.WrittenBytes == 4u) &&
		(Stats.Records == 1u) &&
		(Stats.Syncs == 1u),
		"Logger append file stats changed"
	);
	testRequire(xrtLogSinkFlush(pSink), "Logger explicit file flush failed");
	testRequire(
		xrtLogFileStats(pSink, &Stats) && (Stats.Syncs == 2u),
		"Logger explicit file sync stat changed"
	);
	xrtLogSinkFree(pSink);
	testLogFileEqual(
		TEST_LOG_FILE_PATH,
		"old\nnew\n",
		"Logger append layout changed"
	);

	Options.Mode = XLOG_FILE_TRUNCATE;
	Options.Sync = XLOG_FILE_SYNC_MANUAL;
	pSink = testLogFileCreate(&Options);
	testRequire(pSink != NULL, "Logger truncate file creation failed");
	Record = testLogFileRecord(XRT_STR_LITERAL("fresh"));
	testRequire(
		xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_WRITTEN,
		"Logger truncate file submit failed"
	);
	xrtLogSinkFree(pSink);
	testLogFileEqual(
		TEST_LOG_FILE_PATH,
		"fresh\n",
		"Logger truncate layout changed"
	);

	Options.Sync = XLOG_FILE_SYNC_INTERVAL;
	Options.SyncInterval = 1u;
	pSink = testLogFileCreate(&Options);
	testRequire(pSink != NULL, "Logger interval file creation failed");
	xrtSleepUs(10u);
	Record = testLogFileRecord(XRT_STR_LITERAL("tick"));
	testRequire(
		(xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_WRITTEN) &&
		xrtLogFileStats(pSink, &Stats) &&
		(Stats.Syncs == 1u),
		"Logger interval sync policy changed"
	);
	xrtLogSinkFree(pSink);
	testLogFileCleanup();
}



/* 验证编码后精确阈值、备份次序和手动滚动。 */
static void testLogFileRotation(void)
{
	xlogfileoptions Options;
	xlogfilestats Stats;
	xlogrecord Record;
	xlogsink* pSink;

	testLogFileCleanup();
	testRequire(
		xrtLogFileOptionsInit(&Options, TEST_LOG_FILE_PATH),
		"Logger rotation options failed"
	);
	Options.Mode = XLOG_FILE_TRUNCATE;
	Options.MaxBytes = 8u;
	Options.BackupCount = 2u;
	pSink = testLogFileCreate(&Options);
	testRequire(pSink != NULL, "Logger rolling file creation failed");
	Record = testLogFileRecord(XRT_STR_LITERAL("aaa"));
	testRequire(
		xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_WRITTEN,
		"Logger rolling first submit failed"
	);
	Record.Message = XRT_STR_LITERAL("bbb");
	testRequire(
		xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_WRITTEN,
		"Logger rolling boundary submit failed"
	);
	Record.Message = XRT_STR_LITERAL("c");
	testRequire(
		xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_WRITTEN,
		"Logger rolling overflow submit failed"
	);
	testRequire(
		xrtLogFileStats(pSink, &Stats) &&
		(Stats.CurrentBytes == 2u) &&
		(Stats.WrittenBytes == 10u) &&
		(Stats.Records == 3u) &&
		(Stats.Rotations == 1u),
		"Logger automatic rotation stats changed"
	);
	testLogFileEqual(
		TEST_LOG_FILE_BACKUP1,
		"aaa\nbbb\n",
		"Logger exact rotation boundary changed"
	);
	testLogFileEqual(
		TEST_LOG_FILE_PATH,
		"c\n",
		"Logger current rolling file changed"
	);
	testRequire(xrtLogFileRotate(pSink), "Logger manual rotation failed");
	Record.Message = XRT_STR_LITERAL("d");
	testRequire(
		xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_WRITTEN,
		"Logger post-rotation submit failed"
	);
	xrtLogSinkFree(pSink);
	testLogFileEqual(
		TEST_LOG_FILE_BACKUP2,
		"aaa\nbbb\n",
		"Logger second backup order changed"
	);
	testLogFileEqual(
		TEST_LOG_FILE_BACKUP1,
		"c\n",
		"Logger first backup order changed"
	);
	testLogFileEqual(
		TEST_LOG_FILE_PATH,
		"d\n",
		"Logger post-rotation file changed"
	);
	testLogFileCleanup();

	Options.BackupCount = 0u;
	Options.MaxBytes = 4u;
	pSink = testLogFileCreate(&Options);
	testRequire(pSink != NULL, "Logger zero-backup file creation failed");
	Record = testLogFileRecord(XRT_STR_LITERAL("aaa"));
	testRequire(
		(xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_WRITTEN),
		"Logger zero-backup boundary submit failed"
	);
	Record.Message = XRT_STR_LITERAL("b");
	testRequire(
		(xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_WRITTEN),
		"Logger zero-backup rotation failed"
	);
	xrtLogSinkFree(pSink);
	testLogFileEqual(
		TEST_LOG_FILE_PATH,
		"b\n",
		"Logger zero-backup rotation did not truncate"
	);
	testRequire(
		!xrtPathExists(TEST_LOG_FILE_BACKUP1),
		"Logger zero-backup rotation created a backup"
	);
	testLogFileCleanup();
}



/* 验证外部改名后 reopen 会切换到当前路径的新文件。 */
static void testLogFileReopen(void)
{
	xlogfileoptions Options;
	xlogfilestats Stats;
	xlogrecord Record;
	xlogsink* pSink;

	testLogFileCleanup();
	testRequire(
		xrtLogFileOptionsInit(&Options, TEST_LOG_FILE_PATH),
		"Logger reopen options failed"
	);
	Options.Mode = XLOG_FILE_TRUNCATE;
	pSink = testLogFileCreate(&Options);
	testRequire(pSink != NULL, "Logger reopen file creation failed");
	Record = testLogFileRecord(XRT_STR_LITERAL("old"));
	testRequire(
		(xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_WRITTEN) &&
		xrtPathRename(
			TEST_LOG_FILE_PATH,
			TEST_LOG_FILE_EXTERNAL,
			false
		) &&
		xrtLogFileReopen(pSink),
		"Logger external reopen failed"
	);
	Record.Message = XRT_STR_LITERAL("new");
	testRequire(
		xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_WRITTEN,
		"Logger reopened submit failed"
	);
	testRequire(
		xrtLogFileStats(pSink, &Stats) &&
		(Stats.Reopens == 1u) &&
		(Stats.CurrentBytes == 4u),
		"Logger reopen stats changed"
	);
	xrtLogSinkFree(pSink);
	testLogFileEqual(
		TEST_LOG_FILE_EXTERNAL,
		"old\n",
		"Logger external rotated file changed"
	);
	testLogFileEqual(
		TEST_LOG_FILE_PATH,
		"new\n",
		"Logger reopened current file changed"
	);
	testLogFileCleanup();
}



/* 验证旧版已经覆盖的 UTF-8 路径能力继续由统一文件底座提供。 */
static void testLogFileUtf8Path(void)
{
	xlogfileoptions Options;
	xlogrecord Record;
	xlogsink* pSink;

	testLogFileRemove(TEST_LOG_FILE_UTF8);
	testRequire(
		xrtLogFileOptionsInit(&Options, TEST_LOG_FILE_UTF8),
		"Logger UTF-8 path options failed"
	);
	Options.Mode = XLOG_FILE_TRUNCATE;
	pSink = testLogFileCreate(&Options);
	testRequire(pSink != NULL, "Logger UTF-8 path creation failed");
	Record = testLogFileRecord(XRT_STR_LITERAL("utf8"));
	testRequire(
		xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_WRITTEN,
		"Logger UTF-8 path submit failed"
	);
	xrtLogSinkFree(pSink);
	testLogFileEqual(
		TEST_LOG_FILE_UTF8,
		"utf8\n",
		"Logger UTF-8 path content changed"
	);
	testLogFileRemove(TEST_LOG_FILE_UTF8);
}



/* 错误通知期间递归提交同一文件 Sink 必须主动丢弃。 */
static void testLogFileErrorHandler(
	const xerror* pError,
	ptr pUserData
)
{
	testlogfileerror* pContext = (testlogfileerror*)pUserData;

	(void)pError;
	pContext->Calls++;
	pContext->Nested = xrtLogSinkSubmit(
		pContext->Sink,
		&pContext->Record
	);
}



/* 验证硬记录上限、递归保护和失败后的继续服务能力。 */
static void testLogFileLimit(void)
{
	testlogfileerror Context;
	xlogfileoptions Options;
	xlogfilestats Stats;
	xlogrecord Record;
	xlogsink* pSink;

	testLogFileCleanup();
	testRequire(
		xrtLogFileOptionsInit(&Options, TEST_LOG_FILE_PATH),
		"Logger limit options failed"
	);
	Options.Mode = XLOG_FILE_TRUNCATE;
	Options.RecordLimit = 4u;
	Options.BufferLimit = 0u;
	pSink = testLogFileCreate(&Options);
	testRequire(pSink != NULL, "Logger limited file creation failed");
	Record = testLogFileRecord(XRT_STR_LITERAL("four"));
	memset(&Context, 0, sizeof(Context));
	Context.Sink = pSink;
	Context.Record = testLogFileRecord(XRT_STR_LITERAL("nested"));
	xrtSetErrorHandler(testLogFileErrorHandler, &Context);
	testRequire(
		xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_ERROR,
		"Logger record limit did not fail"
	);
	xrtSetErrorHandler(NULL, NULL);
	testRequire(
		(Context.Calls != 0u) &&
		(Context.Nested == XLOG_RESULT_DROPPED) &&
		(xrtErrorFind(
			xrtGetError(),
			"xrt.log",
			XLOG_ERROR_FILE_LIMIT
		) != NULL),
		"Logger record limit or recursive guard changed"
	);
	xrtClearError();
	testRequire(
		xrtLogFileStats(pSink, &Stats) &&
		(Stats.CurrentBytes == 0u) &&
		(Stats.Records == 0u),
		"Logger failed record changed file stats"
	);
	Record.Message = XRT_STR_LITERAL("ok");
	testRequire(
		xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_WRITTEN,
		"Logger file did not recover after record limit"
	);
	xrtLogSinkFree(pSink);
	testLogFileEqual(
		TEST_LOG_FILE_PATH,
		"ok\n",
		"Logger limited file recovery changed"
	);
	testLogFileCleanup();

	testRequire(
		xrtLogFileOptionsInit(&Options, TEST_LOG_FILE_PATH),
		"Logger ignored-writer options failed"
	);
	Options.Mode = XLOG_FILE_TRUNCATE;
	Options.RecordLimit = 2u;
	{
		xlogfileconfig Config;

		memset(&Config, 0, sizeof(Config));
		Config.Options = Options;
		Config.Format = testLogFileIgnoringFormat;
		pSink = xrtLogFile(&Config);
	}
	testRequire(pSink != NULL, "Logger ignored-writer file creation failed");
	Record = testLogFileRecord(XRT_STR_LITERAL("toolong"));
	testRequire(
		xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_ERROR,
		"Logger accepted formatter that ignored Writer failure"
	);
	xrtClearError();
	testRequire(
		xrtLogFileStats(pSink, &Stats) &&
		(Stats.CurrentBytes == 0u) &&
		(Stats.Records == 0u),
		"Logger ignored Writer failure wrote partial data"
	);
	xrtLogSinkFree(pSink);
	testLogFileCleanup();
}



/* 验证格式器数据只在成功创建后转移所有权。 */
static void testLogFileOwnership(void)
{
	xlogfileoptions Options;
	xlogfileconfig Config;
	testlogfileowned* pOwned;
	xlogsink* pSink;
	size_t iDrops = 0;

	testLogFileCleanup();
	testRequire(
		xrtLogFileOptionsInit(&Options, TEST_LOG_FILE_PATH),
		"Logger ownership options failed"
	);
	Options.Mode = XLOG_FILE_TRUNCATE;
	pOwned = (testlogfileowned*)xrtMalloc(sizeof(testlogfileowned));
	testRequire(pOwned != NULL, "Logger owned formatter allocation failed");
	pOwned->Drops = &iDrops;
	memset(&Config, 0, sizeof(Config));
	Config.Options = Options;
	Config.Format = testLogFileOwnedFormat;
	Config.Drop = testLogFileOwnedDrop;
	Config.UserData = pOwned;
	pSink = xrtLogFile(&Config);
	testRequire(pSink != NULL, "Logger owned formatter creation failed");
	xrtLogSinkFree(pSink);
	testRequire(iDrops == 1u, "Logger owned formatter was not dropped once");

	pOwned = (testlogfileowned*)xrtMalloc(sizeof(testlogfileowned));
	testRequire(pOwned != NULL, "Logger failed ownership allocation failed");
	pOwned->Drops = &iDrops;
	Config.Options.RecordLimit = 0u;
	Config.UserData = pOwned;
	testRequire(
		xrtLogFile(&Config) == NULL,
		"Logger invalid ownership config succeeded"
	);
	testRequire(iDrops == 1u, "Logger failed creation consumed formatter data");
	xrtClearError();
	xrtFree(pOwned);
	testLogFileCleanup();
}



/* 执行文件 Sink 契约、滚动、恢复、错误和所有权回归。 */
int main(void)
{
	testLogFileBasic();
	testLogFileRotation();
	testLogFileReopen();
	testLogFileUtf8Path();
	testLogFileLimit();
	testLogFileOwnership();
	printf("[PASS] Logger file\n");
	return 0;
}
