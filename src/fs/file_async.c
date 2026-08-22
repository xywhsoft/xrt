#include "../internal/xrt_file_async.h"



#if defined(XRT_FEATURE_FILE_ASYNC)

/* 每个已受理任务持有一个对象引用，关闭只在最后一个任务结束后发生。 */
struct xasyncfile {
	xmutex Lock;
	xfile File;
	xtaskpool* Pool;
	xpromise* ClosePromise;
	xfuture* CloseFuture;
	xrt_task_finalizer Finalizer;
	size_t References;
	uint32 Flags;
	bool Closing;
};



/* 五种文件操作共享同一份紧凑任务参数。 */
typedef enum xrt_async_file_operation {
	XRT_ASYNC_FILE_READ = 1,
	XRT_ASYNC_FILE_WRITE,
	XRT_ASYNC_FILE_FLUSH,
	XRT_ASYNC_FILE_SIZE,
	XRT_ASYNC_FILE_RESIZE
} xrt_async_file_operation;



/* 写任务额外拥有 Data，其他任务只使用标量参数。 */
typedef struct xrt_async_file_task {
	xasyncfile* File;
	xrt_async_file_operation Operation;
	uint64 Offset;
	uint64 Size;
	cbytes Data;
	xfileasyncreleaseproc Release;
	ptr ReleaseContext;
} xrt_async_file_task;



/* 在任务池资源回收通道关闭原生文件并完成 Close Future。 */
static void __xrtAsyncFileFinalize(ptr pData)
{
	xasyncfile* pFile = (xasyncfile*)pData;
	xerror* pError = NULL;

	xrtClearError();
	if ( !xrtClose(pFile->File) ) {
		__xrtFileAsyncError(
			XFILE_ASYNC_ERROR_CLOSE,
			"close-file",
			"failed to close the asynchronous file"
		);
		pError = xrtTakeError();
	}
	if ( pError != NULL ) {
		(void)xrtPromiseReject(
			pFile->ClosePromise,
			pError
		);
	} else {
		(void)xrtPromiseResolve(
			pFile->ClosePromise,
			NULL
		);
	}
	xrtErrorFree(pError);
	xrtPromiseDestroy(pFile->ClosePromise);
	xrtFutureDestroy(pFile->CloseFuture);
	(void)xrtMutexUnit(&pFile->Lock);
	xrtFree(pFile);
}



/*
	释放一个对象引用。
	最后一个引用只投递无分配回收过程，不在调用线程执行文件系统操作。
*/
static void __xrtAsyncFileRelease(xasyncfile* pFile)
{
	bool bFinalize = false;

	if ( pFile == NULL ) {
		return;
	}
	(void)xrtMutexLock(&pFile->Lock);
	if ( pFile->References != 0 ) {
		pFile->References--;
	}
	if ( pFile->Closing && (pFile->References == 0) ) {
		bFinalize = true;
	}
	(void)xrtMutexUnlock(&pFile->Lock);
	if ( bFinalize ) {
		__xrtTaskPoolFinalize(
			pFile->Pool,
			&pFile->Finalizer,
			__xrtAsyncFileFinalize,
			pFile
		);
	}
}



