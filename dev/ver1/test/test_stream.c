#include "../xrt.h"

#include <stdio.h>
#include <string.h>



typedef struct {
	const unsigned char* Data;
	size_t Size;
	size_t Position;
	int* CloseCount;
} short_reader_context;



static int check(int bCondition, const char* sMessage)
{
	if ( bCondition ) { return 1; }
	fprintf(stderr, "test_stream: %s\n", sMessage);
	return 0;
}



// 每次最多返回两个字节，用于验证 ReadExact 能处理短读。
static bool shortRead(ptr pContext, ptr pBuffer, uint64 iRequest, uint64* iRead)
{
	short_reader_context* pReader = (short_reader_context*)pContext;
	size_t iRemain = pReader->Size - pReader->Position;
	size_t iDone = iRemain < 2u ? iRemain : 2u;
	if ( iDone > iRequest ) { iDone = (size_t)iRequest; }
	if ( iDone != 0u ) {
		memcpy(pBuffer, pReader->Data + pReader->Position, iDone);
		pReader->Position += iDone;
	}
	if ( iRead ) { *iRead = iDone; }
	return TRUE;
}



static bool shortClose(ptr pContext)
{
	short_reader_context* pReader = (short_reader_context*)pContext;
	(*pReader->CloseCount)++;
	xrtFree(pReader);
	return TRUE;
}



static int testMemoryAndCallbacks(void)
{
	const char sText[] = "abcdef";
	char sOut[8] = { 0 };
	uint64 iValue = 0;
	xreader pReader = xrtReaderFromMemory(sText, 6u);
	int bOk = 1;
	bOk &= check(pReader != NULL, "memory reader create failed");
	bOk &= check(xrtReaderCanSeek(pReader), "memory reader must be seekable");
	bOk &= check(xrtReaderSize(pReader, &iValue) && iValue == 6u, "memory size mismatch");
	bOk &= check(xrtReaderReadExact(pReader, sOut, 3u) && memcmp(sOut, "abc", 3u) == 0,
		"memory exact read mismatch");
	bOk &= check(xrtReaderTell(pReader, &iValue) && iValue == 3u, "memory tell mismatch");
	bOk &= check(xrtReaderSeek(pReader, -1, XRT_SEEK_END, &iValue) && iValue == 5u,
		"memory seek from end failed");
	bOk &= check(xrtReaderRead(pReader, sOut, 2u, &iValue) && iValue == 1u && sOut[0] == 'f',
		"memory short tail read failed");
	bOk &= check(xrtReaderRead(pReader, sOut, 1u, &iValue) && iValue == 0u && xrtReaderEOF(pReader),
		"memory EOF was not reported");
	bOk &= check(xrtReaderDestroy(pReader), "memory reader close failed");

	{
		int iCloseCount = 0;
		short_reader_context* pContext = (short_reader_context*)xrtMalloc(sizeof(short_reader_context));
		xreader_ops stuOps;
		memset(&stuOps, 0, sizeof(stuOps));
		pContext->Data = (const unsigned char*)sText;
		pContext->Size = 6u;
		pContext->Position = 0u;
		pContext->CloseCount = &iCloseCount;
		stuOps.Read = shortRead;
		stuOps.Close = shortClose;
		pReader = xrtReaderCreate(&stuOps, pContext);
		memset(sOut, 0, sizeof(sOut));
		bOk &= check(pReader != NULL && !xrtReaderCanSeek(pReader), "callback reader contract mismatch");
		bOk &= check(xrtReaderReadExact(pReader, sOut, 6u) && memcmp(sOut, sText, 6u) == 0,
			"ReadExact did not join short reads");
		bOk &= check(xrtReaderDestroy(pReader) && iCloseCount == 1, "callback close count mismatch");
	}
	return bOk;
}



