#include "../internal/xrt_console.h"

#include <errno.h>

#if defined(_WIN32) || defined(_WIN64)
	#include <io.h>
#else
	#include <unistd.h>
#endif



#if defined(XRT_FEATURE_CONSOLE)

#if defined(_WIN32) || defined(_WIN64)
/* Windows CRT 锁不是所有编译器都可链接，使用两个递归临界区统一串行标准流。 */
typedef struct xconsolelocks {
	CRITICAL_SECTION Stdout;
	CRITICAL_SECTION Stderr;
} xconsolelocks;



static xconsolelocks __xrtConsoleLocks;
static volatile LONG __xrtConsoleLockState = 0;



/* 首个调用线程建立进程级标准流锁，其余线程等待初始化发布。 */
static void __xrtConsoleLockInit(void)
{
	if ( InterlockedCompareExchange(&__xrtConsoleLockState, 1, 0) == 0 ) {
		InitializeCriticalSection(&__xrtConsoleLocks.Stdout);
		InitializeCriticalSection(&__xrtConsoleLocks.Stderr);
		(void)InterlockedExchange(&__xrtConsoleLockState, 2);
		return;
	}
	while ( InterlockedCompareExchange(
		&__xrtConsoleLockState,
		2,
		2
	) != 2 ) {
		Sleep(0);
	}
}
#endif



/* 验证标准流并返回对应的 CRT 流。 */
static FILE* __xrtConsoleFile(xconsolestream Stream)
{
	if ( Stream == XCONSOLE_STDOUT ) {
		return stdout;
	}
	if ( Stream == XCONSOLE_STDERR ) {
		return stderr;
	}
	__xrtErrorSetDetail(
		XERR_ARGUMENT,
		"xrt.console",
		XCONSOLE_ERROR_STREAM,
		"stream",
		"invalid console stream",
		NULL
	);
	return NULL;
}



/* 锁定标准流，使一次公共调用或一条日志记录不会被并发分段。 */
static bool __xrtConsoleLock(FILE* pFile)
{
	#if defined(_WIN32) || defined(_WIN64)
		__xrtConsoleLockInit();
		EnterCriticalSection(
			pFile == stdout
				? &__xrtConsoleLocks.Stdout
				: &__xrtConsoleLocks.Stderr
		);
	#else
		flockfile(pFile);
	#endif
	return true;
}



/* 解除 CRT 标准流锁。 */
static void __xrtConsoleUnlock(FILE* pFile)
{
	#if defined(_WIN32) || defined(_WIN64)
		LeaveCriticalSection(
			pFile == stdout
				? &__xrtConsoleLocks.Stdout
				: &__xrtConsoleLocks.Stderr
		);
	#else
		funlockfile(pFile);
	#endif
}



#if defined(_WIN32) || defined(_WIN64)
/* 判断 Windows 句柄当前是否指向可直接写入的控制台。 */
static bool __xrtConsoleHandleIsConsole(HANDLE hHandle)
{
	DWORD iMode;

	return
		(hHandle != NULL) &&
		(hHandle != INVALID_HANDLE_VALUE) &&
		(GetConsoleMode(hHandle, &iMode) != 0);
}



/* 解析当前 Windows 进程标准句柄；仅在原生句柄缺失时回退到 CRT。 */
static ptr __xrtConsoleNative(FILE* pFile, bool* pIsConsole)
{
	intptr_t iCrtValue;
	HANDLE hCrt;
	HANDLE hStandard;
	HANDLE hSelected;

	iCrtValue = _get_osfhandle(_fileno(pFile));
	hCrt = iCrtValue != -1 ? (HANDLE)iCrtValue : NULL;
	hStandard = GetStdHandle(
		pFile == stdout ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE
	);
	if ( hStandard == INVALID_HANDLE_VALUE ) {
		hStandard = NULL;
	}
	hSelected = hStandard != NULL ? hStandard : hCrt;
	*pIsConsole = __xrtConsoleHandleIsConsole(hSelected);
	return (ptr)hSelected;
}
#endif



/* 建立保留原生系统代码的 Console 错误。 */
static void __xrtConsoleError(
	xconsoleerror Code,
	cstr sOperation,
	cstr sMessage,
	int iSystemCode
)
{
	__xrtErrorSetSystem(
		"xrt.console",
		(int32)Code,
		sOperation,
		iSystemCode,
		sMessage
	);
}



