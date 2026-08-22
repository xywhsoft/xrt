#include "../test.h"



/* 把已知长度正文完整读取为拥有型缓冲。 */
static uint8* testFormDataReadBody(
	xhttpbody* pBody,
	size_t* pSize
)
{
	uint64 iLength = xrtHttpBodyLength(pBody);
	xhttpbodyreader* pReader;
	uint8* pOutput;
	size_t iOffset = 0;

	testRequire(iLength < (uint64)SIZE_MAX,
		"FormData encoded body length is not fixed");
	pOutput = (uint8*)xrtMalloc((size_t)iLength + 1u);
	testRequire(pOutput != NULL,
		"FormData encoded body allocation failed");
	pReader = xrtHttpBodyOpen(pBody);
	testRequire(pReader != NULL,
		"FormData encoded body open failed");
	for ( ;; ) {
		xhttpbodychunk Chunk;
		xhttpbodystatus Status = xrtHttpBodyNext(
			pReader, 7, &Chunk
		);

		if ( Status == XHTTP_BODY_EOF ) {
			break;
		}
		testRequire(Status == XHTTP_BODY_DATA,
			"FormData encoded body read failed");
		memcpy(pOutput + iOffset, Chunk.Data, Chunk.Size);
		iOffset += Chunk.Size;
		xrtHttpBodyChunkRelease(&Chunk);
	}
	xrtHttpBodyReaderDestroy(pReader);
	testRequire(iOffset == (size_t)iLength,
		"FormData encoded body length mismatch");
	pOutput[iOffset] = 0;
	*pSize = iOffset;
	return pOutput;
}



/* 未知长度正文借用一个只允许打开一次的流状态。 */
typedef struct test_form_data_stream {
	size_t Offset;
} test_form_data_stream;



/* 静态流数据不需要释放。 */
static void testFormDataStreamRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
}



/* 按调用方上限发布未知长度流数据。 */
static xhttpbodystatus testFormDataStreamNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	static const char Data[] = "stream";
	test_form_data_stream* pState =
		(test_form_data_stream*)pContext;
	size_t iRemaining;
	size_t iSize;

	if ( pState->Offset == (sizeof(Data) - 1u) ) {
		return XHTTP_BODY_EOF;
	}
	iRemaining = (sizeof(Data) - 1u) - pState->Offset;
	iSize = iRemaining < iMaxBytes ? iRemaining : iMaxBytes;
	pChunk->Data = (cbytes)Data + pState->Offset;
	pChunk->Size = iSize;
	pChunk->Release = testFormDataStreamRelease;
	pState->Offset += iSize;
	return XHTTP_BODY_DATA;
}



/* 为未知长度测试返回一次性 Reader。 */
static bool testFormDataStreamOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testFormDataStreamNext;
	*ppReader = pFactory;
	return true;
}



/* 验证容器、编码器和拥有型解析器完整往返。 */
static void testFormDataRoundTrip(void)
{
	static const uint8 Binary[] = {
		0x00, 0x41, 0x0D, 0x0A, 0xFF
	};
	xmultipartboundary Boundary;
	xmultiparterrorinfo Error;
	xformdata* pForm = xrtFormDataCreate(NULL);
	xformdata* pParsed;
	xhttpbody* pEncoded;
	xformdatapart Part;
	xstrview Filename = XRT_STR_LITERAL("a\"b.bin");
	char ContentType[128];
	uint8* pWire;
	xbytesview View;
	size_t iContentType;
	size_t iWire;

	testRequire((pForm != NULL) && xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("xrt-form"), &Boundary
	), "FormData round trip setup failed");
	testRequire(xrtFormDataAppendText(
		pForm, (xstrview){ NULL, 0 }, XRT_STR_LITERAL("empty-name")
	) && xrtFormDataAppendText(
		pForm, XRT_STR_LITERAL("tag"), XRT_STR_LITERAL("one")
	) && xrtFormDataAppendText(
		pForm, XRT_STR_LITERAL("tag"), XRT_STR_LITERAL("two")
	) && xrtFormDataAppendBytes(
		pForm,
		XRT_STR_LITERAL("file"),
		(xbytesview){ Binary, sizeof(Binary) },
		&Filename,
		XRT_STR_LITERAL("application/octet-stream")
	), "FormData round trip append failed");
	pEncoded = xrtFormDataBody(pForm, &Boundary);
	testRequire((pEncoded != NULL) &&
		xrtHttpBodyReplayable(pEncoded),
		"FormData encode failed");
	pWire = testFormDataReadBody(pEncoded, &iWire);
	testRequire(xrtMultipartContentTypeWrite(
		&Boundary,
		ContentType,
		sizeof(ContentType),
		&iContentType
	), "FormData Content-Type write failed");
	pParsed = xrtFormDataParseContentType(
		(xstrview){ ContentType, iContentType },
		(xbytesview){ pWire, iWire },
		NULL,
		NULL,
		&Error
	);
	testRequire((pParsed != NULL) &&
		(xrtFormDataCount(pParsed) == 4) &&
		(xrtFormDataCountName(
			pParsed, XRT_STR_LITERAL("tag")
		) == 2), "FormData parse count mismatch");
	testRequire(xrtFormDataAt(pParsed, 0, &Part) &&
		(Part.Name.Size == 0) &&
		xrtHttpBodyView(Part.Body, &View) &&
		(View.Size == 10) &&
		(memcmp(View.Data, "empty-name", 10) == 0),
		"FormData empty name round trip mismatch");
	testRequire(xrtFormDataAt(pParsed, 3, &Part) &&
		(Part.Filename.Size == 7) &&
		(memcmp(Part.Filename.Data, "a\"b.bin", 7) == 0) &&
		xrtHttpBodyView(Part.Body, &View) &&
		(View.Size == sizeof(Binary)) &&
		(memcmp(View.Data, Binary, sizeof(Binary)) == 0),
		"FormData file round trip mismatch");
	xrtFormDataDestroy(pParsed);
	xrtFree(pWire);
	xrtHttpBodyDestroy(pEncoded);
	xrtFormDataDestroy(pForm);
}



