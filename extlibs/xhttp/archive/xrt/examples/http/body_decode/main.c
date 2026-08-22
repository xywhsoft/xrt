#include <xrt/http_body_decode.h>

#include <stdio.h>



/* 展示从 Content-Encoding 直接得到可流式读取的解码 Body。 */
int main(void)
{
	static const uint8 Encoded[] = {
		0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x0a, 0x4b, 0xce, 0x28, 0xcd,
		0xcb, 0x56, 0xc8, 0xc9, 0x4c, 0x4b, 0x2d,
		0xc9, 0xcc, 0x4d, 0x05, 0x00, 0x19, 0x03,
		0x8f, 0x38, 0x0e, 0x00, 0x00, 0x00
	};
	xhttpbody* pSource = xrtHttpBodyBorrow(
		(xbytesview){ Encoded, sizeof(Encoded) }
	);
	xhttpbody* pBody = NULL;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;

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
	while ( (Status = xrtHttpBodyNext(
		pReader, 8, &Chunk
	)) == XHTTP_BODY_DATA ) {
		fwrite(Chunk.Data, 1, Chunk.Size, stdout);
		xrtHttpBodyChunkRelease(&Chunk);
	}
	fputc('\n', stdout);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	return Status == XHTTP_BODY_EOF ? 0 : 1;
}
