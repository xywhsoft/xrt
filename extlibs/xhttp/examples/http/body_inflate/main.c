#include <xhttp.h>

#include <stdio.h>



/* 把 gzip 线路正文包装成可分段读取的明文正文。 */
int main(void)
{
	static const uint8 Encoded[] = {
		0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x0a, 0x4b, 0x2c, 0xae, 0xcc,
		0x4b, 0x56, 0xc8, 0xcc, 0x4b, 0xcb, 0x49,
		0x2c, 0x49, 0x55, 0x48, 0xca, 0x4f, 0xa9,
		0x04, 0x00, 0x23, 0xf9, 0xbb, 0x00, 0x12,
		0x00, 0x00, 0x00
	};
	xhttpbodyinflateconfig Config;
	xhttpbody* pSource = xrtHttpBodyBorrow(
		(xbytesview){ Encoded, sizeof(Encoded) }
	);
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;
	size_t iDecoded = 0;

	xrtHttpBodyInflateConfigInit(&Config);
	Config.Inflate.Format = XINFLATE_GZIP;
	pBody = xrtHttpBodyInflate(pSource, &Config);
	xrtHttpBodyDestroy(pSource);
	if ( pBody == NULL ) {
		return 1;
	}
	pReader = xrtHttpBodyOpen(pBody);
	if ( pReader == NULL ) {
		xrtHttpBodyDestroy(pBody);
		return 1;
	}
	while ( (Status = xrtHttpBodyNext(
		pReader, 5, &Chunk
	)) == XHTTP_BODY_DATA ) {
		(void)fwrite(Chunk.Data, 1, Chunk.Size, stdout);
		iDecoded += Chunk.Size;
		xrtHttpBodyChunkRelease(&Chunk);
	}
	printf("\ndecoded=%u\n", (unsigned int)iDecoded);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	return Status == XHTTP_BODY_EOF ? 0 : 1;
}

