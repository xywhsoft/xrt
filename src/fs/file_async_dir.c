#include "../internal/xrt_file_async.h"



#if defined(XRT_FEATURE_DIR_ASYNC)

/* 基础目录任务仅保存操作、模式和一条拥有型路径。 */
typedef enum xrt_file_async_dir_operation {
	XRT_FILE_ASYNC_DIR_CREATE = 1,
	XRT_FILE_ASYNC_DIR_CREATE_ALL,
	XRT_FILE_ASYNC_DIR_REMOVE,
	XRT_FILE_ASYNC_DIR_EMPTY
} xrt_file_async_dir_operation;



typedef struct xrt_file_async_dir_task {
	xrt_file_async_dir_operation Operation;
	cstr Path;
	uint32 Mode;
	bool ExplicitMode;
} xrt_file_async_dir_task;



/* 释放连续分配的目录任务和路径。 */
static void __xrtFileAsyncDirFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtFree(pValue);
}



/* 执行创建或删除目录操作。 */
static xtaskoutcome __xrtFileAsyncDirChange(
	xrt_file_async_dir_task* pTask
)
{
	bool bResult;
	xfileasyncerror Code;
	cstr sOperation;
	cstr sMessage;

	if ( pTask->Operation == XRT_FILE_ASYNC_DIR_CREATE ) {
		bResult = pTask->ExplicitMode ?
			xrtDirCreateMode(pTask->Path, pTask->Mode) :
			xrtDirCreate(pTask->Path);
		Code = XFILE_ASYNC_ERROR_CREATE;
		sOperation = "create-directory";
		sMessage = "failed to create the directory asynchronously";
	} else if ( pTask->Operation == XRT_FILE_ASYNC_DIR_CREATE_ALL ) {
		bResult = pTask->ExplicitMode ?
			xrtDirCreateAllMode(pTask->Path, pTask->Mode) :
			xrtDirCreateAll(pTask->Path);
		Code = XFILE_ASYNC_ERROR_CREATE;
		sOperation = "create-directories";
		sMessage = "failed to create the directory path asynchronously";
	} else {
		bResult = xrtDirRemove(pTask->Path);
		Code = XFILE_ASYNC_ERROR_DELETE;
		sOperation = "remove-directory";
		sMessage = "failed to remove the directory asynchronously";
	}
	if ( !bResult ) {
		__xrtFileAsyncError(Code, sOperation, sMessage);
		return XTASK_FAILED;
	}
	return XTASK_SUCCESS;
}



/* 查询目录是否为空，并返回 Future 拥有的布尔结果。 */
static xtaskoutcome __xrtFileAsyncDirEmpty(
	xrt_file_async_dir_task* pTask,
	xtaskvalue* pResult
)
{
	xdirquery* pQuery = (xdirquery*)xrtMalloc(sizeof(xdirquery));
	bool bEmpty;

	if ( pQuery == NULL ) {
		return XTASK_FAILED;
	}
	if ( !xrtDirEmpty(pTask->Path, &bEmpty) ) {
		xrtFree(pQuery);
		__xrtFileAsyncError(
			XFILE_ASYNC_ERROR_QUERY,
			"query-directory-empty",
			"failed to query the directory asynchronously"
		);
		return XTASK_FAILED;
	}
	pQuery->Empty = bEmpty;
	pResult->Value = pQuery;
	pResult->Destroy = __xrtFileAsyncValueFree;
	return XTASK_SUCCESS;
}



/* 在阻塞目录调用前观察一次协作取消。 */
static xtaskoutcome __xrtFileAsyncDirTask(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	xrt_file_async_dir_task* pTask =
		(xrt_file_async_dir_task*)pData;

	if ( xrtCancelRequested(pCancel) ) {
		return XTASK_CANCELLED;
	}
	return pTask->Operation == XRT_FILE_ASYNC_DIR_EMPTY ?
		__xrtFileAsyncDirEmpty(pTask, pResult) :
		__xrtFileAsyncDirChange(pTask);
}



/* 复制路径并提交一个基础目录任务。 */
static xfuture* __xrtFileAsyncDirSubmit(
	xtaskpool* pPool,
	xrt_file_async_dir_operation Operation,
	cstr sPath,
	uint32 iMode,
	bool bExplicitMode
)
{
	xrt_file_async_dir_task* pTask;
	cstr sOwnedPath;
	cstr sUnused;

	if ( pPool == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pTask = (xrt_file_async_dir_task*)
		__xrtFileAsyncPathTaskCreate(
			sizeof(*pTask),
			sPath,
			NULL,
			&sOwnedPath,
			&sUnused
		);
	if ( pTask == NULL ) {
		return NULL;
	}
	pTask->Operation = Operation;
	pTask->Path = sOwnedPath;
	pTask->Mode = iMode;
	pTask->ExplicitMode = bExplicitMode;
	return __xrtFileAsyncSubmit(
		pPool,
		__xrtFileAsyncDirTask,
		pTask,
		__xrtFileAsyncDirFree
	);
}



/* 提交使用平台默认模式的单目录创建。 */
XRT_API xfuture* xrtDirCreateAsync(
	xtaskpool* pPool,
	cstr sPath
)
{
	return __xrtFileAsyncDirSubmit(
		pPool,
		XRT_FILE_ASYNC_DIR_CREATE,
		sPath,
		0,
		false
	);
}



/* 提交使用显式模式的单目录创建。 */
XRT_API xfuture* xrtDirCreateModeAsync(
	xtaskpool* pPool,
	cstr sPath,
	uint32 iMode
)
{
	return __xrtFileAsyncDirSubmit(
		pPool,
		XRT_FILE_ASYNC_DIR_CREATE,
		sPath,
		iMode,
		true
	);
}



/* 提交使用平台默认模式的递归目录创建。 */
XRT_API xfuture* xrtDirCreateAllAsync(
	xtaskpool* pPool,
	cstr sPath
)
{
	return __xrtFileAsyncDirSubmit(
		pPool,
		XRT_FILE_ASYNC_DIR_CREATE_ALL,
		sPath,
		0,
		false
	);
}



/* 提交使用显式模式的递归目录创建。 */
XRT_API xfuture* xrtDirCreateAllModeAsync(
	xtaskpool* pPool,
	cstr sPath,
	uint32 iMode
)
{
	return __xrtFileAsyncDirSubmit(
		pPool,
		XRT_FILE_ASYNC_DIR_CREATE_ALL,
		sPath,
		iMode,
		true
	);
}



/* 提交空目录删除。 */
XRT_API xfuture* xrtDirRemoveAsync(
	xtaskpool* pPool,
	cstr sPath
)
{
	return __xrtFileAsyncDirSubmit(
		pPool,
		XRT_FILE_ASYNC_DIR_REMOVE,
		sPath,
		0,
		false
	);
}



/* 提交目录为空查询。 */
XRT_API xfuture* xrtDirEmptyAsync(
	xtaskpool* pPool,
	cstr sPath
)
{
	return __xrtFileAsyncDirSubmit(
		pPool,
		XRT_FILE_ASYNC_DIR_EMPTY,
		sPath,
		0,
		false
	);
}

#endif
