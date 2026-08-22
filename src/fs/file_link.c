#if !defined(_WIN32) && !defined(_WIN64)
	#if !defined(_POSIX_C_SOURCE)
		#define _POSIX_C_SOURCE 200809L
	#endif
#endif

#include "../internal/xrt_file_link.h"

#include <errno.h>

#if !defined(_WIN32) && !defined(_WIN64)
	#include <fcntl.h>
	#include <sys/stat.h>
	#include <sys/types.h>
	#include <unistd.h>
#endif



#if defined(XRT_FEATURE_FILE_LINK)

/* 设置链接模块结构化错误。 */
static void __xrtLinkSetError(xlinkerror Code, cstr sOperation,
	cstr sMessage, int iSystemCode)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = __xrtSystemErrorKind(iSystemCode);
	Desc.Domain = "xrt.link";
	Desc.Code = (int32)Code;
	Desc.SystemCode = (int32)iSystemCode;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 设置不带系统代码的链接错误。 */
static void __xrtLinkError(xerrkind Kind, xlinkerror Code,
	cstr sOperation, cstr sMessage)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.link";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 检查两个必需路径参数。 */
static bool __xrtLinkPaths(cstr sFirst, cstr sSecond)
{
	if ( (sFirst == NULL) || (sFirst[0] == '\0') ||
		 (sSecond == NULL) || (sSecond[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



#if defined(_WIN32) || defined(_WIN64)

#if !defined(SYMBOLIC_LINK_FLAG_DIRECTORY)
	#define SYMBOLIC_LINK_FLAG_DIRECTORY 0x1u
#endif

#if !defined(SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)
	#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2u
#endif

#if !defined(IO_REPARSE_TAG_SYMLINK)
	#define IO_REPARSE_TAG_SYMLINK ((ULONG)0xA000000Cu)
#endif

#if !defined(IO_REPARSE_TAG_MOUNT_POINT)
	#define IO_REPARSE_TAG_MOUNT_POINT ((ULONG)0xA0000003u)
#endif

#if !defined(FSCTL_GET_REPARSE_POINT)
	#define FSCTL_GET_REPARSE_POINT ((DWORD)0x000900A8u)
#endif

#if defined(__TINYC__)
WINBASEAPI BOOLEAN WINAPI CreateSymbolicLinkW(
	LPCWSTR lpSymlinkFileName, LPCWSTR lpTargetFileName, DWORD dwFlags);
#endif

#define XRT_LINK_REPARSE_BUFFER (16u * 1024u)
#define XRT_LINK_REPARSE_HEADER 8u



/* Windows 符号链接 reparse 数据头。 */
typedef struct __xrt_link_symlink_data {
	ULONG Tag;
	USHORT DataLength;
	USHORT Reserved;
	USHORT SubstituteOffset;
	USHORT SubstituteLength;
	USHORT PrintOffset;
	USHORT PrintLength;
	ULONG Flags;
	WCHAR Path[1];
} __xrt_link_symlink_data;



/* Windows junction reparse 数据头。 */
typedef struct __xrt_link_mount_data {
	ULONG Tag;
	USHORT DataLength;
	USHORT Reserved;
	USHORT SubstituteOffset;
	USHORT SubstituteLength;
	USHORT PrintOffset;
	USHORT PrintLength;
	WCHAR Path[1];
} __xrt_link_mount_data;



/* 校验 reparse 数据声明长度，并返回真实路径缓冲长度。 */
static bool __xrtLinkWindowsPathBytes(DWORD iRead, USHORT iDataLength,
	size_t iPathOffset, size_t* pPathBytes)
{
	size_t iFixedSize = iPathOffset - XRT_LINK_REPARSE_HEADER;

	if (((size_t)iRead < iPathOffset) ||
		 ((size_t)iDataLength < iFixedSize) ||
		 ((size_t)iDataLength >
		  ((size_t)iRead - XRT_LINK_REPARSE_HEADER)) ) {
		__xrtLinkError(XERR_PROTOCOL, XLINK_ERROR_FORMAT, "read",
			"the Windows reparse data has invalid bounds");
		return false;
	}
	*pPathBytes = (size_t)iDataLength - iFixedSize;
	return true;
}



/* 把 NT substitute 名称转换为可供 Win32 再次使用的路径。 */
static str __xrtLinkWindowsSubstitute(const WCHAR* pText, size_t iUnits)
{
	WCHAR* pNormalized;
	str sResult;

	if ( (iUnits < 4u) || (pText[0] != L'\\') ||
		 (pText[1] != L'?') || (pText[2] != L'?') ||
		 (pText[3] != L'\\') ) {
		return __xrtPathFromWide(pText, iUnits);
	}
	pText += 4;
	iUnits -= 4u;

	/* DOS 驱动器路径去掉 NT 前缀即可。 */
	if ( (iUnits >= 2u) && (pText[1] == L':') ) {
		return __xrtPathFromWide(pText, iUnits);
	}

	/* NT 的 UNC\server\share 需要恢复为 \\server\share。 */
	if ( (iUnits >= 4u) &&
		 ((pText[0] == L'U') || (pText[0] == L'u')) &&
		 ((pText[1] == L'N') || (pText[1] == L'n')) &&
		 ((pText[2] == L'C') || (pText[2] == L'c')) &&
		 (pText[3] == L'\\') ) {
		size_t iNormalized = iUnits - 2u;

		pNormalized = (WCHAR*)xrtMalloc(
			(iNormalized + 1u) * sizeof(WCHAR));
		if ( pNormalized == NULL ) {
			return NULL;
		}
		pNormalized[0] = L'\\';
		pNormalized[1] = L'\\';
		memcpy(pNormalized + 2u, pText + 4u,
			(iUnits - 4u) * sizeof(WCHAR));
		pNormalized[iNormalized] = L'\0';
		sResult = __xrtPathFromWide(pNormalized, iNormalized);
		xrtFree(pNormalized);
		return sResult;
	}

	/* 其他 NT 名称保留为等价的 Win32 扩展路径。 */
	pText -= 4;
	iUnits += 4u;
	pNormalized = (WCHAR*)xrtMalloc((iUnits + 1u) * sizeof(WCHAR));
	if ( pNormalized == NULL ) {
		return NULL;
	}
	memcpy(pNormalized, pText, iUnits * sizeof(WCHAR));
	pNormalized[1] = L'\\';
	pNormalized[iUnits] = L'\0';
	sResult = __xrtPathFromWide(pNormalized, iUnits);
	xrtFree(pNormalized);
	return sResult;
}



/* 从 reparse 路径缓冲中提取并转换一个 UTF-16 片段。 */
static str __xrtLinkWindowsText(const WCHAR* pPath, size_t iPathBytes,
	USHORT iOffset, USHORT iLength, bool bSubstitute)
{
	const WCHAR* pText;
	size_t iUnits;

	if ( ((size_t)iOffset > iPathBytes) ||
		 ((size_t)iLength > (iPathBytes - (size_t)iOffset)) ||
		 ((iOffset & 1u) != 0u) || ((iLength & 1u) != 0u) ) {
		__xrtLinkError(XERR_PROTOCOL, XLINK_ERROR_FORMAT, "read",
			"the Windows reparse path has invalid bounds");
		return NULL;
	}
	pText = (const WCHAR*)((const unsigned char*)pPath + iOffset);
	iUnits = (size_t)iLength / sizeof(WCHAR);
	if ( bSubstitute ) {
		return __xrtLinkWindowsSubstitute(pText, iUnits);
	}
	return __xrtPathFromWide(pText, iUnits);
}



/* 创建 Windows 符号链接，旧系统不识别无特权标志时自动重试。 */
static bool __xrtLinkWindowsCreate(cstr sTarget, cstr sLink, bool bDirectory)
{
	uint16* pTarget = __xrtPathToWide(sTarget, NULL);
	uint16* pLink;
	DWORD iFlags = bDirectory ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0u;
	BOOLEAN bResult;

	if ( pTarget == NULL ) {
		return false;
	}
	pLink = __xrtPathToWide(sLink, NULL);
	if ( pLink == NULL ) {
		xrtFree(pTarget);
		return false;
	}
	bResult = CreateSymbolicLinkW((const wchar_t*)pLink,
		(const wchar_t*)pTarget,
		iFlags | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE);
	if ( !bResult && (GetLastError() == ERROR_INVALID_PARAMETER) ) {
		bResult = CreateSymbolicLinkW((const wchar_t*)pLink,
			(const wchar_t*)pTarget, iFlags);
	}
	if ( !bResult ) {
		int iCode = (int)GetLastError();

		xrtFree(pLink);
		xrtFree(pTarget);
		__xrtLinkSetError(XLINK_ERROR_CREATE, "create",
			"failed to create the symbolic link", iCode);
		return false;
	}
	xrtFree(pLink);
	xrtFree(pTarget);
	return true;
}



/* 从已经打开的 reparse point 句柄读取符号链接或 junction 目标。 */
str __xrtLinkWindowsReadHandle(HANDLE hLink, bool bResolve)
{
	bytes pBuffer;
	DWORD iRead = 0;
	str sResult = NULL;

	if ( (hLink == NULL) || (hLink == INVALID_HANDLE_VALUE) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pBuffer = (bytes)xrtMalloc(XRT_LINK_REPARSE_BUFFER);
	if ( pBuffer == NULL ) {
		return NULL;
	}
	if ( !DeviceIoControl(hLink, FSCTL_GET_REPARSE_POINT,
		NULL, 0, pBuffer, XRT_LINK_REPARSE_BUFFER, &iRead, NULL) ) {
		int iCode = (int)GetLastError();

		xrtFree(pBuffer);
		__xrtLinkSetError(XLINK_ERROR_READ, "read",
			"failed to read the symbolic link data", iCode);
		return NULL;
	}
	if ( (iRead >= offsetof(__xrt_link_symlink_data, Path)) &&
		 (((__xrt_link_symlink_data*)pBuffer)->Tag == IO_REPARSE_TAG_SYMLINK) ) {
		__xrt_link_symlink_data* pData = (__xrt_link_symlink_data*)pBuffer;
		size_t iPathBytes;
		bool bPrint = !bResolve && (pData->PrintLength != 0u);
		USHORT iOffset = bPrint ?
			pData->PrintOffset : pData->SubstituteOffset;
		USHORT iLength = bPrint ?
			pData->PrintLength : pData->SubstituteLength;

		if ( __xrtLinkWindowsPathBytes(iRead, pData->DataLength,
			offsetof(__xrt_link_symlink_data, Path), &iPathBytes) ) {
			sResult = __xrtLinkWindowsText(pData->Path, iPathBytes,
				iOffset, iLength, !bPrint);
		}
	} else if ( (iRead >= offsetof(__xrt_link_mount_data, Path)) &&
		 (((__xrt_link_mount_data*)pBuffer)->Tag == IO_REPARSE_TAG_MOUNT_POINT) ) {
		__xrt_link_mount_data* pData = (__xrt_link_mount_data*)pBuffer;
		size_t iPathBytes;
		bool bPrint = !bResolve && (pData->PrintLength != 0u);
		USHORT iOffset = bPrint ?
			pData->PrintOffset : pData->SubstituteOffset;
		USHORT iLength = bPrint ?
			pData->PrintLength : pData->SubstituteLength;

		if ( __xrtLinkWindowsPathBytes(iRead, pData->DataLength,
			offsetof(__xrt_link_mount_data, Path), &iPathBytes) ) {
			sResult = __xrtLinkWindowsText(pData->Path, iPathBytes,
				iOffset, iLength, !bPrint);
		}
	} else {
		__xrtLinkError(XERR_TYPE, XLINK_ERROR_FORMAT, "read",
			"the path is not a supported symbolic link");
	}
	xrtFree(pBuffer);
	return sResult;
}



/* 读取 Windows 路径中保存的符号链接或 junction 目标。 */
static str __xrtLinkWindowsRead(cstr sLink)
{
	uint16* pLink = __xrtPathToWide(sLink, NULL);
	HANDLE hLink;
	str sResult;

	if ( pLink == NULL ) {
		return NULL;
	}
	hLink = CreateFileW((const wchar_t*)pLink, 0,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL, OPEN_EXISTING,
		FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if ( hLink == INVALID_HANDLE_VALUE ) {
		int iCode = (int)GetLastError();

		xrtFree(pLink);
		__xrtLinkSetError(XLINK_ERROR_READ, "read",
			"failed to open the symbolic link", iCode);
		return NULL;
	}
	xrtFree(pLink);
	sResult = __xrtLinkWindowsReadHandle(hLink, false);
	(void)CloseHandle(hLink);
	return sResult;
}

#endif



#if !defined(_WIN32) && !defined(_WIN64)

/* 相对目录描述符读取 POSIX 符号链接目标。 */
str __xrtLinkReadAt(int hDirectory, cstr sLink)
{
	struct stat Native;
	size_t iCapacity;

	if ( (sLink == NULL) || (sLink[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( fstatat(hDirectory, sLink, &Native, AT_SYMLINK_NOFOLLOW) != 0 ) {
		int iCode = errno;

		__xrtLinkSetError(XLINK_ERROR_READ, "read",
			"failed to query the symbolic link", iCode);
		return NULL;
	}
	if ( !S_ISLNK(Native.st_mode) ) {
		__xrtLinkError(XERR_TYPE, XLINK_ERROR_READ, "read",
			"the path is not a symbolic link");
		return NULL;
	}
	if ( (Native.st_size > 0) &&
		 ((uintmax_t)Native.st_size > (uintmax_t)(SIZE_MAX - 1u)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iCapacity = (Native.st_size > 0) ? (size_t)Native.st_size : 256u;
	for ( ;; ) {
		str sTarget;
		ssize_t iSize;

		if ( iCapacity == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
		sTarget = (str)xrtMalloc(iCapacity + 1u);
		if ( sTarget == NULL ) {
			return NULL;
		}
		do {
			iSize = readlinkat(hDirectory, sLink, sTarget, iCapacity);
		} while ( (iSize < 0) && (errno == EINTR) );
		if ( iSize < 0 ) {
			int iCode = errno;

			xrtFree(sTarget);
			__xrtLinkSetError(XLINK_ERROR_READ, "read",
				"failed to read the symbolic link", iCode);
			return NULL;
		}
		if ( (size_t)iSize < iCapacity ) {
			sTarget[iSize] = '\0';
			return sTarget;
		}
		xrtFree(sTarget);
		if ( iCapacity > (SIZE_MAX / 2u) ) {
			iCapacity = SIZE_MAX;
		} else {
			iCapacity *= 2u;
		}
	}
}

#endif



/* 创建符号链接。 */
XRT_API bool xrtLinkCreate(cstr sTarget, cstr sLink, bool bDirectory)
{
	if ( !__xrtLinkPaths(sTarget, sLink) ) {
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		return __xrtLinkWindowsCreate(sTarget, sLink, bDirectory);
	#else
		(void)bDirectory;
		if ( symlink(sTarget, sLink) != 0 ) {
			int iCode = errno;

			__xrtLinkSetError(XLINK_ERROR_CREATE, "create",
				"failed to create the symbolic link", iCode);
			return false;
		}
		return true;
	#endif
}



/* 为已存在文件创建硬链接。 */
XRT_API bool xrtLinkHard(cstr sExisting, cstr sLink)
{
	if ( !__xrtLinkPaths(sExisting, sLink) ) {
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		{
			uint16* pExisting = __xrtPathToWide(sExisting, NULL);
			uint16* pLink;

			if ( pExisting == NULL ) {
				return false;
			}
			pLink = __xrtPathToWide(sLink, NULL);
			if ( pLink == NULL ) {
				xrtFree(pExisting);
				return false;
			}
			if ( !CreateHardLinkW((const wchar_t*)pLink,
				(const wchar_t*)pExisting, NULL) ) {
				int iCode = (int)GetLastError();

				xrtFree(pLink);
				xrtFree(pExisting);
				__xrtLinkSetError(XLINK_ERROR_CREATE, "hard-link",
					"failed to create the hard link", iCode);
				return false;
			}
			xrtFree(pLink);
			xrtFree(pExisting);
			return true;
		}
	#else
		if ( link(sExisting, sLink) != 0 ) {
			int iCode = errno;

			__xrtLinkSetError(XLINK_ERROR_CREATE, "hard-link",
				"failed to create the hard link", iCode);
			return false;
		}
		return true;
	#endif
}



/* 读取符号链接中存储的目标文本。 */
XRT_API str xrtLinkRead(cstr sLink)
{
	xfileinfo Info;

	if ( (sLink == NULL) || (sLink[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtPathStat(sLink, false, &Info) ) {
		return NULL;
	}
	if ( Info.Type != XFILE_TYPE_LINK ) {
		__xrtLinkError(XERR_TYPE, XLINK_ERROR_READ, "read",
			"the path is not a symbolic link");
		return NULL;
	}
	#if defined(_WIN32) || defined(_WIN64)
		return __xrtLinkWindowsRead(sLink);
	#else
		return __xrtLinkReadAt(AT_FDCWD, sLink);
	#endif
}



/* 删除符号链接自身。 */
XRT_API bool xrtLinkDelete(cstr sLink)
{
	xfileinfo Info;

	if ( (sLink == NULL) || (sLink[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtPathStat(sLink, false, &Info) ) {
		return false;
	}
	if ( Info.Type != XFILE_TYPE_LINK ) {
		__xrtLinkError(XERR_TYPE, XLINK_ERROR_DELETE, "delete",
			"the path is not a symbolic link");
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		{
			uint16* pLink = __xrtPathToWide(sLink, NULL);
			BOOL bResult;

			if ( pLink == NULL ) {
				return false;
			}
			bResult = ((Info.Attributes & FILE_ATTRIBUTE_DIRECTORY) != 0u) ?
				RemoveDirectoryW((const wchar_t*)pLink) :
				DeleteFileW((const wchar_t*)pLink);
			if ( !bResult ) {
				int iCode = (int)GetLastError();

				xrtFree(pLink);
				__xrtLinkSetError(XLINK_ERROR_DELETE, "delete",
					"failed to delete the symbolic link", iCode);
				return false;
			}
			xrtFree(pLink);
			return true;
		}
	#else
		if ( unlink(sLink) != 0 ) {
			int iCode = errno;

			__xrtLinkSetError(XLINK_ERROR_DELETE, "delete",
				"failed to delete the symbolic link", iCode);
			return false;
		}
		return true;
	#endif
}

#endif
