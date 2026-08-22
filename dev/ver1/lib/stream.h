


// 字节流的统一实现。
// 压缩、归档和格式解析器只依赖这层接口，不直接依赖文件系统。

static const str sErrorStream_Handle = (str)"Incorrect stream handle !";
static const str sErrorStream_Read = (str)"Stream read failure !";
static const str sErrorStream_Write = (str)"Stream write failure !";
static const str sErrorStream_Seek = (str)"Stream does not support seek !";
static const str sErrorStream_EOF = (str)"Unexpected end of stream !";
static const str sErrorStream_Size = (str)"Stream size limit exceeded !";



struct xreader_struct {
	xreader_ops Ops;
	ptr Context;
	bool AtEnd;
	bool Closed;
};

struct xwriter_struct {
	xwriter_ops Ops;
	ptr Context;
	bool Closed;
};



typedef struct {
	xfile File;
	bool Own;
} __xrt_file_stream;

typedef struct {
	xbuffer Buffer;
	uint64 Position;
	bool Own;
} __xrt_buffer_stream;

typedef struct {
	const uint8* Data;
	uint64 Size;
	uint64 Position;
} __xrt_memory_reader;



static bool __xrtStreamMove(uint64 iBase, int64 iOffset, uint64 iLimit, uint64* iPosition)
{
	uint64 iResult;
	if ( iOffset < 0 ) {
		uint64 iDistance = (uint64)(-(iOffset + 1)) + 1u;
		if ( iDistance > iBase ) { return FALSE; }
		iResult = iBase - iDistance;
		if ( iResult > iLimit ) { return FALSE; }
	} else {
		uint64 iDistance = (uint64)iOffset;
		if ( iBase > iLimit || iDistance > iLimit - iBase ) { return FALSE; }
		iResult = iBase + iDistance;
	}
	if ( iPosition ) { *iPosition = iResult; }
	return TRUE;
}



// ------------------------------------ File 适配器 ------------------------------------

static bool __xrtFileReaderRead(ptr pContext, ptr pBuffer, uint64 iRequest, uint64* iRead)
{
	__xrt_file_stream* pStream = (__xrt_file_stream*)pContext;
	size_t iDone = 0;
	size_t iChunk = iRequest > (uint64)(size_t)-1 ? (size_t)-1 : (size_t)iRequest;
	if ( !xrtFileReadRaw(pStream->File, pBuffer, iChunk, &iDone) ) { return FALSE; }
	if ( iRead ) { *iRead = (uint64)iDone; }
	return TRUE;
}

static bool __xrtFileReaderSeek(ptr pContext, int64 iOffset, int iMoveMethod, uint64* iPosition)
{
	return xrtFileSeekRaw(((__xrt_file_stream*)pContext)->File, iOffset, iMoveMethod, iPosition);
}

static bool __xrtFileReaderTell(ptr pContext, uint64* iPosition)
{
	return xrtFileTellRaw(((__xrt_file_stream*)pContext)->File, iPosition);
}

static bool __xrtFileReaderSize(ptr pContext, uint64* iSize)
{
	return xrtFileSizeRaw(((__xrt_file_stream*)pContext)->File, iSize);
}

static bool __xrtFileStreamClose(ptr pContext)
{
	__xrt_file_stream* pStream = (__xrt_file_stream*)pContext;
	if ( pStream->Own && pStream->File ) { xrtClose(pStream->File); }
	xrtFree(pStream);
	return TRUE;
}

static bool __xrtFileWriterWrite(ptr pContext, const void* pBuffer, uint64 iRequest, uint64* iWritten)
{
	__xrt_file_stream* pStream = (__xrt_file_stream*)pContext;
	size_t iDone = 0;
	size_t iChunk = iRequest > (uint64)(size_t)-1 ? (size_t)-1 : (size_t)iRequest;
	if ( !xrtFileWriteRaw(pStream->File, pBuffer, iChunk, &iDone) ) { return FALSE; }
	if ( iWritten ) { *iWritten = (uint64)iDone; }
	return TRUE;
}

