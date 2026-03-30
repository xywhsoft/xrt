#if !defined(XRT_NO_NETWORK)

#define __XAFILE_CHUNK_MAX		0x40000000u

static const char* __xafileErrHandle = "async file handle is invalid.";
static const char* __xafileErrRead = "async file read failed.";
static const char* __xafileErrWrite = "async file write failed.";
static const char* __xafileErrFlush = "async file flush failed.";
static const char* __xafileErrSize = "async file size query failed.";
static const char* __xafileErrSetSize = "async file resize failed.";
static const char* __xafileErrClosed = "async file is already closing.";
static const char* __xafileErrConfig = "async file config is invalid.";
static const char* __xafileErrMemory = "async file ran out of memory.";

struct xasyncfile_struct {
	xmutex pLock;
	uint32 iFlags;
	uint32 iShareFlags;
	volatile long iRefCount;
	bool bClosing;
	#if defined(_WIN32) || defined(_WIN64)
		HANDLE hFile;
	#else
		int fd;
	#endif
};

typedef struct {
	xasyncfile* pFile;
	uint64 iOffset;
	size_t iSize;
} __xafile_read_task;

typedef struct {
	xasyncfile* pFile;
	uint64 iOffset;
	size_t iSize;
	ptr pData;
} __xafile_write_task;

typedef struct {
	xasyncfile* pFile;
	uint64 iSize;
} __xafile_size_task;

typedef enum {
	__XAFILE_PATH_APPEND = 1,
	__XAFILE_PATH_READ_ALL,
	__XAFILE_PATH_WRITE_ALL,
	__XAFILE_PATH_GET_ALL,
	__XAFILE_PATH_PUT_ALL,
	__XAFILE_PATH_COPY,
	__XAFILE_PATH_MOVE,
	__XAFILE_PATH_DELETE,
	__XAFILE_PATH_DIR_CREATE,
	__XAFILE_PATH_DIR_CREATE_ALL,
	__XAFILE_PATH_DIR_COPY,
	__XAFILE_PATH_DIR_MOVE,
	__XAFILE_PATH_DIR_DELETE
} __xafile_path_task_kind;

typedef struct {
	__xafile_path_task_kind iKind;
	str sPath;
	str sPath2;
	ptr pData;
	size_t iSize;
	int iCharset;
	bool bReWrite;
} __xafile_path_task;

static void __xafileSetError(const char* sError)
{
	xrtSetError((str)(sError ? sError : __xafileErrHandle), FALSE);
}

static int32 __xafileTaskFail(xfuture_result* pOut, const char* sError)
{
	if ( pOut == NULL ) {
		return XRT_NET_ERROR;
	}

	memset(pOut, 0, sizeof(*pOut));
	pOut->iStatus = XRT_NET_ERROR;
	if ( sError && sError[0] ) {
		pOut->sError = xrtCopyStr((str)sError, 0);
		if ( pOut->sError && pOut->sError != xCore.sNull ) {
			pOut->iFlags |= XFUTURE_RESULT_F_OWN_ERROR;
		}
		else {
			pOut->sError = (str)sError;
		}
	}
	return pOut->iStatus;
}

static int32 __xafileTaskFailLastError(xfuture_result* pOut, const char* sFallback)
{
	str sError = xrtGetError();

	if ( sError == NULL || sError == xCore.sNull || sError[0] == '\0' ) {
		sError = (str)(sFallback ? sFallback : __xafileErrHandle);
	}
	return __xafileTaskFail(pOut, (const char*)sError);
}

static int32 __xafileTaskResolve(xfuture_result* pOut, ptr pValue)
{
	if ( pOut == NULL ) {
		return XRT_NET_ERROR;
	}

	memset(pOut, 0, sizeof(*pOut));
	pOut->iStatus = XRT_NET_OK;
	pOut->pValue = pValue;
	return pOut->iStatus;
}

static xasyncfilebuf* __xafileBufCreate(void)
{
	xasyncfilebuf* pBuf = (xasyncfilebuf*)xrtMalloc(sizeof(xasyncfilebuf));

	if ( pBuf == NULL ) {
		return NULL;
	}

	memset(pBuf, 0, sizeof(*pBuf));
	return pBuf;
}

static xasyncfileio* __xafileIoCreate(uint64 iValue, uint64 iOffset)
{
	xasyncfileio* pInfo = (xasyncfileio*)xrtMalloc(sizeof(xasyncfileio));

	if ( pInfo == NULL ) {
		return NULL;
	}

	pInfo->iValue = iValue;
	pInfo->iOffset = iOffset;
	return pInfo;
}

static void __xafileBufDestroyInner(xasyncfilebuf* pBuf)
{
	if ( pBuf == NULL ) {
		return;
	}
	if ( pBuf->pData != NULL && pBuf->pData != xCore.sNull ) {
		xrtFree(pBuf->pData);
	}
	xrtFree(pBuf);
}

static void __xafilePathTaskUnit(__xafile_path_task* pTask)
{
	if ( pTask == NULL ) {
		return;
	}
	if ( pTask->sPath && pTask->sPath != xCore.sNull ) {
		xrtFree(pTask->sPath);
	}
	if ( pTask->sPath2 && pTask->sPath2 != xCore.sNull ) {
		xrtFree(pTask->sPath2);
	}
	if ( pTask->pData && pTask->pData != xCore.sNull ) {
		xrtFree(pTask->pData);
	}
	xrtFree(pTask);
}

static bool __xafileAddRef(xasyncfile* pFile, bool bRejectClosing)
{
	bool bOk = false;

	if ( pFile == NULL || pFile->pLock == NULL ) {
		return false;
	}

	xrtMutexLock(pFile->pLock);
	if ( pFile->iRefCount > 0 ) {
		if ( !bRejectClosing || !pFile->bClosing ) {
			pFile->iRefCount++;
			bOk = true;
		}
	}
	xrtMutexUnlock(pFile->pLock);
	return bOk;
}

