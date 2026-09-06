/*
 * 范例：io/stream_tour —— Reader/Writer 全接口（内存/文件/缓冲/自定义）
 * ----------------------------------------------------------------
 * 演示 API：
 *   【Reader 读】  xrtReaderRead（部分读）/ ReadAll（读到新 Buffer）/
 *                  EOF / CanSeek / CanSize / Seek / Tell / Size /
 *                  CopyN（精确复制）/ CopyLimit（硬上限复制）
 *   【Reader 构建】 xrtReaderCreate（自定义回调表）/
 *                  FromBuffer / TakeBuffer / FromFile / TakeFile
 *   【LineReader】  xrtLineReaderCreate（非接管形态）
 *   【Writer 写】  xrtWriterWrite（部分写）/ WriteBuffer /
 *                  CanSeek / CanSize / Seek / Tell / Size / Flush
 *   【Writer 构建】 xrtWriterCreate（自定义回调表）/ Discard（丢弃计数）/
 *                  FromFile / TakeFile / OpenAppend
 * 模块宏：XRT_MODULE_IO + XRT_MODULE_IO_FILE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/io/stream_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   io: read=4 seek-tell=6 eof=0 readall=10
 *   io: copy-n=5 copy-limit=4
 *   io: custom reader/writer bytes=9 buffer take/discard/writebuffer ok file append lines=2
 *   io: custom close=1
 *
 * Reader/Writer 是统一抽象：内存/文件/缓冲/丢弃/自定义回调
 *   共享同一套定位与复制语义；Take 系列原子接管来源槽。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

/* 自定义 Reader 上下文：借用一段内存。 */
typedef struct examplemem {
	cbytes Data;
	size_t Size;
	size_t Pos;
	int Closes;
} examplemem;

static bool exampleRead(ptr pContext, ptr pBuffer, size_t iRequest,
	size_t* pRead)
{
	examplemem* pMem = (examplemem*)pContext;
	size_t iLeft = pMem->Size - pMem->Pos;
	size_t iGive = iRequest < iLeft ? iRequest : iLeft;

	memcpy(pBuffer, pMem->Data + pMem->Pos, iGive);
	pMem->Pos = pMem->Pos + iGive;
	*pRead = iGive;
	return true;
}

static bool exampleSeekMem(ptr pContext, int64 iOffset,
	xseek Origin, uint64* pPosition)
{
	examplemem* pMem = (examplemem*)pContext;
	uint64 uBase = Origin == XSEEK_START ? 0u :
		(Origin == XSEEK_END ? pMem->Size : pMem->Pos);
	uint64 uTarget = uBase + (uint64)iOffset;

	if ( uTarget > pMem->Size ) {
		return false;
	}
	pMem->Pos = (size_t)uTarget;
	*pPosition = uTarget;
	return true;
}

static bool exampleTellMem(ptr pContext, uint64* pPosition)
{
	*pPosition = ((examplemem*)pContext)->Pos;
	return true;
}

static bool exampleSizeMem(ptr pContext, uint64* pSize)
{
	*pSize = ((examplemem*)pContext)->Size;
	return true;
}

static bool exampleCloseMem(ptr pContext)
{
	((examplemem*)pContext)->Closes = 1;
	return true;
}

/* 自定义 Writer 上下文：累计字节数。 */
typedef struct examplesink {
	uint64 Written;
	uint64 Pos;
	int Flushes;
	int Closes;
} examplesink;

static bool exampleWrite(ptr pContext, const void* pData,
	size_t iSize, size_t* pWritten)
{
	examplesink* pSink = (examplesink*)pContext;

	(void)pData;
	pSink->Written = pSink->Written + iSize;
	pSink->Pos = pSink->Pos + iSize;
	*pWritten = iSize;
	return true;
}

static bool exampleFlushSink(ptr pContext)
{
	((examplesink*)pContext)->Flushes = 1;
	return true;
}

static bool exampleCloseSink(ptr pContext)
{
	((examplesink*)pContext)->Closes = 1;
	return true;
}

