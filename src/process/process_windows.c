#include "../internal/xrt_process.h"

#include <wchar.h>



#if defined(XRT_FEATURE_PROCESS) && \
	(defined(_WIN32) || defined(_WIN64))

#if !defined(EXTENDED_STARTUPINFO_PRESENT)
	#define EXTENDED_STARTUPINFO_PRESENT 0x00080000u
#endif

#if !defined(PROC_THREAD_ATTRIBUTE_HANDLE_LIST)
	#define PROC_THREAD_ATTRIBUTE_HANDLE_LIST 0x00020002u
#endif

#if !defined(PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE)
	#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016u
#endif



/* TinyCC 和旧 SDK 可能没有 STARTUPINFOEXW 声明，布局仍由 Win32 ABI 固定。 */
typedef struct xprocessstartupinfoexw {
	STARTUPINFOW StartupInfo;
	void* AttributeList;
} xprocessstartupinfoexw;



/* 动态解析扩展启动函数，避免公共头依赖特定 Windows SDK 版本。 */
typedef BOOL (WINAPI *xprocessinitattributesproc)(
	void*,
	DWORD,
	DWORD,
	SIZE_T*
);

typedef BOOL (WINAPI *xprocessupdateattributeproc)(
	void*,
	DWORD,
	DWORD_PTR,
	PVOID,
	SIZE_T,
	PVOID,
	SIZE_T*
);

typedef VOID (WINAPI *xprocessdeleteattributesproc)(void*);



/* Process 核心只需要精确句柄列表的三项扩展启动入口。 */
typedef struct xprocesswinapi {
	xprocessinitattributesproc InitAttributes;
	xprocessupdateattributeproc UpdateAttribute;
	xprocessdeleteattributesproc DeleteAttributes;
} xprocesswinapi;



#if defined(XRT_FEATURE_PROCESS_TERMINAL)
/* ConPTY 入口按运行时解析，保持旧 SDK 与 TinyCC 可编译。 */
typedef HRESULT (WINAPI *xprocesscreatepseudoconsoleproc)(
	COORD,
	HANDLE,
	HANDLE,
	DWORD,
	HANDLE*
);

typedef void (WINAPI *xprocessclosepseudoconsoleproc)(HANDLE);
typedef HRESULT (WINAPI *xprocessresizepseudoconsoleproc)(HANDLE, COORD);



/* Terminal 启动复用扩展属性入口，并补充三项 ConPTY 操作。 */
typedef struct xprocessterminalapi {
	xprocesswinapi Startup;
	xprocesscreatepseudoconsoleproc Create;
	xprocessclosepseudoconsoleproc Close;
	xprocessresizepseudoconsoleproc Resize;
} xprocessterminalapi;
#endif



/* 命令行构造器只为当前 Spawn 动态增长，不形成 Process 常驻缓冲。 */
typedef struct xprocesscommandbuffer {
	char* Data;
	size_t Size;
	size_t Capacity;
} xprocesscommandbuffer;



/* Windows 环境项保存完整文本，排序前全部由 XRT 独立拥有。 */
typedef struct xprocesswinenventry {
	wchar_t* Text;
} xprocesswinenventry;



/* 通过 memcpy 转移 FARPROC，规避数据指针与函数指针直接转换告警。 */
static void __xrtProcessWinProcSet(
	void* pTarget,
	size_t iTargetSize,
	FARPROC pProc
)
{
	size_t iSize = iTargetSize < sizeof(pProc) ? iTargetSize : sizeof(pProc);

	memset(pTarget, 0, iTargetSize);
	memcpy(pTarget, &pProc, iSize);
}



/* 从 Kernel32 读取扩展启动函数。 */
static bool __xrtProcessWinApiLoad(xprocesswinapi* pApi)
{
	HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");

	memset(pApi, 0, sizeof(xprocesswinapi));
	if ( hKernel == NULL ) {
		return false;
	}
	__xrtProcessWinProcSet(
		&pApi->InitAttributes,
		sizeof(pApi->InitAttributes),
		GetProcAddress(hKernel, "InitializeProcThreadAttributeList")
	);
	__xrtProcessWinProcSet(
		&pApi->UpdateAttribute,
		sizeof(pApi->UpdateAttribute),
		GetProcAddress(hKernel, "UpdateProcThreadAttribute")
	);
	__xrtProcessWinProcSet(
		&pApi->DeleteAttributes,
		sizeof(pApi->DeleteAttributes),
		GetProcAddress(hKernel, "DeleteProcThreadAttributeList")
	);
	return (pApi->InitAttributes != NULL) &&
		(pApi->UpdateAttribute != NULL) &&
		(pApi->DeleteAttributes != NULL);
}



#if defined(XRT_FEATURE_PROCESS_TERMINAL)
/* 动态读取完整 ConPTY API。 */
static bool __xrtProcessTerminalApiLoad(xprocessterminalapi* pApi)
{
	HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");

	memset(pApi, 0, sizeof(xprocessterminalapi));
	if ( (hKernel == NULL) || !__xrtProcessWinApiLoad(&pApi->Startup) ) {
		return false;
	}
	__xrtProcessWinProcSet(
		&pApi->Create,
		sizeof(pApi->Create),
		GetProcAddress(hKernel, "CreatePseudoConsole")
	);
	__xrtProcessWinProcSet(
		&pApi->Close,
		sizeof(pApi->Close),
		GetProcAddress(hKernel, "ClosePseudoConsole")
	);
	__xrtProcessWinProcSet(
		&pApi->Resize,
		sizeof(pApi->Resize),
		GetProcAddress(hKernel, "ResizePseudoConsole")
	);
	return (pApi->Create != NULL) && (pApi->Close != NULL) &&
		(pApi->Resize != NULL);
}



/* Windows 只有完整解析三项 ConPTY 入口时才报告支持。 */
bool __xrtProcessTerminalSupportedPlatform(void)
{
	xprocessterminalapi Api;

	return __xrtProcessTerminalApiLoad(&Api);
}
#endif



/* 为命令行追加一段原始字节。 */
static bool __xrtProcessCommandAdd(
	xprocesscommandbuffer* pBuffer,
	const char* pData,
	size_t iSize
)
{
	size_t iNeed;
	size_t iCapacity;
	char* pNew;

	if ( iSize > (SIZE_MAX - pBuffer->Size - 1u) ) {
		__xrtProcessErrorSet(
			XERR_RANGE,
			XPROCESS_ERROR_LIMIT,
			"spawn.command",
			"process command line is too large",
			0
		);
		return false;
	}
	iNeed = pBuffer->Size + iSize + 1u;
	if ( iNeed > pBuffer->Capacity ) {
		iCapacity = pBuffer->Capacity != 0u ? pBuffer->Capacity : 128u;
		while ( iCapacity < iNeed ) {
			if ( iCapacity > (SIZE_MAX / 2u) ) {
				iCapacity = iNeed;
				break;
			}
			iCapacity *= 2u;
		}
		pNew = (char*)xrtRealloc(pBuffer->Data, iCapacity);
		if ( pNew == NULL ) {
			return false;
		}
		pBuffer->Data = pNew;
		pBuffer->Capacity = iCapacity;
	}
	if ( iSize != 0u ) {
		memcpy(pBuffer->Data + pBuffer->Size, pData, iSize);
	}
	pBuffer->Size += iSize;
	pBuffer->Data[pBuffer->Size] = 0;
	return true;
}