#if defined(_WIN32) || defined(_WIN64)
/* 把严格 UTF-8 分块转换为 UTF-16，并完整写入真实 Windows 控制台。 */
static bool __xrtConsoleWriteWide(
	xconsolewriter* pWriter,
	const void* pData,
	size_t iSize
)
{
	enum { XRT_CONSOLE_UTF8_CHUNK = 512 };
	WCHAR arrWide[XRT_CONSOLE_UTF8_CHUNK];
	const char* pText = (const char*)pData;

	while ( iSize != 0 ) {
		size_t iChunk = iSize < XRT_CONSOLE_UTF8_CHUNK
			? iSize
			: XRT_CONSOLE_UTF8_CHUNK;
		int iWide = 0;
		DWORD iSystemCode = ERROR_NO_UNICODE_TRANSLATION;

		/* 最多回退三个尾字节，允许 UTF-8 标量跨内部块边界。 */
		for ( size_t iRetry = 0; iRetry < 4u; iRetry++ ) {
			iWide = MultiByteToWideChar(
				CP_UTF8,
				MB_ERR_INVALID_CHARS,
				pText,
				(int)iChunk,
				arrWide,
				XRT_CONSOLE_UTF8_CHUNK
			);
			if ( iWide != 0 ) {
				break;
			}
			iSystemCode = GetLastError();
			if (
				(iSystemCode != ERROR_NO_UNICODE_TRANSLATION) ||
				(iChunk == iSize) ||
				(iChunk == 1u)
			) {
				break;
			}
			iChunk--;
		}
		if ( iWide == 0 ) {
			__xrtConsoleError(
				XCONSOLE_ERROR_UTF8,
				"write",
				"console text is not valid UTF-8",
				(int)iSystemCode
			);
			return false;
		}

		for ( size_t iPosition = 0; iPosition < (size_t)iWide; ) {
			DWORD iWritten = 0;
			DWORD iRemain = (DWORD)((size_t)iWide - iPosition);
			BOOL bWritten = WriteConsoleW(
				(HANDLE)pWriter->Console,
				arrWide + iPosition,
				iRemain,
				&iWritten,
				NULL
			);

			if ( !bWritten || (iWritten == 0) ) {
				iSystemCode = bWritten ? ERROR_WRITE_FAULT : GetLastError();
				__xrtConsoleError(
					XCONSOLE_ERROR_WRITE,
					"write",
					"failed to write console",
					(int)iSystemCode
				);
				return false;
			}
			iPosition += (size_t)iWritten;
		}
		pText += iChunk;
		iSize -= iChunk;
	}
	return true;
}



/* 向 Windows 文件或管道标准句柄完整写入原始 UTF-8 字节。 */
static bool __xrtConsoleWriteNative(
	xconsolewriter* pWriter,
	const void* pData,
	size_t iSize
)
{
	const unsigned char* pBytes = (const unsigned char*)pData;

	while ( iSize != 0 ) {
		DWORD iChunk = iSize > (size_t)MAXDWORD
			? MAXDWORD
			: (DWORD)iSize;
		DWORD iWritten = 0;

		if ( !WriteFile(
			(HANDLE)pWriter->Native,
			pBytes,
			iChunk,
			&iWritten,
			NULL
		) || (iWritten == 0) ) {
			__xrtConsoleError(
				XCONSOLE_ERROR_WRITE,
				"write",
				"failed to write redirected console stream",
				(int)GetLastError()
			);
			return false;
		}
		pBytes += iWritten;
		iSize -= (size_t)iWritten;
	}
	return true;
}
#endif



/* 打开并锁定标准流，真实 Windows 控制台在直接写入前同步 stdio 缓冲。 */
bool __xrtConsoleWriterOpen(
	xconsolestream Stream,
	xconsolewriter* pWriter
)
{
	FILE* pFile;
	#if defined(_WIN32) || defined(_WIN64)
		bool bConsole;
	#endif

	if ( pWriter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pWriter, 0, sizeof(xconsolewriter));
	pFile = __xrtConsoleFile(Stream);
	if ( pFile == NULL ) {
		return false;
	}
	if ( !__xrtConsoleLock(pFile) ) {
		return false;
	}
	pWriter->File = pFile;
	#if defined(_WIN32) || defined(_WIN64)
		pWriter->Native = __xrtConsoleNative(pFile, &bConsole);
		pWriter->Console = bConsole ? pWriter->Native : NULL;
		if ( pWriter->Native != NULL ) {
			errno = 0;
			if ( fflush(pFile) != 0 ) {
				int iSystemCode = errno;

				__xrtConsoleUnlock(pFile);
				memset(pWriter, 0, sizeof(xconsolewriter));
				__xrtConsoleError(
					XCONSOLE_ERROR_FLUSH,
					"open",
					"failed to flush console before direct write",
					iSystemCode
				);
				return false;
			}
		}
	#endif
	return true;
}