static void __xafileRelease(xasyncfile* pFile)
{
	xmutex pLock;
	bool bDestroy = false;
	#if defined(_WIN32) || defined(_WIN64)
		HANDLE hFile = INVALID_HANDLE_VALUE;
	#else
		int fd = -1;
	#endif

	if ( pFile == NULL ) {
		return;
	}

	pLock = pFile->pLock;
	if ( pLock == NULL ) {
		xrtFree(pFile);
		return;
	}

	xrtMutexLock(pLock);
	if ( pFile->iRefCount > 0 ) {
		pFile->iRefCount--;
	}
	if ( pFile->iRefCount == 0 ) {
		bDestroy = true;
		#if defined(_WIN32) || defined(_WIN64)
			hFile = pFile->hFile;
			pFile->hFile = INVALID_HANDLE_VALUE;
		#else
			fd = pFile->fd;
			pFile->fd = -1;
		#endif
		pFile->pLock = NULL;
	}
	xrtMutexUnlock(pLock);

	if ( !bDestroy ) {
		return;
	}

	#if defined(_WIN32) || defined(_WIN64)
		if ( hFile != INVALID_HANDLE_VALUE ) {
			CloseHandle(hFile);
		}
	#else
		if ( fd != -1 ) {
			close(fd);
		}
	#endif
	xrtMutexDestroy(pLock);
	xrtFree(pFile);
}

static bool __xafileSeekLocked(xasyncfile* pFile, uint64 iOffset)
{
	#if defined(_WIN32) || defined(_WIN64)
		LARGE_INTEGER tOffset;

		if ( pFile == NULL || pFile->hFile == INVALID_HANDLE_VALUE ) {
			return false;
		}
		tOffset.QuadPart = (LONGLONG)iOffset;
		return SetFilePointerEx(pFile->hFile, tOffset, NULL, FILE_BEGIN) != 0;
	#else
		if ( pFile == NULL || pFile->fd == -1 ) {
			return false;
		}
		if ( iOffset > 0x7fffffffffffffffULL ) {
			errno = EINVAL;
			return false;
		}
		return lseek(pFile->fd, (off_t)iOffset, SEEK_SET) != (off_t)-1;
	#endif
}

static bool __xafileReadLocked(xasyncfile* pFile, ptr pData, size_t iSize, size_t* piRead)
{
	size_t iTotal = 0u;

	if ( piRead ) {
		*piRead = 0u;
	}
	if ( iSize == 0u ) {
		return true;
	}
	if ( pFile == NULL || pData == NULL ) {
		return false;
	}

	while ( iTotal < iSize ) {
		size_t iChunkSize = iSize - iTotal;
		if ( iChunkSize > __XAFILE_CHUNK_MAX ) {
			iChunkSize = __XAFILE_CHUNK_MAX;
		}

		#if defined(_WIN32) || defined(_WIN64)
			DWORD iChunkRead = 0u;

			if ( !ReadFile(pFile->hFile, (uint8*)pData + iTotal, (DWORD)iChunkSize, &iChunkRead, NULL) ) {
				return false;
			}
			iTotal += (size_t)iChunkRead;
			if ( iChunkRead == 0u || iChunkRead < (DWORD)iChunkSize ) {
				break;
			}
		#else
			ssize_t iChunkRead = read(pFile->fd, (uint8*)pData + iTotal, iChunkSize);

			if ( iChunkRead < 0 ) {
				return false;
			}
			iTotal += (size_t)iChunkRead;
			if ( iChunkRead == 0 || (size_t)iChunkRead < iChunkSize ) {
				break;
			}
		#endif
	}

	if ( piRead ) {
		*piRead = iTotal;
	}
	return true;
}

static bool __xafileWriteLocked(xasyncfile* pFile, const void* pData, size_t iSize, size_t* piWrite)
{
	size_t iTotal = 0u;

	if ( piWrite ) {
		*piWrite = 0u;
	}
	if ( iSize == 0u ) {
		return true;
	}
	if ( pFile == NULL || pData == NULL ) {
		return false;
	}

	while ( iTotal < iSize ) {
		size_t iChunkSize = iSize - iTotal;
		if ( iChunkSize > __XAFILE_CHUNK_MAX ) {
			iChunkSize = __XAFILE_CHUNK_MAX;
		}

		#if defined(_WIN32) || defined(_WIN64)
			DWORD iChunkWrite = 0u;

			if ( !WriteFile(pFile->hFile, (const uint8*)pData + iTotal, (DWORD)iChunkSize, &iChunkWrite, NULL) ) {
				return false;
			}
			if ( iChunkWrite == 0u ) {
				return false;
			}
			iTotal += (size_t)iChunkWrite;
		#else
			ssize_t iChunkWrite = write(pFile->fd, (const uint8*)pData + iTotal, iChunkSize);

			if ( iChunkWrite <= 0 ) {
				return false;
			}
			iTotal += (size_t)iChunkWrite;
		#endif
	}

	if ( piWrite ) {
		*piWrite = iTotal;
	}
	return true;
}

static bool __xafileFlushLocked(xasyncfile* pFile)
{
	#if defined(_WIN32) || defined(_WIN64)
		if ( pFile == NULL || pFile->hFile == INVALID_HANDLE_VALUE ) {
			return false;
		}
		return FlushFileBuffers(pFile->hFile) != 0;
	#else
		if ( pFile == NULL || pFile->fd == -1 ) {
			return false;
		}
		return fsync(pFile->fd) == 0;
	#endif
}

