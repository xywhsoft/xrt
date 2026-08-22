#include <stdio.h>

#include <xhttp.h>



/* 等待 Future 成功并返回借用值。 */
static ptr exampleFutureValue(xfuture* pFuture)
{
	if ( (pFuture == NULL) ||
		(xrtFutureWait(pFuture) != XWAIT_OK) ||
		(xrtFutureState(pFuture) != XFUTURE_RESOLVED) ) {
		return NULL;
	}
	return xrtFutureValue(pFuture);
}



/* 使用统一正文 Reader 异步读取文件内容。 */
int main(void)
{
	static const char sPath[] = "xrt-http-body-file-example.tmp";
	xtaskpoolconfig Config = { 1, 16, 0 };
	xtaskpool* pPool;
	xfile File;
	xfuture* pPrepare;
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;

	(void)xrtFileDelete(sPath);
	xrtClearError();
	File = xrtOpen(
		sPath,
		XFILE_WRITE | XFILE_CREATE | XFILE_EXCLUSIVE
	);
	if ( (File == NULL) ||
		!xrtWriteFull(File, "HTTP file body\n", 15, NULL) ||
		!xrtClose(File) ) {
		return 1;
	}
	pPool = xrtTaskPoolCreate(&Config);
	if ( pPool == NULL ) {
		(void)xrtFileDelete(sPath);
		return 1;
	}
	/* NULL 配置按需读取，默认单次申请不超过 64 KiB。 */
	pPrepare = xrtHttpBodyFileFuture(pPool, sPath, NULL);
	pBody = (xhttpbody*)exampleFutureValue(pPrepare);
	pReader = pBody != NULL ? xrtHttpBodyOpen(pBody) : NULL;
	while ( pReader != NULL ) {
		Status = xrtHttpBodyNext(pReader, 4096, &Chunk);
		if ( Status == XHTTP_BODY_AGAIN ) {
			xfuture* pReady = xrtHttpBodyReaderWait(pReader);

			if ( exampleFutureValue(pReady) == NULL ) {
				xrtFutureDestroy(pReady);
				break;
			}
			xrtFutureDestroy(pReady);
			continue;
		}
		if ( Status == XHTTP_BODY_EOF ) {
			break;
		}
		if ( Status == XHTTP_BODY_ERROR ) {
			break;
		}
		(void)fwrite(Chunk.Data, 1, Chunk.Size, stdout);
		xrtHttpBodyChunkRelease(&Chunk);
	}
	xrtHttpBodyReaderDestroy(pReader);
	xrtFutureDestroy(pPrepare);
	(void)xrtTaskPoolDestroy(pPool);
	(void)xrtFileDelete(sPath);
	return 0;
}

