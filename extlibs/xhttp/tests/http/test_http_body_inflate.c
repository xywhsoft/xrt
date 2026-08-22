#include "../test.h"
#include "test_http_body_inflate_fixture.h"



/* 测试来源失败时发布一个可识别的结构化错误。 */
static xhttpbodystatus testHttpBodyInflateErrorNext(
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
		"test.http.body.inflate.source",
		91,
		"source failed"
	);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return XHTTP_BODY_ERROR;
}



/* 测试来源打开一个无额外状态的失败 Reader。 */
static bool testHttpBodyInflateErrorOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	(void)pFactory;
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testHttpBodyInflateErrorNext;
	*ppReader = NULL;
	return true;
}



/* 空来源用于验证非可重放能力不会被变换层放大。 */
static xhttpbodystatus testHttpBodyInflateEmptyNext(
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
static bool testHttpBodyInflateEmptyOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	(void)pFactory;
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testHttpBodyInflateEmptyNext;
	*ppReader = NULL;
	return true;
}



/* 创建指定格式和来源推进粒度的解压正文。 */
static xhttpbody* testHttpBodyInflateCreate(
	xbytesview Data,
	xinflateformat Format,
	size_t iReadSize
)
{
	xhttpbodyinflateconfig Config;
	xhttpbody* pSource = xrtHttpBodyBorrow(Data);
	xhttpbody* pBody;

	if ( pSource == NULL ) {
		return NULL;
	}
	xrtHttpBodyInflateConfigInit(&Config);
	Config.Inflate.Format = Format;
	Config.ReadSize = iReadSize;
	pBody = xrtHttpBodyInflate(pSource, &Config);
	xrtHttpBodyDestroy(pSource);
	return pBody;
}



/* 完整读取正文，并返回 EOF 或稳定失败终态。 */
static xhttpbodystatus testHttpBodyInflateRead(
	xhttpbody* pBody,
	uint8* pOutput,
	size_t iCapacity,
	size_t iMaxBytes,
	size_t* pOutputSize
)
{
	xhttpbodyreader* pReader = xrtHttpBodyOpen(pBody);
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;
	size_t iSize = 0;

	testRequire(pReader != NULL,
		"HTTP Inflate body reader open failed");
	while ( (Status = xrtHttpBodyNext(
		pReader, iMaxBytes, &Chunk
	)) == XHTTP_BODY_DATA ) {
		testRequire((Chunk.Size <= iMaxBytes) &&
			(Chunk.Size <= (iCapacity - iSize)),
			"HTTP Inflate body exceeded output bounds");
		memcpy(pOutput + iSize, Chunk.Data, Chunk.Size);
		iSize += Chunk.Size;
		xrtHttpBodyChunkRelease(&Chunk);
	}
	*pOutputSize = iSize;
	xrtHttpBodyReaderDestroy(pReader);
	return Status;
}



/* 验证 raw、zlib、HTTP deflate 双格式识别和 gzip。 */
static void testHttpBodyInflateFormats(void)
{
	typedef struct test_http_body_inflate_case {
		xbytesview Encoded;
		xinflateformat Format;
	} test_http_body_inflate_case;
	static const test_http_body_inflate_case Cases[] = {
		{
			{
				TestHttpBodyInflateRaw,
				sizeof(TestHttpBodyInflateRaw)
			},
			XINFLATE_RAW
		},
		{
			{
				TestHttpBodyInflateZlib,
				sizeof(TestHttpBodyInflateZlib)
			},
			XINFLATE_ZLIB
		},
		{
			{
				TestHttpBodyInflateRaw,
				sizeof(TestHttpBodyInflateRaw)
			},
			XINFLATE_DEFLATE
		},
		{
			{
				TestHttpBodyInflateZlib,
				sizeof(TestHttpBodyInflateZlib)
			},
			XINFLATE_DEFLATE
		},
		{
			{
				TestHttpBodyInflateGzip,
				sizeof(TestHttpBodyInflateGzip)
			},
			XINFLATE_GZIP
		}
	};
	uint8 Output[128];
	uint8 Replay[128];
	size_t i;

	for ( i = 0; i < (sizeof(Cases) / sizeof(Cases[0])); i++ ) {
		xhttpbody* pBody = testHttpBodyInflateCreate(
			Cases[i].Encoded,
			Cases[i].Format,
			3
		);
		size_t iOutput;
		size_t iReplay;

		testRequire((pBody != NULL) &&
			xrtHttpBodyReplayable(pBody) &&
			(xrtHttpBodyLength(pBody) == XHTTP_BODY_UNKNOWN),
			"HTTP Inflate body metadata mismatch");
		testRequire(
			(testHttpBodyInflateRead(
				pBody,
				Output,
				sizeof(Output),
				1,
				&iOutput
			) == XHTTP_BODY_EOF) &&
			(testHttpBodyInflateRead(
				pBody,
				Replay,
				sizeof(Replay),
				13,
				&iReplay
			) == XHTTP_BODY_EOF) &&
			(iOutput == (sizeof(TestHttpBodyInflatePlain) - 1u)) &&
			(iReplay == iOutput) &&
			(memcmp(
				Output,
				TestHttpBodyInflatePlain,
				iOutput
			) == 0) &&
			(memcmp(Replay, Output, iOutput) == 0),
			"HTTP Inflate body output mismatch");
		xrtHttpBodyDestroy(pBody);
	}
}



