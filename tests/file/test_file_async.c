#include "../test.h"



/* 阻塞任务用于确定性验证已受理文件操作和关闭的先后关系。 */
typedef struct testasyncfileblock {
	xmutex Lock;
	xcond Cond;
	bool Started;
	bool Release;
} testasyncfileblock;



/* 记录零复制写入释放次数和回调参数，验证所有权边界。 */
typedef struct testasyncfileref {
	xmutex Lock;
	xcond Cond;
	uint32 Releases;
	bool Mismatch;
	cbytes Data;
	size_t Size;
} testasyncfileref;



/* 初始化零复制释放同步状态。 */
static bool testAsyncFileRefInit(
	testasyncfileref* pRef,
	cbytes pData,
	size_t iSize
)
{
	memset(pRef, 0, sizeof(*pRef));
	if ( !xrtMutexInit(&pRef->Lock) ) {
		return false;
	}
	if ( !xrtCondInit(&pRef->Cond) ) {
		(void)xrtMutexUnit(&pRef->Lock);
		return false;
	}
	pRef->Data = pData;
	pRef->Size = iSize;
	return true;
}



/* 校验零复制写入只以原始 Span 发布一次释放。 */
static void testAsyncFileRefRelease(
	ptr pData,
	cbytes pBytes,
	size_t iSize
)
{
	testasyncfileref* pRef = (testasyncfileref*)pData;

	(void)xrtMutexLock(&pRef->Lock);
	if ( (pBytes != pRef->Data) || (iSize != pRef->Size) ) {
		pRef->Mismatch = true;
	}
	pRef->Releases++;
	(void)xrtCondBroadcast(&pRef->Cond);
	(void)xrtMutexUnlock(&pRef->Lock);
}



/* 等待任务析构发布零复制释放，避免只等待 Future 的时序空隙。 */
static bool testAsyncFileRefWait(
	testasyncfileref* pRef,
	uint32 iExpected
)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(2000000));
	bool bResult = xrtMutexLock(&pRef->Lock);

	while ( bResult && (pRef->Releases != iExpected) ) {
		bResult = xrtCondWaitUntil(
			&pRef->Cond,
			&pRef->Lock,
			Deadline
		) == XWAIT_OK;
	}
	bResult = bResult && !pRef->Mismatch;
	(void)xrtMutexUnlock(&pRef->Lock);
	return bResult;
}



/* 销毁零复制释放同步状态。 */
static bool testAsyncFileRefUnit(testasyncfileref* pRef)
{
	return xrtCondUnit(&pRef->Cond) &&
		xrtMutexUnit(&pRef->Lock);
}



/* 在系统临时目录下构造当前测试独占使用的路径。 */
static str testAsyncFilePath(cstr sName)
{
	str sDirectory = xrtPathTemp();
	str sPath;

	testRequire(sDirectory != NULL, "temporary directory query failed");
	sPath = xrtPathJoin(sDirectory, sName);
	xrtFree(sDirectory);
	testRequire(sPath != NULL, "temporary async file path allocation failed");
	return sPath;
}



/* 等待 Future 成功并返回其借用值。 */
static ptr testAsyncFileValue(xfuture* pFuture, cstr sMessage)
{
	testRequire(pFuture != NULL, sMessage);
	testRequire(
		xrtFutureWaitFor(
			pFuture,
			UINT64_C(2000000)
		) == XWAIT_OK,
		sMessage
	);
	testRequire(
		xrtFutureState(pFuture) == XFUTURE_RESOLVED,
		sMessage
	);
	return xrtFutureValue(pFuture);
}



/* 占住唯一工作线程，使后续文件任务稳定停留在有界队列中。 */
static xtaskoutcome testAsyncFileBlockTask(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testasyncfileblock* pBlock =
		(testasyncfileblock*)pData;
	bool bResult = true;

	(void)pCancel;
	(void)pResult;
	if ( !xrtMutexLock(&pBlock->Lock) ) {
		return XTASK_FAILED;
	}
	pBlock->Started = true;
	bResult = xrtCondBroadcast(&pBlock->Cond);
	while ( bResult && !pBlock->Release ) {
		bResult = xrtCondWait(
			&pBlock->Cond,
			&pBlock->Lock
		) == XWAIT_OK;
	}
	(void)xrtMutexUnlock(&pBlock->Lock);
	return bResult ? XTASK_SUCCESS : XTASK_FAILED;
}