/* 验证未知长度、不可重放正文的能力会精确传播到封包正文。 */
static void testFormDataStreamBody(void)
{
	static const xhttpbodyops Ops = {
		testFormDataStreamOpen,
		NULL
	};
	test_form_data_stream State = { 0 };
	xformdataconfig Config;
	xmultipartboundary Boundary;
	xformdata* pLimited;
	xformdata* pForm;
	xhttpbody* pStream;
	xhttpbody* pEncoded;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;
	bool bFound = false;

	pStream = xrtHttpBodyCreate(
		&Ops, &State, XHTTP_BODY_UNKNOWN, XHTTP_BODY_NONE
	);
	testRequire((pStream != NULL) && xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("stream-boundary"), &Boundary
	), "stream FormData setup failed");
	xrtFormDataConfigInit(&Config);
	Config.InitialParts = 0;
	Config.MaxPartBytes = 1024u;
	Config.MaxBodyBytes = 1024u;
	pLimited = xrtFormDataCreate(&Config);
	testRequire((pLimited != NULL) && !xrtFormDataAppendBody(
		pLimited,
		XRT_STR_LITERAL("stream"),
		pStream,
		NULL,
		(xstrview){ NULL, 0 }
	) && (xrtFormDataCount(pLimited) == 0),
		"finite FormData accepted unknown body length");
	xrtClearError();
	xrtFormDataDestroy(pLimited);

	pForm = xrtFormDataCreate(NULL);
	testRequire((pForm != NULL) && xrtFormDataAppendBody(
		pForm,
		XRT_STR_LITERAL("stream"),
		pStream,
		NULL,
		(xstrview){ NULL, 0 }
	), "unlimited FormData rejected unknown body length");
	xrtHttpBodyDestroy(pStream);
	pEncoded = xrtFormDataBody(pForm, &Boundary);
	testRequire((pEncoded != NULL) &&
		(xrtHttpBodyLength(pEncoded) == XHTTP_BODY_UNKNOWN) &&
		!xrtHttpBodyReplayable(pEncoded),
		"FormData did not propagate stream body capabilities");
	pReader = xrtHttpBodyOpen(pEncoded);
	testRequire(pReader != NULL,
		"stream FormData body open failed");
	for ( ;; ) {
		Status = xrtHttpBodyNext(pReader, 64, &Chunk);
		if ( Status == XHTTP_BODY_EOF ) {
			break;
		}
		testRequire(Status == XHTTP_BODY_DATA,
			"stream FormData body read failed");
		if ( (Chunk.Size == 6) &&
			(memcmp(Chunk.Data, "stream", 6) == 0) ) {
			bFound = true;
		}
		xrtHttpBodyChunkRelease(&Chunk);
	}
	testRequire(bFound,
		"stream FormData payload was not preserved");
	xrtHttpBodyReaderDestroy(pReader);
	testRequire(xrtHttpBodyOpen(pEncoded) == NULL,
		"non-replayable FormData body reopened");
	xrtClearError();
	xrtHttpBodyDestroy(pEncoded);
	xrtFormDataDestroy(pForm);
}