/* 为命令行追加一个字符。 */
static bool __xrtProcessCommandByte(
	xprocesscommandbuffer* pBuffer,
	char iByte
)
{
	return __xrtProcessCommandAdd(pBuffer, &iByte, 1u);
}



/* 判断 CRT 参数是否需要引号。 */
static bool __xrtProcessCommandNeedsQuote(cstr sArgument)
{
	const unsigned char* p = (const unsigned char*)sArgument;

	if ( (sArgument == NULL) || (sArgument[0] == 0) ) {
		return true;
	}
	while ( *p != 0u ) {
		if ( (*p == ' ') || (*p == '\t') || (*p == '\n') ||
			(*p == '\v') || (*p == '"') ) {
			return true;
		}
		p++;
	}
	return false;
}



/* 按 Microsoft CRT 规则转义反斜杠与双引号。 */
static bool __xrtProcessCommandArgument(
	xprocesscommandbuffer* pBuffer,
	cstr sArgument
)
{
	size_t i = 0u;
	size_t iSlash;

	if ( sArgument == NULL ) {
		sArgument = "";
	}
	if ( !__xrtProcessCommandNeedsQuote(sArgument) ) {
		return __xrtProcessCommandAdd(
			pBuffer,
			sArgument,
			strlen(sArgument)
		);
	}
	if ( !__xrtProcessCommandByte(pBuffer, '"') ) {
		return false;
	}
	while ( true ) {
		iSlash = 0u;
		while ( sArgument[i] == '\\' ) {
			iSlash++;
			i++;
		}
		if ( sArgument[i] == '"' ) {
			for ( size_t j = 0u; j < ((iSlash * 2u) + 1u); j++ ) {
				if ( !__xrtProcessCommandByte(pBuffer, '\\') ) {
					return false;
				}
			}
			if ( !__xrtProcessCommandByte(pBuffer, '"') ) {
				return false;
			}
			i++;
			continue;
		}
		if ( sArgument[i] == 0 ) {
			for ( size_t j = 0u; j < (iSlash * 2u); j++ ) {
				if ( !__xrtProcessCommandByte(pBuffer, '\\') ) {
					return false;
				}
			}
			return __xrtProcessCommandByte(pBuffer, '"');
		}
		for ( size_t j = 0u; j < iSlash; j++ ) {
			if ( !__xrtProcessCommandByte(pBuffer, '\\') ) {
				return false;
			}
		}
		if ( !__xrtProcessCommandByte(pBuffer, sArgument[i]) ) {
			return false;
		}
		i++;
	}
}



/* 构造可由 CreateProcessW 修改的 UTF-16 命令行。 */
static wchar_t* __xrtProcessCommandBuild(
	const xprocessconfig* pConfig,
	cstr sProgram
)
{
	xprocesscommandbuffer Buffer;
	uint16* pWide;
	cstr sArg0;

	memset(&Buffer, 0, sizeof(Buffer));
	sArg0 = (pConfig->Target == XPROCESS_EXEC) &&
		(pConfig->Arg0 != NULL) ? pConfig->Arg0 : sProgram;
	if ( !__xrtProcessCommandArgument(&Buffer, sArg0) ) {
		goto fail;
	}
	if ( pConfig->Target == XPROCESS_SHELL ) {
		static const cstr pShellArgs[] = { "/D", "/S", "/C" };

		for ( size_t i = 0u; i < 3u; i++ ) {
			if ( !__xrtProcessCommandByte(&Buffer, ' ') ||
				!__xrtProcessCommandArgument(&Buffer, pShellArgs[i]) ) {
				goto fail;
			}
		}
		/* cmd 的命令串不是 CRT argv，内部引号必须原样保留。 */
		if ( !__xrtProcessCommandAdd(&Buffer, " \"", 2u) ||
			!__xrtProcessCommandAdd(
				&Buffer,
				pConfig->Command,
				strlen(pConfig->Command)
			) ||
			!__xrtProcessCommandByte(&Buffer, '"') ) {
			goto fail;
		}
	} else {
		for ( size_t i = 0u; i < pConfig->ArgCount; i++ ) {
			if ( !__xrtProcessCommandByte(&Buffer, ' ') ||
				!__xrtProcessCommandArgument(&Buffer, pConfig->Args[i]) ) {
				goto fail;
			}
		}
	}
	pWide = xrtUtf8To16(Buffer.Data != NULL ? Buffer.Data : "", NULL);
	xrtFree(Buffer.Data);
	return (wchar_t*)pWide;

fail:
	xrtFree(Buffer.Data);
	return NULL;
}



/* 在父进程按 Windows 程序搜索规则解析明确的 ApplicationName。 */
static wchar_t* __xrtProcessProgramResolve(cstr sProgram)
{
	wchar_t* sInput = (wchar_t*)xrtUtf8To16(sProgram, NULL);
	wchar_t* sOutput = NULL;
	const wchar_t* pExtension = NULL;
	DWORD iCapacity = MAX_PATH;

	if ( sInput == NULL ) {
		return NULL;
	}
	for ( int iAttempt = 0; iAttempt < 2; iAttempt++ ) {
		while ( true ) {
			DWORD iLength;
			wchar_t* sNew = (wchar_t*)xrtRealloc(
				sOutput,
				(size_t)iCapacity * sizeof(wchar_t)
			);

			if ( sNew == NULL ) {
				goto cleanup;
			}
			sOutput = sNew;
			iLength = SearchPathW(
				NULL,
				sInput,
				pExtension,
				iCapacity,
				sOutput,
				NULL
			);
			if ( iLength == 0u ) {
				break;
			}
			if ( iLength < iCapacity ) {
				xrtFree(sInput);
				return sOutput;
			}
			if ( iLength == UINT32_MAX ) {
				goto cleanup;
			}
			iCapacity = iLength + 1u;
		}
		pExtension = L".exe";
	}

cleanup:
	xrtFree(sInput);
	xrtFree(sOutput);
	return NULL;
}



/* 返回 Windows 环境项名称长度，驱动变量包含开头等号。 */
static size_t __xrtProcessEnvNameSize(const wchar_t* sEntry)
{
	const wchar_t* pEquals;

	if ( sEntry[0] == L'=' ) {
		pEquals = wcschr(sEntry + 1, L'=');
	} else {
		pEquals = wcschr(sEntry, L'=');
	}
	return pEquals != NULL ? (size_t)(pEquals - sEntry) : wcslen(sEntry);
}



/* 按 Windows 环境名称规则执行不区分大小写比较。 */
static bool __xrtProcessEnvNameEqual(
	const wchar_t* sEntry,
	const wchar_t* sName
)
{
	size_t iEntry = __xrtProcessEnvNameSize(sEntry);
	size_t iName = wcslen(sName);

	return (iEntry == iName) &&
		(_wcsnicmp(sEntry, sName, iName) == 0);
}