/* 完整写入 UTF-8 文本；普通文件和管道保留原字节。 */
bool __xrtConsoleWriterWrite(
	xconsolewriter* pWriter,
	const void* pData,
	size_t iSize
)
{
	size_t iWritten;

	if (
		(pWriter == NULL) ||
		(pWriter->File == NULL) ||
		((pData == NULL) && (iSize != 0))
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iSize == 0 ) {
		return true;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( pWriter->Console != NULL ) {
			return __xrtConsoleWriteWide(pWriter, pData, iSize);
		}
		if ( pWriter->Native != NULL ) {
			return __xrtConsoleWriteNative(pWriter, pData, iSize);
		}
	#endif
	errno = 0;
	iWritten = fwrite(pData, 1u, iSize, pWriter->File);
	if ( iWritten == iSize ) {
		return true;
	}
	__xrtConsoleError(
		XCONSOLE_ERROR_WRITE,
		"write",
		"failed to write console",
		errno
	);
	return false;
}



/* 刷新已锁定的标准流。 */
bool __xrtConsoleWriterFlush(xconsolewriter* pWriter)
{
	if ( (pWriter == NULL) || (pWriter->File == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	errno = 0;
	if ( fflush(pWriter->File) == 0 ) {
		return true;
	}
	__xrtConsoleError(
		XCONSOLE_ERROR_FLUSH,
		"flush",
		"failed to flush console",
		errno
	);
	return false;
}



/* 解锁标准流并清空 Writer。 */
void __xrtConsoleWriterClose(xconsolewriter* pWriter)
{
	FILE* pFile;

	if ( (pWriter == NULL) || (pWriter->File == NULL) ) {
		return;
	}
	pFile = pWriter->File;
	memset(pWriter, 0, sizeof(xconsolewriter));
	__xrtConsoleUnlock(pFile);
}



/* 写入一段 UTF-8 文本。 */
XRT_API bool xrtConsoleWrite(xconsolestream Stream, xstrview Text)
{
	xconsolewriter Writer;
	bool bResult;

	if ( (Text.Data == NULL) && (Text.Size != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtConsoleWriterOpen(Stream, &Writer) ) {
		return false;
	}
	bResult = __xrtConsoleWriterWrite(&Writer, Text.Data, Text.Size);
	__xrtConsoleWriterClose(&Writer);
	return bResult;
}



/* 在一个标准流锁内写入文本和换行符。 */
XRT_API bool xrtConsoleWriteLine(xconsolestream Stream, xstrview Text)
{
	xconsolewriter Writer;
	bool bResult;

	if ( (Text.Data == NULL) && (Text.Size != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtConsoleWriterOpen(Stream, &Writer) ) {
		return false;
	}
	bResult =
		__xrtConsoleWriterWrite(&Writer, Text.Data, Text.Size) &&
		__xrtConsoleWriterWrite(&Writer, "\n", 1u);
	__xrtConsoleWriterClose(&Writer);
	return bResult;
}



/* 刷新一个标准流。 */
XRT_API bool xrtConsoleFlush(xconsolestream Stream)
{
	xconsolewriter Writer;
	bool bResult;

	if ( !__xrtConsoleWriterOpen(Stream, &Writer) ) {
		return false;
	}
	bResult = __xrtConsoleWriterFlush(&Writer);
	__xrtConsoleWriterClose(&Writer);
	return bResult;
}



/* 判断标准流当前是否连接交互终端。 */
XRT_API bool xrtConsoleIsTerminal(xconsolestream Stream)
{
	FILE* pFile = __xrtConsoleFile(Stream);

	if ( pFile == NULL ) {
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		bool bConsole;

		(void)__xrtConsoleNative(pFile, &bConsole);
		return bConsole;
	#else
		return isatty(fileno(pFile)) != 0;
	#endif
}

#endif
