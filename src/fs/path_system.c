#if !defined(_WIN32) && !defined(_WIN64) && !defined(_POSIX_C_SOURCE)
	#define _POSIX_C_SOURCE 200809L
#endif

#if !defined(_WIN32) && !defined(_WIN64) && !defined(_XOPEN_SOURCE)
	#define _XOPEN_SOURCE 700
#endif

#include "../internal/xrt_path.h"

#include <errno.h>

#if !defined(_WIN32) && !defined(_WIN64)
	#include <pwd.h>
	#include <sys/types.h>
	#include <unistd.h>
	#if defined(__APPLE__)
		#include <mach-o/dyld.h>
	#endif
#endif

#if (defined(_WIN32) || defined(_WIN64)) && defined(__TINYC__)
	/* TinyCC 的旧 Win32 头缺少 Vista 起提供的宽字符物理路径查询原型。 */
	DWORD WINAPI GetFinalPathNameByHandleW(HANDLE hFile,
		LPWSTR sPath, DWORD iCapacity, DWORD iFlags);
#endif



#if defined(XRT_FEATURE_PATH_SYSTEM)

/* 设置带系统代码的路径错误。 */
static void __xrtPathSystemError(cstr sOperation, cstr sMessage, int iCode)
{
	__xrtPathSetError(__xrtSystemErrorKind(iCode), XPATH_ERROR_SYSTEM,
		sOperation, sMessage, iCode);
}



#if defined(_WIN32) || defined(_WIN64)