static bool __xrtFileWriterFlush(ptr pContext)
{
	return xrtFileFlush(((__xrt_file_stream*)pContext)->File);
}



// ------------------------------------ Buffer 适配器 ------------------------------------

static bool __xrtBufferReaderRead(ptr pContext, ptr pBuffer, uint64 iRequest, uint64* iRead)
{
	__xrt_buffer_stream* pStream = (__xrt_buffer_stream*)pContext;
	uint64 iRemain;
	uint64 iDone;
	if ( pStream->Position >= pStream->Buffer->Length ) {
		if ( iRead ) { *iRead = 0; }
		return TRUE;
	}
	iRemain = (uint64)pStream->Buffer->Length - pStream->Position;
	iDone = iRequest < iRemain ? iRequest : iRemain;
	memcpy(pBuffer, pStream->Buffer->Buffer + (size_t)pStream->Position, (size_t)iDone);
	pStream->Position += iDone;
	if ( iRead ) { *iRead = iDone; }
	return TRUE;
}

static bool __xrtBufferReaderSeek(ptr pContext, int64 iOffset, int iMoveMethod, uint64* iPosition)
{
	__xrt_buffer_stream* pStream = (__xrt_buffer_stream*)pContext;
	uint64 iBase;
	if ( iMoveMethod == XRT_SEEK_SET ) { iBase = 0; }
	else if ( iMoveMethod == XRT_SEEK_CUR ) { iBase = pStream->Position; }
	else if ( iMoveMethod == XRT_SEEK_END ) { iBase = pStream->Buffer->Length; }
	else { return FALSE; }
	if ( !__xrtStreamMove(iBase, iOffset, pStream->Buffer->Length, &pStream->Position) ) { return FALSE; }
	if ( iPosition ) { *iPosition = pStream->Position; }
	return TRUE;
}

static bool __xrtBufferWriterSeek(ptr pContext, int64 iOffset, int iMoveMethod, uint64* iPosition)
{
	__xrt_buffer_stream* pStream = (__xrt_buffer_stream*)pContext;
	uint64 iBase;
	if ( iMoveMethod == XRT_SEEK_SET ) { iBase = 0; }
	else if ( iMoveMethod == XRT_SEEK_CUR ) { iBase = pStream->Position; }
	else if ( iMoveMethod == XRT_SEEK_END ) { iBase = pStream->Buffer->Length; }
	else { return FALSE; }
	if ( !__xrtStreamMove(iBase, iOffset, UINT32_MAX, &pStream->Position) ) { return FALSE; }
	if ( iPosition ) { *iPosition = pStream->Position; }
	return TRUE;
}

static bool __xrtBufferStreamTell(ptr pContext, uint64* iPosition)
{
	if ( iPosition ) { *iPosition = ((__xrt_buffer_stream*)pContext)->Position; }
	return TRUE;
}

static bool __xrtBufferReaderSize(ptr pContext, uint64* iSize)
{
	if ( iSize ) { *iSize = ((__xrt_buffer_stream*)pContext)->Buffer->Length; }
	return TRUE;
}

static bool __xrtBufferStreamClose(ptr pContext)
{
	__xrt_buffer_stream* pStream = (__xrt_buffer_stream*)pContext;
	if ( pStream->Own && pStream->Buffer ) { xrtBufferDestroy(pStream->Buffer); }
	xrtFree(pStream);
	return TRUE;
}

