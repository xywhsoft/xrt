#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件保留流式 gzip 解压和细粒度 Chunk。 */
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
	xhttpbodyinflateconfig Config;
	xhttpbody* pSource = xrtHttpBodyBorrow(
		(xbytesview){ Encoded, sizeof(Encoded) }
	);
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	uint8 Output[sizeof(Expected) - 1u];
	size_t iOutput = 0;
	xhttpbodystatus Status = XHTTP_BODY_ERROR;

	xrtHttpBodyInflateConfigInit(&Config);
	Config.Inflate.Format = XINFLATE_GZIP;
	pBody = xrtHttpBodyInflate(pSource, &Config);
	xrtHttpBodyDestroy(pSource);
	if ( pBody == NULL ) {
		return 1;
	}
	pReader = xrtHttpBodyOpen(pBody);
	while ( (pReader != NULL) &&
		((Status = xrtHttpBodyNext(
			pReader, 1, &Chunk
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
