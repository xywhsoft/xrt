#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_BODY_DEFLATE
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头文件保留流式 gzip Body 和细粒度 Chunk。 */
int main(void)
{
	xhttpbody* pSource = xrtHttpBodyBorrow(
		XRT_BYTES_LITERAL("single body deflate")
	);
	xhttpbody* pBody = xrtHttpBodyDeflate(pSource, NULL);
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	uint8 Header[2];
	size_t i = 0;
	bool bPass = true;

	if ( (pSource == NULL) || (pBody == NULL) ) {
		xrtHttpBodyDestroy(pSource);
		return 1;
	}
	xrtHttpBodyDestroy(pSource);
	pReader = xrtHttpBodyOpen(pBody);
	while ( (pReader != NULL) && (i < sizeof(Header)) ) {
		if ( xrtHttpBodyNext(
			pReader, 1, &Chunk
		) != XHTTP_BODY_DATA ) {
			bPass = false;
			break;
		}
		Header[i++] = Chunk.Data[0];
		xrtHttpBodyChunkRelease(&Chunk);
	}
	bPass = bPass && (i == 2) &&
		(Header[0] == UINT8_C(0x1F)) &&
		(Header[1] == UINT8_C(0x8B));
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	return bPass ? 0 : 1;
}
