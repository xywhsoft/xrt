#if !defined(_WIN32) && !defined(_WIN64)
	#define _POSIX_C_SOURCE 200809L
#endif

#include "../test.h"
#include "../test_console_redirect.h"
#include "../test_thread.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <fcntl.h>
	#include <windows.h>
#endif



/* 并发 Writer 共享同一个 Console Sink。 */
typedef struct testconsolethread {
	xlogsink* Sink;
	uint32 Index;
	uint32 Count;
} testconsolethread;



/* 错误处理器递归测试保存同一 Sink 和嵌套结果。 */
typedef struct testconsoleerror {
	xlogsink* Sink;
	xlogrecord Record;
	size_t Calls;
	xlogresult Nested;
} testconsoleerror;



/* 验证默认值、级别分流和自动颜色不会污染重定向输出。 */
static void testConsoleSplit(void)
{
	testconsoleredirect Out;
	testconsoleredirect Error;
	xlogconsoleconfig Config;
	xlogrecord Record;
	xlogsink* pSink;
	char arrOut[64];
	char arrError[64];
	size_t iOut;
	size_t iError;

	testConsoleRedirectBegin(&Out, stdout);
	testConsoleRedirectBegin(&Error, stderr);
	testRequire(
		xrtLogConsoleConfigInit(&Config) &&
		(Config.Level == XLOG_INFO) &&
		(Config.Target == XLOG_CONSOLE_SPLIT) &&
		(Config.Color == XLOG_CONSOLE_COLOR_AUTO) &&
		(Config.ErrorLevel == XLOG_ERROR) &&
		Config.Flush,
		"console default config changed"
	);
	testRequire(
		xrtLogTextConfigInit(&Config.Text, XLOG_TEXT_MESSAGE),
		"console message config failed"
	);
	pSink = xrtLogConsole(&Config);
	testRequire(pSink != NULL, "console split creation failed");
	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = XRT_STR_LITERAL("out");
	testRequire(
		xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_WRITTEN,
		"console stdout submit failed"
	);
	Record.Level = XLOG_ERROR;
	Record.Message = XRT_STR_LITERAL("error");
	testRequire(
		xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_WRITTEN,
		"console stderr submit failed"
	);
	testRequire(xrtLogSinkFlush(pSink), "console split flush failed");
	xrtLogSinkFree(pSink);
	testConsoleRedirectEnd(&Error);
	testConsoleRedirectEnd(&Out);
	iOut = testConsoleRedirectRead(&Out, arrOut, sizeof(arrOut));
	iError = testConsoleRedirectRead(&Error, arrError, sizeof(arrError));
	testRequire(
		(iOut == sizeof("out\n") - 1u) &&
		(memcmp(arrOut, "out\n", iOut) == 0),
		"console stdout split layout changed"
	);
	testRequire(
		(iError == sizeof("error\n") - 1u) &&
		(memcmp(arrError, "error\n", iError) == 0),
		"console stderr split layout changed"
	);
	testConsoleRedirectUnit(&Error);
	testConsoleRedirectUnit(&Out);
}



