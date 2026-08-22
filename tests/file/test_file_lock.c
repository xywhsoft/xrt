#include "../test.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#else
	#include <sys/types.h>
	#include <sys/wait.h>
	#include <unistd.h>
#endif



/* 子进程验证非阻塞冲突或无竞争锁定。 */
static int testFileLockChild(cstr sPath, bool bBusy)
{
	xfile File = xrtOpen(sPath, XFILE_READ | XFILE_WRITE);
	bool bLocked;

	if ( File == NULL ) {
		return 10;
	}
	bLocked = xrtFileLock(File, XFILE_LOCK_EXCLUSIVE, false);
	if ( bBusy ) {
		if ( bLocked || (xrtGetError() == NULL) ||
			 (xrtErrorKind(xrtGetError()) != XERR_AGAIN) ) {
			(void)xrtClose(File);
			return 11;
		}
		xrtClearError();
	} else if ( !bLocked || !xrtFileUnlock(File) ) {
		(void)xrtClose(File);
		return 12;
	}
	return xrtClose(File) ? 0 : 13;
}



/* 在独立进程中运行锁竞争检查。 */
static bool testFileLockRunChild(cstr sPath, bool bBusy)
{
	#if defined(_WIN32) || defined(_WIN64)
		char sExecutable[4096];
		char sCommand[8192];
		STARTUPINFOA Startup;
		PROCESS_INFORMATION Process;
		DWORD iExitCode;
		DWORD iSize = GetModuleFileNameA(NULL,
			sExecutable, (DWORD)sizeof(sExecutable));

		if ( (iSize == 0u) || (iSize >= sizeof(sExecutable)) ) {
			return false;
		}
		(void)snprintf(sCommand, sizeof(sCommand),
			"\"%s\" child %s \"%s\"", sExecutable,
			bBusy ? "busy" : "free", sPath);
		memset(&Startup, 0, sizeof(Startup));
		memset(&Process, 0, sizeof(Process));
		Startup.cb = sizeof(Startup);
		if ( !CreateProcessA(NULL, sCommand, NULL, NULL,
			FALSE, 0u, NULL, NULL, &Startup, &Process) ) {
			return false;
		}
		(void)WaitForSingleObject(Process.hProcess, INFINITE);
		if ( !GetExitCodeProcess(Process.hProcess, &iExitCode) ) {
			iExitCode = UINT32_MAX;
		}
		(void)CloseHandle(Process.hThread);
		(void)CloseHandle(Process.hProcess);
		return iExitCode == 0u;
	#else
		pid_t iChild = fork();
		int iStatus;

		if ( iChild < 0 ) {
			return false;
		}
		if ( iChild == 0 ) {
			_exit(testFileLockChild(sPath, bBusy));
		}
		if ( waitpid(iChild, &iStatus, 0) != iChild ) {
			return false;
		}
		return WIFEXITED(iStatus) && (WEXITSTATUS(iStatus) == 0);
	#endif
}



/* 文件锁必须跨进程冲突，并支持区间、整文件和参数边界。 */
int main(int iArgumentCount, char** pArguments)
{
	str sDirectory;
	str sPath;
	xfile File;

	if ( iArgumentCount == 4 ) {
		return testFileLockChild(
			pArguments[3], strcmp(pArguments[2], "busy") == 0);
	}
	sDirectory = xrtPathTemp();
	testRequire(sDirectory != NULL,
		"file lock temporary directory query failed");
	sPath = xrtPathJoin(sDirectory, "xrt-file-lock.tmp");
	xrtFree(sDirectory);
	testRequire(sPath != NULL, "file lock path allocation failed");
	(void)xrtFileDelete(sPath);
	xrtClearError();
	File = xrtOpen(sPath, XFILE_READ | XFILE_WRITE |
		XFILE_CREATE | XFILE_EXCLUSIVE);
	testRequire(File != NULL, "file lock fixture open failed");
	testRequire(xrtWriteFull(File, "lock", 4u, NULL),
		"file lock fixture write failed");

	testRequire(xrtFileLockRange(File,
		XFILE_LOCK_SHARED, 1u, 2u, true),
		"shared file range lock failed");
	testRequire(testFileLockRunChild(sPath, true),
		"cross-process file lock did not conflict");
	testRequire(xrtFileUnlockRange(File, 1u, 2u),
		"shared file range unlock failed");
	testRequire(testFileLockRunChild(sPath, false),
		"file lock remained busy after unlock");

	testRequire(xrtFileLock(File,
		XFILE_LOCK_EXCLUSIVE, true), "whole-file lock failed");
	testRequire(xrtFileUnlock(File), "whole-file unlock failed");
	testRequire(!xrtFileLockRange(File,
		XFILE_LOCK_EXCLUSIVE, (uint64)INT64_MAX, 2u, false),
		"file lock accepted an overflowing range");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"invalid file lock range reported the wrong error");
	xrtClearError();

	testRequire(xrtClose(File), "file lock fixture close failed");
	File = xrtOpen(sPath, XFILE_WRITE | XFILE_APPEND);
	testRequire(File != NULL,
		"append lock fixture open failed");
	testRequire(xrtFileLock(File,
		XFILE_LOCK_EXCLUSIVE, true),
		"append handle exclusive lock failed");
	testRequire(xrtFileUnlock(File),
		"append handle exclusive unlock failed");
	testRequire(xrtClose(File),
		"append lock fixture close failed");
	testRequire(xrtFileDelete(sPath), "file lock fixture delete failed");
	xrtFree(sPath);
	return 0;
}