static bool __xrtBufferWriterWrite(ptr pContext, const void* pBuffer, uint64 iRequest, uint64* iWritten)
{
	__xrt_buffer_stream* pStream = (__xrt_buffer_stream*)pContext;
	uint64 iEnd;
	uint32 iOldLength;
	if ( iRequest > UINT32_MAX || pStream->Position > UINT32_MAX - iRequest ) { return FALSE; }
	iEnd = pStream->Position + iRequest;
	iOldLength = pStream->Buffer->Length;
	if ( iEnd > pStream->Buffer->AllocLength && !xrtBufferMalloc(pStream->Buffer, (uint32)iEnd) ) {
		return FALSE;
	}
	if ( pStream->Position > iOldLength ) {
		memset(pStream->Buffer->Buffer + iOldLength, 0, (size_t)(pStream->Position - iOldLength));
	}
	if ( iRequest != 0 ) {
		memcpy(pStream->Buffer->Buffer + (size_t)pStream->Position, pBuffer, (size_t)iRequest);
	}
	pStream->Position = iEnd;
	if ( iEnd > pStream->Buffer->Length ) { pStream->Buffer->Length = (uint32)iEnd; }
	if ( iWritten ) { *iWritten = iRequest; }
	return TRUE;
}

static bool __xrtBufferWriterFlush(ptr pContext)
{
	(void)pContext;
	return TRUE;
}



// ------------------------------------ 只读内存适配器 ------------------------------------

static bool __xrtMemoryReaderRead(ptr pContext, ptr pBuffer, uint64 iRequest, uint64* iRead)
{
	__xrt_memory_reader* pStream = (__xrt_memory_reader*)pContext;
	uint64 iRemain = pStream->Size - pStream->Position;
	uint64 iDone = iRequest < iRemain ? iRequest : iRemain;
	if ( iDone != 0 ) { memcpy(pBuffer, pStream->Data + (size_t)pStream->Position, (size_t)iDone); }
	pStream->Position += iDone;
	if ( iRead ) { *iRead = iDone; }
	return TRUE;
}

static bool __xrtMemoryReaderSeek(ptr pContext, int64 iOffset, int iMoveMethod, uint64* iPosition)
{
	__xrt_memory_reader* pStream = (__xrt_memory_reader*)pContext;
	uint64 iBase;
	if ( iMoveMethod == XRT_SEEK_SET ) { iBase = 0; }
	else if ( iMoveMethod == XRT_SEEK_CUR ) { iBase = pStream->Position; }
	else if ( iMoveMethod == XRT_SEEK_END ) { iBase = pStream->Size; }
	else { return FALSE; }
	if ( !__xrtStreamMove(iBase, iOffset, pStream->Size, &pStream->Position) ) { return FALSE; }
	if ( iPosition ) { *iPosition = pStream->Position; }
	return TRUE;
}

static bool __xrtMemoryReaderTell(ptr pContext, uint64* iPosition)
{
	if ( iPosition ) { *iPosition = ((__xrt_memory_reader*)pContext)->Position; }
	return TRUE;
}

static bool __xrtMemoryReaderSize(ptr pContext, uint64* iSize)
{
	if ( iSize ) { *iSize = ((__xrt_memory_reader*)pContext)->Size; }
	return TRUE;
}

static bool __xrtMemoryReaderClose(ptr pContext)
{
	xrtFree(pContext);
	return TRUE;
}



// ------------------------------------ 公共 Reader API ------------------------------------

XXAPI void xrtResourceLimitsInit(xrtresourcelimits* pLimits)
{
	if ( pLimits == NULL ) { return; }
	memset(pLimits, 0, sizeof(*pLimits));
	pLimits->iSize = sizeof(*pLimits);
	pLimits->iVersion = XRT_RESOURCE_LIMITS_VERSION;
	pLimits->iMaxInputBytes = 256u * 1024u * 1024u;
	pLimits->iMaxOutputBytes = 512u * 1024u * 1024u;
	pLimits->iMaxItemBytes = 256u * 1024u * 1024u;
	pLimits->iMaxEntries = 100000u;
	pLimits->iMaxNodes = 1000000u;
	pLimits->iMaxDepth = 128u;
	pLimits->iMaxCompressionRatio = 1000u;
}


XXAPI xreader xrtReaderCreate(const xreader_ops* pOps, ptr pContext)
{
	xreader pReader;
	if ( pOps == NULL || pOps->Read == NULL ) { return NULL; }
	pReader = (xreader)xrtMalloc(sizeof(xreader_struct));
	if ( pReader == NULL ) { return NULL; }
	pReader->Ops = *pOps;
	pReader->Context = pContext;
	pReader->AtEnd = FALSE;
	pReader->Closed = FALSE;
	return pReader;
}