/* 验证强制颜色、Sink 阈值和一行附加 Helper。 */
static void testConsoleColorAndHelper(void)
{
	testconsoleredirect Out;
	xlogconsoleconfig Config;
	xlogrecord Record;
	xlogsink* pSink;
	xlogger* pLogger;
	char arrOutput[256];
	size_t iSize;

	testConsoleRedirectBegin(&Out, stdout);
	testRequire(
		xrtLogConsoleConfigInit(&Config) &&
		xrtLogTextConfigInit(&Config.Text, XLOG_TEXT_MESSAGE),
		"console color config failed"
	);
	Config.Target = XLOG_CONSOLE_STDOUT;
	Config.Color = XLOG_CONSOLE_COLOR_ALWAYS;
	Config.Level = XLOG_WARN;
	pSink = xrtLogConsole(&Config);
	testRequire(pSink != NULL, "colored console creation failed");
	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = XRT_STR_LITERAL("skip");
	testRequire(
		xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_SKIPPED,
		"console threshold did not skip"
	);
	Record.Level = XLOG_WARN;
	Record.Message = XRT_STR_LITERAL("color");
	testRequire(
		xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_WRITTEN,
		"colored console submit failed"
	);
	xrtLogSinkFree(pSink);

	Config.Color = XLOG_CONSOLE_COLOR_NEVER;
	Config.Level = XLOG_INFO;
	pLogger = xrtLogCreate(XRT_STR_LITERAL("helper"), XLOG_INFO);
	testRequire(
		(pLogger != NULL) &&
		xrtLogAddConsole(pLogger, &Config) &&
		(xrtLog(pLogger, XLOG_INFO, XRT_STR_LITERAL("helper")) ==
		 XLOG_RESULT_WRITTEN) &&
		xrtLogFlush(pLogger),
		"console one-line attach helper failed"
	);
	xrtLogFree(pLogger);
	testConsoleRedirectEnd(&Out);
	iSize = testConsoleRedirectRead(&Out, arrOutput, sizeof(arrOutput));
	testRequire(
		(iSize == sizeof("\033[33mcolor\n\033[0mhelper\n") - 1u) &&
		(memcmp(
			arrOutput,
			"\033[33mcolor\n\033[0mhelper\n",
			iSize
		) == 0),
		"console color or helper layout changed"
	);
	testConsoleRedirectUnit(&Out);
}



/* 错误处理器递归提交同一 Sink 时必须主动丢弃。 */
static void testConsoleErrorHandler(
	const xerror* pError,
	ptr pUserData
)
{
	testconsoleerror* pContext = (testconsoleerror*)pUserData;

	(void)pError;
	pContext->Calls++;
	pContext->Nested = xrtLogSinkSubmit(
		pContext->Sink,
		&pContext->Record
	);
}



/* 验证输出失败不会因错误处理器记录日志而自锁。 */
static void testConsoleRecursiveError(void)
{
	testconsoleredirect Out;
	testconsoleerror Context;
	xlogconsoleconfig Config;
	xlogrecord Record;
	xlogsink* pSink;
	int iDescriptor;

	testConsoleRedirectBegin(&Out, stdout);
	testRequire(
		xrtLogConsoleConfigInit(&Config) &&
		xrtLogTextConfigInit(&Config.Text, XLOG_TEXT_MESSAGE),
		"recursive console config failed"
	);
	Config.Target = XLOG_CONSOLE_STDOUT;
	Config.Color = XLOG_CONSOLE_COLOR_NEVER;
	pSink = xrtLogConsole(&Config);
	testRequire(pSink != NULL, "recursive console creation failed");
	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_ERROR;
	Record.Message = XRT_STR_LITERAL("outer");
	memset(&Context, 0, sizeof(Context));
	Context.Sink = pSink;
	Context.Record = Record;
	iDescriptor = testConsoleFileno(stdout);
	testRequire(testConsoleClose(iDescriptor) == 0, "console failure close failed");
	xrtSetErrorHandler(testConsoleErrorHandler, &Context);
	testRequire(
		xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_ERROR,
		"closed console did not report failure"
	);
	xrtSetErrorHandler(NULL, NULL);
	testRequire(
		(Context.Calls != 0) &&
		(Context.Nested == XLOG_RESULT_DROPPED) &&
		(
			(xrtErrorCode(xrtGetError()) == XLOG_ERROR_CONSOLE_WRITE) ||
			(xrtErrorCode(xrtGetError()) == XLOG_ERROR_CONSOLE_FLUSH)
		),
		"console recursive error guard failed"
	);
	xrtClearError();
	testConsoleRedirectRestore(&Out);
	xrtLogSinkFree(pSink);
	testConsoleRedirectUnit(&Out);
}



/* 每个工作线程提交固定长度记录。 */
static int testConsoleThreadRun(ptr pData)
{
	testconsolethread* pThread = (testconsolethread*)pData;
	xlogrecord Record;
	char arrMessage[16];

	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	for ( uint32 i = 0; i < pThread->Count; i++ ) {
		int iSize = snprintf(
			arrMessage,
			sizeof(arrMessage),
			"T%u-%03u",
			pThread->Index,
			i
		);

		if ( iSize != 6 ) {
			return 1;
		}
		Record.Message = (xstrview){ arrMessage, (size_t)iSize };
		if (
			xrtLogSinkSubmit(pThread->Sink, &Record) !=
			XLOG_RESULT_WRITTEN
		) {
			return 2;
		}
	}
	return 0;
}



