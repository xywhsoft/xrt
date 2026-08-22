#include "../internal/xrt_internal.h"

#if !defined(_WIN32) && !defined(_WIN64)
	#include <pthread.h>
#endif



/* 错误对象内部标志。 */
#define XRT_ERROR_STATIC 0x01u



/* 不可变错误对象的内部布局。 */
struct xerror {
	volatile int32 RefCount;
	uint32 Flags;
	xerrkind Kind;
	int32 Code;
	int32 SystemCode;
	cstr Domain;
	cstr Operation;
	cstr Message;
	cstr Data;
	xerror* Cause;
};



/* 核心错误使用静态对象，保证分配失败时仍能报告。 */
static xerror __xrtOutOfMemoryError = {
	INT32_MAX, XRT_ERROR_STATIC, XERR_MEMORY, 1, 0,
	"xrt.memory", "allocate", "memory allocation failed", "", NULL
};
static xerror __xrtIoErrorStatic = {
	INT32_MAX, XRT_ERROR_STATIC, XERR_IO, 1, 0,
	"xrt.io", "io", "input or output operation failed", "", NULL
};
static xerror __xrtNotFoundErrorStatic = {
	INT32_MAX, XRT_ERROR_STATIC, XERR_NOT_FOUND, 1, 0,
	"xrt.core", "lookup", "requested value was not found", "", NULL
};
static xerror __xrtPermissionErrorStatic = {
	INT32_MAX, XRT_ERROR_STATIC, XERR_PERMISSION, 1, 0,
	"xrt.core", "access", "operation is not permitted", "", NULL
};
static xerror __xrtProtocolErrorStatic = {
	INT32_MAX, XRT_ERROR_STATIC, XERR_PROTOCOL, 1, 0,
	"xrt.core", "protocol", "protocol contract was violated", "", NULL
};
static xerror __xrtInvalidArgumentError = {
	INT32_MAX, XRT_ERROR_STATIC, XERR_ARGUMENT, 1, 0,
	"xrt.core", "validate", "invalid argument", "", NULL
};
static xerror __xrtTypeError = {
	INT32_MAX, XRT_ERROR_STATIC, XERR_TYPE, 1, 0,
	"xrt.core", "type", "value has an incompatible type", "", NULL
};
static xerror __xrtValueError = {
	INT32_MAX, XRT_ERROR_STATIC, XERR_VALUE, 1, 0,
	"xrt.core", "value", "value is not valid for this operation", "", NULL
};
static xerror __xrtInvalidStateError = {
	INT32_MAX, XRT_ERROR_STATIC, XERR_STATE, 1, 0,
	"xrt.core", "state", "operation is not valid in the current state", "", NULL
};
static xerror __xrtSizeOverflowError = {
	INT32_MAX, XRT_ERROR_STATIC, XERR_RANGE, 1, 0,
	"xrt.memory", "size", "memory size overflow", "", NULL
};
static xerror __xrtRangeError = {
	INT32_MAX, XRT_ERROR_STATIC, XERR_RANGE, 2, 0,
	"xrt.core", "index", "index or range is out of bounds", "", NULL
};
static xerror __xrtAgainError = {
	INT32_MAX, XRT_ERROR_STATIC, XERR_AGAIN, 1, 0,
	"xrt.core", "capacity", "operation cannot continue without available capacity", "", NULL
};
static xerror __xrtUnsupportedError = {
	INT32_MAX, XRT_ERROR_STATIC, XERR_UNSUPPORTED, 1, 0,
	"xrt.core", "operation", "operation is not supported", "", NULL
};
static xerror __xrtExistsError = {
	INT32_MAX, XRT_ERROR_STATIC, XERR_EXISTS, 1, 0,
	"xrt.core", "insert", "value already exists", "", NULL
};
static xerror __xrtCancelledError = {
	INT32_MAX, XRT_ERROR_STATIC, XERR_CANCELLED, 1, 0,
	"xrt.core", "cancel", "operation was cancelled", "", NULL
};
static xerror __xrtTimeoutError = {
	INT32_MAX, XRT_ERROR_STATIC, XERR_TIMEOUT, 1, 0,
	"xrt.core", "wait", "operation timed out", "", NULL
};
static xerror __xrtClosedError = {
	INT32_MAX, XRT_ERROR_STATIC, XERR_CLOSED, 1, 0,
	"xrt.core", "close", "resource is closed", "", NULL
};
static xerror __xrtInternalError = {
	INT32_MAX, XRT_ERROR_STATIC, XERR_INTERNAL, 1, 0,
	"xrt.core", "invariant", "internal contract was violated", "", NULL
};



