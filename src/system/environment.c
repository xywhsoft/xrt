#if !defined(_WIN32) && !defined(_WIN64) && !defined(_POSIX_C_SOURCE)
	#define _POSIX_C_SOURCE 200809L
#endif

#include "../internal/xrt_internal.h"

#include <xrt/charset.h>

#include <errno.h>



#if defined(XRT_FEATURE_ENVIRONMENT)

/* 环境变量名称和值必须使用跨平台一致的严格 UTF-8。 */
static bool __xrtEnvUtf8(cstr sText, int32 iCode, cstr sOperation, cstr sMessage)
{
	xstrview Text;

	if ( sText == NULL ) {
		__xrtErrorSetDetail(
			XERR_ARGUMENT,
			"xrt.environment",
			iCode,
			sOperation,
			sMessage,
			NULL
		);
		return false;
	}
	Text.Data = sText;
	Text.Size = strlen(sText);
	if ( !xrtUtf8Valid(Text, NULL) ) {
		__xrtErrorWrapDetail(
			XERR_VALUE,
			"xrt.environment",
			iCode,
			sOperation,
			sMessage
		);
		return false;
	}
	return true;
}



/* 名称必须非空且不能包含系统使用的赋值分隔符。 */
static bool __xrtEnvName(cstr sName)
{
	if ( !__xrtEnvUtf8(
			sName,
			XENV_ERROR_NAME,
			"name",
			"environment variable name is not valid UTF-8"
		) ) {
		return false;
	}
	if ( (sName[0] == '\0') || (strchr(sName, '=') != NULL) ) {
		__xrtErrorSetDetail(
			XERR_ARGUMENT,
			"xrt.environment",
			XENV_ERROR_NAME,
			"name",
			"environment variable name is empty or contains '='",
			NULL
		);
		return false;
	}
	return true;
}



/* 环境变量值允许为空，但必须是严格 UTF-8。 */
static bool __xrtEnvValue(cstr sValue)
{
	return __xrtEnvUtf8(
		sValue,
		XENV_ERROR_VALUE,
		"value",
		"environment variable value is not valid UTF-8"
	);
}



#if defined(_WIN32) || defined(_WIN64)

/* 把 UTF-8 名称转换为 Windows 环境 API 使用的 UTF-16。 */
static uint16* __xrtEnvNameWide(cstr sName)
{
	uint16* pName = xrtUtf8To16(sName, NULL);

	if ( pName == NULL ) {
		__xrtErrorWrapDetail(
			XERR_VALUE,
			"xrt.environment",
			XENV_ERROR_NAME,
			"name",
			"environment variable name conversion failed"
		);
	}
	return pName;
}



/* 使用 Windows 宽字符 API 读取并转换环境变量。 */
static bool __xrtEnvLookupPlatform(cstr sName, str* psValue)
{
	uint16* pName = __xrtEnvNameWide(sName);
	uint16* pValue = NULL;
	DWORD iCapacity;
	DWORD iResult;
	DWORD iCode;

	if ( pName == NULL ) {
		return false;
	}
	SetLastError(ERROR_SUCCESS);
	iCapacity = GetEnvironmentVariableW((LPCWSTR)pName, NULL, 0);
	if ( iCapacity == 0 ) {
		iCode = GetLastError();
		if ( iCode == ERROR_ENVVAR_NOT_FOUND ) {
			xrtFree(pName);
			return true;
		}
		if ( iCode == ERROR_SUCCESS ) {
			*psValue = (str)xrtMalloc(1);
			if ( *psValue != NULL ) {
				(*psValue)[0] = '\0';
			}
			xrtFree(pName);
			return *psValue != NULL;
		}
		xrtFree(pName);
		__xrtErrorSetSystem(
			"xrt.environment",
			XENV_ERROR_SYSTEM,
			"lookup",
			(int)iCode,
			"environment variable query failed"
		);
		return false;
	}

	for ( ;; ) {
		size_t iBytes = (size_t)iCapacity * sizeof(uint16);

		if ( (iCapacity != 0) &&
			((iBytes / sizeof(uint16)) != (size_t)iCapacity) ) {
			xrtFree(pName);
			__xrtErrorSetSizeOverflow();
			return false;
		}
		pValue = (uint16*)xrtMalloc(iBytes);
		if ( pValue == NULL ) {
			xrtFree(pName);
			return false;
		}
		SetLastError(ERROR_SUCCESS);
		iResult = GetEnvironmentVariableW(
			(LPCWSTR)pName,
			(LPWSTR)pValue,
			iCapacity
		);
		if ( (iResult != 0) && (iResult < iCapacity) ) {
			break;
		}
		iCode = GetLastError();
		if ( iResult >= iCapacity ) {
			xrtFree(pValue);
			pValue = NULL;
			iCapacity = iResult;
			continue;
		}
		xrtFree(pValue);
		xrtFree(pName);
		if ( iCode == ERROR_ENVVAR_NOT_FOUND ) {
			return true;
		}
		if ( iCode == ERROR_SUCCESS ) {
			*psValue = (str)xrtMalloc(1);
			if ( *psValue != NULL ) {
				(*psValue)[0] = '\0';
			}
			return *psValue != NULL;
		}
		__xrtErrorSetSystem(
			"xrt.environment",
			XENV_ERROR_SYSTEM,
			"lookup",
			(int)iCode,
			"environment variable read failed"
		);
		return false;
	}
	xrtFree(pName);
	*psValue = xrtUtf16To8(pValue, NULL);
	xrtFree(pValue);
	if ( *psValue == NULL ) {
		__xrtErrorWrapDetail(
			XERR_VALUE,
			"xrt.environment",
			XENV_ERROR_VALUE,
			"lookup",
			"environment variable value conversion failed"
		);
		return false;
	}
	return true;
}