/* 验证协议错误、本地配额错误和重叠输出保持不同口径。 */
static void testFormDataParseErrors(void)
{
	static const char Encoded[] =
		"--error\r\n"
		"Content-Disposition: form-data; name=\"value\"\r\n"
		"Content-Transfer-Encoding: base64\r\n"
		"\r\n"
		"eA==\r\n"
		"--error--\r\n";
	static const char Plain[] =
		"--error\r\n"
		"Content-Disposition: form-data; name=\"value\"\r\n"
		"Content-Transfer-Encoding: 8bit\r\n"
		"\r\n"
		"abc\r\n"
		"--error--\r\n";
	union {
		xmultiparterrorinfo Align;
		uint8 Bytes[sizeof(Plain)];
	} Storage;
	uint8 Snapshot[sizeof(Plain)];
	xformdataconfig Config;
	xmultipartboundary Boundary;
	xmultiparterrorinfo Error;
	xformdata* pForm;

	testRequire(xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("error"), &Boundary
	), "FormData error boundary setup failed");
	pForm = xrtFormDataParse(
		(xbytesview){ (cbytes)Encoded, sizeof(Encoded) - 1u },
		&Boundary,
		NULL,
		NULL,
		&Error
	);
	testRequire((pForm == NULL) &&
		(Error.Code == XMULTIPART_ERROR_TRANSFER_ENCODING),
		"FormData did not reject transforming transfer encoding");
	xrtClearError();

	pForm = xrtFormDataParse(
		(xbytesview){ (cbytes)Plain, sizeof(Plain) - 1u },
		&Boundary,
		NULL,
		NULL,
		&Error
	);
	testRequire((pForm != NULL) &&
		(xrtFormDataCount(pForm) == 1),
		"FormData rejected transparent transfer encoding");
	xrtFormDataDestroy(pForm);

	xrtFormDataConfigInit(&Config);
	Config.InitialParts = 0;
	Config.MaxPartBytes = 2;
	Config.MaxBodyBytes = 2;
	pForm = xrtFormDataParse(
		(xbytesview){ (cbytes)Plain, sizeof(Plain) - 1u },
		&Boundary,
		&Config,
		NULL,
		&Error
	);
	testRequire((pForm == NULL) &&
		(Error.Code == XMULTIPART_ERROR_NONE),
		"FormData local quota was misreported as protocol failure");
	xrtClearError();

	pForm = xrtFormDataParseContentType(
		XRT_STR_LITERAL("text/plain"),
		(xbytesview){ (cbytes)Plain, sizeof(Plain) - 1u },
		NULL,
		NULL,
		&Error
	);
	testRequire((pForm == NULL) &&
		(Error.Code == XMULTIPART_ERROR_BOUNDARY),
		"FormData invalid Content-Type error mismatch");
	xrtClearError();

	memcpy(Storage.Bytes, Plain, sizeof(Plain));
	memcpy(Snapshot, Plain, sizeof(Plain));
	pForm = xrtFormDataParse(
		(xbytesview){ Storage.Bytes, sizeof(Plain) - 1u },
		&Boundary,
		NULL,
		NULL,
		&Storage.Align
	);
	testRequire((pForm == NULL) &&
		(memcmp(Storage.Bytes, Snapshot, sizeof(Plain)) == 0),
		"FormData overlapping error output corrupted input");
	xrtClearError();
}



/* 验证空 FormData 可以编码和解析，而通用严格校验仍保持原策略。 */
static void testFormDataEmpty(void)
{
	xmultipartboundary Boundary;
	xmultiparterrorinfo Error;
	xformdata* pForm = xrtFormDataCreate(NULL);
	xformdata* pParsed;
	xhttpbody* pBody;
	uint8* pWire;
	size_t iWire;

	testRequire((pForm != NULL) && xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("empty"), &Boundary
	), "empty FormData setup failed");
	pBody = xrtFormDataBody(pForm, &Boundary);
	testRequire(pBody != NULL, "empty FormData encode failed");
	pWire = testFormDataReadBody(pBody, &iWire);
	testRequire(!xrtMultipartValidate(
		(xbytesview){ pWire, iWire },
		&Boundary,
		NULL,
		&Error
	), "strict multipart unexpectedly accepted zero Parts");
	xrtClearError();
	pParsed = xrtFormDataParse(
		(xbytesview){ pWire, iWire },
		&Boundary,
		NULL,
		NULL,
		&Error
	);
	testRequire((pParsed != NULL) &&
		(xrtFormDataCount(pParsed) == 0),
		"empty FormData parse failed");
	xrtFormDataDestroy(pParsed);
	xrtFree(pWire);
	xrtHttpBodyDestroy(pBody);
	xrtFormDataDestroy(pForm);
}



/* 运行 FormData multipart 编码与解析测试。 */
int main(void)
{
	testFormDataRoundTrip();
	testFormDataStreamBody();
	testFormDataParseErrors();
	testFormDataEmpty();
	printf("[PASS] form_data_multipart\n");
	return 0;
}