XXAPI xreader xrtReaderFromFile(xfile pFile, bool bTakeOwnership)
{
	__xrt_file_stream* pContext;
	xreader_ops stuOps;
	if ( pFile == NULL ) { return NULL; }
	pContext = (__xrt_file_stream*)xrtMalloc(sizeof(__xrt_file_stream));
	if ( pContext == NULL ) { return NULL; }
	pContext->File = pFile;
	pContext->Own = bTakeOwnership;
	memset(&stuOps, 0, sizeof(stuOps));
	stuOps.Read = __xrtFileReaderRead;
	stuOps.Seek = __xrtFileReaderSeek;
	stuOps.Tell = __xrtFileReaderTell;
	stuOps.Size = __xrtFileReaderSize;
	stuOps.Close = __xrtFileStreamClose;
	{
		xreader pReader = xrtReaderCreate(&stuOps, pContext);
		if ( pReader == NULL ) { xrtFree(pContext); }
		return pReader;
	}
}

XXAPI xreader xrtReaderFromBuffer(xbuffer pBuffer, bool bTakeOwnership)
{
	__xrt_buffer_stream* pContext;
	xreader_ops stuOps;
	if ( pBuffer == NULL ) { return NULL; }
	pContext = (__xrt_buffer_stream*)xrtMalloc(sizeof(__xrt_buffer_stream));
	if ( pContext == NULL ) { return NULL; }
	pContext->Buffer = pBuffer;
	pContext->Position = 0;
	pContext->Own = bTakeOwnership;
	memset(&stuOps, 0, sizeof(stuOps));
	stuOps.Read = __xrtBufferReaderRead;
	stuOps.Seek = __xrtBufferReaderSeek;
	stuOps.Tell = __xrtBufferStreamTell;
	stuOps.Size = __xrtBufferReaderSize;
	stuOps.Close = __xrtBufferStreamClose;
	{
		xreader pReader = xrtReaderCreate(&stuOps, pContext);
		if ( pReader == NULL ) { xrtFree(pContext); }
		return pReader;
	}
}

XXAPI xreader xrtReaderFromMemory(const void* pData, uint64 iSize)
{
	__xrt_memory_reader* pContext;
	xreader_ops stuOps;
	if ( pData == NULL && iSize != 0 ) { return NULL; }
	if ( iSize > (uint64)(size_t)-1 ) { return NULL; }
	pContext = (__xrt_memory_reader*)xrtMalloc(sizeof(__xrt_memory_reader));
	if ( pContext == NULL ) { return NULL; }
	pContext->Data = (const uint8*)pData;
	pContext->Size = iSize;
	pContext->Position = 0;
	memset(&stuOps, 0, sizeof(stuOps));
	stuOps.Read = __xrtMemoryReaderRead;
	stuOps.Seek = __xrtMemoryReaderSeek;
	stuOps.Tell = __xrtMemoryReaderTell;
	stuOps.Size = __xrtMemoryReaderSize;
	stuOps.Close = __xrtMemoryReaderClose;
	{
		xreader pReader = xrtReaderCreate(&stuOps, pContext);
		if ( pReader == NULL ) { xrtFree(pContext); }
		return pReader;
	}
}

XXAPI xreader xrtReaderOpen(str sPath)
{
	xfile pFile = xrtOpen(sPath, TRUE, XRT_CP_BINARY);
	xreader pReader;
	if ( pFile == NULL ) { return NULL; }
	pReader = xrtReaderFromFile(pFile, XRT_STREAM_TAKE_OWNERSHIP);
	if ( pReader == NULL ) { xrtClose(pFile); }
	return pReader;
}

