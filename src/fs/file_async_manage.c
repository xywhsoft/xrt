#include "../internal/xrt_file_async.h"



#if defined(XRT_FEATURE_FILE_ASYNC_MANAGE)

/* 文件管理任务最多拥有两个连续存储的路径。 */
typedef enum xrt_file_async_manage_operation {
	XRT_FILE_ASYNC_COPY = 1,
	XRT_FILE_ASYNC_MOVE,
	XRT_FILE_ASYNC_DELETE
} xrt_file_async_manage_operation;



typedef struct xrt_file_async_manage_task {
	xrt_file_async_manage_operation Operation;
	cstr Source;
	cstr Target;
	bool Replace;
} xrt_file_async_manage_task;



/* 释放单次连续分配的管理任务。 */
static void __xrtFileAsyncManageFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtFree(pValue);
}



/* 执行一次文件复制、移动或删除。 */
static xtaskoutcome __xrtFileAsyncManageTask(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	xrt_file_async_manage_task* pTask =
		(xrt_file_async_manage_task*)pData;
	bool bResult;
	xfileasyncerror Code;
	cstr sOperation;
	cstr sMessage;

	(void)pResult;
	if ( xrtCancelRequested(pCancel) ) {
		return XTASK_CANCELLED;
	}
	if ( pTask->Operation == XRT_FILE_ASYNC_COPY ) {
		bResult = xrtFileCopy(
			pTask->Source,
			pTask->Target,
			pTask->Replace
		);
		Code = XFILE_ASYNC_ERROR_COPY;
		sOperation = "copy-file";
		sMessage = "failed to copy the file asynchronously";
	} else if ( pTask->Operation == XRT_FILE_ASYNC_MOVE ) {
		bResult = xrtFileMove(
			pTask->Source,
			pTask->Target,
			pTask->Replace
		);
		Code = XFILE_ASYNC_ERROR_MOVE;
		sOperation = "move-file";
		sMessage = "failed to move the file asynchronously";
	} else {
		bResult = xrtFileDelete(pTask->Source);
		Code = XFILE_ASYNC_ERROR_DELETE;
		sOperation = "delete-file";
		sMessage = "failed to delete the file asynchronously";
	}
	if ( !bResult ) {
		__xrtFileAsyncError(Code, sOperation, sMessage);
		return XTASK_FAILED;
	}
	return XTASK_SUCCESS;
}



/* 一次分配任务头和两个可选路径。 */
static xrt_file_async_manage_task* __xrtFileAsyncManageCreate(
	xrt_file_async_manage_operation Operation,
	cstr sSource,
	cstr sTarget,
	bool bReplace
)
{
	xrt_file_async_manage_task* pTask;
	cstr sOwnedSource;
	cstr sOwnedTarget;

	if ( (sSource == NULL) || (sSource[0] == '\0') ||
		((Operation != XRT_FILE_ASYNC_DELETE) &&
		 ((sTarget == NULL) || (sTarget[0] == '\0'))) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pTask = (xrt_file_async_manage_task*)
		__xrtFileAsyncPathTaskCreate(
			sizeof(*pTask),
			sSource,
			sTarget,
			&sOwnedSource,
			&sOwnedTarget
		);
	if ( pTask == NULL ) {
		return NULL;
	}
	pTask->Operation = Operation;
	pTask->Source = sOwnedSource;
	pTask->Target = sOwnedTarget;
	pTask->Replace = bReplace;
	return pTask;
}



/* 校验任务池并提交文件管理任务。 */
static xfuture* __xrtFileAsyncManageSubmit(
	xtaskpool* pPool,
	xrt_file_async_manage_operation Operation,
	cstr sSource,
	cstr sTarget,
	bool bReplace
)
{
	xrt_file_async_manage_task* pTask;

	if ( pPool == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pTask = __xrtFileAsyncManageCreate(
		Operation,
		sSource,
		sTarget,
		bReplace
	);
	return pTask != NULL ? __xrtFileAsyncSubmit(
		pPool,
		__xrtFileAsyncManageTask,
		pTask,
		__xrtFileAsyncManageFree
	) : NULL;
}



/* 提交异步文件复制。 */
XRT_API xfuture* xrtFileCopyAsync(
	xtaskpool* pPool,
	cstr sSource,
	cstr sTarget,
	bool bReplace
)
{
	return __xrtFileAsyncManageSubmit(
		pPool,
		XRT_FILE_ASYNC_COPY,
		sSource,
		sTarget,
		bReplace
	);
}



/* 提交异步文件移动。 */
XRT_API xfuture* xrtFileMoveAsync(
	xtaskpool* pPool,
	cstr sSource,
	cstr sTarget,
	bool bReplace
)
{
	return __xrtFileAsyncManageSubmit(
		pPool,
		XRT_FILE_ASYNC_MOVE,
		sSource,
		sTarget,
		bReplace
	);
}



/* 提交异步文件删除。 */
XRT_API xfuture* xrtFileDeleteAsync(
	xtaskpool* pPool,
	cstr sPath
)
{
	return __xrtFileAsyncManageSubmit(
		pPool,
		XRT_FILE_ASYNC_DELETE,
		sPath,
		NULL,
		false
	);
}

#endif
