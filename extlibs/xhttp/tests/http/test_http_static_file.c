#include "../test.h"



/* 阻塞任务让静态文件准备稳定停留在有界队列中。 */
typedef struct test_http_static_file_block {
	xatomic32 Started;
	xatomic32 Release;
} test_http_static_file_block;



/* 等待测试线程放行，期间不执行任何文件系统操作。 */
static xtaskoutcome testHttpStaticFileBlockTask(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	test_http_static_file_block* pBlock =
		(test_http_static_file_block*)pData;

	(void)pCancel;
	(void)pResult;
	xrtAtomic32Store(
		&pBlock->Started,
		1,
		XMEMORY_RELEASE
	);
	while ( xrtAtomic32Load(
		&pBlock->Release,
		XMEMORY_ACQUIRE
	) == 0u ) {
		xrtThreadYield();
	}
	return XTASK_SUCCESS;
}



/* 创建低冲突的本地静态文件测试目录名。 */
static void testHttpStaticFileName(
	char* sOutput,
	size_t iCapacity
)
{
	int iSize = snprintf(
		sOutput,
		iCapacity,
		".xrt-http-static-file-%lld",
		(long long)xrtNow()
	);

	testRequire(
		(iSize > 0) &&
		((size_t)iSize < iCapacity),
		"HTTP static file fixture name failed"
	);
}



/* 在文件根内创建或替换一个短文件。 */
static void testHttpStaticFileWrite(
	xroot Root,
	cstr sPath,
	cstr sText
)
{
	xfileoptions Options;
	xfile File;
	size_t iSize = strlen(sText);

	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_WRITE |
		XFILE_CREATE |
		XFILE_TRUNCATE;
	File = xrtRootFileOpen(
		Root,
		sPath,
		&Options
	);
	testRequire(
		(File != NULL) &&
		xrtWriteFull(
			File,
			sText,
			iSize,
			NULL
		) &&
		xrtClose(File),
		"HTTP static file fixture write failed"
	);
}



/* 读取异步文件正文的全部已选字节。 */
static size_t testHttpStaticFileBodyRead(
	xhttpbody* pBody,
	void* pOutput,
	size_t iCapacity
)
{
	xhttpbodyreader* pReader =
		xrtHttpBodyOpen(pBody);
	size_t iUsed = 0;

	testRequire(
		pReader != NULL,
		"HTTP static file body reader open failed"
	);
	for ( ;; ) {
		xhttpbodychunk Chunk;
		xhttpbodystatus Status = xrtHttpBodyNext(
			pReader,
			3,
			&Chunk
		);

		if ( Status == XHTTP_BODY_AGAIN ) {
			xfuture* pWait =
				xrtHttpBodyReaderWait(pReader);

			testRequire(
				(pWait != NULL) &&
				(xrtFutureWaitFor(
					pWait,
					UINT64_C(2000000)
				) == XWAIT_OK),
				"HTTP static file body wait failed"
			);
			xrtFutureDestroy(pWait);
			continue;
		}
		if ( Status == XHTTP_BODY_EOF ) {
			break;
		}
		testRequire(
			(Status == XHTTP_BODY_DATA) &&
			(Chunk.Size <=
			 (iCapacity - iUsed)),
			"HTTP static file body read failed"
		);
		memcpy(
			(bytes)pOutput + iUsed,
			Chunk.Data,
			Chunk.Size
		);
		iUsed += Chunk.Size;
		xrtHttpBodyChunkRelease(&Chunk);
	}
	xrtHttpBodyReaderDestroy(pReader);
	return iUsed;
}



