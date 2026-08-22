#if !defined(_WIN32) && !defined(_WIN64) && !defined(_POSIX_C_SOURCE)
	#define _POSIX_C_SOURCE 200809L
#endif

#include "../internal/xrt_file.h"

#include <errno.h>

#if !defined(_WIN32) && !defined(_WIN64)
	#include <fcntl.h>
#endif



#if defined(XRT_FEATURE_FILE_LOCK)

/* 检查锁模式、文件权限和跨平台可表达的偏移范围。 */
static bool __xrtFileLockCheck(xfile File, xfilelock Mode,
	uint64 iOffset, uint64 iSize)
{
	uint32 iFlags;

	if ( (File == NULL) ||
		 ((Mode != XFILE_LOCK_SHARED) &&
		  (Mode != XFILE_LOCK_EXCLUSIVE)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iFlags = xrtFileFlags(File);
	if ( ((Mode == XFILE_LOCK_SHARED) &&
		 ((iFlags & XFILE_READ) == 0u)) ||
		 ((Mode == XFILE_LOCK_EXCLUSIVE) &&
		 ((iFlags & XFILE_WRITE) == 0u)) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( (iOffset > (uint64)INT64_MAX) ||
		 ((iSize != 0u) &&
		  ((iSize - 1u) > ((uint64)INT64_MAX - iOffset))) ) {
		__xrtFileError(XERR_RANGE, XFILE_ERROR_LOCK, "lock-range",
			"the file lock range is outside the supported range");
		return false;
	}
	return true;
}



/* 把系统忙状态转换为可重试的稳定文件错误。 */
static void __xrtFileLockBusy(cstr sOperation, int iCode)
{
	__xrtFileSetKindError(XERR_AGAIN, XFILE_ERROR_LOCK,
		sOperation, "the requested file range is already locked", iCode);
}



/* 锁定一个文件字节区间。 */
XRT_API bool xrtFileLockRange(xfile File, xfilelock Mode,
	uint64 iOffset, uint64 iSize, bool bWait)
{
	intptr_t hFile;

	if ( !__xrtFileLockCheck(File, Mode, iOffset, iSize) ) {
		return false;
	}
	hFile = __xrtFileControlNative(File);
	if ( hFile == (intptr_t)-1 ) {
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		{
			OVERLAPPED Position;
			DWORD iFlags = (Mode == XFILE_LOCK_EXCLUSIVE) ?
				LOCKFILE_EXCLUSIVE_LOCK : 0u;
			uint64 iLength = (iSize == 0u) ? UINT64_MAX : iSize;

			if ( !bWait ) {
				iFlags |= LOCKFILE_FAIL_IMMEDIATELY;
			}
			memset(&Position, 0, sizeof(Position));
			Position.Offset = (DWORD)iOffset;
			Position.OffsetHigh = (DWORD)(iOffset >> 32);
			if ( !LockFileEx((HANDLE)hFile, iFlags, 0u,
				(DWORD)iLength, (DWORD)(iLength >> 32), &Position) ) {
				int iCode = (int)GetLastError();

				if ( !bWait && (iCode == ERROR_LOCK_VIOLATION) ) {
					__xrtFileLockBusy("lock", iCode);
				} else {
					__xrtFileSetError(XFILE_ERROR_LOCK, "lock",
						"failed to lock the file range", iCode);
				}
				return false;
			}
		}
	#else
		{
			struct flock Lock;
			int iCommand = bWait ? F_SETLKW : F_SETLK;
			int iResult;

			memset(&Lock, 0, sizeof(Lock));
			Lock.l_type = (Mode == XFILE_LOCK_SHARED) ?
				F_RDLCK : F_WRLCK;
			Lock.l_whence = SEEK_SET;
			Lock.l_start = (off_t)iOffset;
			Lock.l_len = (off_t)iSize;
			do {
				iResult = fcntl((int)hFile, iCommand, &Lock);
			} while ( bWait && (iResult != 0) && (errno == EINTR) );
			if ( iResult != 0 ) {
				int iCode = errno;

				if ( !bWait &&
					 ((iCode == EACCES) || (iCode == EAGAIN)) ) {
					__xrtFileLockBusy("lock", iCode);
				} else {
					__xrtFileSetError(XFILE_ERROR_LOCK, "lock",
						"failed to lock the file range", iCode);
				}
				return false;
			}
		}
	#endif
	return true;
}



/* 解除一个文件字节区间锁。 */
XRT_API bool xrtFileUnlockRange(xfile File,
	uint64 iOffset, uint64 iSize)
{
	intptr_t hFile;
	xfilelock Mode;

	if ( File == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Mode = ((xrtFileFlags(File) & XFILE_READ) != 0u) ?
		XFILE_LOCK_SHARED : XFILE_LOCK_EXCLUSIVE;
	if ( !__xrtFileLockCheck(File, Mode, iOffset, iSize) ) {
		return false;
	}
	hFile = __xrtFileControlNative(File);
	if ( hFile == (intptr_t)-1 ) {
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		{
			OVERLAPPED Position;
			uint64 iLength = (iSize == 0u) ? UINT64_MAX : iSize;

			memset(&Position, 0, sizeof(Position));
			Position.Offset = (DWORD)iOffset;
			Position.OffsetHigh = (DWORD)(iOffset >> 32);
			if ( !UnlockFileEx((HANDLE)hFile, 0u,
				(DWORD)iLength, (DWORD)(iLength >> 32), &Position) ) {
				int iCode = (int)GetLastError();

				__xrtFileSetError(XFILE_ERROR_LOCK, "unlock",
					"failed to unlock the file range", iCode);
				return false;
			}
		}
	#else
		{
			struct flock Lock;
			int iResult;

			memset(&Lock, 0, sizeof(Lock));
			Lock.l_type = F_UNLCK;
			Lock.l_whence = SEEK_SET;
			Lock.l_start = (off_t)iOffset;
			Lock.l_len = (off_t)iSize;
			do {
				iResult = fcntl((int)hFile, F_SETLK, &Lock);
			} while ( (iResult != 0) && (errno == EINTR) );
			if ( iResult != 0 ) {
				int iCode = errno;

				__xrtFileSetError(XFILE_ERROR_LOCK, "unlock",
					"failed to unlock the file range", iCode);
				return false;
			}
		}
	#endif
	return true;
}



/* 锁定整个文件。 */
XRT_API bool xrtFileLock(xfile File, xfilelock Mode, bool bWait)
{
	return xrtFileLockRange(File, Mode, 0u, 0u, bWait);
}



/* 解除整个文件锁。 */
XRT_API bool xrtFileUnlock(xfile File)
{
	return xrtFileUnlockRange(File, 0u, 0u);
}

#endif