/* 进程级错误处理器不拥有错误对象。 */
static xerrorhandler __xrtErrorHandler = NULL;
static ptr __xrtErrorHandlerData = NULL;
#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64)
static xrt_spinlock __xrtErrorHandlerLock = { PTHREAD_MUTEX_INITIALIZER };
#else
static xrt_spinlock __xrtErrorHandlerLock = { 0 };
#endif



/* 原子地复制当前错误处理器及其用户数据。 */
static void __xrtErrorHandlerGet(xerrorhandler* pHandler, ptr* pUserData)
{
	__xrtSpinLock(&__xrtErrorHandlerLock);
	*pHandler = __xrtErrorHandler;
	*pUserData = __xrtErrorHandlerData;
	__xrtSpinUnlock(&__xrtErrorHandlerLock);
}



#if defined(_WIN32) || defined(_WIN64)

/* Windows 默认错误使用 FLS 析构，显式执行上下文必须跨 Fiber 可见。 */
static DWORD __xrtErrorTlsError = FLS_OUT_OF_INDEXES;
static DWORD __xrtErrorTlsContext = TLS_OUT_OF_INDEXES;
static DWORD __xrtErrorTlsHandler = FLS_OUT_OF_INDEXES;
static volatile LONG __xrtErrorTlsState = 0;



/* 释放线程退出时仍未取走或清除的错误对象。 */
static VOID WINAPI __xrtErrorTlsDestroy(PVOID pValue)
{
	xrtErrorFree((xerror*)pValue);
}



/* 初始化两个 FLS 槽和一个 TLS 槽，失败时释放全部已取得资源。 */
static bool __xrtErrorTlsEnsure(void)
{
	LONG iState = InterlockedCompareExchange(&__xrtErrorTlsState, 1, 0);

	if ( iState == 0 ) {
		__xrtErrorTlsError = FlsAlloc(__xrtErrorTlsDestroy);
		__xrtErrorTlsContext = TlsAlloc();
		__xrtErrorTlsHandler = FlsAlloc(NULL);
		if ( (__xrtErrorTlsError == FLS_OUT_OF_INDEXES) ||
			 (__xrtErrorTlsContext == TLS_OUT_OF_INDEXES) ||
			 (__xrtErrorTlsHandler == FLS_OUT_OF_INDEXES) ) {
			if ( __xrtErrorTlsError != FLS_OUT_OF_INDEXES ) {
				(void)FlsFree(__xrtErrorTlsError);
			}
			if ( __xrtErrorTlsContext != TLS_OUT_OF_INDEXES ) {
				(void)TlsFree(__xrtErrorTlsContext);
			}
			if ( __xrtErrorTlsHandler != FLS_OUT_OF_INDEXES ) {
				(void)FlsFree(__xrtErrorTlsHandler);
			}
			__xrtErrorTlsError = FLS_OUT_OF_INDEXES;
			__xrtErrorTlsContext = TLS_OUT_OF_INDEXES;
			__xrtErrorTlsHandler = FLS_OUT_OF_INDEXES;
			(void)InterlockedExchange(&__xrtErrorTlsState, 3);
			return false;
		}
		(void)InterlockedExchange(&__xrtErrorTlsState, 2);
		return true;
	}
	while ( (iState = InterlockedCompareExchange(&__xrtErrorTlsState, 0, 0)) == 1 ) {
		(void)SwitchToThread();
	}

	return iState == 2;
}



/* 读取当前线程错误。 */
static xerror* __xrtThreadErrorGet(void)
{
	return __xrtErrorTlsEnsure() ?
		(xerror*)FlsGetValue(__xrtErrorTlsError) : NULL;
}



