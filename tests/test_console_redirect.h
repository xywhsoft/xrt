#ifndef XRT_TEST_CONSOLE_REDIRECT_H
#define XRT_TEST_CONSOLE_REDIRECT_H

#include "test.h"

#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
	#include <io.h>
	#include <windows.h>
	#define testConsoleClose _close
	#define testConsoleDup _dup
	#define testConsoleDup2 _dup2
	#define testConsoleFileno _fileno
#else
	#include <unistd.h>
	#define testConsoleClose close
	#define testConsoleDup dup
	#define testConsoleDup2 dup2
	#define testConsoleFileno fileno
#endif



/* 标准流重定向夹具保存临时文件和原描述符。 */
typedef struct testconsoleredirect {
	FILE* Stream;
	FILE* File;
	int Saved;
	#if defined(_WIN32) || defined(_WIN64)
		DWORD Standard;
		HANDLE Native;
	#endif
} testconsoleredirect;



/* 把一个标准流重定向到临时文件。 */
static void testConsoleRedirectBegin(
	testconsoleredirect* pRedirect,
	FILE* pStream
)
{
	int iDescriptor = testConsoleFileno(pStream);

	memset(pRedirect, 0, sizeof(testconsoleredirect));
	pRedirect->Stream = pStream;
	pRedirect->File = tmpfile();
	testRequire(pRedirect->File != NULL, "console redirect tmpfile failed");
	testRequire(fflush(pStream) == 0, "console redirect initial flush failed");
	#if defined(_WIN32) || defined(_WIN64)
		pRedirect->Standard = pStream == stdout
			? STD_OUTPUT_HANDLE
			: STD_ERROR_HANDLE;
		pRedirect->Native = GetStdHandle(pRedirect->Standard);
	#endif
	pRedirect->Saved = testConsoleDup(iDescriptor);
	testRequire(pRedirect->Saved >= 0, "console redirect duplicate failed");
	testRequire(
		testConsoleDup2(
			testConsoleFileno(pRedirect->File),
			iDescriptor
		) >= 0,
		"console redirect replace failed"
	);
	#if defined(_WIN32) || defined(_WIN64)
		{
			intptr_t iNative = _get_osfhandle(iDescriptor);

			testRequire(iNative != -1, "console redirect native handle failed");
			testRequire(
				SetStdHandle(pRedirect->Standard, (HANDLE)iNative) != 0,
				"console redirect standard handle failed"
			);
		}
	#endif
	clearerr(pStream);
}



/* 恢复标准流的 CRT 描述符和 Windows 进程标准句柄。 */
static void testConsoleRedirectRestore(testconsoleredirect* pRedirect)
{
	int iDescriptor = testConsoleFileno(pRedirect->Stream);

	testRequire(
		testConsoleDup2(pRedirect->Saved, iDescriptor) >= 0,
		"console redirect restore failed"
	);
	testRequire(
		testConsoleClose(pRedirect->Saved) == 0,
		"console redirect saved descriptor close failed"
	);
	pRedirect->Saved = -1;
	#if defined(_WIN32) || defined(_WIN64)
		testRequire(
			SetStdHandle(pRedirect->Standard, pRedirect->Native) != 0,
			"console redirect standard handle restore failed"
		);
	#endif
	clearerr(pRedirect->Stream);
}



/* 恢复标准流，但保留临时文件供读取。 */
static void testConsoleRedirectEnd(testconsoleredirect* pRedirect)
{
	(void)fflush(pRedirect->Stream);
	testConsoleRedirectRestore(pRedirect);
}



/* 读取重定向临时文件的全部现有字节。 */
static size_t testConsoleRedirectRead(
	testconsoleredirect* pRedirect,
	char* sOutput,
	size_t iCapacity
)
{
	size_t iSize;

	testRequire(iCapacity != 0, "console redirect output has no capacity");
	testRequire(fflush(pRedirect->File) == 0, "console temp flush failed");
	testRequire(fseek(pRedirect->File, 0, SEEK_SET) == 0, "console temp seek failed");
	iSize = fread(sOutput, 1u, iCapacity - 1u, pRedirect->File);
	testRequire(!ferror(pRedirect->File), "console temp read failed");
	sOutput[iSize] = 0;
	return iSize;
}



/* 关闭重定向测试文件。 */
static void testConsoleRedirectUnit(testconsoleredirect* pRedirect)
{
	testRequire(fclose(pRedirect->File) == 0, "console temp close failed");
}

#endif
