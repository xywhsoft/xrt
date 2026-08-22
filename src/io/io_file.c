#include "../internal/xrt_io.h"



#if defined(XRT_FEATURE_IO_FILE)

/* File 适配器可以借用文件，也可以在 Reader 或 Writer 销毁时接管关闭。 */
typedef struct __xrt_io_file {
	xfile File;
	bool Own;
} __xrt_io_file;



/* 清理构造失败的文件，同时保留触发失败的首个错误。 */
static void __xrtIoFileCleanup(xfile File)
{
	xerror* pSaved = xrtTakeError();

	(void)xrtClose(File);
	if ( pSaved != NULL ) {
		__xrtErrorSetOwned(pSaved);
	}
}



/* 从文件执行一次允许短读的同步读取。 */
static bool __xrtFileReaderRead(
	ptr pContext,
	ptr pBuffer,
	size_t iRequest,
	size_t* pRead
)
{
	return xrtRead(
		((__xrt_io_file*)pContext)->File,
		pBuffer,
		iRequest,
		pRead
	);
}



/* 向文件执行一次允许短写的同步写入。 */
static bool __xrtFileWriterWrite(
	ptr pContext,
	const void* pBuffer,
	size_t iRequest,
	size_t* pWritten
)
{
	return xrtWrite(
		((__xrt_io_file*)pContext)->File,
		pBuffer,
		iRequest,
		pWritten
	);
}



/* 移动文件共享游标。 */
static bool __xrtFileSeek(
	ptr pContext,
	int64 iOffset,
	xseek Origin,
	uint64* pPosition
)
{
	return xrtSeek(
		((__xrt_io_file*)pContext)->File,
		iOffset,
		Origin,
		pPosition
	);
}



/* 查询文件共享游标。 */
static bool __xrtFileTell(ptr pContext, uint64* pPosition)
{
	return xrtTell(((__xrt_io_file*)pContext)->File, pPosition);
}



/* 查询打开文件的当前大小。 */
static bool __xrtFileSize(ptr pContext, uint64* pSize)
{
	return xrtFileSize(((__xrt_io_file*)pContext)->File, pSize);
}



/* 把文件内容与必要元数据显式提交到稳定存储。 */
static bool __xrtFileFlush(ptr pContext)
{
	return xrtFlush(((__xrt_io_file*)pContext)->File);
}



/* 只关闭适配器接管的文件对象。 */
static bool __xrtFileClose(ptr pContext)
{
	__xrt_io_file* pFile = (__xrt_io_file*)pContext;

	if ( !pFile->Own ) {
		return true;
	}
	pFile->Own = false;
	return xrtClose(pFile->File);
}