static bool __xafileGetSizeLocked(xasyncfile* pFile, uint64* piSize)
{
	if ( piSize ) {
		*piSize = 0u;
	}

	#if defined(_WIN32) || defined(_WIN64)
		LARGE_INTEGER tSize;

		if ( pFile == NULL || pFile->hFile == INVALID_HANDLE_VALUE ) {
			return false;
		}
		if ( !GetFileSizeEx(pFile->hFile, &tSize) ) {
			return false;
		}
		if ( piSize ) {
			*piSize = (uint64)tSize.QuadPart;
		}
		return true;
	#else
		struct stat tStat;

		if ( pFile == NULL || pFile->fd == -1 ) {
			return false;
		}
		if ( fstat(pFile->fd, &tStat) != 0 ) {
			return false;
		}
		if ( piSize ) {
			*piSize = (uint64)tStat.st_size;
		}
		return true;
	#endif
}

static bool __xafileSetSizeLocked(xasyncfile* pFile, uint64 iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		LARGE_INTEGER tSize;

		if ( pFile == NULL || pFile->hFile == INVALID_HANDLE_VALUE ) {
			return false;
		}
		tSize.QuadPart = (LONGLONG)iSize;
		if ( !SetFilePointerEx(pFile->hFile, tSize, NULL, FILE_BEGIN) ) {
			return false;
		}
		return SetEndOfFile(pFile->hFile) != 0;
	#else
		if ( pFile == NULL || pFile->fd == -1 ) {
			return false;
		}
		if ( iSize > 0x7fffffffffffffffULL ) {
			errno = EINVAL;
			return false;
		}
		return ftruncate(pFile->fd, (off_t)iSize) == 0;
	#endif
}

XXAPI void xrtAsyncFileConfigInit(xasyncfileconfig* pConfig)
{
	if ( pConfig == NULL ) {
		return;
	}

	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->iFlags = XAFILE_F_READ;
	pConfig->iShareFlags = XAFILE_SHARE_READ;
}

XXAPI void xrtAsyncFileBufDestroy(xasyncfilebuf* pBuf)
{
	__xafileBufDestroyInner(pBuf);
}

XXAPI void xrtAsyncFileIoDestroy(xasyncfileio* pInfo)
{
	if ( pInfo != NULL ) {
		xrtFree(pInfo);
	}
}

#if defined(XRT_NO_THREAD)

static const char* __xafileErrThreadRequired = "async file requires thread support.";

XXAPI xasyncfile* xrtAsyncFileOpen(const xasyncfileconfig* pConfig)
{
	(void)pConfig;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}

XXAPI void xrtAsyncFileClose(xasyncfile* pFile)
{
	(void)pFile;
}

XXAPI xfuture* xrtAsyncFileReadAt(xasyncfile* pFile, uint64 iOffset, size_t iSize)
{
	(void)pFile;
	(void)iOffset;
	(void)iSize;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}

XXAPI xfuture* xrtAsyncFileWriteAt(xasyncfile* pFile, uint64 iOffset, const void* pData, size_t iSize)
{
	(void)pFile;
	(void)iOffset;
	(void)pData;
	(void)iSize;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}

XXAPI xfuture* xrtAsyncFileFlush(xasyncfile* pFile)
{
	(void)pFile;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}

XXAPI xfuture* xrtAsyncFileGetSize(xasyncfile* pFile)
{
	(void)pFile;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}

XXAPI xfuture* xrtAsyncFileSetSize(xasyncfile* pFile, uint64 iSize)
{
	(void)pFile;
	(void)iSize;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}

XXAPI xfuture* xrtFileAppendAsync(str sPath, str sText, size_t iSize, int iCharset)
{
	(void)sPath;
	(void)sText;
	(void)iSize;
	(void)iCharset;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}

XXAPI xfuture* xrtFileReadAllAsync(str sPath, int iCharset)
{
	(void)sPath;
	(void)iCharset;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}

XXAPI xfuture* xrtFileWriteAllAsync(str sPath, str sText, size_t iSize, int iCharset)
{
	(void)sPath;
	(void)sText;
	(void)iSize;
	(void)iCharset;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}

XXAPI xfuture* xrtFileGetAllAsync(str sPath)
{
	(void)sPath;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}

XXAPI xfuture* xrtFilePutAllAsync(str sPath, const void* pData, size_t iSize)
{
	(void)sPath;
	(void)pData;
	(void)iSize;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}

XXAPI xfuture* xrtFileCopyAsync(str sSrc, str sDst, bool bReWrite)
{
	(void)sSrc;
	(void)sDst;
	(void)bReWrite;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}

XXAPI xfuture* xrtFileMoveAsync(str sSrc, str sDst, bool bReWrite)
{
	(void)sSrc;
	(void)sDst;
	(void)bReWrite;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}

XXAPI xfuture* xrtFileDeleteAsync(str sPath)
{
	(void)sPath;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}

XXAPI xfuture* xrtDirCreateAsync(str sPath)
{
	(void)sPath;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}

XXAPI xfuture* xrtDirCreateAllAsync(str sPath)
{
	(void)sPath;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}

XXAPI xfuture* xrtDirCopyAsync(str sSrc, str sDst, bool bReWrite)
{
	(void)sSrc;
	(void)sDst;
	(void)bReWrite;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}

XXAPI xfuture* xrtDirMoveAsync(str sSrc, str sDst, bool bReWrite)
{
	(void)sSrc;
	(void)sDst;
	(void)bReWrite;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}

XXAPI xfuture* xrtDirDeleteAsync(str sPath)
{
	(void)sPath;
	__xafileSetError(__xafileErrThreadRequired);
	return NULL;
}

#else

