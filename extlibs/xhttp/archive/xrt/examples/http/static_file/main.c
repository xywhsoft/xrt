#include <xrt.h>

#include <stdio.h>



/* 展示从受限文件根准备静态表示元数据和完整文件正文。 */
int main(void)
{
	static const char sPath[] =
		"xrt-http-static-file-example.tmp";
	xtaskpoolconfig Config = { 1, 8, 0 };
	const xhttprepresentation* pCurrent;
	xtaskpool* pPool;
	xhttpstaticfile* pFile;
	xhttpbody* pBody;
	xroot Root;
	xfile File;
	int iResult = 1;

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
			"static resource",
			15,
			NULL
		) ||
		!xrtClose(File) ) {
		return 1;
	}
	Root = xrtRootOpen(".");
	pPool = xrtTaskPoolCreate(&Config);
	pFile = (Root != NULL) && (pPool != NULL) ?
		xrtHttpStaticFileOpen(
			pPool,
			Root,
			sPath
		) : NULL;
	pCurrent = pFile != NULL ?
		xrtHttpStaticFileRepresentation(
			pFile
		) : NULL;
	pBody = pFile != NULL ?
		xrtHttpStaticFileTakeBodyAll(
			pFile
		) : NULL;
	if ( (pCurrent != NULL) &&
		pCurrent->HasETag &&
		(pBody != NULL) ) {
		printf(
			"size=%llu weak-etag=%.*s\n",
			(unsigned long long)
				xrtHttpBodyLength(pBody),
			(int)pCurrent->ETag.Opaque.Size,
			pCurrent->ETag.Opaque.Data
		);
		iResult = 0;
	}
	xrtHttpBodyDestroy(pBody);
	xrtHttpStaticFileDestroy(pFile);
	if ( pPool != NULL ) {
		(void)xrtTaskPoolDestroy(pPool);
	}
	if ( Root != NULL ) {
		(void)xrtRootClose(Root);
	}
	(void)xrtFileDelete(sPath);
	return iResult;
}
