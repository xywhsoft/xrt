#include "../test.h"
#include "../test_thread.h"

#include <stdio.h>



#define TEST_LOG_FILE_THREAD_PATH "test_logger_file_threads.log"



/* 每个并发工作线程提交固定长度记录。 */
typedef struct testlogfilethread {
	xlogsink* Sink;
	uint32 Index;
	uint32 Count;
} testlogfilethread;



/* 文件线程测试格式器分段输出，验证文件核心会合并为单次写入。 */
static bool testLogFileThreadFormat(
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



/* 工作线程写入可逐行验证的六字节消息。 */
static int testLogFileThreadRun(ptr pData)
{
	testlogfilethread* pThread = (testlogfilethread*)pData;
	xlogrecord Record;
	char arrMessage[16];

	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	for ( uint32 i = 0; i < pThread->Count; i++ ) {
		int iSize = snprintf(
			arrMessage,
			sizeof(arrMessage),
			"T%u-%03u",
			pThread->Index,
			i
		);

		if ( iSize != 6 ) {
			return 1;
		}
		Record.Message = (xstrview){ arrMessage, (size_t)iSize };
		if (
			xrtLogSinkSubmit(pThread->Sink, &Record) !=
			XLOG_RESULT_WRITTEN
		) {
			return 2;
		}
	}
	return 0;
}



/* 验证并发记录不会在格式器分段或系统调用边界交错。 */
int main(void)
{
	testlogfilethread arrContext[4];
	testthread arrThread[4];
	xlogfileoptions Options;
	xlogfileconfig Config;
	xlogsink* pSink;
	FILE* pFile;
	char arrOutput[8192];
	size_t iSize;

	if ( xrtPathExists(TEST_LOG_FILE_THREAD_PATH) ) {
		testRequire(
			xrtFileDelete(TEST_LOG_FILE_THREAD_PATH),
			"Logger threaded file cleanup failed"
		);
	}
	testRequire(
		xrtLogFileOptionsInit(&Options, TEST_LOG_FILE_THREAD_PATH),
		"Logger threaded file options failed"
	);
	Options.Mode = XLOG_FILE_TRUNCATE;
	memset(&Config, 0, sizeof(Config));
	Config.Options = Options;
	Config.Format = testLogFileThreadFormat;
	pSink = xrtLogFile(&Config);
	testRequire(pSink != NULL, "Logger threaded file creation failed");
	memset(arrContext, 0, sizeof(arrContext));
	memset(arrThread, 0, sizeof(arrThread));
	for ( size_t i = 0; i < 4u; i++ ) {
		arrContext[i].Sink = pSink;
		arrContext[i].Index = (uint32)i;
		arrContext[i].Count = 200u;
		arrThread[i].Proc = testLogFileThreadRun;
		arrThread[i].Data = &arrContext[i];
	}
	testThreadsStart(arrThread, 4u);
	testThreadsJoin(arrThread, 4u);
	for ( size_t i = 0; i < 4u; i++ ) {
		testRequire(arrThread[i].Result == 0, "Logger file worker failed");
	}
	xrtLogSinkFree(pSink);
	pFile = fopen(TEST_LOG_FILE_THREAD_PATH, "rb");
	testRequire(pFile != NULL, "Logger threaded file read open failed");
	iSize = fread(arrOutput, 1u, sizeof(arrOutput), pFile);
	testRequire(!ferror(pFile), "Logger threaded file read failed");
	testRequire(fclose(pFile) == 0, "Logger threaded file close failed");
	testRequire(iSize == (4u * 200u * 7u), "Logger threaded file size mismatch");
	for ( size_t i = 0; i < iSize; i += 7u ) {
		testRequire(
			(arrOutput[i] == 'T') &&
			(arrOutput[i + 1u] >= '0') &&
			(arrOutput[i + 1u] <= '3') &&
			(arrOutput[i + 2u] == '-') &&
			(arrOutput[i + 3u] >= '0') &&
			(arrOutput[i + 3u] <= '9') &&
			(arrOutput[i + 4u] >= '0') &&
			(arrOutput[i + 4u] <= '9') &&
			(arrOutput[i + 5u] >= '0') &&
			(arrOutput[i + 5u] <= '9') &&
			(arrOutput[i + 6u] == '\n'),
			"Logger concurrent file records interleaved"
		);
	}
	testRequire(
		xrtFileDelete(TEST_LOG_FILE_THREAD_PATH),
		"Logger threaded file final cleanup failed"
	);
	printf("[PASS] Logger file threads\n");
	return 0;
}
