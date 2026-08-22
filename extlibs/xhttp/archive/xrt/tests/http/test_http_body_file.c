#include "../test.h"



/* 阻塞唯一工作线程，以确定性验证准备任务的有界队列行为。 */
typedef struct test_http_body_file_block {
	xmutex Lock;
	xcond Cond;
	bool Started;
	bool Release;
} test_http_body_file_block;



/* 在系统临时目录下构造当前测试独占的文件路径。 */
static str testHttpBodyFilePath(cstr sName)
{
	str sDirectory = xrtPathTemp();
	str sPath;

	testRequire(sDirectory != NULL, "HTTP file body temp directory failed");
	sPath = xrtPathJoin(sDirectory, sName);
	xrtFree(sDirectory);
	testRequire(sPath != NULL, "HTTP file body temp path failed");
	return sPath;
}



/* 创建固定测试文件。 */
static void testHttpBodyFileWrite(cstr sPath, cstr sData)
{
	xfile File;
	size_t iSize = strlen(sData);

	(void)xrtFileDelete(sPath);
	xrtClearError();
	File = xrtOpen(
		sPath,
		XFILE_WRITE | XFILE_CREATE |
		XFILE_EXCLUSIVE
	);
	testRequire(File != NULL, "HTTP file body fixture open failed");
	testRequire(
		xrtWriteFull(File, sData, iSize, NULL),
		"HTTP file body fixture write failed"
	);
	testRequire(xrtClose(File), "HTTP file body fixture close failed");
}



/* 等待准备 Future，并取得脱离 Future 生命周期的正文引用。 */
static xhttpbody* testHttpBodyFilePrepared(
	xfuture* pFuture,
	cstr sMessage
)
{
	xhttpbody* pBody;

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
	pBody = (xhttpbody*)xrtFutureValue(pFuture);
	testRequire(pBody != NULL, sMessage);
	pBody = xrtHttpBodyRef(pBody);
	testRequire(pBody != NULL, sMessage);
	xrtFutureDestroy(pFuture);
	return pBody;
}



/* 等待一次 Reader 可读性 Future。 */
static void testHttpBodyFileWait(
	xhttpbodyreader* pReader,
	cstr sMessage
)
{
	xfuture* pFuture = xrtHttpBodyReaderWait(pReader);

	testRequire(pFuture != NULL, sMessage);
	testRequire(
		xrtFutureWaitFor(
			pFuture,
			UINT64_C(2000000)
		) == XWAIT_OK,
		sMessage
	);
	xrtFutureDestroy(pFuture);
}



/* 完整文件正文必须按调用方上限拆分，并且不携带固定连接缓冲。 */
static void testHttpBodyFileFull(
	xtaskpool* pPool,
	cstr sPath
)
{
	xhttpbody* pBody = testHttpBodyFilePrepared(
		xrtHttpBodyFileFuture(pPool, sPath, NULL),
		"full HTTP file body preparation failed"
	);
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	unsigned char arrData[16];
	size_t iUsed = 0;
	xhttpbodystatus Status;

	testRequire(
		(xrtHttpBodyLength(pBody) == 10) &&
		!xrtHttpBodyReplayable(pBody),
		"full HTTP file body metadata mismatch"
	);
	pReader = xrtHttpBodyOpen(pBody);
	testRequire(pReader != NULL, "full HTTP file body open failed");
	testRequire(
		xrtHttpBodyOpen(pBody) == NULL,
		"non-replayable HTTP file body reopened"
	);
	xrtClearError();

	Status = xrtHttpBodyNext(pReader, 6, &Chunk);
	testRequire(
		Status == XHTTP_BODY_AGAIN,
		"full HTTP file body did not start asynchronously"
	);
	testHttpBodyFileWait(pReader, "full HTTP file body first wait failed");
	Status = xrtHttpBodyNext(pReader, 2, &Chunk);
	testRequire(
		(Status == XHTTP_BODY_DATA) &&
		(Chunk.Size == 2),
		"HTTP file body did not honor a reduced chunk limit"
	);
	memcpy(arrData + iUsed, Chunk.Data, Chunk.Size);
	iUsed += Chunk.Size;
	xrtHttpBodyChunkRelease(&Chunk);

	for ( ;; ) {
		Status = xrtHttpBodyNext(pReader, 3, &Chunk);
		if ( Status == XHTTP_BODY_AGAIN ) {
			testHttpBodyFileWait(
				pReader,
				"full HTTP file body wait failed"
			);
			continue;
		}
		if ( Status == XHTTP_BODY_EOF ) {
			break;
		}
		testRequire(
			(Status == XHTTP_BODY_DATA) &&
			((iUsed + Chunk.Size) <= sizeof(arrData)),
			"full HTTP file body read failed"
		);
		memcpy(arrData + iUsed, Chunk.Data, Chunk.Size);
		iUsed += Chunk.Size;
		xrtHttpBodyChunkRelease(&Chunk);
	}
	testRequire(
		(iUsed == 10) &&
		(memcmp(arrData, "0123456789", 10) == 0),
		"full HTTP file body bytes mismatch"
	);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
}



