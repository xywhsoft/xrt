#include "../test.h"



/* 创建本测试独占的受限文件目录名。 */
static void testHttpStaticMultipartBodyName(
	char* sOutput,
	size_t iCapacity
)
{
	int iSize = snprintf(
		sOutput,
		iCapacity,
		".xrt-http-static-multipart-%lld",
		(long long)xrtNow()
	);

	testRequire(
		(iSize > 0) &&
		((size_t)iSize < iCapacity),
		"HTTP static multipart fixture name failed"
	);
}



/* 在文件根中写入固定的十字节表示。 */
static void testHttpStaticMultipartBodyWrite(xroot Root)
{
	xfileoptions Options;
	xfile File;

	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_WRITE |
		XFILE_CREATE |
		XFILE_TRUNCATE;
	File = xrtRootFileOpen(
		Root,
		"asset.txt",
		&Options
	);
	testRequire(
		(File != NULL) &&
		xrtWriteFull(
			File,
			"0123456789",
			10,
			NULL
		) &&
		xrtClose(File),
		"HTTP static multipart fixture write failed"
	);
}



/* 等待异步 Body 的当前文件读取。 */
static void testHttpStaticMultipartBodyWait(
	xhttpbodyreader* pReader
)
{
	xfuture* pFuture = xrtHttpBodyReaderWait(pReader);

	testRequire(
		(pFuture != NULL) &&
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(2000000)
		) == XWAIT_OK),
		"HTTP static multipart body wait failed"
	);
	xrtFutureDestroy(pFuture);
}



/* 以很小的动态上限读取整个多范围正文。 */
static size_t testHttpStaticMultipartBodyRead(
	xhttpbody* pBody,
	void* pOutput,
	size_t iCapacity
)
{
	xhttpbodyreader* pReader = xrtHttpBodyOpen(pBody);
	size_t iUsed = 0;
	size_t iLimit = 1;

	testRequire(
		pReader != NULL,
		"HTTP static multipart body open failed"
	);
	for ( ;; ) {
		xhttpbodychunk Chunk;
		xhttpbodystatus Status = xrtHttpBodyNext(
			pReader,
			iLimit,
			&Chunk
		);

		if ( Status == XHTTP_BODY_AGAIN ) {
			testHttpStaticMultipartBodyWait(pReader);
			continue;
		}
		if ( Status == XHTTP_BODY_EOF ) {
			break;
		}
		testRequire(
			(Status == XHTTP_BODY_DATA) &&
			(Chunk.Size <= iLimit) &&
			(Chunk.Size <= (iCapacity - iUsed)),
			"HTTP static multipart body read failed"
		);
		memcpy(
			(bytes)pOutput + iUsed,
			Chunk.Data,
			Chunk.Size
		);
		iUsed += Chunk.Size;
		iLimit = (iLimit % 5u) + 1u;
		xrtHttpBodyChunkRelease(&Chunk);
	}
	xrtHttpBodyReaderDestroy(pReader);
	return iUsed;
}



