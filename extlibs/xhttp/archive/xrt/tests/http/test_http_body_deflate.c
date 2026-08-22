#include "../test.h"



/* 测试来源失败时发布一个可识别的结构化错误。 */
static xhttpbodystatus testHttpBodyDeflateErrorNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	xerror* pError;

	(void)pContext;
	(void)iMaxBytes;
	(void)pChunk;
	pError = xrtErrorCreate(
		XERR_IO,
		"test.http.body.deflate.source",
		81,
		"source failed"
	);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return XHTTP_BODY_ERROR;
}



/* 测试来源打开一个无额外状态的失败 Reader。 */
static bool testHttpBodyDeflateErrorOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	(void)pFactory;
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testHttpBodyDeflateErrorNext;
	*ppReader = NULL;
	return true;
}



/* 空来源用于验证非可重放能力不会被变换层放大。 */
static xhttpbodystatus testHttpBodyDeflateEmptyNext(
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



/* 打开无状态空来源。 */
static bool testHttpBodyDeflateEmptyOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	(void)pFactory;
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testHttpBodyDeflateEmptyNext;
	*ppReader = NULL;
	return true;
}



/* 完整读取正文，并校验每个 Chunk 都服从调用方上限。 */
static size_t testHttpBodyDeflateRead(
	xhttpbody* pBody,
	uint8* pOutput,
	size_t iCapacity,
	size_t iMaxBytes
)
{
	xhttpbodyreader* pReader = xrtHttpBodyOpen(pBody);
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;
	size_t iSize = 0;

	testRequire(pReader != NULL,
		"HTTP Deflate body reader open failed");
	for ( ;; ) {
		Status = xrtHttpBodyNext(
			pReader, iMaxBytes, &Chunk
		);
		if ( Status == XHTTP_BODY_EOF ) {
			break;
		}
		testRequire(Status == XHTTP_BODY_DATA,
			"HTTP Deflate body read failed");
		testRequire((Chunk.Size <= iMaxBytes) &&
			(Chunk.Size <= (iCapacity - iSize)),
			"HTTP Deflate body exceeded output bounds");
		memcpy(pOutput + iSize, Chunk.Data, Chunk.Size);
		iSize += Chunk.Size;
		xrtHttpBodyChunkRelease(&Chunk);
	}
	xrtHttpBodyReaderDestroy(pReader);
	return iSize;
}



/* 验证三种格式、确定性输出、重放和来源引用所有权。 */
static void testHttpBodyDeflateFormats(void)
{
	static const uint8 Input[] =
		"alpha alpha alpha beta beta beta gamma gamma gamma";
	static const xdeflateformat Formats[] = {
		XDEFLATE_RAW,
		XDEFLATE_ZLIB,
		XDEFLATE_GZIP
	};
	xhttpbodydeflateconfig Config;
	uint8 Output[256];
	uint8 Replay[256];
	size_t i;

	for ( i = 0; i < (sizeof(Formats) / sizeof(Formats[0])); i++ ) {
		xhttpbody* pSource = xrtHttpBodyBorrow(
			(xbytesview){ Input, sizeof(Input) - 1u }
		);
		xhttpbody* pBody;
		bytes pExpected;
		size_t iExpected;
		size_t iOutput;
		size_t iReplay;

		testRequire(pSource != NULL,
			"HTTP Deflate source creation failed");
		xrtHttpBodyDeflateConfigInit(&Config);
		Config.Deflate.Format = Formats[i];
		Config.ReadSize = 7;
		pExpected = xrtDeflateAll(
			(xbytesview){ Input, sizeof(Input) - 1u },
			&Config.Deflate,
			&iExpected
		);
		pBody = xrtHttpBodyDeflate(pSource, &Config);
		xrtHttpBodyDestroy(pSource);
		testRequire((pExpected != NULL) &&
			(pBody != NULL) &&
			xrtHttpBodyReplayable(pBody) &&
			(xrtHttpBodyLength(pBody) == XHTTP_BODY_UNKNOWN),
			"HTTP Deflate body metadata mismatch");
		iOutput = testHttpBodyDeflateRead(
			pBody, Output, sizeof(Output), 1
		);
		iReplay = testHttpBodyDeflateRead(
			pBody, Replay, sizeof(Replay), 13
		);
		testRequire((iOutput == iExpected) &&
			(iReplay == iExpected) &&
			(memcmp(Output, pExpected, iExpected) == 0) &&
			(memcmp(Replay, pExpected, iExpected) == 0),
			"HTTP Deflate body output mismatch");
		xrtFree(pExpected);
		xrtHttpBodyDestroy(pBody);
	}
}



