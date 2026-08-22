#include "../test.h"



/* 短操作上下文用于验证 Full 循环、关闭次数和显式 Flush。 */
typedef struct testshortio {
	const unsigned char* Input;
	size_t InputSize;
	size_t InputPosition;
	unsigned char Output[32];
	size_t OutputSize;
	size_t CloseCount;
	size_t FlushCount;
} testshortio;



/* 每次最多读取两个字节。 */
static bool testShortRead(
	ptr pContext,
	ptr pBuffer,
	size_t iRequest,
	size_t* pRead
)
{
	testshortio* pIo = (testshortio*)pContext;
	size_t iRemain = pIo->InputSize - pIo->InputPosition;
	size_t iDone = iRemain < 2u ? iRemain : 2u;

	if ( iDone > iRequest ) {
		iDone = iRequest;
	}
	if ( iDone != 0u ) {
		memcpy(
			pBuffer,
			pIo->Input + pIo->InputPosition,
			iDone
		);
		pIo->InputPosition += iDone;
	}
	*pRead = iDone;
	return true;
}



/* 每次最多写入两个字节。 */
static bool testShortWrite(
	ptr pContext,
	const void* pBuffer,
	size_t iRequest,
	size_t* pWritten
)
{
	testshortio* pIo = (testshortio*)pContext;
	size_t iRemain = sizeof(pIo->Output) - pIo->OutputSize;
	size_t iDone = iRemain < 2u ? iRemain : 2u;

	if ( iDone > iRequest ) {
		iDone = iRequest;
	}
	if ( iDone != 0u ) {
		memcpy(pIo->Output + pIo->OutputSize, pBuffer, iDone);
		pIo->OutputSize += iDone;
	}
	*pWritten = iDone;
	return true;
}



/* 记录显式 Flush 调用。 */
static bool testShortFlush(ptr pContext)
{
	((testshortio*)pContext)->FlushCount++;
	return true;
}



/* 记录 Reader 或 Writer 的唯一关闭调用。 */
static bool testShortClose(ptr pContext)
{
	((testshortio*)pContext)->CloseCount++;
	return true;
}



/* 返回不合法读取计数以验证回调边界。 */
static bool testInvalidRead(
	ptr pContext,
	ptr pBuffer,
	size_t iRequest,
	size_t* pRead
)
{
	(void)pContext;
	(void)pBuffer;
	*pRead = iRequest + 1u;
	return true;
}



/* 不设置错误直接失败，用于验证 IO 层不会沿用旧错误。 */
static bool testSilentReadFailure(
	ptr pContext,
	ptr pBuffer,
	size_t iRequest,
	size_t* pRead
)
{
	(void)pContext;
	(void)pBuffer;
	(void)iRequest;
	(void)pRead;
	return false;
}



/* 返回零进展成功以验证 Writer 不会造成无限循环。 */
static bool testNoProgressWrite(
	ptr pContext,
	const void* pBuffer,
	size_t iRequest,
	size_t* pWritten
)
{
	(void)pContext;
	(void)pBuffer;
	(void)iRequest;
	*pWritten = 0u;
	return true;
}



