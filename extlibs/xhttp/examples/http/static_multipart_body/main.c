#include <stdio.h>

#include <xhttp.h>



/* 从异步文件创建可直接交给 HTTP 响应发送器的多范围正文。 */
int main(int argc, char** argv)
{
	xtaskpoolconfig PoolConfig = { 2, 32, 0 };
	xhttpbyterange Ranges[2];
	xtaskpool* pPool;
	xasyncfile* pFile;
	xhttpbody* pBody;
	xfile File;
	cstr sPath;
	uint64 iSize;

	sPath = argc == 2 ? argv[1] : argv[0];
	File = xrtOpen(sPath, XFILE_READ);
	if ( (File == NULL) ||
		!xrtFileSize(File, &iSize) ||
		(iSize < 2) ) {
		if ( File != NULL ) {
			(void)xrtClose(File);
		}
		return 2;
	}
	Ranges[0].First = 0;
	Ranges[0].Last = 0;
	Ranges[1].First = iSize - 1u;
	Ranges[1].Last = iSize - 1u;
	pPool = xrtTaskPoolCreate(&PoolConfig);
	if ( pPool == NULL ) {
		(void)xrtClose(File);
		return 3;
	}
	pFile = xrtAsyncFileAdopt(
		pPool,
		File
	);
	if ( pFile == NULL ) {
		(void)xrtClose(File);
		(void)xrtTaskPoolDestroy(pPool);
		return 4;
	}
	pBody = xrtHttpStaticMultipartBodyAdopt(
		pFile,
		Ranges,
		2,
		iSize,
		XRT_STR_LITERAL("application/octet-stream"),
		XRT_STR_LITERAL("xrt-example")
	);
	if ( pBody == NULL ) {
		xfuture* pClose = xrtAsyncFileClose(pFile);

		xrtFutureDestroy(pClose);
		(void)xrtTaskPoolDestroy(pPool);
		return 5;
	}
	printf(
		"multipart content-length: %llu\n",
		(unsigned long long)xrtHttpBodyLength(pBody)
	);
	xrtHttpBodyDestroy(pBody);
	return xrtTaskPoolDestroy(pPool) ? 0 : 6;
}