/* 严格把零结尾 UTF-8 路径转换为 Windows UTF-16。 */
uint16* __xrtPathToWide(cstr sPath, size_t* pSize)
{
	if ( sPath == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return xrtUtf8ViewTo16(xrtStrView(sPath), XUTF_STRICT, pSize);
}



/* 严格把 Windows UTF-16 查询结果转换为 UTF-8。 */
str __xrtPathFromWide(const wchar_t* sPath, size_t iSize)
{
	return xrtUtf16ViewTo8(
		(xutf16view){ (const uint16*)sPath, iSize }, XUTF_STRICT, NULL);
}



/* 按 DWORD 字符容量分配 UTF-16 缓冲，并在 Win32 检查乘法溢出。 */
static wchar_t* __xrtPathWideAlloc(DWORD iCapacity)
{
	#if !defined(_WIN64)
		if ( iCapacity > (DWORD)(SIZE_MAX / sizeof(wchar_t)) ) {
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
	#endif
	return (wchar_t*)xrtMalloc((size_t)iCapacity * sizeof(wchar_t));
}



/* 动态查询当前工作目录，不受 MAX_PATH 限制。 */
XRT_API str xrtPathCwd(void)
{
	DWORD iCapacity = GetCurrentDirectoryW(0, NULL);

	if ( iCapacity == 0 ) {
		int iCode = (int)GetLastError();

		__xrtPathSystemError("cwd", "failed to query the current directory", iCode);
		return NULL;
	}
	for ( ;; ) {
		wchar_t* sWide;
		DWORD iSize;
		str sResult;

		sWide = __xrtPathWideAlloc(iCapacity);
		if ( sWide == NULL ) {
			return NULL;
		}
		iSize = GetCurrentDirectoryW(iCapacity, sWide);
		if ( iSize == 0 ) {
			int iCode = (int)GetLastError();

			xrtFree(sWide);
			__xrtPathSystemError("cwd", "failed to read the current directory", iCode);
			return NULL;
		}
		if ( iSize < iCapacity ) {
			sResult = __xrtPathFromWide(sWide, (size_t)iSize);
			xrtFree(sWide);
			return sResult;
		}
		xrtFree(sWide);
		if ( iSize == UINT32_MAX ) {
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
		iCapacity = iSize + 1u;
	}
}



/* 修改 Windows 进程当前工作目录。 */
XRT_API bool xrtPathSetCwd(cstr sPath)
{
	uint16* pWide = __xrtPathToWide(sPath, NULL);

	if ( pWide == NULL ) {
		return false;
	}
	if ( !SetCurrentDirectoryW((const wchar_t*)pWide) ) {
		int iCode = (int)GetLastError();

		xrtFree(pWide);
		__xrtPathSystemError("set-cwd", "failed to change the current directory", iCode);
		return false;
	}
	xrtFree(pWide);
	return true;
}



/* 使用 GetFullPathNameW 处理驱动器相对和根相对 Windows 路径。 */
XRT_API str xrtPathAbs(cstr sPath)
{
	uint16* pInput;
	DWORD iCapacity;

	if ( sPath == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( sPath[0] == 0 ) {
		return xrtPathCwd();
	}
	pInput = __xrtPathToWide(sPath, NULL);
	if ( pInput == NULL ) {
		return NULL;
	}
	iCapacity = GetFullPathNameW((const wchar_t*)pInput, 0, NULL, NULL);
	if ( iCapacity == 0 ) {
		int iCode = (int)GetLastError();

		xrtFree(pInput);
		__xrtPathSystemError("absolute", "failed to resolve an absolute path", iCode);
		return NULL;
	}
	for ( ;; ) {
		wchar_t* sWide;
		DWORD iSize;
		str sUtf8;
		str sResult;

		sWide = __xrtPathWideAlloc(iCapacity);
		if ( sWide == NULL ) {
			xrtFree(pInput);
			return NULL;
		}
		iSize = GetFullPathNameW((const wchar_t*)pInput, iCapacity, sWide, NULL);
		if ( iSize == 0 ) {
			int iCode = (int)GetLastError();

			xrtFree(sWide);
			xrtFree(pInput);
			__xrtPathSystemError("absolute", "failed to resolve an absolute path", iCode);
			return NULL;
		}
		if ( iSize < iCapacity ) {
			sUtf8 = __xrtPathFromWide(sWide, (size_t)iSize);
			xrtFree(sWide);
			xrtFree(pInput);
			if ( sUtf8 == NULL ) {
				return NULL;
			}
			sResult = xrtPathClean(xrtStrView(sUtf8), XPATH_WINDOWS);
			xrtFree(sUtf8);
			return sResult;
		}
		xrtFree(sWide);
		if ( iSize == UINT32_MAX ) {
			xrtFree(pInput);
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
		iCapacity = iSize + 1u;
	}
}



/* 打开路径并读取 Windows 解析全部重解析点后的物理名称。 */
XRT_API str xrtPathReal(cstr sPath)
{
	uint16* pInput;
	HANDLE hPath;
	DWORD iCapacity;
	/* 零值同时表示规范文件名和 DOS 卷名，也兼容缺少新宏的 TinyCC 头。 */
	const DWORD iFlags = 0u;

	if ( (sPath == NULL) || (sPath[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pInput = __xrtPathToWide(sPath, NULL);
	if ( pInput == NULL ) {
		return NULL;
	}
	hPath = CreateFileW((const wchar_t*)pInput, FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
		OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if ( hPath == INVALID_HANDLE_VALUE ) {
		int iCode = (int)GetLastError();

		xrtFree(pInput);
		__xrtPathSystemError("real", "failed to open the path for resolution", iCode);
		return NULL;
	}
	xrtFree(pInput);
	iCapacity = GetFinalPathNameByHandleW(hPath, NULL, 0, iFlags);
	if ( iCapacity == 0u ) {
		int iCode = (int)GetLastError();

		CloseHandle(hPath);
		__xrtPathSystemError("real", "failed to query the physical path", iCode);
		return NULL;
	}
	for ( ;; ) {
		wchar_t* sWide = __xrtPathWideAlloc(iCapacity);
		DWORD iSize;
		str sResult;

		if ( sWide == NULL ) {
			CloseHandle(hPath);
			return NULL;
		}
		iSize = GetFinalPathNameByHandleW(hPath, sWide, iCapacity, iFlags);
		if ( iSize == 0u ) {
			int iCode = (int)GetLastError();

			xrtFree(sWide);
			CloseHandle(hPath);
			__xrtPathSystemError("real", "failed to read the physical path", iCode);
			return NULL;
		}
		if ( iSize < iCapacity ) {
			CloseHandle(hPath);
			sResult = __xrtPathFromWide(sWide, (size_t)iSize);
			xrtFree(sWide);
			return sResult;
		}
		xrtFree(sWide);
		if ( iSize == UINT32_MAX ) {
			CloseHandle(hPath);
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
		iCapacity = iSize + 1u;
	}
}



/* 读取 Windows 用户主目录环境值，不用当前目录伪装缺失结果。 */
XRT_API str xrtPathHome(void)
{
	DWORD iCapacity = GetEnvironmentVariableW(L"USERPROFILE", NULL, 0);

	if ( iCapacity == 0 ) {
		int iCode = (int)GetLastError();

		if ( iCode == ERROR_ENVVAR_NOT_FOUND ) {
			__xrtPathSetError(XERR_NOT_FOUND, XPATH_ERROR_SYSTEM, "home",
				"the user profile directory is not available", iCode);
		} else {
			__xrtPathSystemError("home", "failed to query the user profile directory", iCode);
		}
		return NULL;
	}
	for ( ;; ) {
		wchar_t* sWide = __xrtPathWideAlloc(iCapacity);
		DWORD iSize;
		str sResult;

		if ( sWide == NULL ) {
			return NULL;
		}
		SetLastError(ERROR_SUCCESS);
		iSize = GetEnvironmentVariableW(L"USERPROFILE", sWide, iCapacity);
		if ( iSize == 0 ) {
			int iCode = (int)GetLastError();

			xrtFree(sWide);
			if ( iCode == ERROR_SUCCESS ) {
				__xrtPathSetError(XERR_NOT_FOUND, XPATH_ERROR_SYSTEM, "home",
					"the user profile directory is empty", 0);
			} else {
				__xrtPathSystemError("home",
					"failed to read the user profile directory", iCode);
			}
			return NULL;
		}
		if ( iSize < iCapacity ) {
			sResult = __xrtPathFromWide(sWide, (size_t)iSize);
			xrtFree(sWide);
			return sResult;
		}
		xrtFree(sWide);
		iCapacity = iSize;
	}
}



/* 动态读取 Windows 临时目录。 */
XRT_API str xrtPathTemp(void)
{
	DWORD iCapacity = GetTempPathW(0, NULL);

	if ( iCapacity == 0 ) {
		int iCode = (int)GetLastError();

		__xrtPathSystemError("temp", "failed to query the temporary directory", iCode);
		return NULL;
	}
	for ( ;; ) {
		wchar_t* sWide;
		DWORD iSize;
		str sUtf8;
		str sResult;

		sWide = __xrtPathWideAlloc(iCapacity);
		if ( sWide == NULL ) {
			return NULL;
		}
		iSize = GetTempPathW(iCapacity, sWide);
		if ( iSize == 0 ) {
			int iCode = (int)GetLastError();

			xrtFree(sWide);
			__xrtPathSystemError("temp", "failed to read the temporary directory", iCode);
			return NULL;
		}
		if ( iSize < iCapacity ) {
			sUtf8 = __xrtPathFromWide(sWide, (size_t)iSize);
			xrtFree(sWide);
			if ( sUtf8 == NULL ) {
				return NULL;
			}
			sResult = xrtPathClean(xrtStrView(sUtf8), XPATH_WINDOWS);
			xrtFree(sUtf8);
			return sResult;
		}
		xrtFree(sWide);
		if ( iSize == UINT32_MAX ) {
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
		iCapacity = iSize + 1u;
	}
}



/* 动态读取当前 Windows 可执行文件路径。 */
XRT_API str xrtPathExecutable(void)
{
	DWORD iCapacity = 256;

	for ( ;; ) {
		wchar_t* sWide;
		DWORD iSize;
		str sResult;

		sWide = __xrtPathWideAlloc(iCapacity);
		if ( sWide == NULL ) {
			return NULL;
		}
		SetLastError(ERROR_SUCCESS);
		iSize = GetModuleFileNameW(NULL, sWide, iCapacity);
		if ( iSize == 0 ) {
			int iCode = (int)GetLastError();

			xrtFree(sWide);
			__xrtPathSystemError("executable", "failed to read the executable path", iCode);
			return NULL;
		}
		if ( iSize < iCapacity ) {
			sResult = __xrtPathFromWide(sWide, (size_t)iSize);
			xrtFree(sWide);
			return sResult;
		}
		xrtFree(sWide);
		if ( iCapacity > (UINT32_MAX / 2u) ) {
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
		iCapacity *= 2u;
	}
}

#else

/* 动态读取 POSIX 当前工作目录，不依赖 PATH_MAX。 */
XRT_API str xrtPathCwd(void)
{
	size_t iCapacity = 256;

	for ( ;; ) {
		str sResult = (str)xrtMalloc(iCapacity);

		if ( sResult == NULL ) {
			return NULL;
		}
		errno = 0;
		if ( getcwd(sResult, iCapacity) != NULL ) {
			return sResult;
		}
		if ( errno != ERANGE ) {
			int iCode = errno;

			xrtFree(sResult);
			__xrtPathSystemError("cwd", "failed to read the current directory", iCode);
			return NULL;
		}
		xrtFree(sResult);
		if ( iCapacity > (SIZE_MAX / 2u) ) {
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
		iCapacity *= 2u;
	}
}



/* 修改 POSIX 进程当前工作目录。 */
XRT_API bool xrtPathSetCwd(cstr sPath)
{
	if ( sPath == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( chdir(sPath) != 0 ) {
		int iCode = errno;

		__xrtPathSystemError("set-cwd", "failed to change the current directory", iCode);
		return false;
	}
	return true;
}



/* 使用当前目录构造 POSIX 绝对路径。 */
XRT_API str xrtPathAbs(cstr sPath)
{
	str sCwd;
	str sResult;

	if ( sPath == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( sPath[0] == 0 ) {
		return xrtPathCwd();
	}
	if ( xrtPathIsAbs(sPath) ) {
		return xrtPathClean(xrtStrView(sPath), XPATH_POSIX);
	}
	sCwd = xrtPathCwd();
	if ( sCwd == NULL ) {
		return NULL;
	}
	sResult = xrtPathJoin(sCwd, sPath);
	xrtFree(sCwd);
	return sResult;
}



/* 使用 realpath 解析 POSIX 符号链接并复制到 XRT 分配器。 */
XRT_API str xrtPathReal(cstr sPath)
{
	char* sNative;
	str sResult;

	if ( (sPath == NULL) || (sPath[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	errno = 0;
	sNative = realpath(sPath, NULL);
	if ( sNative == NULL ) {
		int iCode = errno;

		__xrtPathSystemError("real", "failed to resolve the physical path", iCode);
		return NULL;
	}
	sResult = xrtStrDup(sNative);
	free(sNative);
	return sResult;
}



/* 读取环境变量或账户数据库中的 POSIX 用户主目录。 */
XRT_API str xrtPathHome(void)
{
	cstr sHome = getenv("HOME");
	long iHint;
	size_t iCapacity;

	if ( (sHome != NULL) && (sHome[0] != 0) ) {
		return xrtStrDup(sHome);
	}
	iHint = sysconf(_SC_GETPW_R_SIZE_MAX);
	iCapacity = iHint > 0 ? (size_t)iHint : 1024u;
	for ( ;; ) {
		char* sBuffer = (char*)xrtMalloc(iCapacity);
		struct passwd Account;
		struct passwd* pResult = NULL;
		int iCode;

		if ( sBuffer == NULL ) {
			return NULL;
		}
		iCode = getpwuid_r(getuid(), &Account, sBuffer, iCapacity, &pResult);
		if ( (iCode == 0) && (pResult != NULL) &&
			 (pResult->pw_dir != NULL) && (pResult->pw_dir[0] != 0) ) {
			str sResult = xrtStrDup(pResult->pw_dir);

			xrtFree(sBuffer);
			return sResult;
		}
		xrtFree(sBuffer);
		if ( (iCode == 0) && (pResult == NULL) ) {
			__xrtPathSetError(XERR_NOT_FOUND, XPATH_ERROR_SYSTEM, "home",
				"the current user has no home directory entry", 0);
			return NULL;
		}
		if ( iCode != ERANGE ) {
			__xrtPathSystemError("home",
				"the user home directory is not available", iCode);
			return NULL;
		}
		if ( iCapacity > (SIZE_MAX / 2u) ) {
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
		iCapacity *= 2u;
	}
}



/* 返回 POSIX 临时目录环境值或标准回退目录。 */
XRT_API str xrtPathTemp(void)
{
	cstr sPath = getenv("TMPDIR");

	if ( (sPath == NULL) || (sPath[0] == 0) ) {
		sPath = "/tmp";
	}
	return xrtPathClean(xrtStrView(sPath), XPATH_POSIX);
}



#if defined(__linux__)
	/* 动态读取 procfs 中的 Linux 可执行文件符号链接。 */
	static str __xrtPathLinuxExecutable(void)
	{
		size_t iCapacity = 256;

		for ( ;; ) {
			str sResult = (str)xrtMalloc(iCapacity + 1u);
			ssize_t iSize;

			if ( sResult == NULL ) {
				return NULL;
			}
			iSize = readlink("/proc/self/exe", sResult, iCapacity);
			if ( iSize < 0 ) {
				int iCode = errno;

				xrtFree(sResult);
				__xrtPathSystemError("executable",
					"failed to read the executable path", iCode);
				return NULL;
			}
			if ( (size_t)iSize < iCapacity ) {
				sResult[iSize] = 0;
				return sResult;
			}
			xrtFree(sResult);
			if ( iCapacity > ((SIZE_MAX - 1u) / 2u) ) {
				__xrtErrorSetSizeOverflow();
				return NULL;
			}
			iCapacity *= 2u;
		}
	}
#endif



/* 返回当前 POSIX 可执行文件路径。 */
XRT_API str xrtPathExecutable(void)
{
	#if defined(__linux__)
		return __xrtPathLinuxExecutable();
	#elif defined(__APPLE__)
		uint32_t iCapacity = 0;
		char* sResult;
		str sAbsolute;

		(void)_NSGetExecutablePath(NULL, &iCapacity);
		if ( iCapacity == 0 ) {
			__xrtPathSetError(XERR_UNSUPPORTED, XPATH_ERROR_SYSTEM,
				"executable", "the executable path is not available", 0);
			return NULL;
		}
		sResult = (char*)xrtMalloc((size_t)iCapacity);
		if ( sResult == NULL ) {
			return NULL;
		}
		if ( _NSGetExecutablePath(sResult, &iCapacity) != 0 ) {
			xrtFree(sResult);
			__xrtPathSetError(XERR_IO, XPATH_ERROR_SYSTEM,
				"executable", "failed to read the executable path", 0);
			return NULL;
		}
		sAbsolute = xrtPathAbs(sResult);
		xrtFree(sResult);
		return sAbsolute;
	#else
		__xrtPathSetError(XERR_UNSUPPORTED, XPATH_ERROR_SYSTEM,
			"executable", "the platform does not expose the executable path", 0);
		return NULL;
	#endif
}

#endif



/* 把两个绝对路径交给纯词法相对路径原语。 */
XRT_API str xrtPathRel(cstr sBase, cstr sTarget)
{
	str sBaseAbs;
	str sTargetAbs;
	str sResult;

	if ( (sBase == NULL) || (sTarget == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	sBaseAbs = xrtPathAbs(sBase);
	if ( sBaseAbs == NULL ) {
		return NULL;
	}
	sTargetAbs = xrtPathAbs(sTarget);
	if ( sTargetAbs == NULL ) {
		xrtFree(sBaseAbs);
		return NULL;
	}
	sResult = xrtPathRelative(xrtStrView(sBaseAbs),
		xrtStrView(sTargetAbs), XPATH_NATIVE);
	xrtFree(sBaseAbs);
	xrtFree(sTargetAbs);
	return sResult;
}



/* 返回当前可执行文件所在目录。 */
XRT_API str xrtPathAppDir(void)
{
	str sExecutable = xrtPathExecutable();
	str sResult;

	if ( sExecutable == NULL ) {
		return NULL;
	}
	sResult = xrtPathParent(sExecutable);
	xrtFree(sExecutable);
	if ( (sResult != NULL) && (sResult[0] == 0) ) {
		xrtFree(sResult);
		__xrtPathSetError(XERR_NOT_FOUND, XPATH_ERROR_SYSTEM,
			"app-dir", "the executable path has no parent directory", 0);
		return NULL;
	}
	return sResult;
}

#endif
