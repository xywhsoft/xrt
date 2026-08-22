#if !defined(_WIN32) && !defined(_WIN64)
	#define _POSIX_C_SOURCE 200809L
#endif

#include "../test.h"
#include "../test_console_redirect.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <fcntl.h>
	#include <windows.h>
#endif



/* 验证两个标准流的精确 UTF-8 字节、整行 Helper 和刷新契约。 */
static void testConsoleOutput(void)
{
	testconsoleredirect Out;
	testconsoleredirect Error;
	char arrOut[64];
	char arrError[64];
	size_t iOut;
	size_t iError;

	testConsoleRedirectBegin(&Out, stdout);
	testConsoleRedirectBegin(&Error, stderr);
	testRequire(
		!xrtConsoleIsTerminal(XCONSOLE_STDOUT) &&
		!xrtConsoleIsTerminal(XCONSOLE_STDERR),
		"redirected console reported terminal"
	);
	testRequire(
		xrtConsoleWrite(XCONSOLE_STDOUT, XRT_STR_LITERAL("hello ")) &&
		xrtConsoleWriteLine(XCONSOLE_STDOUT, XRT_STR_LITERAL("world")) &&
		xrtConsoleWriteLine(XCONSOLE_STDERR, XRT_STR_LITERAL("error")) &&
		xrtConsoleFlush(XCONSOLE_STDOUT) &&
		xrtConsoleFlush(XCONSOLE_STDERR),
		"console output failed"
	);
	testConsoleRedirectEnd(&Error);
	testConsoleRedirectEnd(&Out);
	iOut = testConsoleRedirectRead(&Out, arrOut, sizeof(arrOut));
	iError = testConsoleRedirectRead(&Error, arrError, sizeof(arrError));
	testRequire(
		(iOut == sizeof("hello world\n") - 1u) &&
		(memcmp(arrOut, "hello world\n", iOut) == 0),
		"console stdout bytes changed"
	);
	testRequire(
		(iError == sizeof("error\n") - 1u) &&
		(memcmp(arrError, "error\n", iError) == 0),
		"console stderr bytes changed"
	);
	testConsoleRedirectUnit(&Error);
	testConsoleRedirectUnit(&Out);
}



/* 验证空文本、非法视图和非法流具有确定结果。 */
static void testConsoleArguments(void)
{
	testRequire(
		xrtConsoleWrite(XCONSOLE_STDOUT, (xstrview){ NULL, 0 }),
		"console rejected empty text"
	);
	testRequire(
		!xrtConsoleWrite(
			XCONSOLE_STDOUT,
			(xstrview){ NULL, 1u }
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"console invalid view error mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtConsoleFlush((xconsolestream)0) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.console") == 0) &&
		(xrtErrorCode(xrtGetError()) == XCONSOLE_ERROR_STREAM),
		"console invalid stream error mismatch"
	);
	xrtClearError();
}



/* 验证关闭的标准流产生 xrt.console 写入错误。 */
static void testConsoleFailure(void)
{
	testconsoleredirect Out;
	bool bWritten;
	int iDescriptor;

	testConsoleRedirectBegin(&Out, stdout);
	iDescriptor = testConsoleFileno(stdout);
	testRequire(testConsoleClose(iDescriptor) == 0, "console failure close failed");
	bWritten = xrtConsoleWrite(XCONSOLE_STDOUT, XRT_STR_LITERAL("fail"));
	if ( bWritten ) {
		testRequire(
			!xrtConsoleFlush(XCONSOLE_STDOUT) &&
			(strcmp(xrtErrorDomain(xrtGetError()), "xrt.console") == 0) &&
			(xrtErrorCode(xrtGetError()) == XCONSOLE_ERROR_FLUSH),
			"buffered console failure did not fail on flush"
		);
	} else {
		testRequire(
			(strcmp(xrtErrorDomain(xrtGetError()), "xrt.console") == 0) &&
			(xrtErrorCode(xrtGetError()) == XCONSOLE_ERROR_WRITE),
			"closed console write error mismatch"
		);
	}
	xrtClearError();
	testConsoleRedirectRestore(&Out);
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
			xrtConsoleWrite(
				XCONSOLE_STDOUT,
				(xstrview){ arrMessage, sizeof(arrMessage) }
			),
			"Windows Unicode console write failed"
		);
		testRequire(
			!xrtConsoleWrite(
				XCONSOLE_STDOUT,
				(xstrview){ "\xC3", 1u }
			) &&
			(xrtErrorCode(xrtGetError()) == XCONSOLE_ERROR_UTF8),
			"Windows console accepted invalid UTF-8"
		);
		xrtClearError();

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