/* 验证完整线缆、精确长度、异步分块和资源一次消费。 */
static void testHttpStaticMultipartBodyWire(
	xtaskpool* pPool,
	xroot Root
)
{
	static const char sExpected[] =
		"--parts\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Range: bytes 0-2/10\r\n"
		"\r\n"
		"012\r\n"
		"--parts\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Range: bytes 7-9/10\r\n"
		"\r\n"
		"789\r\n"
		"--parts--\r\n";
	xhttpbyterange Ranges[2] = {
		{ 0, 2 },
		{ 7, 9 }
	};
	xhttpstaticfile* pFile = xrtHttpStaticFileOpen(
		pPool,
		Root,
		"asset.txt"
	);
	xhttpbody* pBody;
	unsigned char Output[256];
	uint64 iLength;
	size_t iSize;

	testRequire(
		pFile != NULL,
		"HTTP static multipart file open failed"
	);
	testRequire(xrtHttpRangeMultipartLength(
		Ranges,
		2,
		10,
		XRT_STR_LITERAL("text/plain"),
		XRT_STR_LITERAL("parts"),
		&iLength
	), "HTTP static multipart expected length failed");
	pBody = xrtHttpStaticFileTakeMultipartBody(
		pFile,
		Ranges,
		2,
		XRT_STR_LITERAL("text/plain"),
		XRT_STR_LITERAL("parts")
	);
	testRequire(
		(pBody != NULL) &&
		!xrtHttpBodyReplayable(pBody) &&
		(xrtHttpBodyLength(pBody) == iLength) &&
		(iLength == (sizeof(sExpected) - 1u)),
		"HTTP static multipart body metadata mismatch"
	);
	iSize = testHttpStaticMultipartBodyRead(
		pBody,
		Output,
		sizeof(Output)
	);
	testRequire(
		(iSize == (sizeof(sExpected) - 1u)) &&
		(memcmp(
			Output,
			sExpected,
			sizeof(sExpected) - 1u
		) == 0),
		"HTTP static multipart body wire mismatch"
	);
	xrtHttpBodyDestroy(pBody);
	testRequire(xrtHttpStaticFileTakeBodyAll(
		pFile
	) == NULL, "HTTP static multipart file was consumed twice");
	xrtClearError();
	xrtHttpStaticFileDestroy(pFile);
}



/* 验证未对齐范围被立即复制，非法范围也不会提前消费静态文件。 */
static void testHttpStaticMultipartBodyMemory(
	xtaskpool* pPool,
	xroot Root
)
{
	static const char sExpected[] =
		"--memory\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Range: bytes 0-2/10\r\n"
		"\r\n"
		"012\r\n"
		"--memory\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Range: bytes 7-9/10\r\n"
		"\r\n"
		"789\r\n"
		"--memory--\r\n";
	xhttpbyterange Ranges[2] = {
		{ 0, 2 },
		{ 7, 9 }
	};
	uint8 RangeStorage[
		(sizeof(xhttpbyterange) * 2u) + 2u
	];
	xhttpstaticfile* pFile;
	xhttpbody* pBody;
	unsigned char Output[256];
	size_t iSize;

	memset(RangeStorage, 0xA5, sizeof(RangeStorage));
	memcpy(
		RangeStorage + 1u,
		Ranges,
		sizeof(Ranges)
	);
	pFile = xrtHttpStaticFileOpen(
		pPool,
		Root,
		"asset.txt"
	);
	testRequire(
		pFile != NULL,
		"HTTP static multipart unaligned file open failed"
	);
	pBody = xrtHttpStaticFileTakeMultipartBody(
		pFile,
		(const xhttpbyterange*)(RangeStorage + 1u),
		2,
		XRT_STR_LITERAL("text/plain"),
		XRT_STR_LITERAL("memory")
	);
	testRequire(
		(pBody != NULL) &&
		(RangeStorage[0] == 0xA5) &&
		(RangeStorage[sizeof(RangeStorage) - 1u] == 0xA5),
		"HTTP static multipart unaligned range failed"
	);
	memset(
		RangeStorage + 1u,
		0,
		sizeof(Ranges)
	);
	iSize = testHttpStaticMultipartBodyRead(
		pBody,
		Output,
		sizeof(Output)
	);
	testRequire(
		(iSize == (sizeof(sExpected) - 1u)) &&
		(memcmp(
			Output,
			sExpected,
			sizeof(sExpected) - 1u
		) == 0),
		"HTTP static multipart did not copy ranges"
	);
	xrtHttpBodyDestroy(pBody);
	xrtHttpStaticFileDestroy(pFile);

	pFile = xrtHttpStaticFileOpen(
		pPool,
		Root,
		"asset.txt"
	);
	testRequire(
		pFile != NULL,
		"HTTP static multipart retry file open failed"
	);
	testRequire(xrtHttpStaticFileTakeMultipartBody(
		pFile,
		(const xhttpbyterange*)(uintptr_t)(
			UINTPTR_MAX - 1u
		),
		1,
		XRT_STR_LITERAL("text/plain"),
		XRT_STR_LITERAL("memory")
	) == NULL, "HTTP static multipart accepted wrapping ranges");
	xrtClearError();
	pBody = xrtHttpStaticFileTakeBodyAll(pFile);
	testRequire(
		pBody != NULL,
		"HTTP static multipart failure consumed static file"
	);
	xrtHttpBodyDestroy(pBody);
	xrtHttpStaticFileDestroy(pFile);
}



