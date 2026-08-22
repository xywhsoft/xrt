#include "../internal/xrt_net.h"



#if defined(XRT_FEATURE_NET)

/* 设置网络模块结构化错误。 */
void __xrtNetSetError(xerrkind Kind, xneterror Code,
	cstr sOperation, cstr sMessage, int iSystemCode)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.net";
	Desc.Code = (int32)Code;
	Desc.SystemCode = (int32)iSystemCode;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



#if defined(_WIN32) || defined(_WIN64)

/* Winsock 只初始化一次，避免把初始化和清理负担暴露给每个使用者。 */
bool __xrtNetEnsure(void)
{
	static volatile LONG iState = 0;
	static volatile LONG iError = 0;
	WSADATA Data;
	LONG iCurrent;
	int iResult;

	iCurrent = InterlockedCompareExchange(&iState, 1, 0);
	if ( iCurrent == 0 ) {
		iResult = WSAStartup(MAKEWORD(2, 2), &Data);
		if ( iResult != 0 ) {
			InterlockedExchange(&iError, (LONG)iResult);
			InterlockedExchange(&iState, 3);
			__xrtNetSetError(XERR_IO, XNET_ERROR_SYSTEM,
				"initialize", "Winsock initialization failed", iResult);
			return false;
		}
		InterlockedExchange(&iState, 2);
		return true;
	}

	while ( (iCurrent = InterlockedCompareExchange(&iState, 0, 0)) == 1 ) {
		Sleep(0);
	}
	if ( iCurrent == 2 ) {
		return true;
	}

	__xrtNetSetError(XERR_IO, XNET_ERROR_SYSTEM,
		"initialize", "Winsock initialization failed", (int)iError);
	return false;
}

#else

/* POSIX Socket 不需要进程级初始化。 */
bool __xrtNetEnsure(void)
{
	return true;
}

#endif

#endif
