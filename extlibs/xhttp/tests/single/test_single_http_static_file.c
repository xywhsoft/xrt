#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头文件可完成受根约束的静态文件准备与区间读取。 */
int main(void)
{
	static const char sPath[] =
		"xrt-single-http-static-file.tmp";
	xtaskpoolconfig Config = { 1, 8, 0 };
	xtaskpool* pPool;
	xhttpstaticfile* pFile;
	xhttpbody* pBody;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xfuture* pWait;
	xroot Root;
	xfile File;
	bool bPass = false;

	(void)xrtFileDelete(sPath);
	xrtClearError();
	File = xrtOpen(
		sPath,
		XFILE_WRITE |
		XFILE_CREATE |
		XFILE_EXCLUSIVE
	);
	if ( (File == NULL) ||
		!xrtWriteFull(
			File,
			"static",
			6,
			NULL
		) ||
		!xrtClose(File) ) {
		return 1;
	}
	Root = xrtRootOpen(".");
	pPool = xrtTaskPoolCreate(&Config);
	if ( (Root == NULL) || (pPool == NULL) ) {
		return 1;
	}
	pFile = xrtHttpStaticFileOpen(
		pPool,
		Root,
		sPath
	);
	pBody = pFile != NULL ?
		xrtHttpStaticFileTakeBody(
			pFile,
			1,
			4
		) : NULL;
	pReader = pBody != NULL ?
		xrtHttpBodyOpen(pBody) : NULL;
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
			bPass = (Chunk.Size == 4u) &&
				(memcmp(
					Chunk.Data,
					"tati",
					4u
				) == 0);
			xrtHttpBodyChunkRelease(&Chunk);
		}
		xrtFutureDestroy(pWait);
	}
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtHttpStaticFileDestroy(pFile);
	(void)xrtTaskPoolDestroy(pPool);
	(void)xrtRootClose(Root);
	(void)xrtFileDelete(sPath);
	return bPass ? 0 : 1;
}