XXAPI bool xrtReaderRead(xreader pReader, ptr pBuffer, uint64 iRequest, uint64* iRead)
{
	uint64 iDone = 0;
	if ( iRead ) { *iRead = 0; }
	if ( pReader == NULL || pReader->Closed ) {
		xrtSetError(sErrorStream_Handle, FALSE);
		return FALSE;
	}
	if ( iRequest == 0 ) { return TRUE; }
	if ( pBuffer == NULL || iRequest > (uint64)(size_t)-1 ) {
		xrtSetError(sErrorStream_Read, FALSE);
		return FALSE;
	}
	if ( !pReader->Ops.Read(pReader->Context, pBuffer, iRequest, &iDone) || iDone > iRequest ) {
		xrtSetError(sErrorStream_Read, FALSE);
		return FALSE;
	}
	pReader->AtEnd = iDone == 0;
	if ( iRead ) { *iRead = iDone; }
	return TRUE;
}

XXAPI bool xrtReaderReadExact(xreader pReader, ptr pBuffer, uint64 iRequest)
{
	uint64 iTotal = 0;
	while ( iTotal < iRequest ) {
		uint64 iDone = 0;
		if ( !xrtReaderRead(pReader, (uint8*)pBuffer + (size_t)iTotal, iRequest - iTotal, &iDone) ) {
			return FALSE;
		}
		if ( iDone == 0 ) {
			xrtSetError(sErrorStream_EOF, FALSE);
			return FALSE;
		}
		iTotal += iDone;
	}
	return TRUE;
}

XXAPI xbuffer xrtReaderReadAll(xreader pReader, uint64 iLimit)
{
	uint8 sChunk[16384];
	xbuffer pResult;
	if ( iLimit == 0 || iLimit > UINT32_MAX ) { iLimit = UINT32_MAX; }
	pResult = xrtBufferCreate(65536);
	if ( pResult == NULL ) { return NULL; }
	for ( ;; ) {
		uint64 iRead = 0;
		uint64 iRemain = iLimit - pResult->Length;
		uint64 iRequest = iRemain < sizeof(sChunk) ? iRemain : sizeof(sChunk);
		if ( iRequest == 0 ) {
			uint8 iExtra;
			if ( !xrtReaderRead(pReader, &iExtra, 1, &iRead) ) { break; }
			if ( iRead == 0 ) { return pResult; }
			xrtSetError(sErrorStream_Size, FALSE);
			break;
		}
		if ( !xrtReaderRead(pReader, sChunk, iRequest, &iRead) ) { break; }
		if ( iRead == 0 ) { return pResult; }
		if ( !xrtBufferAppend(pResult, sChunk, (uint32)iRead, XBUF_BINARY) ) { break; }
	}
	xrtBufferDestroy(pResult);
	return NULL;
}

XXAPI bool xrtReaderSeek(xreader pReader, int64 iOffset, int iMoveMethod, uint64* iPosition)
{
	if ( iPosition ) { *iPosition = 0; }
	if ( pReader == NULL || pReader->Closed || pReader->Ops.Seek == NULL ) {
		xrtSetError(pReader == NULL || pReader->Closed ? sErrorStream_Handle : sErrorStream_Seek, FALSE);
		return FALSE;
	}
	if ( !pReader->Ops.Seek(pReader->Context, iOffset, iMoveMethod, iPosition) ) {
		xrtSetError(sErrorStream_Seek, FALSE);
		return FALSE;
	}
	pReader->AtEnd = FALSE;
	return TRUE;
}

XXAPI bool xrtReaderTell(xreader pReader, uint64* iPosition)
{
	if ( iPosition ) { *iPosition = 0; }
	if ( pReader == NULL || pReader->Closed || pReader->Ops.Tell == NULL ) {
		xrtSetError(sErrorStream_Handle, FALSE);
		return FALSE;
	}
	return pReader->Ops.Tell(pReader->Context, iPosition);
}

XXAPI bool xrtReaderSize(xreader pReader, uint64* iSize)
{
	if ( iSize ) { *iSize = 0; }
	if ( pReader == NULL || pReader->Closed || pReader->Ops.Size == NULL ) {
		xrtSetError(sErrorStream_Handle, FALSE);
		return FALSE;
	}
	return pReader->Ops.Size(pReader->Context, iSize);
}