/* 区间正文必须严格保留偏移、长度、零长度和 Chunk 独立租约。 */
static void testHttpBodyFileRange(
	xtaskpool* pPool,
	cstr sPath
)
{
	xhttpbody* pBody = testHttpBodyFilePrepared(
		xrtHttpBodyFileRangeFuture(pPool, sPath, 3, 4, NULL),
		"range HTTP file body preparation failed"
	);
	xhttpbodyreader* pReader = xrtHttpBodyOpen(pBody);
	xhttpbodychunk Chunk;

	testRequire(
		(pReader != NULL) &&
		(xrtHttpBodyLength(pBody) == 4),
		"range HTTP file body metadata mismatch"
	);
	testRequire(
		xrtHttpBodyNext(
			pReader,
			4,
			&Chunk
		) == XHTTP_BODY_AGAIN,
		"range HTTP file body did not wait"
	);
	testHttpBodyFileWait(pReader, "range HTTP file body wait failed");
	testRequire(
		(xrtHttpBodyNext(
			pReader,
			4,
			&Chunk
		) == XHTTP_BODY_DATA) &&
		(Chunk.Size == 4) &&
		(memcmp(Chunk.Data, "3456", 4) == 0),
		"range HTTP file body bytes mismatch"
	);

	/* Reader 和 Body 可以先释放，Chunk 的 Future 租约仍保护数据。 */
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	testRequire(
		memcmp(Chunk.Data, "3456", 4) == 0,
		"HTTP file body chunk did not outlive its reader"
	);
	xrtHttpBodyChunkRelease(&Chunk);

	pBody = testHttpBodyFilePrepared(
		xrtHttpBodyFileRangeFuture(pPool, sPath, 10, 0, NULL),
		"zero range HTTP file body preparation failed"
	);
	pReader = xrtHttpBodyOpen(pBody);
	testRequire(
		(pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader,
			1,
			&Chunk
		) == XHTTP_BODY_EOF),
		"zero range HTTP file body was not empty"
	);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
}



