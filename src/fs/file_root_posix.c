#if !defined(_WIN32) && !defined(_WIN64)
	#if !defined(_POSIX_C_SOURCE)
		#define _POSIX_C_SOURCE 200809L
	#endif
	#if !defined(_FILE_OFFSET_BITS)
		#define _FILE_OFFSET_BITS 64
	#endif
#endif

#include "../internal/xrt_file_root.h"

#if !defined(_WIN32) && !defined(_WIN64)
	#include <errno.h>
	#include <fcntl.h>
	#include <sys/stat.h>
	#include <sys/types.h>
	#include <unistd.h>
#endif



#if defined(XRT_FEATURE_FILE_ROOT) && \
	!defined(_WIN32) && !defined(_WIN64)

/* 判断 ENOTDIR 探测失败时哪些链接错误不得被覆盖。 */
static bool __xrtRootPosixKeepLinkError(void)
{
	const xerror* pError = xrtGetError();
	xerrkind Kind;

	if ( pError == NULL ) {
		return false;
	}
	Kind = xrtErrorKind(pError);
	return (Kind == XERR_MEMORY) || (Kind == XERR_RANGE) ||
		(Kind == XERR_PROTOCOL) || (Kind == XERR_UNSUPPORTED);
}



/* 在打开失败后检查当前分量是否为符号链接。 */
static xrootstep __xrtRootPosixLink(int hDirectory,
	cstr sName, int iOpenCode, str* pLink, xrooterror Code,
	cstr sOperation, cstr sMessage)
{
	str sTarget = __xrtLinkReadAt(hDirectory, sName);

	if ( sTarget != NULL ) {
		*pLink = sTarget;
		return XROOT_STEP_LINK;
	}
	if ( (iOpenCode == ELOOP) || __xrtRootPosixKeepLinkError() ) {
		__xrtRootWrapError(Code, sOperation, sMessage);
		return XROOT_STEP_ERROR;
	}
	xrtClearError();
	__xrtRootSetError(Code, sOperation, sMessage, iOpenCode);
	return XROOT_STEP_ERROR;
}



/* 打开并验证初始 POSIX 根目录。 */
bool __xrtRootNativeOpen(cstr sPath, xrootnative* pHandle)
{
	int iFlags = O_RDONLY;
	int hDirectory;

	#if defined(O_DIRECTORY)
		iFlags |= O_DIRECTORY;
	#else
		__xrtRootError(XERR_UNSUPPORTED, XROOT_ERROR_OPEN,
			"open", "the platform does not provide directory-only open");
		return false;
	#endif
	hDirectory = __xrtFilePosixOpenAt(AT_FDCWD, sPath, iFlags, 0u);
	if ( hDirectory < 0 ) {
		int iCode = errno;

		__xrtRootSetError(XROOT_ERROR_OPEN, "open",
			"failed to open the root directory", iCode);
		return false;
	}
	*pHandle = hDirectory;
	return true;
}



/* 关闭 POSIX 根目录或中间目录描述符。 */
bool __xrtRootNativeClose(xrootnative Handle, bool bReport)
{
	int iResult = close(Handle);

	if ( iResult == 0 ) {
		return true;
	}
	if ( bReport ) {
		int iCode = errno;

		__xrtRootSetError(XROOT_ERROR_CLOSE, "close",
			"failed to close the root directory", iCode);
	}
	return false;
}



/* 不跟随当前分量打开 POSIX 子目录。 */
xrootstep __xrtRootNativeOpenDir(xrootnative Parent, cstr sName,
	xrootnative* pHandle, str* pLink)
{
	int iFlags = O_RDONLY;
	int hDirectory;

	#if defined(O_DIRECTORY) && defined(O_NOFOLLOW)
		iFlags |= O_DIRECTORY | O_NOFOLLOW;
	#else
		__xrtRootError(XERR_UNSUPPORTED, XROOT_ERROR_RESOLVE,
			"resolve", "the platform cannot traverse directories without following links");
		return XROOT_STEP_ERROR;
	#endif
	hDirectory = __xrtFilePosixOpenAt(Parent, sName, iFlags, 0u);
	if ( hDirectory >= 0 ) {
		*pHandle = hDirectory;
		return XROOT_STEP_DONE;
	}
	{
		int iCode = errno;

		if ( (iCode == ELOOP) || (iCode == ENOTDIR) ) {
			return __xrtRootPosixLink(Parent, sName, iCode,
				pLink, XROOT_ERROR_RESOLVE, "resolve",
				"failed to open a root path directory");
		}
		__xrtRootSetError(XROOT_ERROR_RESOLVE, "resolve",
			"failed to open a root path directory", iCode);
		return XROOT_STEP_ERROR;
	}
}