/* 验证并发记录不会在格式器分段边界交错。 */
static void testConsoleThreads(void)
{
	testconsoleredirect Out;
	testconsolethread arrContext[4];
	testthread arrThread[4];
	xlogconsoleconfig Config;
	xlogsink* pSink;
	char arrOutput[8192];
	size_t iSize;

	testConsoleRedirectBegin(&Out, stdout);
	testRequire(
		xrtLogConsoleConfigInit(&Config) &&
		xrtLogTextConfigInit(&Config.Text, XLOG_TEXT_MESSAGE),
		"threaded console config failed"
	);
	Config.Target = XLOG_CONSOLE_STDOUT;
	Config.Color = XLOG_CONSOLE_COLOR_NEVER;
	Config.Flush = false;
	pSink = xrtLogConsole(&Config);
	testRequire(pSink != NULL, "threaded console creation failed");
	memset(arrContext, 0, sizeof(arrContext));
	memset(arrThread, 0, sizeof(arrThread));
	for ( size_t i = 0; i < 4u; i++ ) {
		arrContext[i].Sink = pSink;
		arrContext[i].Index = (uint32)i;
		arrContext[i].Count = 200u;
		arrThread[i].Proc = testConsoleThreadRun;
		arrThread[i].Data = &arrContext[i];
	}
	testThreadsStart(arrThread, 4u);
	testThreadsJoin(arrThread, 4u);
	for ( size_t i = 0; i < 4u; i++ ) {
		testRequire(arrThread[i].Result == 0, "console worker failed");
	}
	testRequire(xrtLogSinkFlush(pSink), "threaded console flush failed");
	xrtLogSinkFree(pSink);
	testConsoleRedirectEnd(&Out);
	iSize = testConsoleRedirectRead(&Out, arrOutput, sizeof(arrOutput));
	testRequire(iSize == (4u * 200u * 7u), "threaded console size mismatch");
	for ( size_t i = 0; i < iSize; i += 7u ) {
		testRequire(
			(arrOutput[i] == 'T') &&
			(arrOutput[i + 1u] >= '0') &&
			(arrOutput[i + 1u] <= '3') &&
			(arrOutput[i + 2u] == '-') &&
			(arrOutput[i + 3u] >= '0') &&
			(arrOutput[i + 3u] <= '9') &&
			(arrOutput[i + 4u] >= '0') &&
			(arrOutput[i + 4u] <= '9') &&
			(arrOutput[i + 5u] >= '0') &&
			(arrOutput[i + 5u] <= '9') &&
			(arrOutput[i + 6u] == '\n'),
			"concurrent console records interleaved"
		);
	}
	testConsoleRedirectUnit(&Out);
}



