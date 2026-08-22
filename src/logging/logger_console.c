#include "../internal/xrt_console.h"
#include "../internal/xrt_internal.h"
#include <xrt/logger.h>

#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64)
	#include <io.h>
#endif



#if defined(XRT_FEATURE_LOGGER_CONSOLE)

#if (defined(_WIN32) || defined(_WIN64)) && \
	!defined(ENABLE_VIRTUAL_TERMINAL_PROCESSING)
	#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif



/* Console Sink 保存不可变配置、终端能力和逐条记录串行锁。 */
typedef struct xlogconsolestate {
	xmutex Lock;
	xatomic64 Owner;
	xlogconsoleconfig Config;
	bool StdoutColor;
	bool StderrColor;
} xlogconsolestate;



/* 验证 Console 配置和内嵌文本配置。 */
static bool __xrtLogConsoleConfigValid(const xlogconsoleconfig* pConfig)
{
	return
		(pConfig != NULL) &&
		(pConfig->Level >= XLOG_TRACE) &&
		(pConfig->Level <= XLOG_OFF) &&
		(pConfig->Target >= XLOG_CONSOLE_STDOUT) &&
		(pConfig->Target <= XLOG_CONSOLE_SPLIT) &&
		(pConfig->Color >= XLOG_CONSOLE_COLOR_AUTO) &&
		(pConfig->Color <= XLOG_CONSOLE_COLOR_ALWAYS) &&
		(pConfig->ErrorLevel >= XLOG_TRACE) &&
		(pConfig->ErrorLevel <= XLOG_OFF) &&
		xrtLogTextConfigValidate(&pConfig->Text);
}



/* 建立 Logger Console 自身的结构化错误。 */
static void __xrtLogConsoleError(
	xlogerror Code,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtErrorSetDetail(
		Kind,
		"xrt.log",
		(int32)Code,
		sOperation,
		sMessage,
		NULL
	);
}



#if defined(_WIN32) || defined(_WIN64)
/* 从标准流枚举取得 CRT 流。 */
static FILE* __xrtLogConsoleFile(xconsolestream Stream)
{
	return Stream == XCONSOLE_STDOUT ? stdout : stderr;
}
#endif