/* 验证同步资源、同句柄元数据、区间正文和一次消费契约。 */
static void testHttpStaticFileSync(
	xtaskpool* pPool,
	xroot Root,
	cstr sDirectory
)
{
	xhttpstaticfile* pFile;
	xhttpstaticfile* pRetained;
	const xhttprepresentation* pCurrent;
	const xfileinfo* pInfo;
	xhttpbody* pBody;
	str sOriginal;
	str sMoved;
	char Output[32];
	size_t iSize;

	pFile = xrtHttpStaticFileOpen(
		pPool,
		Root,
		"asset.txt"
	);
	testRequire(
		pFile != NULL,
		"HTTP static file synchronous open failed"
	);
	pInfo = xrtHttpStaticFileInfo(pFile);
	pCurrent = xrtHttpStaticFileRepresentation(
		pFile
	);
	testRequire(
		(pInfo != NULL) &&
		(pInfo->Type == XFILE_TYPE_FILE) &&
		(pInfo->Size == 10u) &&
		(xrtHttpStaticFileSize(pFile) == 10u) &&
		(pCurrent != NULL) &&
		pCurrent->Exists &&
		pCurrent->HasLastModified &&
		pCurrent->HasETag &&
		pCurrent->ETag.Weak &&
		(pCurrent->ETag.Opaque.Size != 0u),
		"HTTP static file metadata mismatch"
	);
	pRetained = xrtHttpStaticFileRef(pFile);
	testRequire(
		pRetained == pFile,
		"HTTP static file retain failed"
	);
	xrtHttpStaticFileDestroy(pFile);
	pFile = pRetained;

	testRequire(
		xrtHttpStaticFileTakeBody(
			pFile,
			9,
			2
		) == NULL,
		"HTTP static file accepted an invalid range"
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) ==
		 XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP_STATIC_FILE_ERROR_RANGE),
		"HTTP static file range error mismatch"
	);
	xrtClearError();

	sOriginal = xrtPathJoin(
		sDirectory,
		"asset.txt"
	);
	sMoved = xrtPathJoin(
		sDirectory,
		"opened.txt"
	);
	testRequire(
		(sOriginal != NULL) &&
		(sMoved != NULL) &&
		xrtPathRename(
			sOriginal,
			sMoved,
			false
		),
		"HTTP static file rename fixture failed"
	);
	testHttpStaticFileWrite(
		Root,
		"asset.txt",
		"replacement"
	);

	pBody = xrtHttpStaticFileTakeBody(
		pFile,
		2,
		4
	);
	testRequire(
		pBody != NULL,
		"HTTP static file range body failed"
	);
	iSize = testHttpStaticFileBodyRead(
		pBody,
		Output,
		sizeof(Output)
	);
	testRequire(
		(iSize == 4u) &&
		(memcmp(Output, "2345", 4u) == 0),
		"HTTP static file did not retain the opened file identity"
	);
	xrtHttpBodyDestroy(pBody);
	testRequire(
		xrtHttpStaticFileTakeBodyAll(
			pFile
		) == NULL,
		"HTTP static file allowed repeated body adoption"
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) ==
		 XERR_STATE) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP_STATIC_FILE_ERROR_CONSUMED),
		"HTTP static file consumed error mismatch"
	);
	xrtClearError();
	xrtHttpStaticFileDestroy(pFile);
	xrtFree(sMoved);
	xrtFree(sOriginal);
}



/* 验证异步准备值可以显式保留到 Future 生命周期之外。 */
static void testHttpStaticFileFuture(
	xtaskpool* pPool,
	xroot Root
)
{
	xfuture* pFuture = xrtHttpStaticFileFuture(
		pPool,
		Root,
		"asset.txt"
	);
	xhttpstaticfile* pFile;
	xhttpbody* pBody;
	char Output[32];
	size_t iSize;

	testRequire(
		(pFuture != NULL) &&
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(2000000)
		) == XWAIT_OK) &&
		(xrtFutureState(pFuture) ==
		 XFUTURE_RESOLVED),
		"HTTP static file Future failed"
	);
	pFile = xrtHttpStaticFileRef(
		(xhttpstaticfile*)xrtFutureValue(
			pFuture
		)
	);
	testRequire(
		pFile != NULL,
		"HTTP static file Future value retain failed"
	);
	xrtFutureDestroy(pFuture);
	pBody = xrtHttpStaticFileTakeBodyAll(pFile);
	testRequire(
		pBody != NULL,
		"HTTP static file full body failed"
	);
	iSize = testHttpStaticFileBodyRead(
		pBody,
		Output,
		sizeof(Output)
	);
	testRequire(
		(iSize == 11u) &&
		(memcmp(
			Output,
			"replacement",
			11u
		) == 0),
		"HTTP static file Future body mismatch"
	);
	xrtHttpBodyDestroy(pBody);
	xrtHttpStaticFileDestroy(pFile);
}



