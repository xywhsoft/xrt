#include "../test.h"



static const char testIoFileOomPath[] = "test-io-file-oom.bin";



/* 对 ReaderOpen 的每个逻辑分配点逐一注入失败。 */
static void testIoReaderOpenOom(void)
{
	bool bCompleted = false;

	for ( uint64 i = 0; i < 16u; i++ ) {
		xreader* pReader;
		bool bTriggered;

		testRequire(xrtMemDebugFailAfter(i), "ReaderOpen OOM setup failed");
		pReader = xrtReaderOpen(testIoFileOomPath);
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( bTriggered ) {
			testRequire(
				(pReader == NULL) &&
				(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
				"ReaderOpen allocation failure was not propagated"
			);
			xrtClearError();
			continue;
		}
		testRequire(pReader != NULL, "ReaderOpen failed without injection");
		testRequire(xrtReaderDestroy(pReader), "ReaderOpen recovery close failed");
		bCompleted = true;
		break;
	}
	testRequire(bCompleted, "ReaderOpen OOM sweep did not complete");
}



/* 对 WriterOpen 的每个逻辑分配点逐一注入失败。 */
static void testIoWriterOpenOom(void)
{
	bool bCompleted = false;

	for ( uint64 i = 0; i < 16u; i++ ) {
		xwriter* pWriter;
		bool bTriggered;

		testRequire(xrtMemDebugFailAfter(i), "WriterOpen OOM setup failed");
		pWriter = xrtWriterOpen(testIoFileOomPath);
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( bTriggered ) {
			testRequire(
				(pWriter == NULL) &&
				(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
				"WriterOpen allocation failure was not propagated"
			);
			xrtClearError();
			continue;
		}
		testRequire(pWriter != NULL, "WriterOpen failed without injection");
		testRequire(xrtWriterDestroy(pWriter), "WriterOpen recovery close failed");
		bCompleted = true;
		break;
	}
	testRequire(bCompleted, "WriterOpen OOM sweep did not complete");
}



/* 验证 Take 构造失败保持文件槽和文件所有权。 */
static void testIoFileTakeOom(void)
{
	xfile File;
	xreader* pReader;
	xwriter* pWriter;

	File = xrtOpen(testIoFileOomPath, XFILE_READ);
	testRequire(File != NULL, "Reader take OOM setup failed");
	testRequire(xrtMemDebugFailAfter(0u), "Reader take OOM arm failed");
	pReader = xrtReaderTakeFile(&File);
	testRequire(
		(pReader == NULL) && (File != NULL) &&
		xrtMemDebugFailTriggered(),
		"failed Reader take consumed file ownership"
	);
	xrtMemDebugFailClear();
	xrtClearError();
	testRequire(xrtClose(File), "Reader take OOM file close failed");

	File = xrtOpen(testIoFileOomPath, XFILE_WRITE);
	testRequire(File != NULL, "Writer take OOM setup failed");
	testRequire(xrtMemDebugFailAfter(0u), "Writer take OOM arm failed");
	pWriter = xrtWriterTakeFile(&File);
	testRequire(
		(pWriter == NULL) && (File != NULL) &&
		xrtMemDebugFailTriggered(),
		"failed Writer take consumed file ownership"
	);
	xrtMemDebugFailClear();
	xrtClearError();
	testRequire(xrtClose(File), "Writer take OOM file close failed");
}



/* 运行文件适配器所有权与构造清理的 OOM 回归。 */
int main(void)
{
	xfile File = xrtOpen(
		testIoFileOomPath,
		XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE
	);

	testRequire(File != NULL, "IO file OOM test open failed");
	testRequire(
		xrtWriteFull(File, "data", 4u, NULL) && xrtClose(File),
		"IO file OOM test setup failed"
	);
	testIoFileTakeOom();
	testIoReaderOpenOom();
	testIoWriterOpenOom();
	testRequire(
		xrtFileDelete(testIoFileOomPath),
		"IO file OOM cleanup found a leaked handle"
	);
	testMemoryDebugDrain("IO file OOM test leaked memory");
	printf("[PASS] IO file OOM\n");
	return 0;
}
