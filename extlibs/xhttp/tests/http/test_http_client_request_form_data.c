#include "../test.h"



/* 判断一个正文 Chunk 是否完整包含指定的短文本。 */
static bool testHttpRequestChunkContains(
	const xhttpbodychunk* pChunk,
	cstr sText
)
{
	size_t iText = strlen(sText);
	size_t i;

	if ( iText > pChunk->Size ) {
		return false;
	}
	for ( i = 0; i <= (pChunk->Size - iText); i++ ) {
		if ( memcmp(pChunk->Data + i, sText, iText) == 0 ) {
			return true;
		}
	}
	return false;
}



/* 验证请求正文仍由公开的流式组合器构成，并包含字段元数据与原始正文。 */
static void testHttpRequestFormDataBody(xhttpbody* pBody)
{
	xhttpbodyreader* pReader = xrtHttpBodyOpen(pBody);
	bool bName = false;
	bool bValue = false;
	bool bFilename = false;
	bool bBinary = false;

	testRequire(
		(pReader != NULL) && xrtHttpBodyReplayable(pBody) &&
		(xrtHttpBodyLength(pBody) != XHTTP_BODY_UNKNOWN),
		"HTTP request FormData body capabilities mismatch"
	);
	for ( ;; ) {
		xhttpbodychunk Chunk;
		xhttpbodystatus Status = xrtHttpBodyNext(
			pReader,
			1024u,
			&Chunk
		);

		if ( Status == XHTTP_BODY_EOF ) {
			break;
		}
		testRequire(
			Status == XHTTP_BODY_DATA,
			"HTTP request FormData body read failed"
		);
		bName = bName || testHttpRequestChunkContains(
			&Chunk, "name=\"title\""
		);
		bValue = bValue || testHttpRequestChunkContains(
			&Chunk, "xrt runtime"
		);
		bFilename = bFilename || testHttpRequestChunkContains(
			&Chunk, "filename=\"data.bin\""
		);
		bBinary = bBinary || testHttpRequestChunkContains(
			&Chunk, "binary"
		);
		xrtHttpBodyChunkRelease(&Chunk);
	}
	xrtHttpBodyReaderDestroy(pReader);
	testRequire(
		bName && bValue && bFilename && bBinary,
		"HTTP request FormData encoded content mismatch"
	);
}



/* 验证给定 boundary 的 FormData 正文、Content-Type 与失败原子性。 */
int main(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("https://example.test/upload")
	);
	xformdata* pForm = xrtFormDataCreate(NULL);
	xmultipartboundary Boundary;
	xmultipartboundary Parsed;
	xmultipartboundary Invalid = { 0 };
	xstrview Filename = XRT_STR_LITERAL("data.bin");
	xhttpbody* pBefore;
	const xhttpfield* pType;

	testRequire(
		(pRequest != NULL) && (pForm != NULL) &&
		xrtMultipartBoundaryParse(
			XRT_STR_LITERAL("xrt-client-form"),
			&Boundary
		) && xrtHttpRequestSetBytes(
			pRequest,
			XRT_BYTES_LITERAL("old"),
			XRT_STR_LITERAL("text/plain")
		) && xrtFormDataAppendText(
			pForm,
			XRT_STR_LITERAL("title"),
			XRT_STR_LITERAL("xrt runtime")
		) && xrtFormDataAppendBytes(
			pForm,
			XRT_STR_LITERAL("file"),
			XRT_BYTES_LITERAL("binary"),
			&Filename,
			XRT_STR_LITERAL("application/octet-stream")
		),
		"HTTP request FormData setup failed"
	);
	pBefore = xrtHttpRequestBody(pRequest);
	testRequire(
		!xrtHttpRequestSetFormData(
			pRequest,
			pForm,
			&Invalid
		) && (xrtHttpRequestBody(pRequest) == pBefore) &&
		(xrtErrorCode(xrtGetError()) ==
			XHTTP_REQUEST_ERROR_FORM_DATA) &&
		(strcmp(
			xrtErrorDomain(xrtGetError()),
			"http.request"
		) == 0),
		"invalid FormData boundary changed request state"
	);
	xrtClearError();

	testRequire(
		xrtHttpRequestSetFormData(
			pRequest,
			pForm,
			&Boundary
		) && (xrtHttpRequestBody(pRequest) != pBefore),
		"HTTP request FormData commit failed"
	);
	pType = xrtHttpRequestHeader(
		pRequest,
		XRT_STR_LITERAL("Content-Type")
	);
	testRequire(
		(pType != NULL) && xrtMultipartBoundaryFromContentType(
			pType->Value,
			&Parsed
		) && (Parsed.Size == Boundary.Size) &&
		(memcmp(
			Parsed.Data,
			Boundary.Data,
			Boundary.Size
		) == 0),
		"HTTP request FormData Content-Type boundary mismatch"
	);
	testHttpRequestFormDataBody(
		xrtHttpRequestBody(pRequest)
	);

	xrtFormDataDestroy(pForm);
	xrtHttpRequestDestroy(pRequest);
	printf("[PASS] HTTP client request FormData\n");
	return 0;
}