/* 判断当前环境项是否被任意用户项覆盖或删除。 */
static bool __xrtProcessEnvOverridden(
	const wchar_t* sEntry,
	wchar_t* const* pNames,
	size_t iCount
)
{
	for ( size_t i = 0u; i < iCount; i++ ) {
		if ( __xrtProcessEnvNameEqual(sEntry, pNames[i]) ) {
			return true;
		}
	}
	return false;
}



/* 判断同名配置是否在数组后面再次出现，最终采用最后一项。 */
static bool __xrtProcessEnvHasLater(
	wchar_t* const* pNames,
	size_t iCount,
	size_t iIndex
)
{
	for ( size_t i = iIndex + 1u; i < iCount; i++ ) {
		if ( _wcsicmp(pNames[iIndex], pNames[i]) == 0 ) {
			return true;
		}
	}
	return false;
}



/* 为环境项数组追加一份完整副本。 */
static bool __xrtProcessEnvEntryAdd(
	xprocesswinenventry** ppEntries,
	size_t* pCount,
	size_t* pCapacity,
	const wchar_t* sText
)
{
	xprocesswinenventry* pNew;
	size_t iLength = wcslen(sText);
	wchar_t* sCopy;

	if ( *pCount == *pCapacity ) {
		size_t iCapacity = *pCapacity != 0u ? (*pCapacity * 2u) : 64u;

		if ( iCapacity < *pCapacity ) {
			return false;
		}
		pNew = (xprocesswinenventry*)xrtRealloc(
			*ppEntries,
			iCapacity * sizeof(xprocesswinenventry)
		);
		if ( pNew == NULL ) {
			return false;
		}
		*ppEntries = pNew;
		*pCapacity = iCapacity;
	}
	sCopy = (wchar_t*)xrtMalloc((iLength + 1u) * sizeof(wchar_t));
	if ( sCopy == NULL ) {
		return false;
	}
	memcpy(sCopy, sText, (iLength + 1u) * sizeof(wchar_t));
	(*ppEntries)[*pCount].Text = sCopy;
	(*pCount)++;
	return true;
}



/* 释放环境构造期间持有的字符串和数组。 */
static void __xrtProcessEnvEntriesFree(
	xprocesswinenventry* pEntries,
	size_t iCount
)
{
	for ( size_t i = 0u; i < iCount; i++ ) {
		xrtFree(pEntries[i].Text);
	}
	xrtFree(pEntries);
}



/* qsort 比较器遵循 Windows 环境块的不区分大小写排序。 */
static int __xrtProcessEnvCompare(const void* pLeft, const void* pRight)
{
	const xprocesswinenventry* pA = (const xprocesswinenventry*)pLeft;
	const xprocesswinenventry* pB = (const xprocesswinenventry*)pRight;

	return _wcsicmp(pA->Text, pB->Text);
}



/* 把 UTF-8 Name/Value 合成为一个 UTF-16 Name=Value 项。 */
static wchar_t* __xrtProcessEnvPair(
	const wchar_t* sName,
	cstr sValue8
)
{
	uint16* pValue16 = xrtUtf8To16(sValue8, NULL);
	wchar_t* sValue16 = (wchar_t*)pValue16;
	size_t iName;
	size_t iValue;
	wchar_t* sEntry;

	if ( sValue16 == NULL ) {
		return NULL;
	}
	iName = wcslen(sName);
	iValue = wcslen(sValue16);
	if ( iName > ((SIZE_MAX / sizeof(wchar_t)) - iValue - 2u) ) {
		xrtFree(sValue16);
		return NULL;
	}
	sEntry = (wchar_t*)xrtMalloc(
		(iName + iValue + 2u) * sizeof(wchar_t)
	);
	if ( sEntry != NULL ) {
		memcpy(sEntry, sName, iName * sizeof(wchar_t));
		sEntry[iName] = L'=';
		memcpy(
			sEntry + iName + 1u,
			sValue16,
			(iValue + 1u) * sizeof(wchar_t)
		);
	}
	xrtFree(sValue16);
	return sEntry;
}



/* 构建排序、双零结尾且支持覆盖和删除的 UTF-16 环境块。 */
static bool __xrtProcessEnvironmentBuild(
	const xprocessconfig* pConfig,
	wchar_t** ppBlock
)
{
	wchar_t** pNames = NULL;
	xprocesswinenventry* pEntries = NULL;
	size_t iEntryCount = 0u;
	size_t iEntryCapacity = 0u;
	LPWCH sParent = NULL;
	wchar_t* sBlock = NULL;
	size_t iBlockSize = 1u;
	size_t iOffset = 0u;
	bool bOk = false;

	*ppBlock = NULL;
	if ( pConfig->InheritEnv && (pConfig->EnvCount == 0u) ) {
		return true;
	}
	if ( pConfig->EnvCount != 0u ) {
		pNames = (wchar_t**)xrtCalloc(
			pConfig->EnvCount,
			sizeof(wchar_t*)
		);
		if ( pNames == NULL ) {
			goto cleanup;
		}
		for ( size_t i = 0u; i < pConfig->EnvCount; i++ ) {
			pNames[i] = (wchar_t*)xrtUtf8To16(
				pConfig->Env[i].Name,
				NULL
			);
			if ( pNames[i] == NULL ) {
				goto cleanup;
			}
		}
	}
	if ( pConfig->InheritEnv ) {
		sParent = GetEnvironmentStringsW();
		if ( sParent == NULL ) {
			__xrtProcessErrorSet(
				__xrtSystemErrorKind((int)GetLastError()),
				XPROCESS_ERROR_ENVIRONMENT,
				"spawn.environment",
				"parent environment could not be read",
				(int)GetLastError()
			);
			goto cleanup;
		}
		for ( const wchar_t* sItem = sParent; *sItem != 0;
			sItem += wcslen(sItem) + 1u ) {
			if ( __xrtProcessEnvOverridden(
				sItem,
				pNames,
				pConfig->EnvCount
			) ) {
				continue;
			}
			if ( !__xrtProcessEnvEntryAdd(
				&pEntries,
				&iEntryCount,
				&iEntryCapacity,
				sItem
			) ) {
				goto cleanup;
			}
		}
	}
	for ( size_t i = 0u; i < pConfig->EnvCount; i++ ) {
		wchar_t* sEntry;

		if ( (pConfig->Env[i].Value == NULL) ||
			__xrtProcessEnvHasLater(pNames, pConfig->EnvCount, i) ) {
			continue;
		}
		sEntry = __xrtProcessEnvPair(
			pNames[i],
			pConfig->Env[i].Value
		);
		if ( sEntry == NULL ) {
			goto cleanup;
		}
		if ( !__xrtProcessEnvEntryAdd(
			&pEntries,
			&iEntryCount,
			&iEntryCapacity,
			sEntry
		) ) {
			xrtFree(sEntry);
			goto cleanup;
		}
		xrtFree(sEntry);
	}
	qsort(
		pEntries,
		iEntryCount,
		sizeof(xprocesswinenventry),
		__xrtProcessEnvCompare
	);
	for ( size_t i = 0u; i < iEntryCount; i++ ) {
		size_t iSize = wcslen(pEntries[i].Text) + 1u;

		if ( iSize > (SIZE_MAX - iBlockSize) ) {
			goto cleanup;
		}
		iBlockSize += iSize;
	}
	if ( iEntryCount == 0u ) {
		iBlockSize = 2u;
	}
	sBlock = (wchar_t*)xrtCalloc(iBlockSize, sizeof(wchar_t));
	if ( sBlock == NULL ) {
		goto cleanup;
	}
	for ( size_t i = 0u; i < iEntryCount; i++ ) {
		size_t iSize = wcslen(pEntries[i].Text) + 1u;

		memcpy(
			sBlock + iOffset,
			pEntries[i].Text,
			iSize * sizeof(wchar_t)
		);
		iOffset += iSize;
	}
	*ppBlock = sBlock;
	sBlock = NULL;
	bOk = true;

cleanup:
	if ( sParent != NULL ) {
		(void)FreeEnvironmentStringsW(sParent);
	}
	for ( size_t i = 0u; i < pConfig->EnvCount; i++ ) {
		xrtFree(pNames != NULL ? pNames[i] : NULL);
	}
	xrtFree(pNames);
	__xrtProcessEnvEntriesFree(pEntries, iEntryCount);
	xrtFree(sBlock);
	if ( !bOk && (xrtGetError() == NULL) ) {
		__xrtProcessErrorSet(
			XERR_MEMORY,
			XPROCESS_ERROR_ENVIRONMENT,
			"spawn.environment",
			"process environment could not be built",
			0
		);
	}
	return bOk;
}