/* 不跟随当前分量打开 POSIX 普通文件。 */
xrootstep __xrtRootNativeOpenFile(xrootnative Parent, cstr sName,
	const xfileoptions* pOptions, xfile* pFile, str* pLink)
{
	int iFlags = __xrtFilePosixFlags(pOptions->Flags);
	int hFile;
	struct stat Info;

	#if defined(O_NOFOLLOW)
		iFlags |= O_NOFOLLOW;
	#else
		__xrtRootError(XERR_UNSUPPORTED, XROOT_ERROR_FILE,
			"open-file", "the platform cannot open files without following links");
		return XROOT_STEP_ERROR;
	#endif
	hFile = __xrtFilePosixOpenAt(Parent, sName,
		iFlags, pOptions->Mode);
	if ( hFile < 0 ) {
		int iCode = errno;
		bool bExclusive = (pOptions->Flags &
			(XFILE_CREATE | XFILE_EXCLUSIVE)) ==
			(XFILE_CREATE | XFILE_EXCLUSIVE);

		if ( ((iCode == ELOOP) || (iCode == ENOTDIR)) &&
			 ((pOptions->Flags & XFILE_NOFOLLOW) == 0u) &&
			 !bExclusive ) {
			return __xrtRootPosixLink(Parent, sName, iCode,
				pLink, XROOT_ERROR_FILE, "open-file",
				"failed to open the root-relative file");
		}
		__xrtRootSetError(XROOT_ERROR_FILE, "open-file",
			"failed to open the root-relative file", iCode);
		return XROOT_STEP_ERROR;
	}
	{
		int iResult;

		do {
			iResult = fstat(hFile, &Info);
		} while ( (iResult != 0) && (errno == EINTR) );
		if ( iResult != 0 ) {
			int iCode = errno;

			(void)close(hFile);
			__xrtRootSetError(XROOT_ERROR_FILE, "open-file",
				"failed to inspect the root-relative file", iCode);
			return XROOT_STEP_ERROR;
		}
	}
	if ( S_ISDIR(Info.st_mode) ) {
		(void)close(hFile);
		__xrtRootError(XERR_TYPE, XROOT_ERROR_FILE, "open-file",
			"the root-relative path is a directory");
		return XROOT_STEP_ERROR;
	}
	*pFile = __xrtFileTakeNative((intptr_t)hFile, pOptions->Flags);
	return *pFile != NULL ? XROOT_STEP_DONE : XROOT_STEP_ERROR;
}



/* 查询 POSIX 根内当前分量元数据。 */
xrootstep __xrtRootNativeStat(xrootnative Parent, cstr sName,
	bool bFollowLink, xfileinfo* pInfo, str* pLink)
{
	struct stat Native;
	int iResult;

	if ( strcmp(sName, ".") == 0 ) {
		(void)bFollowLink;
		(void)pLink;
		do {
			iResult = fstat(Parent, &Native);
		} while ( (iResult != 0) && (errno == EINTR) );
	} else {
		do {
			iResult = fstatat(Parent, sName,
				&Native, AT_SYMLINK_NOFOLLOW);
		} while ( (iResult != 0) && (errno == EINTR) );
	}
	if ( iResult != 0 ) {
		int iCode = errno;

		__xrtRootSetError(XROOT_ERROR_STAT, "stat",
			"failed to query root-relative metadata", iCode);
		return XROOT_STEP_ERROR;
	}
	if ( bFollowLink && S_ISLNK(Native.st_mode) ) {
		*pLink = __xrtLinkReadAt(Parent, sName);
		if ( *pLink == NULL ) {
			__xrtRootWrapError(XROOT_ERROR_STAT, "stat",
				"failed to resolve root-relative link metadata");
			return XROOT_STEP_ERROR;
		}
		return XROOT_STEP_LINK;
	}
	if ( !__xrtFilePosixInfo(&Native, pInfo) ) {
		__xrtRootWrapError(XROOT_ERROR_STAT, "stat",
			"failed to convert root-relative metadata");
		return XROOT_STEP_ERROR;
	}
	return XROOT_STEP_DONE;
}



/* 相对 POSIX 父目录创建一个新目录。 */
bool __xrtRootNativeCreateDir(xrootnative Parent,
	cstr sName, uint32 iMode)
{
	int iResult;

	do {
		iResult = mkdirat(Parent, sName, (mode_t)iMode);
	} while ( (iResult != 0) && (errno == EINTR) );
	if ( iResult == 0 ) {
		return true;
	}
	{
		int iCode = errno;

		__xrtRootSetError(XROOT_ERROR_CREATE, "create-dir",
			"failed to create the root-relative directory", iCode);
		return false;
	}
}



/* 相对 POSIX 父目录删除非目录对象或空目录。 */
bool __xrtRootNativeRemove(xrootnative Parent,
	cstr sName, bool bDirectoryOnly)
{
	int iFileResult;
	int iFileCode;
	int iDirResult;
	int iDirCode;

	iFileResult = -1;
	iFileCode = ENOTDIR;
	if ( !bDirectoryOnly ) {
		do {
			iFileResult = unlinkat(Parent, sName, 0);
		} while ( (iFileResult != 0) && (errno == EINTR) );
		if ( iFileResult == 0 ) {
			return true;
		}
		iFileCode = errno;
	}
	do {
		iDirResult = unlinkat(Parent, sName, AT_REMOVEDIR);
	} while ( (iDirResult != 0) && (errno == EINTR) );
	if ( iDirResult == 0 ) {
		return true;
	}
	iDirCode = errno;
	__xrtRootSetError(XROOT_ERROR_REMOVE, "remove",
		"failed to remove the root-relative object",
		iDirCode != ENOTDIR ? iDirCode : iFileCode);
	return false;
}



