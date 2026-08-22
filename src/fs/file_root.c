#include "../internal/xrt_file_root.h"



#if defined(XRT_FEATURE_FILE_ROOT)

#define XRT_ROOT_MAX_STEPS 255u
#define XRT_ROOT_MAX_LINKS 8u



/* 可变路径只分配一份文本和一份分量指针表。 */
typedef struct __xrt_root_path {
	str Text;
	str* Parts;
	size_t Count;
	bool Trailing;
} __xrt_root_path;



/* 末级操作可以返回需要继续解析的链接。 */
typedef xrootstep (*__xrt_root_proc)(xrootnative Parent,
	cstr sName, bool bTrailing, ptr pData, str* pLink);



/* 文件打开回调的输入选项和输出对象。 */
typedef struct __xrt_root_file_data {
	const xfileoptions* Options;
	xfile File;
} __xrt_root_file_data;



/* 元数据回调保留跟随策略和成功结果。 */
typedef struct __xrt_root_stat_data {
	bool Follow;
	xfileinfo Info;
} __xrt_root_stat_data;



/* 符号链接创建回调保存目标文本和目录提示。 */
typedef struct __xrt_root_link_data {
	cstr Target;
	bool Directory;
} __xrt_root_link_data;



/* 硬链接目标回调借用已经解析的源父目录和名称。 */
typedef struct __xrt_root_hard_target {
	xrootnative SourceParent;
	cstr SourceName;
} __xrt_root_hard_target;



/* 硬链接源回调保存目标根和目标相对路径。 */
typedef struct __xrt_root_hard_source {
	xroot Root;
	cstr TargetPath;
} __xrt_root_hard_source;



/* 权限回调保存跟随策略和目标模式。 */
typedef struct __xrt_root_mode_data {
	bool Follow;
	uint32 Mode;
} __xrt_root_mode_data;



/* 设置带系统代码的目录根错误。 */
void __xrtRootSetError(xrooterror Code, cstr sOperation,
	cstr sMessage, int iSystemCode)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = __xrtSystemErrorKind(iSystemCode);
	Desc.Domain = "xrt.root";
	Desc.Code = (int32)Code;
	Desc.SystemCode = (int32)iSystemCode;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 设置不带系统代码的目录根错误。 */
void __xrtRootError(xerrkind Kind, xrooterror Code,
	cstr sOperation, cstr sMessage)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.root";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 把当前错误保留为原因并改写为目录根错误。 */
void __xrtRootWrapError(xrooterror Code,
	cstr sOperation, cstr sMessage)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	if ( pCause == NULL ) {
		__xrtRootError(XERR_INTERNAL, Code, sOperation, sMessage);
		return;
	}
	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = xrtErrorKind(pCause);
	Desc.Domain = "xrt.root";
	Desc.Code = (int32)Code;
	Desc.SystemCode = xrtErrorSystemCode(pCause);
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	xrtErrorFree(pCause);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 判断当前平台接受的目录分隔符。 */
static bool __xrtRootSeparator(char iChar)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (iChar == '/') || (iChar == '\\');
	#else
		return iChar == '/';
	#endif
}



#if defined(_WIN32) || defined(_WIN64)

/* 把 ASCII 文件名字符转换为大写，避免依赖进程区域设置。 */
static char __xrtRootUpper(char iChar)
{
	if ( (iChar >= 'a') && (iChar <= 'z') ) {
		return (char)(iChar - ('a' - 'A'));
	}
	return iChar;
}