/* 验证已交付 Chunk 可以安全晚于 Reader、Body 和来源销毁。 */
static void testHttpBodyInflateChunkLifetime(void)
{
	xhttpbody* pBody = testHttpBodyInflateCreate(
		(xbytesview){
			TestHttpBodyInflateGzip,
			sizeof(TestHttpBodyInflateGzip)
		},
		XINFLATE_GZIP,
		4
	);
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	uint8 iByte;

	testRequire(pBody != NULL,
		"HTTP Inflate chunk lifetime setup failed");
	pReader = xrtHttpBodyOpen(pBody);
	testRequire((pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader, 1, &Chunk
		) == XHTTP_BODY_DATA),
		"HTTP Inflate chunk lifetime read failed");
	iByte = Chunk.Data[0];
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	testRequire(Chunk.Data[0] == iByte,
		"HTTP Inflate chunk died with its reader");
	xrtHttpBodyChunkRelease(&Chunk);
}



/* 读取到失败，并验证 Reader 重放同一个结构化 Inflate 错误。 */
static void testHttpBodyInflateFailure(
	xhttpbody* pBody,
	xinflateerror Code,
	cstr sMessage
)
{
	xhttpbodyreader* pReader = xrtHttpBodyOpen(pBody);
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;
	const xerror* pFirst;

	testRequire(pReader != NULL, sMessage);
	while ( (Status = xrtHttpBodyNext(
		pReader, 7, &Chunk
	)) == XHTTP_BODY_DATA ) {
		xrtHttpBodyChunkRelease(&Chunk);
	}
	pFirst = xrtHttpBodyReaderError(pReader);
	testRequire((Status == XHTTP_BODY_ERROR) &&
		(pFirst != NULL) &&
		(strcmp(xrtErrorDomain(pFirst), "xrt.inflate") == 0) &&
		(xrtErrorCode(pFirst) == (int32)Code) &&
		(xrtHttpBodyNext(
			pReader, 7, &Chunk
		) == XHTTP_BODY_ERROR) &&
		(xrtHttpBodyReaderError(pReader) == pFirst),
		sMessage);
	xrtHttpBodyReaderDestroy(pReader);
	xrtClearError();
}



/* 验证截断、校验损坏、解压上限和来源错误。 */
static void testHttpBodyInflateErrors(void)
{
	static const xhttpbodyops ErrorOps = {
		testHttpBodyInflateErrorOpen,
		NULL
	};
	uint8 Corrupt[sizeof(TestHttpBodyInflateGzip)];
	xhttpbodyinflateconfig Config;
	xhttpbody* pSource;
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;

	pBody = testHttpBodyInflateCreate(
		(xbytesview){
			TestHttpBodyInflateGzip,
			sizeof(TestHttpBodyInflateGzip) - 1u
		},
		XINFLATE_GZIP,
		3
	);
	testRequire(pBody != NULL,
		"truncated HTTP Inflate body creation failed");
	testHttpBodyInflateFailure(
		pBody,
		XINFLATE_ERROR_DATA,
		"truncated HTTP Inflate body was accepted"
	);
	xrtHttpBodyDestroy(pBody);

	memcpy(
		Corrupt,
		TestHttpBodyInflateGzip,
		sizeof(Corrupt)
	);
	Corrupt[31] ^= UINT8_C(0x80);
	pBody = testHttpBodyInflateCreate(
		(xbytesview){ Corrupt, sizeof(Corrupt) },
		XINFLATE_GZIP,
		5
	);
	testRequire(pBody != NULL,
		"corrupt HTTP Inflate body creation failed");
	testHttpBodyInflateFailure(
		pBody,
		XINFLATE_ERROR_DATA,
		"corrupt HTTP Inflate body was accepted"
	);
	xrtHttpBodyDestroy(pBody);

	pSource = xrtHttpBodyBorrow(
		(xbytesview){
			TestHttpBodyInflateGzip,
			sizeof(TestHttpBodyInflateGzip)
		}
	);
	xrtHttpBodyInflateConfigInit(&Config);
	Config.Inflate.Format = XINFLATE_GZIP;
	Config.Inflate.OutputLimit = 8;
	pBody = xrtHttpBodyInflate(pSource, &Config);
	testRequire((pSource != NULL) && (pBody != NULL),
		"limited HTTP Inflate body creation failed");
	testHttpBodyInflateFailure(
		pBody,
		XINFLATE_ERROR_LIMIT,
		"HTTP Inflate output limit was ignored"
	);
	xrtHttpBodyDestroy(pBody);
	xrtHttpBodyDestroy(pSource);

	pSource = xrtHttpBodyCreate(
		&ErrorOps,
		NULL,
		XHTTP_BODY_UNKNOWN,
		XHTTP_BODY_REPLAYABLE
	);
	pBody = xrtHttpBodyInflate(pSource, NULL);
	pReader = xrtHttpBodyOpen(pBody);
	testRequire((pSource != NULL) &&
		(pBody != NULL) &&
		(pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader, 64, &Chunk
		) == XHTTP_BODY_ERROR) &&
		(strcmp(
			xrtErrorDomain(xrtHttpBodyReaderError(pReader)),
			"test.http.body.inflate.source"
		) == 0),
		"HTTP Inflate replaced its source error");
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtHttpBodyDestroy(pSource);
	xrtClearError();
}



