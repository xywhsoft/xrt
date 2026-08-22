#include "../test.h"



/* 自定义释放状态用于验证 Chunk 独立延长正文数据生命周期。 */
typedef struct test_http_body_release {
	size_t Calls;
} test_http_body_release;



/* 记录固定正文底层数据的最终释放。 */
static void testHttpBodyRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	test_http_body_release* pState =
		(test_http_body_release*)pContext;

	testRequire((pData != NULL) && (iSize == 6),
		"HTTP body release data mismatch");
	pState->Calls++;
}



/* 静态测试 Chunk 不拥有额外资源。 */
static void testHttpBodyStaticRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
}



/* 清理测试记录三类回调，并尝试发布一个不应逃逸的错误。 */
typedef struct test_http_body_cleanup {
	xerror* Error;
	size_t Releases;
	size_t Closes;
	size_t Destroys;
} test_http_body_cleanup;



/* 清理回调统一尝试覆盖调用线程的当前错误。 */
static void testHttpBodyCleanupPublish(
	test_http_body_cleanup* pCleanup
)
{
	if ( pCleanup->Error != NULL ) {
		xrtSetError(pCleanup->Error);
	}
}



/* 记录 Chunk 数据租约释放。 */
static void testHttpBodyCleanupRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	test_http_body_cleanup* pCleanup =
		(test_http_body_cleanup*)pContext;

	(void)pData;
	(void)iSize;
	pCleanup->Releases++;
	testHttpBodyCleanupPublish(pCleanup);
}



/* 清理来源不发布数据，只用于 Reader 生命周期测试。 */
static xhttpbodystatus testHttpBodyCleanupNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	(void)pContext;
	(void)iMaxBytes;
	(void)pChunk;
	return XHTTP_BODY_EOF;
}



/* 记录 Reader 关闭并尝试覆盖当前错误。 */
static void testHttpBodyCleanupClose(ptr pContext)
{
	test_http_body_cleanup* pCleanup =
		(test_http_body_cleanup*)pContext;

	pCleanup->Closes++;
	testHttpBodyCleanupPublish(pCleanup);
}



/* 打开清理测试 Reader。 */
static bool testHttpBodyCleanupOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testHttpBodyCleanupNext;
	pOps->Close = testHttpBodyCleanupClose;
	*ppReader = pFactory;
	return true;
}



/* 记录工厂销毁并尝试覆盖当前错误。 */
static void testHttpBodyCleanupDestroy(ptr pFactory)
{
	test_http_body_cleanup* pCleanup =
		(test_http_body_cleanup*)pFactory;

	pCleanup->Destroys++;
	testHttpBodyCleanupPublish(pCleanup);
}



/* Open 失败来源可以不设置错误，也可以重新发布已有错误对象。 */
typedef struct test_http_body_open_failure {
	xerror* Error;
	size_t Opens;
} test_http_body_open_failure;



/* 按配置发布来源错误后拒绝打开。 */
static bool testHttpBodyOpenFailure(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	test_http_body_open_failure* pFailure =
		(test_http_body_open_failure*)pFactory;

	(void)pOps;
	(void)ppReader;
	pFailure->Opens++;
	if ( pFailure->Error != NULL ) {
		xrtSetError(pFailure->Error);
	}
	return false;
}