/* 可配置读取粒度必须按需限流，并在构造时复制未对齐配置。 */
static void testHttpBodyFileReadSize(
	xtaskpool* pPool,
	cstr sPath
)
{
	uint8 arrConfig[sizeof(xhttpbodyfileconfig) + 2u];
	xhttpbodyfileconfig Config;
	xhttpbodyfileconfig Snapshot;
	xfile File;
	xasyncfile* pAsync;
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xfuture* pClose;

	memset(arrConfig, 0xA5, sizeof(arrConfig));
	xrtHttpBodyFileConfigInit(
		(xhttpbodyfileconfig*)(arrConfig + 1u)
	);
	memcpy(&Snapshot, arrConfig + 1u, sizeof(Snapshot));
	testRequire(
		(Snapshot.ReadSize == XHTTP_BODY_FILE_READ_DEFAULT) &&
		(arrConfig[0] == 0xA5) &&
		(arrConfig[sizeof(arrConfig) - 1u] == 0xA5),
		"HTTP file body unaligned config init mismatch"
	);
	Config.ReadSize = 4;
	memcpy(arrConfig + 1u, &Config, sizeof(Config));

	File = xrtOpen(sPath, XFILE_READ);
	testRequire(File != NULL, "HTTP file body read-size open failed");
	pAsync = xrtAsyncFileAdopt(pPool, File);
	testRequire(pAsync != NULL, "HTTP file body read-size adopt failed");
	pBody = xrtHttpBodyFileAdopt(
		pAsync,
		0,
		10,
		(const xhttpbodyfileconfig*)(arrConfig + 1u)
	);
	testRequire(pBody != NULL, "HTTP file body read-size create failed");
	Config.ReadSize = 2;
	memcpy(arrConfig + 1u, &Config, sizeof(Config));
	pReader = xrtHttpBodyOpen(pBody);
	testRequire(pReader != NULL, "HTTP file body read-size open failed");
	testRequire(
		xrtHttpBodyNext(
			pReader,
			SIZE_MAX,
			&Chunk
		) == XHTTP_BODY_AGAIN,
		"HTTP file body read-size did not submit asynchronously"
	);
	testHttpBodyFileWait(pReader, "HTTP file body read-size wait failed");
	testRequire(
		(xrtHttpBodyNext(
			pReader,
			SIZE_MAX,
			&Chunk
		) == XHTTP_BODY_DATA) &&
		(Chunk.Size == 4) &&
		(memcmp(Chunk.Data, "0123", 4) == 0),
		"HTTP file body did not enforce its copied read size"
	);
	xrtHttpBodyChunkRelease(&Chunk);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);

	Config.ReadSize = 0;
	testRequire(
		xrtHttpBodyFileFuture(pPool, sPath, &Config) == NULL,
		"HTTP file body accepted a zero read size"
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP file body zero read-size error mismatch"
	);
	xrtClearError();

	File = xrtOpen(sPath, XFILE_READ);
	testRequire(File != NULL, "invalid-config source open failed");
	pAsync = xrtAsyncFileAdopt(pPool, File);
	testRequire(pAsync != NULL, "invalid-config async adopt failed");
	testRequire(
		xrtHttpBodyFileAdopt(pAsync, 0, 10, &Config) == NULL,
		"HTTP file body adopted a file with invalid config"
	);
	pClose = xrtAsyncFileClose(pAsync);
	testRequire(
		(pClose != NULL) &&
		(xrtFutureWaitFor(
			pClose,
			UINT64_C(2000000)
		) == XWAIT_OK),
		"invalid-config HTTP file body consumed its source"
	);
	xrtFutureDestroy(pClose);
}



/* 文件在准备后缩短时必须稳定失败，不能伪造声明长度。 */
static void testHttpBodyFileShrink(
	xtaskpool* pPool,
	cstr sPath
)
{
	xhttpbody* pBody = testHttpBodyFilePrepared(
		xrtHttpBodyFileFuture(pPool, sPath, NULL),
		"shrinking HTTP file body preparation failed"
	);
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	const xerror* pError;

	testRequire(
		xrtFileSetSize(sPath, 3),
		"HTTP file body shrink fixture failed"
	);
	pReader = xrtHttpBodyOpen(pBody);
	testRequire(pReader != NULL, "shrinking HTTP file body open failed");
	testRequire(
		xrtHttpBodyNext(
			pReader,
			10,
			&Chunk
		) == XHTTP_BODY_AGAIN,
		"shrinking HTTP file body did not start a read"
	);
	testHttpBodyFileWait(pReader, "shrinking HTTP file body wait failed");
	testRequire(
		xrtHttpBodyNext(
			pReader,
			10,
			&Chunk
		) == XHTTP_BODY_ERROR,
		"shrinking HTTP file body did not fail"
	);
	pError = xrtHttpBodyReaderError(pReader);
	testRequire(
		(pError != NULL) &&
		(strcmp(
			xrtErrorDomain(pError),
			"http.body.file"
		) == 0) &&
		(xrtErrorCode(pError) == XHTTP_BODY_FILE_ERROR_READ),
		"shrinking HTTP file body error mismatch"
	);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtClearError();
	testHttpBodyFileWrite(sPath, "0123456789");
}