/* 等待阻塞任务进入工作线程。 */
static bool testAsyncFileBlockStarted(
	testasyncfileblock* pBlock
)
{
	bool bResult = xrtMutexLock(&pBlock->Lock);

	while ( bResult && !pBlock->Started ) {
		bResult = xrtCondWaitFor(
			&pBlock->Cond,
			&pBlock->Lock,
			UINT64_C(2000000)
		) == XWAIT_OK;
	}
	(void)xrtMutexUnlock(&pBlock->Lock);
	return bResult;
}



/* 允许阻塞任务退出并运行排队文件操作。 */
static bool testAsyncFileBlockRelease(
	testasyncfileblock* pBlock
)
{
	bool bResult = xrtMutexLock(&pBlock->Lock);

	if ( bResult ) {
		pBlock->Release = true;
		bResult = xrtCondBroadcast(&pBlock->Cond);
		(void)xrtMutexUnlock(&pBlock->Lock);
	}
	return bResult;
}



/* 排队任务用于验证资源回收通道不会占用普通任务槽位。 */
static xtaskoutcome testAsyncFileCountTask(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	size_t* pHits = (size_t*)pData;

	(void)pCancel;
	(void)pResult;
	(*pHits)++;
	return XTASK_SUCCESS;
}



/* 绝对偏移读写、复制所有权、EOF、大小、调整和刷新必须口径一致。 */
static void testAsyncFileOperations(
	xtaskpool* pPool,
	cstr sPath
)
{
	xfileoptions Options;
	xasyncfile* pFile;
	xfuture* pFuture;
	xfilechange* pChange;
	xfilesize* pSize;
	xfiledata* pData;
	testasyncfileref Ref;
	unsigned char arrSource[] = "hello";
	static const uint8 arrRef[] = "ref";
	bytes pTake;

	(void)xrtFileDelete(sPath);
	xrtClearError();
	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_READ | XFILE_WRITE |
		XFILE_CREATE | XFILE_TRUNCATE;
	pFile = xrtAsyncFileOpen(pPool, sPath, &Options);
	testRequire(pFile != NULL, "async file open failed");

	pFuture = xrtAsyncFileWriteAt(
		pFile,
		2,
		(xbytesview){ arrSource, 5 }
	);
	memset(arrSource, 'x', 5);
	pChange = (xfilechange*)testAsyncFileValue(
		pFuture,
		"async file write failed"
	);
	testRequire(
		(pChange != NULL) &&
		(pChange->Offset == 2) &&
		(pChange->Size == 5),
		"async file write result mismatch"
	);
	xrtFutureDestroy(pFuture);

	pFuture = xrtAsyncFileSize(pFile);
	pSize = (xfilesize*)testAsyncFileValue(
		pFuture,
		"async file size failed"
	);
	testRequire(
		(pSize != NULL) && (pSize->Size == 7),
		"async file size result mismatch"
	);
	xrtFutureDestroy(pFuture);

	pFuture = xrtAsyncFileReadAt(pFile, 0, 8);
	pData = (xfiledata*)testAsyncFileValue(
		pFuture,
		"async file read failed"
	);
	testRequire(
		(pData != NULL) &&
		(pData->Offset == 0) &&
		(pData->Size == 7) &&
		pData->End &&
		(pData->Data[0] == 0) &&
		(pData->Data[1] == 0) &&
		(memcmp(pData->Data + 2, "hello", 5) == 0) &&
		(pData->Data[7] == 0),
		"async file read data or EOF mismatch"
	);
	xrtFutureDestroy(pFuture);

	pFuture = xrtAsyncFileFlush(pFile);
	(void)testAsyncFileValue(
		pFuture,
		"async file flush failed"
	);
	xrtFutureDestroy(pFuture);

	pFuture = xrtAsyncFileResize(pFile, 4);
	pChange = (xfilechange*)testAsyncFileValue(
		pFuture,
		"async file resize failed"
	);
	testRequire(
		(pChange != NULL) && (pChange->Size == 4),
		"async file resize result mismatch"
	);
	xrtFutureDestroy(pFuture);

	pFuture = xrtAsyncFileReadAt(pFile, 2, 4);
	pData = (xfiledata*)testAsyncFileValue(
		pFuture,
		"async file short read failed"
	);
	testRequire(
		(pData != NULL) &&
		(pData->Size == 2) &&
		pData->End &&
		(memcmp(pData->Data, "he", 2) == 0) &&
		(pData->Data[2] == 0),
		"async file short read mismatch"
	);
	xrtFutureDestroy(pFuture);

	testRequire(
		testAsyncFileRefInit(&Ref, arrRef, 3),
		"async file reference state init failed"
	);
	pFuture = xrtAsyncFileWriteAtRef(
		pFile,
		4,
		(xbytesview) { arrRef, 3 },
		testAsyncFileRefRelease,
		&Ref
	);
	pChange = (xfilechange*)testAsyncFileValue(
		pFuture,
		"async file reference write failed"
	);
	testRequire(
		(pChange != NULL) &&
		(pChange->Offset == 4) &&
		(pChange->Size == 3),
		"async file reference write result mismatch"
	);
	xrtFutureDestroy(pFuture);
	testRequire(
		testAsyncFileRefWait(&Ref, 1),
		"async file reference release mismatch"
	);
	testRequire(
		testAsyncFileRefUnit(&Ref),
		"async file reference state cleanup failed"
	);

	pTake = (bytes)xrtMalloc(4);
	testRequire(pTake != NULL, "async file take allocation failed");
	memcpy(pTake, "take", 4);
	pFuture = xrtAsyncFileWriteAtTake(pFile, 7, pTake, 4);
	pChange = (xfilechange*)testAsyncFileValue(
		pFuture,
		"async file take write failed"
	);
	testRequire(
		(pChange != NULL) &&
		(pChange->Offset == 7) &&
		(pChange->Size == 4),
		"async file take write result mismatch"
	);
	xrtFutureDestroy(pFuture);

	pFuture = xrtAsyncFileReadAt(pFile, 0, 11);
	pData = (xfiledata*)testAsyncFileValue(
		pFuture,
		"async file zero-copy readback failed"
	);
	testRequire(
		(pData != NULL) &&
		(pData->Size == 11) &&
		(memcmp(pData->Data, "\0\0hereftake", 11) == 0),
		"async file zero-copy readback mismatch"
	);
	xrtFutureDestroy(pFuture);

	pFuture = xrtAsyncFileClose(pFile);
	(void)testAsyncFileValue(
		pFuture,
		"async file close failed"
	);
	xrtFutureDestroy(pFuture);
}



