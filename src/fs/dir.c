#if !defined(_WIN32) && !defined(_WIN64)
	#if !defined(_POSIX_C_SOURCE)
		#define _POSIX_C_SOURCE 200809L
	#endif
	#if !defined(_FILE_OFFSET_BITS)
		#define _FILE_OFFSET_BITS 64
	#endif
#endif

#include "../internal/xrt_dir.h"

#include <errno.h>

#if !defined(_WIN32) && !defined(_WIN64)
	#include <dirent.h>
	#include <sys/stat.h>
	#include <sys/types.h>
	#include <unistd.h>
#endif



#if defined(XRT_FEATURE_DIR)

/* 公共目录对象只保存后端分派、状态、路径和迭代终态。 */
struct xdir_impl {
	const xrt_dir_backend_ops* Ops;
	ptr State;
	str Path;
	uint32 Flags;
	bool Done;
	bool Failed;
};



/* native backend 是平台枚举句柄和借用名称缓冲的唯一承载者。 */
typedef struct xrt_native_dir_state {
	#if defined(_WIN32) || defined(_WIN64)
		HANDLE Handle;
		WIN32_FIND_DATAW Pending;
		bool HasPending;
		bool Empty;
		str Name;
		size_t NameCapacity;
	#else
		DIR* Handle;
	#endif
} xrt_native_dir_state;



static xdirnext __xrtNativeDirNext(ptr pState, cstr sPath,
	uint32 iFlags, xdirentry* pEntry);
static bool __xrtNativeDirClose(ptr pState);



static const xrt_dir_backend_ops __xrtNativeDirOps = {
	(uint32)sizeof(xrt_dir_backend_ops),
	XRT_DIR_BACKEND_VERSION,
	false,
	{ 0u, 0u, 0u },
	__xrtNativeDirNext,
	__xrtNativeDirClose
};



/* 设置带系统代码的目录错误。 */
void __xrtDirSetError(xdirerror Code, cstr sOperation,
	cstr sMessage, int iSystemCode)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = __xrtSystemErrorKind(iSystemCode);
	Desc.Domain = "xrt.dir";
	Desc.Code = (int32)Code;
	Desc.SystemCode = (int32)iSystemCode;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 设置不带系统代码的目录错误。 */
void __xrtDirError(xerrkind Kind, xdirerror Code,
	cstr sOperation, cstr sMessage)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.dir";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 判断名称是否为目录枚举器返回的点条目。 */
static bool __xrtDirDot(cstr sName)
{
	return (sName[0] == '.') &&
		((sName[1] == '\0') || ((sName[1] == '.') && (sName[2] == '\0')));
}