int main(void)
{
	static const char sText[] = "0123456789";
	xreader* pReader = NULL;
	xreader* pCustom = NULL;
	xwriter* pWriter = NULL;
	xwriter* pSink = NULL;
	xwriter* pAppend = NULL;
	xlinereader* pLines = NULL;
	xbuffer* pBuffer = NULL;
	xbuffer* pReadAll = NULL;
	xfile File = NULL;
	examplemem Mem;
	examplesink Sink;
	xreaderops ReaderOps;
	xwriterops WriterOps;
	char Out[16];
	uint64 uPos = 0;
	uint64 uSize = 0;
	uint64 uCopied = 0;
	size_t iRead = 0;
	xlineview Line;
	int iLines = 0;
	int iResult = 1;

	/* ---- 内存 Reader：读/定位/EOF/ReadAll ---- */
	pReader = xrtReaderFromMemory((xbytesview) {
		(cbytes)sText, sizeof(sText) - 1u
	});
	if ( (pReader == NULL) ||
		!xrtReaderCanSeek(pReader) ||
		!xrtReaderCanSize(pReader) ||
		!xrtReaderSize(pReader, &uSize) ||
		(uSize != 10u) ||
		!xrtReaderRead(pReader, Out, 4u, &iRead) ||
		(iRead != 4u) || (memcmp(Out, "0123", 4u) != 0) ||
		!xrtReaderTell(pReader, &uPos) || (uPos != 4u) ) {
		goto Cleanup;
	}
	/* Seek 回 2 后再读 4 字节 → "2345"，Tell=6。 */
	if ( !xrtReaderSeek(pReader, 2, XSEEK_START, &uPos) ||
		!xrtReaderRead(pReader, Out, 4u, &iRead) ||
		(memcmp(Out, "2345", 4u) != 0) ||
		!xrtReaderTell(pReader, &uPos) || (uPos != 6u) ||
		xrtReaderEOF(pReader) ) {
		goto Cleanup;
	}
	printf("io: read=4 seek-tell=%llu eof=0", (unsigned long long)uPos);
	/* ReadAll：从头读全部到新 Buffer。 */
	if ( !xrtReaderSeek(pReader, 0, XSEEK_START, &uPos) ) {
		goto Cleanup;
	}
	pReadAll = xrtReaderReadAll(pReader, 64u);
	if ( (pReadAll == NULL) ||
		(xrtBufferView(pReadAll).Size != 10u) ) {
		goto Cleanup;
	}
	printf(" readall=%zu\n", xrtBufferView(pReadAll).Size);
	/* 成功 Seek 会清除 EOF，直接复用同一个 Reader。 */
	if ( !xrtReaderEOF(pReader) ||
		!xrtReaderSeek(pReader, 0, XSEEK_START, &uPos) ||
		xrtReaderEOF(pReader) ) {
		goto Cleanup;
	}

	/* ---- 复制族：精确 N 与硬上限；成功也核对计数出参。 ---- */
	pWriter = xrtWriterFromMemory(Out, sizeof(Out));
	if ( (pWriter == NULL) ||
		!xrtReaderCopyN(pReader, pWriter, 5u, &uCopied) ||
		(uCopied != 5u) || (memcmp(Out, sText, 5u) != 0) ||
		!xrtReaderSeek(pReader, 0, XSEEK_START, &uPos) ||
		!xrtWriterSeek(pWriter, 0, XSEEK_START, &uPos) ||
		!xrtReaderCopyLimit(pReader, pWriter, 64u, &uCopied) ||
		(uCopied != 10u) || (memcmp(Out, sText, 10u) != 0) ) {
		goto Cleanup;
	}
	/* 超限是错误而非成功截断；出参记录已经复制的 4 字节。 */
	if ( !xrtReaderSeek(pReader, 0, XSEEK_START, &uPos) ||
		xrtReaderEOF(pReader) ||
		!xrtWriterSeek(pWriter, 0, XSEEK_START, &uPos) ||
		xrtReaderCopyLimit(pReader, pWriter, 4u, &uCopied) ||
		(uCopied != 4u) ) {
		goto Cleanup;
	}
	printf("io: copy-n=5 copy-limit=%llu\n",
		(unsigned long long)uCopied);

	/* ---- 自定义回调表 Reader + Writer ---- */
	memset(&Mem, 0, sizeof(Mem));
	Mem.Data = (cbytes)sText;
	Mem.Size = 9u;
	memset(&Sink, 0, sizeof(Sink));
	memset(&ReaderOps, 0, sizeof(ReaderOps));
	ReaderOps.Read = exampleRead;
	ReaderOps.Seek = exampleSeekMem;
	ReaderOps.Tell = exampleTellMem;
	ReaderOps.Size = exampleSizeMem;
	ReaderOps.Close = exampleCloseMem;
	memset(&WriterOps, 0, sizeof(WriterOps));
	WriterOps.Write = exampleWrite;
	WriterOps.Flush = exampleFlushSink;
	/* Writer 上下文是 examplesink，不能复用 examplemem 的 Close 回调。 */
	WriterOps.Close = exampleCloseSink;
	pCustom = xrtReaderCreate(&ReaderOps, &Mem);
	pSink = xrtWriterCreate(&WriterOps, &Sink);
	if ( (pCustom == NULL) || (pSink == NULL) ||
		!xrtReaderCopy(pCustom, pSink, &uCopied) ||
		(uCopied != 9u) ||
		!xrtWriterFlush(pSink) || (Sink.Flushes != 1) ) {
		goto Cleanup;
	}
	printf("io: custom reader/writer bytes=%llu",
		(unsigned long long)uCopied);

	/* ---- 缓冲构造：TakeBuffer / FromBuffer / WriteBuffer / Discard ---- */
	pBuffer = xrtBufferCreate();
	if ( (pBuffer == NULL) ||
		!xrtBufferAppend(pBuffer, (xbytesview) {
			(cbytes)"buffer!", 7u
		}) ) {
		goto Cleanup;
	}
	{
		/* TakeBuffer 接管后原槽清空；内容经 Discard Writer 计数。 */
		xreader* pBufReader = xrtReaderTakeBuffer(&pBuffer);

		if ( (pBufReader == NULL) || (pBuffer != NULL) ) {
			goto Cleanup;
		}
		{
			xwriter* pDiscard = xrtWriterDiscard();
			uint64 uDrained = 0;

			if ( (pDiscard == NULL) ||
				!xrtReaderCopy(pBufReader, pDiscard,
					&uDrained) ||
				(uDrained != 7u) ) {
				goto Cleanup;
			}
			(void)xrtWriterDestroy(pDiscard);
		}
		xrtReaderDestroy(pBufReader);
		/* FromBuffer 借用 + WriteBuffer 写回内存 Writer 核对。 */
		pBuffer = xrtBufferCreate();
		if ( (pBuffer == NULL) ||
			!xrtBufferAppend(pBuffer, (xbytesview) {
				(cbytes)"AB", 2u
			}) ) {
			goto Cleanup;
		}
		{
			xreader* pBorrowed = xrtReaderFromBuffer(pBuffer);

			if ( (pBorrowed == NULL) ||
				!xrtReaderRead(pBorrowed, Out, 2u, &iRead) ||
				(iRead != 2u) || (Out[0] != 'A') ) {
				goto Cleanup;
			}
			(void)xrtWriterSeek(pWriter, 0, XSEEK_START,
				&uPos);
			if ( !xrtWriterWriteBuffer(pWriter, pBuffer) ||
				!xrtWriterTell(pWriter, &uPos) ||
				(uPos != 2u) ) {
				goto Cleanup;
			}
			xrtReaderDestroy(pBorrowed);
		}
	}
	printf(" buffer take/discard/writebuffer ok");

	/* ---- 文件族：TakeFile + OpenAppend + LineReaderCreate ---- */
	File = xrtOpen("xrt-io-tour.tmp",
		XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE);
	if ( (File == NULL) ||
		!xrtWriteFull(File, "ab\n", 3u, NULL) ) {
		goto Cleanup;
	}
	{
		/* 借用形态（读取与写入方向）+ CanSize 探测。 */
		xfile FileB = xrtOpen("xrt-io-tour.tmp", XFILE_READ);
		xreader* pFileReader = FileB != NULL ?
			xrtReaderFromFile(FileB) : NULL;
		xfile FileW = xrtOpen("xrt-io-tour.tmp", XFILE_WRITE);
		xwriter* pFileWriter = FileW != NULL ?
			xrtWriterFromFile(FileW) : NULL;

		if ( (pFileReader == NULL) ||
			!xrtReaderCanSize(pFileReader) ||
			(pFileWriter == NULL) ||
			!xrtWriterCanSize(pFileWriter) ||
			!xrtWriterFlush(pFileWriter) ) {
			goto Cleanup;
		}
		(void)xrtReaderDestroy(pFileReader);
		(void)xrtWriterDestroy(pFileWriter);
		(void)xrtClose(FileB);
		(void)xrtClose(FileW);
		FileB = xrtOpen("xrt-io-tour.tmp", XFILE_READ);
		{
			xreader* pTaken = xrtReaderTakeFile(&FileB);
			bool bTaken = (pTaken != NULL) && (FileB == NULL);

			xrtReaderDestroy(pTaken);
			(void)xrtClose(FileB);  /* 失败时来源仍归调用方。 */
			if ( !bTaken ) goto Cleanup;
		}
	}
	/* OpenAppend：追加模式写一行。 */
	pAppend = xrtWriterOpenAppend("xrt-io-tour.tmp");
	if ( (pAppend == NULL) ||
		!xrtWriterWrite(pAppend, "cd\n", 3u, &iRead) ||
		(iRead != 3u) ||
		!xrtWriterCanSeek(pAppend) ||
		!xrtWriterSize(pAppend, &uSize) ||
		(uSize != 6u) ||
		!xrtWriterFlush(pAppend) ) {
		goto Cleanup;
	}
	(void)xrtWriterTell(pAppend, &uPos);
	/* TakeFile（Writer 方向）。 */
	{
		xfile FileC = xrtOpen("xrt-io-tour.tmp",
			XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE);
		xwriter* pTakeWriter;

		if ( FileC == NULL ) {
			goto Cleanup;
		}
		pTakeWriter = xrtWriterTakeFile(&FileC);
		if ( (pTakeWriter == NULL) || (FileC != NULL) ) {
			goto Cleanup;
		}
		(void)xrtWriterWrite(pTakeWriter, "a\nb\n", 4u, &iRead);
		(void)xrtWriterFlush(pTakeWriter);
		(void)xrtWriterDestroy(pTakeWriter);
	}
	/* 行读取：非接管 LineReaderCreate。 */
	{
		xreader* pFileReader = xrtReaderOpen("xrt-io-tour.tmp");

		if ( pFileReader == NULL ) {
			goto Cleanup;
		}
		pLines = xrtLineReaderCreate(pFileReader, 64u);
		if ( pLines == NULL ) {
			goto Cleanup;
		}
		while ( xrtLineReaderNext(pLines, &Line) ==
			XLINE_NEXT_LINE ) {
			++iLines;
		}
		(void)xrtLineReaderDestroy(pLines);
		pLines = NULL;
		xrtReaderDestroy(pFileReader);
	}
	printf(" file append lines=%d\n", iLines);
	/* 自定义 Reader 的 Close 回调在销毁时触发一次。 */
	(void)xrtReaderDestroy(pCustom);
	pCustom = NULL;
	(void)xrtWriterDestroy(pSink);
	pSink = NULL;
	if ( (iLines != 2) || (Mem.Closes != 1) || (Sink.Closes != 1) ) {
		goto Cleanup;
	}
	printf("io: custom close=%d\n", Mem.Closes);
	iResult = 0;

Cleanup:
	xrtLineReaderDestroy(pLines);
	xrtBufferDestroy(pBuffer);
	xrtBufferDestroy(pReadAll);
	xrtReaderDestroy(pCustom);
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pSink);
	xrtWriterDestroy(pWriter);
	xrtWriterDestroy(pAppend);
	(void)xrtClose(File);
	(void)xrtFileDelete("xrt-io-tour.tmp");
	return iResult;
}