/* 验证自定义回调、短操作、可选能力和销毁语义。 */
static void testIoCallbacks(void)
{
	static const unsigned char arrInput[] = "abcdef";
	testshortio Io;
	xreaderops ReaderOps;
	xwriterops WriterOps;
	xreader* pReader;
	xwriter* pWriter;
	unsigned char arrRead[sizeof(arrInput) - 1u];
	size_t iDone = 0;
	uint64 iValue = 0;

	memset(&Io, 0, sizeof(Io));
	Io.Input = arrInput;
	Io.InputSize = sizeof(arrInput) - 1u;
	memset(&ReaderOps, 0, sizeof(ReaderOps));
	ReaderOps.Read = testShortRead;
	ReaderOps.Close = testShortClose;
	pReader = xrtReaderCreate(&ReaderOps, &Io);
	testRequire(pReader != NULL, "custom Reader creation failed");
	testRequire(
		!xrtReaderCanSeek(pReader) && !xrtReaderCanSize(pReader),
		"custom Reader reported unavailable capabilities"
	);
	testRequire(
		xrtReaderReadFull(pReader, arrRead, sizeof(arrRead), &iDone) &&
		(iDone == sizeof(arrRead)) &&
		(memcmp(arrRead, arrInput, sizeof(arrRead)) == 0),
		"ReadFull did not join short reads"
	);
	testRequire(
		!xrtReaderTell(pReader, &iValue) &&
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED) &&
		(xrtErrorCode(xrtGetError()) == XIO_ERROR_TELL),
		"unsupported Reader tell error mismatch"
	);
	xrtClearError();
	testRequire(
		xrtReaderDestroy(pReader) && (Io.CloseCount == 1u),
		"Reader Close was not called exactly once"
	);

	memset(&WriterOps, 0, sizeof(WriterOps));
	WriterOps.Write = testShortWrite;
	WriterOps.Flush = testShortFlush;
	WriterOps.Close = testShortClose;
	pWriter = xrtWriterCreate(&WriterOps, &Io);
	testRequire(pWriter != NULL, "custom Writer creation failed");
	testRequire(
		xrtWriterWriteFull(pWriter, arrInput, sizeof(arrRead), &iDone) &&
		(iDone == sizeof(arrRead)) &&
		(Io.OutputSize == sizeof(arrRead)) &&
		(memcmp(Io.Output, arrInput, sizeof(arrRead)) == 0),
		"WriteFull did not join short writes"
	);
	testRequire(
		xrtWriterFlush(pWriter) && (Io.FlushCount == 1u),
		"explicit Writer Flush failed"
	);
	testRequire(
		xrtWriterDestroy(pWriter) &&
		(Io.CloseCount == 2u) && (Io.FlushCount == 1u),
		"Writer destroy flushed implicitly or missed Close"
	);
}



/* 验证内存 Reader 的定位、EOF 锁定和部分读取量。 */
static void testIoMemoryReader(void)
{
	static const unsigned char arrInput[] = "abcdef";
	xreader* pReader = xrtReaderFromMemory(
		(xbytesview){ arrInput, sizeof(arrInput) - 1u }
	);
	unsigned char arrRead[8];
	size_t iDone = 0;
	uint64 iValue = 0;

	testRequire(pReader != NULL, "memory Reader creation failed");
	testRequire(
		xrtReaderCanSeek(pReader) && xrtReaderCanSize(pReader) &&
		xrtReaderSize(pReader, &iValue) && (iValue == 6u),
		"memory Reader capability or size mismatch"
	);
	testRequire(
		xrtReaderReadFull(pReader, arrRead, 3u, &iDone) &&
		(iDone == 3u) && (memcmp(arrRead, "abc", 3u) == 0),
		"memory Reader prefix mismatch"
	);
	testRequire(
		xrtReaderSeek(pReader, -1, XSEEK_END, &iValue) &&
		(iValue == 5u) &&
		xrtReaderRead(pReader, arrRead, 2u, &iDone) &&
		(iDone == 1u) && (arrRead[0] == 'f'),
		"memory Reader end-relative seek failed"
	);
	testRequire(
		xrtReaderRead(pReader, arrRead, 1u, &iDone) &&
		(iDone == 0u) && xrtReaderEOF(pReader),
		"memory Reader EOF was not locked"
	);
	testRequire(
		xrtReaderSeek(pReader, 0, XSEEK_START, NULL) &&
		!xrtReaderEOF(pReader),
		"successful seek did not clear EOF"
	);
	memset(arrRead, 0xA5, sizeof(arrRead));
	testRequire(
		!xrtReaderReadFull(pReader, arrRead, sizeof(arrRead), &iDone) &&
		(iDone == 6u) && (arrRead[6] == 0xA5) &&
		(xrtErrorCode(xrtGetError()) == XIO_ERROR_EOF),
		"ReadFull premature EOF contract failed"
	);
	xrtClearError();
	testRequire(xrtReaderDestroy(pReader), "memory Reader destroy failed");
}