/* 检查打开标志并要求跟随链接时同时请求完整元数据。 */
bool __xrtDirFlagsValid(uint32 iFlags)
{
	const uint32 iKnown = XDIR_STAT | XDIR_FOLLOW_LINKS | XDIR_INCLUDE_DOTS;

	if ( ((iFlags & ~iKnown) != 0u) ||
		 (((iFlags & XDIR_FOLLOW_LINKS) != 0u) &&
		  ((iFlags & XDIR_STAT) == 0u)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 接管后端状态并创建统一目录对象；分配失败仍消费 State。 */
xdir __xrtDirTakeBackend(const xrt_dir_backend_ops* pOps,
	ptr pState, cstr sPath, uint32 iFlags)
{
	xdir Dir;

	if ( (pOps == NULL) || (pOps->Size != sizeof(*pOps)) ||
		 (pOps->Version != XRT_DIR_BACKEND_VERSION) ||
		 (pOps->Next == NULL) || (pOps->Close == NULL) ||
		 (pState == NULL) || (sPath == NULL) || (sPath[0] == '\0') ) {
		if ( (pOps != NULL) && (pOps->Size == sizeof(*pOps)) &&
			 (pOps->Close != NULL) && (pState != NULL) ) {
			(void)pOps->Close(pState);
		}
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	Dir = (xdir)xrtCalloc(1u, sizeof(*Dir));
	if ( Dir == NULL ) {
		xerror* pError = xrtTakeError();

		(void)pOps->Close(pState);
		xrtClearError();
		__xrtErrorSetOwned(pError);
		return NULL;
	}
	Dir->Path = xrtStrDup(sPath);
	if ( Dir->Path == NULL ) {
		xerror* pError = xrtTakeError();

		(void)pOps->Close(pState);
		xrtFree(Dir);
		xrtClearError();
		__xrtErrorSetOwned(pError);
		return NULL;
	}
	Dir->Ops = pOps;
	Dir->State = pState;
	Dir->Flags = iFlags;
	return Dir;
}



#if defined(_WIN32) || defined(_WIN64)

/* FindFirstFileW 的目录部分必须是字面路径，只允许扩展前缀自带的问号。 */
static bool __xrtDirWindowsLiteralPath(cstr sPath)
{
	size_t iPosition = ((sPath[0] == '\\') && (sPath[1] == '\\') &&
		(sPath[2] == '?') && (sPath[3] == '\\')) ? 4u : 0u;

	for ( ; sPath[iPosition] != '\0'; iPosition++ ) {
		if ( (sPath[iPosition] == '*') || (sPath[iPosition] == '?') ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
	}
	return true;
}



/* 打开 Windows 目录枚举句柄，并保留首条结果。 */
static bool __xrtNativeDirOpen(xrt_native_dir_state* pNative, cstr sPath)
{
	str sPattern = xrtPathJoin(sPath, "*");
	uint16* pPattern;

	if ( sPattern == NULL ) {
		return false;
	}
	pPattern = __xrtPathToWide(sPattern, NULL);
	xrtFree(sPattern);
	if ( pPattern == NULL ) {
		return false;
	}
	pNative->Handle = FindFirstFileW(
		(const wchar_t*)pPattern,
		&pNative->Pending
	);
	xrtFree(pPattern);
	if ( pNative->Handle == INVALID_HANDLE_VALUE ) {
		int iCode = (int)GetLastError();

		if ( iCode == ERROR_FILE_NOT_FOUND ) {
			xfileinfo Info;

			if ( xrtPathStat(sPath, true, &Info) &&
				 (Info.Type == XFILE_TYPE_DIRECTORY) ) {
				pNative->Empty = true;
				return true;
			}
			if ( xrtGetError() != NULL ) {
				return false;
			}
		}
		__xrtDirSetError(XDIR_ERROR_OPEN, "open",
			"failed to open the directory iterator", iCode);
		return false;
	}
	pNative->HasPending = true;
	return true;
}



/* 取得下一份 Windows 枚举数据。 */
static xdirnext __xrtNativeDirWindowsData(
	xrt_native_dir_state* pNative,
	WIN32_FIND_DATAW* pData
)
{
	if ( pNative->Empty ) {
		return XDIR_NEXT_END;
	}
	if ( pNative->HasPending ) {
		*pData = pNative->Pending;
		pNative->HasPending = false;
		return XDIR_NEXT_ITEM;
	}
	if ( FindNextFileW(pNative->Handle, pData) ) {
		return XDIR_NEXT_ITEM;
	}
	{
		int iCode = (int)GetLastError();

		if ( iCode == ERROR_NO_MORE_FILES ) {
			return XDIR_NEXT_END;
		}
		__xrtDirSetError(XDIR_ERROR_NEXT, "next",
			"failed while reading the directory", iCode);
		return XDIR_NEXT_ERROR;
	}
}



/* 严格转换并复用 Windows 条目名称缓冲，避免逐条分配。 */
static bool __xrtNativeDirWindowsName(xrt_native_dir_state* pNative,
	const wchar_t* sName, size_t iWideSize, size_t* pNameSize)
{
	xutf16view Source = { (const uint16*)sName, iWideSize };
	xutfresult Measure;
	xutfresult Result;
	size_t iNeed;

	Measure = xrtUtf16To8Buffer(Source, NULL, 0u, XUTF_STRICT);
	if ( Measure.Status != XUTF_OK ) {
		return false;
	}
	if ( Measure.Written == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iNeed = Measure.Written + 1u;
	if ( pNative->NameCapacity < iNeed ) {
		str sBuffer = (str)xrtRealloc(pNative->Name, iNeed);

		if ( sBuffer == NULL ) {
			return false;
		}
		pNative->Name = sBuffer;
		pNative->NameCapacity = iNeed;
	}
	Result = xrtUtf16To8Buffer(Source, pNative->Name,
		pNative->NameCapacity - 1u, XUTF_STRICT);
	if ( (Result.Status != XUTF_OK) || (Result.Read != iWideSize) ||
		 (Result.Written != Measure.Written) ) {
		if ( Result.Status == XUTF_NO_SPACE ) {
			__xrtDirError(XERR_STATE, XDIR_ERROR_ENTRY, "entry-name",
				"the measured directory entry name did not fit");
		}
		return false;
	}
	pNative->Name[Result.Written] = '\0';
	*pNameSize = Result.Written;
	return true;
}



/* 从 Windows 枚举数据构造借用目录条目。 */
static xdirnext __xrtNativeDirNext(ptr pState, cstr sPath,
	uint32 iFlags, xdirentry* pEntry)
{
	xrt_native_dir_state* pNative = (xrt_native_dir_state*)pState;

	for ( ;; ) {
		WIN32_FIND_DATAW Data;
		size_t iWideSize;
		size_t iNameSize;
		xdirentry Entry;
		xdirnext Next = __xrtNativeDirWindowsData(pNative, &Data);

		if ( Next != XDIR_NEXT_ITEM ) {
			return Next;
		}
		iWideSize = wcslen(Data.cFileName);
		if ( !__xrtNativeDirWindowsName(pNative,
			Data.cFileName, iWideSize, &iNameSize) ) {
			return XDIR_NEXT_ERROR;
		}
		if ( ((iFlags & XDIR_INCLUDE_DOTS) == 0u) &&
			 __xrtDirDot(pNative->Name) ) {
			continue;
		}
		memset(&Entry, 0, sizeof(Entry));
		Entry.Name.Data = pNative->Name;
		Entry.Name.Size = iNameSize;
		Entry.Flags = XDIR_ENTRY_UTF8;
		__xrtFileWindowsFindInfo(&Data, &Entry.Info);
		if ( (iFlags & XDIR_STAT) != 0u ) {
			str sEntryPath = xrtPathJoin(sPath, pNative->Name);

			if ( sEntryPath == NULL ) {
				return XDIR_NEXT_ERROR;
			}
			if ( !xrtPathStat(sEntryPath,
				(iFlags & XDIR_FOLLOW_LINKS) != 0u, &Entry.Info) ) {
				xrtFree(sEntryPath);
				return XDIR_NEXT_ERROR;
			}
			xrtFree(sEntryPath);
		}
		*pEntry = Entry;
		return XDIR_NEXT_ITEM;
	}
}

#else

/* 把 POSIX dirent 类型映射为无需额外 stat 的对象类别。 */
#if defined(DT_REG)
static xfiletype __xrtDirPosixType(unsigned char iType)
{
	if ( iType == DT_REG ) return XFILE_TYPE_FILE;
	if ( iType == DT_DIR ) return XFILE_TYPE_DIRECTORY;
	if ( iType == DT_LNK ) return XFILE_TYPE_LINK;
	if ( iType == DT_FIFO ) return XFILE_TYPE_FIFO;
	if ( iType == DT_SOCK ) return XFILE_TYPE_SOCKET;
	if ( (iType == DT_CHR) || (iType == DT_BLK) ) return XFILE_TYPE_DEVICE;
	return XFILE_TYPE_NONE;
}
#endif



/* 打开 POSIX 目录枚举句柄。 */
static bool __xrtNativeDirOpen(xrt_native_dir_state* pNative, cstr sPath)
{
	do {
		pNative->Handle = opendir(sPath);
	} while ( (pNative->Handle == NULL) && (errno == EINTR) );
	if ( pNative->Handle == NULL ) {
		int iCode = errno;

		__xrtDirSetError(XDIR_ERROR_OPEN, "open",
			"failed to open the directory iterator", iCode);
		return false;
	}
	return true;
}



/* 从 POSIX dirent 构造借用目录条目。 */
static xdirnext __xrtNativeDirNext(ptr pState, cstr sPath,
	uint32 iFlags, xdirentry* pEntry)
{
	xrt_native_dir_state* pNative = (xrt_native_dir_state*)pState;

	for ( ;; ) {
		struct dirent* pData;
		xdirentry Entry;
		size_t iNameSize;

		do {
			errno = 0;
			pData = readdir(pNative->Handle);
		} while ( (pData == NULL) && (errno == EINTR) );
		if ( pData == NULL ) {
			if ( errno == 0 ) return XDIR_NEXT_END;
			__xrtDirSetError(XDIR_ERROR_NEXT, "next",
				"failed while reading the directory", errno);
			return XDIR_NEXT_ERROR;
		}
		if ( ((iFlags & XDIR_INCLUDE_DOTS) == 0u) &&
			 __xrtDirDot(pData->d_name) ) {
			continue;
		}
		iNameSize = strlen(pData->d_name);
		memset(&Entry, 0, sizeof(Entry));
		Entry.Name.Data = pData->d_name;
		Entry.Name.Size = iNameSize;
		#if defined(DT_REG)
			Entry.Info.Type = __xrtDirPosixType(pData->d_type);
		#else
			Entry.Info.Type = XFILE_TYPE_NONE;
		#endif
		if ( xrtUtf8Valid(Entry.Name, NULL) ) {
			Entry.Flags |= XDIR_ENTRY_UTF8;
		}
		if ( (iFlags & XDIR_STAT) != 0u ) {
			str sEntryPath = xrtPathJoin(sPath, pData->d_name);

			if ( sEntryPath == NULL ) {
				return XDIR_NEXT_ERROR;
			}
			if ( !xrtPathStat(sEntryPath,
				(iFlags & XDIR_FOLLOW_LINKS) != 0u, &Entry.Info) ) {
				xrtFree(sEntryPath);
				return XDIR_NEXT_ERROR;
			}
			xrtFree(sEntryPath);
		}
		*pEntry = Entry;
		return XDIR_NEXT_ITEM;
	}
}

#endif



/* 关闭 native 枚举状态。 */
static bool __xrtNativeDirClose(ptr pState)
{
	xrt_native_dir_state* pNative = (xrt_native_dir_state*)pState;
	bool bResult = true;
	int iCode = 0;

	#if defined(_WIN32) || defined(_WIN64)
		if ( (pNative->Handle != NULL) &&
			 (pNative->Handle != INVALID_HANDLE_VALUE) &&
			 !FindClose(pNative->Handle) ) {
			bResult = false;
			iCode = (int)GetLastError();
		}
		xrtFree(pNative->Name);
	#else
		if ( (pNative->Handle != NULL) && (closedir(pNative->Handle) != 0) ) {
			bResult = false;
			iCode = errno;
		}
	#endif
	xrtFree(pNative);
	if ( !bResult ) {
		__xrtDirSetError(XDIR_ERROR_CLOSE, "close",
			"failed to close the directory iterator", iCode);
	}
	return bResult;
}



/* 打开原生目录迭代器。 */
XRT_API xdir xrtDirOpen(cstr sPath, uint32 iFlags)
{
	xfileinfo Info;
	xrt_native_dir_state* pNative;

	if ( (sPath == NULL) || (sPath[0] == '\0') ||
		 !__xrtDirFlagsValid(iFlags) ) {
		if ( (sPath == NULL) || (sPath[0] == '\0') ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( !__xrtDirWindowsLiteralPath(sPath) ) {
			return NULL;
		}
	#endif
	if ( !xrtPathStat(sPath, true, &Info) ) {
		return NULL;
	}
	if ( Info.Type != XFILE_TYPE_DIRECTORY ) {
		__xrtDirError(XERR_TYPE, XDIR_ERROR_OPEN, "open",
			"the path is not a directory");
		return NULL;
	}
	pNative = (xrt_native_dir_state*)xrtCalloc(1u, sizeof(*pNative));
	if ( pNative == NULL ) {
		return NULL;
	}
	#if defined(_WIN32) || defined(_WIN64)
		pNative->Handle = INVALID_HANDLE_VALUE;
	#endif
	if ( !__xrtNativeDirOpen(pNative, sPath) ) {
		xrtFree(pNative);
		return NULL;
	}
	return __xrtDirTakeBackend(&__xrtNativeDirOps,
		pNative, sPath, iFlags);
}



/* 读取下一条目录项。 */
XRT_API xdirnext xrtDirNext(xdir Dir, xdirentry* pEntry)
{
	xdirnext Next;
	xdirentry Entry;

	if ( (Dir == NULL) || (pEntry == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return XDIR_NEXT_ERROR;
	}
	if ( Dir->Failed ) {
		__xrtErrorSetInvalidState();
		return XDIR_NEXT_ERROR;
	}
	if ( Dir->Done ) {
		return XDIR_NEXT_END;
	}
	Next = Dir->Ops->Next(Dir->State, Dir->Path, Dir->Flags, &Entry);
	if ( Next == XDIR_NEXT_ITEM ) {
		*pEntry = Entry;
	} else if ( Next == XDIR_NEXT_END ) {
		Dir->Done = true;
	} else {
		Dir->Failed = true;
		Next = XDIR_NEXT_ERROR;
	}
	return Next;
}



/* 关闭并销毁目录迭代器。 */
XRT_API bool xrtDirClose(xdir Dir)
{
	bool bResult;

	if ( Dir == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	bResult = Dir->Ops->Close(Dir->State);
	xrtFree(Dir->Path);
	xrtFree(Dir);
	return bResult;
}



/* 返回迭代器借用的目录路径。 */
XRT_API cstr xrtDirPath(xdir Dir)
{
	if ( Dir == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return Dir->Path;
}



/* 把迭代器目录与条目名称拼成拥有路径。 */
XRT_API str xrtDirEntryPath(xdir Dir, const xdirentry* pEntry)
{
	str sResult;
	size_t iPath;
	size_t iSize;
	bool bRoot;

	if ( (Dir == NULL) || (pEntry == NULL) ||
		 (pEntry->Name.Data == NULL) ||
		 (strlen(pEntry->Name.Data) != pEntry->Name.Size) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !Dir->Ops->VirtualPath ) {
		return xrtPathJoin(Dir->Path, pEntry->Name.Data);
	}
	iPath = strlen(Dir->Path);
	bRoot = (iPath == 1u) && (Dir->Path[0] == '/');
	if ( iPath > (SIZE_MAX - pEntry->Name.Size - (bRoot ? 1u : 2u)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iSize = iPath + pEntry->Name.Size + (bRoot ? 0u : 1u);
	sResult = (str)xrtMalloc(iSize + 1u);
	if ( sResult == NULL ) {
		return NULL;
	}
	memcpy(sResult, Dir->Path, iPath);
	if ( !bRoot ) sResult[iPath++] = '/';
	memcpy(sResult + iPath, pEntry->Name.Data, pEntry->Name.Size);
	sResult[iSize] = '\0';
	return sResult;
}



/* 使用平台原生接口创建一个目录。 */
static bool __xrtDirCreateOne(cstr sPath, uint32 iMode, bool bExistingOk)
{
	#if defined(_WIN32) || defined(_WIN64)
		uint16* pPath = __xrtPathToWide(sPath, NULL);

		(void)iMode;
		if ( pPath == NULL ) {
			return false;
		}
		if ( CreateDirectoryW((const wchar_t*)pPath, NULL) ) {
			xrtFree(pPath);
			return true;
		}
		{
			int iCode = (int)GetLastError();

			xrtFree(pPath);
			if ( bExistingOk && (iCode == ERROR_ALREADY_EXISTS) &&
				xrtDirExists(sPath) ) {
				return true;
			}
			__xrtDirSetError(XDIR_ERROR_CREATE, "create",
				"failed to create the directory", iCode);
			return false;
		}
	#else
		int iResult;

		do {
			iResult = mkdir(sPath, (mode_t)iMode);
		} while ( (iResult != 0) && (errno == EINTR) );
		if ( iResult == 0 ) {
			return true;
		}
		{
			int iCode = errno;

			if ( bExistingOk && (iCode == EEXIST) && xrtDirExists(sPath) ) {
				return true;
			}
			__xrtDirSetError(XDIR_ERROR_CREATE, "create",
				"failed to create the directory", iCode);
			return false;
		}
	#endif
}



/* 使用显式 POSIX 模式创建一个目录。 */
XRT_API bool xrtDirCreateMode(cstr sPath, uint32 iMode)
{
	if ( (sPath == NULL) || (sPath[0] == '\0') || ((iMode & ~07777u) != 0u) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtDirCreateOne(sPath, iMode, false);
}



/* 使用平台默认模式创建一个目录。 */
XRT_API bool xrtDirCreate(cstr sPath)
{
	return xrtDirCreateMode(sPath, 0777u);
}



/* 使用显式模式逐级创建全部缺失目录。 */
XRT_API bool xrtDirCreateAllMode(cstr sPath, uint32 iMode)
{
	xpathparts Parts;
	str sCurrent;
	size_t iSize;
	size_t iRootSize;
	size_t i;

	if ( (sPath == NULL) || (sPath[0] == '\0') || ((iMode & ~07777u) != 0u) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtPathParse(xrtStrView(sPath), XPATH_NATIVE, &Parts) ) {
		return false;
	}
	sCurrent = xrtStrDup(sPath);
	if ( sCurrent == NULL ) {
		return false;
	}
	iSize = strlen(sCurrent);
	iRootSize = Parts.Root.Size;
	for ( i = iRootSize; i < iSize; i++ ) {
		#if defined(_WIN32) || defined(_WIN64)
			bool bSeparator = (sCurrent[i] == '/') || (sCurrent[i] == '\\');
		#else
			bool bSeparator = sCurrent[i] == '/';
		#endif

		if ( bSeparator && (i > iRootSize) ) {
			char iSaved = sCurrent[i];

			sCurrent[i] = '\0';
			if ( !__xrtDirCreateOne(sCurrent, iMode, true) ) {
				xrtFree(sCurrent);
				return false;
			}
			sCurrent[i] = iSaved;
		}
	}
	while ( (iSize > iRootSize) &&
		 ((sCurrent[iSize - 1u] == '/')
		#if defined(_WIN32) || defined(_WIN64)
		  || (sCurrent[iSize - 1u] == '\\')
		#endif
		 ) ) {
		sCurrent[--iSize] = '\0';
	}
	if ( iSize == iRootSize ) {
		bool bResult = xrtDirExists(sCurrent);

		xrtFree(sCurrent);
		if ( !bResult ) {
			__xrtDirError(XERR_NOT_FOUND, XDIR_ERROR_CREATE, "create-all",
				"the directory root does not exist");
		}
		return bResult;
	}
	if ( !__xrtDirCreateOne(sCurrent, iMode, true) ) {
		xrtFree(sCurrent);
		return false;
	}
	xrtFree(sCurrent);
	return true;
}



/* 使用平台默认模式逐级创建全部缺失目录。 */
XRT_API bool xrtDirCreateAll(cstr sPath)
{
	return xrtDirCreateAllMode(sPath, 0777u);
}



/* 删除一个空目录。 */
XRT_API bool xrtDirRemove(cstr sPath)
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
			if ( !RemoveDirectoryW((const wchar_t*)pPath) ) {
				int iCode = (int)GetLastError();

				xrtFree(pPath);
				__xrtDirSetError(XDIR_ERROR_REMOVE, "remove",
					"failed to remove the directory", iCode);
				return false;
			}
			xrtFree(pPath);
		}
	#else
		int iResult;

		do {
			iResult = rmdir(sPath);
		} while ( (iResult != 0) && (errno == EINTR) );
		if ( iResult != 0 ) {
			int iCode = errno;

			__xrtDirSetError(XDIR_ERROR_REMOVE, "remove",
				"failed to remove the directory", iCode);
			return false;
		}
	#endif
	return true;
}



/* 查询目录是否为空。 */
XRT_API bool xrtDirEmpty(cstr sPath, bool* pEmpty)
{
	xdir Dir;
	xdirentry Entry;
	xdirnext Next;
	bool bClose;

	if ( pEmpty == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Dir = xrtDirOpen(sPath, 0u);
	if ( Dir == NULL ) {
		return false;
	}
	Next = xrtDirNext(Dir, &Entry);
	if ( Next == XDIR_NEXT_ERROR ) {
		xerror* pError = xrtTakeError();

		(void)xrtDirClose(Dir);
		if ( pError != NULL ) {
			__xrtErrorSetOwned(pError);
		}
		return false;
	}
	bClose = xrtDirClose(Dir);
	if ( !bClose ) {
		return false;
	}
	*pEmpty = Next == XDIR_NEXT_END;
	return true;
}



/* 释放系统根目录列表并清零。 */
XRT_API void xrtDirRootsFree(xdirroots* pRoots)
{
	size_t i;

	if ( pRoots == NULL ) {
		return;
	}
	for ( i = 0; i < pRoots->Count; i++ ) {
		xrtFree(pRoots->Items[i]);
	}
	xrtFree(pRoots->Items);
	pRoots->Items = NULL;
	pRoots->Count = 0;
}



/* 查询当前系统可枚举的文件系统根目录。 */
XRT_API bool xrtDirRoots(xdirroots* pRoots)
{
	xdirroots Roots;

	if ( pRoots == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Roots, 0, sizeof(Roots));
	#if defined(_WIN32) || defined(_WIN64)
		{
			DWORD iCapacity = GetLogicalDriveStringsW(0, NULL);
			DWORD iWritten = 0u;
			wchar_t* pWide = NULL;
			wchar_t* pItem;
			size_t iCount = 0;
			uint32 iAttempt;

			if ( iCapacity == 0u ) {
				int iCode = (int)GetLastError();

				__xrtDirSetError(XDIR_ERROR_ROOTS, "roots",
					"failed to measure system roots", iCode);
				return false;
			}

			/* 驱动器列表可能在测量和读取之间变化，按新容量重试。 */
			for ( iAttempt = 0u; iAttempt < 8u; iAttempt++ ) {
				wchar_t* pBuffer;

				#if !defined(_WIN64)
					if ( (size_t)iCapacity >
						 (SIZE_MAX / sizeof(wchar_t)) ) {
						xrtFree(pWide);
						__xrtErrorSetSizeOverflow();
						return false;
					}
				#endif
				pBuffer = (wchar_t*)xrtRealloc(pWide,
					(size_t)iCapacity * sizeof(wchar_t));
				if ( pBuffer == NULL ) {
					xrtFree(pWide);
					return false;
				}
				pWide = pBuffer;
				iWritten = GetLogicalDriveStringsW(iCapacity, pWide);
				if ( iWritten == 0u ) {
					int iCode = (int)GetLastError();

					xrtFree(pWide);
					__xrtDirSetError(XDIR_ERROR_ROOTS, "roots",
						"failed to read system roots", iCode);
					return false;
				}
				if ( iWritten < iCapacity ) {
					break;
				}
				iCapacity = iWritten;
			}
			if ( iWritten >= iCapacity ) {
				xrtFree(pWide);
				__xrtDirSetError(XDIR_ERROR_ROOTS, "roots",
					"system roots changed too frequently", ERROR_MORE_DATA);
				return false;
			}
			for ( pItem = pWide; *pItem != L'\0'; pItem += wcslen(pItem) + 1u ) {
				iCount++;
			}
			Roots.Items = (str*)xrtCalloc(iCount, sizeof(str));
			if ( Roots.Items == NULL ) {
				xrtFree(pWide);
				return false;
			}
			for ( pItem = pWide; *pItem != L'\0'; pItem += wcslen(pItem) + 1u ) {
				Roots.Items[Roots.Count] = __xrtPathFromWide(pItem, wcslen(pItem));
				if ( Roots.Items[Roots.Count] == NULL ) {
					xrtFree(pWide);
					xrtDirRootsFree(&Roots);
					return false;
				}
				Roots.Count++;
			}
			xrtFree(pWide);
		}
	#else
		Roots.Items = (str*)xrtCalloc(1u, sizeof(str));
		if ( Roots.Items == NULL ) {
			return false;
		}
		Roots.Items[0] = xrtStrDup("/");
		if ( Roots.Items[0] == NULL ) {
			xrtFree(Roots.Items);
			return false;
		}
		Roots.Count = 1;
	#endif
	*pRoots = Roots;
	return true;
}

#endif
