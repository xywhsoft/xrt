#if !defined(_WIN32) && !defined(_WIN64)
	#if !defined(_POSIX_C_SOURCE)
		#define _POSIX_C_SOURCE 200809L
	#endif
	#if !defined(_FILE_OFFSET_BITS)
		#define _FILE_OFFSET_BITS 64
	#endif
#endif

#include "../internal/xrt_file_root.h"

#include <errno.h>
#include <stddef.h>

#if !defined(_WIN32) && !defined(_WIN64)
	#include <dirent.h>
	#include <sys/stat.h>
	#include <sys/types.h>
	#include <unistd.h>
#endif



#if defined(XRT_FEATURE_FILE_ROOT)

#if defined(_WIN32) || defined(_WIN64)
	typedef LONG xrt_root_dir_status;
	typedef struct xrt_root_dir_io_status xrt_root_dir_io_status;
	typedef xrt_root_dir_status (WINAPI *xrt_root_dir_query_proc)(
		HANDLE hFile, HANDLE hEvent, PVOID pApcRoutine, PVOID pApcContext,
		xrt_root_dir_io_status* pStatus, PVOID pInformation, ULONG iSize,
		int iClass, BYTE bSingleEntry, PVOID pName, BYTE bRestart);
#endif



/* 迭代器始终拥有锚定目录句柄；条目名称借用到下一次迭代。 */
typedef struct xrt_root_dir_state {
	xrootnative Directory;
	#if defined(_WIN32) || defined(_WIN64)
		xrt_root_dir_query_proc Query;
		bytes Buffer;
		size_t BufferSize;
		str Name;
		size_t NameCapacity;
		bool Restart;
	#else
		DIR* Stream;
	#endif
} xrt_root_dir_state;



/* 点条目不能通过 root API 表示父目录，root iterator 始终隐藏它们。 */
static bool __xrtRootDirDot(cstr sName)
{
	return (sName[0] == '.') &&
		((sName[1] == '\0') ||
		 ((sName[1] == '.') && (sName[2] == '\0')));
}



/* 使用当前枚举目录作为临时根，安全取得条目完整元数据。 */
static bool __xrtRootDirStat(xrt_root_dir_state* pState,
	cstr sDisplayPath, cstr sName, uint32 iFlags, xfileinfo* pInfo)
{
	struct xroot_impl Anchor;

	memset(&Anchor, 0, sizeof(Anchor));
	Anchor.Path = (str)sDisplayPath;
	Anchor.Handle = pState->Directory;
	return xrtRootStat(&Anchor, sName,
		(iFlags & XDIR_FOLLOW_LINKS) != 0u, pInfo);
}



#if defined(_WIN32) || defined(_WIN64)

#define XRT_ROOT_DIR_STATUS_NO_MORE_FILES ((xrt_root_dir_status)0x80000006L)
#define XRT_ROOT_DIR_STATUS_BUFFER_OVERFLOW ((xrt_root_dir_status)0x80000005L)
#define XRT_ROOT_DIR_INFORMATION_CLASS 1
#define XRT_ROOT_DIR_BUFFER_SIZE (64u * 1024u)

struct xrt_root_dir_io_status {
	union {
		xrt_root_dir_status Status;
		PVOID Pointer;
	};
	ULONG_PTR Information;
};

typedef struct xrt_root_dir_information {
	ULONG NextEntryOffset;
	ULONG FileIndex;
	LARGE_INTEGER CreationTime;
	LARGE_INTEGER LastAccessTime;
	LARGE_INTEGER LastWriteTime;
	LARGE_INTEGER ChangeTime;
	LARGE_INTEGER EndOfFile;
	LARGE_INTEGER AllocationSize;
	ULONG FileAttributes;
	ULONG FileNameLength;
	WCHAR FileName[1];
} xrt_root_dir_information;

typedef ULONG (WINAPI *xrt_root_dir_error_proc)(xrt_root_dir_status Status);



/* 把 NTSTATUS 转换为目录错误使用的 Win32 代码。 */
static int __xrtRootDirWindowsError(xrt_root_dir_status Status)
{
	HMODULE hModule = GetModuleHandleW(L"ntdll.dll");
	xrt_root_dir_error_proc pConvert = hModule != NULL ?
		(xrt_root_dir_error_proc)(uintptr_t)
		GetProcAddress(hModule, "RtlNtStatusToDosError") : NULL;

	return pConvert != NULL ? (int)pConvert(Status) : ERROR_GEN_FAILURE;
}



