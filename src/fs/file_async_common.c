#include "../internal/xrt_file_async.h"



#if defined(XRT_FEATURE_FILE_ASYNC_COMMON)

/* 把同步文件或任务池错误包装为稳定的异步文件错误。 */
void __xrtFileAsyncError(
	xfileasyncerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ?
		xrtErrorKind(pCause) : XERR_IO;
	Desc.Domain = "xrt.file.async";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 提交拥有型任务，并在受理失败时完整回收调用参数。 */
xfuture* __xrtFileAsyncSubmit(
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy
)
{
	xtaskargs Args;
	xfuture* pFuture;

	memset(&Args, 0, sizeof(Args));
	Args.Destroy = pDestroy;
	pFuture = xrtTaskSubmit(pPool, pProc, pData, &Args);
	if ( pFuture != NULL ) {
		return pFuture;
	}
	__xrtFileAsyncError(
		XFILE_ASYNC_ERROR_SUBMIT,
		"submit-file-operation",
		"failed to submit the asynchronous file operation"
	);
	if ( pDestroy != NULL ) {
		pDestroy(pData, NULL);
	}
	return NULL;
}



/* 释放单次分配的 Future 值。 */
void __xrtFileAsyncValueFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtFree(pValue);
}



/* 连续分配任务头和路径，避免每个高层异步操作重复多次分配。 */
ptr __xrtFileAsyncPathTaskCreate(
	size_t iTaskSize,
	cstr sSource,
	cstr sTarget,
	cstr* pSource,
	cstr* pTarget
)
{
	size_t iSource;
	size_t iTarget = 0;
	size_t iTotal;
	bytes pTask;
	str sStorage;

	if ( (iTaskSize == 0) ||
		(sSource == NULL) || (sSource[0] == '\0') ||
		(pSource == NULL) || (pTarget == NULL) ||
		((sTarget != NULL) && (sTarget[0] == '\0')) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	iSource = strlen(sSource);
	if ( sTarget != NULL ) {
		iTarget = strlen(sTarget);
	}
	if ( (iTaskSize > (SIZE_MAX - 2u)) ||
		(iSource > (SIZE_MAX - iTaskSize - 2u)) ||
		(iTarget > (SIZE_MAX - iTaskSize - iSource - 2u)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iTotal = iTaskSize + iSource + iTarget + 2u;
	pTask = (bytes)xrtMalloc(iTotal);
	if ( pTask == NULL ) {
		return NULL;
	}
	memset(pTask, 0, iTaskSize);
	sStorage = (str)(pTask + iTaskSize);
	memcpy(sStorage, sSource, iSource + 1u);
	*pSource = sStorage;
	*pTarget = NULL;
	if ( sTarget != NULL ) {
		*pTarget = sStorage + iSource + 1u;
		memcpy((str)*pTarget, sTarget, iTarget + 1u);
	}
	return pTask;
}

#endif