/* 验证固定头部 Chunk 可以晚于 Reader、Body 和静态资源释放。 */
static void testHttpStaticMultipartBodyLease(
	xtaskpool* pPool,
	xroot Root
)
{
	xhttpbyterange Range = { 1, 1 };
	xhttpstaticfile* pFile = xrtHttpStaticFileOpen(
		pPool,
		Root,
		"asset.txt"
	);
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;

	testRequire(
		pFile != NULL,
		"HTTP static multipart lease file open failed"
	);
	pBody = xrtHttpStaticFileTakeMultipartBody(
		pFile,
		&Range,
		1,
		XRT_STR_LITERAL("text/plain"),
		XRT_STR_LITERAL("lease")
	);
	testRequire(
		pBody != NULL,
		"HTTP static multipart lease body failed"
	);
	pReader = xrtHttpBodyOpen(pBody);
	testRequire(
		(pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader,
			8,
			&Chunk
		) == XHTTP_BODY_DATA) &&
		(Chunk.Size == 8),
		"HTTP static multipart metadata lease failed"
	);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtHttpStaticFileDestroy(pFile);
	testRequire(
		memcmp(Chunk.Data, "--lease\r", 8u) == 0,
		"HTTP static multipart metadata lease expired early"
	);
	xrtHttpBodyChunkRelease(&Chunk);
}



/* 验证低层采用失败后异步文件仍由调用方完整拥有。 */
static void testHttpStaticMultipartBodyAdoptFailure(
	xtaskpool* pPool,
	xroot Root
)
{
	xhttpbyterange Ranges[2] = {
		{ 0, 5 },
		{ 4, 9 }
	};
	xhttpstaticfile* pFile = xrtHttpStaticFileOpen(
		pPool,
		Root,
		"asset.txt"
	);
	xasyncfile* pAsync;
	xfuture* pFuture;
	xfilesize* pSize;

	testRequire(
		pFile != NULL,
		"HTTP static multipart adopt file open failed"
	);
	pAsync = xrtHttpStaticFileTakeFile(pFile);
	testRequire(
		pAsync != NULL,
		"HTTP static multipart async file take failed"
	);
	testRequire(xrtHttpStaticMultipartBodyAdopt(
		pAsync,
		Ranges,
		2,
		10,
		XRT_STR_LITERAL("text/plain"),
		XRT_STR_LITERAL("adopt")
	) == NULL, "HTTP static multipart accepted overlapping ranges");
	xrtClearError();
	pFuture = xrtAsyncFileSize(pAsync);
	testRequire(
		(pFuture != NULL) &&
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(2000000)
		) == XWAIT_OK),
		"HTTP static multipart failed adoption consumed file"
	);
	pSize = (xfilesize*)xrtFutureValue(pFuture);
	testRequire(
		(pSize != NULL) && (pSize->Size == 10),
		"HTTP static multipart retained file size mismatch"
	);
	xrtFutureDestroy(pFuture);
	pFuture = xrtAsyncFileClose(pAsync);
	testRequire(
		(pFuture != NULL) &&
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(2000000)
		) == XWAIT_OK),
		"HTTP static multipart retained file close failed"
	);
	xrtFutureDestroy(pFuture);
	xrtHttpStaticFileDestroy(pFile);
}