/* 验证异步准备在提交前复制路径，不借用调用方的文本存储。 */
static void testHttpStaticFileFuturePath(xroot Root)
{
	xtaskpoolconfig Config = { 1, 4, 0 };
	test_http_static_file_block Block;
	char sPath[32] = "asset.txt";
	xtaskpool* pPool;
	xfuture* pBlock;
	xfuture* pFuture;
	xhttpstaticfile* pFile;
	xdeadline Deadline;

	xrtAtomic32Init(&Block.Started, 0);
	xrtAtomic32Init(&Block.Release, 0);
	pPool = xrtTaskPoolCreate(&Config);
	testRequire(
		pPool != NULL,
		"HTTP static file path copy pool failed"
	);
	pBlock = xrtTaskSubmit(
		pPool,
		testHttpStaticFileBlockTask,
		&Block,
		NULL
	);
	testRequire(
		pBlock != NULL,
		"HTTP static file path copy blocker failed"
	);
	Deadline = xrtDeadlineAfter(UINT64_C(2000000));
	while ( xrtAtomic32Load(
		&Block.Started,
		XMEMORY_ACQUIRE
	) == 0u ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP static file path copy blocker did not start"
		);
		xrtThreadYield();
	}
	pFuture = xrtHttpStaticFileFuture(
		pPool,
		Root,
		sPath
	);
	testRequire(
		pFuture != NULL,
		"HTTP static file path copy submit failed"
	);
	memcpy(sPath, "missing.txt", sizeof("missing.txt"));
	xrtAtomic32Store(
		&Block.Release,
		1,
		XMEMORY_RELEASE
	);
	testRequire(
		(xrtFutureWaitFor(
			pBlock,
			UINT64_C(2000000)
		) == XWAIT_OK) &&
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(2000000)
		) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED),
		"HTTP static file Future borrowed path storage"
	);
	pFile = (xhttpstaticfile*)xrtFutureValue(pFuture);
	testRequire(
		(pFile != NULL) &&
		(xrtHttpStaticFileSize(pFile) == 11u),
		"HTTP static file copied path result mismatch"
	);
	xrtFutureDestroy(pFuture);
	xrtFutureDestroy(pBlock);
	testRequire(
		xrtTaskPoolDestroy(pPool),
		"HTTP static file path copy pool cleanup failed"
	);
}



/* 验证底层异步文件可以一次性转交给自定义正文实现。 */
static void testHttpStaticFileTakeFile(
	xtaskpool* pPool,
	xroot Root
)
{
	xhttpstaticfile* pFile =
		xrtHttpStaticFileOpen(
			pPool,
			Root,
			"asset.txt"
		);
	xasyncfile* pAsync;
	xfuture* pFuture;
	xfilesize* pSize;

	testRequire(
		pFile != NULL,
		"HTTP static file extension open failed"
	);
	pAsync = xrtHttpStaticFileTakeFile(pFile);
	testRequire(
		pAsync != NULL,
		"HTTP static file extension take failed"
	);
	xrtHttpStaticFileDestroy(pFile);
	pFuture = xrtAsyncFileSize(pAsync);
	testRequire(
		(pFuture != NULL) &&
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(2000000)
		) == XWAIT_OK) &&
		(xrtFutureState(pFuture) ==
		 XFUTURE_RESOLVED),
		"HTTP static file extension size failed"
	);
	pSize = (xfilesize*)xrtFutureValue(pFuture);
	testRequire(
		(pSize != NULL) &&
		(pSize->Size == 11u),
		"HTTP static file extension metadata mismatch"
	);
	xrtFutureDestroy(pFuture);
	pFuture = xrtAsyncFileClose(pAsync);
	testRequire(
		(pFuture != NULL) &&
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(2000000)
		) == XWAIT_OK),
		"HTTP static file extension close failed"
	);
	xrtFutureDestroy(pFuture);
}