/* 验证固定正文支持分段、复制读取、重放和独立 Chunk 生命周期。 */
static void testHttpBodyBytes(void)
{
	char Source[] = "source";
	char Output[8];
	xhttpbody* pCopy;
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodyreader* pReplay;
	xhttpbodychunk Chunk;
	xbytesview View;
	test_http_body_release Release = { 0 };
	size_t iSize;

	pCopy = xrtHttpBodyCopy(
		(xbytesview){ (cbytes)Source, 6 }
	);
	testRequire(pCopy != NULL, "HTTP body copy create failed");
	Source[0] = 'X';
	pReader = xrtHttpBodyOpen(pCopy);
	testRequire((pReader != NULL) &&
		(xrtHttpBodyLength(pCopy) == 6) &&
		xrtHttpBodyReplayable(pCopy) &&
		xrtHttpBodyView(pCopy, &View) &&
		(View.Size == 6) &&
		(memcmp(View.Data, "source", 6) == 0),
		"HTTP copied body metadata mismatch");
	testRequire((xrtHttpBodyRead(
		pReader, Output, 3, &iSize
	) == XHTTP_BODY_DATA) &&
		(iSize == 3) &&
		(memcmp(Output, "sou", 3) == 0),
		"HTTP copied body first read mismatch");
	testRequire((xrtHttpBodyRead(
		pReader, Output, sizeof(Output), &iSize
	) == XHTTP_BODY_DATA) &&
		(iSize == 3) &&
		(memcmp(Output, "rce", 3) == 0) &&
		(xrtHttpBodyRead(
			pReader, Output, sizeof(Output), &iSize
		) == XHTTP_BODY_EOF) &&
		(iSize == 0) &&
		(xrtHttpBodyReaderBytes(pReader) == 6),
		"HTTP copied body completion mismatch");
	xrtHttpBodyReaderDestroy(pReader);

	pReplay = xrtHttpBodyOpen(pCopy);
	testRequire((pReplay != NULL) &&
		(xrtHttpBodyNext(
			pReplay, 6, &Chunk
		) == XHTTP_BODY_DATA) &&
		(Chunk.Size == 6) &&
		(memcmp(Chunk.Data, "source", 6) == 0),
		"HTTP copied body replay mismatch");
	xrtHttpBodyChunkRelease(&Chunk);
	xrtHttpBodyReaderDestroy(pReplay);
	xrtHttpBodyDestroy(pCopy);

	pBody = xrtHttpBodyReference(
		(xbytesview){ (cbytes)"leased", 6 },
		testHttpBodyRelease,
		&Release
	);
	testRequire(pBody != NULL, "HTTP referenced body create failed");
	pReader = xrtHttpBodyOpen(pBody);
	testRequire((pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader, 6, &Chunk
		) == XHTTP_BODY_DATA),
		"HTTP referenced body next failed");
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	testRequire(Release.Calls == 0,
		"HTTP body released data before outstanding Chunk");
	testRequire(memcmp(Chunk.Data, "leased", 6) == 0,
		"HTTP body Chunk did not survive Reader");
	xrtHttpBodyChunkRelease(&Chunk);
	testRequire(Release.Calls == 1,
		"HTTP body data release count mismatch");
	testRequire(xrtHttpBodyReference(
		(xbytesview){ (cbytes)"borrow", 6 },
		NULL,
		NULL
	) == NULL, "HTTP body Reference accepted no release operation");
	xrtClearError();
}



