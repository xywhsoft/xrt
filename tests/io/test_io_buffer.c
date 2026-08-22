#include "../test.h"



/* 验证 Buffer Reader、接管语义和 ReadAll 硬上限。 */
static void testIoBufferReader(void)
{
	xbuffer* pBuffer = xrtBufferFrom(XRT_BYTES_LITERAL("abcdef"));
	xbuffer* pTaken;
	xbuffer* pCopy;
	xreader* pReader;
	unsigned char arrRead[4];
	size_t iDone = 0;

	testRequire(pBuffer != NULL, "Buffer Reader setup failed");
	pReader = xrtReaderFromBuffer(pBuffer);
	testRequire(pReader != NULL, "borrowed Buffer Reader creation failed");
	testRequire(
		xrtReaderReadFull(pReader, arrRead, 3u, &iDone) &&
		(iDone == 3u) && (memcmp(arrRead, "abc", 3u) == 0),
		"borrowed Buffer Reader contents mismatch"
	);
	testRequire(xrtReaderDestroy(pReader), "borrowed Buffer Reader destroy failed");
	testRequire(
		(pBuffer->Size == 6u) && (memcmp(pBuffer->Data, "abcdef", 6u) == 0),
		"borrowed Buffer Reader consumed the Buffer"
	);

	pTaken = pBuffer;
	pReader = xrtReaderTakeBuffer(&pTaken);
	testRequire(
		(pReader != NULL) && (pTaken == NULL),
		"Buffer Reader take did not clear the source slot"
	);
	pCopy = xrtReaderReadAll(pReader, 6u);
	testRequire(
		(pCopy != NULL) && (pCopy->Size == 6u) &&
		(memcmp(pCopy->Data, "abcdef", 6u) == 0),
		"Buffer Reader ReadAll mismatch"
	);
	xrtBufferDestroy(pCopy);
	testRequire(xrtReaderDestroy(pReader), "owned Buffer Reader destroy failed");

	pReader = xrtReaderFromMemory(XRT_BYTES_LITERAL("abcdef"));
	pCopy = xrtReaderReadAll(pReader, 5u);
	testRequire(
		(pCopy == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) == XIO_ERROR_LIMIT),
		"Buffer ReadAll limit did not reject extra input"
	);
	xrtClearError();
	xrtReaderDestroy(pReader);

	pReader = xrtReaderFromMemory(XRT_BYTES_LITERAL(""));
	pCopy = xrtReaderReadAll(pReader, 0u);
	testRequire(
		(pCopy != NULL) && (pCopy->Size == 0u),
		"zero-limit Buffer ReadAll rejected empty input"
	);
	xrtBufferDestroy(pCopy);
	xrtReaderDestroy(pReader);
}



/* 验证 Buffer Writer 从末尾追加、稀疏覆盖和自别名写入。 */
static void testIoBufferWriter(void)
{
	xbuffer Buffer;
	xwriter* pWriter;
	uint64 iValue = 0;

	testRequire(xrtBufferInit(&Buffer), "Buffer Writer init failed");
	testRequire(
		xrtBufferAppend(&Buffer, XRT_BYTES_LITERAL("abc")),
		"Buffer Writer setup append failed"
	);
	pWriter = xrtWriterFromBuffer(&Buffer);
	testRequire(pWriter != NULL, "Buffer Writer creation failed");
	testRequire(
		xrtWriterTell(pWriter, &iValue) && (iValue == 3u) &&
		xrtWriterWriteFull(pWriter, "d", 1u, NULL),
		"Buffer Writer did not start at the end"
	);
	testRequire(
		xrtWriterSeek(pWriter, 6, XSEEK_START, NULL) &&
		xrtWriterWriteFull(pWriter, "z", 1u, NULL) &&
		(Buffer.Size == 7u) && (Buffer.Data[4] == 0u) &&
		(Buffer.Data[5] == 0u) && (Buffer.Data[6] == 'z'),
		"Buffer Writer sparse write failed"
	);
	testRequire(
		xrtWriterSeek(pWriter, 1, XSEEK_START, NULL) &&
		xrtWriterWriteFull(pWriter, Buffer.Data, 3u, NULL) &&
		(memcmp(Buffer.Data, "aabc", 4u) == 0),
		"Buffer Writer self-alias write failed"
	);
	testRequire(
		xrtWriterWriteBuffer(pWriter, &Buffer),
		"WriterWriteBuffer failed"
	);
	testRequire(xrtWriterDestroy(pWriter), "Buffer Writer destroy failed");
	xrtBufferUnit(&Buffer);
}



/* 运行 Buffer IO 适配器回归。 */
int main(void)
{
	testIoBufferReader();
	testIoBufferWriter();
	printf("[PASS] IO buffer\n");
	return 0;
}
