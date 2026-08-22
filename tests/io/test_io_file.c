#include "../test.h"



/* 使用 UTF-8 字节路径保留旧 Stream 测试的跨平台路径边界。 */
static const char testIoFilePath[] =
	"test-io-\xE4\xB8\xAD\xE6\x96\x87.bin";



/* 验证路径便捷构造器、二进制内容、显式 Flush 和追加语义。 */
static void testIoFileOpen(void)
{
	static const unsigned char arrData[] = { 'A', 0, 'B', 0xFFu };
	unsigned char arrRead[sizeof(arrData) + 1u];
	xwriter* pWriter;
	xreader* pReader;
	size_t iDone = 0;
	uint64 iValue = 0;

	(void)xrtFileDelete(testIoFilePath);
	xrtClearError();
	pWriter = xrtWriterOpen(testIoFilePath);
	testRequire(pWriter != NULL, "file Writer open failed");
	testRequire(
		xrtWriterWriteFull(pWriter, arrData, sizeof(arrData), &iDone) &&
		(iDone == sizeof(arrData)) &&
		xrtWriterTell(pWriter, &iValue) &&
		(iValue == sizeof(arrData)) &&
		xrtWriterSize(pWriter, &iValue) &&
		(iValue == sizeof(arrData)),
		"file Writer content, position or size mismatch"
	);
	testRequire(xrtWriterFlush(pWriter), "file Writer explicit Flush failed");
	testRequire(xrtWriterDestroy(pWriter), "file Writer destroy failed");

	pWriter = xrtWriterOpenAppend(testIoFilePath);
	testRequire(
		(pWriter != NULL) &&
		xrtWriterWriteFull(pWriter, "Z", 1u, NULL) &&
		xrtWriterDestroy(pWriter),
		"append file Writer failed"
	);

	pReader = xrtReaderOpen(testIoFilePath);
	testRequire(pReader != NULL, "file Reader open failed");
	memset(arrRead, 0xA5, sizeof(arrRead));
	testRequire(
		xrtReaderSize(pReader, &iValue) && (iValue == sizeof(arrRead)) &&
		xrtReaderReadFull(pReader, arrRead, sizeof(arrRead), &iDone) &&
		(iDone == sizeof(arrRead)) &&
		(memcmp(arrRead, arrData, sizeof(arrData)) == 0) &&
		(arrRead[sizeof(arrData)] == 'Z'),
		"file Reader binary content mismatch"
	);
	testRequire(
		xrtReaderRead(pReader, arrRead, 1u, &iDone) &&
		(iDone == 0u) && xrtReaderEOF(pReader),
		"file Reader EOF mismatch"
	);
	testRequire(xrtReaderDestroy(pReader), "file Reader destroy failed");
}



/* 验证借用适配器不关闭文件，Take 适配器原子清空所有权槽。 */
static void testIoFileOwnership(void)
{
	xfile File;
	xreader* pReader;
	xwriter* pWriter;
	uint64 iPosition = 0;

	File = xrtOpen(testIoFilePath, XFILE_READ | XFILE_WRITE);
	testRequire(File != NULL, "file ownership setup failed");
	pReader = xrtReaderFromFile(File);
	testRequire(
		(pReader != NULL) && xrtReaderDestroy(pReader) &&
		xrtSeek(File, 0, XSEEK_START, &iPosition),
		"borrowed Reader closed the file"
	);
	pWriter = xrtWriterFromFile(File);
	testRequire(
		(pWriter != NULL) && xrtWriterDestroy(pWriter) &&
		xrtSeek(File, 0, XSEEK_END, &iPosition),
		"borrowed Writer closed the file"
	);
	testRequire(xrtClose(File), "borrowed adapter file close failed");

	File = xrtOpen(testIoFilePath, XFILE_READ);
	testRequire(File != NULL, "Reader take setup failed");
	pReader = xrtReaderTakeFile(&File);
	testRequire(
		(pReader != NULL) && (File == NULL) &&
		xrtReaderDestroy(pReader),
		"Reader take did not consume and close the file"
	);

	File = xrtOpen(testIoFilePath, XFILE_WRITE);
	testRequire(File != NULL, "Writer take setup failed");
	pWriter = xrtWriterTakeFile(&File);
	testRequire(
		(pWriter != NULL) && (File == NULL) &&
		xrtWriterDestroy(pWriter),
		"Writer take did not consume and close the file"
	);
}



/* 验证适配器在构造时拒绝文件访问模式不匹配。 */
static void testIoFileAccess(void)
{
	xfile File = xrtOpen(testIoFilePath, XFILE_READ);
	xwriter* pWriter;

	testRequire(File != NULL, "file access test open failed");
	pWriter = xrtWriterFromFile(File);
	testRequire(
		(pWriter == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtErrorCode(xrtGetError()) == XIO_ERROR_WRITE),
		"Writer accepted a read-only file"
	);
	xrtClearError();
	testRequire(xrtClose(File), "file access test close failed");
}



/* 运行文件 IO 适配器回归并清理临时文件。 */
int main(void)
{
	testIoFileOpen();
	testIoFileOwnership();
	testIoFileAccess();
	testRequire(xrtFileDelete(testIoFilePath), "IO test file cleanup failed");
	printf("[PASS] IO file\n");
	return 0;
}
