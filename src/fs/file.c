#if !defined(_WIN32) && !defined(_WIN64)
	#if !defined(_POSIX_C_SOURCE)
		#define _POSIX_C_SOURCE 200809L
	#endif
	#if !defined(_FILE_OFFSET_BITS)
		#define _FILE_OFFSET_BITS 64
	#endif
#endif

#include "../internal/xrt_file.h"

#include <errno.h>

#if !defined(_WIN32) && !defined(_WIN64)
	#include <fcntl.h>
	#include <stdio.h>
	#include <sys/stat.h>
	#include <sys/types.h>
	#include <unistd.h>
	#if defined(__linux__)
		#include <sys/syscall.h>
		#if !defined(RENAME_NOREPLACE)
			#define RENAME_NOREPLACE 1u
		#endif
		extern long syscall(long iNumber, ...);
	#endif
#endif



#if defined(XRT_FEATURE_FILE)

/* Windows 追加对象分离原子数据句柄和锁等控制操作句柄。 */
struct xfile_impl {
	#if defined(_WIN32) || defined(_WIN64)
		HANDLE Handle;
		HANDLE Control;
		SRWLOCK CursorLock;
	#else
		int Handle;
	#endif
	uint32 Flags;
	#if defined(XRT_FEATURE_NET_FILE)
		xatomic64 AsyncOwner;
		bool AsyncAssociated;
	#endif
};



