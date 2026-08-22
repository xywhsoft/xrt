#include "../internal/xrt_file_async.h"



#if defined(XRT_FEATURE_FILE_TREE_ASYNC)

/* 目录树异步层直接复用同步树选项和统计结构。 */
typedef enum xrt_file_async_tree_operation {
	XRT_FILE_ASYNC_TREE_COPY = 1,
	XRT_FILE_ASYNC_TREE_REMOVE,
	XRT_FILE_ASYNC_TREE_MOVE,
	XRT_FILE_ASYNC_TREE_STATS,
	XRT_FILE_ASYNC_TREE_SIZE,
	XRT_FILE_ASYNC_TREE_ENSURE_EMPTY
} xrt_file_async_tree_operation;



typedef struct xrt_file_async_tree_task {
	xrt_file_async_tree_operation Operation;
	cstr Source;
	cstr Target;
	xtreecopyoptions Options;
	bool Flag;
} xrt_file_async_tree_task;



/* 释放连续分配的目录树任务和路径。 */
static void __xrtFileAsyncTreeFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtFree(pValue);
}



/* 执行产生遍历统计的目录树操作。 */
static xtaskoutcome __xrtFileAsyncTreeStatsTask(
	xrt_file_async_tree_task* pTask,
	xtaskvalue* pResult
)
{
	xwalkstats* pStats = (xwalkstats*)xrtMalloc(sizeof(xwalkstats));
	bool bResult;
	cstr sOperation;
	cstr sMessage;

	if ( pStats == NULL ) {
		return XTASK_FAILED;
	}
	if ( pTask->Operation == XRT_FILE_ASYNC_TREE_COPY ) {
		bResult = xrtFileTreeCopy(
			pTask->Source,
			pTask->Target,
			&pTask->Options,
			pStats
		);
		sOperation = "copy-directory-tree";
		sMessage = "failed to copy the directory tree asynchronously";
	} else if ( pTask->Operation == XRT_FILE_ASYNC_TREE_REMOVE ) {
		bResult = xrtFileTreeRemove(
			pTask->Source,
			pTask->Flag,
			pStats
		);
		sOperation = "remove-directory-tree";
		sMessage = "failed to remove the directory tree asynchronously";
	} else {
		bResult = xrtDirStats(
			pTask->Source,
			pTask->Flag,
			pStats
		);
		sOperation = "stat-directory-tree";
		sMessage = "failed to inspect the directory tree asynchronously";
	}
	if ( !bResult ) {
		xrtFree(pStats);
		__xrtFileAsyncError(
			XFILE_ASYNC_ERROR_TREE,
			sOperation,
			sMessage
		);
		return XTASK_FAILED;
	}
	pResult->Value = pStats;
	pResult->Destroy = __xrtFileAsyncValueFree;
	return XTASK_SUCCESS;
}



/* 计算目录树普通文件总字节数。 */
static xtaskoutcome __xrtFileAsyncTreeSizeTask(
	xrt_file_async_tree_task* pTask,
	xtaskvalue* pResult
)
{
	xfilesize* pSize = (xfilesize*)xrtMalloc(sizeof(xfilesize));

	if ( pSize == NULL ) {
		return XTASK_FAILED;
	}
	if ( !xrtDirSize(pTask->Source, pTask->Flag, &pSize->Size) ) {
		xrtFree(pSize);
		__xrtFileAsyncError(
			XFILE_ASYNC_ERROR_TREE,
			"size-directory-tree",
			"failed to query the directory tree size asynchronously"
		);
		return XTASK_FAILED;
	}
	pResult->Value = pSize;
	pResult->Destroy = __xrtFileAsyncValueFree;
	return XTASK_SUCCESS;
}



/* 执行不产生结果值的目录移动或确保为空操作。 */
static xtaskoutcome __xrtFileAsyncTreeChangeTask(
	xrt_file_async_tree_task* pTask
)
{
	bool bResult;
	cstr sOperation;
	cstr sMessage;

	if ( pTask->Operation == XRT_FILE_ASYNC_TREE_MOVE ) {
		bResult = xrtDirMove(
			pTask->Source,
			pTask->Target,
			pTask->Flag
		);
		sOperation = "move-directory-tree";
		sMessage = "failed to move the directory tree asynchronously";
	} else {
		bResult = xrtDirEnsureEmpty(pTask->Source);
		sOperation = "ensure-empty-directory";
		sMessage = "failed to ensure an empty directory asynchronously";
	}
	if ( !bResult ) {
		__xrtFileAsyncError(
			XFILE_ASYNC_ERROR_TREE,
			sOperation,
			sMessage
		);
		return XTASK_FAILED;
	}
	return XTASK_SUCCESS;
}