/* 只读文件刷新沿用旧版成功空操作语义。 */
static void testAsyncFileReadOnlyFlush(
	xtaskpool* pPool,
	cstr sPath
)
{
	xasyncfile* pFile = xrtAsyncFileOpen(
		pPool,
		sPath,
		NULL
	);
	xfuture* pFlush;
	xfuture* pClose;

	testRequire(pFile != NULL, "read-only async file open failed");
	pFlush = xrtAsyncFileFlush(pFile);
	(void)testAsyncFileValue(
		pFlush,
		"read-only async file flush failed"
	);
	xrtFutureDestroy(pFlush);
	pClose = xrtAsyncFileClose(pFile);
	(void)testAsyncFileValue(
		pClose,
		"read-only async file close failed"
	);
	xrtFutureDestroy(pClose);
}



/* 采用成功后由异步对象独占关闭责任，并保留原句柄的访问标志。 */
static void testAsyncFileAdopt(
	xtaskpool* pPool,
	cstr sPath
)
{
	xfile File = xrtOpen(sPath, XFILE_READ);
	xasyncfile* pFile;
	xfuture* pFuture;
	xfiledata* pData;

	testRequire(File != NULL, "file for async adoption open failed");
	pFile = xrtAsyncFileAdopt(pPool, File);
	testRequire(pFile != NULL, "async file adoption failed");
	testRequire(
		xrtAsyncFileFlags(pFile) == XFILE_READ,
		"adopted async file flags mismatch"
	);

	pFuture = xrtAsyncFileReadAt(pFile, 0, 4);
	pData = (xfiledata*)testAsyncFileValue(
		pFuture,
		"adopted async file read failed"
	);
	testRequire(
		(pData != NULL) &&
		(pData->Size == 4) &&
		(memcmp(pData->Data, "\0\0he", 4) == 0),
		"adopted async file data mismatch"
	);
	xrtFutureDestroy(pFuture);

	pFuture = xrtAsyncFileClose(pFile);
	(void)testAsyncFileValue(
		pFuture,
		"adopted async file close failed"
	);
	xrtFutureDestroy(pFuture);
}