/* 严格转换单条 UTF-16 名称并复用状态缓冲。 */
static bool __xrtRootDirWindowsName(xrt_root_dir_state* pState,
	const WCHAR* sName, size_t iUnits, size_t* pSize)
{
	xutf16view Source = { (const uint16*)sName, iUnits };
	xutfresult Measure = xrtUtf16To8Buffer(Source, NULL, 0u, XUTF_STRICT);
	xutfresult Result;
	size_t iNeed;

	if ( Measure.Status != XUTF_OK ) {
		__xrtDirError(XERR_VALUE, XDIR_ERROR_ENTRY,
			"root-entry-name", "a Windows directory name is not valid UTF-16");
		return false;
	}
	if ( Measure.Written == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iNeed = Measure.Written + 1u;
	if ( pState->NameCapacity < iNeed ) {
		str sBuffer = (str)xrtRealloc(pState->Name, iNeed);

		if ( sBuffer == NULL ) return false;
		pState->Name = sBuffer;
		pState->NameCapacity = iNeed;
	}
	Result = xrtUtf16To8Buffer(Source, pState->Name,
		pState->NameCapacity - 1u, XUTF_STRICT);
	if ( (Result.Status != XUTF_OK) || (Result.Read != iUnits) ||
		 (Result.Written != Measure.Written) ) {
		__xrtDirError(XERR_VALUE, XDIR_ERROR_ENTRY,
			"root-entry-name", "a Windows directory name changed during conversion");
		return false;
	}
	pState->Name[Result.Written] = '\0';
	*pSize = Result.Written;
	return true;
}



/* 把 NT 目录信息映射到公共的基本元数据。 */
static void __xrtRootDirWindowsInfo(
	const xrt_root_dir_information* pNative, xfileinfo* pInfo)
{
	WIN32_FIND_DATAW Data;

	memset(&Data, 0, sizeof(Data));
	Data.dwFileAttributes = pNative->FileAttributes;
	Data.ftCreationTime.dwLowDateTime = pNative->CreationTime.LowPart;
	Data.ftCreationTime.dwHighDateTime = (DWORD)pNative->CreationTime.HighPart;
	Data.ftLastAccessTime.dwLowDateTime = pNative->LastAccessTime.LowPart;
	Data.ftLastAccessTime.dwHighDateTime =
		(DWORD)pNative->LastAccessTime.HighPart;
	Data.ftLastWriteTime.dwLowDateTime = pNative->LastWriteTime.LowPart;
	Data.ftLastWriteTime.dwHighDateTime =
		(DWORD)pNative->LastWriteTime.HighPart;
	Data.nFileSizeLow = pNative->EndOfFile.LowPart;
	Data.nFileSizeHigh = (DWORD)pNative->EndOfFile.HighPart;
	__xrtFileWindowsFindInfo(&Data, pInfo);
}



/* NtQueryDirectoryFile 的 single-entry 模式避免借用易失的路径字符串。 */
static xdirnext __xrtRootDirNext(ptr pData, cstr sDisplayPath,
	uint32 iFlags, xdirentry* pEntry)
{
	xrt_root_dir_state* pState = (xrt_root_dir_state*)pData;

	for ( ;; ) {
		xrt_root_dir_io_status Io;
		xrt_root_dir_information* pNative;
		xrt_root_dir_status Status;
		size_t iNameBytes;
		size_t iNameSize;
		xdirentry Entry;

		memset(&Io, 0, sizeof(Io));
		Status = pState->Query(pState->Directory, NULL, NULL, NULL, &Io,
			pState->Buffer, (ULONG)pState->BufferSize,
			XRT_ROOT_DIR_INFORMATION_CLASS, TRUE, NULL,
			pState->Restart ? TRUE : FALSE);
		pState->Restart = false;
		if ( Status == XRT_ROOT_DIR_STATUS_NO_MORE_FILES )
			return XDIR_NEXT_END;
		if ( Status == XRT_ROOT_DIR_STATUS_BUFFER_OVERFLOW ) {
			__xrtDirError(XERR_RANGE, XDIR_ERROR_ENTRY,
				"root-next", "a directory entry exceeds the native buffer limit");
			return XDIR_NEXT_ERROR;
		}
		if ( Status < 0 ) {
			__xrtDirSetError(XDIR_ERROR_NEXT, "root-next",
				"failed while reading an anchored Windows directory",
				__xrtRootDirWindowsError(Status));
			return XDIR_NEXT_ERROR;
		}
		if ( Io.Information < offsetof(xrt_root_dir_information, FileName) ) {
			__xrtDirError(XERR_PROTOCOL, XDIR_ERROR_ENTRY,
				"root-next", "Windows returned a truncated directory entry");
			return XDIR_NEXT_ERROR;
		}
		pNative = (xrt_root_dir_information*)pState->Buffer;
		iNameBytes = (size_t)pNative->FileNameLength;
		if ( ((iNameBytes % sizeof(WCHAR)) != 0u) ||
			 (iNameBytes > (size_t)Io.Information -
			  offsetof(xrt_root_dir_information, FileName)) ) {
			__xrtDirError(XERR_PROTOCOL, XDIR_ERROR_ENTRY,
				"root-next", "Windows returned an invalid directory name length");
			return XDIR_NEXT_ERROR;
		}
		if ( !__xrtRootDirWindowsName(pState, pNative->FileName,
			iNameBytes / sizeof(WCHAR), &iNameSize) )
			return XDIR_NEXT_ERROR;
		if ( __xrtRootDirDot(pState->Name) ) continue;
		memset(&Entry, 0, sizeof(Entry));
		Entry.Name.Data = pState->Name;
		Entry.Name.Size = iNameSize;
		Entry.Flags = XDIR_ENTRY_UTF8;
		__xrtRootDirWindowsInfo(pNative, &Entry.Info);
		if ( (iFlags & XDIR_STAT) != 0u &&
			 !__xrtRootDirStat(pState, sDisplayPath,
				pState->Name, iFlags, &Entry.Info) )
			return XDIR_NEXT_ERROR;
		*pEntry = Entry;
		return XDIR_NEXT_ITEM;
	}
}

#else

/* 把 POSIX dirent 类型映射到无需额外 stat 的对象类别。 */
static xfiletype __xrtRootDirPosixType(unsigned char iType)
{
	#if defined(DT_REG)
		if ( iType == DT_REG ) return XFILE_TYPE_FILE;
		if ( iType == DT_DIR ) return XFILE_TYPE_DIRECTORY;
		if ( iType == DT_LNK ) return XFILE_TYPE_LINK;
		if ( iType == DT_FIFO ) return XFILE_TYPE_FIFO;
		if ( iType == DT_SOCK ) return XFILE_TYPE_SOCKET;
		if ( (iType == DT_CHR) || (iType == DT_BLK) )
			return XFILE_TYPE_DEVICE;
	#else
		(void)iType;
	#endif
	return XFILE_TYPE_NONE;
}



/* readdir 与 fstatat 始终以同一个打开目录描述符为锚点。 */
static xdirnext __xrtRootDirNext(ptr pData, cstr sDisplayPath,
	uint32 iFlags, xdirentry* pEntry)
{
	xrt_root_dir_state* pState = (xrt_root_dir_state*)pData;

	for ( ;; ) {
		struct dirent* pNative;
		xdirentry Entry;

		do {
			errno = 0;
			pNative = readdir(pState->Stream);
		} while ( (pNative == NULL) && (errno == EINTR) );
		if ( pNative == NULL ) {
			if ( errno == 0 ) return XDIR_NEXT_END;
			__xrtDirSetError(XDIR_ERROR_NEXT, "root-next",
				"failed while reading an anchored POSIX directory", errno);
			return XDIR_NEXT_ERROR;
		}
		if ( __xrtRootDirDot(pNative->d_name) ) continue;
		memset(&Entry, 0, sizeof(Entry));
		Entry.Name.Data = pNative->d_name;
		Entry.Name.Size = strlen(pNative->d_name);
		#if defined(DT_REG)
			Entry.Info.Type = __xrtRootDirPosixType(pNative->d_type);
		#else
			Entry.Info.Type = __xrtRootDirPosixType(0u);
		#endif
		if ( xrtUtf8Valid(Entry.Name, NULL) )
			Entry.Flags = XDIR_ENTRY_UTF8;
		if ( (iFlags & XDIR_STAT) != 0u &&
			 !__xrtRootDirStat(pState, sDisplayPath,
				pNative->d_name, iFlags, &Entry.Info) )
			return XDIR_NEXT_ERROR;
		*pEntry = Entry;
		return XDIR_NEXT_ITEM;
	}
}

#endif



/* 关闭枚举流和它拥有的目录句柄。 */
static bool __xrtRootDirClose(ptr pData)
{
	xrt_root_dir_state* pState = (xrt_root_dir_state*)pData;
	bool bResult = true;
	int iCode = 0;

	#if defined(_WIN32) || defined(_WIN64)
		if ( !__xrtRootNativeClose(pState->Directory, false) ) {
			bResult = false;
			iCode = (int)GetLastError();
		}
		xrtFree(pState->Name);
		xrtFree(pState->Buffer);
	#else
		if ( closedir(pState->Stream) != 0 ) {
			bResult = false;
			iCode = errno;
		}
	#endif
	xrtFree(pState);
	if ( !bResult ) {
		__xrtDirSetError(XDIR_ERROR_CLOSE, "root-close",
			"failed to close the anchored directory iterator", iCode);
	}
	return bResult;
}



static const xrt_dir_backend_ops __xrtRootDirOps = {
	(uint32)sizeof(xrt_dir_backend_ops),
	XRT_DIR_BACKEND_VERSION,
	false,
	{ 0u, 0u, 0u },
	__xrtRootDirNext,
	__xrtRootDirClose
};



/* 接管已经由 root resolver 打开的目录句柄。 */
xdir __xrtRootNativeDirTake(xrootnative Handle,
	cstr sDisplayPath, uint32 iFlags)
{
	xrt_root_dir_state* pState;

	if ( (Handle == XRT_ROOT_NATIVE_INVALID) ||
		 (sDisplayPath == NULL) || (sDisplayPath[0] == '\0') ||
		 ((iFlags & XDIR_INCLUDE_DOTS) != 0u) ) {
		if ( Handle != XRT_ROOT_NATIVE_INVALID )
			(void)__xrtRootNativeClose(Handle, false);
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pState = (xrt_root_dir_state*)xrtCalloc(1u, sizeof(*pState));
	if ( pState == NULL ) {
		(void)__xrtRootNativeClose(Handle, false);
		return NULL;
	}
	pState->Directory = Handle;
	#if defined(_WIN32) || defined(_WIN64)
		{
			HMODULE hModule = GetModuleHandleW(L"ntdll.dll");

			pState->Query = hModule != NULL ?
				(xrt_root_dir_query_proc)(uintptr_t)
				GetProcAddress(hModule, "NtQueryDirectoryFile") : NULL;
			if ( pState->Query == NULL ) {
				(void)__xrtRootNativeClose(Handle, false);
				xrtFree(pState);
				__xrtDirError(XERR_UNSUPPORTED, XDIR_ERROR_OPEN,
					"root-open", "NtQueryDirectoryFile is unavailable");
				return NULL;
			}
		}
		pState->BufferSize = XRT_ROOT_DIR_BUFFER_SIZE;
		pState->Buffer = (bytes)xrtMalloc(pState->BufferSize);
		if ( pState->Buffer == NULL ) {
			(void)__xrtRootNativeClose(Handle, false);
			xrtFree(pState);
			return NULL;
		}
		pState->Restart = true;
	#else
		pState->Stream = fdopendir(Handle);
		if ( pState->Stream == NULL ) {
			int iCode = errno;

			(void)__xrtRootNativeClose(Handle, false);
			xrtFree(pState);
			__xrtDirSetError(XDIR_ERROR_OPEN, "root-open",
				"failed to attach a stream to the anchored directory", iCode);
			return NULL;
		}
		pState->Directory = dirfd(pState->Stream);
	#endif
	return __xrtDirTakeBackend(&__xrtRootDirOps,
		pState, sDisplayPath, iFlags);
}

#endif