/* 创建借用或接管文件的 Reader。 */
static xreader* __xrtReaderFileCreate(xfile File, bool bOwn)
{
	xreaderops Ops;
	xreader* pReader;
	__xrt_io_file* pContext;
	ptr pStorage;
	uint32 iFlags;

	if ( File == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	iFlags = xrtFileFlags(File);
	if ( (iFlags & XFILE_READ) == 0u ) {
		__xrtIoError(
			XERR_STATE,
			XIO_ERROR_READ,
			"reader-file",
			"file was not opened for reading"
		);
		return NULL;
	}
	memset(&Ops, 0, sizeof(Ops));
	Ops.Read = __xrtFileReaderRead;
	Ops.Seek = __xrtFileSeek;
	Ops.Tell = __xrtFileTell;
	Ops.Size = __xrtFileSize;
	Ops.Close = __xrtFileClose;
	pReader = __xrtReaderCreateInline(
		&Ops,
		sizeof(__xrt_io_file),
		&pStorage
	);
	if ( pReader == NULL ) {
		return NULL;
	}
	pContext = (__xrt_io_file*)pStorage;
	pContext->File = File;
	pContext->Own = bOwn;
	return pReader;
}



/* 创建借用或接管文件的 Writer。 */
static xwriter* __xrtWriterFileCreate(xfile File, bool bOwn)
{
	xwriterops Ops;
	xwriter* pWriter;
	__xrt_io_file* pContext;
	ptr pStorage;
	uint32 iFlags;

	if ( File == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	iFlags = xrtFileFlags(File);
	if ( (iFlags & XFILE_WRITE) == 0u ) {
		__xrtIoError(
			XERR_STATE,
			XIO_ERROR_WRITE,
			"writer-file",
			"file was not opened for writing"
		);
		return NULL;
	}
	memset(&Ops, 0, sizeof(Ops));
	Ops.Write = __xrtFileWriterWrite;
	Ops.Seek = __xrtFileSeek;
	Ops.Tell = __xrtFileTell;
	Ops.Size = __xrtFileSize;
	Ops.Flush = __xrtFileFlush;
	Ops.Close = __xrtFileClose;
	pWriter = __xrtWriterCreateInline(
		&Ops,
		sizeof(__xrt_io_file),
		&pStorage
	);
	if ( pWriter == NULL ) {
		return NULL;
	}
	pContext = (__xrt_io_file*)pStorage;
	pContext->File = File;
	pContext->Own = bOwn;
	return pWriter;
}



/* 创建借用文件对象的 Reader。 */
XRT_API xreader* xrtReaderFromFile(xfile File)
{
	return __xrtReaderFileCreate(File, false);
}



/* 创建借用文件对象的 Writer。 */
XRT_API xwriter* xrtWriterFromFile(xfile File)
{
	return __xrtWriterFileCreate(File, false);
}



/* 原子接管文件对象并创建 Reader。 */
XRT_API xreader* xrtReaderTakeFile(xfile* pFile)
{
	xreader* pReader;
	xfile File;

	if ( (pFile == NULL) || !__xrtRangeValid(pFile, sizeof(*pFile)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	File = *pFile;
	pReader = __xrtReaderFileCreate(File, true);
	if ( pReader != NULL ) {
		*pFile = NULL;
	}
	return pReader;
}



/* 原子接管文件对象并创建 Writer。 */
XRT_API xwriter* xrtWriterTakeFile(xfile* pFile)
{
	xwriter* pWriter;
	xfile File;

	if ( (pFile == NULL) || !__xrtRangeValid(pFile, sizeof(*pFile)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	File = *pFile;
	pWriter = __xrtWriterFileCreate(File, true);
	if ( pWriter != NULL ) {
		*pFile = NULL;
	}
	return pWriter;
}



/* 打开路径并把文件所有权交给 Reader。 */
XRT_API xreader* xrtReaderOpen(cstr sPath)
{
	xfile File = xrtOpen(sPath, XFILE_READ);
	xreader* pReader;

	if ( File == NULL ) {
		return NULL;
	}
	pReader = __xrtReaderFileCreate(File, true);
	if ( pReader == NULL ) {
		__xrtIoFileCleanup(File);
	}
	return pReader;
}



/* 使用指定写入标志打开路径并把所有权交给 Writer。 */
static xwriter* __xrtWriterOpen(cstr sPath, uint32 iFlags)
{
	xfile File = xrtOpen(sPath, iFlags);
	xwriter* pWriter;

	if ( File == NULL ) {
		return NULL;
	}
	pWriter = __xrtWriterFileCreate(File, true);
	if ( pWriter == NULL ) {
		__xrtIoFileCleanup(File);
	}
	return pWriter;
}



/* 创建或截断路径并创建拥有文件对象的 Writer。 */
XRT_API xwriter* xrtWriterOpen(cstr sPath)
{
	return __xrtWriterOpen(
		sPath,
		XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE
	);
}



/* 以操作系统追加语义创建拥有文件对象的 Writer。 */
XRT_API xwriter* xrtWriterOpenAppend(cstr sPath)
{
	return __xrtWriterOpen(
		sPath,
		XFILE_WRITE | XFILE_CREATE | XFILE_APPEND
	);
}

#endif