/* 验证输出别名不会覆盖正文、Reader 或固定数据。 */
static void testHttpBodyAliases(void)
{
	union {
		size_t Size;
		uint8 Bytes[32];
	} Alias;
	xhttpbody* pBody = xrtHttpBodyCopy(
		(xbytesview){ (cbytes)"abcdef", 6 }
	);
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xbytesview View;
	size_t iSize = 99;

	testRequire((pBody != NULL) &&
		xrtHttpBodyView(pBody, &View),
		"HTTP body alias fixture failed");
	testRequire(!xrtHttpBodyView(
		pBody, (xbytesview*)(ptr)View.Data
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP body View accepted output over fixed data");
	xrtClearError();

	pReader = xrtHttpBodyOpen(pBody);
	testRequire(pReader != NULL,
		"HTTP body alias Reader open failed");
	testRequire((xrtHttpBodyNext(
		pReader, 6, (xhttpbodychunk*)pReader
	) == XHTTP_BODY_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP body Next accepted output over Reader");
	xrtClearError();
	testRequire((xrtHttpBodyNext(
		pReader, 6, (xhttpbodychunk*)(ptr)View.Data
	) == XHTTP_BODY_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP body Next accepted output over fixed data");
	xrtClearError();

	testRequire((xrtHttpBodyRead(
		pReader, pReader, 1, &iSize
	) == XHTTP_BODY_ERROR) && (iSize == 99) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP body Read accepted output over Reader");
	xrtClearError();
	testRequire((xrtHttpBodyRead(
		pReader, (ptr)View.Data, View.Size, &iSize
	) == XHTTP_BODY_ERROR) && (iSize == 99) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP body Read accepted output over fixed data");
	xrtClearError();
	Alias.Size = 77;
	testRequire((xrtHttpBodyRead(
		pReader,
		Alias.Bytes,
		sizeof(Alias.Bytes),
		&Alias.Size
	) == XHTTP_BODY_ERROR) && (Alias.Size == 77) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP body Read accepted overlapping size output");
	xrtClearError();

	testRequire((xrtHttpBodyNext(
		pReader, 6, &Chunk
	) == XHTTP_BODY_DATA) && (Chunk.Size == 6) &&
		(memcmp(Chunk.Data, "abcdef", 6) == 0),
		"HTTP body alias failure changed Reader state");
	xrtHttpBodyChunkRelease(&Chunk);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
}



/* 验证 Open 错误隔离和全部无返回清理过程的错误保持。 */
static void testHttpBodyCleanupErrors(void)
{
	static const xhttpbodyops FailureOps = {
		testHttpBodyOpenFailure,
		NULL
	};
	static const xhttpbodyops CleanupOps = {
		testHttpBodyCleanupOpen,
		testHttpBodyCleanupDestroy
	};
	test_http_body_open_failure Failure = { 0 };
	test_http_body_cleanup Cleanup = { 0 };
	xhttpbodychunk Chunk = {
		(cbytes)"x",
		1,
		testHttpBodyCleanupRelease,
		&Cleanup
	};
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xerror* pOld = xrtErrorCreate(
		XERR_VALUE, "test.http.body.old", 81, "old error"
	);
	xerror* pSource = xrtErrorCreate(
		XERR_IO, "test.http.body.open", 82, "open failed"
	);
	xerror* pCleanup = xrtErrorCreate(
		XERR_STATE, "test.http.body.cleanup", 83, "cleanup error"
	);

	testRequire(
		(pOld != NULL) && (pSource != NULL) && (pCleanup != NULL),
		"HTTP body cleanup error setup failed"
	);
	Failure.Error = pSource;
	pBody = xrtHttpBodyCreate(
		&FailureOps,
		&Failure,
		XHTTP_BODY_UNKNOWN,
		XHTTP_BODY_REPLAYABLE
	);
	testRequire(pBody != NULL,
		"HTTP body source error setup failed");
	xrtSetError(pSource);
	testRequire(
		(xrtHttpBodyOpen(pBody) == NULL) &&
		(xrtGetError() == pSource) &&
		(Failure.Opens == 1),
		"HTTP body Open replaced a republished source error"
	);
	xrtClearError();
	xrtHttpBodyDestroy(pBody);

	memset(&Failure, 0, sizeof(Failure));
	pBody = xrtHttpBodyCreate(
		&FailureOps,
		&Failure,
		XHTTP_BODY_UNKNOWN,
		XHTTP_BODY_REPLAYABLE
	);
	xrtSetError(pOld);
	testRequire(
		(xrtHttpBodyOpen(pBody) == NULL) &&
		(xrtGetError() != NULL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "http.body") == 0) &&
		(xrtErrorCode(xrtGetError()) == XHTTP_BODY_ERROR_SOURCE),
		"HTTP body Open reused a stale error"
	);
	xrtClearError();
	xrtHttpBodyDestroy(pBody);

	Cleanup.Error = pCleanup;
	xrtSetError(pOld);
	xrtHttpBodyChunkRelease(&Chunk);
	testRequire(
		(Cleanup.Releases == 1) && (Chunk.Data == NULL) &&
		(xrtGetError() == pOld),
		"HTTP body Chunk cleanup replaced the current error"
	);
	pBody = xrtHttpBodyCreate(
		&CleanupOps,
		&Cleanup,
		0,
		XHTTP_BODY_REPLAYABLE
	);
	pReader = xrtHttpBodyOpen(pBody);
	testRequire((pBody != NULL) && (pReader != NULL),
		"HTTP body cleanup lifecycle setup failed");
	xrtSetError(pOld);
	xrtHttpBodyReaderDestroy(pReader);
	testRequire(
		(Cleanup.Closes == 1) && (xrtGetError() == pOld),
		"HTTP body Reader cleanup replaced the current error"
	);
	xrtHttpBodyDestroy(pBody);
	testRequire(
		(Cleanup.Destroys == 1) && (xrtGetError() == pOld),
		"HTTP body factory cleanup replaced the current error"
	);
	xrtClearError();
	xrtErrorFree(pCleanup);
	xrtErrorFree(pSource);
	xrtErrorFree(pOld);
}



/* 自定义来源状态覆盖一次性 Open、长度校验和关闭。 */
typedef struct test_http_body_source {
	size_t Opens;
	size_t Closes;
	size_t Step;
	size_t Mode;
} test_http_body_source;



/* 测试来源直接借用静态工厂状态。 */
static bool testHttpBodyOpenSource(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
);



/* 测试来源关闭时记录一次生命周期结束。 */
static void testHttpBodyCloseSource(ptr pContext)
{
	test_http_body_source* pSource =
		(test_http_body_source*)pContext;

	pSource->Closes++;
}



/* 按测试模式发布正常数据或故意违反来源契约。 */
static xhttpbodystatus testHttpBodyNextSource(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	static const uint8 Data[] = "abc";
	test_http_body_source* pSource =
		(test_http_body_source*)pContext;

	if ( pSource->Mode == 1 ) {
		pChunk->Data = Data;
		pChunk->Release = testHttpBodyStaticRelease;
		return XHTTP_BODY_DATA;
	}
	if ( pSource->Mode == 2 ) {
		pChunk->Data = Data;
		pChunk->Size = iMaxBytes + 1;
		pChunk->Release = testHttpBodyStaticRelease;
		return XHTTP_BODY_DATA;
	}
	if ( pSource->Mode == 3 ) {
		return XHTTP_BODY_EOF;
	}
	if ( pSource->Mode == 4 ) {
		pChunk->Data = Data;
		pChunk->Size = 3;
		pChunk->Release = testHttpBodyStaticRelease;
		return XHTTP_BODY_DATA;
	}
	if ( pSource->Mode == 5 ) {
		xerror* pError;

		xrtClearError();
		pError = xrtErrorCreate(
			XERR_IO,
			"test.http.body.source",
			71,
			"source read failed"
		);

		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return XHTTP_BODY_ERROR;
	}
	if ( pSource->Mode == 6 ) {
		return XHTTP_BODY_ERROR;
	}
	if ( pSource->Mode == 7 ) {
		pChunk->Data = (cbytes)(uintptr_t)(UINTPTR_MAX - 1u);
		pChunk->Size = 3;
		pChunk->Release = testHttpBodyStaticRelease;
		return XHTTP_BODY_DATA;
	}
	if ( pSource->Step++ == 0 ) {
		pChunk->Data = Data;
		pChunk->Size = 3 < iMaxBytes ? 3 : iMaxBytes;
		pChunk->Release = testHttpBodyStaticRelease;
		return XHTTP_BODY_DATA;
	}
	return XHTTP_BODY_EOF;
}



/* 测试工厂每次 Open 返回一组完整 Reader 操作。 */
static bool testHttpBodyOpenSource(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	test_http_body_source* pSource =
		(test_http_body_source*)pFactory;

	pSource->Opens++;
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testHttpBodyNextSource;
	pOps->Close = testHttpBodyCloseSource;
	*ppReader = pSource;
	return true;
}



/* 验证一次性来源只允许一次 Open，且已知长度必须严格匹配。 */
static void testHttpBodySource(void)
{
	xhttpbodyops Ops = {
		testHttpBodyOpenSource,
		NULL
	};
	test_http_body_source Source = { 0 };
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xbytesview View = {
		(cbytes)"unchanged",
		9
	};
	xerror* pOld;

	pBody = xrtHttpBodyCreate(
		&Ops, &Source, 3, XHTTP_BODY_NONE
	);
	testRequire(pBody != NULL, "custom HTTP body create failed");
	pOld = xrtErrorCreate(
		XERR_VALUE, "test.old", 19, "old error"
	);
	testRequire(pOld != NULL,
		"custom HTTP body view error setup failed");
	xrtSetError(pOld);
	xrtErrorFree(pOld);
	testRequire(!xrtHttpBodyView(pBody, &View) &&
		(View.Data == NULL) &&
		(View.Size == 0) &&
		(xrtGetError() != NULL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "test.old") == 0) &&
		(xrtErrorCode(xrtGetError()) == 19),
		"custom HTTP body view contract mismatch");
	xrtClearError();
	pReader = xrtHttpBodyOpen(pBody);
	testRequire((pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader, 8, &Chunk
		) == XHTTP_BODY_DATA) &&
		(Chunk.Size == 3),
		"custom HTTP body data mismatch");
	xrtHttpBodyChunkRelease(&Chunk);
	testRequire(xrtHttpBodyNext(
		pReader, 8, &Chunk
	) == XHTTP_BODY_EOF, "custom HTTP body EOF mismatch");
	testRequire(xrtHttpBodyOpen(pBody) == NULL,
		"non-replayable HTTP body reopened");
	testRequire((xrtErrorDomain(xrtGetError()) != NULL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "http.body") == 0) &&
		(xrtErrorCode(xrtGetError()) == XHTTP_BODY_ERROR_REOPEN),
		"non-replayable HTTP body error mismatch");
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	testRequire((Source.Opens == 1) && (Source.Closes == 1),
		"custom HTTP body lifecycle mismatch");
	testRequire(!xrtHttpBodyView(NULL, &View) &&
		(xrtGetError() != NULL),
		"HTTP body view accepted null body");
	xrtClearError();
	testRequire(!xrtHttpBodyView(
		(const xhttpbody*)&View,
		&View
	) && (xrtGetError() != NULL),
		"HTTP body view accepted overlapping output");
	xrtClearError();
}



/* 在一种来源错误模式下验证 Reader 固定失败并重放同一错误。 */
static void testHttpBodyContractMode(
	size_t iMode,
	uint64 iLength
)
{
	xhttpbodyops Ops = {
		testHttpBodyOpenSource,
		NULL
	};
	test_http_body_source Source = { 0 };
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	const xerror* pFirst;

	Source.Mode = iMode;
	pBody = xrtHttpBodyCreate(
		&Ops, &Source, iLength, XHTTP_BODY_REPLAYABLE
	);
	testRequire(pBody != NULL,
		"contract HTTP body create failed");
	pReader = xrtHttpBodyOpen(pBody);
	testRequire((pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader, 3, &Chunk
		) == XHTTP_BODY_ERROR),
		"invalid HTTP body source did not fail");
	pFirst = xrtHttpBodyReaderError(pReader);
	testRequire((pFirst != NULL) &&
		(xrtHttpBodyNext(
			pReader, 3, &Chunk
		) == XHTTP_BODY_ERROR) &&
		(xrtHttpBodyReaderError(pReader) == pFirst),
		"HTTP body source error was not stable");
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtClearError();
}



/* 验证来源失败不会复用旧错误，也不会覆盖来源发布的新错误。 */
static void testHttpBodySourceErrors(void)
{
	xhttpbodyops Ops = {
		testHttpBodyOpenSource,
		NULL
	};
	test_http_body_source Source = { 0 };
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xerror* pOld;

	Source.Mode = 6;
	pBody = xrtHttpBodyCreate(
		&Ops, &Source, XHTTP_BODY_UNKNOWN, XHTTP_BODY_REPLAYABLE
	);
	testRequire(pBody != NULL,
		"stale-error HTTP body create failed");
	pReader = xrtHttpBodyOpen(pBody);
	pOld = xrtErrorCreate(
		XERR_VALUE, "test.old", 11, "old error"
	);
	testRequire((pReader != NULL) && (pOld != NULL),
		"stale-error HTTP body setup failed");
	xrtSetError(pOld);
	xrtErrorFree(pOld);
	testRequire(xrtHttpBodyNext(
		pReader, 3, &Chunk
	) == XHTTP_BODY_ERROR, "HTTP body source failure was ignored");
	testRequire((xrtHttpBodyReaderError(pReader) != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtHttpBodyReaderError(pReader)),
			"http.body"
		) == 0),
		"HTTP body source reused a stale error");
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtClearError();

	memset(&Source, 0, sizeof(Source));
	Source.Mode = 5;
	pBody = xrtHttpBodyCreate(
		&Ops, &Source, XHTTP_BODY_UNKNOWN, XHTTP_BODY_REPLAYABLE
	);
	pReader = xrtHttpBodyOpen(pBody);
	pOld = xrtErrorCreate(
		XERR_VALUE, "test.old", 11, "old error"
	);
	testRequire((pReader != NULL) && (pOld != NULL),
		"custom-error HTTP body setup failed");
	xrtSetError(pOld);
	xrtErrorFree(pOld);
	testRequire((pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader, 3, &Chunk
		) == XHTTP_BODY_ERROR),
		"custom-error HTTP body source did not fail");
	testRequire((xrtHttpBodyReaderError(pReader) != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtHttpBodyReaderError(pReader)),
			"test.http.body.source"
		) == 0) &&
		(xrtErrorCode(
			xrtHttpBodyReaderError(pReader)
		) == 71),
		"HTTP body source error was replaced");
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtClearError();
}



/* 验证来源状态、Chunk 和已知长度的全部防御边界。 */
static void testHttpBodyContracts(void)
{
	testHttpBodyContractMode(1, XHTTP_BODY_UNKNOWN);
	testHttpBodyContractMode(2, XHTTP_BODY_UNKNOWN);
	testHttpBodyContractMode(3, 3);
	testHttpBodyContractMode(4, 2);
	testHttpBodyContractMode(7, XHTTP_BODY_UNKNOWN);
}



/* 运行 HTTP 正文基础契约测试。 */
int main(void)
{
	testHttpBodyBytes();
	testHttpBodyAliases();
	testHttpBodySource();
	testHttpBodyContracts();
	testHttpBodySourceErrors();
	testHttpBodyCleanupErrors();
	printf("[PASS] HTTP body\n");
	return 0;
}