/* 判断 Windows 保留的 DOS 设备名。 */
static bool __xrtRootDeviceName(cstr sName)
{
	size_t iSize = 0;
	char sBase[8];
	size_t i;

	while ( (sName[iSize] != '\0') && (sName[iSize] != '.') ) {
		iSize++;
	}
	if ( (iSize == 0u) || (iSize >= sizeof(sBase)) ) {
		return false;
	}
	for ( i = 0; i < iSize; i++ ) {
		sBase[i] = __xrtRootUpper(sName[i]);
	}
	sBase[iSize] = '\0';
	if ( (strcmp(sBase, "CON") == 0) ||
		 (strcmp(sBase, "PRN") == 0) ||
		 (strcmp(sBase, "AUX") == 0) ||
		 (strcmp(sBase, "NUL") == 0) ||
		 (strcmp(sBase, "CONIN$") == 0) ||
		 (strcmp(sBase, "CONOUT$") == 0) ) {
		return true;
	}
	if ( (iSize == 5u) &&
		 ((memcmp(sBase, "COM", 3u) == 0) ||
		  (memcmp(sBase, "LPT", 3u) == 0)) &&
		 ((unsigned char)sBase[3] == 0xC2u) &&
		 (((unsigned char)sBase[4] == 0xB9u) ||
		  ((unsigned char)sBase[4] == 0xB2u) ||
		  ((unsigned char)sBase[4] == 0xB3u)) ) {
		return true;
	}
	return (iSize == 4u) &&
		(((memcmp(sBase, "COM", 3u) == 0) ||
		  (memcmp(sBase, "LPT", 3u) == 0)) &&
		 (sBase[3] >= '1') && (sBase[3] <= '9'));
}



/* 检查 Windows 根内分量不会触发设备名、数据流或名称折叠。 */
static bool __xrtRootWindowsPart(cstr sPart)
{
	size_t iSize = strlen(sPart);
	size_t i;

	if ( !xrtUtf8Valid(xrtStrView(sPart), NULL) ) {
		__xrtRootError(XERR_ARGUMENT, XROOT_ERROR_RESOLVE,
			"resolve", "the Windows root path is not valid UTF-8");
		return false;
	}
	if ( (iSize == 0u) || (sPart[iSize - 1u] == '.') ||
		 (sPart[iSize - 1u] == ' ') || __xrtRootDeviceName(sPart) ) {
		__xrtRootError(XERR_ARGUMENT, XROOT_ERROR_RESOLVE,
			"resolve", "the Windows root path contains a reserved name");
		return false;
	}
	for ( i = 0; i < iSize; i++ ) {
		unsigned char iChar = (unsigned char)sPart[i];

		if ( (iChar < 32u) || (iChar == ':') || (iChar == '*') ||
			 (iChar == '?') || (iChar == '"') || (iChar == '<') ||
			 (iChar == '>') || (iChar == '|') ) {
			__xrtRootError(XERR_ARGUMENT, XROOT_ERROR_RESOLVE,
				"resolve", "the Windows root path contains an invalid character");
			return false;
		}
	}
	return true;
}

#endif



/* 释放一份路径分量视图。 */
static void __xrtRootPathFree(__xrt_root_path* pPath)
{
	if ( pPath == NULL ) {
		return;
	}
	xrtFree(pPath->Parts);
	xrtFree(pPath->Text);
	memset(pPath, 0, sizeof(*pPath));
}