/* 验证默认配置、参数检查和非可重放来源边界。 */
static void testHttpBodyInflateContracts(void)
{
	static const xhttpbodyops EmptyOps = {
		testHttpBodyInflateEmptyOpen,
		NULL
	};
	xhttpbodyinflateconfig Config;
	xhttpbody* pSource;
	xhttpbody* pBody;
	xhttpbodyreader* pReader;

	xrtHttpBodyInflateConfigInit(&Config);
	testRequire((Config.Inflate.Format == XINFLATE_DEFLATE) &&
		(Config.Inflate.OutputLimit ==
		 XHTTP_BODY_INFLATE_OUTPUT_DEFAULT) &&
		(Config.ReadSize == XHTTP_BODY_INFLATE_READ_DEFAULT) &&
		(Config.QueueLimit == XHTTP_BODY_INFLATE_QUEUE_DEFAULT),
		"HTTP Inflate default config mismatch");
	Config.ReadSize = 0;
	pSource = xrtHttpBodyEmpty();
	testRequire((pSource != NULL) &&
		(xrtHttpBodyInflate(pSource, &Config) == NULL),
		"HTTP Inflate accepted zero ReadSize");
	xrtClearError();
	xrtHttpBodyInflateConfigInit(&Config);
	Config.Inflate.Format = (xinflateformat)99;
	testRequire(xrtHttpBodyInflate(pSource, &Config) == NULL,
		"HTTP Inflate accepted invalid Inflate config");
	xrtClearError();
	testRequire(xrtHttpBodyInflate(NULL, NULL) == NULL,
		"HTTP Inflate accepted a null source");
	xrtClearError();
	testRequire(xrtHttpBodyInflate(
		pSource,
		(const xhttpbodyinflateconfig*)(uintptr_t)(
			UINTPTR_MAX - 1u
		)
	) == NULL, "HTTP Inflate accepted wrapping config");
	xrtClearError();
	xrtHttpBodyInflateConfigInit(NULL);
	testRequire(xrtGetError() != NULL,
		"HTTP Inflate config init accepted null");
	xrtClearError();
	xrtHttpBodyInflateConfigInit(
		(xhttpbodyinflateconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(xrtGetError() != NULL,
		"HTTP Inflate config init accepted wrapping output");
	xrtClearError();
	xrtHttpBodyDestroy(pSource);

	pSource = xrtHttpBodyCreate(
		&EmptyOps,
		NULL,
		0,
		XHTTP_BODY_NONE
	);
	pBody = xrtHttpBodyInflate(pSource, NULL);
	testRequire((pSource != NULL) &&
		(pBody != NULL) &&
		!xrtHttpBodyReplayable(pBody),
		"HTTP Inflate enlarged source replay capability");
	pReader = xrtHttpBodyOpen(pBody);
	testRequire(pReader != NULL,
		"HTTP Inflate non-replayable first open failed");
	xrtHttpBodyReaderDestroy(pReader);
	testRequire(xrtHttpBodyOpen(pBody) == NULL,
		"HTTP Inflate reopened a non-replayable source");
	xrtClearError();
	xrtHttpBodyDestroy(pBody);
	xrtHttpBodyDestroy(pSource);
}



/* 执行 HTTP 流式解压正文测试。 */
int main(void)
{
	testHttpBodyInflateFormats();
	testHttpBodyInflateChunkLifetime();
	testHttpBodyInflateErrors();
	testHttpBodyInflateContracts();
	printf("[PASS] HTTP Inflate body\n");
	return 0;
}

