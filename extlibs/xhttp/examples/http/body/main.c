#include <xhttp.h>

#include <stdio.h>



/* 以拥有型 Chunk 逐段读取可重放正文。 */
int main(void)
{
	xhttpbody* pBody = xrtHttpBodyBorrow(
		(xbytesview){ (cbytes)"hello body", 10 }
	);
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;

	if ( pBody == NULL ) {
		return 1;
	}
	pReader = xrtHttpBodyOpen(pBody);
	if ( pReader == NULL ) {
		xrtHttpBodyDestroy(pBody);
		return 1;
	}
	while ( (Status = xrtHttpBodyNext(
		pReader, 4, &Chunk
	)) == XHTTP_BODY_DATA ) {
		printf("%.*s\n", (int)Chunk.Size, (cstr)Chunk.Data);
		xrtHttpBodyChunkRelease(&Chunk);
	}
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	return Status == XHTTP_BODY_EOF ? 0 : 1;
}