/* 在对象仍接收操作时增加任务引用。 */
static bool __xrtAsyncFileAcquire(xasyncfile* pFile)
{
	bool bResult = false;

	if ( pFile == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	(void)xrtMutexLock(&pFile->Lock);
	if ( !pFile->Closing ) {
		pFile->References++;
		bResult = true;
	}
	(void)xrtMutexUnlock(&pFile->Lock);
	if ( !bResult ) {
		__xrtErrorSetClosed();
	}
	return bResult;
}



/* 释放任务数据、写入副本和任务持有的文件引用。 */
static void __xrtAsyncFileTaskFree(
	ptr pValue,
	ptr pData
)
{
	xrt_async_file_task* pTask =
		(xrt_async_file_task*)pValue;

	(void)pData;
	if ( pTask == NULL ) {
		return;
	}
	if ( pTask->Release != NULL ) {
		pTask->Release(
			pTask->ReleaseContext,
			pTask->Data,
			(size_t)pTask->Size
		);
	}
	__xrtAsyncFileRelease(pTask->File);
	xrtFree(pTask);
}



/* 创建由 Future 一次拥有的读取结果及其连续数据区。 */
static xfiledata* __xrtAsyncFileDataCreate(
	uint64 iOffset,
	size_t iSize
)
{
	xfiledata* pData;

	if ( iSize > (SIZE_MAX - sizeof(xfiledata) - 1) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	pData = (xfiledata*)xrtMalloc(
		sizeof(xfiledata) + iSize + 1
	);
	if ( pData == NULL ) {
		return NULL;
	}
	pData->Data = (bytes)(pData + 1);
	pData->Size = 0;
	pData->Offset = iOffset;
	pData->End = false;
	pData->Data[0] = 0;
	return pData;
}



/* 创建写入、大小查询或修改大小的轻量结果。 */
static xfilechange* __xrtAsyncFileChangeCreate(
	uint64 iOffset,
	uint64 iSize
)
{
	xfilechange* pChange =
		(xfilechange*)xrtMalloc(sizeof(xfilechange));

	if ( pChange != NULL ) {
		pChange->Offset = iOffset;
		pChange->Size = iSize;
	}
	return pChange;
}



/* 创建不带写入偏移语义的文件大小查询结果。 */
static xfilesize* __xrtAsyncFileSizeCreate(uint64 iSize)
{
	xfilesize* pSize = (xfilesize*)xrtMalloc(sizeof(xfilesize));

	if ( pSize != NULL ) {
		pSize->Size = iSize;
	}
	return pSize;
}



/* 使用原生 positioned I/O 执行读取，不修改共享文件游标。 */
static xtaskoutcome __xrtAsyncFileReadTask(
	xcancel* pCancel,
	xrt_async_file_task* pTask,
	xtaskvalue* pResult
)
{
	xfiledata* pData;
	size_t iDone = 0;

	pData = __xrtAsyncFileDataCreate(
		pTask->Offset,
		(size_t)pTask->Size
	);
	if ( pData == NULL ) {
		return XTASK_FAILED;
	}
	if ( xrtCancelRequested(pCancel) ) {
		xrtFree(pData);
		return XTASK_CANCELLED;
	}
	if ( !xrtReadAt(
		pTask->File->File,
		pTask->Offset,
		pData->Data,
		(size_t)pTask->Size,
		&iDone
	) ) {
		xrtFree(pData);
		__xrtFileAsyncError(
			XFILE_ASYNC_ERROR_READ,
			"read-file",
			"failed to read the asynchronous file"
		);
		return XTASK_FAILED;
	}
	pData->Size = iDone;
	pData->End = iDone < (size_t)pTask->Size;
	pData->Data[iDone] = 0;
	pResult->Value = pData;
	pResult->Destroy = __xrtFileAsyncValueFree;
	return XTASK_SUCCESS;
}



/* 使用原生 positioned I/O 完整写入，不修改共享文件游标。 */
static xtaskoutcome __xrtAsyncFileWriteTask(
	xcancel* pCancel,
	xrt_async_file_task* pTask,
	xtaskvalue* pResult
)
{
	xfilechange* pChange = __xrtAsyncFileChangeCreate(
		pTask->Offset,
		pTask->Size
	);
	size_t iDone = 0;

	if ( pChange == NULL ) {
		return XTASK_FAILED;
	}
	if ( xrtCancelRequested(pCancel) ) {
		xrtFree(pChange);
		return XTASK_CANCELLED;
	}
	if ( !xrtWriteAtFull(
		pTask->File->File,
		pTask->Offset,
		pTask->Data,
		(size_t)pTask->Size,
		&iDone
	) ) {
		xrtFree(pChange);
		__xrtFileAsyncError(
			XFILE_ASYNC_ERROR_WRITE,
			"write-file",
			"failed to write the asynchronous file"
		);
		return XTASK_FAILED;
	}
	pChange->Size = iDone;
	pResult->Value = pChange;
	pResult->Destroy = __xrtFileAsyncValueFree;
	return XTASK_SUCCESS;
}



/* 刷新文件；只读对象沿用同步文件层的成功空操作语义。 */
static xtaskoutcome __xrtAsyncFileFlushTask(
	xcancel* pCancel,
	xrt_async_file_task* pTask
)
{
	if ( xrtCancelRequested(pCancel) ) {
		return XTASK_CANCELLED;
	}
	if ( ((pTask->File->Flags & XFILE_WRITE) != 0u) &&
		!xrtFlush(pTask->File->File) ) {
		__xrtFileAsyncError(
			XFILE_ASYNC_ERROR_FLUSH,
			"flush-file",
			"failed to flush the asynchronous file"
		);
		return XTASK_FAILED;
	}
	return XTASK_SUCCESS;
}



/* 查询文件大小并返回拥有型标量结果。 */
static xtaskoutcome __xrtAsyncFileSizeTask(
	xcancel* pCancel,
	xrt_async_file_task* pTask,
	xtaskvalue* pResult
)
{
	xfilesize* pSize = __xrtAsyncFileSizeCreate(0);
	uint64 iSize = 0;

	if ( pSize == NULL ) {
		return XTASK_FAILED;
	}
	if ( xrtCancelRequested(pCancel) ) {
		xrtFree(pSize);
		return XTASK_CANCELLED;
	}
	if ( !xrtFileSize(pTask->File->File, &iSize) ) {
		xrtFree(pSize);
		__xrtFileAsyncError(
			XFILE_ASYNC_ERROR_SIZE,
			"size-file",
			"failed to query the asynchronous file size"
		);
		return XTASK_FAILED;
	}
	pSize->Size = iSize;
	pResult->Value = pSize;
	pResult->Destroy = __xrtFileAsyncValueFree;
	return XTASK_SUCCESS;
}



/* 修改文件大小并返回新大小。 */
static xtaskoutcome __xrtAsyncFileResizeTask(
	xcancel* pCancel,
	xrt_async_file_task* pTask,
	xtaskvalue* pResult
)
{
	xfilechange* pChange = __xrtAsyncFileChangeCreate(
		0,
		pTask->Size
	);

	if ( pChange == NULL ) {
		return XTASK_FAILED;
	}
	if ( xrtCancelRequested(pCancel) ) {
		xrtFree(pChange);
		return XTASK_CANCELLED;
	}
	if ( !xrtFileResize(
		pTask->File->File,
		pTask->Size
	) ) {
		xrtFree(pChange);
		__xrtFileAsyncError(
			XFILE_ASYNC_ERROR_RESIZE,
			"resize-file",
			"failed to resize the asynchronous file"
		);
		return XTASK_FAILED;
	}
	pResult->Value = pChange;
	pResult->Destroy = __xrtFileAsyncValueFree;
	return XTASK_SUCCESS;
}



/* 把统一任务参数分派到单一文件操作。 */
static xtaskoutcome __xrtAsyncFileTask(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	xrt_async_file_task* pTask =
		(xrt_async_file_task*)pData;

	switch ( pTask->Operation ) {
		case XRT_ASYNC_FILE_READ:
			return __xrtAsyncFileReadTask(
				pCancel,
				pTask,
				pResult
			);

		case XRT_ASYNC_FILE_WRITE:
			return __xrtAsyncFileWriteTask(
				pCancel,
				pTask,
				pResult
			);

		case XRT_ASYNC_FILE_FLUSH:
			return __xrtAsyncFileFlushTask(
				pCancel,
				pTask
			);

		case XRT_ASYNC_FILE_SIZE:
			return __xrtAsyncFileSizeTask(
				pCancel,
				pTask,
				pResult
			);

		case XRT_ASYNC_FILE_RESIZE:
			return __xrtAsyncFileResizeTask(
				pCancel,
				pTask,
				pResult
			);
	}
	__xrtErrorSetInternal();
	return XTASK_FAILED;
}



/* 校验绝对偏移和操作范围能够由跨平台文件层表达。 */
static bool __xrtAsyncFileRange(
	uint64 iOffset,
	uint64 iSize
)
{
	if ( (iOffset > (uint64)INT64_MAX) ||
		(iSize > ((uint64)INT64_MAX - iOffset)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	return true;
}



/* 创建任务参数并取得一个文件引用。 */
static xrt_async_file_task* __xrtAsyncFileTaskCreate(
	xasyncfile* pFile,
	xrt_async_file_operation Operation,
	uint64 iOffset,
	uint64 iSize,
	size_t iExtra
)
{
	xrt_async_file_task* pTask;

	if ( !__xrtAsyncFileAcquire(pFile) ) {
		return NULL;
	}
	if ( iExtra > (SIZE_MAX - sizeof(xrt_async_file_task)) ) {
		__xrtAsyncFileRelease(pFile);
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	pTask = (xrt_async_file_task*)xrtCalloc(
		1,
		sizeof(xrt_async_file_task) + iExtra
	);
	if ( pTask == NULL ) {
		__xrtAsyncFileRelease(pFile);
		return NULL;
	}
	pTask->File = pFile;
	pTask->Operation = Operation;
	pTask->Offset = iOffset;
	pTask->Size = iSize;
	return pTask;
}



/* 提交任务，并在任务池拒绝时完整回滚调用方尚未转移的资源。 */
static xfuture* __xrtAsyncFileSubmit(
	xrt_async_file_task* pTask,
	bool bExternalData
)
{
	xtaskargs Args;
	xfuture* pFuture;

	memset(&Args, 0, sizeof(Args));
	Args.Destroy = __xrtAsyncFileTaskFree;
	pFuture = xrtTaskSubmit(
		pTask->File->Pool,
		__xrtAsyncFileTask,
		pTask,
		&Args
	);
	if ( pFuture == NULL ) {
		__xrtFileAsyncError(
			XFILE_ASYNC_ERROR_SUBMIT,
			"submit-file-operation",
			"failed to submit the asynchronous file operation"
		);
		if ( bExternalData ) {
			pTask->Data = NULL;
			pTask->Release = NULL;
			pTask->ReleaseContext = NULL;
		}
		__xrtAsyncFileTaskFree(pTask, NULL);
	}
	return pFuture;
}



/* 建立尚未采用原生文件的异步对象；失败时不接管任何文件。 */
static xasyncfile* __xrtAsyncFileCreate(
	xtaskpool* pPool,
	uint32 iFlags
)
{
	xasyncfile* pFile;

	if ( pPool == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pFile = (xasyncfile*)xrtCalloc(
		1,
		sizeof(xasyncfile)
	);
	if ( pFile == NULL ) {
		return NULL;
	}
	if ( !xrtMutexInit(&pFile->Lock) ) {
		xrtFree(pFile);
		return NULL;
	}
	pFile->ClosePromise = xrtPromiseCreate(
		&pFile->CloseFuture,
		NULL
	);
	if ( pFile->ClosePromise == NULL ) {
		(void)xrtMutexUnit(&pFile->Lock);
		xrtFree(pFile);
		return NULL;
	}
	pFile->Pool = pPool;
	pFile->References = 1;
	pFile->Flags = iFlags;
	return pFile;
}



/* 销毁尚未接管原生文件的异步对象。 */
static void __xrtAsyncFileCreateFree(xasyncfile* pFile)
{
	if ( pFile == NULL ) {
		return;
	}
	xrtPromiseDestroy(pFile->ClosePromise);
	xrtFutureDestroy(pFile->CloseFuture);
	(void)xrtMutexUnit(&pFile->Lock);
	xrtFree(pFile);
}



/* 打开文件并建立可等待关闭的对象生命周期。 */
XRT_API xasyncfile* xrtAsyncFileOpen(
	xtaskpool* pPool,
	cstr sPath,
	const xfileoptions* pOptions
)
{
	xasyncfile* pFile;
	uint32 iFlags = XFILE_READ;

	if ( (pPool == NULL) ||
		(sPath == NULL) ||
		(sPath[0] == '\0') ||
		((pOptions != NULL) &&
		 ((pOptions->Flags & XFILE_APPEND) != 0u)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pOptions != NULL ) {
		iFlags = pOptions->Flags;
	}
	pFile = __xrtAsyncFileCreate(pPool, iFlags);
	if ( pFile == NULL ) {
		return NULL;
	}
	pFile->File = xrtFileOpen(sPath, pOptions);
	if ( pFile->File == NULL ) {
		__xrtFileAsyncError(
			XFILE_ASYNC_ERROR_OPEN,
			"open-file",
			"failed to open the asynchronous file"
		);
		__xrtAsyncFileCreateFree(pFile);
		return NULL;
	}
	return pFile;
}



/* 采用已打开文件，并在成功后独占其关闭责任。 */
XRT_API xasyncfile* xrtAsyncFileAdopt(
	xtaskpool* pPool,
	xfile File
)
{
	xasyncfile* pFile;
	uint32 iFlags;

	if ( (pPool == NULL) || (File == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	iFlags = xrtFileFlags(File);
	if ( iFlags == 0u ) {
		return NULL;
	}
	if ( (iFlags & XFILE_APPEND) != 0u ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pFile = __xrtAsyncFileCreate(pPool, iFlags);
	if ( pFile == NULL ) {
		return NULL;
	}
	pFile->File = File;
	return pFile;
}



/* 返回异步文件采用时保存的不可变打开标志。 */
XRT_API uint32 xrtAsyncFileFlags(const xasyncfile* pFile)
{
	if ( pFile == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	return pFile->Flags;
}



/* 请求关闭并返回独立的完成 Future。 */
XRT_API xfuture* xrtAsyncFileClose(xasyncfile* pFile)
{
	xfuture* pFuture;

	if ( pFile == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	(void)xrtMutexLock(&pFile->Lock);
	if ( pFile->Closing ) {
		(void)xrtMutexUnlock(&pFile->Lock);
		__xrtErrorSetClosed();
		return NULL;
	}
	pFile->Closing = true;
	pFuture = xrtFutureRef(pFile->CloseFuture);
	if ( pFuture == NULL ) {
		pFile->Closing = false;
		(void)xrtMutexUnlock(&pFile->Lock);
		return NULL;
	}
	(void)xrtMutexUnlock(&pFile->Lock);
	__xrtAsyncFileRelease(pFile);
	return pFuture;
}



/* 提交一次拥有型异步读取。 */
XRT_API xfuture* xrtAsyncFileReadAt(
	xasyncfile* pFile,
	uint64 iOffset,
	size_t iSize
)
{
	xrt_async_file_task* pTask;

	if ( !__xrtAsyncFileRange(
		iOffset,
		(uint64)iSize
	) ) {
		return NULL;
	}
	pTask = __xrtAsyncFileTaskCreate(
		pFile,
		XRT_ASYNC_FILE_READ,
		iOffset,
		(uint64)iSize,
		0
	);
	return pTask != NULL ?
		__xrtAsyncFileSubmit(pTask, false) : NULL;
}



/* 复制源数据并提交一次完整异步写入。 */
XRT_API xfuture* xrtAsyncFileWriteAt(
	xasyncfile* pFile,
	uint64 iOffset,
	xbytesview Data
)
{
	xrt_async_file_task* pTask;

	if ( ((Data.Size != 0) && (Data.Data == NULL)) ||
		!__xrtAsyncFileRange(
			iOffset,
			(uint64)Data.Size
		) ) {
		if ( (Data.Size != 0) &&
			(Data.Data == NULL) ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}
	pTask = __xrtAsyncFileTaskCreate(
		pFile,
		XRT_ASYNC_FILE_WRITE,
		iOffset,
		(uint64)Data.Size,
		Data.Size
	);
	if ( pTask == NULL ) {
		return NULL;
	}
	if ( Data.Size != 0 ) {
		pTask->Data = (cbytes)(pTask + 1);
		memcpy(
			(bytes)pTask->Data,
			Data.Data,
			Data.Size
		);
	}
	return __xrtAsyncFileSubmit(pTask, false);
}



/* 受理外部写入所有权；任务终态恰好释放一次。 */
XRT_API xfuture* xrtAsyncFileWriteAtRef(
	xasyncfile* pFile,
	uint64 iOffset,
	xbytesview Data,
	xfileasyncreleaseproc pRelease,
	ptr pContext
)
{
	xrt_async_file_task* pTask;

	if ( ((Data.Size != 0) &&
		 ((Data.Data == NULL) || (pRelease == NULL))) ||
		!__xrtAsyncFileRange(iOffset, (uint64)Data.Size) ) {
		if ( (Data.Size != 0) &&
			((Data.Data == NULL) || (pRelease == NULL)) ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}
	pTask = __xrtAsyncFileTaskCreate(
		pFile,
		XRT_ASYNC_FILE_WRITE,
		iOffset,
		(uint64)Data.Size,
		0
	);
	if ( pTask == NULL ) {
		return NULL;
	}
	if ( Data.Size != 0 ) {
		pTask->Data = Data.Data;
		pTask->Release = pRelease;
		pTask->ReleaseContext = pContext;
	}
	return __xrtAsyncFileSubmit(pTask, true);
}



/* 释放由 Take 成功转移给任务的数据。 */
static void __xrtAsyncFileTakeFree(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)iSize;
	xrtFree((ptr)pData);
}



/* 受理由 xrtMalloc 家族分配的数据，失败时不消费所有权。 */
XRT_API xfuture* xrtAsyncFileWriteAtTake(
	xasyncfile* pFile,
	uint64 iOffset,
	bytes pData,
	size_t iSize
)
{
	if ( ((pData == NULL) && (iSize != 0)) ||
		((pData != NULL) && (iSize == 0)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return xrtAsyncFileWriteAtRef(
		pFile,
		iOffset,
		(xbytesview) { pData, iSize },
		iSize == 0 ? NULL : __xrtAsyncFileTakeFree,
		NULL
	);
}



/* 提交文件刷新。 */
XRT_API xfuture* xrtAsyncFileFlush(xasyncfile* pFile)
{
	xrt_async_file_task* pTask =
		__xrtAsyncFileTaskCreate(
			pFile,
			XRT_ASYNC_FILE_FLUSH,
			0,
			0,
			0
		);

	return pTask != NULL ?
		__xrtAsyncFileSubmit(pTask, false) : NULL;
}



/* 提交文件大小查询。 */
XRT_API xfuture* xrtAsyncFileSize(xasyncfile* pFile)
{
	xrt_async_file_task* pTask =
		__xrtAsyncFileTaskCreate(
			pFile,
			XRT_ASYNC_FILE_SIZE,
			0,
			0,
			0
		);

	return pTask != NULL ?
		__xrtAsyncFileSubmit(pTask, false) : NULL;
}



/* 提交文件大小修改。 */
XRT_API xfuture* xrtAsyncFileResize(
	xasyncfile* pFile,
	uint64 iSize
)
{
	xrt_async_file_task* pTask;

	if ( iSize > (uint64)INT64_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	pTask = __xrtAsyncFileTaskCreate(
		pFile,
		XRT_ASYNC_FILE_RESIZE,
		0,
		iSize,
		0
	);
	return pTask != NULL ?
		__xrtAsyncFileSubmit(pTask, false) : NULL;
}

#endif
