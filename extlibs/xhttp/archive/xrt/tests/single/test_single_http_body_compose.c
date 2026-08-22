#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件中的字节与正文组合主路径。 */
int main(void)
{
	xhttpbody* pChild = xrtHttpBodyBorrow(
		(xbytesview){ (cbytes)"body", 4 }
	);
	xhttpbodypiece Pieces[3];
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	char Output[8];
	size_t iWritten = 0;

	if ( pChild == NULL ) {
		return 1;
	}
	Pieces[0] = xrtHttpBodyPieceBytes(
		(xbytesview){ (cbytes)"[", 1 }
	);
	Pieces[1] = xrtHttpBodyPieceBody(pChild);
	Pieces[2] = xrtHttpBodyPieceBytes(
		(xbytesview){ (cbytes)"]", 1 }
	);
	pBody = xrtHttpBodyCompose(Pieces, 3);
	xrtHttpBodyDestroy(pChild);
	if ( pBody == NULL ) {
		return 1;
	}
	pReader = xrtHttpBodyOpen(pBody);
	while ( (pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader, 2, &Chunk
		) == XHTTP_BODY_DATA) ) {
		memcpy(Output + iWritten, Chunk.Data, Chunk.Size);
		iWritten += Chunk.Size;
		xrtHttpBodyChunkRelease(&Chunk);
	}
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	if ( (iWritten != 6) ||
		(memcmp(Output, "[body]", 6) != 0) ) {
		return 1;
	}
	printf("[PASS] single-http-body-compose\n");
	return 0;
}