/* 在 Windows 控制台启用 ANSI 虚拟终端序列。 */
static bool __xrtLogConsoleEnableAnsi(xconsolestream Stream)
{
	#if defined(_WIN32) || defined(_WIN64)
		FILE* pFile = __xrtLogConsoleFile(Stream);
		intptr_t iHandle = _get_osfhandle(_fileno(pFile));
		HANDLE hConsole;
		DWORD iMode;

		if ( iHandle == -1 ) {
			return false;
		}
		hConsole = (HANDLE)iHandle;
		if ( !GetConsoleMode(hConsole, &iMode) ) {
			return false;
		}
		if ( (iMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0 ) {
			return true;
		}
		return SetConsoleMode(
			hConsole,
			iMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING
		) != 0;
	#else
		(void)Stream;
		return true;
	#endif
}



/* 按显式策略、NO_COLOR、TTY 和 ANSI 能力决定颜色。 */
static bool __xrtLogConsoleColor(
	xconsolestream Stream,
	xlogconsolecolor Policy
)
{
	if ( Policy == XLOG_CONSOLE_COLOR_NEVER ) {
		return false;
	}
	if ( Policy == XLOG_CONSOLE_COLOR_ALWAYS ) {
		(void)__xrtLogConsoleEnableAnsi(Stream);
		return true;
	}
	if ( getenv("NO_COLOR") != NULL ) {
		return false;
	}
	return
		xrtConsoleIsTerminal(Stream) &&
		__xrtLogConsoleEnableAnsi(Stream);
}



/* 返回各日志级别稳定使用的 ANSI 颜色。 */
static cstr __xrtLogConsoleLevelColor(xloglevel Level)
{
	static const cstr arrColors[] = {
		"\033[90m",
		"\033[37;1m",
		"\033[39m",
		"\033[33m",
		"\033[31m",
		"\033[35m"
	};

	return arrColors[(size_t)Level];
}



/* 把文本格式器产生的一个分段写入已锁定 Console Writer。 */
static bool __xrtLogConsoleEmit(xbytesview Data, ptr pUserData)
{
	xconsolewriter* pWriter = (xconsolewriter*)pUserData;

	return __xrtConsoleWriterWrite(pWriter, Data.Data, Data.Size);
}



/* 保存第一处底层失败并用 Logger 错误包装原因链。 */
static void __xrtLogConsoleCapture(
	xerror** ppError,
	xlogerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	if ( *ppError == NULL ) {
		__xrtErrorWrapDetail(
			XERR_IO,
			"xrt.log",
			(int32)Code,
			sOperation,
			sMessage
		);
		*ppError = xrtTakeError();
	} else {
		xrtClearError();
	}
}



/* 按 Sink 配置选择当前记录的标准流。 */
static xconsolestream __xrtLogConsoleStream(
	const xlogconsolestate* pState,
	xloglevel Level
)
{
	if ( pState->Config.Target == XLOG_CONSOLE_STDOUT ) {
		return XCONSOLE_STDOUT;
	}
	if ( pState->Config.Target == XLOG_CONSOLE_STDERR ) {
		return XCONSOLE_STDERR;
	}
	return Level >= pState->Config.ErrorLevel
		? XCONSOLE_STDERR
		: XCONSOLE_STDOUT;
}



/* 读取一个标准流在 Sink 创建时缓存的颜色能力。 */
static bool __xrtLogConsoleStreamColor(
	const xlogconsolestate* pState,
	xconsolestream Stream
)
{
	return Stream == XCONSOLE_STDOUT
		? pState->StdoutColor
		: pState->StderrColor;
}



/* 串行写出一条完整文本记录，并在失败后尽力复位颜色。 */
static xlogresult __xrtLogConsoleWrite(
	const xlogrecord* pRecord,
	ptr pUserData
)
{
	xlogconsolestate* pState = (xlogconsolestate*)pUserData;
	xconsolewriter Writer;
	xerror* pFirstError = NULL;
	xconsolestream Stream;
	cstr sColor;
	uint64 iThread;
	bool bOpened;
	bool bColor;
	bool bResult;
	bool bFailed;

	iThread = __xrtCurrentThreadId();
	if (
		xrtAtomic64Load(&pState->Owner, XMEMORY_ACQUIRE) ==
		iThread
	) {
		return XLOG_RESULT_DROPPED;
	}
	if ( !xrtMutexLock(&pState->Lock) ) {
		return XLOG_RESULT_ERROR;
	}
	xrtAtomic64Store(&pState->Owner, iThread, XMEMORY_RELEASE);
	Stream = __xrtLogConsoleStream(pState, pRecord->Level);
	bOpened = __xrtConsoleWriterOpen(Stream, &Writer);
	if ( !bOpened ) {
		__xrtLogConsoleCapture(
			&pFirstError,
			XLOG_ERROR_CONSOLE_WRITE,
			"console-write",
			"failed to open log console"
		);
	}
	bColor = __xrtLogConsoleStreamColor(pState, Stream);
	sColor = bColor ? __xrtLogConsoleLevelColor(pRecord->Level) : "";
	bResult = bOpened && (!bColor || __xrtConsoleWriterWrite(
		&Writer,
		sColor,
		strlen(sColor)
	));
	if ( bResult ) {
		bResult = xrtLogTextWrite(
			pRecord,
			&pState->Config.Text,
			__xrtLogConsoleEmit,
			&Writer,
			NULL
		);
	}
	if ( bOpened && !bResult ) {
		__xrtLogConsoleCapture(
			&pFirstError,
			XLOG_ERROR_CONSOLE_WRITE,
			"console-write",
			"failed to write log console"
		);
	}
	if (
		bOpened &&
		bColor &&
		!__xrtConsoleWriterWrite(&Writer, "\033[0m", 4u)
	) {
		__xrtLogConsoleCapture(
			&pFirstError,
			XLOG_ERROR_CONSOLE_WRITE,
			"console-reset",
			"failed to reset log console color"
		);
	}
	if (
		bOpened &&
		pState->Config.Flush &&
		!__xrtConsoleWriterFlush(&Writer)
	) {
		__xrtLogConsoleCapture(
			&pFirstError,
			XLOG_ERROR_CONSOLE_FLUSH,
			"console-flush",
			"failed to flush log console"
		);
	}
	if ( bOpened ) {
		__xrtConsoleWriterClose(&Writer);
	}
	bFailed = pFirstError != NULL;
	if ( bFailed ) {
		__xrtErrorSetOwned(pFirstError);
	}
	xrtAtomic64Store(&pState->Owner, 0, XMEMORY_RELEASE);
	if ( !xrtMutexUnlock(&pState->Lock) ) {
		return XLOG_RESULT_ERROR;
	}
	return bFailed ? XLOG_RESULT_ERROR : XLOG_RESULT_WRITTEN;
}



/* 刷新一个标准流，并保留本次 Flush 的第一处失败。 */
static void __xrtLogConsoleFlushStream(
	xconsolestream Stream,
	xerror** ppError
)
{
	xconsolewriter Writer;

	if ( !__xrtConsoleWriterOpen(Stream, &Writer) ) {
		__xrtLogConsoleCapture(
			ppError,
			XLOG_ERROR_CONSOLE_FLUSH,
			"console-flush",
			"failed to open log console for flush"
		);
		return;
	}
	if ( !__xrtConsoleWriterFlush(&Writer) ) {
		__xrtLogConsoleCapture(
			ppError,
			XLOG_ERROR_CONSOLE_FLUSH,
			"console-flush",
			"failed to flush log console"
		);
	}
	__xrtConsoleWriterClose(&Writer);
}



/* 刷新 Sink 可能使用的全部标准流。 */
static bool __xrtLogConsoleFlush(ptr pUserData)
{
	xlogconsolestate* pState = (xlogconsolestate*)pUserData;
	xerror* pFirstError = NULL;
	uint64 iThread = __xrtCurrentThreadId();
	bool bFailed;

	if (
		xrtAtomic64Load(&pState->Owner, XMEMORY_ACQUIRE) ==
		iThread
	) {
		return true;
	}
	if ( !xrtMutexLock(&pState->Lock) ) {
		return false;
	}
	xrtAtomic64Store(&pState->Owner, iThread, XMEMORY_RELEASE);
	if ( pState->Config.Target != XLOG_CONSOLE_STDERR ) {
		__xrtLogConsoleFlushStream(XCONSOLE_STDOUT, &pFirstError);
	}
	if ( pState->Config.Target != XLOG_CONSOLE_STDOUT ) {
		__xrtLogConsoleFlushStream(XCONSOLE_STDERR, &pFirstError);
	}
	bFailed = pFirstError != NULL;
	if ( bFailed ) {
		__xrtErrorSetOwned(pFirstError);
	}
	xrtAtomic64Store(&pState->Owner, 0, XMEMORY_RELEASE);
	if ( !xrtMutexUnlock(&pState->Lock) ) {
		return false;
	}
	return !bFailed;
}



/* 释放 Console Sink 状态，不关闭进程标准流。 */
static void __xrtLogConsoleDrop(ptr pUserData)
{
	xlogconsolestate* pState = (xlogconsolestate*)pUserData;

	(void)xrtMutexUnit(&pState->Lock);
	xrtFree(pState);
}



/* 初始化常用 Console Sink 配置。 */
XRT_API bool xrtLogConsoleConfigInit(xlogconsoleconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pConfig, 0, sizeof(xlogconsoleconfig));
	pConfig->Level = XLOG_INFO;
	pConfig->Target = XLOG_CONSOLE_SPLIT;
	pConfig->Color = XLOG_CONSOLE_COLOR_AUTO;
	pConfig->ErrorLevel = XLOG_ERROR;
	pConfig->Flush = true;
	return xrtLogTextConfigInit(&pConfig->Text, XLOG_TEXT_FULL);
}



