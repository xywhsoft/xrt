#include <xrt.h>

#include <stdio.h>



/* 组合固定元数据与已有正文，并按流式 Chunk 消费。 */
int main(void)
{
	xhttpbody* pValue = xrtHttpBodyBorrow(
		(xbytesview){ (cbytes)"42", 2 }
	);
	xhttpbodypiece Pieces[3];
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xhttpbodystatus Status = XHTTP_BODY_ERROR;
	bool bOpened;

	if ( pValue == NULL ) {
		return 1;
	}
	Pieces[0] = xrtHttpBodyPieceBytes(
		(xbytesview){ (cbytes)"{\"value\":", 9 }
	);
	Pieces[1] = xrtHttpBodyPieceBody(pValue);
	Pieces[2] = xrtHttpBodyPieceBytes(
		(xbytesview){ (cbytes)"}", 1 }
	);
	pBody = xrtHttpBodyCompose(Pieces, 3);
	xrtHttpBodyDestroy(pValue);
	if ( pBody == NULL ) {
		return 1;
	}
	pReader = xrtHttpBodyOpen(pBody);
	bOpened = pReader != NULL;
	while ( (pReader != NULL) &&
		((Status = xrtHttpBodyNext(
			pReader, 16, &Chunk
		)) == XHTTP_BODY_DATA) ) {
		printf("%.*s", (int)Chunk.Size, (cstr)Chunk.Data);
		xrtHttpBodyChunkRelease(&Chunk);
	}
	printf("\n");
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	return bOpened && (Status == XHTTP_BODY_EOF) ? 0 : 1;
}
