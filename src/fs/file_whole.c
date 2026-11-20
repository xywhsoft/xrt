#include "../internal/xrt_file_temp.h"

#include <errno.h>



#if defined(XRT_FEATURE_FILE_WHOLE)

/* 流式复制使用固定栈缓冲，不随文件大小增长内存。 */
#define XRT_FILE_COPY_BUFFER (64u * 1024u)

/* 在清理临时资源时保留触发失败的首个错误。 */
static void __xrtFileCleanup(xfile File, cstr sPath, bool bDelete)
{
	xerror* pError = xrtTakeError();

	if ( File != NULL ) {
		(void)xrtClose(File);
	}
	if ( bDelete && (sPath != NULL) ) {
		(void)xrtFileDelete(sPath);
	}
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 检查字节视图是否具有合法的借用内存。 */
static bool __xrtFileData(xbytesview Data)
{
	if ( (Data.Size != 0u) && (Data.Data == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 报告整文件读取超过调用方硬上限。 */
static void __xrtFileReadLimit(void)
{
	__xrtFileError(XERR_RANGE, XFILE_ERROR_LIMIT, "read-all",
		"the file exceeds the configured read limit");
}



/* 在硬上限内增长读取缓冲，并始终为末尾零字节保留空间。 */
static bool __xrtFileReadGrow(bytes* pBuffer, size_t* pCapacity,
	size_t iRequired, size_t iLimit)
{
	size_t iCapacity = *pCapacity;
	bytes pResult;

	if ( iRequired > iLimit ) {
		__xrtFileReadLimit();
		return false;
	}
	if ( iRequired > (SIZE_MAX - 1u) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	if ( iCapacity >= iRequired ) {
		return true;
	}
	if ( iCapacity == 0u ) {
		iCapacity = 1u;
	}
	while ( iCapacity < iRequired ) {
		size_t iNext = (iCapacity > (iLimit / 2u)) ?
			iLimit : (iCapacity * 2u);

		if ( iNext <= iCapacity ) {
			__xrtFileReadLimit();
			return false;
		}
		iCapacity = iNext;
	}
	pResult = (bytes)xrtRealloc(*pBuffer, iCapacity + 1u);
	if ( pResult == NULL ) {
		return false;
	}
	*pBuffer = pResult;
	*pCapacity = iCapacity;
	return true;
}



/* 在硬上限内读取整个文件，并适应读取期间继续增长。 */
XRT_API bytes xrtFileReadAllLimit(cstr sPath,
	size_t iLimit, size_t* pSize)
{
	bytes pBuffer = NULL;
	size_t iCapacity = 0;
	size_t iSize = 0;
	size_t iUseLimit = (iLimit == SIZE_MAX) ? (SIZE_MAX - 1u) : iLimit;
	uint64 iHint;
	xfile File;

	if ( pSize != NULL ) {
		*pSize = 0;
	}
	File = xrtOpen(sPath, XFILE_READ);
	if ( File == NULL ) {
		return NULL;
	}
	if ( !xrtFileSize(File, &iHint) ) {
		__xrtFileCleanup(File, NULL, false);
		return NULL;
	}
	if ( iHint > (uint64)iUseLimit ) {
		__xrtFileReadLimit();
		__xrtFileCleanup(File, NULL, false);
		return NULL;
	}
	iCapacity = (size_t)iHint;
	pBuffer = (bytes)xrtMalloc(iCapacity + 1u);
	if ( pBuffer == NULL ) {
		__xrtFileCleanup(File, NULL, false);
		return NULL;
	}
	for ( ;; ) {
		size_t iDone;

		if ( iSize == iCapacity ) {
			unsigned char iProbe;

			if ( !xrtRead(File, &iProbe, 1u, &iDone) ) {
				xrtFree(pBuffer);
				__xrtFileCleanup(File, NULL, false);
				return NULL;
			}
			if ( iDone == 0u ) {
				break;
			}
			if ( !__xrtFileReadGrow(&pBuffer, &iCapacity,
				iSize + 1u, iUseLimit) ) {
				xrtFree(pBuffer);
				__xrtFileCleanup(File, NULL, false);
				return NULL;
			}
			pBuffer[iSize++] = iProbe;
			continue;
		}
		if ( !xrtRead(File, pBuffer + iSize, iCapacity - iSize, &iDone) ) {
			xrtFree(pBuffer);
			__xrtFileCleanup(File, NULL, false);
			return NULL;
		}
		if ( iDone == 0u ) {
			break;
		}
		iSize += iDone;
	}
	if ( !xrtClose(File) ) {
		xrtFree(pBuffer);
		return NULL;
	}
	pBuffer[iSize] = 0;
	if ( pSize != NULL ) {
		*pSize = iSize;
	}
	return pBuffer;
}



/* 不限制业务大小地读取整个文件。 */
XRT_API bytes xrtFileReadAll(cstr sPath, size_t* pSize)
{
	return xrtFileReadAllLimit(sPath, SIZE_MAX - 1u, pSize);
}



/* 使用指定打开语义完整写入一个字节视图。 */
static bool __xrtFileWriteView(cstr sPath, xbytesview Data, uint32 iFlags)
{
	xfile File;

	if ( !__xrtFileData(Data) ) {
		return false;
	}
	File = xrtOpen(sPath, iFlags);
	if ( File == NULL ) {
		return false;
	}
	if ( !xrtWriteFull(File, Data.Data, Data.Size, NULL) ) {
		__xrtFileCleanup(File, NULL, false);
		return false;
	}
	return xrtClose(File);
}



/* 创建或截断文件并完整写入全部字节。 */
XRT_API bool xrtFileWriteAll(cstr sPath, xbytesview Data)
{
	return __xrtFileWriteView(sPath, Data,
		XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE);
}



/* 使用操作系统追加语义完整写入全部字节。 */
XRT_API bool xrtFileAppend(cstr sPath, xbytesview Data)
{
	return __xrtFileWriteView(sPath, Data,
		XFILE_WRITE | XFILE_CREATE | XFILE_APPEND);
}



#if defined(_WIN32) || defined(_WIN64)

/* 使用 Windows 替换原语保留已有目标的 ACL 与文件属性。 */
static bool __xrtFileReplacePreserve(cstr sTemporary, cstr sTarget)
{
	uint16* pTemporary = __xrtPathToWide(sTemporary, NULL);
	uint16* pTarget;

	if ( pTemporary == NULL ) {
		return false;
	}
	pTarget = __xrtPathToWide(sTarget, NULL);
	if ( pTarget == NULL ) {
		xrtFree(pTemporary);
		return false;
	}
	if ( !ReplaceFileW((const wchar_t*)pTarget,
		(const wchar_t*)pTemporary, NULL, 0u, NULL, NULL) ) {
		int iCode = (int)GetLastError();

		xrtFree(pTarget);
		xrtFree(pTemporary);
		if ( (iCode == ERROR_FILE_NOT_FOUND) ||
			 (iCode == ERROR_PATH_NOT_FOUND) ) {
			return xrtPathRename(sTemporary, sTarget, true);
		}
		__xrtFileSetError(XFILE_ERROR_MOVE, "publish",
			"failed to atomically replace the target file", iCode);
		return false;
	}
	xrtFree(pTarget);
	xrtFree(pTemporary);
	return true;
}

/* FILE_RENAME_INFO layout for SDKs predating the FileRenameInfoEx enum. */
typedef struct __xrt_file_publish_info {
	DWORD Flags;
	HANDLE RootDirectory;
	DWORD FileNameLength;
	WCHAR FileName[1];
} __xrt_file_publish_info;

#if defined(__TINYC__)
BOOL WINAPI SetFileInformationByHandle(HANDLE File, int Class, LPVOID Info, DWORD Size);
#endif

static bool __xrtFileReplaceByRename(cstr sTemporary, cstr sTarget)
{
	enum { RenameInfoEx = 22, ReplaceIfExists = 1, PosixSemantics = 2 };
	size_t iUnits;
	uint16* pTarget = __xrtPathToWide(sTarget, &iUnits);
	if ( pTarget == NULL ) {
		return false;
	}
	const size_t iHeader = offsetof(__xrt_file_publish_info, FileName);
	if ( iUnits > ((UINT32_MAX - iHeader - sizeof(WCHAR)) / sizeof(WCHAR)) ) {
		xrtFree(pTarget);
		__xrtErrorSetSizeOverflow();
		return false;
	}
	size_t iNameBytes = iUnits * sizeof(WCHAR);
	size_t iBytes = iHeader + iNameBytes + sizeof(WCHAR);
	if ( iBytes < sizeof(__xrt_file_publish_info) ) {
		iBytes = sizeof(__xrt_file_publish_info);
	}
	__xrt_file_publish_info* pInfo = (__xrt_file_publish_info*)xrtCalloc(1u, iBytes);
	if ( pInfo == NULL ) {
		xrtFree(pTarget);
		return false;
	}
	pInfo->Flags = ReplaceIfExists | PosixSemantics;
	pInfo->FileNameLength = (DWORD)iNameBytes;
	memcpy(pInfo->FileName, pTarget, iNameBytes);
	xrtFree(pTarget);
	uint16* pTemporary = __xrtPathToWide(sTemporary, NULL);
	if ( pTemporary == NULL ) {
		xrtFree(pInfo);
		return false;
	}
	HANDLE hFile = CreateFileW((const WCHAR*)pTemporary, DELETE,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
		OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
	int iCode = 0;
	bool bRenamed = false;
	if ( hFile != INVALID_HANDLE_VALUE ) {
		#if defined(__TINYC__)
			bRenamed = SetFileInformationByHandle(hFile, RenameInfoEx, pInfo, (DWORD)iBytes) != 0;
		#else
			bRenamed = SetFileInformationByHandle(hFile,
				(FILE_INFO_BY_HANDLE_CLASS)RenameInfoEx, pInfo, (DWORD)iBytes) != 0;
		#endif
		if ( !bRenamed ) {
			iCode = (int)GetLastError();
		}
		(void)CloseHandle(hFile);
	} else {
		iCode = (int)GetLastError();
	}
	xrtFree(pTemporary);
	xrtFree(pInfo);
	if ( !bRenamed ) {
		__xrtFileSetError(XFILE_ERROR_MOVE, "publish",
			"failed to publish the file with a single rename", iCode);
	}
	return bRenamed;
}

#endif



/* 在目标同目录写入临时文件并原子替换目标。 */
static bool __xrtFileWriteAtomic(cstr sPath, xbytesview Data,
	bool bPreserveWindowsMetadata)
{
	xfileinfo TargetInfo;
	bool bTargetExists;
	bool bPublished;
	uint32 iMode = 0666u;
	str sDirectory;
	str sTemporary = NULL;
	xfile File;

	if ( (sPath == NULL) || (sPath[0] == '\0') || !__xrtFileData(Data) ) {
		if ( (sPath == NULL) || (sPath[0] == '\0') ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	if ( !__xrtFilePathInfo(sPath, false,
		&bTargetExists, &TargetInfo) ) {
		return false;
	}
	if ( bTargetExists && (TargetInfo.Type != XFILE_TYPE_FILE) ) {
		__xrtFileError(XERR_TYPE, XFILE_ERROR_MOVE, "publish",
			"the atomic write target is not a regular file");
		return false;
	}
	#if !defined(_WIN32) && !defined(_WIN64)
		if ( bTargetExists &&
			 ((TargetInfo.Available & XFILE_INFO_MODE) != 0u) ) {
			iMode = TargetInfo.Mode & 07777u;
		}
	#endif
	sDirectory = xrtPathParent(sPath);
	if ( sDirectory == NULL ) {
		return false;
	}
	File = __xrtFileTempCreate(sDirectory,
		".xrt-write-", ".tmp", iMode, &sTemporary);
	xrtFree(sDirectory);
	if ( File == NULL ) {
		return false;
	}
	if ( !xrtWriteFull(File, Data.Data, Data.Size, NULL) ||
		 !xrtFlush(File) ) {
		__xrtFileCleanup(File, sTemporary, true);
		xrtFree(sTemporary);
		return false;
	}
	if ( !xrtClose(File) ) {
		__xrtFileCleanup(NULL, sTemporary, true);
		xrtFree(sTemporary);
		return false;
	}
	#if !defined(_WIN32) && !defined(_WIN64)
		if ( bTargetExists &&
			 !xrtPathSetMode(sTemporary, true, iMode) ) {
			__xrtFileCleanup(NULL, sTemporary, true);
			xrtFree(sTemporary);
			return false;
		}
	#endif
	#if defined(_WIN32) || defined(_WIN64)
		if ( !bPreserveWindowsMetadata ) {
			bPublished = __xrtFileReplaceByRename(sTemporary, sPath);
		} else if ( bTargetExists ) {
			bPublished = __xrtFileReplacePreserve(sTemporary, sPath);
		} else
	#else
		(void)bPreserveWindowsMetadata;
	#endif
		{
			bPublished = xrtPathRename(sTemporary, sPath, true);
		}
	if ( !bPublished ) {
		__xrtFileCleanup(NULL, sTemporary, true);
		xrtFree(sTemporary);
		return false;
	}
	xrtFree(sTemporary);
	return true;
}



XRT_API bool xrtFileWriteAtomic(cstr sPath, xbytesview Data)
{
	return __xrtFileWriteAtomic(sPath, Data, true);
}



XRT_API bool xrtFileWriteAtomicRename(cstr sPath, xbytesview Data)
{
	return __xrtFileWriteAtomic(sPath, Data, false);
}




/* 流式复制到目标同目录临时文件；按调用方契约决定是否跟随源末级链接。 */
bool __xrtFileCopy(cstr sSource, cstr sTarget,
	bool bReplace, bool bFollowSource)
{
	unsigned char arrBuffer[XRT_FILE_COPY_BUFFER];
	xfileinfo SourcePathInfo;
	xfileinfo SourceInfo;
	xfileinfo TargetInfo;
	bool bTargetExists;
	str sDirectory;
	str sTemporary = NULL;
	xfile Source;
	xfile Target;

	if ( (sSource == NULL) || (sSource[0] == '\0') ||
		 (sTarget == NULL) || (sTarget[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtPathStat(sSource, bFollowSource, &SourcePathInfo) ) {
		return false;
	}
	if ( SourcePathInfo.Type != XFILE_TYPE_FILE ) {
		__xrtFileError(XERR_TYPE, XFILE_ERROR_COPY, "copy",
			"the copy source path is not a regular file");
		return false;
	}
	Source = xrtOpen(sSource, XFILE_READ |
		(bFollowSource ? 0u : XFILE_NOFOLLOW));
	if ( Source == NULL ) {
		return false;
	}
	if ( !xrtFileStat(Source, &SourceInfo) ||
		 !__xrtFilePathInfo(sTarget, false, &bTargetExists, &TargetInfo) ) {
		__xrtFileCleanup(Source, NULL, false);
		return false;
	}
	if ( SourceInfo.Type != XFILE_TYPE_FILE ) {
		__xrtFileError(XERR_TYPE, XFILE_ERROR_COPY, "copy",
			"the copy source is not a regular file");
		__xrtFileCleanup(Source, NULL, false);
		return false;
	}
	if ( bTargetExists && (TargetInfo.Type != XFILE_TYPE_FILE) ) {
		__xrtFileError(XERR_TYPE, XFILE_ERROR_COPY, "copy",
			"the copy target is not a regular file");
		__xrtFileCleanup(Source, NULL, false);
		return false;
	}
	if ( bTargetExists && __xrtFileInfoSame(&SourceInfo, &TargetInfo) ) {
		__xrtFileError(XERR_ARGUMENT, XFILE_ERROR_COPY, "copy",
			"source and target refer to the same file");
		__xrtFileCleanup(Source, NULL, false);
		return false;
	}
	if ( bTargetExists && !bReplace ) {
		__xrtFileError(XERR_EXISTS, XFILE_ERROR_COPY, "copy",
			"the copy target already exists");
		__xrtFileCleanup(Source, NULL, false);
		return false;
	}
	sDirectory = xrtPathParent(sTarget);
	if ( sDirectory == NULL ) {
		__xrtFileCleanup(Source, NULL, false);
		return false;
	}
	Target = __xrtFileTempCreate(sDirectory,
		".xrt-copy-", ".tmp", 0666u, &sTemporary);
	xrtFree(sDirectory);
	if ( Target == NULL ) {
		__xrtFileCleanup(Source, NULL, false);
		return false;
	}
	for ( ;; ) {
		size_t iRead;

		if ( !xrtRead(Source, arrBuffer, sizeof(arrBuffer), &iRead) ) {
			__xrtFileCleanup(Target, sTemporary, true);
			__xrtFileCleanup(Source, NULL, false);
			xrtFree(sTemporary);
			return false;
		}
		if ( iRead == 0u ) {
			break;
		}
		if ( !xrtWriteFull(Target, arrBuffer, iRead, NULL) ) {
			__xrtFileCleanup(Target, sTemporary, true);
			__xrtFileCleanup(Source, NULL, false);
			xrtFree(sTemporary);
			return false;
		}
	}
	if ( !xrtClose(Source) ) {
		__xrtFileCleanup(Target, sTemporary, true);
		xrtFree(sTemporary);
		return false;
	}
	if ( !xrtClose(Target) ) {
		__xrtFileCleanup(NULL, sTemporary, true);
		xrtFree(sTemporary);
		return false;
	}
	if ( !xrtPathRename(sTemporary, sTarget, bReplace) ) {
		__xrtFileCleanup(NULL, sTemporary, true);
		xrtFree(sTemporary);
		return false;
	}
	xrtFree(sTemporary);
	return true;
}



/* 流式复制普通文件；公开契约不跟随源末级链接。 */
XRT_API bool xrtFileCopy(cstr sSource, cstr sTarget, bool bReplace)
{
	return __xrtFileCopy(sSource, sTarget, bReplace, false);
}



/* 判断改名失败是否明确表示源和目标不在同一卷。 */
bool __xrtFileCrossDevice(void)
{
	const xerror* pError = xrtGetError();

	if ( pError == NULL ) {
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		return xrtErrorSystemCode(pError) == ERROR_NOT_SAME_DEVICE;
	#else
		return xrtErrorSystemCode(pError) == EXDEV;
	#endif
}



/* 优先同卷改名，只在明确跨卷时执行复制后删除。 */
XRT_API bool xrtFileMove(cstr sSource, cstr sTarget, bool bReplace)
{
	xfileinfo SourceInfo;
	xfileinfo TargetInfo;
	bool bTargetExists;

	if ( (sSource == NULL) || (sSource[0] == '\0') ||
		 (sTarget == NULL) || (sTarget[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtPathStat(sSource, false, &SourceInfo) ||
		 !__xrtFilePathInfo(sTarget, false,
			&bTargetExists, &TargetInfo) ) {
		return false;
	}
	if ( SourceInfo.Type != XFILE_TYPE_FILE ) {
		__xrtFileError(XERR_TYPE, XFILE_ERROR_MOVE, "move",
			"the move source path is not a regular file");
		return false;
	}
	if ( bTargetExists && (TargetInfo.Type != XFILE_TYPE_FILE) ) {
		__xrtFileError(XERR_TYPE, XFILE_ERROR_MOVE, "move",
			"the move target path is not a regular file");
		return false;
	}
	if ( bTargetExists && __xrtFileInfoSame(&SourceInfo, &TargetInfo) ) {
		__xrtFileError(XERR_ARGUMENT, XFILE_ERROR_MOVE, "move",
			"source and target refer to the same file");
		return false;
	}
	if ( bTargetExists && !bReplace ) {
		__xrtFileError(XERR_EXISTS, XFILE_ERROR_MOVE, "move",
			"the move target already exists");
		return false;
	}
	if ( xrtPathRename(sSource, sTarget, bReplace) ) {
		return true;
	}
	if ( !__xrtFileCrossDevice() ) {
		return false;
	}
	xrtClearError();
	if ( !xrtFileCopy(sSource, sTarget, bReplace) ) {
		return false;
	}
	if ( !xrtFileDelete(sSource) ) {
		return false;
	}
	return true;
}

#endif
