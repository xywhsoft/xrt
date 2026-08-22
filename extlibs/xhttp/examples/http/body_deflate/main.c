#include <xhttp.h>

#include <stdio.h>



/* 把任意正文来源包装成可分段发送的 gzip 正文。 */
int main(void)
{
	static const char Text[] =
		"stream stream stream stream stream stream";
	xhttpbody* pSource = xrtHttpBodyBorrow(
		(xbytesview){
			(cbytes)Text,
			sizeof(Text) - 1u
		}
	);
	xhttpbody* pGzip = xrtHttpBodyDeflate(pSource, NULL);
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;
	size_t iCompressed = 0;

	xrtHttpBodyDestroy(pSource);
	if ( pGzip == NULL ) {
		return 1;
	}
	pReader = xrtHttpBodyOpen(pGzip);
	if ( pReader == NULL ) {
		xrtHttpBodyDestroy(pGzip);
		return 1;
	}
	while ( (Status = xrtHttpBodyNext(
		pReader, 16, &Chunk
	)) == XHTTP_BODY_DATA ) {
		iCompressed += Chunk.Size;
		xrtHttpBodyChunkRelease(&Chunk);
	}
	printf(
		"plain=%u gzip=%u\n",
		(unsigned int)(sizeof(Text) - 1u),
		(unsigned int)iCompressed
	);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pGzip);
	return Status == XHTTP_BODY_EOF ? 0 : 1;
}

