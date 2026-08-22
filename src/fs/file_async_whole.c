#include "../internal/xrt_file_async.h"



#if defined(XRT_FEATURE_FILE_ASYNC_WHOLE)

/* 整文件任务只保留一个路径和可选的连续数据副本。 */
typedef enum xrt_file_async_whole_operation {
	XRT_FILE_ASYNC_READ_ALL = 1,
	XRT_FILE_ASYNC_WRITE_ALL,
	XRT_FILE_ASYNC_APPEND,
	XRT_FILE_ASYNC_WRITE_ATOMIC
} xrt_file_async_whole_operation;



typedef struct xrt_file_async_whole_task {
	xrt_file_async_whole_operation Operation;
	cstr Path;
	cbytes Data;
	size_t Size;
	size_t Limit;
} xrt_file_async_whole_task;



/* 读取结果采用同步整文件缓冲，不复制文件内容。 */
static void __xrtFileAsyncWholeDataFree(ptr pValue, ptr pData)
{
	xfiledata* pFileData = (xfiledata*)pValue;

	(void)pData;
	if ( pFileData != NULL ) {
		xrtFree(pFileData->Data);
		xrtFree(pFileData);
	}
}



/* 释放单次连续分配的路径、数据和任务头。 */
static void __xrtFileAsyncWholeTaskFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtFree(pValue);
}



/* 读取整个文件并把原始缓冲直接交给 Future。 */
static xtaskoutcome __xrtFileAsyncWholeRead(
	xrt_file_async_whole_task* pTask,
	xtaskvalue* pResult
)
{
	xfiledata* pData;
	bytes pBytes;
	size_t iSize = 0;

	pBytes = xrtFileReadAllLimit(
		pTask->Path,
		pTask->Limit,
		&iSize
	);
	if ( pBytes == NULL ) {
		__xrtFileAsyncError(
			XFILE_ASYNC_ERROR_READ,
			"read-all-file",
			"failed to read the complete file asynchronously"
		);
		return XTASK_FAILED;
	}
	pData = (xfiledata*)xrtMalloc(sizeof(xfiledata));
	if ( pData == NULL ) {
		xrtFree(pBytes);
		return XTASK_FAILED;
	}
	pData->Data = pBytes;
	pData->Size = iSize;
	pData->Offset = 0;
	pData->End = true;
	pResult->Value = pData;
	pResult->Destroy = __xrtFileAsyncWholeDataFree;
	return XTASK_SUCCESS;
}



/* 完成一种整文件写入，并返回本次处理的字节数。 */
static xtaskoutcome __xrtFileAsyncWholeWrite(
	xrt_file_async_whole_task* pTask,
	xtaskvalue* pResult
)
{
	xfilechange* pChange =
		(xfilechange*)xrtMalloc(sizeof(xfilechange));
	bool bResult;

	if ( pChange == NULL ) {
		return XTASK_FAILED;
	}
	if ( pTask->Operation == XRT_FILE_ASYNC_WRITE_ALL ) {
		bResult = xrtFileWriteAll(
			pTask->Path,
			(xbytesview) { pTask->Data, pTask->Size }
		);
	} else if ( pTask->Operation == XRT_FILE_ASYNC_APPEND ) {
		bResult = xrtFileAppend(
			pTask->Path,
			(xbytesview) { pTask->Data, pTask->Size }
		);
	} else {
		bResult = xrtFileWriteAtomic(
			pTask->Path,
			(xbytesview) { pTask->Data, pTask->Size }
		);
	}
	if ( !bResult ) {
		xrtFree(pChange);
		__xrtFileAsyncError(
			XFILE_ASYNC_ERROR_WRITE,
			"write-all-file",
			"failed to write the complete file asynchronously"
		);
		return XTASK_FAILED;
	}
	pChange->Offset = 0;
	pChange->Size = pTask->Size;
	pResult->Value = pChange;
	pResult->Destroy = __xrtFileAsyncValueFree;
	return XTASK_SUCCESS;
}



/* 在执行阻塞文件调用前观察一次协作取消。 */
static xtaskoutcome __xrtFileAsyncWholeTask(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	xrt_file_async_whole_task* pTask =
		(xrt_file_async_whole_task*)pData;

	if ( xrtCancelRequested(pCancel) ) {
		return XTASK_CANCELLED;
	}
	return pTask->Operation == XRT_FILE_ASYNC_READ_ALL ?
		__xrtFileAsyncWholeRead(pTask, pResult) :
		__xrtFileAsyncWholeWrite(pTask, pResult);
}



