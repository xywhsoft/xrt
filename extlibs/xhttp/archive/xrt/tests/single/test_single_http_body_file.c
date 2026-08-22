#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件能够执行完整的异步文件正文链路。 */
int main(void)
{
	static const char sPath[] = "xrt-single-http-body-file.tmp";
	xtaskpoolconfig Config = { 1, 8, 0 };
	xtaskpool* pPool;
	xfile File;
	xfuture* pPrepare;
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xfuture* pWait;
	bool bPass = false;

	(void)xrtFileDelete(sPath);
	xrtClearError();
	File = xrtOpen(
		sPath,
		XFILE_WRITE | XFILE_CREATE | XFILE_EXCLUSIVE
	);
	if ( (File == NULL) ||
		!xrtWriteFull(File, "file", 4, NULL) ||
		!xrtClose(File) ) {
		return 1;
	}
	pPool = xrtTaskPoolCreate(&Config);
	if ( pPool == NULL ) {
		(void)xrtFileDelete(sPath);
		return 1;
	}
	pPrepare = xrtHttpBodyFileFuture(pPool, sPath, NULL);
	if ( (pPrepare != NULL) &&
		(xrtFutureWaitFor(
			pPrepare,
			UINT64_C(2000000)
		) == XWAIT_OK) &&
		(xrtFutureState(pPrepare) == XFUTURE_RESOLVED) ) {
		pBody = (xhttpbody*)xrtFutureValue(pPrepare);
		pReader = xrtHttpBodyOpen(pBody);
		if ( (pReader != NULL) &&
			(xrtHttpBodyNext(
				pReader,
				4,
				&Chunk
			) == XHTTP_BODY_AGAIN) ) {
			pWait = xrtHttpBodyReaderWait(pReader);
			if ( (pWait != NULL) &&
				(xrtFutureWaitFor(
					pWait,
					UINT64_C(2000000)
				) == XWAIT_OK) &&
				(xrtHttpBodyNext(
					pReader,
					4,
					&Chunk
				) == XHTTP_BODY_DATA) ) {
				bPass = (Chunk.Size == 4) &&
					(memcmp(Chunk.Data, "file", 4) == 0);
				xrtHttpBodyChunkRelease(&Chunk);
			}
			xrtFutureDestroy(pWait);
		}
		xrtHttpBodyReaderDestroy(pReader);
	}
	xrtFutureDestroy(pPrepare);
	(void)xrtTaskPoolDestroy(pPool);
	(void)xrtFileDelete(sPath);
	return bPass ? 0 : 1;
}