/* 创建线程安全 Console Sink；空配置使用常用默认值。 */
XRT_API xlogsink* xrtLogConsole(const xlogconsoleconfig* pConfig)
{
	xlogconsoleconfig Default;
	xlogconsolestate* pState;
	xlogsinkconfig SinkConfig;
	xlogsink* pSink;

	if ( pConfig == NULL ) {
		if ( !xrtLogConsoleConfigInit(&Default) ) {
			return NULL;
		}
		pConfig = &Default;
	}
	if ( !__xrtLogConsoleConfigValid(pConfig) ) {
		__xrtLogConsoleError(
			XLOG_ERROR_CONSOLE_CONFIG,
			XERR_ARGUMENT,
			"console-create",
			"invalid log console configuration"
		);
		return NULL;
	}
	pState = (xlogconsolestate*)xrtCalloc(1u, sizeof(xlogconsolestate));
	if ( pState == NULL ) {
		return NULL;
	}
	if ( !xrtMutexInit(&pState->Lock) ) {
		xrtFree(pState);
		return NULL;
	}
	xrtAtomic64Init(&pState->Owner, 0);
	pState->Config = *pConfig;
	pState->StdoutColor = __xrtLogConsoleColor(
		XCONSOLE_STDOUT,
		pConfig->Color
	);
	pState->StderrColor = __xrtLogConsoleColor(
		XCONSOLE_STDERR,
		pConfig->Color
	);
	memset(&SinkConfig, 0, sizeof(SinkConfig));
	SinkConfig.Name = XRT_STR_LITERAL("console");
	SinkConfig.Level = pConfig->Level;
	SinkConfig.Write = __xrtLogConsoleWrite;
	SinkConfig.Flush = __xrtLogConsoleFlush;
	SinkConfig.Drop = __xrtLogConsoleDrop;
	SinkConfig.UserData = pState;
	pSink = xrtLogSinkCreate(&SinkConfig);
	if ( pSink == NULL ) {
		(void)xrtMutexUnit(&pState->Lock);
		xrtFree(pState);
		return NULL;
	}
	return pSink;
}



/* 创建并附加常用 Console Sink。 */
XRT_API bool xrtLogAddConsole(
	xlogger* pLogger,
	const xlogconsoleconfig* pConfig
)
{
	xlogsink* pSink;
	bool bResult;

	if ( pLogger == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pSink = xrtLogConsole(pConfig);
	if ( pSink == NULL ) {
		return false;
	}
	bResult = xrtLogAttach(pLogger, pSink);
	xrtLogSinkFree(pSink);
	return bResult;
}

#endif