/* 已受理任务持有文件，关闭 Future 必须等排队取消被任务池确认。 */
static void testAsyncFileCloseOrdering(cstr sPath)
{
	xtaskpoolconfig Config = { 1, 1, 0 };
	testasyncfileblock Block;
	xtaskpool* pPool;
	xasyncfile* pFile;
	xfuture* pBlockFuture;
	xfuture* pReadFuture;
	xfuture* pCloseFuture;
	const xerror* pError;
	testasyncfileref AcceptedRef;
	testasyncfileref RejectedRef;
	static const uint8 arrRef[] = "queued-ref";

	testRequire(
		xrtMutexInit(&Block.Lock),
		"async file ordering mutex init failed"
	);
	testRequire(
		xrtCondInit(&Block.Cond),
		"async file ordering condition init failed"
	);
	Block.Started = false;
	Block.Release = false;
	testRequire(
		testAsyncFileRefInit(
			&AcceptedRef,
			arrRef,
			sizeof(arrRef) - 1u
		) && testAsyncFileRefInit(
			&RejectedRef,
			arrRef,
			sizeof(arrRef) - 1u
		),
		"async file ordering reference state init failed"
	);
	pPool = xrtTaskPoolCreate(&Config);
	testRequire(pPool != NULL, "async file ordering pool create failed");
	pFile = xrtAsyncFileOpen(pPool, sPath, NULL);
	testRequire(pFile != NULL, "async file ordering open failed");
	pBlockFuture = xrtTaskSubmit(
		pPool,
		testAsyncFileBlockTask,
		&Block,
		NULL
	);
	testRequire(pBlockFuture != NULL, "async file blocker submit failed");
	testRequire(
		testAsyncFileBlockStarted(&Block),
		"async file blocker did not start"
	);

	pReadFuture = xrtAsyncFileWriteAtRef(
		pFile,
		0,
		(xbytesview) { arrRef, sizeof(arrRef) - 1u },
		testAsyncFileRefRelease,
		&AcceptedRef
	);
	testRequire(pReadFuture != NULL, "queued async file reference write failed");
	testRequire(
		xrtAsyncFileWriteAtRef(
			pFile,
			0,
			(xbytesview) { arrRef, sizeof(arrRef) - 1u },
			testAsyncFileRefRelease,
			&RejectedRef
		) == NULL,
		"full async file queue accepted another reference write"
	);
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.file.async"
		) == 0) &&
		(xrtErrorCode(pError) == XFILE_ASYNC_ERROR_SUBMIT) &&
		(xrtErrorCause(pError) != NULL) &&
		(xrtErrorKind(
			xrtErrorCause(pError)
		) == XERR_AGAIN),
		"async file queue backpressure error mismatch"
	);
	testRequire(
		(AcceptedRef.Releases == 0) &&
		(RejectedRef.Releases == 0),
		"async file queue rejection consumed reference ownership"
	);
	xrtClearError();
	pCloseFuture = xrtAsyncFileClose(pFile);
	testRequire(pCloseFuture != NULL, "queued async file close failed");
	testRequire(
		xrtFutureState(pCloseFuture) == XFUTURE_PENDING,
		"async file close completed before accepted work"
	);
	testRequire(
		xrtFutureCancel(pReadFuture),
		"queued async file reference write cancellation failed"
	);
	testRequire(
		testAsyncFileBlockRelease(&Block),
		"async file blocker release failed"
	);
	testRequire(
			xrtFutureWaitFor(
			pBlockFuture,
			UINT64_C(2000000)
		) == XWAIT_OK,
		"async file blocker wait failed"
	);
	testRequire(
		xrtFutureWaitFor(
			pReadFuture,
			UINT64_C(2000000)
		) == XWAIT_OK,
		"cancelled async file reference write wait failed"
	);
	testRequire(
		xrtFutureState(pReadFuture) == XFUTURE_CANCELLED,
		"queued async file reference write was not cancelled"
	);
	testRequire(
		testAsyncFileRefWait(&AcceptedRef, 1) &&
		(RejectedRef.Releases == 0),
		"cancelled or rejected async file reference ownership mismatch"
	);
	(void)testAsyncFileValue(
		pCloseFuture,
		"async file close did not follow accepted work"
	);

	xrtFutureDestroy(pBlockFuture);
	xrtFutureDestroy(pReadFuture);
	xrtFutureDestroy(pCloseFuture);
	testRequire(
		xrtTaskPoolDestroy(pPool),
		"async file ordering pool destroy failed"
	);
	testRequire(
		xrtCondUnit(&Block.Cond) &&
		xrtMutexUnit(&Block.Lock),
		"async file ordering synchronization cleanup failed"
	);
	testRequire(
		testAsyncFileRefUnit(&AcceptedRef) &&
		testAsyncFileRefUnit(&RejectedRef),
		"async file ordering reference state cleanup failed"
	);
}