/* 写入当前线程错误。 */
static void __xrtThreadErrorSet(xerror* pError)
{
	if ( __xrtErrorTlsEnsure() ) {
		(void)FlsSetValue(__xrtErrorTlsError, pError);
	}
}



/* 读取当前错误执行上下文。 */
static xrt_error_context* __xrtBoundErrorContextGet(void)
{
	return __xrtErrorTlsEnsure() ?
		(xrt_error_context*)TlsGetValue(__xrtErrorTlsContext) : NULL;
}



/* 写入当前错误执行上下文。 */
static void __xrtBoundErrorContextSet(xrt_error_context* pContext)
{
	if ( __xrtErrorTlsEnsure() ) {
		(void)TlsSetValue(__xrtErrorTlsContext, pContext);
	}
}



/* 返回当前线程是否正在执行错误处理器。 */
static bool __xrtErrorHandlerActive(void)
{
	return __xrtErrorTlsEnsure() &&
		(FlsGetValue(__xrtErrorTlsHandler) != NULL);
}



/* 设置当前线程的错误处理器递归保护。 */
static void __xrtErrorHandlerSetActive(bool bActive)
{
	if ( __xrtErrorTlsEnsure() ) {
		(void)FlsSetValue(
			__xrtErrorTlsHandler,
			bActive ? (ptr)(uintptr_t)1 : NULL
		);
	}
}

#else

/* POSIX pthread key 在线程退出时释放仍由线程错误槽持有的对象。 */
static pthread_key_t __xrtErrorTlsError;
static pthread_key_t __xrtErrorTlsContext;
static pthread_key_t __xrtErrorTlsHandler;
static pthread_once_t __xrtErrorTlsOnce = PTHREAD_ONCE_INIT;
static bool __xrtErrorTlsReady = false;


/* 释放线程退出时仍未取走或清除的错误对象。 */
static void __xrtErrorTlsDestroy(ptr pValue)
{
	xrtErrorFree((xerror*)pValue);
}



/* 初始化三个 pthread key，失败时不遗留已创建的 key。 */
static void __xrtErrorTlsInit(void)
{
	if ( pthread_key_create(
		&__xrtErrorTlsError,
		__xrtErrorTlsDestroy
	) != 0 ) {
		return;
	}
	if ( pthread_key_create(&__xrtErrorTlsContext, NULL) != 0 ) {
		(void)pthread_key_delete(__xrtErrorTlsError);
		return;
	}
	if ( pthread_key_create(&__xrtErrorTlsHandler, NULL) != 0 ) {
		(void)pthread_key_delete(__xrtErrorTlsContext);
		(void)pthread_key_delete(__xrtErrorTlsError);
		return;
	}
	__xrtErrorTlsReady = true;
}



/* 确保 POSIX TLS 已经初始化。 */
static bool __xrtErrorTlsEnsure(void)
{
	(void)pthread_once(&__xrtErrorTlsOnce, __xrtErrorTlsInit);
	return __xrtErrorTlsReady;
}



/* 读取当前线程错误。 */
static xerror* __xrtThreadErrorGet(void)
{
	return __xrtErrorTlsEnsure() ? (xerror*)pthread_getspecific(__xrtErrorTlsError) : NULL;
}



/* 写入当前线程错误。 */
static void __xrtThreadErrorSet(xerror* pError)
{
	if ( __xrtErrorTlsEnsure() ) {
		(void)pthread_setspecific(__xrtErrorTlsError, pError);
	}
}



/* 读取当前错误执行上下文。 */
static xrt_error_context* __xrtBoundErrorContextGet(void)
{
	return __xrtErrorTlsEnsure() ? (xrt_error_context*)pthread_getspecific(__xrtErrorTlsContext) : NULL;
}



/* 写入当前错误执行上下文。 */
static void __xrtBoundErrorContextSet(xrt_error_context* pContext)
{
	if ( __xrtErrorTlsEnsure() ) {
		(void)pthread_setspecific(__xrtErrorTlsContext, pContext);
	}
}