/* 验证内存 Writer 的逻辑大小、稀疏写、重叠写和容量短写。 */
static void testIoMemoryWriter(void)
{
	unsigned char arrOutput[8];
	xwriter* pWriter;
	size_t iDone = 0;
	uint64 iValue = 0;

	memset(arrOutput, 0xA5, sizeof(arrOutput));
	pWriter = xrtWriterFromMemory(arrOutput, sizeof(arrOutput));
	testRequire(pWriter != NULL, "memory Writer creation failed");
	testRequire(
		xrtWriterWriteFull(pWriter, "abc", 3u, NULL) &&
		xrtWriterSeek(pWriter, 5, XSEEK_START, NULL) &&
		xrtWriterWriteFull(pWriter, "z", 1u, NULL) &&
		xrtWriterTell(pWriter, &iValue) && (iValue == 6u) &&
		xrtWriterSize(pWriter, &iValue) && (iValue == 6u),
		"memory Writer sparse positioning failed"
	);
	testRequire(
		(arrOutput[0] == 'a') && (arrOutput[1] == 'b') &&
		(arrOutput[2] == 'c') && (arrOutput[3] == 0u) &&
		(arrOutput[4] == 0u) && (arrOutput[5] == 'z'),
		"memory Writer sparse contents mismatch"
	);
	testRequire(
		xrtWriterSeek(pWriter, 0, XSEEK_START, NULL) &&
		xrtWriterWriteFull(pWriter, "abcdef", 6u, NULL) &&
		xrtWriterSeek(pWriter, 2, XSEEK_START, NULL) &&
		xrtWriterWriteFull(pWriter, arrOutput, 4u, NULL) &&
		(memcmp(arrOutput, "ababcd", 6u) == 0),
		"memory Writer overlapping write failed"
	);
	testRequire(
		xrtWriterSeek(pWriter, 7, XSEEK_START, NULL) &&
		xrtWriterWrite(pWriter, "XY", 2u, &iDone) &&
		(iDone == 1u) && (arrOutput[7] == 'X'),
		"memory Writer capacity short write failed"
	);
	testRequire(xrtWriterDestroy(pWriter), "memory Writer destroy failed");
}



/* 验证三种复制终止条件和丢弃 Writer 统计。 */
static void testIoCopy(void)
{
	static const unsigned char arrInput[] = "abcdef";
	unsigned char arrOutput[8];
	xreader* pReader;
	xwriter* pWriter;
	uint64 iCopied = 0;
	uint64 iSize = 0;

	memset(arrOutput, 0, sizeof(arrOutput));
	pReader = xrtReaderFromMemory((xbytesview){ arrInput, 6u });
	pWriter = xrtWriterFromMemory(arrOutput, sizeof(arrOutput));
	testRequire(
		xrtReaderCopy(pReader, pWriter, &iCopied) &&
		(iCopied == 6u) && (memcmp(arrOutput, arrInput, 6u) == 0),
		"unbounded Reader copy failed"
	);
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);

	pReader = xrtReaderFromMemory((xbytesview){ arrInput, 6u });
	pWriter = xrtWriterDiscard();
	testRequire(
		xrtReaderCopyN(pReader, pWriter, 3u, &iCopied) &&
		(iCopied == 3u) && xrtWriterSize(pWriter, &iSize) &&
		(iSize == 3u),
		"exact Reader copy failed"
	);
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);

	pReader = xrtReaderFromMemory((xbytesview){ arrInput, 6u });
	pWriter = xrtWriterDiscard();
	testRequire(
		!xrtReaderCopyN(pReader, pWriter, 8u, &iCopied) &&
		(iCopied == 6u) &&
		(xrtErrorCode(xrtGetError()) == XIO_ERROR_EOF),
		"exact Reader copy did not report early EOF"
	);
	xrtClearError();
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);

	pReader = xrtReaderFromMemory((xbytesview){ arrInput, 6u });
	pWriter = xrtWriterDiscard();
	testRequire(
		!xrtReaderCopyLimit(pReader, pWriter, 5u, &iCopied) &&
		(iCopied == 5u) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) == XIO_ERROR_LIMIT),
		"limited Reader copy did not reject extra input"
	);
	xrtClearError();
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);

	pReader = xrtReaderFromMemory((xbytesview){ arrInput, 6u });
	pWriter = xrtWriterDiscard();
	testRequire(
		xrtReaderCopyLimit(pReader, pWriter, 6u, &iCopied) &&
		(iCopied == 6u),
		"limited Reader copy rejected exact input"
	);
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);
}