/* 把相对路径切分为可重新组合的分量。 */
static bool __xrtRootPathParse(cstr sPath, __xrt_root_path* pPath)
{
	size_t iSize;
	size_t iCapacity;
	char* pRead;

	memset(pPath, 0, sizeof(*pPath));
	if ( (sPath == NULL) || (sPath[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtRootSeparator(sPath[0]) ) {
		__xrtRootError(XERR_PERMISSION, XROOT_ERROR_ESCAPE,
			"resolve", "an absolute path cannot be used inside a root");
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( (((sPath[0] >= 'A') && (sPath[0] <= 'Z')) ||
			  ((sPath[0] >= 'a') && (sPath[0] <= 'z'))) &&
			 (sPath[1] == ':') ) {
			__xrtRootError(XERR_PERMISSION, XROOT_ERROR_ESCAPE,
				"resolve", "a volume path cannot be used inside a root");
			return false;
		}
	#endif
	iSize = strlen(sPath);
	if ( iSize > ((SIZE_MAX / sizeof(str)) - 2u) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iCapacity = 2u;
	{
		size_t i;

		for ( i = 0; i < iSize; i++ ) {
			if ( __xrtRootSeparator(sPath[i]) ) {
				iCapacity++;
			}
		}
	}
	pPath->Text = xrtStrDup(sPath);
	if ( pPath->Text == NULL ) {
		return false;
	}
	pPath->Parts = (str*)xrtMalloc(iCapacity * sizeof(str));
	if ( pPath->Parts == NULL ) {
		__xrtRootPathFree(pPath);
		return false;
	}
	pPath->Trailing = __xrtRootSeparator(sPath[iSize - 1u]);
	pRead = pPath->Text;
	while ( *pRead != '\0' ) {
		char* pPart;

		while ( __xrtRootSeparator(*pRead) ) {
			pRead++;
		}
		if ( *pRead == '\0' ) {
			break;
		}
		pPart = pRead;
		while ( (*pRead != '\0') && !__xrtRootSeparator(*pRead) ) {
			pRead++;
		}
		if ( *pRead != '\0' ) {
			*pRead++ = '\0';
		}
		if ( strcmp(pPart, ".") == 0 ) {
			continue;
		}
		#if defined(_WIN32) || defined(_WIN64)
			if ( (strcmp(pPart, "..") != 0) &&
				 !__xrtRootWindowsPart(pPart) ) {
				__xrtRootPathFree(pPath);
				return false;
			}
		#endif
		pPath->Parts[pPath->Count++] = pPart;
	}
	if ( pPath->Count == 0u ) {
		pPath->Parts[0] = (str)".";
		pPath->Count = 1u;
	}
	return true;
}



/* 把链接目标替换进当前路径，解析将从根目录重新开始。 */
static bool __xrtRootPathLink(__xrt_root_path* pPath,
	size_t iIndex, cstr sTarget)
{
	const char iSeparator =
	#if defined(_WIN32) || defined(_WIN64)
		'\\';
	#else
		'/';
	#endif
	size_t iTargetSize;
	size_t iSize = 0;
	bool bTrailing;
	str sJoined;
	char* pWrite;
	size_t i;
	__xrt_root_path Joined;

	if ( (sTarget == NULL) || (sTarget[0] == '\0') ) {
		__xrtRootError(XERR_PROTOCOL, XROOT_ERROR_LINK,
			"resolve-link", "the link has an empty target");
		return false;
	}
	if ( __xrtRootSeparator(sTarget[0]) ) {
		__xrtRootError(XERR_PERMISSION, XROOT_ERROR_ESCAPE,
			"resolve-link", "an absolute link target escapes the root");
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( (((sTarget[0] >= 'A') && (sTarget[0] <= 'Z')) ||
			 ((sTarget[0] >= 'a') && (sTarget[0] <= 'z'))) &&
			 (sTarget[1] == ':') ) {
			__xrtRootError(XERR_PERMISSION, XROOT_ERROR_ESCAPE,
				"resolve-link", "a volume link target escapes the root");
			return false;
		}
	#endif
	iTargetSize = strlen(sTarget);
	bTrailing = pPath->Trailing ||
		((iIndex == (pPath->Count - 1u)) &&
		 __xrtRootSeparator(sTarget[iTargetSize - 1u]));
	for ( i = 0; i < iIndex; i++ ) {
		size_t iPartSize = strlen(pPath->Parts[i]);

		if ( iSize > (SIZE_MAX - iPartSize - 1u) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iSize += iPartSize + 1u;
	}
	if ( iSize > (SIZE_MAX - iTargetSize - 1u) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iSize += iTargetSize;
	for ( i = iIndex + 1u; i < pPath->Count; i++ ) {
		size_t iPartSize = strlen(pPath->Parts[i]);

		if ( iSize > (SIZE_MAX - iPartSize - 1u) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iSize += iPartSize + 1u;
	}
	if ( bTrailing ) {
		if ( iSize == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iSize++;
	}
	if ( iSize == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	sJoined = (str)xrtMalloc(iSize + 1u);
	if ( sJoined == NULL ) {
		return false;
	}
	pWrite = sJoined;
	for ( i = 0; i < iIndex; i++ ) {
		size_t iPartSize = strlen(pPath->Parts[i]);

		memcpy(pWrite, pPath->Parts[i], iPartSize);
		pWrite += iPartSize;
		*pWrite++ = iSeparator;
	}
	memcpy(pWrite, sTarget, iTargetSize);
	pWrite += iTargetSize;
	for ( i = iIndex + 1u; i < pPath->Count; i++ ) {
		size_t iPartSize = strlen(pPath->Parts[i]);

		if ( (pWrite != sJoined) && !__xrtRootSeparator(pWrite[-1]) ) {
			*pWrite++ = iSeparator;
		}
		memcpy(pWrite, pPath->Parts[i], iPartSize);
		pWrite += iPartSize;
	}
	if ( bTrailing && (pWrite != sJoined) &&
		 !__xrtRootSeparator(pWrite[-1]) ) {
		*pWrite++ = iSeparator;
	}
	*pWrite = '\0';
	if ( !__xrtRootPathParse(sJoined, &Joined) ) {
		xrtFree(sJoined);
		return false;
	}
	xrtFree(sJoined);
	__xrtRootPathFree(pPath);
	*pPath = Joined;
	return true;
}



/* 关闭中间目录并恢复到根句柄。 */
static void __xrtRootRestart(xroot Root, xrootnative* pDirectory)
{
	if ( *pDirectory != Root->Handle ) {
		(void)__xrtRootNativeClose(*pDirectory, false);
	}
	*pDirectory = Root->Handle;
}



/* 逐分量解析路径，并且只通过目录句柄访问后续对象。 */
static bool __xrtRootResolve(xroot Root, cstr sPath,
	__xrt_root_proc pProc, ptr pData)
{
	__xrt_root_path Path;
	xrootnative Directory;
	size_t iIndex = 0;
	size_t iSteps = 0;
	size_t iLinks = 0;
	bool bResult = false;

	if ( (Root == NULL) || (pProc == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtRootPathParse(sPath, &Path) ) {
		return false;
	}
	Directory = Root->Handle;
	while ( iIndex < Path.Count ) {
		xrootstep Step;
		str sLink = NULL;

		if ( ++iSteps > XRT_ROOT_MAX_STEPS ) {
			__xrtRootError(XERR_RANGE, XROOT_ERROR_LIMIT,
				"resolve", "the root path requires too many resolution steps");
			break;
		}
		if ( strcmp(Path.Parts[iIndex], "..") == 0 ) {
			if ( iIndex == 0u ) {
				__xrtRootError(XERR_PERMISSION, XROOT_ERROR_ESCAPE,
					"resolve", "the path escapes above the root directory");
				break;
			}
			memmove(Path.Parts + iIndex - 1u,
				Path.Parts + iIndex + 1u,
				(Path.Count - iIndex - 1u) * sizeof(str));
			Path.Count -= 2u;
			if ( Path.Count == 0u ) {
				Path.Parts[0] = (str)".";
				Path.Count = 1u;
			}
			iIndex = 0u;
			__xrtRootRestart(Root, &Directory);
			continue;
		}
		if ( iIndex == (Path.Count - 1u) ) {
			Step = pProc(Directory, Path.Parts[iIndex],
				Path.Trailing, pData, &sLink);
		} else {
			xrootnative Next = XRT_ROOT_NATIVE_INVALID;

			Step = __xrtRootNativeOpenDir(Directory,
				Path.Parts[iIndex], &Next, &sLink);
			if ( Step == XROOT_STEP_DONE ) {
				if ( Directory != Root->Handle ) {
					(void)__xrtRootNativeClose(Directory, false);
				}
				Directory = Next;
				iIndex++;
				continue;
			}
		}
		if ( Step == XROOT_STEP_DONE ) {
			bResult = true;
			break;
		}
		if ( Step == XROOT_STEP_ERROR ) {
			break;
		}
		if ( ++iLinks > XRT_ROOT_MAX_LINKS ) {
			xrtFree(sLink);
			__xrtRootError(XERR_RANGE, XROOT_ERROR_LIMIT,
				"resolve-link", "the root path contains too many links");
			break;
		}
		if ( !__xrtRootPathLink(&Path, iIndex, sLink) ) {
			xrtFree(sLink);
			break;
		}
		xrtFree(sLink);
		iIndex = 0u;
		__xrtRootRestart(Root, &Directory);
	}
	if ( Directory != Root->Handle ) {
		(void)__xrtRootNativeClose(Directory, false);
	}
	__xrtRootPathFree(&Path);
	return bResult;
}



/* 从已经打开的目录句柄创建根对象。 */
static xroot __xrtRootCreate(xrootnative Handle, cstr sPath)
{
	xroot Root = (xroot)xrtMalloc(sizeof(*Root));

	if ( Root == NULL ) {
		(void)__xrtRootNativeClose(Handle, false);
		return NULL;
	}
	Root->Path = xrtStrDup(sPath);
	if ( Root->Path == NULL ) {
		(void)__xrtRootNativeClose(Handle, false);
		xrtFree(Root);
		return NULL;
	}
	Root->Handle = Handle;
	return Root;
}



/* 打开并锚定一个真实目录。 */
XRT_API xroot xrtRootOpen(cstr sPath)
{
	xrootnative Handle = XRT_ROOT_NATIVE_INVALID;

	if ( (sPath == NULL) || (sPath[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !__xrtRootNativeOpen(sPath, &Handle) ) {
		return NULL;
	}
	return __xrtRootCreate(Handle, sPath);
}



/* 子根解析末级操作。 */
static xrootstep __xrtRootOpenInProc(xrootnative Parent,
	cstr sName, bool bTrailing, ptr pData, str* pLink)
{
	xrootnative* pHandle = (xrootnative*)pData;

	(void)bTrailing;
	return __xrtRootNativeOpenDir(Parent, sName, pHandle, pLink);
}



/* 在已有根内打开并锚定一个子目录。 */
XRT_API xroot xrtRootOpenIn(xroot Root, cstr sPath)
{
	xrootnative Handle = XRT_ROOT_NATIVE_INVALID;
	str sDisplay;

	if ( (Root == NULL) || (sPath == NULL) || (sPath[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	sDisplay = xrtPathJoin(Root->Path, sPath);
	if ( sDisplay == NULL ) {
		return NULL;
	}
	if ( !__xrtRootResolve(Root, sPath,
		__xrtRootOpenInProc, &Handle) ) {
		xrtFree(sDisplay);
		return NULL;
	}
	{
		xroot Child = __xrtRootCreate(Handle, sDisplay);

		xrtFree(sDisplay);
		return Child;
	}
}



/* 关闭原生目录句柄并销毁根对象。 */
XRT_API bool xrtRootClose(xroot Root)
{
	bool bResult;

	if ( Root == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	bResult = __xrtRootNativeClose(Root->Handle, true);
	xrtFree(Root->Path);
	xrtFree(Root);
	return bResult;
}



/* 返回创建根对象时保存的诊断路径。 */
XRT_API cstr xrtRootPath(xroot Root)
{
	if ( Root == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return Root->Path;
}



/* 返回根目录原生句柄的整数表示。 */
XRT_API intptr_t xrtRootNative(xroot Root)
{
	if ( Root == NULL ) {
		__xrtErrorSetInvalidArgument();
		return (intptr_t)-1;
	}
	return (intptr_t)Root->Handle;
}



/* 根内文件打开末级操作。 */
static xrootstep __xrtRootFileProc(xrootnative Parent,
	cstr sName, bool bTrailing, ptr pData, str* pLink)
{
	__xrt_root_file_data* pFile = (__xrt_root_file_data*)pData;

	if ( bTrailing ) {
		__xrtRootError(XERR_TYPE, XROOT_ERROR_FILE,
			"open-file", "a file path cannot end with a directory separator");
		return XROOT_STEP_ERROR;
	}
	return __xrtRootNativeOpenFile(Parent, sName,
		pFile->Options, &pFile->File, pLink);
}



/* 在根内使用完整文件选项打开普通文件。 */
XRT_API xfile xrtRootFileOpen(xroot Root, cstr sPath,
	const xfileoptions* pOptions)
{
	__xrt_root_file_data Data;
	xfileoptions Options;

	if ( (Root == NULL) || (sPath == NULL) || (sPath[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !__xrtFileOptions(pOptions, &Options) ) {
		return NULL;
	}
	Data.Options = &Options;
	Data.File = NULL;
	if ( !__xrtRootResolve(Root, sPath, __xrtRootFileProc, &Data) ) {
		return NULL;
	}
	return Data.File;
}



/* 根内元数据查询末级操作。 */
static xrootstep __xrtRootStatProc(xrootnative Parent,
	cstr sName, bool bTrailing, ptr pData, str* pLink)
{
	__xrt_root_stat_data* pStat = (__xrt_root_stat_data*)pData;
	xrootstep Step = __xrtRootNativeStat(Parent, sName,
		pStat->Follow, &pStat->Info, pLink);

	if ( (Step == XROOT_STEP_DONE) && bTrailing &&
		 (pStat->Info.Type != XFILE_TYPE_DIRECTORY) ) {
		__xrtRootError(XERR_TYPE, XROOT_ERROR_STAT,
			"stat", "a path ending with a separator is not a directory");
		return XROOT_STEP_ERROR;
	}
	return Step;
}



/* 查询根内对象元数据。 */
XRT_API bool xrtRootStat(xroot Root, cstr sPath,
	bool bFollowLink, xfileinfo* pInfo)
{
	__xrt_root_stat_data Data;

	if ( (Root == NULL) || (sPath == NULL) || (sPath[0] == '\0') ||
		 (pInfo == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Data, 0, sizeof(Data));
	Data.Follow = bFollowLink;
	if ( !__xrtRootResolve(Root, sPath, __xrtRootStatProc, &Data) ) {
		return false;
	}
	*pInfo = Data.Info;
	return true;
}



/* 根内目录创建末级操作。 */
static xrootstep __xrtRootCreateProc(xrootnative Parent,
	cstr sName, bool bTrailing, ptr pData, str* pLink)
{
	uint32 iMode = *(const uint32*)pData;

	(void)bTrailing;
	(void)pLink;
	return __xrtRootNativeCreateDir(Parent, sName, iMode) ?
		XROOT_STEP_DONE : XROOT_STEP_ERROR;
}



/* 在根内创建一个目录。 */
XRT_API bool xrtRootDirCreate(xroot Root, cstr sPath, uint32 iMode)
{
	if ( (Root == NULL) || (sPath == NULL) || (sPath[0] == '\0') ||
		 ((iMode & ~07777u) != 0u) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtRootResolve(Root, sPath, __xrtRootCreateProc, &iMode);
}



/* 根内删除末级操作。 */
static xrootstep __xrtRootRemoveProc(xrootnative Parent,
	cstr sName, bool bTrailing, ptr pData, str* pLink)
{
	(void)pData;
	(void)pLink;
	if ( strcmp(sName, ".") == 0 ) {
		__xrtRootError(XERR_PERMISSION, XROOT_ERROR_ESCAPE,
			"remove", "the root directory itself cannot be removed");
		return XROOT_STEP_ERROR;
	}
	return __xrtRootNativeRemove(Parent, sName, bTrailing) ?
		XROOT_STEP_DONE : XROOT_STEP_ERROR;
}



/* 删除根内一个非目录对象或空目录。 */
XRT_API bool xrtRootRemove(xroot Root, cstr sPath)
{
	if ( (Root == NULL) || (sPath == NULL) || (sPath[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtRootResolve(Root, sPath, __xrtRootRemoveProc, NULL);
}



/* 根内链接读取末级操作。 */
static xrootstep __xrtRootLinkProc(xrootnative Parent,
	cstr sName, bool bTrailing, ptr pData, str* pLink)
{
	str* pTarget = (str*)pData;

	(void)pLink;
	if ( bTrailing ) {
		__xrtRootError(XERR_TYPE, XROOT_ERROR_LINK,
			"read-link", "a link path cannot end with a directory separator");
		return XROOT_STEP_ERROR;
	}
	*pTarget = __xrtRootNativeReadLink(Parent, sName);
	return *pTarget != NULL ? XROOT_STEP_DONE : XROOT_STEP_ERROR;
}



/* 读取根内末级链接保存的目标文本。 */
XRT_API str xrtRootLinkRead(xroot Root, cstr sPath)
{
	str sTarget = NULL;

	if ( (Root == NULL) || (sPath == NULL) || (sPath[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !__xrtRootResolve(Root, sPath, __xrtRootLinkProc, &sTarget) ) {
		return NULL;
	}
	return sTarget;
}





/* 根内符号链接创建末级操作。 */
static xrootstep __xrtRootLinkCreateProc(xrootnative Parent,
	cstr sName, bool bTrailing, ptr pData, str* pLink)
{
	const __xrt_root_link_data* pLinkData =
		(const __xrt_root_link_data*)pData;

	(void)pLink;
	if ( bTrailing || (strcmp(sName, ".") == 0) ) {
		__xrtRootError(XERR_ARGUMENT, XROOT_ERROR_LINK,
			"create-link", "a link path must name a new root-relative object");
		return XROOT_STEP_ERROR;
	}
	return __xrtRootNativeLinkCreate(Parent, sName,
		pLinkData->Target, pLinkData->Directory) ?
		XROOT_STEP_DONE : XROOT_STEP_ERROR;
}



/* 在根内创建符号链接。 */
XRT_API bool xrtRootLinkCreate(xroot Root, cstr sTarget,
	cstr sLink, bool bDirectory)
{
	__xrt_root_link_data Data;

	if ( (Root == NULL) || (sTarget == NULL) || (sTarget[0] == '\0') ||
		 (sLink == NULL) || (sLink[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Data.Target = sTarget;
	Data.Directory = bDirectory;
	return __xrtRootResolve(Root, sLink,
		__xrtRootLinkCreateProc, &Data);
}





/* 已解析硬链接目标的末级创建操作。 */
static xrootstep __xrtRootHardTargetProc(xrootnative Parent,
	cstr sName, bool bTrailing, ptr pData, str* pLink)
{
	const __xrt_root_hard_target* pSource =
		(const __xrt_root_hard_target*)pData;

	(void)pLink;
	if ( bTrailing || (strcmp(sName, ".") == 0) ) {
		__xrtRootError(XERR_ARGUMENT, XROOT_ERROR_LINK,
			"hard-link", "a hard-link path must name a new file");
		return XROOT_STEP_ERROR;
	}
	return __xrtRootNativeLinkHard(pSource->SourceParent,
		pSource->SourceName, Parent, sName) ?
		XROOT_STEP_DONE : XROOT_STEP_ERROR;
}



/* 解析硬链接源，确认它是普通文件后解析并创建目标。 */
static xrootstep __xrtRootHardSourceProc(xrootnative Parent,
	cstr sName, bool bTrailing, ptr pData, str* pLink)
{
	const __xrt_root_hard_source* pTarget =
		(const __xrt_root_hard_source*)pData;
	__xrt_root_hard_target Source;
	xfileinfo Info;
	xrootstep Step;

	if ( bTrailing ) {
		__xrtRootError(XERR_TYPE, XROOT_ERROR_LINK,
			"hard-link", "a hard-link source cannot end with a separator");
		return XROOT_STEP_ERROR;
	}
	Step = __xrtRootNativeStat(Parent, sName, true, &Info, pLink);
	if ( Step != XROOT_STEP_DONE ) { return Step; }
	if ( Info.Type != XFILE_TYPE_FILE ) {
		__xrtRootError(XERR_TYPE, XROOT_ERROR_LINK,
			"hard-link", "a hard-link source must be a regular file");
		return XROOT_STEP_ERROR;
	}
	Source.SourceParent = Parent;
	Source.SourceName = sName;
	return __xrtRootResolve(pTarget->Root, pTarget->TargetPath,
		__xrtRootHardTargetProc, &Source) ?
		XROOT_STEP_DONE : XROOT_STEP_ERROR;
}



/* 在同一根内创建硬链接。 */
XRT_API bool xrtRootLinkHard(xroot Root, cstr sExisting, cstr sLink)
{
	__xrt_root_hard_source Data;

	if ( (Root == NULL) || (sExisting == NULL) ||
		 (sExisting[0] == '\0') || (sLink == NULL) ||
		 (sLink[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Data.Root = Root;
	Data.TargetPath = sLink;
	return __xrtRootResolve(Root, sExisting,
		__xrtRootHardSourceProc, &Data);
}





/* 根内 FIFO 创建末级操作。 */
static xrootstep __xrtRootFifoProc(xrootnative Parent,
	cstr sName, bool bTrailing, ptr pData, str* pLink)
{
	uint32 iMode = *(const uint32*)pData;

	(void)pLink;
	if ( bTrailing || (strcmp(sName, ".") == 0) ) {
		__xrtRootError(XERR_ARGUMENT, XROOT_ERROR_CREATE,
			"create-fifo", "a FIFO path must name a new root-relative object");
		return XROOT_STEP_ERROR;
	}
	return __xrtRootNativeFifoCreate(Parent, sName, iMode) ?
		XROOT_STEP_DONE : XROOT_STEP_ERROR;
}



/* 在根内创建 FIFO。 */
XRT_API bool xrtRootFifoCreate(xroot Root, cstr sPath, uint32 iMode)
{
	if ( (Root == NULL) || (sPath == NULL) || (sPath[0] == '\0') ||
		 ((iMode & ~07777u) != 0u) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtRootResolve(Root, sPath,
		__xrtRootFifoProc, &iMode);
}





/* 根内权限设置末级操作。 */
static xrootstep __xrtRootSetModeProc(xrootnative Parent,
	cstr sName, bool bTrailing, ptr pData, str* pLink)
{
	const __xrt_root_mode_data* pMode =
		(const __xrt_root_mode_data*)pData;

	if ( bTrailing ) {
		xfileinfo Info;
		xrootstep Step = __xrtRootNativeStat(Parent,
			sName, pMode->Follow, &Info, pLink);

		if ( Step != XROOT_STEP_DONE ) { return Step; }
		if ( Info.Type != XFILE_TYPE_DIRECTORY ) {
			__xrtRootError(XERR_TYPE, XROOT_ERROR_STAT,
				"set-mode", "a path ending with a separator is not a directory");
			return XROOT_STEP_ERROR;
		}
	}
	return __xrtRootNativeSetMode(Parent, sName,
		pMode->Follow, pMode->Mode, pLink);
}



/* 在根内设置对象权限。 */
XRT_API bool xrtRootSetMode(xroot Root, cstr sPath,
	bool bFollowLink, uint32 iMode)
{
	__xrt_root_mode_data Data;

	if ( (Root == NULL) || (sPath == NULL) || (sPath[0] == '\0') ||
		 ((iMode & ~07777u) != 0u) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Data.Follow = bFollowLink;
	Data.Mode = iMode;
	return __xrtRootResolve(Root, sPath,
		__xrtRootSetModeProc, &Data);
}

#endif