/* 返回当前线程是否正在执行错误处理器。 */
static bool __xrtErrorHandlerActive(void)
{
	return __xrtErrorTlsEnsure() && (pthread_getspecific(__xrtErrorTlsHandler) != NULL);
}



/* 设置当前线程的错误处理器递归保护。 */
static void __xrtErrorHandlerSetActive(bool bActive)
{
	if ( __xrtErrorTlsEnsure() ) {
		(void)pthread_setspecific(__xrtErrorTlsHandler, bActive ? (ptr)(uintptr_t)1 : NULL);
	}
}

#endif



/* 返回当前执行上下文借用的错误。 */
static xerror* __xrtCurrentErrorGet(void)
{
	xrt_error_context* pContext = __xrtBoundErrorContextGet();

	if ( pContext != NULL ) {
		return pContext->Error;
	}

	return __xrtThreadErrorGet();
}



/* 写入当前执行上下文错误，不改变引用计数。 */
static void __xrtCurrentErrorSet(xerror* pError)
{
	xrt_error_context* pContext = __xrtBoundErrorContextGet();

	if ( pContext != NULL ) {
		pContext->Error = pError;
	} else {
		__xrtThreadErrorSet(pError);
	}
}



/* 复制一个可选字符串到连续错误内存。 */
static cstr __xrtErrorCopyText(char** pWrite, cstr sText)
{
	size_t iSize;
	cstr sResult;

	if ( sText == NULL ) {
		sText = "";
	}
	iSize = strlen(sText) + 1;
	sResult = *pWrite;
	memcpy(*pWrite, sText, iSize);
	*pWrite += iSize;

	return sResult;
}



/* 将错误所有权转移到当前执行上下文。 */
void __xrtErrorSetOwned(xerror* pError)
{
	xerror* pOldError = __xrtCurrentErrorGet();
	xerror* pObservedError;
	xerror* pHandlerError;
	xerrorhandler pHandler;
	ptr pUserData;

	__xrtCurrentErrorSet(pError);
	__xrtErrorHandlerGet(&pHandler, &pUserData);
	if ( (pHandler != NULL) && !__xrtErrorHandlerActive() && (pError != NULL) ) {
		/* 处理器中的失败不能覆盖正在通知的主错误。 */
		pObservedError = xrtErrorRef(pError);
		__xrtErrorHandlerSetActive(true);
		pHandler(pError, pUserData);
		__xrtErrorHandlerSetActive(false);
		if ( pObservedError != NULL ) {
			pHandlerError = __xrtErrorSwapOwned(pObservedError);
			xrtErrorFree(pHandlerError);
		}
	}
	xrtErrorFree(pOldError);
}



/* 静默交换当前错误所有权，供受控的用户回调边界隔离旧错误。 */
xerror* __xrtErrorSwapOwned(xerror* pError)
{
	xerror* pPrevious = __xrtCurrentErrorGet();

	__xrtCurrentErrorSet(pError);
	return pPrevious;
}



/* 无分配地设置内存不足错误。 */
void __xrtErrorSetOutOfMemory(void)
{
	__xrtErrorSetOwned(&__xrtOutOfMemoryError);
}