XXAPI xasyncfile* xrtAsyncFileOpen(const xasyncfileconfig* pConfig)
{
	xasyncfile* pFile = NULL;
	uint32 iFlags;
	uint32 iShareFlags;

	if ( pConfig == NULL || pConfig->sPath == NULL || pConfig->sPath[0] == '\0' ) {
		__xafileSetError(__xafileErrConfig);
		return NULL;
	}

	iFlags = pConfig->iFlags;
	if ( iFlags == 0u ) {
		iFlags = XAFILE_F_READ;
	}
	if ( (iFlags & (XAFILE_F_CREATE | XAFILE_F_TRUNCATE)) && !(iFlags & XAFILE_F_WRITE) ) {
		__xafileSetError(__xafileErrConfig);
		return NULL;
	}

	iShareFlags = pConfig->iShareFlags;
	if ( iShareFlags == 0u ) {
		iShareFlags = XAFILE_SHARE_READ;
	}

	pFile = (xasyncfile*)xrtMalloc(sizeof(*pFile));
	if ( pFile == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	memset(pFile, 0, sizeof(*pFile));

	pFile->pLock = xrtMutexCreate();
	if ( pFile->pLock == NULL ) {
		xrtFree(pFile);
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}

	pFile->iFlags = iFlags;
	pFile->iShareFlags = iShareFlags;
	pFile->iRefCount = 1;
	#if defined(_WIN32) || defined(_WIN64)
		pFile->hFile = INVALID_HANDLE_VALUE;
	#else
		pFile->fd = -1;
	#endif

	#if defined(_WIN32) || defined(_WIN64)
	{
		DWORD iDesiredAccess = 0u;
		DWORD iShareMode = 0u;
		DWORD iCreation = OPEN_EXISTING;
		u16str sPathW;

		if ( iFlags & XAFILE_F_READ ) {
			iDesiredAccess |= GENERIC_READ;
		}
		if ( iFlags & XAFILE_F_WRITE ) {
			iDesiredAccess |= GENERIC_WRITE;
		}
		if ( iShareFlags & XAFILE_SHARE_READ ) {
			iShareMode |= FILE_SHARE_READ;
		}
		if ( iShareFlags & XAFILE_SHARE_WRITE ) {
			iShareMode |= FILE_SHARE_WRITE;
		}
		if ( iShareFlags & XAFILE_SHARE_DELETE ) {
			iShareMode |= FILE_SHARE_DELETE;
		}
		if ( (iFlags & XAFILE_F_CREATE) && (iFlags & XAFILE_F_TRUNCATE) ) {
			iCreation = CREATE_ALWAYS;
		}
		else if ( iFlags & XAFILE_F_CREATE ) {
			iCreation = OPEN_ALWAYS;
		}
		else if ( iFlags & XAFILE_F_TRUNCATE ) {
			iCreation = TRUNCATE_EXISTING;
		}

		sPathW = xrtUTF8to16(pConfig->sPath, 0, NULL);
		if ( sPathW == NULL || sPathW == (u16str)xCore.sNull ) {
			xrtMutexDestroy(pFile->pLock);
			xrtFree(pFile);
			__xafileSetError(__xafileErrMemory);
			return NULL;
		}

		pFile->hFile = CreateFileW(
			sPathW,
			iDesiredAccess,
			iShareMode,
			NULL,
			iCreation,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
		xrtFree(sPathW);

		if ( pFile->hFile == INVALID_HANDLE_VALUE ) {
			xrtMutexDestroy(pFile->pLock);
			xrtFree(pFile);
			xrtSetError(sErrorFile_Open, FALSE);
			return NULL;
		}
	}
	#else
	{
		int iOpenFlags = 0;

		if ( (iFlags & XAFILE_F_READ) && (iFlags & XAFILE_F_WRITE) ) {
			iOpenFlags |= O_RDWR;
		}
		else if ( iFlags & XAFILE_F_WRITE ) {
			iOpenFlags |= O_WRONLY;
		}
		else {
			iOpenFlags |= O_RDONLY;
		}
		if ( iFlags & XAFILE_F_CREATE ) {
			iOpenFlags |= O_CREAT;
		}
		if ( iFlags & XAFILE_F_TRUNCATE ) {
			iOpenFlags |= O_TRUNC;
		}

		pFile->fd = open(pConfig->sPath, iOpenFlags, 0666);
		if ( pFile->fd == -1 ) {
			xrtMutexDestroy(pFile->pLock);
			xrtFree(pFile);
			xrtSetError(sErrorFile_Open, FALSE);
			return NULL;
		}
	}
	#endif

	return pFile;
}

XXAPI void xrtAsyncFileClose(xasyncfile* pFile)
{
	if ( pFile == NULL || pFile->pLock == NULL ) {
		return;
	}

	xrtMutexLock(pFile->pLock);
	pFile->bClosing = true;
	xrtMutexUnlock(pFile->pLock);
	__xafileRelease(pFile);
}

static int32 __xafileReadTask(ptr pArg, xfuture_result* pOut)
{
	__xafile_read_task* pTask = (__xafile_read_task*)pArg;
	xasyncfilebuf* pBuf = NULL;
	ptr pData = NULL;
	size_t iRead = 0u;
	int32 iRet = XRT_NET_ERROR;

	xrtClearError();
	if ( pTask == NULL || pTask->pFile == NULL ) {
		return __xafileTaskFail(pOut, __xafileErrHandle);
	}

	pBuf = __xafileBufCreate();
	if ( pBuf == NULL ) {
		iRet = __xafileTaskFail(pOut, __xafileErrMemory);
		goto Exit;
	}
	pBuf->iOffset = pTask->iOffset;

	if ( pTask->iSize > 0u ) {
		pData = xrtMalloc(pTask->iSize + 1u);
		if ( pData == NULL ) {
			iRet = __xafileTaskFail(pOut, __xafileErrMemory);
			goto Exit;
		}
	}

	xrtMutexLock(pTask->pFile->pLock);
	if ( !(pTask->pFile->iFlags & XAFILE_F_READ) ) {
		xrtMutexUnlock(pTask->pFile->pLock);
		iRet = __xafileTaskFail(pOut, __xafileErrRead);
		goto Exit;
	}
	if ( !__xafileSeekLocked(pTask->pFile, pTask->iOffset) ) {
		xrtMutexUnlock(pTask->pFile->pLock);
		iRet = __xafileTaskFailLastError(pOut, __xafileErrRead);
		goto Exit;
	}
	if ( !__xafileReadLocked(pTask->pFile, pData, pTask->iSize, &iRead) ) {
		xrtMutexUnlock(pTask->pFile->pLock);
		iRet = __xafileTaskFailLastError(pOut, __xafileErrRead);
		goto Exit;
	}
	xrtMutexUnlock(pTask->pFile->pLock);

	if ( pData != NULL ) {
		((char*)pData)[iRead] = '\0';
	}
	if ( iRead == 0u && pData != NULL ) {
		xrtFree(pData);
		pData = NULL;
	}

	pBuf->pData = pData;
	pBuf->iSize = iRead;
	pBuf->bEOF = (pTask->iSize > 0u && iRead < pTask->iSize);
	pData = NULL;
	iRet = __xafileTaskResolve(pOut, pBuf);
	pBuf = NULL;

Exit:
	if ( pData != NULL && pData != xCore.sNull ) {
		xrtFree(pData);
	}
	if ( pBuf != NULL ) {
		__xafileBufDestroyInner(pBuf);
	}
	if ( pTask != NULL ) {
		__xafileRelease(pTask->pFile);
		xrtFree(pTask);
	}
	return iRet;
}

static int32 __xafileWriteTask(ptr pArg, xfuture_result* pOut)
{
	__xafile_write_task* pTask = (__xafile_write_task*)pArg;
	xasyncfileio* pInfo = NULL;
	size_t iWrite = 0u;
	int32 iRet = XRT_NET_ERROR;

	xrtClearError();
	if ( pTask == NULL || pTask->pFile == NULL ) {
		return __xafileTaskFail(pOut, __xafileErrHandle);
	}

	xrtMutexLock(pTask->pFile->pLock);
	if ( !(pTask->pFile->iFlags & XAFILE_F_WRITE) ) {
		xrtMutexUnlock(pTask->pFile->pLock);
		iRet = __xafileTaskFail(pOut, __xafileErrWrite);
		goto Exit;
	}
	if ( !__xafileSeekLocked(pTask->pFile, pTask->iOffset) ) {
		xrtMutexUnlock(pTask->pFile->pLock);
		iRet = __xafileTaskFailLastError(pOut, __xafileErrWrite);
		goto Exit;
	}
	if ( !__xafileWriteLocked(pTask->pFile, pTask->pData, pTask->iSize, &iWrite) ) {
		xrtMutexUnlock(pTask->pFile->pLock);
		iRet = __xafileTaskFailLastError(pOut, __xafileErrWrite);
		goto Exit;
	}
	xrtMutexUnlock(pTask->pFile->pLock);

	pInfo = __xafileIoCreate((uint64)iWrite, pTask->iOffset);
	if ( pInfo == NULL ) {
		iRet = __xafileTaskFail(pOut, __xafileErrMemory);
		goto Exit;
	}
	iRet = __xafileTaskResolve(pOut, pInfo);
	pInfo = NULL;

Exit:
	if ( pInfo != NULL ) {
		xrtFree(pInfo);
	}
	if ( pTask != NULL ) {
		if ( pTask->pData != NULL && pTask->pData != xCore.sNull ) {
			xrtFree(pTask->pData);
		}
		__xafileRelease(pTask->pFile);
		xrtFree(pTask);
	}
	return iRet;
}

static int32 __xafileFlushTask(ptr pArg, xfuture_result* pOut)
{
	__xafile_size_task* pTask = (__xafile_size_task*)pArg;
	int32 iRet = XRT_NET_ERROR;

	xrtClearError();
	if ( pTask == NULL || pTask->pFile == NULL ) {
		return __xafileTaskFail(pOut, __xafileErrHandle);
	}

	xrtMutexLock(pTask->pFile->pLock);
	if ( pTask->pFile->iFlags & XAFILE_F_WRITE ) {
		if ( !__xafileFlushLocked(pTask->pFile) ) {
			xrtMutexUnlock(pTask->pFile->pLock);
			iRet = __xafileTaskFailLastError(pOut, __xafileErrFlush);
			goto Exit;
		}
	}
	xrtMutexUnlock(pTask->pFile->pLock);

	iRet = __xafileTaskResolve(pOut, NULL);

Exit:
	if ( pTask != NULL ) {
		__xafileRelease(pTask->pFile);
		xrtFree(pTask);
	}
	return iRet;
}

static int32 __xafileGetSizeTask(ptr pArg, xfuture_result* pOut)
{
	__xafile_size_task* pTask = (__xafile_size_task*)pArg;
	xasyncfileio* pInfo = NULL;
	uint64 iSize = 0u;
	int32 iRet = XRT_NET_ERROR;

	xrtClearError();
	if ( pTask == NULL || pTask->pFile == NULL ) {
		return __xafileTaskFail(pOut, __xafileErrHandle);
	}

	xrtMutexLock(pTask->pFile->pLock);
	if ( !__xafileGetSizeLocked(pTask->pFile, &iSize) ) {
		xrtMutexUnlock(pTask->pFile->pLock);
		iRet = __xafileTaskFailLastError(pOut, __xafileErrSize);
		goto Exit;
	}
	xrtMutexUnlock(pTask->pFile->pLock);

	pInfo = __xafileIoCreate(iSize, 0u);
	if ( pInfo == NULL ) {
		iRet = __xafileTaskFail(pOut, __xafileErrMemory);
		goto Exit;
	}
	iRet = __xafileTaskResolve(pOut, pInfo);
	pInfo = NULL;

Exit:
	if ( pInfo != NULL ) {
		xrtFree(pInfo);
	}
	if ( pTask != NULL ) {
		__xafileRelease(pTask->pFile);
		xrtFree(pTask);
	}
	return iRet;
}

static int32 __xafileSetSizeTask(ptr pArg, xfuture_result* pOut)
{
	__xafile_size_task* pTask = (__xafile_size_task*)pArg;
	xasyncfileio* pInfo = NULL;
	int32 iRet = XRT_NET_ERROR;

	xrtClearError();
	if ( pTask == NULL || pTask->pFile == NULL ) {
		return __xafileTaskFail(pOut, __xafileErrHandle);
	}

	xrtMutexLock(pTask->pFile->pLock);
	if ( !(pTask->pFile->iFlags & XAFILE_F_WRITE) ) {
		xrtMutexUnlock(pTask->pFile->pLock);
		iRet = __xafileTaskFail(pOut, __xafileErrSetSize);
		goto Exit;
	}
	if ( !__xafileSetSizeLocked(pTask->pFile, pTask->iSize) ) {
		xrtMutexUnlock(pTask->pFile->pLock);
		iRet = __xafileTaskFailLastError(pOut, __xafileErrSetSize);
		goto Exit;
	}
	xrtMutexUnlock(pTask->pFile->pLock);

	pInfo = __xafileIoCreate(pTask->iSize, 0u);
	if ( pInfo == NULL ) {
		iRet = __xafileTaskFail(pOut, __xafileErrMemory);
		goto Exit;
	}
	iRet = __xafileTaskResolve(pOut, pInfo);
	pInfo = NULL;

Exit:
	if ( pInfo != NULL ) {
		xrtFree(pInfo);
	}
	if ( pTask != NULL ) {
		__xafileRelease(pTask->pFile);
		xrtFree(pTask);
	}
	return iRet;
}

static int32 __xafilePathTaskProc(ptr pArg, xfuture_result* pOut)
{
	__xafile_path_task* pTask = (__xafile_path_task*)pArg;
	xasyncfilebuf* pBuf = NULL;
	xasyncfileio* pInfo = NULL;
	ptr pData = NULL;
	size_t iSize = 0u;
	int iCount = 0;
	int32 iRet = XRT_NET_ERROR;

	xrtClearError();
	if ( pTask == NULL || pTask->sPath == NULL ) {
		return __xafileTaskFail(pOut, __xafileErrConfig);
	}

	switch ( pTask->iKind ) {
		case __XAFILE_PATH_APPEND:
			iCount = xrtFileAppend(pTask->sPath, (str)pTask->pData, pTask->iSize, pTask->iCharset);
			pInfo = __xafileIoCreate((uint64)iCount, 0u);
			break;

		case __XAFILE_PATH_READ_ALL:
			pData = xrtFileReadAll(pTask->sPath, pTask->iCharset, &iSize);
			if ( pData == NULL ) {
				iRet = __xafileTaskFailLastError(pOut, (const char*)sErrorFile_Read);
				goto Exit;
			}
			if ( pData == xCore.sNull ) {
				pData = NULL;
			}
			pBuf = __xafileBufCreate();
			if ( pBuf == NULL ) {
				iRet = __xafileTaskFail(pOut, __xafileErrMemory);
				goto Exit;
			}
			pBuf->pData = pData;
			pBuf->iSize = iSize;
			pData = NULL;
			iRet = __xafileTaskResolve(pOut, pBuf);
			pBuf = NULL;
			goto Exit;

		case __XAFILE_PATH_WRITE_ALL:
			iCount = xrtFileWriteAll(pTask->sPath, (str)pTask->pData, pTask->iSize, pTask->iCharset);
			pInfo = __xafileIoCreate((uint64)iCount, 0u);
			break;

		case __XAFILE_PATH_GET_ALL:
			pData = xrtFileGetAll(pTask->sPath, &iSize);
			if ( pData == NULL ) {
				iRet = __xafileTaskFailLastError(pOut, (const char*)sErrorFile_Read);
				goto Exit;
			}
			if ( pData == xCore.sNull ) {
				pData = NULL;
			}
			pBuf = __xafileBufCreate();
			if ( pBuf == NULL ) {
				iRet = __xafileTaskFail(pOut, __xafileErrMemory);
				goto Exit;
			}
			pBuf->pData = pData;
			pBuf->iSize = iSize;
			pData = NULL;
			iRet = __xafileTaskResolve(pOut, pBuf);
			pBuf = NULL;
			goto Exit;

		case __XAFILE_PATH_PUT_ALL:
			iCount = xrtFilePutAll(pTask->sPath, pTask->pData, pTask->iSize);
			pInfo = __xafileIoCreate((uint64)iCount, 0u);
			break;

		case __XAFILE_PATH_COPY:
			if ( !xrtFileCopy(pTask->sPath, pTask->sPath2, pTask->bReWrite) ) {
				iRet = __xafileTaskFailLastError(pOut, "async file copy failed.");
				goto Exit;
			}
			pInfo = __xafileIoCreate(1u, 0u);
			break;

		case __XAFILE_PATH_MOVE:
			if ( !xrtFileMove(pTask->sPath, pTask->sPath2, pTask->bReWrite) ) {
				iRet = __xafileTaskFailLastError(pOut, "async file move failed.");
				goto Exit;
			}
			pInfo = __xafileIoCreate(1u, 0u);
			break;

		case __XAFILE_PATH_DELETE:
			if ( !xrtFileDelete(pTask->sPath) ) {
				iRet = __xafileTaskFailLastError(pOut, "async file delete failed.");
				goto Exit;
			}
			pInfo = __xafileIoCreate(1u, 0u);
			break;

		case __XAFILE_PATH_DIR_CREATE:
			if ( !xrtDirCreate(pTask->sPath) ) {
				iRet = __xafileTaskFailLastError(pOut, "async dir create failed.");
				goto Exit;
			}
			pInfo = __xafileIoCreate(1u, 0u);
			break;

		case __XAFILE_PATH_DIR_CREATE_ALL:
			if ( !xrtDirCreateAll(pTask->sPath) ) {
				iRet = __xafileTaskFailLastError(pOut, "async dir create-all failed.");
				goto Exit;
			}
			pInfo = __xafileIoCreate(1u, 0u);
			break;

		case __XAFILE_PATH_DIR_COPY:
			iCount = xrtDirCopy(pTask->sPath, pTask->sPath2, pTask->bReWrite);
			if ( !xrtDirExists(pTask->sPath2) ) {
				iRet = __xafileTaskFailLastError(pOut, "async dir copy failed.");
				goto Exit;
			}
			pInfo = __xafileIoCreate((uint64)iCount, 0u);
			break;

		case __XAFILE_PATH_DIR_MOVE:
			iCount = xrtDirMove(pTask->sPath, pTask->sPath2, pTask->bReWrite);
			if ( !xrtDirExists(pTask->sPath2) || xrtDirExists(pTask->sPath) ) {
				iRet = __xafileTaskFailLastError(pOut, "async dir move failed.");
				goto Exit;
			}
			pInfo = __xafileIoCreate((uint64)iCount, 0u);
			break;

		case __XAFILE_PATH_DIR_DELETE:
			iCount = xrtDirDelete(pTask->sPath);
			if ( xrtDirExists(pTask->sPath) ) {
				iRet = __xafileTaskFailLastError(pOut, "async dir delete failed.");
				goto Exit;
			}
			pInfo = __xafileIoCreate((uint64)iCount, 0u);
			break;

		default:
			iRet = __xafileTaskFail(pOut, __xafileErrConfig);
			goto Exit;
	}

	if ( pInfo == NULL ) {
		iRet = __xafileTaskFail(pOut, __xafileErrMemory);
		goto Exit;
	}
	iRet = __xafileTaskResolve(pOut, pInfo);
	pInfo = NULL;

Exit:
	if ( pData != NULL && pData != xCore.sNull ) {
		xrtFree(pData);
	}
	if ( pBuf != NULL ) {
		__xafileBufDestroyInner(pBuf);
	}
	if ( pInfo != NULL ) {
		xrtFree(pInfo);
	}
	__xafilePathTaskUnit(pTask);
	return iRet;
}

static xfuture* __xafileStartReadTask(xasyncfile* pFile, uint64 iOffset, size_t iSize)
{
	__xafile_read_task* pTask;
	xfuture* pFuture;

	if ( pFile == NULL ) {
		__xafileSetError(__xafileErrHandle);
		return NULL;
	}
	if ( !__xafileAddRef(pFile, true) ) {
		__xafileSetError(__xafileErrClosed);
		return NULL;
	}

	pTask = (__xafile_read_task*)xrtMalloc(sizeof(__xafile_read_task));
	if ( pTask == NULL ) {
		__xafileRelease(pFile);
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	pTask->pFile = pFile;
	pTask->iOffset = iOffset;
	pTask->iSize = iSize;

	pFuture = xTaskRunThread(__xafileReadTask, pTask, 0);
	if ( pFuture == NULL ) {
		__xafileRelease(pFile);
		xrtFree(pTask);
	}
	return pFuture;
}

static xfuture* __xafileStartWriteTask(xasyncfile* pFile, uint64 iOffset, const void* pData, size_t iSize)
{
	__xafile_write_task* pTask;
	xfuture* pFuture;

	if ( pFile == NULL ) {
		__xafileSetError(__xafileErrHandle);
		return NULL;
	}
	if ( !__xafileAddRef(pFile, true) ) {
		__xafileSetError(__xafileErrClosed);
		return NULL;
	}

	pTask = (__xafile_write_task*)xrtMalloc(sizeof(__xafile_write_task));
	if ( pTask == NULL ) {
		__xafileRelease(pFile);
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	memset(pTask, 0, sizeof(*pTask));
	pTask->pFile = pFile;
	pTask->iOffset = iOffset;
	pTask->iSize = iSize;

	if ( iSize > 0u ) {
		pTask->pData = xrtCopyMem((ptr)pData, iSize);
		if ( pTask->pData == NULL || pTask->pData == xCore.sNull ) {
			__xafileRelease(pFile);
			xrtFree(pTask);
			__xafileSetError(__xafileErrMemory);
			return NULL;
		}
	}

	pFuture = xTaskRunThread(__xafileWriteTask, pTask, 0);
	if ( pFuture == NULL ) {
		if ( pTask->pData != NULL && pTask->pData != xCore.sNull ) {
			xrtFree(pTask->pData);
		}
		__xafileRelease(pFile);
		xrtFree(pTask);
	}
	return pFuture;
}

static xfuture* __xafileStartSizeTask(xasyncfile* pFile, xtask_thread_fn pfnTask, uint64 iSize)
{
	__xafile_size_task* pTask;
	xfuture* pFuture;

	if ( pFile == NULL ) {
		__xafileSetError(__xafileErrHandle);
		return NULL;
	}
	if ( !__xafileAddRef(pFile, true) ) {
		__xafileSetError(__xafileErrClosed);
		return NULL;
	}

	pTask = (__xafile_size_task*)xrtMalloc(sizeof(__xafile_size_task));
	if ( pTask == NULL ) {
		__xafileRelease(pFile);
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	pTask->pFile = pFile;
	pTask->iSize = iSize;

	pFuture = xTaskRunThread(pfnTask, pTask, 0);
	if ( pFuture == NULL ) {
		__xafileRelease(pFile);
		xrtFree(pTask);
	}
	return pFuture;
}

static __xafile_path_task* __xafilePathTaskCreate(__xafile_path_task_kind iKind, str sPath, str sPath2)
{
	__xafile_path_task* pTask = (__xafile_path_task*)xrtMalloc(sizeof(__xafile_path_task));

	if ( pTask == NULL ) {
		return NULL;
	}
	memset(pTask, 0, sizeof(*pTask));
	pTask->iKind = iKind;
	pTask->sPath = xrtCopyStr(sPath, 0);
	if ( sPath != NULL && sPath[0] != '\0' && (pTask->sPath == NULL || pTask->sPath == xCore.sNull) ) {
		__xafilePathTaskUnit(pTask);
		return NULL;
	}
	if ( sPath2 != NULL ) {
		pTask->sPath2 = xrtCopyStr(sPath2, 0);
		if ( sPath2[0] != '\0' && (pTask->sPath2 == NULL || pTask->sPath2 == xCore.sNull) ) {
			__xafilePathTaskUnit(pTask);
			return NULL;
		}
	}
	return pTask;
}

static xfuture* __xafileStartPathTask(__xafile_path_task* pTask)
{
	xfuture* pFuture;

	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}

	pFuture = xTaskRunThread(__xafilePathTaskProc, pTask, 0);
	if ( pFuture == NULL ) {
		__xafilePathTaskUnit(pTask);
	}
	return pFuture;
}

XXAPI xfuture* xrtAsyncFileReadAt(xasyncfile* pFile, uint64 iOffset, size_t iSize)
{
	return __xafileStartReadTask(pFile, iOffset, iSize);
}

XXAPI xfuture* xrtAsyncFileWriteAt(xasyncfile* pFile, uint64 iOffset, const void* pData, size_t iSize)
{
	return __xafileStartWriteTask(pFile, iOffset, pData, iSize);
}

XXAPI xfuture* xrtAsyncFileFlush(xasyncfile* pFile)
{
	return __xafileStartSizeTask(pFile, __xafileFlushTask, 0u);
}

XXAPI xfuture* xrtAsyncFileGetSize(xasyncfile* pFile)
{
	return __xafileStartSizeTask(pFile, __xafileGetSizeTask, 0u);
}

XXAPI xfuture* xrtAsyncFileSetSize(xasyncfile* pFile, uint64 iSize)
{
	return __xafileStartSizeTask(pFile, __xafileSetSizeTask, iSize);
}

XXAPI xfuture* xrtFileAppendAsync(str sPath, str sText, size_t iSize, int iCharset)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_APPEND, sPath, NULL);

	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	if ( iSize == 0u && sText != NULL ) {
		iSize = strlen((const char*)sText);
	}
	pTask->iCharset = iCharset;
	pTask->iSize = iSize;
	if ( iSize > 0u ) {
		pTask->pData = xrtCopyMem((ptr)sText, iSize);
		if ( pTask->pData == NULL || pTask->pData == xCore.sNull ) {
			__xafilePathTaskUnit(pTask);
			__xafileSetError(__xafileErrMemory);
			return NULL;
		}
	}
	return __xafileStartPathTask(pTask);
}

XXAPI xfuture* xrtFileReadAllAsync(str sPath, int iCharset)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_READ_ALL, sPath, NULL);

	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	pTask->iCharset = iCharset;
	return __xafileStartPathTask(pTask);
}

