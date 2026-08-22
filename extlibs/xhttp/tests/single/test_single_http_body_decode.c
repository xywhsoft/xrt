#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_BODY_DECODE
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头文件保留 Content-Encoding 计划和通用 Body 解码。 */
int main(void)
{
	static const uint8 Encoded[] = {
		0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x0a, 0x4b, 0xce, 0x28, 0xcd,
		0xcb, 0x56, 0xc8, 0xc9, 0x4c, 0x4b, 0x2d,
		0xc9, 0xcc, 0x4d, 0x05, 0x00, 0x19, 0x03,
		0x8f, 0x38, 0x0e, 0x00, 0x00, 0x00
	};
	static const uint8 Expected[] = "chunk lifetime";
	xhttpbody* pSource = xrtHttpBodyBorrow(
		(xbytesview){ Encoded, sizeof(Encoded) }
	);
	xhttpbody* pBody = NULL;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	uint8 Output[sizeof(Expected) - 1u];
	size_t iOutput = 0;
	xhttpbodystatus Status = XHTTP_BODY_ERROR;

	if ( (pSource == NULL) ||
		(xrtHttpBodyDecode(
			pSource,
			XRT_STR_LITERAL("gzip"),
			NULL,
			&pBody
		 ) != XHTTP_BODY_DECODE_APPLIED) ) {
		xrtHttpBodyDestroy(pSource);
		return 1;
	}
	xrtHttpBodyDestroy(pSource);
	pReader = xrtHttpBodyOpen(pBody);
	while ( (pReader != NULL) &&
		((Status = xrtHttpBodyNext(
			pReader, 2, &Chunk
		)) == XHTTP_BODY_DATA) ) {
		if ( Chunk.Size > (sizeof(Output) - iOutput) ) {
			xrtHttpBodyChunkRelease(&Chunk);
			xrtHttpBodyReaderDestroy(pReader);
			xrtHttpBodyDestroy(pBody);
			return 1;
		}
		memcpy(Output + iOutput, Chunk.Data, Chunk.Size);
		iOutput += Chunk.Size;
		xrtHttpBodyChunkRelease(&Chunk);
	}
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	return ((Status == XHTTP_BODY_EOF) &&
		(iOutput == sizeof(Output)) &&
		(memcmp(Output, Expected, sizeof(Output)) == 0)) ? 0 : 1;
}