/* 设置带显式错误类别和系统代码的文件错误。 */
void __xrtFileSetKindError(xerrkind Kind, xfileerror Code,
	cstr sOperation, cstr sMessage, int iSystemCode)
{
	xerrordesc tDesc;
	xerror* pError;

	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.Kind = Kind;
	tDesc.Domain = "xrt.file";
	tDesc.Code = (int32)Code;
	tDesc.SystemCode = (int32)iSystemCode;
	tDesc.Operation = sOperation;
	tDesc.Message = sMessage;
	pError = xrtErrorBuild(&tDesc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 设置带系统代码的文件错误。 */
void __xrtFileSetError(xfileerror Code, cstr sOperation,
	cstr sMessage, int iSystemCode)
{
	__xrtFileSetKindError(__xrtSystemErrorKind(iSystemCode),
		Code, sOperation, sMessage, iSystemCode);
}



/* 设置不带系统代码的文件错误。 */
void __xrtFileError(xerrkind Kind, xfileerror Code,
	cstr sOperation, cstr sMessage)
{
	xerrordesc tDesc;
	xerror* pError;

	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.Kind = Kind;
	tDesc.Domain = "xrt.file";
	tDesc.Code = (int32)Code;
	tDesc.Operation = sOperation;
	tDesc.Message = sMessage;
	pError = xrtErrorBuild(&tDesc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 检查文件对象以及操作所需访问权限。 */
static bool __xrtFileCheck(xfile File, uint32 iAccess)
{
	if ( File == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (File->Flags & iAccess) != iAccess ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( (File->Flags & XFILE_ASYNC) != 0u ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 检查打开选项组合，并把空选项展开为稳定默认值。 */
bool __xrtFileOptions(const xfileoptions* pInput, xfileoptions* pOptions)
{
	const uint32 iKnownFlags = XFILE_READ | XFILE_WRITE | XFILE_CREATE |
		XFILE_TRUNCATE | XFILE_APPEND | XFILE_EXCLUSIVE |
		XFILE_NOFOLLOW | XFILE_SYNC | XFILE_ASYNC;

	if ( pInput == NULL ) {
		xrtFileOptionsInit(pOptions);
	} else {
		*pOptions = *pInput;
	}
	if ( ((pOptions->Flags & ~iKnownFlags) != 0u) ||
		 ((pOptions->Share & ~((uint32)XFILE_SHARE_ALL)) != 0u) ||
		 ((pOptions->Mode & ~07777u) != 0u) ||
		 ((pOptions->Flags & (XFILE_READ | XFILE_WRITE)) == 0u) ||
		 (((pOptions->Flags & (XFILE_CREATE | XFILE_TRUNCATE |
			XFILE_APPEND | XFILE_EXCLUSIVE | XFILE_SYNC)) != 0u) &&
		  ((pOptions->Flags & XFILE_WRITE) == 0u)) ||
		 (((pOptions->Flags & XFILE_EXCLUSIVE) != 0u) &&
		  ((pOptions->Flags & XFILE_CREATE) == 0u)) ||
		 (((pOptions->Flags & XFILE_ASYNC) != 0u) &&
		  ((pOptions->Flags & XFILE_APPEND) != 0u)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 初始化高级打开选项。 */
XRT_API void xrtFileOptionsInit(xfileoptions* pOptions)
{
	if ( pOptions == NULL ) {
		return;
	}
	pOptions->Flags = XFILE_READ;
	pOptions->Mode = 0666u;
	pOptions->Share = XFILE_SHARE_ALL;
}



#if defined(_WIN32) || defined(_WIN64)

/* 把公共标志转换为 Windows 文件访问模式。 */
DWORD __xrtFileWindowsAccess(uint32 iFlags)
{
	DWORD iAccess = 0;

	if ( (iFlags & XFILE_READ) != 0u ) {
		iAccess |= GENERIC_READ;
	}
	if ( (iFlags & XFILE_WRITE) != 0u ) {
		iAccess |= GENERIC_WRITE;
	}
	return iAccess;
}



/* 把公共标志转换为 Windows 创建方式。 */
DWORD __xrtFileWindowsDisposition(uint32 iFlags)
{
	if ( (iFlags & XFILE_EXCLUSIVE) != 0u ) {
		return CREATE_NEW;
	}
	if ( (iFlags & XFILE_CREATE) != 0u ) {
		return ((iFlags & XFILE_TRUNCATE) != 0u) ? CREATE_ALWAYS : OPEN_ALWAYS;
	}
	return ((iFlags & XFILE_TRUNCATE) != 0u) ? TRUNCATE_EXISTING : OPEN_EXISTING;
}



/* 把追加数据限制在内核末尾写，原句柄保留给文件锁等控制操作。 */
bool __xrtFileWindowsAppendHandles(HANDLE* pHandle,
	HANDLE* pControl, uint32 iFlags)
{
	HANDLE hRestricted;
	DWORD iAccess = SYNCHRONIZE |
		FILE_READ_ATTRIBUTES | FILE_APPEND_DATA;

	*pControl = INVALID_HANDLE_VALUE;
	if ( (iFlags & XFILE_APPEND) == 0u ) {
		return true;
	}
	if ( (iFlags & XFILE_READ) != 0u ) {
		iAccess |= FILE_READ_DATA;
	}
	if ( !DuplicateHandle(GetCurrentProcess(), *pHandle,
		GetCurrentProcess(), &hRestricted, iAccess, FALSE, 0u) ) {
		return false;
	}
	*pControl = *pHandle;
	*pHandle = hRestricted;
	return true;
}



/* 使用 Windows 原生接口打开一个普通文件。 */
static xfile __xrtFileOpenNative(cstr sPath, const xfileoptions* pOptions)
{
	uint16* pPath = __xrtPathToWide(sPath, NULL);
	DWORD iShare = 0;
	DWORD iAttributes = FILE_ATTRIBUTE_NORMAL;
	HANDLE hFile;
	HANDLE hControl;
	xfile File;

	if ( pPath == NULL ) {
		return NULL;
	}
	if ( (pOptions->Share & XFILE_SHARE_READ) != 0u ) {
		iShare |= FILE_SHARE_READ;
	}
	if ( (pOptions->Share & XFILE_SHARE_WRITE) != 0u ) {
		iShare |= FILE_SHARE_WRITE;
	}
	if ( (pOptions->Share & XFILE_SHARE_DELETE) != 0u ) {
		iShare |= FILE_SHARE_DELETE;
	}
	if ( (pOptions->Flags & XFILE_NOFOLLOW) != 0u ) {
		iAttributes |= FILE_FLAG_OPEN_REPARSE_POINT;
	}
	if ( (pOptions->Flags & XFILE_SYNC) != 0u ) {
		iAttributes |= FILE_FLAG_WRITE_THROUGH;
	}
	if ( (pOptions->Flags & XFILE_ASYNC) != 0u ) {
		iAttributes |= FILE_FLAG_OVERLAPPED;
	}
	hFile = CreateFileW((const wchar_t*)pPath,
		__xrtFileWindowsAccess(pOptions->Flags), iShare, NULL,
		__xrtFileWindowsDisposition(pOptions->Flags), iAttributes, NULL);
	if ( hFile == INVALID_HANDLE_VALUE ) {
		int iCode = (int)GetLastError();

		xrtFree(pPath);
		__xrtFileSetError(XFILE_ERROR_OPEN, "open",
			"failed to open the file", iCode);
		return NULL;
	}
	if ( !__xrtFileWindowsAppendHandles(
		&hFile, &hControl, pOptions->Flags) ) {
		int iCode = (int)GetLastError();

		(void)CloseHandle(hFile);
		xrtFree(pPath);
		__xrtFileSetError(XFILE_ERROR_OPEN, "open",
			"failed to restrict the append file handle", iCode);
		return NULL;
	}
	xrtFree(pPath);
	File = __xrtFileTakeNativePair((intptr_t)hFile,
		(intptr_t)hControl, pOptions->Flags);
	return File;
}

#else

/* 把公共标志转换为 POSIX open 标志。 */
int __xrtFilePosixFlags(uint32 iFlags)
{
	int iOpenFlags;

	if ( (iFlags & (XFILE_READ | XFILE_WRITE)) == (XFILE_READ | XFILE_WRITE) ) {
		iOpenFlags = O_RDWR;
	} else if ( (iFlags & XFILE_WRITE) != 0u ) {
		iOpenFlags = O_WRONLY;
	} else {
		iOpenFlags = O_RDONLY;
	}
	if ( (iFlags & XFILE_CREATE) != 0u ) {
		iOpenFlags |= O_CREAT;
	}
	if ( (iFlags & XFILE_TRUNCATE) != 0u ) {
		iOpenFlags |= O_TRUNC;
	}
	if ( (iFlags & XFILE_APPEND) != 0u ) {
		iOpenFlags |= O_APPEND;
	}
	if ( (iFlags & XFILE_EXCLUSIVE) != 0u ) {
		iOpenFlags |= O_EXCL;
	}
	#if defined(O_NOFOLLOW)
		if ( (iFlags & XFILE_NOFOLLOW) != 0u ) {
			iOpenFlags |= O_NOFOLLOW;
		}
	#endif
	#if defined(O_SYNC)
		if ( (iFlags & XFILE_SYNC) != 0u ) {
			iOpenFlags |= O_SYNC;
		}
	#endif
	#if defined(O_CLOEXEC)
		iOpenFlags |= O_CLOEXEC;
	#endif
	return iOpenFlags;
}



/* 相对目录描述符打开默认不可继承的 POSIX 描述符并保留失败 errno。 */
int __xrtFilePosixOpenAt(int hDirectory, cstr sPath,
	int iFlags, uint32 iMode)
{
	int hFile;

	do {
		hFile = openat(hDirectory, sPath, iFlags, (mode_t)iMode);
	} while ( (hFile < 0) && (errno == EINTR) );
	#if !defined(O_CLOEXEC)
		if ( hFile >= 0 ) {
			int iDescriptorFlags;

			do {
				iDescriptorFlags = fcntl(hFile, F_GETFD);
			} while ( (iDescriptorFlags < 0) && (errno == EINTR) );
			if ( iDescriptorFlags >= 0 ) {
				int iResult;

				do {
					iResult = fcntl(hFile, F_SETFD,
						iDescriptorFlags | FD_CLOEXEC);
				} while ( (iResult < 0) && (errno == EINTR) );
				if ( iResult < 0 ) {
					iDescriptorFlags = -1;
				}
			}
			if ( iDescriptorFlags < 0 ) {
				int iCode = errno;

				(void)close(hFile);
				errno = iCode;
				return -1;
			}
		}
	#endif
	return hFile;
}



/* 使用 POSIX 原生接口打开一个普通文件。 */
static xfile __xrtFileOpenNative(cstr sPath, const xfileoptions* pOptions)
{
	int hFile;
	struct stat Info;
	xfile File;

	#if !defined(O_NOFOLLOW)
		if ( (pOptions->Flags & XFILE_NOFOLLOW) != 0u ) {
			__xrtFileError(XERR_UNSUPPORTED, XFILE_ERROR_OPEN, "open",
				"the platform cannot open a path without following links");
			return NULL;
		}
	#endif
	#if !defined(O_SYNC)
		if ( (pOptions->Flags & XFILE_SYNC) != 0u ) {
			__xrtFileError(XERR_UNSUPPORTED, XFILE_ERROR_OPEN, "open",
				"the platform cannot provide synchronous file writes");
			return NULL;
		}
	#endif
	hFile = __xrtFilePosixOpenAt(AT_FDCWD, sPath,
		__xrtFilePosixFlags(pOptions->Flags), pOptions->Mode);
	if ( hFile < 0 ) {
		int iCode = errno;

		__xrtFileSetError(XFILE_ERROR_OPEN, "open",
			"failed to open the file", iCode);
		return NULL;
	}
	{
		int iResult;

		do {
			iResult = fstat(hFile, &Info);
		} while ( (iResult != 0) && (errno == EINTR) );
		if ( iResult != 0 ) {
			int iCode = errno;

			(void)close(hFile);
			__xrtFileSetError(XFILE_ERROR_OPEN, "open",
				"failed to inspect the opened file", iCode);
			return NULL;
		}
	}
	if ( S_ISDIR(Info.st_mode) ) {
		(void)close(hFile);
		__xrtFileSetError(XFILE_ERROR_OPEN, "open",
			"the path is a directory", EISDIR);
		return NULL;
	}
	File = __xrtFileTakeNative((intptr_t)hFile, pOptions->Flags);
	return File;
}

#endif



/* 接管数据句柄和可选控制句柄并创建文件对象。 */
xfile __xrtFileTakeNativePair(intptr_t iHandle,
	intptr_t iControl, uint32 iFlags)
{
	xfile File = (xfile)xrtMalloc(sizeof(*File));

	if ( File == NULL ) {
		#if defined(_WIN32) || defined(_WIN64)
			(void)CloseHandle((HANDLE)iHandle);
			if ( (HANDLE)iControl != INVALID_HANDLE_VALUE ) {
				(void)CloseHandle((HANDLE)iControl);
			}
		#else
			(void)close((int)iHandle);
			(void)iControl;
		#endif
		return NULL;
	}
	#if defined(_WIN32) || defined(_WIN64)
		File->Handle = (HANDLE)iHandle;
		File->Control = (HANDLE)iControl;
		InitializeSRWLock(&File->CursorLock);
	#else
		File->Handle = (int)iHandle;
	#endif
	File->Flags = iFlags;
	#if defined(XRT_FEATURE_NET_FILE)
		xrtAtomic64Init(&File->AsyncOwner, 0);
		File->AsyncAssociated = false;
	#endif
	return File;
}



/* 接管单个原生句柄并创建普通文件对象。 */
xfile __xrtFileTakeNative(intptr_t iHandle, uint32 iFlags)
{
	return __xrtFileTakeNativePair(iHandle, (intptr_t)-1, iFlags);
}



/* 返回控制操作使用的原生句柄。 */
intptr_t __xrtFileControlNative(xfile File)
{
	if ( File == NULL ) {
		__xrtErrorSetInvalidArgument();
		return (intptr_t)-1;
	}
	#if defined(_WIN32) || defined(_WIN64)
		return (intptr_t)(File->Control != INVALID_HANDLE_VALUE ?
			File->Control : File->Handle);
	#else
		return (intptr_t)File->Handle;
	#endif
}



#if defined(XRT_FEATURE_NET_FILE)
/* 原子绑定异步文件的唯一完成端口，同一 owner 可幂等复用。 */
bool __xrtFileAsyncBind(
	xfile File,
	uint64 iOwner,
	bool** ppAssociated
)
{
	uint64 iExpected = 0;

	if ( (File == NULL) || (iOwner == 0) || (ppAssociated == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (File->Flags & XFILE_ASYNC) == 0u ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( !xrtAtomic64CompareExchange(
		&File->AsyncOwner,
		&iExpected,
		iOwner,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		if ( iExpected != iOwner ) {
			__xrtErrorSetInvalidState();
			return false;
		}
	}
	*ppAssociated = &File->AsyncAssociated;
	return true;
}
#endif



/* 使用完整选项打开文件。 */
XRT_API xfile xrtFileOpen(cstr sPath, const xfileoptions* pOptions)
{
	xfileoptions Options;

	if ( (sPath == NULL) || (sPath[0] == '\0') ||
		 !__xrtFileOptions(pOptions, &Options) ) {
		if ( (sPath == NULL) || (sPath[0] == '\0') ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}
	return __xrtFileOpenNative(sPath, &Options);
}



/* 使用默认权限和共享策略按标志打开文件。 */
XRT_API xfile xrtOpen(cstr sPath, uint32 iFlags)
{
	xfileoptions Options;

	xrtFileOptionsInit(&Options);
	Options.Flags = iFlags;
	return xrtFileOpen(sPath, &Options);
}



/* 关闭原生句柄并销毁文件对象。 */
XRT_API bool xrtClose(xfile File)
{
	bool bResult;
	int iCode = 0;

	if ( File == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		bResult = CloseHandle(File->Handle) != 0;
		if ( !bResult ) {
			iCode = (int)GetLastError();
		}
		if ( File->Control != INVALID_HANDLE_VALUE ) {
			bool bControl = CloseHandle(File->Control) != 0;

			if ( bResult && !bControl ) {
				bResult = false;
				iCode = (int)GetLastError();
			}
		}
	#else
		bResult = close(File->Handle) == 0;
		if ( !bResult ) {
			iCode = errno;
		}
	#endif
	xrtFree(File);
	if ( !bResult ) {
		__xrtFileSetError(XFILE_ERROR_CLOSE, "close",
			"failed to close the file", iCode);
	}
	return bResult;
}



/* 单次读取一段文件数据。 */
XRT_API bool xrtRead(xfile File, ptr pBuffer, size_t iRequest, size_t* pRead)
{
	if ( pRead != NULL ) {
		*pRead = 0;
	}
	if ( (iRequest != 0u) && (pBuffer == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtFileCheck(File, XFILE_READ) ) {
		return false;
	}
	if ( iRequest == 0u ) {
		return true;
	}
	#if defined(_WIN32) || defined(_WIN64)
		{
			DWORD iDone = 0;
			DWORD iChunk = (iRequest > (size_t)UINT32_MAX) ? UINT32_MAX : (DWORD)iRequest;

			AcquireSRWLockExclusive(&File->CursorLock);
			if ( !ReadFile(File->Handle, pBuffer, iChunk, &iDone, NULL) ) {
				int iCode = (int)GetLastError();

				ReleaseSRWLockExclusive(&File->CursorLock);
				if ( (iCode == ERROR_HANDLE_EOF) || (iCode == ERROR_BROKEN_PIPE) ) {
					return true;
				}
				__xrtFileSetError(XFILE_ERROR_READ, "read",
					"failed to read the file", iCode);
				return false;
			}
			ReleaseSRWLockExclusive(&File->CursorLock);
			if ( pRead != NULL ) {
				*pRead = (size_t)iDone;
			}
		}
	#else
		{
			ssize_t iDone;
			size_t iChunk = (iRequest > (size_t)0x7FFFFFFF) ?
				(size_t)0x7FFFFFFF : iRequest;

			do {
				iDone = read(File->Handle, pBuffer, iChunk);
			} while ( (iDone < 0) && (errno == EINTR) );
			if ( iDone < 0 ) {
				int iCode = errno;

				__xrtFileSetError(XFILE_ERROR_READ, "read",
					"failed to read the file", iCode);
				return false;
			}
			if ( pRead != NULL ) {
				*pRead = (size_t)iDone;
			}
		}
	#endif
	return true;
}



/* 单次写入一段文件数据。 */
XRT_API bool xrtWrite(xfile File, const void* pBuffer,
	size_t iRequest, size_t* pWritten)
{
	if ( pWritten != NULL ) {
		*pWritten = 0;
	}
	if ( (iRequest != 0u) && (pBuffer == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtFileCheck(File, XFILE_WRITE) ) {
		return false;
	}
	if ( iRequest == 0u ) {
		return true;
	}
	#if defined(_WIN32) || defined(_WIN64)
		{
			DWORD iDone = 0;
			DWORD iChunk = (iRequest > (size_t)UINT32_MAX) ? UINT32_MAX : (DWORD)iRequest;

			AcquireSRWLockExclusive(&File->CursorLock);
			if ( !WriteFile(File->Handle, pBuffer, iChunk, &iDone, NULL) ) {
				int iCode = (int)GetLastError();

				ReleaseSRWLockExclusive(&File->CursorLock);
				__xrtFileSetError(XFILE_ERROR_WRITE, "write",
					"failed to write the file", iCode);
				return false;
			}
			ReleaseSRWLockExclusive(&File->CursorLock);
			if ( pWritten != NULL ) {
				*pWritten = (size_t)iDone;
			}
		}
	#else
		{
			ssize_t iDone;
			size_t iChunk = (iRequest > (size_t)0x7FFFFFFF) ?
				(size_t)0x7FFFFFFF : iRequest;

			do {
				iDone = write(File->Handle, pBuffer, iChunk);
			} while ( (iDone < 0) && (errno == EINTR) );
			if ( iDone < 0 ) {
				int iCode = errno;

				__xrtFileSetError(XFILE_ERROR_WRITE, "write",
					"failed to write the file", iCode);
				return false;
			}
			if ( pWritten != NULL ) {
				*pWritten = (size_t)iDone;
			}
		}
	#endif
	return true;
}



/* 持续读取直到填满目标缓冲或发生错误。 */
XRT_API bool xrtReadFull(xfile File, ptr pBuffer,
	size_t iRequest, size_t* pRead)
{
	size_t iTotal = 0;

	if ( pRead != NULL ) {
		*pRead = 0;
	}
	if ( (iRequest != 0u) && (pBuffer == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtFileCheck(File, XFILE_READ) ) {
		return false;
	}
	while ( iTotal < iRequest ) {
		size_t iDone;

		if ( !xrtRead(File, (bytes)pBuffer + iTotal,
			iRequest - iTotal, &iDone) ) {
			if ( pRead != NULL ) {
				*pRead = iTotal;
			}
			return false;
		}
		if ( iDone == 0u ) {
			if ( pRead != NULL ) {
				*pRead = iTotal;
			}
			__xrtFileError(XERR_IO, XFILE_ERROR_EOF, "read-full",
				"the file ended before the requested bytes were read");
			return false;
		}
		iTotal += iDone;
	}
	if ( pRead != NULL ) {
		*pRead = iTotal;
	}
	return true;
}



/* 持续写入直到消费全部源数据或发生错误。 */
XRT_API bool xrtWriteFull(xfile File, const void* pBuffer,
	size_t iRequest, size_t* pWritten)
{
	size_t iTotal = 0;

	if ( pWritten != NULL ) {
		*pWritten = 0;
	}
	if ( (iRequest != 0u) && (pBuffer == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtFileCheck(File, XFILE_WRITE) ) {
		return false;
	}
	while ( iTotal < iRequest ) {
		size_t iDone;

		if ( !xrtWrite(File, (cbytes)pBuffer + iTotal,
			iRequest - iTotal, &iDone) ) {
			if ( pWritten != NULL ) {
				*pWritten = iTotal;
			}
			return false;
		}
		if ( iDone == 0u ) {
			if ( pWritten != NULL ) {
				*pWritten = iTotal;
			}
			__xrtFileError(XERR_IO, XFILE_ERROR_WRITE, "write-full",
				"the file made no write progress");
			return false;
		}
		iTotal += iDone;
	}
	if ( pWritten != NULL ) {
		*pWritten = iTotal;
	}
	return true;
}



/* 检查绝对偏移及本次系统调用覆盖的字节范围。 */
static bool __xrtFileAtRange(uint64 iOffset, size_t iRequest)
{
	if ( iRequest == 0u ) {
		return true;
	}
	if ( (iOffset > (uint64)INT64_MAX) ||
		 ((uint64)(iRequest - 1u) > ((uint64)INT64_MAX - iOffset)) ) {
		__xrtFileError(XERR_RANGE, XFILE_ERROR_SEEK, "offset",
			"the absolute file range is outside the supported range");
		return false;
	}
	return true;
}



/* 从绝对偏移执行一次读取且不改变共享游标。 */
XRT_API bool xrtReadAt(xfile File, uint64 iOffset,
	ptr pBuffer, size_t iRequest, size_t* pRead)
{
	size_t iChunk = (iRequest > (size_t)0x7FFFFFFF) ?
		(size_t)0x7FFFFFFF : iRequest;

	if ( pRead != NULL ) {
		*pRead = 0;
	}
	if ( (iRequest != 0u) && (pBuffer == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtFileCheck(File, XFILE_READ) ) {
		return false;
	}
	if ( (iRequest == 0u) || !__xrtFileAtRange(iOffset, iChunk) ) {
		return iRequest == 0u;
	}
	#if defined(_WIN32) || defined(_WIN64)
		{
			LARGE_INTEGER Original;
			LARGE_INTEGER Target;
			LARGE_INTEGER Zero;
			DWORD iDone = 0;
			int iReadCode = 0;
			int iRestoreCode = 0;

			Zero.QuadPart = 0;
			Target.QuadPart = (LONGLONG)iOffset;
			AcquireSRWLockExclusive(&File->CursorLock);
			if ( !SetFilePointerEx(File->Handle, Zero,
				&Original, FILE_CURRENT) ) {
				int iCode = (int)GetLastError();

				ReleaseSRWLockExclusive(&File->CursorLock);
				__xrtFileSetError(XFILE_ERROR_SEEK, "read-at-position",
					"failed to save the shared file position", iCode);
				return false;
			}
			if ( !SetFilePointerEx(File->Handle, Target, NULL, FILE_BEGIN) ) {
				iReadCode = (int)GetLastError();
			} else if ( !ReadFile(File->Handle, pBuffer,
				(DWORD)iChunk, &iDone, NULL) ) {
				iReadCode = (int)GetLastError();
			}
			if ( !SetFilePointerEx(File->Handle, Original,
				NULL, FILE_BEGIN) ) {
				iRestoreCode = (int)GetLastError();
			}
			ReleaseSRWLockExclusive(&File->CursorLock);
			if ( pRead != NULL ) {
				*pRead = (size_t)iDone;
			}
			if ( (iReadCode != 0) && (iReadCode != ERROR_HANDLE_EOF) ) {
				__xrtFileSetError(XFILE_ERROR_READ, "read-at",
					"failed to read the file at the requested offset",
					iReadCode);
				return false;
			}
			if ( iRestoreCode != 0 ) {
				__xrtFileSetError(XFILE_ERROR_SEEK, "read-at-restore",
					"the data was read but the shared file position could not be restored",
					iRestoreCode);
				return false;
			}
		}
	#else
		{
			ssize_t iDone;

			do {
				iDone = pread(File->Handle, pBuffer,
					iChunk, (off_t)iOffset);
			} while ( (iDone < 0) && (errno == EINTR) );
			if ( iDone < 0 ) {
				int iCode = errno;

				__xrtFileSetError(XFILE_ERROR_READ, "read-at",
					"failed to read the file at the requested offset",
					iCode);
				return false;
			}
			if ( pRead != NULL ) {
				*pRead = (size_t)iDone;
			}
		}
	#endif
	return true;
}



/* 向绝对偏移执行一次写入且不改变共享游标。 */
XRT_API bool xrtWriteAt(xfile File, uint64 iOffset,
	const void* pBuffer, size_t iRequest, size_t* pWritten)
{
	size_t iChunk = (iRequest > (size_t)0x7FFFFFFF) ?
		(size_t)0x7FFFFFFF : iRequest;

	if ( pWritten != NULL ) {
		*pWritten = 0;
	}
	if ( (iRequest != 0u) && (pBuffer == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtFileCheck(File, XFILE_WRITE) ) {
		return false;
	}
	if ( (File->Flags & XFILE_APPEND) != 0u ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( (iRequest == 0u) || !__xrtFileAtRange(iOffset, iChunk) ) {
		return iRequest == 0u;
	}
	#if defined(_WIN32) || defined(_WIN64)
		{
			LARGE_INTEGER Original;
			LARGE_INTEGER Target;
			LARGE_INTEGER Zero;
			DWORD iDone = 0;
			int iWriteCode = 0;
			int iRestoreCode = 0;

			Zero.QuadPart = 0;
			Target.QuadPart = (LONGLONG)iOffset;
			AcquireSRWLockExclusive(&File->CursorLock);
			if ( !SetFilePointerEx(File->Handle, Zero,
				&Original, FILE_CURRENT) ) {
				int iCode = (int)GetLastError();

				ReleaseSRWLockExclusive(&File->CursorLock);
				__xrtFileSetError(XFILE_ERROR_SEEK, "write-at-position",
					"failed to save the shared file position", iCode);
				return false;
			}
			if ( !SetFilePointerEx(File->Handle, Target, NULL, FILE_BEGIN) ) {
				iWriteCode = (int)GetLastError();
			} else if ( !WriteFile(File->Handle, pBuffer,
				(DWORD)iChunk, &iDone, NULL) ) {
				iWriteCode = (int)GetLastError();
			}
			if ( !SetFilePointerEx(File->Handle, Original,
				NULL, FILE_BEGIN) ) {
				iRestoreCode = (int)GetLastError();
			}
			ReleaseSRWLockExclusive(&File->CursorLock);
			if ( pWritten != NULL ) {
				*pWritten = (size_t)iDone;
			}
			if ( iWriteCode != 0 ) {
				__xrtFileSetError(XFILE_ERROR_WRITE, "write-at",
					"failed to write the file at the requested offset",
					iWriteCode);
				return false;
			}
			if ( iRestoreCode != 0 ) {
				__xrtFileSetError(XFILE_ERROR_SEEK, "write-at-restore",
					"the data was written but the shared file position could not be restored",
					iRestoreCode);
				return false;
			}
		}
	#else
		{
			ssize_t iDone;

			do {
				iDone = pwrite(File->Handle, pBuffer,
					iChunk, (off_t)iOffset);
			} while ( (iDone < 0) && (errno == EINTR) );
			if ( iDone < 0 ) {
				int iCode = errno;

				__xrtFileSetError(XFILE_ERROR_WRITE, "write-at",
					"failed to write the file at the requested offset",
					iCode);
				return false;
			}
			if ( pWritten != NULL ) {
				*pWritten = (size_t)iDone;
			}
		}
	#endif
	return true;
}



/* 从绝对偏移持续读取直到填满目标缓冲。 */
XRT_API bool xrtReadAtFull(xfile File, uint64 iOffset,
	ptr pBuffer, size_t iRequest, size_t* pRead)
{
	size_t iTotal = 0;

	if ( pRead != NULL ) {
		*pRead = 0;
	}
	if ( (iRequest != 0u) && (pBuffer == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtFileCheck(File, XFILE_READ) ||
		 !__xrtFileAtRange(iOffset, iRequest) ) {
		return false;
	}
	while ( iTotal < iRequest ) {
		size_t iDone;

		if ( !xrtReadAt(File, iOffset + (uint64)iTotal,
			(bytes)pBuffer + iTotal, iRequest - iTotal, &iDone) ) {
			if ( pRead != NULL ) {
				*pRead = iTotal;
			}
			return false;
		}
		if ( iDone == 0u ) {
			if ( pRead != NULL ) {
				*pRead = iTotal;
			}
			__xrtFileError(XERR_IO, XFILE_ERROR_EOF, "read-at-full",
				"the file ended before the requested bytes were read");
			return false;
		}
		iTotal += iDone;
	}
	if ( pRead != NULL ) {
		*pRead = iTotal;
	}
	return true;
}



/* 向绝对偏移持续写入直到消费全部源数据。 */
XRT_API bool xrtWriteAtFull(xfile File, uint64 iOffset,
	const void* pBuffer, size_t iRequest, size_t* pWritten)
{
	size_t iTotal = 0;

	if ( pWritten != NULL ) {
		*pWritten = 0;
	}
	if ( (iRequest != 0u) && (pBuffer == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtFileCheck(File, XFILE_WRITE) ||
		 !__xrtFileAtRange(iOffset, iRequest) ) {
		return false;
	}
	if ( (File->Flags & XFILE_APPEND) != 0u ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	while ( iTotal < iRequest ) {
		size_t iDone;

		if ( !xrtWriteAt(File, iOffset + (uint64)iTotal,
			(cbytes)pBuffer + iTotal, iRequest - iTotal, &iDone) ) {
			if ( pWritten != NULL ) {
				*pWritten = iTotal;
			}
			return false;
		}
		if ( iDone == 0u ) {
			if ( pWritten != NULL ) {
				*pWritten = iTotal;
			}
			__xrtFileError(XERR_IO, XFILE_ERROR_WRITE, "write-at-full",
				"the file made no positional write progress");
			return false;
		}
		iTotal += iDone;
	}
	if ( pWritten != NULL ) {
		*pWritten = iTotal;
	}
	return true;
}



/* 按 64 位偏移移动共享文件游标。 */
XRT_API bool xrtSeek(xfile File, int64 iOffset, xseek Origin, uint64* pPosition)
{
	if ( (File == NULL) || ((Origin != XSEEK_START) &&
		 (Origin != XSEEK_CURRENT) && (Origin != XSEEK_END)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		{
			LARGE_INTEGER Offset;
			LARGE_INTEGER Position;
			DWORD iMethod = (Origin == XSEEK_START) ? FILE_BEGIN :
				((Origin == XSEEK_CURRENT) ? FILE_CURRENT : FILE_END);

			Offset.QuadPart = iOffset;
			AcquireSRWLockExclusive(&File->CursorLock);
			if ( !SetFilePointerEx(File->Handle, Offset, &Position, iMethod) ) {
				int iCode = (int)GetLastError();

				ReleaseSRWLockExclusive(&File->CursorLock);
				__xrtFileSetError(XFILE_ERROR_SEEK, "seek",
					"failed to move the file position", iCode);
				return false;
			}
			ReleaseSRWLockExclusive(&File->CursorLock);
			if ( Position.QuadPart < 0 ) {
				__xrtFileError(XERR_RANGE, XFILE_ERROR_SEEK, "seek",
					"the resulting file position is negative");
				return false;
			}
			if ( pPosition != NULL ) {
				*pPosition = (uint64)Position.QuadPart;
			}
		}
	#else
		{
			int iWhence = (Origin == XSEEK_START) ? SEEK_SET :
				((Origin == XSEEK_CURRENT) ? SEEK_CUR : SEEK_END);
			off_t iPosition;

			errno = 0;
			iPosition = lseek(File->Handle, (off_t)iOffset, iWhence);
			if ( iPosition < (off_t)0 ) {
				int iCode = errno;

				__xrtFileSetError(XFILE_ERROR_SEEK, "seek",
					"failed to move the file position", iCode);
				return false;
			}
			if ( pPosition != NULL ) {
				*pPosition = (uint64)iPosition;
			}
		}
	#endif
	return true;
}



/* 返回共享文件游标的当前位置。 */
XRT_API bool xrtTell(xfile File, uint64* pPosition)
{
	if ( pPosition == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return xrtSeek(File, 0, XSEEK_CURRENT, pPosition);
}



#if defined(_WIN32) || defined(_WIN64)

/* 把 Windows FILETIME 转换为 Unix Epoch 微秒。 */
xtime __xrtFileWindowsTime(FILETIME Time)
{
	const uint64 iEpoch = UINT64_C(116444736000000000);
	uint64 iTicks = ((uint64)Time.dwHighDateTime << 32) | Time.dwLowDateTime;
	uint64 iDifference;

	if ( iTicks >= iEpoch ) {
		return (xtime)((iTicks - iEpoch) / UINT64_C(10));
	}
	iDifference = iEpoch - iTicks;
	return -(xtime)(iDifference / UINT64_C(10)) -
		((iDifference % UINT64_C(10)) != 0u ? 1 : 0);
}



/* 把 Unix Epoch 微秒安全转换为 Windows FILETIME。 */
static bool __xrtFileWindowsTimeValue(xtime Time, FILETIME* pValue)
{
	const uint64 iEpoch = UINT64_C(116444736000000000);
	uint64 iTicks;

	if ( Time >= 0 ) {
		if ( (uint64)Time > ((UINT64_MAX - iEpoch) / UINT64_C(10)) ) {
			__xrtFileError(XERR_RANGE, XFILE_ERROR_METADATA, "set-times",
				"the timestamp is outside the Windows FILETIME range");
			return false;
		}
		iTicks = iEpoch + ((uint64)Time * UINT64_C(10));
	} else {
		uint64 iMagnitude = (uint64)(-(Time + 1)) + 1u;

		if ( iMagnitude > (iEpoch / UINT64_C(10)) ) {
			__xrtFileError(XERR_RANGE, XFILE_ERROR_METADATA, "set-times",
				"the timestamp is outside the Windows FILETIME range");
			return false;
		}
		iTicks = iEpoch - (iMagnitude * UINT64_C(10));
	}
	pValue->dwLowDateTime = (DWORD)iTicks;
	pValue->dwHighDateTime = (DWORD)(iTicks >> 32);
	return true;
}



/* 把 Windows 枚举条目已有信息转换为稳定元数据。 */
void __xrtFileWindowsFindInfo(const WIN32_FIND_DATAW* pNative,
	xfileinfo* pInfo)
{
	memset(pInfo, 0, sizeof(*pInfo));
	if ( (pNative->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u ) {
		pInfo->Type = XFILE_TYPE_LINK;
	} else if ( (pNative->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0u ) {
		pInfo->Type = XFILE_TYPE_DIRECTORY;
	} else {
		pInfo->Type = XFILE_TYPE_FILE;
	}
	pInfo->Available = XFILE_INFO_SIZE | XFILE_INFO_ACCESS_TIME |
		XFILE_INFO_MODIFY_TIME | XFILE_INFO_CREATE_TIME;
	pInfo->Attributes = (uint32)pNative->dwFileAttributes;
	pInfo->Size = ((uint64)pNative->nFileSizeHigh << 32) |
		pNative->nFileSizeLow;
	pInfo->Accessed = __xrtFileWindowsTime(pNative->ftLastAccessTime);
	pInfo->Modified = __xrtFileWindowsTime(pNative->ftLastWriteTime);
	pInfo->Created = __xrtFileWindowsTime(pNative->ftCreationTime);
}



/* 从 Windows 文件信息填充稳定元数据。 */
static void __xrtFileWindowsInfo(const BY_HANDLE_FILE_INFORMATION* pNative,
	xfileinfo* pInfo)
{
	memset(pInfo, 0, sizeof(*pInfo));
	if ( (pNative->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u ) {
		pInfo->Type = XFILE_TYPE_LINK;
	} else if ( (pNative->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0u ) {
		pInfo->Type = XFILE_TYPE_DIRECTORY;
	} else {
		pInfo->Type = XFILE_TYPE_FILE;
	}
	pInfo->Available = XFILE_INFO_SIZE | XFILE_INFO_ACCESS_TIME |
		XFILE_INFO_MODIFY_TIME | XFILE_INFO_CREATE_TIME |
		XFILE_INFO_IDENTITY | XFILE_INFO_LINK_COUNT;
	pInfo->Attributes = (uint32)pNative->dwFileAttributes;
	pInfo->Size = ((uint64)pNative->nFileSizeHigh << 32) | pNative->nFileSizeLow;
	pInfo->Device = (uint64)pNative->dwVolumeSerialNumber;
	pInfo->Identity = ((uint64)pNative->nFileIndexHigh << 32) | pNative->nFileIndexLow;
	pInfo->LinkCount = (uint64)pNative->nNumberOfLinks;
	pInfo->Accessed = __xrtFileWindowsTime(pNative->ftLastAccessTime);
	pInfo->Modified = __xrtFileWindowsTime(pNative->ftLastWriteTime);
	pInfo->Created = __xrtFileWindowsTime(pNative->ftCreationTime);
}



/* 查询 Windows 打开句柄元数据，失败时不修改调用方输出。 */
bool __xrtFileWindowsStat(HANDLE hFile, xfileinfo* pInfo, bool bReport)
{
	BY_HANDLE_FILE_INFORMATION Native;
	xfileinfo Info;

	if ( !GetFileInformationByHandle(hFile, &Native) ) {
		if ( bReport ) {
			int iCode = (int)GetLastError();

			__xrtFileSetError(XFILE_ERROR_STAT, "stat",
				"failed to query file metadata", iCode);
		}
		return false;
	}
	__xrtFileWindowsInfo(&Native, &Info);
	*pInfo = Info;
	return true;
}

#else

/* 把纳秒精度系统时间安全转换为 Unix 微秒。 */
static bool __xrtFileTime(int64 iSeconds, int64 iNanoseconds, xtime* pTime)
{
	int64 iValue;

	if ( !__xrtTimeMulChecked(iSeconds, XRT_TIME_SECOND, &iValue) ||
		 !__xrtTimeAddChecked(iValue, iNanoseconds / 1000, &iValue) ) {
		__xrtFileError(XERR_RANGE, XFILE_ERROR_STAT, "stat",
			"file timestamp is outside the supported range");
		return false;
	}
	*pTime = iValue;
	return true;
}



/* 把 Unix Epoch 微秒安全转换为 POSIX timespec。 */
static bool __xrtFileTimeValue(xtime Time, struct timespec* pValue)
{
	int64 iSeconds = Time / XRT_TIME_SECOND;
	int64 iMicros = Time % XRT_TIME_SECOND;
	time_t Seconds;

	if ( iMicros < 0 ) {
		iSeconds--;
		iMicros += XRT_TIME_SECOND;
	}
	Seconds = (time_t)iSeconds;
	if ( (int64)Seconds != iSeconds ) {
		__xrtFileError(XERR_RANGE, XFILE_ERROR_METADATA, "set-times",
			"the timestamp is outside the platform time range");
		return false;
	}
	pValue->tv_sec = Seconds;
	pValue->tv_nsec = (long)(iMicros * 1000);
	return true;
}




/* 把 POSIX 文件类型映射为稳定对象类别。 */
static xfiletype __xrtFilePosixType(mode_t iMode)
{
	if ( S_ISREG(iMode) ) {
		return XFILE_TYPE_FILE;
	}
	if ( S_ISDIR(iMode) ) {
		return XFILE_TYPE_DIRECTORY;
	}
	if ( S_ISLNK(iMode) ) {
		return XFILE_TYPE_LINK;
	}
	if ( S_ISFIFO(iMode) ) {
		return XFILE_TYPE_FIFO;
	}
	if ( S_ISSOCK(iMode) ) {
		return XFILE_TYPE_SOCKET;
	}
	if ( S_ISCHR(iMode) || S_ISBLK(iMode) ) {
		return XFILE_TYPE_DEVICE;
	}
	return XFILE_TYPE_OTHER;
}



/* 从 POSIX stat 填充稳定元数据。 */
bool __xrtFilePosixInfo(const struct stat* pNative, xfileinfo* pInfo)
{
	xfileinfo Info;
	int64 iAccessNs = 0;
	int64 iModifyNs = 0;
	int64 iChangeNs = 0;

	memset(&Info, 0, sizeof(Info));
	Info.Type = __xrtFilePosixType(pNative->st_mode);
	Info.Available = XFILE_INFO_MODE | XFILE_INFO_ACCESS_TIME |
		XFILE_INFO_MODIFY_TIME | XFILE_INFO_CHANGE_TIME |
		XFILE_INFO_IDENTITY | XFILE_INFO_LINK_COUNT;
	Info.Mode = (uint32)(pNative->st_mode & 07777);
	if ( pNative->st_size >= (off_t)0 ) {
		Info.Available |= XFILE_INFO_SIZE;
		Info.Size = (uint64)pNative->st_size;
	}
	Info.Device = (uint64)pNative->st_dev;
	Info.Identity = (uint64)pNative->st_ino;
	Info.LinkCount = (uint64)pNative->st_nlink;
	#if defined(__APPLE__)
		iAccessNs = (int64)pNative->st_atimespec.tv_nsec;
		iModifyNs = (int64)pNative->st_mtimespec.tv_nsec;
		iChangeNs = (int64)pNative->st_ctimespec.tv_nsec;
		if ( !__xrtFileTime((int64)pNative->st_birthtimespec.tv_sec,
			(int64)pNative->st_birthtimespec.tv_nsec, &Info.Created) ) {
			return false;
		}
		Info.Available |= XFILE_INFO_CREATE_TIME;
	#else
		iAccessNs = (int64)pNative->st_atim.tv_nsec;
		iModifyNs = (int64)pNative->st_mtim.tv_nsec;
		iChangeNs = (int64)pNative->st_ctim.tv_nsec;
	#endif
	if ( !__xrtFileTime((int64)pNative->st_atime, iAccessNs, &Info.Accessed) ||
		 !__xrtFileTime((int64)pNative->st_mtime, iModifyNs, &Info.Modified) ||
		 !__xrtFileTime((int64)pNative->st_ctime, iChangeNs, &Info.Changed) ) {
		return false;
	}
	*pInfo = Info;
	return true;
}



/* 查询 POSIX 打开句柄元数据，失败时不修改调用方输出。 */
static bool __xrtFilePosixStat(int hFile, xfileinfo* pInfo, bool bReport)
{
	struct stat Native;
	int iResult;

	do {
		iResult = fstat(hFile, &Native);
	} while ( (iResult != 0) && (errno == EINTR) );
	if ( iResult != 0 ) {
		if ( bReport ) {
			int iCode = errno;

			__xrtFileSetError(XFILE_ERROR_STAT, "stat",
				"failed to query file metadata", iCode);
		}
		return false;
	}
	return __xrtFilePosixInfo(&Native, pInfo);
}

#endif



/* 查询打开文件的元数据。 */
XRT_API bool xrtFileStat(xfile File, xfileinfo* pInfo)
{
	if ( (File == NULL) || (pInfo == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		return __xrtFileWindowsStat(File->Handle, pInfo, true);
	#else
		return __xrtFilePosixStat(File->Handle, pInfo, true);
	#endif
}



/* 返回打开文件当前大小。 */
XRT_API bool xrtFileSize(xfile File, uint64* pSize)
{
	xfileinfo Info;

	if ( pSize == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtFileStat(File, &Info) ) {
		return false;
	}
	if ( (Info.Available & XFILE_INFO_SIZE) == 0u ) {
		__xrtFileError(XERR_UNSUPPORTED, XFILE_ERROR_STAT, "size",
			"the file object does not expose a byte size");
		return false;
	}
	*pSize = Info.Size;
	return true;
}



/* 修改打开文件大小。 */
XRT_API bool xrtFileResize(xfile File, uint64 iSize)
{
	if ( !__xrtFileCheck(File, XFILE_WRITE) ) {
		return false;
	}
	if ( (File->Flags & XFILE_APPEND) != 0u ) {
		__xrtFileError(XERR_STATE, XFILE_ERROR_RESIZE, "resize",
			"an append-only file cannot be resized");
		return false;
	}
	if ( iSize > (uint64)INT64_MAX ) {
		__xrtFileError(XERR_RANGE, XFILE_ERROR_RESIZE, "resize",
			"the requested file size is outside the supported range");
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		{
			LARGE_INTEGER Original;
			LARGE_INTEGER Target;
			LARGE_INTEGER Zero;

			Zero.QuadPart = 0;
			Target.QuadPart = (LONGLONG)iSize;
			AcquireSRWLockExclusive(&File->CursorLock);
			if ( !SetFilePointerEx(File->Handle, Zero,
				&Original, FILE_CURRENT) ) {
				int iCode = (int)GetLastError();

				ReleaseSRWLockExclusive(&File->CursorLock);
				__xrtFileSetError(XFILE_ERROR_SEEK, "resize-position",
					"failed to save the shared file position", iCode);
				return false;
			}
			if ( !SetFilePointerEx(File->Handle, Target, NULL, FILE_BEGIN) ||
				 !SetEndOfFile(File->Handle) ) {
				int iCode = (int)GetLastError();

				(void)SetFilePointerEx(File->Handle, Original, NULL, FILE_BEGIN);
				ReleaseSRWLockExclusive(&File->CursorLock);
				__xrtFileSetError(XFILE_ERROR_RESIZE, "resize",
					"failed to resize the file", iCode);
				return false;
			}
			if ( !SetFilePointerEx(File->Handle, Original, NULL, FILE_BEGIN) ) {
				int iCode = (int)GetLastError();

				ReleaseSRWLockExclusive(&File->CursorLock);
				__xrtFileSetError(XFILE_ERROR_SEEK, "resize-restore",
					"the file was resized but its position could not be restored", iCode);
				return false;
			}
			ReleaseSRWLockExclusive(&File->CursorLock);
		}
	#else
		int iResult;

		do {
			iResult = ftruncate(File->Handle, (off_t)iSize);
		} while ( (iResult != 0) && (errno == EINTR) );
		if ( iResult != 0 ) {
			int iCode = errno;

			__xrtFileSetError(XFILE_ERROR_RESIZE, "resize",
				"failed to resize the file", iCode);
			return false;
		}
	#endif
	return true;
}



/* 打开已有路径并修改普通文件大小。 */
XRT_API bool xrtFileSetSize(cstr sPath, uint64 iSize)
{
	xfile File = xrtOpen(sPath, XFILE_WRITE);
	xerror* pError = NULL;
	bool bResult;
	bool bClosed;

	if ( File == NULL ) {
		return false;
	}
	bResult = xrtFileResize(File, iSize);
	if ( !bResult ) {
		pError = xrtTakeError();
	}
	bClosed = xrtClose(File);
	if ( pError != NULL ) {
		xrtClearError();
		__xrtErrorSetOwned(pError);
	}
	return bResult && bClosed;
}



/* 把文件数据和必要元数据提交给稳定存储。 */
XRT_API bool xrtFlush(xfile File)
{
	if ( !__xrtFileCheck(File, XFILE_WRITE) ) {
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( !FlushFileBuffers(File->Handle) ) {
			int iCode = (int)GetLastError();

			__xrtFileSetError(XFILE_ERROR_SYNC, "flush",
				"failed to flush the file", iCode);
			return false;
		}
	#else
		int iResult;

		do {
			iResult = fsync(File->Handle);
		} while ( (iResult != 0) && (errno == EINTR) );
		if ( iResult != 0 ) {
			int iCode = errno;

			__xrtFileSetError(XFILE_ERROR_SYNC, "flush",
				"failed to flush the file", iCode);
			return false;
		}
	#endif
	return true;
}



/* 返回打开文件经过验证的标志。 */
XRT_API uint32 xrtFileFlags(xfile File)
{
	if ( File == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	return File->Flags;
}



/* 返回原生文件句柄的整数表示。 */
XRT_API intptr_t xrtFileNative(xfile File)
{
	if ( File == NULL ) {
		__xrtErrorSetInvalidArgument();
		return (intptr_t)-1;
	}
	#if defined(_WIN32) || defined(_WIN64)
		return (intptr_t)File->Handle;
	#else
		return (intptr_t)File->Handle;
	#endif
}



/* 查询路径元数据的内部实现，可选择是否报告预期查询失败。 */
static bool __xrtPathStatNative(cstr sPath, bool bFollowLink,
	xfileinfo* pInfo, bool bReport)
{
	if ( (sPath == NULL) || (sPath[0] == '\0') || (pInfo == NULL) ) {
		if ( bReport ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		{
			uint16* pPath = __xrtPathToWide(sPath, NULL);
			DWORD iFlags = FILE_FLAG_BACKUP_SEMANTICS;
			HANDLE hFile;
			bool bResult;

			if ( pPath == NULL ) {
				return false;
			}
			if ( !bFollowLink ) {
				iFlags |= FILE_FLAG_OPEN_REPARSE_POINT;
			}
			hFile = CreateFileW((const wchar_t*)pPath, FILE_READ_ATTRIBUTES,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				NULL, OPEN_EXISTING, iFlags, NULL);
			if ( hFile == INVALID_HANDLE_VALUE ) {
				int iCode = bReport ? (int)GetLastError() : 0;

				xrtFree(pPath);
				if ( bReport ) {
					__xrtFileSetError(XFILE_ERROR_STAT, "stat",
						"failed to open the path for metadata", iCode);
				}
				return false;
			}
			xrtFree(pPath);
			bResult = __xrtFileWindowsStat(hFile, pInfo, bReport);
			(void)CloseHandle(hFile);
			return bResult;
		}
	#else
		{
			struct stat Native;
			int iResult;

			do {
				iResult = bFollowLink ? stat(sPath, &Native) : lstat(sPath, &Native);
			} while ( (iResult != 0) && (errno == EINTR) );
			if ( iResult != 0 ) {
				if ( bReport ) {
					int iCode = errno;

					__xrtFileSetError(XFILE_ERROR_STAT, "stat",
						"failed to query path metadata", iCode);
				}
				return false;
			}
			return __xrtFilePosixInfo(&Native, pInfo);
		}
	#endif
}



/* 查询路径元数据。 */
XRT_API bool xrtPathStat(cstr sPath, bool bFollowLink, xfileinfo* pInfo)
{
	return __xrtPathStatNative(sPath, bFollowLink, pInfo, true);
}



/* 查询可选路径并把不存在转换为显式状态。 */
bool __xrtFilePathInfo(cstr sPath, bool bFollowLink,
	bool* pExists, xfileinfo* pInfo)
{
	if ( xrtPathStat(sPath, bFollowLink, pInfo) ) {
		*pExists = true;
		return true;
	}
	if ( (xrtGetError() != NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND) ) {
		xrtClearError();
		*pExists = false;
		return true;
	}
	return false;
}



/* 判断两份元数据是否明确指向同一个文件系统对象。 */
bool __xrtFileInfoSame(const xfileinfo* pLeft, const xfileinfo* pRight)
{
	return ((pLeft->Available & XFILE_INFO_IDENTITY) != 0u) &&
		((pRight->Available & XFILE_INFO_IDENTITY) != 0u) &&
		(pLeft->Device == pRight->Device) &&
		(pLeft->Identity == pRight->Identity);
}



/* 设置路径访问和修改时间。 */
XRT_API bool xrtPathSetTimes(cstr sPath, bool bFollowLink,
	const xtime* pAccessed, const xtime* pModified)
{
	if ( (sPath == NULL) || (sPath[0] == '\0') ||
		 ((pAccessed == NULL) && (pModified == NULL)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		{
			FILETIME Accessed;
			FILETIME Modified;
			const FILETIME* pAccessValue = NULL;
			const FILETIME* pModifyValue = NULL;
			uint16* pPath;
			DWORD iFlags = FILE_FLAG_BACKUP_SEMANTICS;
			HANDLE hFile;
			bool bResult;

			if ( ((pAccessed != NULL) &&
				 !__xrtFileWindowsTimeValue(*pAccessed, &Accessed)) ||
				 ((pModified != NULL) &&
				 !__xrtFileWindowsTimeValue(*pModified, &Modified)) ) {
				return false;
			}
			if ( pAccessed != NULL ) {
				pAccessValue = &Accessed;
			}
			if ( pModified != NULL ) {
				pModifyValue = &Modified;
			}
			pPath = __xrtPathToWide(sPath, NULL);
			if ( pPath == NULL ) {
				return false;
			}
			if ( !bFollowLink ) {
				iFlags |= FILE_FLAG_OPEN_REPARSE_POINT;
			}
			hFile = CreateFileW((const wchar_t*)pPath, FILE_WRITE_ATTRIBUTES,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				NULL, OPEN_EXISTING, iFlags, NULL);
			if ( hFile == INVALID_HANDLE_VALUE ) {
				int iCode = (int)GetLastError();

				xrtFree(pPath);
				__xrtFileSetError(XFILE_ERROR_METADATA, "set-times",
					"failed to open the path for timestamp update", iCode);
				return false;
			}
			xrtFree(pPath);
			bResult = SetFileTime(hFile, NULL,
				pAccessValue, pModifyValue) != 0;
			if ( !bResult ) {
				int iCode = (int)GetLastError();

				(void)CloseHandle(hFile);
				__xrtFileSetError(XFILE_ERROR_METADATA, "set-times",
					"failed to update path timestamps", iCode);
				return false;
			}
			if ( !CloseHandle(hFile) ) {
				int iCode = (int)GetLastError();

				__xrtFileSetError(XFILE_ERROR_CLOSE, "set-times-close",
					"timestamps were updated but the metadata handle did not close", iCode);
				return false;
			}
		}
	#else
		{
			struct timespec Times[2];
			int iFlags = bFollowLink ? 0 : AT_SYMLINK_NOFOLLOW;
			int iResult;

			if ( pAccessed != NULL ) {
				if ( !__xrtFileTimeValue(*pAccessed, &Times[0]) ) {
					return false;
				}
			} else {
				Times[0].tv_sec = 0;
				Times[0].tv_nsec = UTIME_OMIT;
			}
			if ( pModified != NULL ) {
				if ( !__xrtFileTimeValue(*pModified, &Times[1]) ) {
					return false;
				}
			} else {
				Times[1].tv_sec = 0;
				Times[1].tv_nsec = UTIME_OMIT;
			}
			do {
				iResult = utimensat(AT_FDCWD, sPath, Times, iFlags);
			} while ( (iResult != 0) && (errno == EINTR) );
			if ( iResult != 0 ) {
				int iCode = errno;

				__xrtFileSetError(XFILE_ERROR_METADATA, "set-times",
					"failed to update path timestamps", iCode);
				return false;
			}
		}
	#endif
	return true;
}



/* 设置 POSIX 权限模式。 */
XRT_API bool xrtPathSetMode(cstr sPath, bool bFollowLink, uint32 iMode)
{
	if ( (sPath == NULL) || (sPath[0] == '\0') || ((iMode & ~07777u) != 0u) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		(void)bFollowLink;
		__xrtFileError(XERR_UNSUPPORTED, XFILE_ERROR_METADATA, "set-mode",
			"POSIX permission modes are not available on Windows");
		return false;
	#else
		{
			int iResult;

			do {
				if ( bFollowLink ) {
					iResult = chmod(sPath, (mode_t)iMode);
				} else {
					#if defined(AT_SYMLINK_NOFOLLOW)
						iResult = fchmodat(AT_FDCWD, sPath,
							(mode_t)iMode, AT_SYMLINK_NOFOLLOW);
					#else
						__xrtFileError(XERR_UNSUPPORTED, XFILE_ERROR_METADATA,
							"set-mode", "the platform cannot set a link mode safely");
						return false;
					#endif
				}
			} while ( (iResult != 0) && (errno == EINTR) );
			if ( iResult != 0 ) {
				int iCode = errno;

				__xrtFileSetError(XFILE_ERROR_METADATA, "set-mode",
					"failed to update the path permission mode", iCode);
				return false;
			}
		}
		return true;
	#endif
}



/* 设置 Windows 原生文件属性。 */
XRT_API bool xrtPathSetAttributes(cstr sPath, uint32 iAttributes)
{
	if ( (sPath == NULL) || (sPath[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		{
			uint16* pPath = __xrtPathToWide(sPath, NULL);

			if ( pPath == NULL ) {
				return false;
			}
			if ( !SetFileAttributesW((const wchar_t*)pPath, (DWORD)iAttributes) ) {
				int iCode = (int)GetLastError();

				xrtFree(pPath);
				__xrtFileSetError(XFILE_ERROR_METADATA, "set-attributes",
					"failed to update Windows path attributes", iCode);
				return false;
			}
			xrtFree(pPath);
		}
		return true;
	#else
		(void)iAttributes;
		__xrtFileError(XERR_UNSUPPORTED, XFILE_ERROR_METADATA,
			"set-attributes", "Windows file attributes are not available on POSIX");
		return false;
	#endif
}



/* 判断路径是否存在。 */
XRT_API bool xrtPathExists(cstr sPath)
{
	xfileinfo Info;

	return __xrtPathStatNative(sPath, false, &Info, false);
}



/* 判断路径是否为普通文件。 */
XRT_API bool xrtFileExists(cstr sPath)
{
	xfileinfo Info;

	return __xrtPathStatNative(sPath, true, &Info, false) &&
		(Info.Type == XFILE_TYPE_FILE);
}



/* 判断路径是否为目录。 */
XRT_API bool xrtDirExists(cstr sPath)
{
	xfileinfo Info;

	return __xrtPathStatNative(sPath, true, &Info, false) &&
		(Info.Type == XFILE_TYPE_DIRECTORY);
}



/* 在缺少原子排他改名的平台保留功能性回退。 */
#if !defined(_WIN32) && !defined(_WIN64)
static int __xrtFileRenameNoReplaceFallback(cstr sSource, cstr sTarget)
{
	struct stat Info;

	if ( lstat(sSource, &Info) != 0 ) {
		return -1;
	}
	if ( !S_ISDIR(Info.st_mode) ) {
		if ( linkat(AT_FDCWD, sSource, AT_FDCWD, sTarget, 0) != 0 ) {
			return -1;
		}
		if ( unlink(sSource) != 0 ) {
			int iCode = errno;

			(void)unlink(sTarget);
			errno = iCode;
			return -1;
		}
		return 0;
	}
	errno = ENOTSUP;
	return -1;
}
#endif



/* 使用平台原语执行不替换已有目标的同卷改名。 */
#if !defined(_WIN32) && !defined(_WIN64)
static int __xrtFileRenameNoReplace(cstr sSource, cstr sTarget)
{
	#if defined(__linux__) && defined(SYS_renameat2)
		int iResult = (int)syscall(SYS_renameat2, AT_FDCWD, sSource,
			AT_FDCWD, sTarget, RENAME_NOREPLACE);

		if ( (iResult == 0) || ((errno != ENOSYS) && (errno != EINVAL)) ) {
			return iResult;
		}
	#elif defined(__APPLE__)
		return renamex_np(sSource, sTarget, RENAME_EXCL);
	#elif defined(__FreeBSD__)
		return renameatx_np(AT_FDCWD, sSource,
			AT_FDCWD, sTarget, RENAME_EXCL);
	#endif
	return __xrtFileRenameNoReplaceFallback(sSource, sTarget);
}
#endif



/* 在同一卷内重命名文件、链接或目录。 */
XRT_API bool xrtPathRename(cstr sSource, cstr sTarget, bool bReplace)
{
	if ( (sSource == NULL) || (sSource[0] == '\0') ||
		 (sTarget == NULL) || (sTarget[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		{
			uint16* pSource = __xrtPathToWide(sSource, NULL);
			uint16* pTarget;
			DWORD iFlags = MOVEFILE_WRITE_THROUGH;

			if ( pSource == NULL ) {
				return false;
			}
			pTarget = __xrtPathToWide(sTarget, NULL);
			if ( pTarget == NULL ) {
				xrtFree(pSource);
				return false;
			}
			if ( bReplace ) {
				iFlags |= MOVEFILE_REPLACE_EXISTING;
			}
			if ( !MoveFileExW((const wchar_t*)pSource,
				(const wchar_t*)pTarget, iFlags) ) {
				int iCode = (int)GetLastError();

				xrtFree(pTarget);
				xrtFree(pSource);
				__xrtFileSetError(XFILE_ERROR_MOVE, "rename",
					"failed to rename the file", iCode);
				return false;
			}
			xrtFree(pTarget);
			xrtFree(pSource);
		}
	#else
		if ( bReplace ) {
			if ( rename(sSource, sTarget) != 0 ) {
				int iCode = errno;

				__xrtFileSetError(XFILE_ERROR_MOVE, "rename",
					"failed to rename the file", iCode);
				return false;
			}
		} else {
			if ( __xrtFileRenameNoReplace(sSource, sTarget) != 0 ) {
				int iCode = errno;

				__xrtFileSetError(XFILE_ERROR_MOVE, "rename",
					"failed to rename without replacing the target", iCode);
				return false;
			}
		}
	#endif
	return true;
}



/* 删除一个非目录文件或符号链接。 */
XRT_API bool xrtFileDelete(cstr sPath)
{
	if ( (sPath == NULL) || (sPath[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		{
			uint16* pPath = __xrtPathToWide(sPath, NULL);

			if ( pPath == NULL ) {
				return false;
			}
			if ( !DeleteFileW((const wchar_t*)pPath) ) {
				int iCode = (int)GetLastError();

				xrtFree(pPath);
				__xrtFileSetError(XFILE_ERROR_DELETE, "delete",
					"failed to delete the file", iCode);
				return false;
			}
			xrtFree(pPath);
		}
	#else
		if ( unlink(sPath) != 0 ) {
			int iCode = errno;

			__xrtFileSetError(XFILE_ERROR_DELETE, "delete",
				"failed to delete the file", iCode);
			return false;
		}
	#endif
	return true;
}



/* 创建空文件或把已有对象的访问和修改时间更新为当前时刻。 */
XRT_API bool xrtFileTouch(cstr sPath)
{
	if ( (sPath == NULL) || (sPath[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		{
			uint16* pPath = __xrtPathToWide(sPath, NULL);
			FILETIME Now;
			HANDLE hFile;

			if ( pPath == NULL ) {
				return false;
			}
			hFile = CreateFileW((const wchar_t*)pPath, FILE_WRITE_ATTRIBUTES,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if ( hFile == INVALID_HANDLE_VALUE ) {
				int iCode = (int)GetLastError();

				xrtFree(pPath);
				__xrtFileSetError(XFILE_ERROR_TOUCH, "touch",
					"failed to create or open the touched file", iCode);
				return false;
			}
			xrtFree(pPath);
			GetSystemTimeAsFileTime(&Now);
			if ( !SetFileTime(hFile, NULL, &Now, &Now) ) {
				int iCode = (int)GetLastError();

				(void)CloseHandle(hFile);
				__xrtFileSetError(XFILE_ERROR_TOUCH, "touch",
					"failed to update the touched file timestamps", iCode);
				return false;
			}
			if ( !CloseHandle(hFile) ) {
				int iCode = (int)GetLastError();

				__xrtFileSetError(XFILE_ERROR_CLOSE, "touch-close",
					"the touched file handle did not close", iCode);
				return false;
			}
		}
	#else
		{
			int iResult;

			do {
				iResult = utimensat(AT_FDCWD, sPath, NULL, 0);
			} while ( (iResult != 0) && (errno == EINTR) );
			if ( iResult != 0 ) {
				int iCode = errno;
				int hFile;

				if ( iCode != ENOENT ) {
					__xrtFileSetError(XFILE_ERROR_TOUCH, "touch",
						"failed to update the touched file timestamps",
						iCode);
					return false;
				}
				hFile = __xrtFilePosixOpenAt(AT_FDCWD, sPath,
					__xrtFilePosixFlags(XFILE_WRITE | XFILE_CREATE),
					0666u);
				if ( hFile < 0 ) {
					iCode = errno;
					__xrtFileSetError(XFILE_ERROR_TOUCH, "touch",
						"failed to create the touched file", iCode);
					return false;
				}
				do {
					iResult = futimens(hFile, NULL);
				} while ( (iResult != 0) && (errno == EINTR) );
				if ( iResult != 0 ) {
					iCode = errno;
					(void)close(hFile);
					__xrtFileSetError(XFILE_ERROR_TOUCH, "touch",
						"failed to update the touched file timestamps",
						iCode);
					return false;
				}
				if ( close(hFile) != 0 ) {
					iCode = errno;
					__xrtFileSetError(XFILE_ERROR_CLOSE, "touch-close",
						"the touched file descriptor did not close", iCode);
					return false;
				}
			}
		}
	#endif
	return true;
}

#endif