XXAPI xfuture* xrtFileWriteAllAsync(str sPath, str sText, size_t iSize, int iCharset)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_WRITE_ALL, sPath, NULL);

	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	if ( iSize == 0u && sText != NULL ) {
		iSize = strlen((const char*)sText);
	}
	pTask->iCharset = iCharset;
	pTask->iSize = iSize;
	if ( iSize > 0u ) {
		pTask->pData = xrtCopyMem((ptr)sText, iSize);
		if ( pTask->pData == NULL || pTask->pData == xCore.sNull ) {
			__xafilePathTaskUnit(pTask);
			__xafileSetError(__xafileErrMemory);
			return NULL;
		}
	}
	return __xafileStartPathTask(pTask);
}

XXAPI xfuture* xrtFileGetAllAsync(str sPath)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_GET_ALL, sPath, NULL);

	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	return __xafileStartPathTask(pTask);
}

XXAPI xfuture* xrtFilePutAllAsync(str sPath, const void* pData, size_t iSize)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_PUT_ALL, sPath, NULL);

	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	pTask->iSize = iSize;
	if ( iSize > 0u ) {
		pTask->pData = xrtCopyMem((ptr)pData, iSize);
		if ( pTask->pData == NULL || pTask->pData == xCore.sNull ) {
			__xafilePathTaskUnit(pTask);
			__xafileSetError(__xafileErrMemory);
			return NULL;
		}
	}
	return __xafileStartPathTask(pTask);
}