/*
	关闭必须绕过已满的普通队列，并且在任务池停止普通提交后仍能完成。
	这同时证明调用线程不会直接执行原生文件关闭。
*/
static void testAsyncFileCloseFinalizer(cstr sPath)
{
	xtaskpoolconfig Config = { 1, 1, 0 };
	testasyncfileblock Block;
	xtaskpool* pPool;
	xasyncfile* pFile;
	xfuture* pBlockFuture;
	xfuture* pQueuedFuture;
	xfuture* pCloseFuture;
	size_t iHits = 0;

	testRequire(
		xrtMutexInit(&Block.Lock),
		"async file finalizer mutex init failed"
	);
	testRequire(
		xrtCondInit(&Block.Cond),
		"async file finalizer condition init failed"
	);
	Block.Started = false;
	Block.Release = false;
	pPool = xrtTaskPoolCreate(&Config);
	testRequire(pPool != NULL, "async file finalizer pool create failed");
	pFile = xrtAsyncFileOpen(pPool, sPath, NULL);
	testRequire(pFile != NULL, "async file finalizer open failed");
	pBlockFuture = xrtTaskSubmit(
		pPool,
		testAsyncFileBlockTask,
		&Block,
		NULL
	);
	testRequire(pBlockFuture != NULL, "async file finalizer blocker submit failed");
	testRequire(
		testAsyncFileBlockStarted(&Block),
		"async file finalizer blocker did not start"
	);
	pQueuedFuture = xrtTaskSubmit(
		pPool,
		testAsyncFileCountTask,
		&iHits,
		NULL
	);
	testRequire(
		pQueuedFuture != NULL,
		"async file finalizer queue fill failed"
	);

	pCloseFuture = xrtAsyncFileClose(pFile);
	testRequire(
		pCloseFuture != NULL,
		"full task queue rejected async file finalizer"
	);
	testRequire(
		xrtFutureState(pCloseFuture) == XFUTURE_PENDING,
		"async file close ran synchronously on the caller"
	);
	testRequire(
		testAsyncFileBlockRelease(&Block),
		"async file finalizer blocker release failed"
	);
	(void)testAsyncFileValue(
		pCloseFuture,
		"async file finalizer did not complete"
	);
	(void)testAsyncFileValue(
		pQueuedFuture,
		"queued task did not survive async file finalizer"
	);
	testRequire(iHits == 1, "queued task execution count mismatch");
	xrtFutureDestroy(pCloseFuture);
	xrtFutureDestroy(pQueuedFuture);
	xrtFutureDestroy(pBlockFuture);
	testRequire(
		xrtTaskPoolDestroy(pPool),
		"async file finalizer pool destroy failed"
	);
	testRequire(
		xrtCondUnit(&Block.Cond) &&
		xrtMutexUnit(&Block.Lock),
		"async file finalizer synchronization cleanup failed"
	);

	/* Close 只停止普通提交，资源回收线程保留到 Destroy。 */
	pPool = xrtTaskPoolCreate(&Config);
	testRequire(
		pPool != NULL,
		"closed async file finalizer pool create failed"
	);
	pFile = xrtAsyncFileOpen(pPool, sPath, NULL);
	testRequire(
		pFile != NULL,
		"closed async file finalizer open failed"
	);
	testRequire(
		xrtTaskPoolClose(pPool),
		"task pool close before finalizer failed"
	);
	pCloseFuture = xrtAsyncFileClose(pFile);
	(void)testAsyncFileValue(
		pCloseFuture,
		"closed task pool did not execute resource finalizer"
	);
	xrtFutureDestroy(pCloseFuture);
	testRequire(
		xrtTaskPoolDestroy(pPool),
		"closed async file finalizer pool destroy failed"
	);
}