/* 验证已交付 Chunk 可以安全晚于 Reader、Body 和来源销毁。 */
static void testHttpBodyDeflateChunkLifetime(void)
{
	xhttpbody* pSource = xrtHttpBodyBorrow(
		XRT_BYTES_LITERAL("chunk lifetime")
	);
	xhttpbody* pBody = xrtHttpBodyDeflate(pSource, NULL);
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	uint8 iByte;

	testRequire((pSource != NULL) && (pBody != NULL),
		"HTTP Deflate chunk lifetime setup failed");
	xrtHttpBodyDestroy(pSource);
	pReader = xrtHttpBodyOpen(pBody);
	testRequire((pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader, 1, &Chunk
		) == XHTTP_BODY_DATA),
		"HTTP Deflate chunk lifetime read failed");
	iByte = Chunk.Data[0];
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	testRequire(Chunk.Data[0] == iByte,
		"HTTP Deflate chunk died with its reader");
	xrtHttpBodyChunkRelease(&Chunk);
}



/* 验证压缩上限和来源错误都形成稳定 Reader 失败终态。 */
static void testHttpBodyDeflateErrors(void)
{
	static const xhttpbodyops ErrorOps = {
		testHttpBodyDeflateErrorOpen,
		NULL
	};
	xhttpbodydeflateconfig Config;
	xhttpbody* pSource;
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	const xerror* pFirst;

	pSource = xrtHttpBodyBorrow(
		XRT_BYTES_LITERAL("output limit")
	);
	xrtHttpBodyDeflateConfigInit(&Config);
	Config.Deflate.OutputLimit = 1;
	pBody = xrtHttpBodyDeflate(pSource, &Config);
	pReader = xrtHttpBodyOpen(pBody);
	testRequire((pSource != NULL) &&
		(pBody != NULL) &&
		(pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader, 64, &Chunk
		) == XHTTP_BODY_ERROR),
		"HTTP Deflate output limit was ignored");
	pFirst = xrtHttpBodyReaderError(pReader);
	testRequire((pFirst != NULL) &&
		(strcmp(xrtErrorDomain(pFirst), "xrt.deflate") == 0) &&
		(xrtErrorCode(pFirst) == XDEFLATE_ERROR_LIMIT) &&
		(xrtHttpBodyNext(
			pReader, 64, &Chunk
		) == XHTTP_BODY_ERROR) &&
		(xrtHttpBodyReaderError(pReader) == pFirst),
		"HTTP Deflate limit error was not stable");
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtHttpBodyDestroy(pSource);
	xrtClearError();

	pSource = xrtHttpBodyCreate(
		&ErrorOps,
		NULL,
		XHTTP_BODY_UNKNOWN,
		XHTTP_BODY_REPLAYABLE
	);
	pBody = xrtHttpBodyDeflate(pSource, NULL);
	pReader = xrtHttpBodyOpen(pBody);
	testRequire((pSource != NULL) &&
		(pBody != NULL) &&
		(pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader, 64, &Chunk
		) == XHTTP_BODY_ERROR) &&
		(strcmp(
			xrtErrorDomain(xrtHttpBodyReaderError(pReader)),
			"test.http.body.deflate.source"
		) == 0),
		"HTTP Deflate replaced its source error");
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtHttpBodyDestroy(pSource);
	xrtClearError();

	pSource = xrtHttpBodyBorrow(
		XRT_BYTES_LITERAL("queue limit")
	);
	xrtHttpBodyDeflateConfigInit(&Config);
	Config.QueueLimit = 9;
	pBody = xrtHttpBodyDeflate(pSource, &Config);
	pReader = xrtHttpBodyOpen(pBody);
	testRequire(
		(pSource != NULL) &&
		(pBody != NULL) &&
		(pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader,
			64,
			&Chunk
		) == XHTTP_BODY_ERROR) &&
		(xrtHttpBodyReaderError(pReader) != NULL) &&
		(xrtErrorKind(
			xrtHttpBodyReaderError(pReader)
		) == XERR_RANGE),
		"HTTP Deflate queue limit was ignored"
	);
	pFirst = xrtHttpBodyReaderError(pReader);
	testRequire(
		(xrtHttpBodyNext(
			pReader,
			64,
			&Chunk
		) == XHTTP_BODY_ERROR) &&
		(xrtHttpBodyReaderError(pReader) == pFirst),
		"HTTP Deflate queue-limit error was not stable"
	);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtHttpBodyDestroy(pSource);
	xrtClearError();
}