/* 验证排队准备取消后不会运行文件打开任务。 */
static void testHttpStaticFileCancel(
	xroot Root
)
{
	xtaskpoolconfig Config = { 1, 1, 0 };
	test_http_static_file_block Block;
	xtaskpool* pPool;
	xfuture* pBlock;
	xfuture* pFile;
	xfuture* pReplacement;
	xdeadline Deadline;

	xrtAtomic32Init(&Block.Started, 0);
	xrtAtomic32Init(&Block.Release, 0);
	pPool = xrtTaskPoolCreate(&Config);
	testRequire(
		pPool != NULL,
		"HTTP static file cancellation pool failed"
	);
	pBlock = xrtTaskSubmit(
		pPool,
		testHttpStaticFileBlockTask,
		&Block,
		NULL
	);
	testRequire(
		pBlock != NULL,
		"HTTP static file blocker submit failed"
	);
	Deadline = xrtDeadlineAfter(
		UINT64_C(2000000)
	);
	while ( xrtAtomic32Load(
		&Block.Started,
		XMEMORY_ACQUIRE
	) == 0u ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP static file blocker did not start"
		);
		xrtThreadYield();
	}
	pFile = xrtHttpStaticFileFuture(
		pPool,
		Root,
		"asset.txt"
	);
	testRequire(
		(pFile != NULL) &&
		xrtFutureCancel(pFile),
		"HTTP static file cancellation request failed"
	);
	pReplacement = xrtTaskSubmit(
		pPool,
		testHttpStaticFileBlockTask,
		&Block,
		NULL
	);
	testRequire(
		(pReplacement != NULL) &&
		(xrtFutureWaitFor(
			pFile,
			UINT64_C(2000000)
		) == XWAIT_OK) &&
		(xrtFutureState(pFile) ==
		 XFUTURE_CANCELLED),
		"HTTP static file queued cancellation failed"
	);
	xrtAtomic32Store(
		&Block.Release,
		1,
		XMEMORY_RELEASE
	);
	testRequire(
		(xrtFutureWaitFor(
			pBlock,
			UINT64_C(2000000)
		) == XWAIT_OK) &&
		(xrtFutureWaitFor(
			pReplacement,
			UINT64_C(2000000)
		) == XWAIT_OK),
		"HTTP static file blocker release failed"
	);
	xrtFutureDestroy(pFile);
	xrtFutureDestroy(pReplacement);
	xrtFutureDestroy(pBlock);
	testRequire(
		xrtTaskPoolDestroy(pPool),
		"HTTP static file cancellation pool cleanup failed"
	);
}



/* 验证文件根拒绝越界，并保留缺失与类型错误的原因链。 */
static void testHttpStaticFileErrors(
	xtaskpool* pPool,
	xroot Root
)
{
	const xerror* pError;
	xfuture* pFuture;

	testRequire(
		xrtHttpStaticFileOpen(
			pPool,
			Root,
			"../outside.txt"
		) == NULL,
		"HTTP static file escaped above its root"
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) ==
		 XERR_PERMISSION) &&
		(strcmp(
			xrtErrorDomain(xrtGetError()),
			"http.static.file"
		) == 0) &&
		(xrtErrorCause(xrtGetError()) != NULL),
		"HTTP static file escape error mismatch"
	);
	xrtClearError();

	testRequire(
		xrtHttpStaticFileOpen(
			pPool,
			Root,
			"."
		) == NULL,
		"HTTP static file accepted a directory"
	);
	pError = xrtGetError();
	if ( (pError == NULL) ||
		(xrtErrorKind(pError) != XERR_TYPE) ) {
		fprintf(
			stderr,
			"[INFO] directory error kind=%d code=%d domain=%s operation=%s message=%s\n",
			(int)(pError != NULL ? xrtErrorKind(pError) : XERR_NONE),
			(int)(pError != NULL ? xrtErrorCode(pError) : 0),
			(pError != NULL) && (xrtErrorDomain(pError) != NULL) ?
				xrtErrorDomain(pError) : "",
			(pError != NULL) && (xrtErrorOperation(pError) != NULL) ?
				xrtErrorOperation(pError) : "",
			(pError != NULL) && (xrtErrorMessage(pError) != NULL) ?
				xrtErrorMessage(pError) : ""
		);
	}
	testRequire(
		(pError != NULL) &&
		(xrtErrorKind(pError) ==
		 XERR_TYPE),
		"HTTP static file directory error mismatch"
	);
	xrtClearError();

	pFuture = xrtHttpStaticFileFuture(
		pPool,
		Root,
		"missing.txt"
	);
	testRequire(
		(pFuture != NULL) &&
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(2000000)
		) == XWAIT_OK) &&
		(xrtFutureState(pFuture) ==
		 XFUTURE_FAILED) &&
		(xrtErrorKind(
			xrtFutureError(pFuture)
		 ) == XERR_NOT_FOUND) &&
		(strcmp(
			xrtErrorDomain(
				xrtFutureError(pFuture)
			),
			"http.static.file"
		) == 0),
		"HTTP static file missing error mismatch"
	);
	xrtFutureDestroy(pFuture);
}