/* 使用 Windows 宽字符 API 设置或删除环境变量。 */
static bool __xrtEnvWritePlatform(cstr sName, cstr sValue, bool bRemove)
{
	uint16* pName = __xrtEnvNameWide(sName);
	uint16* pValue = NULL;
	BOOL bResult;
	DWORD iCode;

	if ( pName == NULL ) {
		return false;
	}
	if ( !bRemove ) {
		pValue = xrtUtf8To16(sValue, NULL);
		if ( pValue == NULL ) {
			xrtFree(pName);
			__xrtErrorWrapDetail(
				XERR_VALUE,
				"xrt.environment",
				XENV_ERROR_VALUE,
				"value",
				"environment variable value conversion failed"
			);
			return false;
		}
	}
	bResult = SetEnvironmentVariableW(
		(LPCWSTR)pName,
		bRemove ? NULL : (LPCWSTR)pValue
	);
	iCode = bResult ? ERROR_SUCCESS : GetLastError();
	xrtFree(pValue);
	xrtFree(pName);
	if ( bResult ) {
		return true;
	}
	if ( bRemove && (iCode == ERROR_ENVVAR_NOT_FOUND) ) {
		return true;
	}
	__xrtErrorSetSystem(
		"xrt.environment",
		XENV_ERROR_SYSTEM,
		bRemove ? "remove" : "set",
		(int)iCode,
		bRemove ?
			"environment variable removal failed" :
			"environment variable update failed"
	);
	return false;
}

#else

/* TinyCC POSIX 的内部短锁需要静态 pthread 初始化器。 */
#if defined(__TINYC__)
static xrt_spinlock __xrtEnvLock = { PTHREAD_MUTEX_INITIALIZER };
#else
static xrt_spinlock __xrtEnvLock = { 0 };
#endif



/* 在不持有环境锁时分配，再重新确认并复制当前值。 */
static bool __xrtEnvLookupPlatform(cstr sName, str* psValue)
{
	for ( ;; ) {
		const char* sValue;
		size_t iCapacity;
		size_t iSize;
		str sCopy;

		__xrtSpinLock(&__xrtEnvLock);
		sValue = getenv(sName);
		if ( sValue == NULL ) {
			__xrtSpinUnlock(&__xrtEnvLock);
			return true;
		}
		iCapacity = strlen(sValue) + 1u;
		__xrtSpinUnlock(&__xrtEnvLock);

		sCopy = (str)xrtMalloc(iCapacity);
		if ( sCopy == NULL ) {
			return false;
		}
		__xrtSpinLock(&__xrtEnvLock);
		sValue = getenv(sName);
		if ( sValue == NULL ) {
			__xrtSpinUnlock(&__xrtEnvLock);
			xrtFree(sCopy);
			return true;
		}
		iSize = strlen(sValue) + 1u;
		if ( iSize > iCapacity ) {
			__xrtSpinUnlock(&__xrtEnvLock);
			xrtFree(sCopy);
			continue;
		}
		memcpy(sCopy, sValue, iSize);
		__xrtSpinUnlock(&__xrtEnvLock);
		if ( !__xrtEnvUtf8(
				sCopy,
				XENV_ERROR_VALUE,
				"lookup",
				"environment variable value is not valid UTF-8"
			) ) {
			xrtFree(sCopy);
			return false;
		}
		*psValue = sCopy;
		return true;
	}
}



/* 使用 POSIX 进程环境设置或删除变量。 */
static bool __xrtEnvWritePlatform(cstr sName, cstr sValue, bool bRemove)
{
	int iResult;
	int iCode;

	__xrtSpinLock(&__xrtEnvLock);
	if ( bRemove ) {
		iResult = unsetenv(sName);
	} else {
		iResult = setenv(sName, sValue, 1);
	}
	iCode = iResult == 0 ? 0 : errno;
	__xrtSpinUnlock(&__xrtEnvLock);
	if ( iResult == 0 ) {
		return true;
	}
	__xrtErrorSetSystem(
		"xrt.environment",
		XENV_ERROR_SYSTEM,
		bRemove ? "remove" : "set",
		iCode,
		bRemove ?
			"environment variable removal failed" :
			"environment variable update failed"
	);
	return false;
}

#endif



/* 读取变量并明确区分缺失与调用失败。 */
XRT_API bool xrtEnvLookup(cstr sName, str* psValue)
{
	if ( psValue == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*psValue = NULL;
	if ( !__xrtEnvName(sName) ) {
		return false;
	}
	return __xrtEnvLookupPlatform(sName, psValue);
}



/* 一次调用返回变量的拥有副本。 */
XRT_API str xrtEnvGet(cstr sName)
{
	str sValue = NULL;

	if ( !xrtEnvLookup(sName, &sValue) ) {
		return NULL;
	}
	return sValue;
}



/* 验证并覆盖一个进程级环境变量。 */
XRT_API bool xrtEnvSet(cstr sName, cstr sValue)
{
	if ( !__xrtEnvName(sName) || !__xrtEnvValue(sValue) ) {
		return false;
	}
	return __xrtEnvWritePlatform(sName, sValue, false);
}



/* 幂等删除一个进程级环境变量。 */
XRT_API bool xrtEnvRemove(cstr sName)
{
	if ( !__xrtEnvName(sName) ) {
		return false;
	}
	return __xrtEnvWritePlatform(sName, NULL, true);
}

#endif