/* 构建并安装带稳定元数据和可选原因链的结构化错误。 */
void __xrtErrorSetDetail(
	xerrkind Kind,
	cstr sDomain,
	int32 iCode,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = sDomain;
	Desc.Code = iCode;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 构建并安装同时保留稳定模块代码与原生系统代码的错误。 */
void __xrtErrorSetSystem(
	cstr sDomain,
	int32 iCode,
	cstr sOperation,
	int iSystemCode,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = __xrtSystemErrorKind(iSystemCode);
	Desc.Domain = sDomain;
	Desc.Code = iCode;
	Desc.SystemCode = iSystemCode;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 取得当前错误并包装，包装分配失败时保留原错误或内存错误。 */
void __xrtErrorWrapDetail(
	xerrkind DefaultKind,
	cstr sDomain,
	int32 iCode,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrkind Kind = pCause != NULL ?
		xrtErrorKind(pCause) : DefaultKind;

	__xrtErrorSetDetail(
		Kind,
		sDomain,
		iCode,
		sOperation,
		sMessage,
		pCause
	);
	if ( (xrtGetError() == NULL) && (pCause != NULL) ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 无分配地设置参数错误。 */
void __xrtErrorSetInvalidArgument(void)
{
	__xrtErrorSetOwned(&__xrtInvalidArgumentError);
}



/* 无分配地设置类型不匹配错误。 */
void __xrtErrorSetType(void)
{
	__xrtErrorSetOwned(&__xrtTypeError);
}



/* 无分配地设置非法值错误。 */
void __xrtErrorSetValue(void)
{
	__xrtErrorSetOwned(&__xrtValueError);
}



/* 无分配地设置状态错误。 */
void __xrtErrorSetInvalidState(void)
{
	__xrtErrorSetOwned(&__xrtInvalidStateError);
}



/* 无分配地设置大小溢出错误。 */
void __xrtErrorSetSizeOverflow(void)
{
	__xrtErrorSetOwned(&__xrtSizeOverflowError);
}



/* 无分配地设置索引越界错误。 */
void __xrtErrorSetRange(void)
{
	__xrtErrorSetOwned(&__xrtRangeError);
}



/* 无分配地设置暂时无法继续错误。 */
void __xrtErrorSetAgain(void)
{
	__xrtErrorSetOwned(&__xrtAgainError);
}



/* 无分配地设置不支持错误。 */
void __xrtErrorSetUnsupported(void)
{
	__xrtErrorSetOwned(&__xrtUnsupportedError);
}



/* 无分配地设置资源或键已经存在错误。 */
void __xrtErrorSetExists(void)
{
	__xrtErrorSetOwned(&__xrtExistsError);
}



/* 无分配地设置操作已取消错误。 */
void __xrtErrorSetCancelled(void)
{
	__xrtErrorSetOwned(&__xrtCancelledError);
}



/* 无分配地设置操作超时错误。 */
void __xrtErrorSetTimeout(void)
{
	__xrtErrorSetOwned(&__xrtTimeoutError);
}



/* 无分配地设置资源已经关闭错误。 */
void __xrtErrorSetClosed(void)
{
	__xrtErrorSetOwned(&__xrtClosedError);
}



/* 无分配地设置内部契约错误。 */
void __xrtErrorSetInternal(void)
{
	__xrtErrorSetOwned(&__xrtInternalError);
}



/* 为协程或任务切换错误执行上下文。 */
xrt_error_context* __xrtErrorContextSwap(xrt_error_context* pContext)
{
	xrt_error_context* pPrevious = __xrtBoundErrorContextGet();

	__xrtBoundErrorContextSet(pContext);
	return pPrevious;
}



/* 从完整描述创建一个错误对象。 */
XRT_API xerror* xrtErrorBuild(const xerrordesc* pDesc)
{
	size_t iDomainSize;
	size_t iOperationSize;
	size_t iMessageSize;
	size_t iDataSize;
	size_t iTextSize;
	xerror* pError;
	char* pWrite;

	if ( pDesc == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( (pDesc->Kind <= XERR_NONE) || (pDesc->Kind > XERR_INTERNAL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}

	iDomainSize = strlen(pDesc->Domain != NULL ? pDesc->Domain : "") + 1;
	iOperationSize = strlen(pDesc->Operation != NULL ? pDesc->Operation : "") + 1;
	iMessageSize = strlen(pDesc->Message != NULL ? pDesc->Message : "") + 1;
	iDataSize = strlen(pDesc->Data != NULL ? pDesc->Data : "") + 1;
	if ( (iDomainSize > (SIZE_MAX - iOperationSize)) ||
		 ((iDomainSize + iOperationSize) > (SIZE_MAX - iMessageSize)) ||
		 ((iDomainSize + iOperationSize + iMessageSize) > (SIZE_MAX - iDataSize)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iTextSize = iDomainSize + iOperationSize + iMessageSize + iDataSize;
	if ( iTextSize > (SIZE_MAX - sizeof(xerror)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}

	pError = (xerror*)xrtMalloc(sizeof(xerror) + iTextSize);
	if ( pError == NULL ) {
		return NULL;
	}
	memset(pError, 0, sizeof(xerror));
	pError->RefCount = 1;
	pError->Kind = pDesc->Kind;
	pError->Code = pDesc->Code;
	pError->SystemCode = pDesc->SystemCode;
	pError->Cause = xrtErrorRef(pDesc->Cause);
	if ( (pDesc->Cause != NULL) && (pError->Cause == NULL) ) {
		xrtFree(pError);
		__xrtErrorSetInvalidState();
		return NULL;
	}
	pWrite = (char*)(pError + 1);
	pError->Domain = __xrtErrorCopyText(&pWrite, pDesc->Domain);
	pError->Operation = __xrtErrorCopyText(&pWrite, pDesc->Operation);
	pError->Message = __xrtErrorCopyText(&pWrite, pDesc->Message);
	pError->Data = __xrtErrorCopyText(&pWrite, pDesc->Data);

	return pError;
}



/* 创建一个常用错误对象。 */
XRT_API xerror* xrtErrorCreate(xerrkind Kind, cstr sDomain, int32 iCode, cstr sMessage)
{
	xerrordesc tDesc;

	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.Kind = Kind;
	tDesc.Domain = sDomain;
	tDesc.Code = iCode;
	tDesc.Message = sMessage;

	return xrtErrorBuild(&tDesc);
}



/* 创建带有原因链的错误对象。 */
XRT_API xerror* xrtErrorWrap(const xerror* pCause, xerrkind Kind, cstr sDomain, int32 iCode, cstr sMessage)
{
	xerrordesc tDesc;

	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.Kind = Kind;
	tDesc.Domain = sDomain;
	tDesc.Code = iCode;
	tDesc.Message = sMessage;
	tDesc.Cause = pCause;

	return xrtErrorBuild(&tDesc);
}



/* 增加错误对象引用并返回原指针。 */
XRT_API xerror* xrtErrorRef(const xerror* pError)
{
	if ( (pError != NULL) && ((pError->Flags & XRT_ERROR_STATIC) == 0) ) {
		if ( xrtRefRetain((volatile int32*)&pError->RefCount) < 0 ) {
			return NULL;
		}
	}

	return (xerror*)pError;
}



/* 释放错误对象引用。 */
XRT_API void xrtErrorFree(xerror* pError)
{
	while ( (pError != NULL) &&
		 ((pError->Flags & XRT_ERROR_STATIC) == 0) &&
		 (xrtRefRelease(&pError->RefCount) == 0) ) {
		xerror* pCause = pError->Cause;

		xrtFree(pError);
		pError = pCause;
	}
}



/* 返回错误的通用类别。 */
XRT_API xerrkind xrtErrorKind(const xerror* pError)
{
	return pError != NULL ? pError->Kind : XERR_NONE;
}



/* 返回错误所属的稳定域。 */
XRT_API cstr xrtErrorDomain(const xerror* pError)
{
	return pError != NULL ? pError->Domain : "";
}



/* 返回模块定义的错误代码。 */
XRT_API int32 xrtErrorCode(const xerror* pError)
{
	return pError != NULL ? pError->Code : 0;
}



/* 返回操作系统或外部库错误代码。 */
XRT_API int32 xrtErrorSystemCode(const xerror* pError)
{
	return pError != NULL ? pError->SystemCode : 0;
}



/* 返回发生错误的操作名称。 */
XRT_API cstr xrtErrorOperation(const xerror* pError)
{
	return pError != NULL ? pError->Operation : "";
}



/* 返回错误消息。 */
XRT_API cstr xrtErrorMessage(const xerror* pError)
{
	return pError != NULL ? pError->Message : "";
}



/* 返回可选的机器可读附加数据。 */
XRT_API cstr xrtErrorData(const xerror* pError)
{
	return pError != NULL ? pError->Data : "";
}



/* 返回借用的原因错误。 */
XRT_API const xerror* xrtErrorCause(const xerror* pError)
{
	return pError != NULL ? pError->Cause : NULL;
}



/* 沿原因链查找指定通用类别，返回借用的错误对象。 */
XRT_API const xerror* xrtErrorIs(const xerror* pError, xerrkind Kind)
{
	while ( pError != NULL ) {
		if ( pError->Kind == Kind ) {
			return pError;
		}
		pError = pError->Cause;
	}
	return NULL;
}



/* 沿原因链查找完全匹配的错误域和代码，返回借用的错误对象。 */
XRT_API const xerror* xrtErrorFind(const xerror* pError, cstr sDomain, int32 iCode)
{
	if ( sDomain == NULL ) {
		return NULL;
	}
	while ( pError != NULL ) {
		if ( (pError->Code == iCode) && (strcmp(pError->Domain, sDomain) == 0) ) {
			return pError;
		}
		pError = pError->Cause;
	}
	return NULL;
}



/* 返回当前执行上下文借用的错误对象。 */
XRT_API const xerror* xrtGetError(void)
{
	return __xrtCurrentErrorGet();
}



/* 取走当前执行上下文的错误对象。 */
XRT_API xerror* xrtTakeError(void)
{
	xerror* pError = __xrtCurrentErrorGet();

	__xrtCurrentErrorSet(NULL);
	return pError;
}



/* 将错误对象设置到当前执行上下文，函数会增加引用。 */
XRT_API void xrtSetError(const xerror* pError)
{
	__xrtErrorSetOwned(xrtErrorRef((xerror*)pError));
}



/* 将错误对象所有权转移到当前执行上下文。 */
XRT_API void xrtSetErrorTake(xerror* pError)
{
	__xrtErrorSetOwned(pError);
}



/* 创建常用错误并直接设置到当前执行上下文。 */
XRT_API void xrtSetErrorInfo(
	xerrkind Kind,
	cstr sDomain,
	int32 iCode,
	cstr sMessage
)
{
	xerror* pError = xrtErrorCreate(Kind, sDomain, iCode, sMessage);

	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 以静态对象发布稳定类别，确保 OOM 和终态路径无需再次分配。 */
XRT_API void xrtSetErrorKind(xerrkind Kind)
{
	xerror* pError = NULL;

	switch ( Kind ) {
		case XERR_NONE:
			break;
		case XERR_ARGUMENT:
			pError = &__xrtInvalidArgumentError;
			break;
		case XERR_TYPE:
			pError = &__xrtTypeError;
			break;
		case XERR_VALUE:
			pError = &__xrtValueError;
			break;
		case XERR_RANGE:
			pError = &__xrtRangeError;
			break;
		case XERR_STATE:
			pError = &__xrtInvalidStateError;
			break;
		case XERR_MEMORY:
			pError = &__xrtOutOfMemoryError;
			break;
		case XERR_IO:
			pError = &__xrtIoErrorStatic;
			break;
		case XERR_NOT_FOUND:
			pError = &__xrtNotFoundErrorStatic;
			break;
		case XERR_EXISTS:
			pError = &__xrtExistsError;
			break;
		case XERR_PERMISSION:
			pError = &__xrtPermissionErrorStatic;
			break;
		case XERR_AGAIN:
			pError = &__xrtAgainError;
			break;
		case XERR_TIMEOUT:
			pError = &__xrtTimeoutError;
			break;
		case XERR_CANCELLED:
			pError = &__xrtCancelledError;
			break;
		case XERR_CLOSED:
			pError = &__xrtClosedError;
			break;
		case XERR_PROTOCOL:
			pError = &__xrtProtocolErrorStatic;
			break;
		case XERR_UNSUPPORTED:
			pError = &__xrtUnsupportedError;
			break;
		case XERR_INTERNAL:
			pError = &__xrtInternalError;
			break;
		default:
			pError = &__xrtInvalidArgumentError;
			break;
	}
	__xrtErrorSetOwned(pError);
}



/* 清除当前执行上下文的错误。 */
XRT_API void xrtClearError(void)
{
	__xrtErrorSetOwned(NULL);
}



/* 设置进程级错误通知处理器。 */
XRT_API void xrtSetErrorHandler(xerrorhandler pHandler, ptr pUserData)
{
	__xrtSpinLock(&__xrtErrorHandlerLock);
	__xrtErrorHandlerData = pUserData;
	__xrtErrorHandler = pHandler;
	__xrtSpinUnlock(&__xrtErrorHandlerLock);
}