/* 验证 Windows 输出跟随 SetStdHandle，而不是固定使用启动时的 CRT 句柄。 */
static void testConsoleWindowsStandardHandle(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		HANDLE hCrtBuffer;
		HANDLE hCrtWriter;
		HANDLE hTargetBuffer;
		HANDLE hSavedStandard;
		HANDLE hProcess = GetCurrentProcess();
		COORD Origin = { 0, 0 };
		WCHAR arrOutput[3];
		DWORD iRead = 0;
		int iDescriptor = testConsoleFileno(stdout);
		int iSaved;
		int iWriter;

		hCrtBuffer = CreateConsoleScreenBuffer(
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			CONSOLE_TEXTMODE_BUFFER,
			NULL
		);
		testRequire(
			hCrtBuffer != INVALID_HANDLE_VALUE,
			"Windows CRT console buffer creation failed"
		);
		hTargetBuffer = CreateConsoleScreenBuffer(
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			CONSOLE_TEXTMODE_BUFFER,
			NULL
		);
		testRequire(
			hTargetBuffer != INVALID_HANDLE_VALUE,
			"Windows standard console buffer creation failed"
		);
		testRequire(
			DuplicateHandle(
				hProcess,
				hCrtBuffer,
				hProcess,
				&hCrtWriter,
				0,
				FALSE,
				DUPLICATE_SAME_ACCESS
			) != 0,
			"Windows CRT console handle duplicate failed"
		);
		iWriter = _open_osfhandle((intptr_t)hCrtWriter, _O_BINARY);
		testRequire(iWriter >= 0, "Windows CRT console handle open failed");
		hSavedStandard = GetStdHandle(STD_OUTPUT_HANDLE);
		testRequire(fflush(stdout) == 0, "Windows standard handle initial flush failed");
		iSaved = testConsoleDup(iDescriptor);
		testRequire(iSaved >= 0, "Windows standard handle stdout duplicate failed");
		testRequire(
			testConsoleDup2(iWriter, iDescriptor) >= 0,
			"Windows CRT console redirect failed"
		);
		testRequire(
			testConsoleClose(iWriter) == 0,
			"Windows CRT console writer close failed"
		);
		clearerr(stdout);

		testRequire(
			SetStdHandle(STD_OUTPUT_HANDLE, hTargetBuffer) != 0,
			"Windows standard handle replacement failed"
		);
		testRequire(
			xrtConsoleWrite(XCONSOLE_STDOUT, XRT_STR_LITERAL("std")),
			"Windows standard handle write failed"
		);
		testRequire(
			SetStdHandle(STD_OUTPUT_HANDLE, hSavedStandard) != 0,
			"Windows standard handle restore failed"
		);

		testRequire(
			testConsoleDup2(iSaved, iDescriptor) >= 0,
			"Windows standard handle CRT restore failed"
		);
		testRequire(
			testConsoleClose(iSaved) == 0,
			"Windows standard handle saved descriptor close failed"
		);
		clearerr(stdout);
		testRequire(
			ReadConsoleOutputCharacterW(
				hTargetBuffer,
				arrOutput,
				(DWORD)(sizeof(arrOutput) / sizeof(arrOutput[0])),
				Origin,
				&iRead
			) != 0,
			"Windows standard handle readback failed"
		);
		testRequire(
			(iRead == 3u) &&
			(arrOutput[0] == L's') &&
			(arrOutput[1] == L't') &&
			(arrOutput[2] == L'd'),
			"Windows standard handle output mismatch"
		);
		testRequire(
			CloseHandle(hTargetBuffer) != 0,
			"Windows standard console buffer close failed"
		);
		testRequire(
			CloseHandle(hCrtBuffer) != 0,
			"Windows CRT console buffer close failed"
		);
	#endif
}



/* 执行 Console 输出、错误和 Windows Unicode 边界测试。 */
int main(void)
{
	testConsoleOutput();
	testConsoleArguments();
	testConsoleFailure();
	testConsoleWindowsUnicode();
	testConsoleWindowsStandardHandle();
	printf("[PASS] Console\n");
	return 0;
}