/* 打开可继承的 NUL 句柄。 */
static HANDLE __xrtProcessNullHandle(DWORD iAccess)
{
	SECURITY_ATTRIBUTES Security;

	memset(&Security, 0, sizeof(Security));
	Security.nLength = sizeof(Security);
	Security.bInheritHandle = TRUE;
	return CreateFileW(
		L"NUL",
		iAccess,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		&Security,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);
}



/* 复制一个借用句柄并显式打开子进程继承位。 */
static HANDLE __xrtProcessHandleDuplicate(HANDLE hSource)
{
	HANDLE hCopy = INVALID_HANDLE_VALUE;

	if ( (hSource == NULL) || (hSource == INVALID_HANDLE_VALUE) ) {
		return INVALID_HANDLE_VALUE;
	}
	if ( !DuplicateHandle(
		GetCurrentProcess(),
		hSource,
		GetCurrentProcess(),
		&hCopy,
		0u,
		TRUE,
		DUPLICATE_SAME_ACCESS
	) ) {
		return INVALID_HANDLE_VALUE;
	}
	return hCopy;
}



/* 为一个标准流创建子端句柄和可选父端管道。 */
static bool __xrtProcessStdioPrepare(
	xprocessstream Stream,
	xprocessio Io,
	HANDLE* pChild,
	HANDLE* pParent
)
{
	SECURITY_ATTRIBUTES Security;
	HANDLE hRead = INVALID_HANDLE_VALUE;
	HANDLE hWrite = INVALID_HANDLE_VALUE;
	DWORD iStd;
	DWORD iNullAccess;

	*pChild = INVALID_HANDLE_VALUE;
	*pParent = INVALID_HANDLE_VALUE;
	memset(&Security, 0, sizeof(Security));
	Security.nLength = sizeof(Security);
	Security.bInheritHandle = TRUE;
	if ( Io.Mode == XPROCESS_IO_PIPE ) {
		if ( !CreatePipe(&hRead, &hWrite, &Security, 0u) ) {
			return false;
		}
		if ( Stream == XPROCESS_STDIN ) {
			*pChild = hRead;
			*pParent = hWrite;
		} else {
			*pChild = hWrite;
			*pParent = hRead;
		}
		if ( !SetHandleInformation(*pParent, HANDLE_FLAG_INHERIT, 0u) ) {
			(void)CloseHandle(*pChild);
			(void)CloseHandle(*pParent);
			*pChild = INVALID_HANDLE_VALUE;
			*pParent = INVALID_HANDLE_VALUE;
			return false;
		}
		return true;
	}
	if ( Io.Mode == XPROCESS_IO_NULL ) {
		iNullAccess = Stream == XPROCESS_STDIN ? GENERIC_READ : GENERIC_WRITE;
		*pChild = __xrtProcessNullHandle(iNullAccess);
		return (*pChild != NULL) && (*pChild != INVALID_HANDLE_VALUE);
	}
	if ( Io.Mode == XPROCESS_IO_HANDLE ) {
		*pChild = __xrtProcessHandleDuplicate((HANDLE)Io.Handle);
		return *pChild != INVALID_HANDLE_VALUE;
	}
	iStd = Stream == XPROCESS_STDIN ? STD_INPUT_HANDLE :
		(Stream == XPROCESS_STDOUT ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);
	*pChild = __xrtProcessHandleDuplicate(GetStdHandle(iStd));
	if ( *pChild != INVALID_HANDLE_VALUE ) {
		return true;
	}
	SetLastError(ERROR_SUCCESS);
	iNullAccess = Stream == XPROCESS_STDIN ? GENERIC_READ : GENERIC_WRITE;
	*pChild = __xrtProcessNullHandle(iNullAccess);
	return (*pChild != NULL) && (*pChild != INVALID_HANDLE_VALUE);
}



/* 向精确继承列表追加一个尚未出现的子端句柄。 */
static void __xrtProcessHandleListAdd(
	HANDLE pHandles[3],
	size_t* pCount,
	HANDLE hHandle
)
{
	for ( size_t i = 0u; i < *pCount; i++ ) {
		if ( pHandles[i] == hHandle ) {
			return;
		}
	}
	pHandles[*pCount] = hHandle;
	(*pCount)++;
}



/* 关闭有效 Windows 句柄并把槽恢复为无效值。 */
static void __xrtProcessHandleClose(HANDLE* pHandle)
{
	if ( (*pHandle != NULL) && (*pHandle != INVALID_HANDLE_VALUE) ) {
		(void)CloseHandle(*pHandle);
	}
	*pHandle = INVALID_HANDLE_VALUE;
}



/* 创建无 KILL_ON_CLOSE 副作用的 Job，用于显式 KillTree。 */
static HANDLE __xrtProcessJobCreate(void)
{
	return CreateJobObjectW(NULL, NULL);
}



#if defined(XRT_FEATURE_PROCESS_TERMINAL)
/* 关闭 Process 持有的 ConPTY，并使输出管道进入 EOF。 */
static void __xrtProcessTerminalClose(xprocess* pProcess)
{
	xprocessterminalapi Api;
	HANDLE hTerminal;

	(void)xrtMutexLock(&pProcess->Lock);
	hTerminal = pProcess->TerminalHandle;
	pProcess->TerminalHandle = NULL;
	(void)xrtMutexUnlock(&pProcess->Lock);
	if ( (hTerminal != NULL) && __xrtProcessTerminalApiLoad(&Api) ) {
		Api.Close(hTerminal);
	}
}