/* 一次分配任务头、路径和写入副本。 */
static xrt_file_async_whole_task* __xrtFileAsyncWholeCreate(
	xrt_file_async_whole_operation Operation,
	cstr sPath,
	xbytesview Data,
	size_t iLimit
)
{
	xrt_file_async_whole_task* pTask;
	size_t iPath;
	size_t iTotal;
	char* pStorage;

	if ( (sPath == NULL) || (sPath[0] == '\0') ||
		((Data.Size != 0) && (Data.Data == NULL)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	iPath = strlen(sPath);
	if ( (iPath > (SIZE_MAX - sizeof(*pTask) - 1u)) ||
		(Data.Size >
		 (SIZE_MAX - sizeof(*pTask) - iPath - 1u)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iTotal = sizeof(*pTask) + iPath + 1u + Data.Size;
	pTask = (xrt_file_async_whole_task*)xrtMalloc(iTotal);
	if ( pTask == NULL ) {
		return NULL;
	}
	memset(pTask, 0, sizeof(*pTask));
	pStorage = (char*)(pTask + 1);
	memcpy(pStorage, sPath, iPath + 1u);
	pTask->Operation = Operation;
	pTask->Path = pStorage;
	pTask->Limit = iLimit;
	if ( Data.Size != 0 ) {
		pTask->Data = (cbytes)(pStorage + iPath + 1u);
		memcpy((bytes)pTask->Data, Data.Data, Data.Size);
		pTask->Size = Data.Size;
	}
	return pTask;
}



/* 校验任务池并提交一个整文件任务。 */
static xfuture* __xrtFileAsyncWholeSubmit(
	xtaskpool* pPool,
	xrt_file_async_whole_operation Operation,
	cstr sPath,
	xbytesview Data,
	size_t iLimit
)
{
	xrt_file_async_whole_task* pTask;

	if ( pPool == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pTask = __xrtFileAsyncWholeCreate(
		Operation,
		sPath,
		Data,
		iLimit
	);
	return pTask != NULL ? __xrtFileAsyncSubmit(
		pPool,
		__xrtFileAsyncWholeTask,
		pTask,
		__xrtFileAsyncWholeTaskFree
	) : NULL;
}



/* 提交无业务大小上限的整文件读取。 */
XRT_API xfuture* xrtFileReadAllAsync(
	xtaskpool* pPool,
	cstr sPath
)
{
	return xrtFileReadAllLimitAsync(
		pPool,
		sPath,
		SIZE_MAX - 1u
	);
}



/* 提交带硬上限的整文件读取。 */
XRT_API xfuture* xrtFileReadAllLimitAsync(
	xtaskpool* pPool,
	cstr sPath,
	size_t iLimit
)
{
	return __xrtFileAsyncWholeSubmit(
		pPool,
		XRT_FILE_ASYNC_READ_ALL,
		sPath,
		(xbytesview) { NULL, 0 },
		iLimit
	);
}



/* 提交复制输入的整文件覆盖写入。 */
XRT_API xfuture* xrtFileWriteAllAsync(
	xtaskpool* pPool,
	cstr sPath,
	xbytesview Data
)
{
	return __xrtFileAsyncWholeSubmit(
		pPool,
		XRT_FILE_ASYNC_WRITE_ALL,
		sPath,
		Data,
		0
	);
}



/* 提交复制输入的整文件追加。 */
XRT_API xfuture* xrtFileAppendAsync(
	xtaskpool* pPool,
	cstr sPath,
	xbytesview Data
)
{
	return __xrtFileAsyncWholeSubmit(
		pPool,
		XRT_FILE_ASYNC_APPEND,
		sPath,
		Data,
		0
	);
}



/* 提交复制输入的原子整文件写入。 */
XRT_API xfuture* xrtFileWriteAtomicAsync(
	xtaskpool* pPool,
	cstr sPath,
	xbytesview Data
)
{
	return __xrtFileAsyncWholeSubmit(
		pPool,
		XRT_FILE_ASYNC_WRITE_ATOMIC,
		sPath,
		Data,
		0
	);
}

#endif
