#include "../internal/xrt_file_root.h"

#if defined(_WIN32) || defined(_WIN64)
	#if !defined(__TINYC__)
		#include <winternl.h>
	#endif
#endif



#if defined(XRT_FEATURE_FILE_ROOT) && \
	(defined(_WIN32) || defined(_WIN64))

#if defined(__TINYC__)

/* TinyCC 精简 Win32 头缺少 winternl.h，只声明本模块实际使用的稳定 NT ABI。 */
typedef LONG NTSTATUS;



typedef struct __xrt_unicode_string {
	USHORT Length;
	USHORT MaximumLength;
	PWSTR Buffer;
} UNICODE_STRING, *PUNICODE_STRING;



typedef struct __xrt_object_attributes {
	ULONG Length;
	HANDLE RootDirectory;
	PUNICODE_STRING ObjectName;
	ULONG Attributes;
	PVOID SecurityDescriptor;
	PVOID SecurityQualityOfService;
} OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;



typedef struct __xrt_io_status_block {
	union {
		NTSTATUS Status;
		PVOID Pointer;
	};
	ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

#endif

#if !defined(OBJ_CASE_INSENSITIVE)
	#define OBJ_CASE_INSENSITIVE 0x00000040u
#endif

#if !defined(OBJ_DONT_REPARSE)
	#define OBJ_DONT_REPARSE 0x00001000u
#endif

#if !defined(FILE_OPEN)
	#define FILE_OPEN 0x00000001u
#endif

#if !defined(FILE_CREATE)
	#define FILE_CREATE 0x00000002u
#endif

#if !defined(FILE_OPEN_IF)
	#define FILE_OPEN_IF 0x00000003u
#endif

#if !defined(FILE_OVERWRITE)
	#define FILE_OVERWRITE 0x00000004u
#endif

#if !defined(FILE_OVERWRITE_IF)
	#define FILE_OVERWRITE_IF 0x00000005u
#endif

#if !defined(FILE_DIRECTORY_FILE)
	#define FILE_DIRECTORY_FILE 0x00000001u
#endif

#if !defined(FILE_WRITE_THROUGH)
	#define FILE_WRITE_THROUGH 0x00000002u
#endif

#if !defined(FILE_SYNCHRONOUS_IO_NONALERT)
	#define FILE_SYNCHRONOUS_IO_NONALERT 0x00000020u
#endif

#if !defined(FILE_NON_DIRECTORY_FILE)
	#define FILE_NON_DIRECTORY_FILE 0x00000040u
#endif

#if !defined(FILE_OPEN_FOR_BACKUP_INTENT)
	#define FILE_OPEN_FOR_BACKUP_INTENT 0x00004000u
#endif

#if !defined(FILE_OPEN_REPARSE_POINT)
	#define FILE_OPEN_REPARSE_POINT 0x00200000u
#endif

#if !defined(FILE_LIST_DIRECTORY)
	#define FILE_LIST_DIRECTORY 0x00000001u
#endif

#if !defined(FILE_READ_ATTRIBUTES)
	#define FILE_READ_ATTRIBUTES 0x00000080u
#endif

#define XRT_STATUS_STOPPED_ON_SYMLINK ((NTSTATUS)0x8000002Du)
#define XRT_STATUS_REPARSE_POINT_ENCOUNTERED ((NTSTATUS)0xC000050Bu)



#if defined(__TINYC__)

/* TinyCC 的旧 Win32 头缺少 Vista 起提供的句柄删除入口。 */
BOOL WINAPI SetFileInformationByHandle(HANDLE hFile,
	int iClass, LPVOID pInformation, DWORD iSize);

#endif



/* ntdll 入口动态取得，避免给 TinyCC 增加额外导入库依赖。 */
typedef NTSTATUS (WINAPI *__xrt_nt_create_file_proc)(
	PHANDLE pFile, ACCESS_MASK iAccess,
	POBJECT_ATTRIBUTES pAttributes, PIO_STATUS_BLOCK pStatus,
	PLARGE_INTEGER pAllocation, ULONG iFileAttributes,
	ULONG iShare, ULONG iDisposition, ULONG iOptions,
	PVOID pEaBuffer, ULONG iEaSize);



/* 句柄相对硬链接使用 NT 文件信息接口。 */
typedef NTSTATUS (WINAPI *__xrt_nt_set_file_proc)(
	HANDLE hFile, PIO_STATUS_BLOCK pStatus,
	PVOID pInformation, ULONG iSize, int iClass);



/* NTSTATUS 到 Win32 错误的转换入口。 */
typedef ULONG (WINAPI *__xrt_nt_error_proc)(NTSTATUS Status);



/* 传统句柄删除信息只包含一个布尔字段。 */
typedef struct __xrt_root_disposition {
	BOOL DeleteFile;
} __xrt_root_disposition;



/* FileLinkInfo 的稳定布局允许使用目标目录句柄和相对文件名。 */
typedef struct __xrt_root_file_link_info {
	BYTE ReplaceIfExists;
	HANDLE RootDirectory;
	DWORD FileNameLength;
	WCHAR FileName[1];
} __xrt_root_file_link_info;



/* 判断 NTSTATUS 是否表示成功。 */
static bool __xrtRootNtSuccess(NTSTATUS Status)
{
	return Status >= 0;
}



/* 判断 NT 打开是否停在了重解析点。 */
static bool __xrtRootNtReparse(NTSTATUS Status)
{
	return (Status == XRT_STATUS_REPARSE_POINT_ENCOUNTERED) ||
		(Status == XRT_STATUS_STOPPED_ON_SYMLINK);
}



/* 把 NTSTATUS 转换为结构化错误使用的 Win32 代码。 */
static int __xrtRootNtErrorCode(NTSTATUS Status)
{
	HMODULE hModule = GetModuleHandleW(L"ntdll.dll");
	__xrt_nt_error_proc pConvert;

	if ( hModule == NULL ) {
		return ERROR_GEN_FAILURE;
	}
	pConvert = (__xrt_nt_error_proc)(uintptr_t)
		GetProcAddress(hModule, "RtlNtStatusToDosError");
	if ( pConvert == NULL ) {
		return ERROR_GEN_FAILURE;
	}
	return (int)pConvert(Status);
}



/* 使用目录句柄和单个 UTF-8 分量调用 NtCreateFile。 */
static bool __xrtRootNtCreate(xrootnative Parent, cstr sName,
	ACCESS_MASK iAccess, ULONG iShare, ULONG iDisposition,
	ULONG iOptions, ULONG iObjectFlags, HANDLE* pHandle,
	NTSTATUS* pStatus)
{
	HMODULE hModule = GetModuleHandleW(L"ntdll.dll");
	__xrt_nt_create_file_proc pCreate;
	size_t iUnits = 0;
	uint16* pWide;
	UNICODE_STRING Name;
	OBJECT_ATTRIBUTES Attributes;
	IO_STATUS_BLOCK Status;

	if ( hModule == NULL ) {
		__xrtRootError(XERR_UNSUPPORTED, XROOT_ERROR_RESOLVE,
			"native-open", "ntdll is unavailable for handle-relative file access");
		return false;
	}
	pCreate = (__xrt_nt_create_file_proc)(uintptr_t)
		GetProcAddress(hModule, "NtCreateFile");
	if ( pCreate == NULL ) {
		__xrtRootError(XERR_UNSUPPORTED, XROOT_ERROR_RESOLVE,
			"native-open", "NtCreateFile is unavailable for handle-relative access");
		return false;
	}
	pWide = __xrtPathToWide(sName, &iUnits);
	if ( pWide == NULL ) {
		return false;
	}
	if ( iUnits > (((size_t)UINT16_MAX / sizeof(uint16)) - 1u) ) {
		xrtFree(pWide);
		__xrtRootError(XERR_RANGE, XROOT_ERROR_LIMIT,
			"native-open", "the Windows root path component is too long");
		return false;
	}
	memset(&Name, 0, sizeof(Name));
	Name.Length = (USHORT)(iUnits * sizeof(uint16));
	Name.MaximumLength = (USHORT)((iUnits + 1u) * sizeof(uint16));
	Name.Buffer = (PWSTR)pWide;
	memset(&Attributes, 0, sizeof(Attributes));
	Attributes.Length = (ULONG)sizeof(Attributes);
	Attributes.RootDirectory = Parent;
	Attributes.ObjectName = &Name;
	Attributes.Attributes = OBJ_CASE_INSENSITIVE | iObjectFlags;
	memset(&Status, 0, sizeof(Status));
	*pHandle = INVALID_HANDLE_VALUE;
	*pStatus = pCreate(pHandle, iAccess, &Attributes, &Status,
		NULL, FILE_ATTRIBUTE_NORMAL, iShare, iDisposition,
		iOptions, NULL, 0u);
	xrtFree(pWide);
	return true;
}



/* 打开重解析点自身并读取受支持的链接目标。 */
static str __xrtRootWindowsReadAt(xrootnative Parent, cstr sName)
{
	HANDLE hLink;
	NTSTATUS Status;
	str sTarget;

	if ( !__xrtRootNtCreate(Parent, sName,
		SYNCHRONIZE | FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		FILE_OPEN,
		FILE_SYNCHRONOUS_IO_NONALERT |
			FILE_OPEN_FOR_BACKUP_INTENT |
			FILE_OPEN_REPARSE_POINT,
		0u, &hLink, &Status) ) {
		return NULL;
	}
	if ( !__xrtRootNtSuccess(Status) ) {
		__xrtRootSetError(XROOT_ERROR_LINK, "read-link",
			"failed to open the root-relative reparse point",
			__xrtRootNtErrorCode(Status));
		return NULL;
	}
	sTarget = __xrtLinkWindowsReadHandle(hLink, true);
	(void)CloseHandle(hLink);
	return sTarget;
}



/* 在 NT 打开失败后检查当前分量是否为受支持链接。 */
static xrootstep __xrtRootWindowsLink(xrootnative Parent,
	cstr sName, str* pLink, xrooterror Code,
	cstr sOperation, cstr sMessage)
{
	str sTarget = __xrtRootWindowsReadAt(Parent, sName);

	if ( sTarget != NULL ) {
		*pLink = sTarget;
		return XROOT_STEP_LINK;
	}
	__xrtRootWrapError(Code, sOperation, sMessage);
	return XROOT_STEP_ERROR;
}



/* 打开并验证初始 Windows 根目录。 */
bool __xrtRootNativeOpen(cstr sPath, xrootnative* pHandle)
{
	uint16* pWide = __xrtPathToWide(sPath, NULL);
	BY_HANDLE_FILE_INFORMATION Info;
	HANDLE hDirectory;

	if ( pWide == NULL ) {
		return false;
	}
	hDirectory = CreateFileW((const wchar_t*)pWide,
		FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if ( hDirectory == INVALID_HANDLE_VALUE ) {
		int iCode = (int)GetLastError();

		xrtFree(pWide);
		__xrtRootSetError(XROOT_ERROR_OPEN, "open",
			"failed to open the root directory", iCode);
		return false;
	}
	xrtFree(pWide);
	if ( !GetFileInformationByHandle(hDirectory, &Info) ) {
		int iCode = (int)GetLastError();

		(void)CloseHandle(hDirectory);
		__xrtRootSetError(XROOT_ERROR_OPEN, "open",
			"failed to inspect the root directory", iCode);
		return false;
	}
	if ( (Info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0u ) {
		(void)CloseHandle(hDirectory);
		__xrtRootError(XERR_TYPE, XROOT_ERROR_OPEN,
			"open", "the root path is not a directory");
		return false;
	}
	*pHandle = hDirectory;
	return true;
}



/* 关闭 Windows 根目录或中间目录句柄。 */
bool __xrtRootNativeClose(xrootnative Handle, bool bReport)
{
	if ( CloseHandle(Handle) ) {
		return true;
	}
	if ( bReport ) {
		int iCode = (int)GetLastError();

		__xrtRootSetError(XROOT_ERROR_CLOSE, "close",
			"failed to close the root directory", iCode);
	}
	return false;
}



/* 不跟随当前分量打开 Windows 子目录。 */
xrootstep __xrtRootNativeOpenDir(xrootnative Parent, cstr sName,
	xrootnative* pHandle, str* pLink)
{
	HANDLE hDirectory;
	NTSTATUS Status;

	if ( strcmp(sName, ".") == 0 ) {
		if ( !DuplicateHandle(GetCurrentProcess(), Parent,
			GetCurrentProcess(), &hDirectory, 0u, FALSE,
			DUPLICATE_SAME_ACCESS) ) {
			int iCode = (int)GetLastError();

			__xrtRootSetError(XROOT_ERROR_RESOLVE, "resolve",
				"failed to duplicate the root directory handle", iCode);
			return XROOT_STEP_ERROR;
		}
		*pHandle = hDirectory;
		return XROOT_STEP_DONE;
	}
	if ( !__xrtRootNtCreate(Parent, sName,
		SYNCHRONIZE | FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		FILE_OPEN,
		FILE_SYNCHRONOUS_IO_NONALERT |
			FILE_OPEN_FOR_BACKUP_INTENT | FILE_DIRECTORY_FILE,
		OBJ_DONT_REPARSE, &hDirectory, &Status) ) {
		return XROOT_STEP_ERROR;
	}
	if ( __xrtRootNtSuccess(Status) ) {
		*pHandle = hDirectory;
		return XROOT_STEP_DONE;
	}
	if ( __xrtRootNtReparse(Status) ) {
		return __xrtRootWindowsLink(Parent, sName,
			pLink, XROOT_ERROR_RESOLVE, "resolve",
			"failed to open a root path directory");
	}
	__xrtRootSetError(XROOT_ERROR_RESOLVE, "resolve",
		"failed to open a root path directory",
		__xrtRootNtErrorCode(Status));
	return XROOT_STEP_ERROR;
}



/* 把公共文件选项转换为 NT 创建方式。 */
static ULONG __xrtRootWindowsDisposition(uint32 iFlags)
{
	if ( (iFlags & XFILE_EXCLUSIVE) != 0u ) {
		return FILE_CREATE;
	}
	if ( (iFlags & XFILE_CREATE) != 0u ) {
		return ((iFlags & XFILE_TRUNCATE) != 0u) ?
			FILE_OVERWRITE_IF : FILE_OPEN_IF;
	}
	return ((iFlags & XFILE_TRUNCATE) != 0u) ?
		FILE_OVERWRITE : FILE_OPEN;
}



/* 把公共文件共享位转换为 Windows 共享方式。 */
static ULONG __xrtRootWindowsShare(uint32 iShare)
{
	ULONG iNative = 0u;

	if ( (iShare & XFILE_SHARE_READ) != 0u ) {
		iNative |= FILE_SHARE_READ;
	}
	if ( (iShare & XFILE_SHARE_WRITE) != 0u ) {
		iNative |= FILE_SHARE_WRITE;
	}
	if ( (iShare & XFILE_SHARE_DELETE) != 0u ) {
		iNative |= FILE_SHARE_DELETE;
	}
	return iNative;
}



/* 不跟随当前分量打开 Windows 普通文件。 */
xrootstep __xrtRootNativeOpenFile(xrootnative Parent, cstr sName,
	const xfileoptions* pOptions, xfile* pFile, str* pLink)
{
	ACCESS_MASK iAccess = SYNCHRONIZE | FILE_READ_ATTRIBUTES;
	ULONG iOptions = FILE_SYNCHRONOUS_IO_NONALERT |
		FILE_OPEN_FOR_BACKUP_INTENT | FILE_NON_DIRECTORY_FILE;
	HANDLE hFile;
	HANDLE hControl;
	NTSTATUS Status;

	if ( strcmp(sName, ".") == 0 ) {
		__xrtRootError(XERR_TYPE, XROOT_ERROR_FILE,
			"open-file", "the root-relative path is a directory");
		return XROOT_STEP_ERROR;
	}
	iAccess |= __xrtFileWindowsAccess(pOptions->Flags);
	if ( (pOptions->Flags & XFILE_SYNC) != 0u ) {
		iOptions |= FILE_WRITE_THROUGH;
	}
	if ( !__xrtRootNtCreate(Parent, sName, iAccess,
		__xrtRootWindowsShare(pOptions->Share),
		__xrtRootWindowsDisposition(pOptions->Flags),
		iOptions, OBJ_DONT_REPARSE, &hFile, &Status) ) {
		return XROOT_STEP_ERROR;
	}
	if ( __xrtRootNtSuccess(Status) ) {
		if ( !__xrtFileWindowsAppendHandles(
			&hFile, &hControl, pOptions->Flags) ) {
			int iCode = (int)GetLastError();

			(void)CloseHandle(hFile);
			__xrtRootSetError(XROOT_ERROR_FILE, "open-file",
				"failed to restrict the root-relative append handle",
				iCode);
			return XROOT_STEP_ERROR;
		}
		*pFile = __xrtFileTakeNativePair((intptr_t)hFile,
			(intptr_t)hControl, pOptions->Flags);
		return *pFile != NULL ? XROOT_STEP_DONE : XROOT_STEP_ERROR;
	}
	if ( __xrtRootNtReparse(Status) &&
		 ((pOptions->Flags & XFILE_NOFOLLOW) == 0u) &&
		 ((pOptions->Flags &
		  (XFILE_CREATE | XFILE_EXCLUSIVE)) !=
		  (XFILE_CREATE | XFILE_EXCLUSIVE)) ) {
		return __xrtRootWindowsLink(Parent, sName,
			pLink, XROOT_ERROR_FILE, "open-file",
			"failed to open the root-relative file");
	}
	__xrtRootSetError(XROOT_ERROR_FILE, "open-file",
		"failed to open the root-relative file",
		__xrtRootNtErrorCode(Status));
	return XROOT_STEP_ERROR;
}



/* 查询 Windows 根内当前分量元数据。 */
xrootstep __xrtRootNativeStat(xrootnative Parent, cstr sName,
	bool bFollowLink, xfileinfo* pInfo, str* pLink)
{
	HANDLE hObject;
	NTSTATUS Status;
	xfileinfo Info;

	if ( strcmp(sName, ".") == 0 ) {
		(void)bFollowLink;
		(void)pLink;
		if ( !__xrtFileWindowsStat(Parent, &Info, false) ) {
			int iCode = (int)GetLastError();

			__xrtRootSetError(XROOT_ERROR_STAT, "stat",
				"failed to query root directory metadata", iCode);
			return XROOT_STEP_ERROR;
		}
		*pInfo = Info;
		return XROOT_STEP_DONE;
	}
	if ( !__xrtRootNtCreate(Parent, sName,
		SYNCHRONIZE | FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		FILE_OPEN,
		FILE_SYNCHRONOUS_IO_NONALERT |
			FILE_OPEN_FOR_BACKUP_INTENT |
			FILE_OPEN_REPARSE_POINT,
		0u, &hObject, &Status) ) {
		return XROOT_STEP_ERROR;
	}
	if ( !__xrtRootNtSuccess(Status) ) {
		__xrtRootSetError(XROOT_ERROR_STAT, "stat",
			"failed to open root-relative metadata",
			__xrtRootNtErrorCode(Status));
		return XROOT_STEP_ERROR;
	}
	if ( !__xrtFileWindowsStat(hObject, &Info, false) ) {
		int iCode = (int)GetLastError();

		(void)CloseHandle(hObject);
		__xrtRootSetError(XROOT_ERROR_STAT, "stat",
			"failed to query root-relative metadata", iCode);
		return XROOT_STEP_ERROR;
	}
	if ( bFollowLink &&
		 ((Info.Attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u) ) {
		*pLink = __xrtLinkWindowsReadHandle(hObject, true);
		(void)CloseHandle(hObject);
		if ( *pLink == NULL ) {
			__xrtRootWrapError(XROOT_ERROR_STAT, "stat",
				"failed to resolve root-relative reparse metadata");
			return XROOT_STEP_ERROR;
		}
		return XROOT_STEP_LINK;
	}
	(void)CloseHandle(hObject);
	*pInfo = Info;
	return XROOT_STEP_DONE;
}



/* 相对 Windows 父目录创建一个新目录。 */
bool __xrtRootNativeCreateDir(xrootnative Parent,
	cstr sName, uint32 iMode)
{
	HANDLE hDirectory;
	NTSTATUS Status;

	(void)iMode;
	if ( !__xrtRootNtCreate(Parent, sName,
		SYNCHRONIZE | FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		FILE_CREATE,
		FILE_SYNCHRONOUS_IO_NONALERT |
			FILE_OPEN_FOR_BACKUP_INTENT | FILE_DIRECTORY_FILE,
		OBJ_DONT_REPARSE, &hDirectory, &Status) ) {
		return false;
	}
	if ( !__xrtRootNtSuccess(Status) ) {
		__xrtRootSetError(XROOT_ERROR_CREATE, "create-dir",
			"failed to create the root-relative directory",
			__xrtRootNtErrorCode(Status));
		return false;
	}
	(void)CloseHandle(hDirectory);
	return true;
}



/* 相对 Windows 父目录删除非目录对象或空目录。 */
bool __xrtRootNativeRemove(xrootnative Parent,
	cstr sName, bool bDirectoryOnly)
{
	HANDLE hObject;
	NTSTATUS Status;
	__xrt_root_disposition Disposition;
	ULONG iOptions = FILE_SYNCHRONOUS_IO_NONALERT |
		FILE_OPEN_FOR_BACKUP_INTENT | FILE_OPEN_REPARSE_POINT;
	ULONG iObjectFlags = 0u;

	if ( bDirectoryOnly ) {
		iOptions |= FILE_DIRECTORY_FILE;
		iObjectFlags |= OBJ_DONT_REPARSE;
	}

	if ( !__xrtRootNtCreate(Parent, sName,
		SYNCHRONIZE | FILE_READ_ATTRIBUTES | DELETE,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		FILE_OPEN, iOptions, iObjectFlags, &hObject, &Status) ) {
		return false;
	}
	if ( !__xrtRootNtSuccess(Status) ) {
		__xrtRootSetError(XROOT_ERROR_REMOVE, "remove",
			"failed to open the root-relative object for removal",
			__xrtRootNtErrorCode(Status));
		return false;
	}
	Disposition.DeleteFile = TRUE;
	if ( !SetFileInformationByHandle(hObject,
		#if defined(__TINYC__)
			4,
		#else
			FileDispositionInfo,
		#endif
		&Disposition, (DWORD)sizeof(Disposition)) ) {
		int iCode = (int)GetLastError();

		(void)CloseHandle(hObject);
		__xrtRootSetError(XROOT_ERROR_REMOVE, "remove",
			"failed to remove the root-relative object", iCode);
		return false;
	}
	if ( !CloseHandle(hObject) ) {
		int iCode = (int)GetLastError();

		__xrtRootSetError(XROOT_ERROR_REMOVE, "remove-close",
			"the object was removed but its handle did not close", iCode);
		return false;
	}
	return true;
}



/* 相对 Windows 父目录读取末级重解析链接。 */
str __xrtRootNativeReadLink(xrootnative Parent, cstr sName)
{
	str sTarget = __xrtRootWindowsReadAt(Parent, sName);

	if ( sTarget == NULL ) {
		__xrtRootWrapError(XROOT_ERROR_LINK, "read-link",
			"failed to read the root-relative reparse link");
	}
	return sTarget;
}





/* Windows 暂不提供可证明安全的句柄相对符号链接创建。 */
bool __xrtRootNativeLinkCreate(xrootnative Parent,
	cstr sName, cstr sTarget, bool bDirectory)
{
	(void)Parent;
	(void)sName;
	(void)sTarget;
	(void)bDirectory;
	__xrtRootError(XERR_UNSUPPORTED, XROOT_ERROR_LINK,
		"create-link", "root-relative symbolic-link creation is not supported on Windows");
	return false;
}



/* 使用 FileLinkInfo 在两个已锚定 Windows 目录之间创建硬链接。 */
bool __xrtRootNativeLinkHard(xrootnative SourceParent, cstr sSource,
	xrootnative TargetParent, cstr sTarget)
{
	HANDLE hSource;
	NTSTATUS Status;
	uint16* pWide;
	size_t iUnits = 0u;
	size_t iBytes;
	__xrt_root_file_link_info* pInfo;
	HMODULE hModule;
	__xrt_nt_set_file_proc pSet;
	IO_STATUS_BLOCK IoStatus;

	if ( !__xrtRootNtCreate(SourceParent, sSource,
		DELETE | SYNCHRONIZE | FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		FILE_OPEN,
		FILE_SYNCHRONOUS_IO_NONALERT |
			FILE_OPEN_FOR_BACKUP_INTENT | FILE_NON_DIRECTORY_FILE,
		OBJ_DONT_REPARSE, &hSource, &Status) ) {
		return false;
	}
	if ( !__xrtRootNtSuccess(Status) ) {
		__xrtRootSetError(XROOT_ERROR_LINK, "hard-link",
			"failed to open the root-relative hard-link source",
			__xrtRootNtErrorCode(Status));
		return false;
	}
	pWide = __xrtPathToWide(sTarget, &iUnits);
	if ( pWide == NULL ) {
		(void)CloseHandle(hSource);
		return false;
	}
	if ( (iUnits > ((SIZE_MAX - offsetof(__xrt_root_file_link_info,
		FileName)) / sizeof(WCHAR))) ||
		 (iUnits > (UINT32_MAX / sizeof(WCHAR))) ) {
		xrtFree(pWide);
		(void)CloseHandle(hSource);
		__xrtRootError(XERR_RANGE, XROOT_ERROR_LIMIT,
			"hard-link", "the root-relative hard-link name is too long");
		return false;
	}
	iBytes = offsetof(__xrt_root_file_link_info, FileName) +
		(iUnits * sizeof(WCHAR));
	if ( iBytes < sizeof(__xrt_root_file_link_info) ) {
		iBytes = sizeof(__xrt_root_file_link_info);
	}
	pInfo = (__xrt_root_file_link_info*)xrtCalloc(1u, iBytes);
	if ( pInfo == NULL ) {
		xrtFree(pWide);
		(void)CloseHandle(hSource);
		return false;
	}
	pInfo->ReplaceIfExists = FALSE;
	pInfo->RootDirectory = TargetParent;
	pInfo->FileNameLength = (DWORD)(iUnits * sizeof(WCHAR));
	memcpy(pInfo->FileName, pWide, iUnits * sizeof(WCHAR));
	xrtFree(pWide);
	hModule = GetModuleHandleW(L"ntdll.dll");
	pSet = hModule != NULL ? (__xrt_nt_set_file_proc)(uintptr_t)
		GetProcAddress(hModule, "NtSetInformationFile") : NULL;
	if ( pSet == NULL ) {
		xrtFree(pInfo);
		(void)CloseHandle(hSource);
		__xrtRootError(XERR_UNSUPPORTED, XROOT_ERROR_LINK,
			"hard-link", "NtSetInformationFile is unavailable for handle-relative hard links");
		return false;
	}
	memset(&IoStatus, 0, sizeof(IoStatus));
	Status = pSet(hSource, &IoStatus, pInfo,
		(ULONG)iBytes, 11);
	xrtFree(pInfo);
	if ( !__xrtRootNtSuccess(Status) ) {
		(void)CloseHandle(hSource);
		__xrtRootSetError(XROOT_ERROR_LINK, "hard-link",
			"failed to create the root-relative hard link",
			__xrtRootNtErrorCode(Status));
		return false;
	}
	if ( !CloseHandle(hSource) ) {
		int iCode = (int)GetLastError();

		__xrtRootSetError(XROOT_ERROR_CLOSE, "hard-link-close",
			"the hard-link source handle did not close", iCode);
		return false;
	}
	return true;
}



/* Windows 不提供 POSIX FIFO。 */
bool __xrtRootNativeFifoCreate(xrootnative Parent,
	cstr sName, uint32 iMode)
{
	(void)Parent;
	(void)sName;
	(void)iMode;
	__xrtRootError(XERR_UNSUPPORTED, XROOT_ERROR_CREATE,
		"create-fifo", "POSIX FIFOs are not available on Windows");
	return false;
}



/* Windows 忽略 POSIX 模式，但仍验证目标存在且安全解析。 */
xrootstep __xrtRootNativeSetMode(xrootnative Parent,
	cstr sName, bool bFollowLink, uint32 iMode, str* pLink)
{
	xfileinfo Info;

	(void)iMode;
	return __xrtRootNativeStat(Parent, sName,
		bFollowLink, &Info, pLink);
}

#endif