/* 缺失文件、越界区间和只写采用必须在提交或准备边界失败。 */
static void testHttpBodyFileErrors(
	xtaskpool* pPool,
	cstr sPath
)
{
	str sMissing = testHttpBodyFilePath(
		"xrt-http-body-file-missing.tmp"
	);
	xfuture* pFuture;
	const xerror* pError;
	const xerror* pCause;
	bool bChainValid;
	xfile File;
	xasyncfile* pAsync;
	xfuture* pClose;

	(void)xrtFileDelete(sMissing);
	xrtClearError();
	pFuture = xrtHttpBodyFileFuture(pPool, sMissing, NULL);
	testRequire(pFuture != NULL, "missing HTTP file body was not submitted");
	testRequire(
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(2000000)
		) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_FAILED),
		"missing HTTP file body did not fail"
	);
	pError = xrtFutureError(pFuture);
	pCause = xrtErrorCause(pError);
	bChainValid =
		(pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "http.body.file") == 0) &&
		(xrtErrorCode(pError) == XHTTP_BODY_FILE_ERROR_OPEN) &&
		(pCause != NULL) &&
		(strcmp(xrtErrorDomain(pCause), "xrt.file") == 0) &&
		(xrtErrorKind(pCause) == XERR_NOT_FOUND);
	if ( !bChainValid ) {
		fprintf(
			stderr,
			"[INFO] HTTP file error domain=%s kind=%d code=%d system=%d "
			"cause_domain=%s cause_kind=%d cause_code=%d system=%d\n",
			pError != NULL ? xrtErrorDomain(pError) : "(null)",
			pError != NULL ? (int)xrtErrorKind(pError) : -1,
			pError != NULL ? (int)xrtErrorCode(pError) : -1,
			pError != NULL ? (int)xrtErrorSystemCode(pError) : -1,
			pCause != NULL ? xrtErrorDomain(pCause) : "(null)",
			pCause != NULL ? (int)xrtErrorKind(pCause) : -1,
			pCause != NULL ? (int)xrtErrorCode(pCause) : -1,
			pCause != NULL ? (int)xrtErrorSystemCode(pCause) : -1
		);
	}
	testRequire(
		bChainValid,
		"missing HTTP file body error chain mismatch"
	);
	xrtFutureDestroy(pFuture);

	pFuture = xrtHttpBodyFileRangeFuture(
		pPool,
		sPath,
		8,
		3,
		NULL
	);
	testRequire(pFuture != NULL, "invalid HTTP file range was not submitted");
	testRequire(
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(2000000)
		) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_FAILED),
		"invalid HTTP file range did not fail"
	);
	pError = xrtFutureError(pFuture);
	testRequire(
		(pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "http.body.file") == 0) &&
		(xrtErrorCode(pError) == XHTTP_BODY_FILE_ERROR_RANGE) &&
		(xrtErrorKind(pError) == XERR_RANGE),
		"invalid HTTP file range error mismatch"
	);
	xrtFutureDestroy(pFuture);

	File = xrtOpen(sPath, XFILE_WRITE);
	testRequire(File != NULL, "write-only HTTP file body fixture open failed");
	pAsync = xrtAsyncFileAdopt(pPool, File);
	testRequire(pAsync != NULL, "write-only async file adoption failed");
	testRequire(
		xrtHttpBodyFileAdopt(pAsync, 0, 1, NULL) == NULL,
		"write-only HTTP file body adoption succeeded"
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtGetError()),
			"http.body.file"
		) == 0) &&
		(xrtErrorCode(
			xrtGetError()
		) == XHTTP_BODY_FILE_ERROR_ADOPT),
		"write-only HTTP file body adoption error mismatch"
	);
	xrtClearError();
	pClose = xrtAsyncFileClose(pAsync);
	testRequire(
		(pClose != NULL) &&
		(xrtFutureWaitFor(
			pClose,
			UINT64_C(2000000)
		) == XWAIT_OK),
		"failed HTTP file body adoption consumed its source"
	);
	xrtFutureDestroy(pClose);
	xrtFree(sMissing);
}



