#include "../internal/xrt_process.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <shellapi.h>
#endif



#if defined(XRT_FEATURE_PROCESS_OPEN)

/* 把默认程序启动失败包装为稳定的 Process Open 错误。 */
static void __xrtProcessOpenError(cstr sMessage)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : XERR_IO;
	Desc.Code = XPROCESS_ERROR_OPEN;
	Desc.SystemCode = pCause != NULL ?
		xrtErrorSystemCode(pCause) : 0;
	Desc.Domain = "xrt.process";
	Desc.Operation = "open";
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



#if defined(_WIN32) || defined(_WIN64)
/* 使用 Shell 关联处理器打开目标，不保留可能返回的进程句柄。 */
static bool __xrtProcessOpenPlatform(cstr sTarget)
{
	HINSTANCE hResult;
	wchar_t* sTarget16 = (wchar_t*)xrtUtf8To16(sTarget, NULL);
	int iError;

	if ( sTarget16 == NULL ) {
		__xrtProcessOpenError(
			"process open target could not be converted to UTF-16"
		);
		return false;
	}
	hResult = ShellExecuteW(
		NULL,
		L"open",
		sTarget16,
		NULL,
		NULL,
		SW_SHOWNORMAL
	);
	if ( ((INT_PTR)hResult) > 32 ) {
		xrtFree(sTarget16);
		return true;
	}
	iError = (int)GetLastError();
	if ( iError == 0 ) {
		iError = (int)(INT_PTR)hResult;
	}
	xrtFree(sTarget16);
	__xrtProcessErrorSet(
		iError != 0 ? __xrtSystemErrorKind(iError) : XERR_IO,
		XPROCESS_ERROR_OPEN,
		"open",
		"system default application rejected the target",
		iError
	);
	return false;
}
#else
/* POSIX 桌面入口作为直接子进程启动，目标永远是独立参数。 */
static bool __xrtProcessOpenPlatform(cstr sTarget)
{
	const cstr pArgs[] = { sTarget };
	xprocessconfig Config;
	xprocess* pProcess;

	if ( !xrtProcessConfigInit(&Config) ) {
		__xrtProcessOpenError(
			"process open configuration could not be initialized"
		);
		return false;
	}
	#if defined(__APPLE__) && defined(__MACH__)
		Config.Program = "/usr/bin/open";
	#else
		Config.Program = "xdg-open";
	#endif
	Config.Args = pArgs;
	Config.ArgCount = 1u;
	Config.Stdin.Mode = XPROCESS_IO_NULL;
	Config.Stdout.Mode = XPROCESS_IO_NULL;
	Config.Stderr.Mode = XPROCESS_IO_NULL;
	pProcess = xrtProcessSpawn(&Config);
	if ( pProcess == NULL ) {
		__xrtProcessOpenError(
			"system default application launcher could not start"
		);
		return false;
	}
	xrtProcessDestroy(pProcess);
	return true;
}
#endif



/* 校验目标后交给平台默认程序机制。 */
XRT_API bool xrtProcessOpen(cstr sTarget)
{
	xstrview Target;

	if ( (sTarget == NULL) || (sTarget[0] == 0) ) {
		__xrtProcessErrorSet(
			XERR_ARGUMENT,
			XPROCESS_ERROR_OPEN,
			"open",
			"process open target is empty",
			0
		);
		return false;
	}
	Target.Data = sTarget;
	Target.Size = strlen(sTarget);
	if ( !xrtUtf8Valid(Target, NULL) ) {
		__xrtProcessErrorSet(
			XERR_VALUE,
			XPROCESS_ERROR_OPEN,
			"open",
			"process open target is not valid UTF-8",
			0
		);
		return false;
	}
	return __xrtProcessOpenPlatform(sTarget);
}

#endif