/* 验证根内相对链接可用，而根能力仍负责阻止越界目标。 */
static void testHttpStaticFileLink(
	xtaskpool* pPool,
	xroot Root,
	cstr sDirectory
)
{
	str sLink = xrtPathJoin(
		sDirectory,
		"inside-link"
	);

	testRequire(
		sLink != NULL,
		"HTTP static file link path failed"
	);
	if ( xrtLinkCreate(
		"asset.txt",
		sLink,
		false
	) ) {
		xhttpstaticfile* pFile =
			xrtHttpStaticFileOpen(
				pPool,
				Root,
				"inside-link"
			);

		testRequire(
			(pFile != NULL) &&
			(xrtHttpStaticFileSize(pFile) ==
			 11u),
			"HTTP static file safe link open failed"
		);
		xrtHttpStaticFileDestroy(pFile);
		testRequire(
			xrtRootRemove(
				Root,
				"inside-link"
			),
			"HTTP static file link cleanup failed"
		);
	} else {
		xrtClearError();
	}
	xrtFree(sLink);
}



/* 执行受根约束的静态文件资源回归。 */
int main(void)
{
	xtaskpoolconfig PoolConfig = { 2, 32, 0 };
	char sDirectory[96];
	xroot Parent;
	xroot Root;
	xtaskpool* pPool;

	testHttpStaticFileName(
		sDirectory,
		sizeof(sDirectory)
	);
	Parent = xrtRootOpen(".");
	testRequire(
		Parent != NULL,
		"HTTP static file parent root failed"
	);
	if ( !xrtRootRemove(Parent, sDirectory) ) {
		xrtClearError();
	}
	testRequire(
		xrtRootDirCreate(
			Parent,
			sDirectory,
			0700u
		),
		"HTTP static file directory create failed"
	);
	Root = xrtRootOpenIn(
		Parent,
		sDirectory
	);
	testRequire(
		Root != NULL,
		"HTTP static file root open failed"
	);
	testHttpStaticFileWrite(
		Root,
		"asset.txt",
		"0123456789"
	);
	pPool = xrtTaskPoolCreate(&PoolConfig);
	testRequire(
		pPool != NULL,
		"HTTP static file task pool failed"
	);

	testHttpStaticFileSync(
		pPool,
		Root,
		sDirectory
	);
	testHttpStaticFileFuture(pPool, Root);
	testHttpStaticFileFuturePath(Root);
	testHttpStaticFileTakeFile(pPool, Root);
	testHttpStaticFileCancel(Root);
	testHttpStaticFileErrors(pPool, Root);
	testHttpStaticFileLink(
		pPool,
		Root,
		sDirectory
	);

	testRequire(
		xrtTaskPoolDestroy(pPool),
		"HTTP static file task pool cleanup failed"
	);
	testRequire(
		xrtRootRemove(Root, "asset.txt") &&
		xrtRootRemove(Root, "opened.txt"),
		"HTTP static file fixture cleanup failed"
	);
	testRequire(
		xrtRootClose(Root) &&
		xrtRootRemove(Parent, sDirectory) &&
		xrtRootClose(Parent),
		"HTTP static file root cleanup failed"
	);
	printf("[PASS] http_static_file\n");
	return 0;
}
