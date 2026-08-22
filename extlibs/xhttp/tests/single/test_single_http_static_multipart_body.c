#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头发布必须能采用异步文件并创建多范围正文。 */
int main(void)
{
	static const char sPath[] =
		".xrt-single-http-static-multipart.tmp";
	xtaskpoolconfig PoolConfig = { 1, 8, 0 };
	xhttpbyterange Ranges[2] = {
		{ 0, 0 },
		{ 2, 2 }
	};
	xfileoptions Options;
	xtaskpool* pPool;
	xasyncfile* pAsync;
	xhttpbody* pBody;
	xfile File;

	(void)xrtFileDelete(sPath);
	xrtClearError();
	File = xrtOpen(
		sPath,
		XFILE_WRITE | XFILE_CREATE |
		XFILE_TRUNCATE
	);
	if ( (File == NULL) ||
		!xrtWriteFull(File, "abc", 3, NULL) ||
		!xrtClose(File) ) {
		return 1;
	}
	pPool = xrtTaskPoolCreate(&PoolConfig);
	if ( pPool == NULL ) {
		return 2;
	}
	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_READ;
	pAsync = xrtAsyncFileOpen(
		pPool,
		sPath,
		&Options
	);
	if ( pAsync == NULL ) {
		return 3;
	}
	pBody = xrtHttpStaticMultipartBodyAdopt(
		pAsync,
		Ranges,
		2,
		3,
		XRT_STR_LITERAL("text/plain"),
		XRT_STR_LITERAL("single")
	);
	if ( pBody == NULL ) {
		return 4;
	}
	xrtHttpBodyDestroy(pBody);
	if ( !xrtTaskPoolDestroy(pPool) ||
		!xrtFileDelete(sPath) ) {
		return 5;
	}
	return 0;
}