/* 验证配置和非可重放来源的公开边界。 */
static void testHttpBodyDeflateContracts(void)
{
	static const xhttpbodyops EmptyOps = {
		testHttpBodyDeflateEmptyOpen,
		NULL
	};
	xhttpbodydeflateconfig Config;
	xhttpbody* pSource;
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	uint8 arrConfig[sizeof(xhttpbodydeflateconfig) + 2u];
	xhttpbodydeflateconfig Snapshot;

	xrtHttpBodyDeflateConfigInit(&Config);
	testRequire((Config.Deflate.Format == XDEFLATE_GZIP) &&
		(Config.ReadSize == XHTTP_BODY_DEFLATE_READ_DEFAULT) &&
		(Config.QueueLimit == XHTTP_BODY_DEFLATE_QUEUE_DEFAULT),
		"HTTP Deflate default config mismatch");
	memset(arrConfig, 0xA5, sizeof(arrConfig));
	xrtHttpBodyDeflateConfigInit(
		(xhttpbodydeflateconfig*)(arrConfig + 1u)
	);
	memcpy(&Snapshot, arrConfig + 1u, sizeof(Snapshot));
	testRequire(
		(Snapshot.Deflate.Format == XDEFLATE_GZIP) &&
		(Snapshot.ReadSize == XHTTP_BODY_DEFLATE_READ_DEFAULT) &&
		(Snapshot.QueueLimit == XHTTP_BODY_DEFLATE_QUEUE_DEFAULT) &&
		(arrConfig[0] == 0xA5) &&
		(arrConfig[sizeof(arrConfig) - 1u] == 0xA5),
		"HTTP Deflate unaligned config init mismatch"
	);
	Snapshot.ReadSize = 1;
	memcpy(arrConfig + 1u, &Snapshot, sizeof(Snapshot));
	pSource = xrtHttpBodyBorrow(XRT_BYTES_LITERAL("snapshot"));
	pBody = xrtHttpBodyDeflate(
		pSource,
		(const xhttpbodydeflateconfig*)(arrConfig + 1u)
	);
	Snapshot.ReadSize = 0;
	memcpy(arrConfig + 1u, &Snapshot, sizeof(Snapshot));
	pReader = xrtHttpBodyOpen(pBody);
	testRequire(
		(pSource != NULL) &&
		(pBody != NULL) &&
		(pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader,
			1,
			&Chunk
		) == XHTTP_BODY_DATA),
		"HTTP Deflate did not snapshot unaligned config"
	);
	xrtHttpBodyChunkRelease(&Chunk);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtHttpBodyDestroy(pSource);

	Config.ReadSize = 0;
	pSource = xrtHttpBodyEmpty();
	testRequire((pSource != NULL) &&
		(xrtHttpBodyDeflate(pSource, &Config) == NULL),
		"HTTP Deflate accepted zero ReadSize");
	xrtClearError();
	xrtHttpBodyDeflateConfigInit(&Config);
	Config.Deflate.Format = (xdeflateformat)99;
	testRequire(xrtHttpBodyDeflate(pSource, &Config) == NULL,
		"HTTP Deflate accepted invalid Deflate config");
	xrtClearError();
	testRequire(xrtHttpBodyDeflate(NULL, NULL) == NULL,
		"HTTP Deflate accepted a null source");
	xrtClearError();
	testRequire(xrtHttpBodyDeflate(
		pSource,
		(const xhttpbodydeflateconfig*)(uintptr_t)(
			UINTPTR_MAX - 1u
		)
	) == NULL, "HTTP Deflate accepted wrapping config");
	xrtClearError();
	xrtHttpBodyDeflateConfigInit(
		(xhttpbodydeflateconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(xrtGetError() != NULL,
		"HTTP Deflate config init accepted wrapping output");
	xrtClearError();
	xrtHttpBodyDestroy(pSource);

	pSource = xrtHttpBodyCreate(
		&EmptyOps,
		NULL,
		0,
		XHTTP_BODY_NONE
	);
	pBody = xrtHttpBodyDeflate(pSource, NULL);
	testRequire((pSource != NULL) &&
		(pBody != NULL) &&
		!xrtHttpBodyReplayable(pBody),
		"HTTP Deflate enlarged source replay capability");
	pReader = xrtHttpBodyOpen(pBody);
	testRequire(pReader != NULL,
		"HTTP Deflate non-replayable first open failed");
	xrtHttpBodyReaderDestroy(pReader);
	testRequire(xrtHttpBodyOpen(pBody) == NULL,
		"HTTP Deflate reopened a non-replayable source");
	xrtClearError();
	xrtHttpBodyDestroy(pBody);
	xrtHttpBodyDestroy(pSource);
}



/* 执行 HTTP 流式压缩正文测试。 */
int main(void)
{
	testHttpBodyDeflateFormats();
	testHttpBodyDeflateChunkLifetime();
	testHttpBodyDeflateErrors();
	testHttpBodyDeflateContracts();
	printf("[PASS] HTTP Deflate body\n");
	return 0;
}