/* 调整仍然打开的 Windows 伪控制台尺寸。 */
bool __xrtProcessTerminalResizePlatform(
	xprocess* pProcess,
	uint32 iColumns,
	uint32 iRows
)
{
	xprocessterminalapi Api;
	COORD Size;
	HRESULT Result;

	if ( !__xrtProcessTerminalApiLoad(&Api) ) {
		__xrtProcessErrorSet(
			XERR_UNSUPPORTED,
			XPROCESS_ERROR_TERMINAL,
			"terminal.resize",
			"ConPTY is unavailable",
			ERROR_NOT_SUPPORTED
		);
		return false;
	}
	Size.X = (SHORT)iColumns;
	Size.Y = (SHORT)iRows;
	(void)xrtMutexLock(&pProcess->Lock);
	if ( pProcess->TerminalHandle == NULL ) {
		(void)xrtMutexUnlock(&pProcess->Lock);
		__xrtProcessErrorSet(
			XERR_CLOSED,
			XPROCESS_ERROR_TERMINAL,
			"terminal.resize",
			"process terminal is closed",
			0
		);
		return false;
	}
	Result = Api.Resize(pProcess->TerminalHandle, Size);
	(void)xrtMutexUnlock(&pProcess->Lock);
	if ( SUCCEEDED(Result) ) {
		return true;
	}
	__xrtProcessErrorSet(
		XERR_IO,
		XPROCESS_ERROR_TERMINAL,
		"terminal.resize",
		"ConPTY could not be resized",
		(int)Result
	);
	return false;
}



/* 使用当前命令、环境和 Job 逻辑启动 ConPTY 进程。 */
bool __xrtProcessTerminalSpawnWindows(
	xprocess* pProcess,
	const xprocessconfig* pConfig
)
{
	xprocessterminalapi Api;
	xprocessstartupinfoexw Startup;
	PROCESS_INFORMATION Info;
	HANDLE hInputRead = INVALID_HANDLE_VALUE;
	HANDLE hInputWrite = INVALID_HANDLE_VALUE;
	HANDLE hOutputRead = INVALID_HANDLE_VALUE;
	HANDLE hOutputWrite = INVALID_HANDLE_VALUE;
	HANDLE hTerminal = NULL;
	HANDLE hJob = NULL;
	SIZE_T iAttributeSize = 0u;
	void* pAttributes = NULL;
	wchar_t* sProgram = NULL;
	wchar_t* sCommand = NULL;
	wchar_t* sWorkDir = NULL;
	wchar_t* sEnvironment = NULL;
	DWORD iFlags = EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT;
	cstr sProgram8 = pConfig->Target == XPROCESS_SHELL ?
		"cmd.exe" : pConfig->Program;
	COORD Size;
	HRESULT Result;
	bool bCreated = false;
	bool bAttributesReady = false;
	bool bOk = false;
	int iError = 0;

	memset(&Startup, 0, sizeof(Startup));
	memset(&Info, 0, sizeof(Info));
	Startup.StartupInfo.cb = sizeof(Startup);
	Startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
	if ( !__xrtProcessTerminalApiLoad(&Api) ) {
		__xrtProcessErrorSet(
			XERR_UNSUPPORTED,
			XPROCESS_ERROR_TERMINAL,
			"spawn.terminal",
			"ConPTY is unavailable",
			ERROR_NOT_SUPPORTED
		);
		goto cleanup;
	}
	if ( !CreatePipe(&hInputRead, &hInputWrite, NULL, 0u) ||
		!CreatePipe(&hOutputRead, &hOutputWrite, NULL, 0u) ) {
		iError = (int)GetLastError();
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_PIPE,
			"spawn.terminal.pipe",
			"ConPTY pipes could not be created",
			iError
		);
		goto cleanup;
	}
	Size.X = (SHORT)pConfig->Columns;
	Size.Y = (SHORT)pConfig->Rows;
	Result = Api.Create(Size, hInputRead, hOutputWrite, 0u, &hTerminal);
	if ( FAILED(Result) ) {
		__xrtProcessErrorSet(
			XERR_IO,
			XPROCESS_ERROR_TERMINAL,
			"spawn.terminal",
			"ConPTY could not be created",
			(int)Result
		);
		goto cleanup;
	}
	(void)Api.Startup.InitAttributes(NULL, 1u, 0u, &iAttributeSize);
	if ( iAttributeSize == 0u ) {
		iError = (int)GetLastError();
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_TERMINAL,
			"spawn.terminal.attributes",
			"ConPTY attribute size is unavailable",
			iError
		);
		goto cleanup;
	}
	pAttributes = xrtMalloc(iAttributeSize);
	if ( pAttributes == NULL ) {
		goto cleanup;
	}
	if ( !Api.Startup.InitAttributes(
		pAttributes,
		1u,
		0u,
		&iAttributeSize
	) ) {
		iError = (int)GetLastError();
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_TERMINAL,
			"spawn.terminal.attributes",
			"ConPTY attribute list could not be initialized",
			iError
		);
		goto cleanup;
	}
	bAttributesReady = true;
	if ( !Api.Startup.UpdateAttribute(
		pAttributes,
		0u,
		(DWORD_PTR)PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
		hTerminal,
		sizeof(hTerminal),
		NULL,
		NULL
	) ) {
		iError = (int)GetLastError();
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_TERMINAL,
			"spawn.terminal.attributes",
			"ConPTY attribute could not be installed",
			iError
		);
		goto cleanup;
	}
	Startup.AttributeList = pAttributes;
	sProgram = __xrtProcessProgramResolve(sProgram8);
	sCommand = __xrtProcessCommandBuild(pConfig, sProgram8);
	if ( (sProgram == NULL) || (sCommand == NULL) ) {
		iError = (int)GetLastError();
		__xrtProcessErrorSet(
			sProgram == NULL ? XERR_NOT_FOUND : XERR_VALUE,
			XPROCESS_ERROR_COMMAND,
			"spawn.command",
			sProgram == NULL ?
				"process program could not be resolved" :
				"process command is not valid UTF-8",
			iError
		);
		goto cleanup;
	}
	if ( pConfig->WorkDir != NULL ) {
		sWorkDir = (wchar_t*)xrtUtf8To16(pConfig->WorkDir, NULL);
		if ( sWorkDir == NULL ) {
			__xrtProcessErrorSet(
				XERR_VALUE,
				XPROCESS_ERROR_CONFIG,
				"spawn.workdir",
				"process working directory is not valid UTF-8",
				0
			);
			goto cleanup;
		}
	}
	if ( !__xrtProcessEnvironmentBuild(pConfig, &sEnvironment) ) {
		goto cleanup;
	}
	if ( pConfig->NewGroup ) {
		hJob = __xrtProcessJobCreate();
		if ( hJob == NULL ) {
			iError = (int)GetLastError();
			__xrtProcessErrorSet(
				__xrtSystemErrorKind(iError),
				XPROCESS_ERROR_SPAWN,
				"spawn.group",
				"process Job could not be created",
				iError
			);
			goto cleanup;
		}
		iFlags |= CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED;
	}
	if ( !CreateProcessW(
		sProgram,
		sCommand,
		NULL,
		NULL,
		FALSE,
		iFlags,
		sEnvironment,
		sWorkDir,
		&Startup.StartupInfo,
		&Info
	) ) {
		iError = (int)GetLastError();
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_SPAWN,
			"spawn.exec",
			"terminal process could not be started",
			iError
		);
		goto cleanup;
	}
	bCreated = true;
	__xrtProcessHandleClose(&hInputRead);
	__xrtProcessHandleClose(&hOutputWrite);
	if ( pConfig->NewGroup ) {
		if ( !AssignProcessToJobObject(hJob, Info.hProcess) ) {
			iError = (int)GetLastError();
			__xrtProcessErrorSet(
				__xrtSystemErrorKind(iError),
				XPROCESS_ERROR_SPAWN,
				"spawn.group",
				"terminal process could not enter its Job",
				iError
			);
			goto cleanup;
		}
		if ( ResumeThread(Info.hThread) == (DWORD)-1 ) {
			iError = (int)GetLastError();
			__xrtProcessErrorSet(
				__xrtSystemErrorKind(iError),
				XPROCESS_ERROR_SPAWN,
				"spawn.resume",
				"terminal process main thread could not resume",
				iError
			);
			goto cleanup;
		}
	}
	pProcess->Process = Info.hProcess;
	pProcess->Job = hJob;
	pProcess->TerminalHandle = hTerminal;
	pProcess->Id = Info.dwProcessId;
	pProcess->Stdin = hInputWrite;
	pProcess->Stdout = hOutputRead;
	pProcess->Stderr = INVALID_HANDLE_VALUE;
	Info.hProcess = NULL;
	hJob = NULL;
	hTerminal = NULL;
	hInputWrite = INVALID_HANDLE_VALUE;
	hOutputRead = INVALID_HANDLE_VALUE;
	bOk = true;