/* 文件缩短必须保留文件正文错误，不能提前结束为合法 multipart。 */
static void testHttpStaticMultipartBodyShrink(
	xtaskpool* pPool,
	xroot Root,
	cstr sDirectory
)
{
	xhttpbyterange Ranges[2] = {
		{ 0, 1 },
		{ 8, 9 }
	};
	xfileoptions Options;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;
	xasyncfile* pAsync;
	xhttpbody* pBody;
	const xerror* pError;
	str sPath = xrtPathJoin(
		sDirectory,
		"asset.txt"
	);

	testRequire(
		sPath != NULL,
		"HTTP static multipart shrink path failed"
	);
	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_READ;
	pAsync = xrtAsyncFileOpen(
		pPool,
		sPath,
		&Options
	);
	testRequire(
		pAsync != NULL,
		"HTTP static multipart shrink open failed"
	);
	pBody = xrtHttpStaticMultipartBodyAdopt(
		pAsync,
		Ranges,
		2,
		10,
		XRT_STR_LITERAL("text/plain"),
		XRT_STR_LITERAL("shrink")
	);
	testRequire(
		pBody != NULL,
		"HTTP static multipart shrink body failed"
	);
	testRequire(
		xrtFileSetSize(sPath, 3),
		"HTTP static multipart shrink fixture failed"
	);
	pReader = xrtHttpBodyOpen(pBody);
	testRequire(
		pReader != NULL,
		"HTTP static multipart shrink reader failed"
	);
	for ( ;; ) {
		Status = xrtHttpBodyNext(
			pReader,
			256,
			&Chunk
		);
		if ( Status == XHTTP_BODY_AGAIN ) {
			testHttpStaticMultipartBodyWait(pReader);
			continue;
		}
		if ( Status != XHTTP_BODY_DATA ) {
			break;
		}
		xrtHttpBodyChunkRelease(&Chunk);
	}
	testRequire(
		Status == XHTTP_BODY_ERROR,
		"HTTP static multipart shrink did not fail"
	);
	pError = xrtHttpBodyReaderError(pReader);
	testRequire(
		(pError != NULL) &&
		(strcmp(
			xrtErrorDomain(pError),
			"http.body.file"
		) == 0) &&
		(xrtErrorCode(pError) ==
		 XHTTP_BODY_FILE_ERROR_READ),
		"HTTP static multipart shrink error mismatch"
	);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtFree(sPath);
	xrtClearError();
	testHttpStaticMultipartBodyWrite(Root);
}



/* 执行静态多范围异步正文测试。 */
int main(void)
{
	xtaskpoolconfig PoolConfig = { 2, 32, 0 };
	char sDirectory[96];
	xtaskpool* pPool;
	xroot Parent;
	xroot Root;

	testHttpStaticMultipartBodyName(
		sDirectory,
		sizeof(sDirectory)
	);
	Parent = xrtRootOpen(".");
	testRequire(
		Parent != NULL,
		"HTTP static multipart parent root failed"
	);
	if ( !xrtRootRemove(Parent, sDirectory) ) {
		xrtClearError();
	}
	testRequire(xrtRootDirCreate(
		Parent,
		sDirectory,
		0700u
	), "HTTP static multipart directory create failed");
	Root = xrtRootOpenIn(Parent, sDirectory);
	testRequire(
		Root != NULL,
		"HTTP static multipart root open failed"
	);
	testHttpStaticMultipartBodyWrite(Root);
	pPool = xrtTaskPoolCreate(&PoolConfig);
	testRequire(
		pPool != NULL,
		"HTTP static multipart task pool failed"
	);

	testHttpStaticMultipartBodyWire(pPool, Root);
	testHttpStaticMultipartBodyMemory(pPool, Root);
	testHttpStaticMultipartBodyLease(pPool, Root);
	testHttpStaticMultipartBodyAdoptFailure(pPool, Root);
	testHttpStaticMultipartBodyShrink(
		pPool,
		Root,
		sDirectory
	);

	testRequire(
		xrtTaskPoolDestroy(pPool),
		"HTTP static multipart task pool cleanup failed"
	);
	testRequire(
		xrtRootRemove(Root, "asset.txt") &&
		xrtRootClose(Root) &&
		xrtRootRemove(Parent, sDirectory) &&
		xrtRootClose(Parent),
		"HTTP static multipart fixture cleanup failed"
	);
	printf("[PASS] http_static_multipart_body\n");
	return 0;
}