/* 相对 POSIX 父目录读取末级符号链接。 */
str __xrtRootNativeReadLink(xrootnative Parent, cstr sName)
{
	str sTarget = __xrtLinkReadAt(Parent, sName);

	if ( sTarget == NULL ) {
		__xrtRootWrapError(XROOT_ERROR_LINK, "read-link",
			"failed to read the root-relative symbolic link");
	}
	return sTarget;
}





/* 相对 POSIX 父目录创建符号链接。 */
bool __xrtRootNativeLinkCreate(xrootnative Parent,
	cstr sName, cstr sTarget, bool bDirectory)
{
	int iResult;

	(void)bDirectory;
	do {
		iResult = symlinkat(sTarget, Parent, sName);
	} while ( (iResult != 0) && (errno == EINTR) );
	if ( iResult == 0 ) { return true; }
	{
		int iCode = errno;

		__xrtRootSetError(XROOT_ERROR_LINK, "create-link",
			"failed to create the root-relative symbolic link", iCode);
		return false;
	}
}



/* 在两个 POSIX 父目录描述符之间创建硬链接。 */
bool __xrtRootNativeLinkHard(xrootnative SourceParent, cstr sSource,
	xrootnative TargetParent, cstr sTarget)
{
	int iResult;

	do {
		iResult = linkat(SourceParent, sSource,
			TargetParent, sTarget, 0);
	} while ( (iResult != 0) && (errno == EINTR) );
	if ( iResult == 0 ) { return true; }
	{
		int iCode = errno;

		__xrtRootSetError(XROOT_ERROR_LINK, "hard-link",
			"failed to create the root-relative hard link", iCode);
		return false;
	}
}



/* 相对 POSIX 父目录创建 FIFO。 */
bool __xrtRootNativeFifoCreate(xrootnative Parent,
	cstr sName, uint32 iMode)
{
	int iResult;

	do {
		iResult = mkfifoat(Parent, sName, (mode_t)iMode);
	} while ( (iResult != 0) && (errno == EINTR) );
	if ( iResult == 0 ) { return true; }
	{
		int iCode = errno;

		__xrtRootSetError(XROOT_ERROR_CREATE, "create-fifo",
			"failed to create the root-relative FIFO", iCode);
		return false;
	}
}



/* 通过不跟随链接的对象描述符设置 POSIX 权限。 */
xrootstep __xrtRootNativeSetMode(xrootnative Parent,
	cstr sName, bool bFollowLink, uint32 iMode, str* pLink)
{
	int iFlags = O_RDONLY;
	int hObject;
	int iResult;

	#if defined(O_NOFOLLOW)
		iFlags |= O_NOFOLLOW;
	#else
		__xrtRootError(XERR_UNSUPPORTED, XROOT_ERROR_STAT,
			"set-mode", "the platform cannot set root-relative modes without following links");
		return XROOT_STEP_ERROR;
	#endif
	#if defined(O_NONBLOCK)
		iFlags |= O_NONBLOCK;
	#endif
	#if defined(O_CLOEXEC)
		iFlags |= O_CLOEXEC;
	#endif
	hObject = __xrtFilePosixOpenAt(Parent, sName, iFlags, 0u);
	if ( hObject < 0 ) {
		int iCode = errno;

		if ( bFollowLink && ((iCode == ELOOP) || (iCode == ENOTDIR)) ) {
			return __xrtRootPosixLink(Parent, sName, iCode,
				pLink, XROOT_ERROR_STAT, "set-mode",
				"failed to open the root-relative mode target");
		}
		__xrtRootSetError(XROOT_ERROR_STAT, "set-mode",
			"failed to open the root-relative mode target", iCode);
		return XROOT_STEP_ERROR;
	}
	do {
		iResult = fchmod(hObject, (mode_t)iMode);
	} while ( (iResult != 0) && (errno == EINTR) );
	if ( iResult != 0 ) {
		int iCode = errno;

		(void)close(hObject);
		__xrtRootSetError(XROOT_ERROR_STAT, "set-mode",
			"failed to set the root-relative object mode", iCode);
		return XROOT_STEP_ERROR;
	}
	if ( close(hObject) != 0 ) {
		int iCode = errno;

		__xrtRootSetError(XROOT_ERROR_CLOSE, "set-mode-close",
			"the root-relative mode target did not close", iCode);
		return XROOT_STEP_ERROR;
	}
	return XROOT_STEP_DONE;
}

#endif