XXAPI bool xrtReaderCanSeek(xreader pReader)
{
	return pReader != NULL && !pReader->Closed && pReader->Ops.Seek != NULL;
}

XXAPI bool xrtReaderEOF(xreader pReader)
{
	return pReader == NULL || pReader->Closed || pReader->AtEnd;
}

XXAPI bool xrtReaderDestroy(xreader pReader)
{
	bool bRet = TRUE;
	if ( pReader == NULL ) { return TRUE; }
	if ( !pReader->Closed ) {
		pReader->Closed = TRUE;
		if ( pReader->Ops.Close ) { bRet = pReader->Ops.Close(pReader->Context); }
	}
	xrtFree(pReader);
	return bRet;
}



// ------------------------------------ 公共 Writer API ------------------------------------

XXAPI xwriter xrtWriterCreate(const xwriter_ops* pOps, ptr pContext)
{
	xwriter pWriter;
	if ( pOps == NULL || pOps->Write == NULL ) { return NULL; }
	pWriter = (xwriter)xrtMalloc(sizeof(xwriter_struct));
	if ( pWriter == NULL ) { return NULL; }
	pWriter->Ops = *pOps;
	pWriter->Context = pContext;
	pWriter->Closed = FALSE;
	return pWriter;
}

XXAPI xwriter xrtWriterFromFile(xfile pFile, bool bTakeOwnership)
{
	__xrt_file_stream* pContext;
	xwriter_ops stuOps;
	if ( pFile == NULL ) { return NULL; }
	pContext = (__xrt_file_stream*)xrtMalloc(sizeof(__xrt_file_stream));
	if ( pContext == NULL ) { return NULL; }
	pContext->File = pFile;
	pContext->Own = bTakeOwnership;
	memset(&stuOps, 0, sizeof(stuOps));
	stuOps.Write = __xrtFileWriterWrite;
	stuOps.Seek = __xrtFileReaderSeek;
	stuOps.Tell = __xrtFileReaderTell;
	stuOps.Flush = __xrtFileWriterFlush;
	stuOps.Close = __xrtFileStreamClose;
	{
		xwriter pWriter = xrtWriterCreate(&stuOps, pContext);
		if ( pWriter == NULL ) { xrtFree(pContext); }
		return pWriter;
	}
}

XXAPI xwriter xrtWriterFromBuffer(xbuffer pBuffer, bool bTakeOwnership)
{
	__xrt_buffer_stream* pContext;
	xwriter_ops stuOps;
	if ( pBuffer == NULL ) { return NULL; }
	pContext = (__xrt_buffer_stream*)xrtMalloc(sizeof(__xrt_buffer_stream));
	if ( pContext == NULL ) { return NULL; }
	pContext->Buffer = pBuffer;
	pContext->Position = pBuffer->Length;
	pContext->Own = bTakeOwnership;
	memset(&stuOps, 0, sizeof(stuOps));
	stuOps.Write = __xrtBufferWriterWrite;
	stuOps.Seek = __xrtBufferWriterSeek;
	stuOps.Tell = __xrtBufferStreamTell;
	stuOps.Flush = __xrtBufferWriterFlush;
	stuOps.Close = __xrtBufferStreamClose;
	{
		xwriter pWriter = xrtWriterCreate(&stuOps, pContext);
		if ( pWriter == NULL ) { xrtFree(pContext); }
		return pWriter;
	}
}

XXAPI xwriter xrtWriterOpen(str sPath, bool bTruncate)
{
	xfile pFile = xrtOpen(sPath, FALSE, XRT_CP_BINARY);
	xwriter pWriter;
	if ( pFile == NULL ) { return NULL; }
	if ( bTruncate ) {
		if ( !xrtFileSeekRaw(pFile, 0, XRT_SEEK_SET, NULL) || !xrtSetEOF(pFile) ) {
			xrtClose(pFile);
			return NULL;
		}
	}
	pWriter = xrtWriterFromFile(pFile, XRT_STREAM_TAKE_OWNERSHIP);
	if ( pWriter == NULL ) { xrtClose(pFile); }
	return pWriter;
}