XXAPI xfuture* xrtFileCopyAsync(str sSrc, str sDst, bool bReWrite)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_COPY, sSrc, sDst);

	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	pTask->bReWrite = bReWrite;
	return __xafileStartPathTask(pTask);
}

XXAPI xfuture* xrtFileMoveAsync(str sSrc, str sDst, bool bReWrite)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_MOVE, sSrc, sDst);

	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	pTask->bReWrite = bReWrite;
	return __xafileStartPathTask(pTask);
}

XXAPI xfuture* xrtFileDeleteAsync(str sPath)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_DELETE, sPath, NULL);

	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	return __xafileStartPathTask(pTask);
}

XXAPI xfuture* xrtDirCreateAsync(str sPath)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_DIR_CREATE, sPath, NULL);

	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	return __xafileStartPathTask(pTask);
}

XXAPI xfuture* xrtDirCreateAllAsync(str sPath)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_DIR_CREATE_ALL, sPath, NULL);

	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	return __xafileStartPathTask(pTask);
}

XXAPI xfuture* xrtDirCopyAsync(str sSrc, str sDst, bool bReWrite)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_DIR_COPY, sSrc, sDst);

	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	pTask->bReWrite = bReWrite;
	return __xafileStartPathTask(pTask);
}

XXAPI xfuture* xrtDirMoveAsync(str sSrc, str sDst, bool bReWrite)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_DIR_MOVE, sSrc, sDst);

	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	pTask->bReWrite = bReWrite;
	return __xafileStartPathTask(pTask);
}

XXAPI xfuture* xrtDirDeleteAsync(str sPath)
{
	__xafile_path_task* pTask = __xafilePathTaskCreate(__XAFILE_PATH_DIR_DELETE, sPath, NULL);

	if ( pTask == NULL ) {
		__xafileSetError(__xafileErrMemory);
		return NULL;
	}
	return __xafileStartPathTask(pTask);
}

#endif

#endif