/* 参数错误和底层打开错误必须提供稳定的同步失败契约。 */
static void testAsyncFileErrors(
	xtaskpool* pPool,
	cstr sPath
)
{
	xfileoptions Options;
	xasyncfile* pFile;
	xfile File;
	const xerror* pError;
	const xerror* pCause;
	str sMissing = testAsyncFilePath(
		"xrt-file-async-missing.tmp"
	);

	(void)xrtFileDelete(sMissing);
	xrtClearError();
	pFile = xrtAsyncFileOpen(pPool, sMissing, NULL);
	testRequire(pFile == NULL, "missing async file open succeeded");
	pError = xrtGetError();
	pCause = xrtErrorCause(pError);
	testRequire(
		(pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.file.async") == 0) &&
		(xrtErrorCode(pError) == XFILE_ASYNC_ERROR_OPEN) &&
		(pCause != NULL) &&
		(strcmp(xrtErrorDomain(pCause), "xrt.file") == 0) &&
		(xrtErrorKind(pCause) == XERR_NOT_FOUND),
		"async file open error chain mismatch"
	);
	xrtClearError();

	File = xrtOpen(sPath, XFILE_READ | XFILE_WRITE | XFILE_APPEND);
	testRequire(File != NULL, "append file for adoption open failed");
	testRequire(
		xrtAsyncFileAdopt(pPool, File) == NULL,
		"append file adoption was accepted"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"append file adoption error mismatch"
	);
	xrtClearError();
	testRequire(
		xrtClose(File),
		"failed adoption consumed the source file"
	);

	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_WRITE | XFILE_APPEND;
	testRequire(
		xrtAsyncFileOpen(pPool, sPath, &Options) == NULL,
		"append async file open was accepted"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"append async file error mismatch"
	);
	xrtClearError();
	testRequire(
		xrtAsyncFileReadAt(
			NULL,
			0,
			0
		) == NULL,
		"null async file read was accepted"
	);
	testRequire(
		xrtAsyncFileClose(NULL) == NULL,
		"null async file close was accepted"
	);
	xrtClearError();
	xrtFree(sMissing);
}



/* 异步文件回归入口。 */
int main(void)
{
	xtaskpoolconfig Config = { 2, 32, 0 };
	xtaskpool* pPool = xrtTaskPoolCreate(&Config);
	str sPath = testAsyncFilePath(
		"xrt-file-async-\xE8\xBE\xB9\xE7\x95\x8C.tmp"
	);

	testRequire(pPool != NULL, "async file task pool create failed");
	testAsyncFileOperations(pPool, sPath);
	testAsyncFileReadOnlyFlush(pPool, sPath);
	testAsyncFileAdopt(pPool, sPath);
	testAsyncFileErrors(pPool, sPath);
	testRequire(
		xrtTaskPoolDestroy(pPool),
		"async file task pool destroy failed"
	);
	testAsyncFileCloseOrdering(sPath);
	testAsyncFileCloseFinalizer(sPath);
	testRequire(
		xrtFileDelete(sPath),
		"async file cleanup failed"
	);
	xrtFree(sPath);
	printf("[PASS] async file\n");
	return 0;
}