static int testBuffer(void)
{
	xbuffer pBuffer = xrtBufferFrom((ptr)"abc", 3u);
	xwriter pWriter;
	xreader pReader;
	xbuffer pCopy;
	uint64 iPosition = 0;
	int bOk = 1;
	bOk &= check(pBuffer != NULL, "buffer create failed");
	pWriter = xrtWriterFromBuffer(pBuffer, XRT_STREAM_BORROW);
	bOk &= check(pWriter != NULL, "buffer writer create failed");
	bOk &= check(xrtWriterTell(pWriter, &iPosition) && iPosition == 3u, "buffer writer must append initially");
	bOk &= check(xrtWriterSeek(pWriter, 5, XRT_SEEK_SET, NULL), "buffer sparse seek failed");
	bOk &= check(xrtWriterWriteAll(pWriter, "z", 1u), "buffer sparse write failed");
	bOk &= check(xrtWriterSeek(pWriter, 1, XRT_SEEK_SET, NULL), "buffer overwrite seek failed");
	bOk &= check(xrtWriterWriteAll(pWriter, "X", 1u), "buffer overwrite failed");
	bOk &= check(xrtWriterDestroy(pWriter), "buffer writer close failed");
	bOk &= check(pBuffer->Length == 6u && pBuffer->Buffer[0] == 'a' && pBuffer->Buffer[1] == 'X' &&
		pBuffer->Buffer[2] == 'c' && pBuffer->Buffer[3] == 0 && pBuffer->Buffer[4] == 0 &&
		pBuffer->Buffer[5] == 'z', "buffer sparse contents mismatch");

	pReader = xrtReaderFromBuffer(pBuffer, XRT_STREAM_BORROW);
	pCopy = xrtReaderReadAll(pReader, 6u);
	bOk &= check(pCopy != NULL && pCopy->Length == 6u && memcmp(pCopy->Buffer, pBuffer->Buffer, 6u) == 0,
		"buffer ReadAll mismatch");
	xrtBufferDestroy(pCopy);
	xrtReaderDestroy(pReader);

	pReader = xrtReaderFromBuffer(pBuffer, XRT_STREAM_BORROW);
	pCopy = xrtReaderReadAll(pReader, 5u);
	bOk &= check(pCopy == NULL, "ReadAll limit was not enforced");
	xrtReaderDestroy(pReader);
	xrtBufferDestroy(pBuffer);
	return bOk;
}



static int testFile(void)
{
	const char* sPath = "test_stream_中文.bin";
	const char sData[] = { 'A', 0, 'B', (char)0xFF };
	char sOut[sizeof(sData)] = { 0 };
	xwriter pWriter;
	xreader pReader;
	uint64 iValue = 0;
	int bOk = 1;
	if ( xrtFileExists((str)sPath) ) { xrtFileDelete((str)sPath); }
	pWriter = xrtWriterOpen((str)sPath, TRUE);
	bOk &= check(pWriter != NULL, "UTF-8 path writer open failed");
	bOk &= check(xrtWriterWriteAll(pWriter, sData, sizeof(sData)), "file write failed");
	bOk &= check(xrtWriterTell(pWriter, &iValue) && iValue == sizeof(sData), "file tell mismatch");
	bOk &= check(xrtWriterDestroy(pWriter), "file writer close failed");

	pReader = xrtReaderOpen((str)sPath);
	bOk &= check(pReader != NULL, "UTF-8 path reader open failed");
	bOk &= check(xrtReaderSize(pReader, &iValue) && iValue == sizeof(sData), "file size mismatch");
	bOk &= check(xrtReaderReadExact(pReader, sOut, sizeof(sOut)) && memcmp(sOut, sData, sizeof(sData)) == 0,
		"binary file contents mismatch");
	bOk &= check(xrtReaderRead(pReader, sOut, 1u, &iValue) && iValue == 0u && xrtReaderEOF(pReader),
		"file EOF mismatch");
	bOk &= check(xrtReaderDestroy(pReader), "file reader close failed");
	bOk &= check(xrtFileDelete((str)sPath), "temporary file delete failed");
	return bOk;
}



static int testArchivePath(void)
{
	return check(xrtPathIsSafeArchiveEntry((str)"folder/file.txt", FALSE) &&
		xrtPathIsSafeArchiveEntry((str)"目录/内容.txt", FALSE) &&
		xrtPathIsSafeArchiveEntry((str)"folder/", TRUE) &&
		!xrtPathIsSafeArchiveEntry((str)"../escape.txt", FALSE) &&
		!xrtPathIsSafeArchiveEntry((str)"C:/escape.txt", FALSE) &&
		!xrtPathIsSafeArchiveEntry((str)"folder\\escape.txt", FALSE) &&
		!xrtPathIsSafeArchiveEntry((str)"con.txt", FALSE) &&
		!xrtPathIsSafeArchiveEntry((str)"folder./file", FALSE),
		"archive path validation failed");
}



int main(void)
{
	int bOk;
	if ( xrtInit() == NULL ) {
		fprintf(stderr, "test_stream: xrtInit failed\n");
		return 1;
	}
	bOk = testMemoryAndCallbacks();
	bOk &= testBuffer();
	bOk &= testFile();
	bOk &= testArchivePath();
	xrtUnit();
	if ( bOk ) { puts("test_stream: PASS"); }
	return bOk ? 0 : 1;
}