XXAPI bool xrtWriterWrite(xwriter pWriter, const void* pBuffer, uint64 iRequest, uint64* iWritten)
{
	uint64 iDone = 0;
	if ( iWritten ) { *iWritten = 0; }
	if ( pWriter == NULL || pWriter->Closed ) {
		xrtSetError(sErrorStream_Handle, FALSE);
		return FALSE;
	}
	if ( iRequest == 0 ) { return TRUE; }
	if ( pBuffer == NULL || iRequest > (uint64)(size_t)-1 ) {
		xrtSetError(sErrorStream_Write, FALSE);
		return FALSE;
	}
	if ( !pWriter->Ops.Write(pWriter->Context, pBuffer, iRequest, &iDone) || iDone > iRequest ) {
		xrtSetError(sErrorStream_Write, FALSE);
		return FALSE;
	}
	if ( iWritten ) { *iWritten = iDone; }
	return TRUE;
}

XXAPI bool xrtWriterWriteAll(xwriter pWriter, const void* pBuffer, uint64 iRequest)
{
	uint64 iTotal = 0;
	while ( iTotal < iRequest ) {
		uint64 iDone = 0;
		if ( !xrtWriterWrite(pWriter, (const uint8*)pBuffer + (size_t)iTotal, iRequest - iTotal, &iDone) ) {
			return FALSE;
		}
		if ( iDone == 0 ) {
			xrtSetError(sErrorStream_Write, FALSE);
			return FALSE;
		}
		iTotal += iDone;
	}
	return TRUE;
}

XXAPI bool xrtWriterWriteBuffer(xwriter pWriter, xbuffer pBuffer)
{
	if ( pBuffer == NULL ) { return FALSE; }
	return xrtWriterWriteAll(pWriter, pBuffer->Buffer, pBuffer->Length);
}

XXAPI bool xrtWriterSeek(xwriter pWriter, int64 iOffset, int iMoveMethod, uint64* iPosition)
{
	if ( iPosition ) { *iPosition = 0; }
	if ( pWriter == NULL || pWriter->Closed || pWriter->Ops.Seek == NULL ) {
		xrtSetError(pWriter == NULL || pWriter->Closed ? sErrorStream_Handle : sErrorStream_Seek, FALSE);
		return FALSE;
	}
	if ( !pWriter->Ops.Seek(pWriter->Context, iOffset, iMoveMethod, iPosition) ) {
		xrtSetError(sErrorStream_Seek, FALSE);
		return FALSE;
	}
	return TRUE;
}

XXAPI bool xrtWriterTell(xwriter pWriter, uint64* iPosition)
{
	if ( iPosition ) { *iPosition = 0; }
	if ( pWriter == NULL || pWriter->Closed || pWriter->Ops.Tell == NULL ) {
		xrtSetError(sErrorStream_Handle, FALSE);
		return FALSE;
	}
	return pWriter->Ops.Tell(pWriter->Context, iPosition);
}

XXAPI bool xrtWriterCanSeek(xwriter pWriter)
{
	return pWriter != NULL && !pWriter->Closed && pWriter->Ops.Seek != NULL;
}

XXAPI bool xrtWriterFlush(xwriter pWriter)
{
	if ( pWriter == NULL || pWriter->Closed ) {
		xrtSetError(sErrorStream_Handle, FALSE);
		return FALSE;
	}
	return pWriter->Ops.Flush == NULL || pWriter->Ops.Flush(pWriter->Context);
}

XXAPI bool xrtWriterDestroy(xwriter pWriter)
{
	bool bRet = TRUE;
	if ( pWriter == NULL ) { return TRUE; }
	if ( !pWriter->Closed ) {
		if ( !xrtWriterFlush(pWriter) ) { bRet = FALSE; }
		pWriter->Closed = TRUE;
		if ( pWriter->Ops.Close && !pWriter->Ops.Close(pWriter->Context) ) { bRet = FALSE; }
	}
	xrtFree(pWriter);
	return bRet;
}