cleanup:
	if ( bCreated && !bOk ) {
		if ( hJob != NULL ) {
			(void)TerminateJobObject(hJob, 1u);
		} else if ( Info.hProcess != NULL ) {
			(void)TerminateProcess(Info.hProcess, 1u);
		}
		if ( Info.hProcess != NULL ) {
			(void)WaitForSingleObject(Info.hProcess, INFINITE);
		}
	}
	if ( bAttributesReady ) {
		Api.Startup.DeleteAttributes(pAttributes);
	}
	xrtFree(pAttributes);
	if ( hTerminal != NULL ) {
		Api.Close(hTerminal);
	}
	__xrtProcessHandleClose(&hInputRead);
	__xrtProcessHandleClose(&hInputWrite);
	__xrtProcessHandleClose(&hOutputRead);
	__xrtProcessHandleClose(&hOutputWrite);
	if ( Info.hThread != NULL ) {
		(void)CloseHandle(Info.hThread);
	}
	if ( Info.hProcess != NULL ) {
		(void)CloseHandle(Info.hProcess);
	}
	if ( hJob != NULL ) {
		(void)CloseHandle(hJob);
	}
	xrtFree(sProgram);
	xrtFree(sCommand);
	xrtFree(sWorkDir);
	xrtFree(sEnvironment);
	return bOk;
}
#endif



/* 用精确句柄列表启动普通管道进程。 */
bool __xrtProcessPlatformSpawn(
	xprocess* pProcess,
	const xprocessconfig* pConfig
)
{
	xprocesswinapi Api;
	xprocessstartupinfoexw Startup;
	PROCESS_INFORMATION Info;
	HANDLE hChildIn = INVALID_HANDLE_VALUE;
	HANDLE hChildOut = INVALID_HANDLE_VALUE;
	HANDLE hChildErr = INVALID_HANDLE_VALUE;
	HANDLE hParentIn = INVALID_HANDLE_VALUE;
	HANDLE hParentOut = INVALID_HANDLE_VALUE;
	HANDLE hParentErr = INVALID_HANDLE_VALUE;
	HANDLE pHandles[3];
	size_t iHandleCount = 0u;
	SIZE_T iAttributeSize = 0u;
	void* pAttributes = NULL;
	wchar_t* sProgram = NULL;
	wchar_t* sCommand = NULL;
	wchar_t* sWorkDir = NULL;
	wchar_t* sEnvironment = NULL;
	HANDLE hJob = NULL;
	DWORD iFlags = EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT;
	bool bCreated = false;
	bool bOk = false;
	int iError = 0;
	cstr sProgram8 = pConfig->Target == XPROCESS_SHELL ? "cmd.exe" : pConfig->Program;

	#if defined(XRT_FEATURE_PROCESS_TERMINAL)
		if ( pConfig->Terminal ) {
			return __xrtProcessTerminalSpawnWindows(pProcess, pConfig);
		}
	#endif
	memset(&Startup, 0, sizeof(Startup));
	memset(&Info, 0, sizeof(Info));
	Startup.StartupInfo.cb = sizeof(Startup);
	Startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
	if ( !__xrtProcessWinApiLoad(&Api) ) {
		__xrtProcessErrorSet(
			XERR_UNSUPPORTED,
			XPROCESS_ERROR_SPAWN,
			"spawn.handles",
			"precise Windows handle inheritance is unavailable",
			(int)GetLastError()
		);
		goto cleanup;
	}
	if ( !__xrtProcessStdioPrepare(
		XPROCESS_STDIN,
		pConfig->Stdin,
		&hChildIn,
		&hParentIn
	) ) {
		iError = (int)GetLastError();
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_PIPE,
			"spawn.stdin",
			"process stdin could not be prepared",
			iError
		);
		goto cleanup;
	}
	if ( !__xrtProcessStdioPrepare(
		XPROCESS_STDOUT,
		pConfig->Stdout,
		&hChildOut,
		&hParentOut
	) ) {
		iError = (int)GetLastError();
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_PIPE,
			"spawn.stdout",
			"process stdout could not be prepared",
			iError
		);
		goto cleanup;
	}
	if ( pConfig->Stderr.Mode == XPROCESS_IO_MERGE ) {
		hChildErr = hChildOut;
	} else if ( !__xrtProcessStdioPrepare(
		XPROCESS_STDERR,
		pConfig->Stderr,
		&hChildErr,
		&hParentErr
	) ) {
		iError = (int)GetLastError();
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_PIPE,
			"spawn.stderr",
			"process stderr could not be prepared",
			iError
		);
		goto cleanup;
	}
	Startup.StartupInfo.hStdInput = hChildIn;
	Startup.StartupInfo.hStdOutput = hChildOut;
	Startup.StartupInfo.hStdError = hChildErr;
	__xrtProcessHandleListAdd(pHandles, &iHandleCount, hChildIn);
	__xrtProcessHandleListAdd(pHandles, &iHandleCount, hChildOut);
	__xrtProcessHandleListAdd(pHandles, &iHandleCount, hChildErr);
	(void)Api.InitAttributes(NULL, 1u, 0u, &iAttributeSize);
	if ( iAttributeSize == 0u ) {
		iError = (int)GetLastError();
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_SPAWN,
			"spawn.handles",
			"Windows handle attribute size is unavailable",
			iError
		);
		goto cleanup;
	}
	pAttributes = xrtMalloc(iAttributeSize);
	if ( pAttributes == NULL ) {
		goto cleanup;
	}
	if ( !Api.InitAttributes(pAttributes, 1u, 0u, &iAttributeSize) ||
		!Api.UpdateAttribute(
			pAttributes,
			0u,
			(DWORD_PTR)PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
			pHandles,
			iHandleCount * sizeof(HANDLE),
			NULL,
			NULL
		) ) {
		iError = (int)GetLastError();
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_SPAWN,
			"spawn.handles",
			"Windows handle inheritance list could not be built",
			iError
		);
		goto cleanup;
	}
	Startup.AttributeList = pAttributes;
	sProgram = __xrtProcessProgramResolve(sProgram8);
	sCommand = __xrtProcessCommandBuild(pConfig, sProgram8);
	if ( (sProgram == NULL) || (sCommand == NULL) ) {
		iError = (int)GetLastError();
		__xrtProcessErrorSet(
			sProgram == NULL ? XERR_NOT_FOUND : XERR_VALUE,
			XPROCESS_ERROR_COMMAND,
			"spawn.command",
			sProgram == NULL ?
				"process program could not be resolved" :
				"process command is not valid UTF-8",
			iError
		);
		goto cleanup;
	}
	if ( pConfig->WorkDir != NULL ) {
		sWorkDir = (wchar_t*)xrtUtf8To16(pConfig->WorkDir, NULL);
		if ( sWorkDir == NULL ) {
			__xrtProcessErrorSet(
				XERR_VALUE,
				XPROCESS_ERROR_CONFIG,
				"spawn.workdir",
				"process working directory is not valid UTF-8",
				0
			);
			goto cleanup;
		}
	}
	if ( !__xrtProcessEnvironmentBuild(pConfig, &sEnvironment) ) {
		goto cleanup;
	}
	if ( pConfig->NewGroup ) {
		hJob = __xrtProcessJobCreate();
		if ( hJob == NULL ) {
			iError = (int)GetLastError();
			__xrtProcessErrorSet(
				__xrtSystemErrorKind(iError),
				XPROCESS_ERROR_SPAWN,
				"spawn.group",
				"process Job could not be created",
				iError
			);
			goto cleanup;
		}
		iFlags |= CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED;
	}
	if ( pConfig->HideWindow ) {
		Startup.StartupInfo.dwFlags |= STARTF_USESHOWWINDOW;
		Startup.StartupInfo.wShowWindow = SW_HIDE;
		iFlags |= CREATE_NO_WINDOW;
	} else if ( pConfig->NewConsole ) {
		iFlags |= CREATE_NEW_CONSOLE;
	}
	if ( !CreateProcessW(
		sProgram,
		sCommand,
		NULL,
		NULL,
		TRUE,
		iFlags,
		sEnvironment,
		sWorkDir,
		&Startup.StartupInfo,
		&Info
	) ) {
		iError = (int)GetLastError();
		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_SPAWN,
			"spawn.exec",
			"process could not be started",
			iError
		);
		goto cleanup;
	}
	bCreated = true;
	if ( pConfig->NewGroup ) {
		if ( !AssignProcessToJobObject(hJob, Info.hProcess) ) {
			iError = (int)GetLastError();
			__xrtProcessErrorSet(
				__xrtSystemErrorKind(iError),
				XPROCESS_ERROR_SPAWN,
				"spawn.group",
				"process could not enter its Job",
				iError
			);
			goto cleanup;
		}
		if ( ResumeThread(Info.hThread) == (DWORD)-1 ) {
			iError = (int)GetLastError();
			__xrtProcessErrorSet(
				__xrtSystemErrorKind(iError),
				XPROCESS_ERROR_SPAWN,
				"spawn.resume",
				"process main thread could not resume",
				iError
			);
			goto cleanup;
		}
	}
	pProcess->Process = Info.hProcess;
	pProcess->Job = hJob;
	pProcess->Id = Info.dwProcessId;
	pProcess->Stdin = hParentIn;
	pProcess->Stdout = hParentOut;
	pProcess->Stderr = hParentErr;
	Info.hProcess = NULL;
	hJob = NULL;
	hParentIn = INVALID_HANDLE_VALUE;
	hParentOut = INVALID_HANDLE_VALUE;
	hParentErr = INVALID_HANDLE_VALUE;
	bOk = true;

