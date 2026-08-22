#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头文件能够组合文件 Body、Reply Future 和所有权回收。 */
int main(void)
{
	static const char sPath[] =
		"xrt-single-http-server-file.tmp";
	xtaskpoolconfig Config = { 1, 8, 0 };
	xtaskpool* pPool;
	xfile File;
	xfuture* pFuture;
	xhttpreply* pReply;
	bool bPass = false;

	(void)xrtFileDelete(sPath);
	xrtClearError();
	File = xrtOpen(
		sPath,
		XFILE_WRITE | XFILE_CREATE |
		XFILE_EXCLUSIVE
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
	pFuture = xrtHttpReplyFileFuture(
		pPool,
		200,
		XRT_STR_LITERAL("text/plain"),
		sPath
	);
	if ( (pFuture != NULL) &&
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(2000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pFuture) ==
		 XFUTURE_RESOLVED) ) {
		pReply = (xhttpreply*)xrtFutureValue(
			pFuture
		);
		bPass = (pReply != NULL) &&
			(xrtHttpReplyStatus(pReply) == 200) &&
			(xrtHttpBodyLength(
				xrtHttpReplyBody(pReply)
			 ) == 4);
	}
	xrtFutureDestroy(pFuture);
	(void)xrtTaskPoolDestroy(pPool);
	(void)xrtFileDelete(sPath);
	return bPass ? 0 : 1;
}