/* 占住唯一工作线程，直到主线程允许任务退出。 */
static xtaskoutcome testHttpBodyFileBlockTask(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	test_http_body_file_block* pBlock =
		(test_http_body_file_block*)pData;
	bool bResult;

	(void)pCancel;
	(void)pResult;
	bResult = xrtMutexLock(&pBlock->Lock);
	if ( !bResult ) {
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



/* 等待阻塞任务真正占用工作线程。 */
static bool testHttpBodyFileBlockStarted(
	test_http_body_file_block* pBlock
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



/* 释放阻塞任务。 */
static bool testHttpBodyFileBlockRelease(
	test_http_body_file_block* pBlock
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



/* 准备队列达到硬上限时必须显式返回 AGAIN cause，不能无界增长。 */
static void testHttpBodyFileBackpressure(cstr sPath)
{
	xtaskpoolconfig Config = { 1, 1, 0 };
	test_http_body_file_block Block;
	xtaskpool* pPool;
	xfuture* pBlock;
	xfuture* pQueued;
	const xerror* pError;

	testRequire(
		xrtMutexInit(&Block.Lock) &&
		xrtCondInit(&Block.Cond),
		"HTTP file body backpressure synchronization init failed"
	);
	Block.Started = false;
	Block.Release = false;
	pPool = xrtTaskPoolCreate(&Config);
	testRequire(pPool != NULL, "HTTP file body backpressure pool failed");
	pBlock = xrtTaskSubmit(
		pPool,
		testHttpBodyFileBlockTask,
		&Block,
		NULL
	);
	testRequire(pBlock != NULL, "HTTP file body blocker submit failed");
	testRequire(
		testHttpBodyFileBlockStarted(&Block),
		"HTTP file body blocker did not start"
	);
	pQueued = xrtHttpBodyFileFuture(pPool, sPath, NULL);
	testRequire(pQueued != NULL, "HTTP file body queued prepare failed");
	testRequire(
		xrtHttpBodyFileFuture(pPool, sPath, NULL) == NULL,
		"full HTTP file body queue accepted excess work"
	);
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "http.body.file") == 0) &&
		(xrtErrorCode(pError) == XHTTP_BODY_FILE_ERROR_SUBMIT) &&
		(xrtErrorCause(pError) != NULL) &&
		(xrtErrorKind(xrtErrorCause(pError)) == XERR_AGAIN),
		"HTTP file body backpressure error mismatch"
	);
	xrtClearError();
	testRequire(
		xrtFutureCancel(pQueued),
		"queued HTTP file body cancellation failed"
	);
	testRequire(
		testHttpBodyFileBlockRelease(&Block),
		"HTTP file body blocker release failed"
	);
	testRequire(
		(xrtFutureWaitFor(
			pBlock,
			UINT64_C(2000000)
		) == XWAIT_OK) &&
		(xrtFutureWaitFor(
			pQueued,
			UINT64_C(2000000)
		) == XWAIT_OK) &&
		(xrtFutureState(pQueued) == XFUTURE_CANCELLED),
		"queued HTTP file body cancellation did not complete"
	);
	xrtFutureDestroy(pBlock);
	xrtFutureDestroy(pQueued);
	testRequire(
		xrtTaskPoolDestroy(pPool),
		"HTTP file body backpressure pool destroy failed"
	);
	testRequire(
		xrtCondUnit(&Block.Cond) &&
		xrtMutexUnit(&Block.Lock),
		"HTTP file body backpressure synchronization cleanup failed"
	);
}



/* HTTP 文件正文回归入口。 */
int main(void)
{
	xtaskpoolconfig Config = { 2, 32, 0 };
	xtaskpool* pPool = xrtTaskPoolCreate(&Config);
	str sPath = testHttpBodyFilePath(
		"xrt-http-body-file-\xE8\xBE\xB9\xE7\x95\x8C.tmp"
	);

	testRequire(pPool != NULL, "HTTP file body task pool create failed");
	testHttpBodyFileWrite(sPath, "0123456789");
	testHttpBodyFileFull(pPool, sPath);
	testHttpBodyFileRange(pPool, sPath);
	testHttpBodyFileReadSize(pPool, sPath);
	testHttpBodyFileShrink(pPool, sPath);
	testHttpBodyFileErrors(pPool, sPath);
	testRequire(
		xrtTaskPoolDestroy(pPool),
		"HTTP file body task pool destroy failed"
	);
	testHttpBodyFileBackpressure(sPath);
	testRequire(
		xrtFileDelete(sPath),
		"HTTP file body fixture cleanup failed"
	);
	xrtFree(sPath);
	printf("[PASS] HTTP file body\n");
	return 0;
}