cleanup:
	if ( bCreated && !bOk ) {
		if ( hJob != NULL ) {
			(void)TerminateJobObject(hJob, 1u);
		} else if ( Info.hProcess != NULL ) {
			(void)TerminateProcess(Info.hProcess, 1u);
		}
		if ( Info.hProcess != NULL ) {
			(void)WaitForSingleObject(Info.hProcess, INFINITE);
		}
	}
	if ( pAttributes != NULL ) {
		Api.DeleteAttributes(pAttributes);
	}
	xrtFree(pAttributes);
	if ( Info.hThread != NULL ) {
		(void)CloseHandle(Info.hThread);
	}
	if ( Info.hProcess != NULL ) {
		(void)CloseHandle(Info.hProcess);
	}
	if ( hJob != NULL ) {
		(void)CloseHandle(hJob);
	}
	if ( hChildErr == hChildOut ) {
		hChildErr = INVALID_HANDLE_VALUE;
	}
	__xrtProcessHandleClose(&hChildIn);
	__xrtProcessHandleClose(&hChildOut);
	__xrtProcessHandleClose(&hChildErr);
	__xrtProcessHandleClose(&hParentIn);
	__xrtProcessHandleClose(&hParentOut);
	__xrtProcessHandleClose(&hParentErr);
	xrtFree(sProgram);
	xrtFree(sCommand);
	xrtFree(sWorkDir);
	xrtFree(sEnvironment);
	return bOk;
}



/* 等待 Windows 进程并读取稳定退出码。 */
bool __xrtProcessPlatformWait(
	xprocess* pProcess,
	xprocessstatus* pStatus
)
{
	DWORD iCode;
	DWORD iWait = WaitForSingleObject(pProcess->Process, INFINITE);

	if ( iWait != WAIT_OBJECT_0 ) {
		int iError = (int)GetLastError();

		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_WAIT,
			"wait",
			"process wait failed",
			iError
		);
		return false;
	}
	if ( !GetExitCodeProcess(pProcess->Process, &iCode) ) {
		int iError = (int)GetLastError();

		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_WAIT,
			"wait.status",
			"process exit code could not be read",
			iError
		);
		return false;
	}
	pStatus->Kind = XPROCESS_EXIT_CODE;
	pStatus->Code = (int32)iCode;
	#if defined(XRT_FEATURE_PROCESS_TERMINAL)
		if ( pProcess->Terminal ) {
			__xrtProcessTerminalClose(pProcess);
		}
	#endif
	return true;
}



