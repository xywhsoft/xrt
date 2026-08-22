#include "../test.h"



/* 把可重放 FormData 正文完整读取到固定测试缓冲区。 */
static size_t testFormDataBodyRead(
	xhttpbody* pBody,
	bytes pOutput,
	size_t iCapacity
)
{
	xhttpbodyreader* pReader = xrtHttpBodyOpen(pBody);
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;
	size_t iOffset = 0;

	testRequire(pReader != NULL,
		"FormData multipart body open failed");
	for ( ;; ) {
		Status = xrtHttpBodyNext(pReader, 7u, &Chunk);
		if ( Status == XHTTP_BODY_EOF ) {
			break;
		}
		testRequire((Status == XHTTP_BODY_DATA) &&
			(Chunk.Size <= (iCapacity - iOffset)),
			"FormData multipart body read failed");
		memcpy(pOutput + iOffset, Chunk.Data, Chunk.Size);
		iOffset += Chunk.Size;
		xrtHttpBodyChunkRelease(&Chunk);
	}
	xrtHttpBodyReaderDestroy(pReader);
	return iOffset;
}



/* 验证显式 boundary 编码保持顺序并产生规范线缆格式。 */
int main(void)
{
	static const char Expected[] =
		"--x\r\n"
		"Content-Disposition: form-data; name=\"a\"\r\n"
		"\r\n"
		"b\r\n"
		"--x--\r\n";
	xmultipartboundary Boundary;
	xformdata* pForm = xrtFormDataCreate(NULL);
	xhttpbody* pBody;
	uint8 Wire[sizeof(Expected) + 8u];
	size_t iWire;

	testRequire((pForm != NULL) && xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("x"), &Boundary
	) && xrtFormDataAppendText(
		pForm, XRT_STR_LITERAL("a"), XRT_STR_LITERAL("b")
	), "FormData multipart setup failed");
	pBody = xrtFormDataBody(pForm, &Boundary);
	testRequire((pBody != NULL) && xrtHttpBodyReplayable(pBody) &&
		(xrtHttpBodyLength(pBody) == (sizeof(Expected) - 1u)),
		"FormData multipart body capabilities mismatch");
	iWire = testFormDataBodyRead(pBody, Wire, sizeof(Wire));
	testRequire((iWire == (sizeof(Expected) - 1u)) &&
		(memcmp(Wire, Expected, iWire) == 0),
		"FormData multipart wire mismatch");
	xrtHttpBodyDestroy(pBody);

	testRequire(xrtFormDataBody(NULL, &Boundary) == NULL,
		"FormData multipart accepted a null container");
	xrtClearError();
	Boundary.Data[0] = '\0';
	testRequire(xrtFormDataBody(pForm, &Boundary) == NULL,
		"FormData multipart accepted a damaged boundary");
	xrtClearError();
	xrtFormDataDestroy(pForm);
	printf("[PASS] form_data_multipart_write\n");
	return 0;
}