/* 验证真实 Windows 控制台使用 UTF-16，并允许标量跨内部 UTF-8 块边界。 */
static void testConsoleWindowsUnicode(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		HANDLE hBuffer;
		HANDLE hWriter;
		HANDLE hSavedStandard;
		HANDLE hProcess = GetCurrentProcess();
		COORD Origin = { 0, 0 };
		WCHAR arrOutput[513];
		char arrMessage[515];
		xlogconsoleconfig Config;
		xlogrecord Record;
		xlogsink* pSink;
		DWORD iRead = 0;
		int iDescriptor = testConsoleFileno(stdout);
		int iSaved;
		int iWriter;

		hBuffer = CreateConsoleScreenBuffer(
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			CONSOLE_TEXTMODE_BUFFER,
			NULL
		);
		testRequire(
			hBuffer != INVALID_HANDLE_VALUE,
			"Windows console buffer creation failed"
		);
		testRequire(
			DuplicateHandle(
				hProcess,
				hBuffer,
				hProcess,
				&hWriter,
				0,
				FALSE,
				DUPLICATE_SAME_ACCESS
			) != 0,
			"Windows console handle duplicate failed"
		);
		iWriter = _open_osfhandle((intptr_t)hWriter, _O_BINARY);
		testRequire(iWriter >= 0, "Windows console CRT handle failed");
		hSavedStandard = GetStdHandle(STD_OUTPUT_HANDLE);
		testRequire(fflush(stdout) == 0, "Windows console initial flush failed");
		iSaved = testConsoleDup(iDescriptor);
		testRequire(iSaved >= 0, "Windows console stdout duplicate failed");
		testRequire(
			testConsoleDup2(iWriter, iDescriptor) >= 0,
			"Windows console stdout redirect failed"
		);
		testRequire(
			testConsoleClose(iWriter) == 0,
			"Windows console writer close failed"
		);
		clearerr(stdout);
		testRequire(
			SetStdHandle(STD_OUTPUT_HANDLE, hBuffer) != 0,
			"Windows console standard handle replacement failed"
		);

		memset(arrMessage, 'a', 511u);
		arrMessage[511] = (char)0xCE;
		arrMessage[512] = (char)0xA9;
		arrMessage[513] = (char)0xC3;
		arrMessage[514] = (char)0xA9;
		testRequire(
			xrtLogConsoleConfigInit(&Config) &&
			xrtLogTextConfigInit(&Config.Text, XLOG_TEXT_MESSAGE),
			"Windows Unicode console config failed"
		);
		Config.Target = XLOG_CONSOLE_STDOUT;
		Config.Color = XLOG_CONSOLE_COLOR_NEVER;
		pSink = xrtLogConsole(&Config);
		testRequire(pSink != NULL, "Windows Unicode console sink failed");
		memset(&Record, 0, sizeof(Record));
		Record.Level = XLOG_INFO;
		Record.Message = (xstrview){ arrMessage, sizeof(arrMessage) };
		testRequire(
			xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_WRITTEN,
			"Windows Unicode console write failed"
		);

		/* 真实文本控制台拒绝不完整 UTF-8，而文件重定向仍保留原字节。 */
		Record.Message = (xstrview){ "\xC3", 1u };
		testRequire(
			xrtLogSinkSubmit(pSink, &Record) == XLOG_RESULT_ERROR,
			"Windows console accepted invalid UTF-8"
		);
		testRequire(
			xrtErrorCode(xrtGetError()) == XLOG_ERROR_CONSOLE_WRITE,
			"Windows invalid UTF-8 error mismatch"
		);
		xrtClearError();
		xrtLogSinkFree(pSink);

		testRequire(
			SetStdHandle(STD_OUTPUT_HANDLE, hSavedStandard) != 0,
			"Windows console standard handle restore failed"
		);
		testRequire(
			testConsoleDup2(iSaved, iDescriptor) >= 0,
			"Windows console stdout restore failed"
		);
		testRequire(
			testConsoleClose(iSaved) == 0,
			"Windows console saved descriptor close failed"
		);
		clearerr(stdout);
		testRequire(
			ReadConsoleOutputCharacterW(
				hBuffer,
				arrOutput,
				(DWORD)(sizeof(arrOutput) / sizeof(arrOutput[0])),
				Origin,
				&iRead
			) != 0,
			"Windows console Unicode readback failed"
		);
		testRequire(iRead == 513u, "Windows console Unicode size mismatch");
		for ( size_t i = 0; i < 511u; i++ ) {
			testRequire(
				arrOutput[i] == L'a',
				"Windows console ASCII prefix mismatch"
			);
		}
		testRequire(
			(arrOutput[511] == (WCHAR)0x03A9) &&
			(arrOutput[512] == (WCHAR)0x00E9),
			"Windows console UTF-16 conversion mismatch"
		);
		testRequire(
			CloseHandle(hBuffer) != 0,
			"Windows console buffer close failed"
		);
	#endif
}



/* 执行 Console Sink 行为、错误和并发回归。 */
int main(void)
{
	testConsoleSplit();
	testConsoleColorAndHelper();
	testConsoleRecursiveError();
	testConsoleThreads();
	testConsoleWindowsUnicode();
	printf("[PASS] Logger console\n");
	return 0;
}