/* 读取父端 stdout 或 stderr。 */
int64 __xrtProcessPlatformRead(
	xprocess* pProcess,
	xprocessstream Stream,
	void* pData,
	size_t iSize
)
{
	HANDLE hHandle;
	DWORD iRead = 0u;
	DWORD iChunk = iSize > UINT32_MAX ? UINT32_MAX : (DWORD)iSize;

	(void)xrtMutexLock(&pProcess->Lock);
	hHandle = Stream == XPROCESS_STDOUT ? pProcess->Stdout : pProcess->Stderr;
	(void)xrtMutexUnlock(&pProcess->Lock);
	if ( (hHandle == NULL) || (hHandle == INVALID_HANDLE_VALUE) ) {
		__xrtProcessErrorSet(
			XERR_CLOSED,
			XPROCESS_ERROR_READ,
			"read",
			"process output pipe is closed",
			0
		);
		return -1;
	}
	if ( ReadFile(hHandle, pData, iChunk, &iRead, NULL) ) {
		return (int64)iRead;
	}
	if ( (GetLastError() == ERROR_BROKEN_PIPE) ||
		(GetLastError() == ERROR_HANDLE_EOF) ) {
		return 0;
	}
	{
		int iError = (int)GetLastError();

		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_READ,
			"read",
			"process output read failed",
			iError
		);
	}
	return -1;
}



/* 写入父端 stdin，允许 Windows 平台短写。 */
int64 __xrtProcessPlatformWrite(
	xprocess* pProcess,
	const void* pData,
	size_t iSize
)
{
	HANDLE hHandle;
	DWORD iWritten = 0u;
	DWORD iChunk = iSize > UINT32_MAX ? UINT32_MAX : (DWORD)iSize;

	(void)xrtMutexLock(&pProcess->Lock);
	hHandle = pProcess->Stdin;
	(void)xrtMutexUnlock(&pProcess->Lock);
	if ( (hHandle == NULL) || (hHandle == INVALID_HANDLE_VALUE) ) {
		__xrtProcessErrorSet(
			XERR_CLOSED,
			XPROCESS_ERROR_WRITE,
			"write",
			"process input pipe is closed",
			0
		);
		return -1;
	}
	if ( WriteFile(hHandle, pData, iChunk, &iWritten, NULL) ) {
		return (int64)iWritten;
	}
	{
		int iError = (int)GetLastError();

		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_WRITE,
			"write",
			"process input write failed",
			iError
		);
	}
	return -1;
}



/* 在 Process 锁下取走一个句柄，再在锁外关闭。 */
bool __xrtProcessPlatformClose(
	xprocess* pProcess,
	xprocessstream Stream
)
{
	HANDLE* pSlot;
	HANDLE hHandle;

	if ( Stream == XPROCESS_STDIN ) {
		pSlot = &pProcess->Stdin;
	} else if ( Stream == XPROCESS_STDOUT ) {
		pSlot = &pProcess->Stdout;
	} else {
		pSlot = &pProcess->Stderr;
	}
	(void)xrtMutexLock(&pProcess->Lock);
	hHandle = *pSlot;
	*pSlot = INVALID_HANDLE_VALUE;
	(void)xrtMutexUnlock(&pProcess->Lock);
	if ( (hHandle == NULL) || (hHandle == INVALID_HANDLE_VALUE) ) {
		return true;
	}
	if ( CloseHandle(hHandle) ) {
		return true;
	}
	{
		int iError = (int)GetLastError();

		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_CLOSE,
			"close",
			"process pipe could not be closed",
			iError
		);
	}
	return false;
}



/* 释放全部 Windows 句柄；调用时对象已经没有并发访问者。 */
void __xrtProcessPlatformUnit(xprocess* pProcess)
{
	#if defined(XRT_FEATURE_PROCESS_TERMINAL)
		__xrtProcessTerminalClose(pProcess);
	#endif
	__xrtProcessHandleClose(&pProcess->Stdin);
	__xrtProcessHandleClose(&pProcess->Stdout);
	__xrtProcessHandleClose(&pProcess->Stderr);
	if ( pProcess->Process != NULL ) {
		(void)CloseHandle(pProcess->Process);
		pProcess->Process = NULL;
	}
	if ( pProcess->Job != NULL ) {
		(void)CloseHandle(pProcess->Job);
		pProcess->Job = NULL;
	}
}



/* 返回 Windows 进程 ID。 */
uint64 __xrtProcessPlatformId(const xprocess* pProcess)
{
	return (uint64)pProcess->Id;
}



/* 返回借用的 Windows Process HANDLE。 */
intptr_t __xrtProcessPlatformNative(const xprocess* pProcess)
{
	return (intptr_t)pProcess->Process;
}



/* 返回借用的 Windows 管道 HANDLE。 */
intptr_t __xrtProcessPlatformStream(
	const xprocess* pProcess,
	xprocessstream Stream
)
{
	HANDLE hHandle;

	(void)xrtMutexLock((xmutex*)&pProcess->Lock);
	if ( Stream == XPROCESS_STDIN ) {
		hHandle = pProcess->Stdin;
	} else if ( Stream == XPROCESS_STDOUT ) {
		hHandle = pProcess->Stdout;
	} else if ( Stream == XPROCESS_STDERR ) {
		hHandle = pProcess->Stderr;
	} else {
		hHandle = INVALID_HANDLE_VALUE;
	}
	(void)xrtMutexUnlock((xmutex*)&pProcess->Lock);
	return (hHandle == INVALID_HANDLE_VALUE) ? -1 : (intptr_t)hHandle;
}



/* 向独立控制台进程组发送 Ctrl+Break。 */
bool __xrtProcessPlatformInterrupt(xprocess* pProcess)
{
	if ( !pProcess->NewGroup ) {
		__xrtProcessErrorSet(
			XERR_UNSUPPORTED,
			XPROCESS_ERROR_SIGNAL,
			"interrupt",
			"Windows interrupt requires a process group",
			ERROR_NOT_SUPPORTED
		);
		return false;
	}
	if ( GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pProcess->Id) ) {
		return true;
	}
	{
		int iError = (int)GetLastError();

		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_SIGNAL,
			"interrupt",
			"process interrupt failed",
			iError
		);
	}
	return false;
}



/* Windows 温和终止复用进程组 Ctrl+Break。 */
bool __xrtProcessPlatformTerminate(xprocess* pProcess)
{
	return __xrtProcessPlatformInterrupt(pProcess);
}



/* 强制结束根进程。 */
bool __xrtProcessPlatformKill(xprocess* pProcess)
{
	if ( TerminateProcess(pProcess->Process, 1u) ) {
		return true;
	}
	if ( GetLastError() == ERROR_ACCESS_DENIED ) {
		return true;
	}
	{
		int iError = (int)GetLastError();

		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_SIGNAL,
			"kill",
			"process kill failed",
			iError
		);
	}
	return false;
}



/* 强制结束 Job 中的完整进程树。 */
bool __xrtProcessPlatformKillTree(xprocess* pProcess)
{
	if ( pProcess->Job == NULL ) {
		__xrtProcessErrorSet(
			XERR_UNSUPPORTED,
			XPROCESS_ERROR_SIGNAL,
			"kill.tree",
			"process was not created with a Windows Job",
			ERROR_NOT_SUPPORTED
		);
		return false;
	}
	if ( TerminateJobObject(pProcess->Job, 1u) ) {
		return true;
	}
	{
		int iError = (int)GetLastError();

		__xrtProcessErrorSet(
			__xrtSystemErrorKind(iError),
			XPROCESS_ERROR_SIGNAL,
			"kill.tree",
			"process tree kill failed",
			iError
		);
	}
	return false;
}

#endif