/* 在阻塞目录树调用前观察一次协作取消。 */
static xtaskoutcome __xrtFileAsyncTreeTask(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	xrt_file_async_tree_task* pTask =
		(xrt_file_async_tree_task*)pData;

	if ( xrtCancelRequested(pCancel) ) {
		return XTASK_CANCELLED;
	}
	if ( pTask->Operation == XRT_FILE_ASYNC_TREE_SIZE ) {
		return __xrtFileAsyncTreeSizeTask(pTask, pResult);
	}
	if ( (pTask->Operation == XRT_FILE_ASYNC_TREE_MOVE) ||
		(pTask->Operation == XRT_FILE_ASYNC_TREE_ENSURE_EMPTY) ) {
		return __xrtFileAsyncTreeChangeTask(pTask);
	}
	return __xrtFileAsyncTreeStatsTask(pTask, pResult);
}



/* 复制路径和选项，并提交一个目录树任务。 */
static xfuture* __xrtFileAsyncTreeSubmit(
	xtaskpool* pPool,
	xrt_file_async_tree_operation Operation,
	cstr sSource,
	cstr sTarget,
	const xtreecopyoptions* pOptions,
	bool bFlag
)
{
	xrt_file_async_tree_task* pTask;
	cstr sOwnedSource;
	cstr sOwnedTarget;

	if ( (pPool == NULL) ||
		((Operation == XRT_FILE_ASYNC_TREE_COPY ||
		  Operation == XRT_FILE_ASYNC_TREE_MOVE) &&
		 ((sTarget == NULL) || (sTarget[0] == '\0'))) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pTask = (xrt_file_async_tree_task*)
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
	pTask->Flag = bFlag;
	if ( pOptions == NULL ) {
		xrtTreeCopyOptionsInit(&pTask->Options);
	} else {
		pTask->Options = *pOptions;
	}
	return __xrtFileAsyncSubmit(
		pPool,
		__xrtFileAsyncTreeTask,
		pTask,
		__xrtFileAsyncTreeFree
	);
}



/* 提交高级目录树复制。 */
XRT_API xfuture* xrtFileTreeCopyAsync(
	xtaskpool* pPool,
	cstr sSource,
	cstr sTarget,
	const xtreecopyoptions* pOptions
)
{
	return __xrtFileAsyncTreeSubmit(
		pPool,
		XRT_FILE_ASYNC_TREE_COPY,
		sSource,
		sTarget,
		pOptions,
		false
	);
}



/* 提交常用目录复制。 */
XRT_API xfuture* xrtDirCopyAsync(
	xtaskpool* pPool,
	cstr sSource,
	cstr sTarget,
	bool bReplace
)
{
	xtreecopyoptions Options;

	xrtTreeCopyOptionsInit(&Options);
	if ( bReplace ) {
		Options.Flags = XTREE_COPY_MERGE | XTREE_COPY_REPLACE;
	}
	return xrtFileTreeCopyAsync(
		pPool,
		sSource,
		sTarget,
		&Options
	);
}



/* 提交高级目录树删除。 */
XRT_API xfuture* xrtFileTreeRemoveAsync(
	xtaskpool* pPool,
	cstr sPath,
	bool bKeepRoot
)
{
	return __xrtFileAsyncTreeSubmit(
		pPool,
		XRT_FILE_ASYNC_TREE_REMOVE,
		sPath,
		NULL,
		NULL,
		bKeepRoot
	);
}



/* 提交目录树整体删除。 */
XRT_API xfuture* xrtDirRemoveAllAsync(
	xtaskpool* pPool,
	cstr sPath
)
{
	return xrtFileTreeRemoveAsync(pPool, sPath, false);
}



/* 提交保留根目录的内容清理。 */
XRT_API xfuture* xrtDirCleanAsync(
	xtaskpool* pPool,
	cstr sPath
)
{
	return xrtFileTreeRemoveAsync(pPool, sPath, true);
}



/* 提交目录树移动。 */
XRT_API xfuture* xrtDirMoveAsync(
	xtaskpool* pPool,
	cstr sSource,
	cstr sTarget,
	bool bReplace
)
{
	return __xrtFileAsyncTreeSubmit(
		pPool,
		XRT_FILE_ASYNC_TREE_MOVE,
		sSource,
		sTarget,
		NULL,
		bReplace
	);
}



/* 提交目录树统计。 */
XRT_API xfuture* xrtDirStatsAsync(
	xtaskpool* pPool,
	cstr sPath,
	bool bRecursive
)
{
	return __xrtFileAsyncTreeSubmit(
		pPool,
		XRT_FILE_ASYNC_TREE_STATS,
		sPath,
		NULL,
		NULL,
		bRecursive
	);
}



/* 提交目录树大小查询。 */
XRT_API xfuture* xrtDirSizeAsync(
	xtaskpool* pPool,
	cstr sPath,
	bool bRecursive
)
{
	return __xrtFileAsyncTreeSubmit(
		pPool,
		XRT_FILE_ASYNC_TREE_SIZE,
		sPath,
		NULL,
		NULL,
		bRecursive
	);
}



/* 提交目录确保为空操作。 */
XRT_API xfuture* xrtDirEnsureEmptyAsync(
	xtaskpool* pPool,
	cstr sPath
)
{
	return __xrtFileAsyncTreeSubmit(
		pPool,
		XRT_FILE_ASYNC_TREE_ENSURE_EMPTY,
		sPath,
		NULL,
		NULL,
		false
	);
}

#endif
