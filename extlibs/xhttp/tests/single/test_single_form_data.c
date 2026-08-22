#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"

#include <stdio.h>



/* 验证单头文件中的 FormData 创建、编码和解析主路径。 */
int main(void)
{
	xmultipartboundary Boundary;
	xmultiparterrorinfo Error;
	xformdata* pForm = xrtFormDataCreate(NULL);
	xformdata* pParsed;
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	uint8 Wire[512];
	size_t iWire = 0;

	if ( (pForm == NULL) || !xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("single-form"), &Boundary
	) || !xrtFormDataAppendText(
		pForm, XRT_STR_LITERAL("name"), XRT_STR_LITERAL("value")
	) ) {
		xrtFormDataDestroy(pForm);
		return 1;
	}
	pBody = xrtFormDataBody(pForm, &Boundary);
	pReader = pBody != NULL ? xrtHttpBodyOpen(pBody) : NULL;
	while ( (pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader, 32, &Chunk
		) == XHTTP_BODY_DATA) ) {
		if ( Chunk.Size > (sizeof(Wire) - iWire) ) {
			xrtHttpBodyChunkRelease(&Chunk);
			xrtHttpBodyReaderDestroy(pReader);
			xrtHttpBodyDestroy(pBody);
			xrtFormDataDestroy(pForm);
			return 1;
		}
		memcpy(Wire + iWire, Chunk.Data, Chunk.Size);
		iWire += Chunk.Size;
		xrtHttpBodyChunkRelease(&Chunk);
	}
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	pParsed = xrtFormDataParse(
		(xbytesview){ Wire, iWire },
		&Boundary,
		NULL,
		NULL,
		&Error
	);
	xrtFormDataDestroy(pForm);
	if ( (pParsed == NULL) || (xrtFormDataCount(pParsed) != 1) ) {
		xrtFormDataDestroy(pParsed);
		return 1;
	}
	xrtFormDataDestroy(pParsed);
	printf("[PASS] single-form-data\n");
	return 0;
}