/* 验证恶意回调和输出别名不会破坏对象或产生死循环。 */
static void testIoContracts(void)
{
	xreaderops ReaderOps;
	xwriterops WriterOps;
	xreader* pReader;
	xwriter* pWriter;
	xerror* pStale;
	union {
		size_t Count;
		unsigned char Data[16];
	} Alias;
	size_t iDone = 0;

	pStale = xrtErrorCreate(XERR_VALUE, "test.stale", 7, "stale error");
	testRequire(pStale != NULL, "stale error setup failed");
	xrtSetError(pStale);
	xrtErrorFree(pStale);
	memset(&ReaderOps, 0, sizeof(ReaderOps));
	ReaderOps.Read = testSilentReadFailure;
	pReader = xrtReaderCreate(&ReaderOps, NULL);
	testRequire(
		(pReader != NULL) &&
		!xrtReaderRead(pReader, Alias.Data, 1u, &iDone) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.io") == 0) &&
		(xrtErrorCode(xrtGetError()) == XIO_ERROR_READ),
		"silent Reader failure reused a stale error"
	);
	xrtClearError();
	xrtReaderDestroy(pReader);

	memset(&ReaderOps, 0, sizeof(ReaderOps));
	ReaderOps.Read = testInvalidRead;
	pReader = xrtReaderCreate(&ReaderOps, NULL);
	testRequire(
		(pReader != NULL) &&
		!xrtReaderRead(pReader, Alias.Data, 1u, &iDone) &&
		(xrtErrorKind(xrtGetError()) == XERR_INTERNAL) &&
		(xrtErrorCode(xrtGetError()) == XIO_ERROR_CALLBACK),
		"invalid Reader callback count was accepted"
	);
	xrtClearError();
	xrtReaderDestroy(pReader);

	memset(&WriterOps, 0, sizeof(WriterOps));
	WriterOps.Write = testNoProgressWrite;
	pWriter = xrtWriterCreate(&WriterOps, NULL);
	testRequire(
		(pWriter != NULL) &&
		!xrtWriterWrite(pWriter, "x", 1u, &iDone) &&
		(xrtErrorCode(xrtGetError()) == XIO_ERROR_NO_PROGRESS),
		"zero-progress Writer callback was accepted"
	);
	xrtClearError();
	xrtWriterDestroy(pWriter);

	pReader = xrtReaderFromMemory(XRT_BYTES_LITERAL("abc"));
	testRequire(pReader != NULL, "alias Reader setup failed");
	testRequire(
		!xrtReaderRead(
			pReader,
			Alias.Data,
			sizeof(Alias.Data),
			&Alias.Count
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"Reader accepted count output overlapping data"
	);
	xrtClearError();
	xrtReaderDestroy(pReader);
}



/* 运行通用同步 IO 的完整契约回归。 */
int main(void)
{
	testIoCallbacks();
	testIoMemoryReader();
	testIoMemoryWriter();
	testIoCopy();
	testIoContracts();
	printf("[PASS] IO core\n");
	return 0;
}
