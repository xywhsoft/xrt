/*
 * XRT Example - Async File Read Write
 * XRT 范例 - 异步文件读写
 *
 * Description / 说明:
 *   EN: Demonstrates explicit-offset async write, flush, size query, and read.
 *   CN: 演示显式偏移异步写入、flush、文件大小查询和读取。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/file_async_async_read_write.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/file_async_async_read_write -lm -lpthread
 */

#include "../../../xrt.c"
#include "../../example_common.h"


static xasyncfileio* WaitIo(xfuture* pFuture, const char* sStep)
{
	xasyncfileio* pInfo = NULL;

	if ( pFuture == NULL ) {
		printf("%s = future_null\n", sStep);
		return NULL;
	}

	pInfo = (xasyncfileio*)xFutureWaitValueTimeout(pFuture, 5000);
	if ( (pInfo == NULL) || (xFutureStatus(pFuture) != XRT_NET_OK) ) {
		printf("%s_status = %d\n", sStep, xFutureStatus(pFuture));
		printf("%s_error = %s\n", sStep, (const char*)xFutureError(pFuture));
		xFutureRelease(pFuture);
		return NULL;
	}

	xFutureRelease(pFuture);
	return pInfo;
}


static xasyncfilebuf* WaitBuf(xfuture* pFuture, const char* sStep)
{
	xasyncfilebuf* pBuf = NULL;

	if ( pFuture == NULL ) {
		printf("%s = future_null\n", sStep);
		return NULL;
	}

	pBuf = (xasyncfilebuf*)xFutureWaitValueTimeout(pFuture, 5000);
	if ( (pBuf == NULL) || (xFutureStatus(pFuture) != XRT_NET_OK) ) {
		printf("%s_status = %d\n", sStep, xFutureStatus(pFuture));
		printf("%s_error = %s\n", sStep, (const char*)xFutureError(pFuture));
		xFutureRelease(pFuture);
		return NULL;
	}

	xFutureRelease(pFuture);
	return pBuf;
}


static bool WaitOk(xfuture* pFuture, const char* sStep)
{
	bool bOk = FALSE;

	if ( pFuture == NULL ) {
		printf("%s = future_null\n", sStep);
		return FALSE;
	}

	bOk = xFutureWaitTimeout(pFuture, 5000) && (xFutureStatus(pFuture) == XRT_NET_OK);
	if ( !bOk ) {
		printf("%s_status = %d\n", sStep, xFutureStatus(pFuture));
		printf("%s_error = %s\n", sStep, (const char*)xFutureError(pFuture));
	}

	xFutureRelease(pFuture);
	return bOk;
}


int main(void)
{
	const char sPartA[] = "async-";
	const char sPartB[] = "file";
	str sPath = NULL;
	xasyncfileconfig tConfig;
	xasyncfile* pFile = NULL;
	xasyncfileio* pInfo = NULL;
	xasyncfilebuf* pBuf = NULL;
	int iRet = 0;

	xrtInit();

	sPath = exMakeAppFilePath("file_async_async_read_write.txt");
	xrtFileDelete(sPath);

	xrtAsyncFileConfigInit(&tConfig);
	tConfig.iFlags = XAFILE_F_READ | XAFILE_F_WRITE | XAFILE_F_CREATE | XAFILE_F_TRUNCATE;
	tConfig.iShareFlags = XAFILE_SHARE_READ;
	tConfig.sPath = sPath;

	pFile = xrtAsyncFileOpen(&tConfig);
	if ( pFile == NULL ) {
		printf("open_status = failed\n");
		printf("open_error = %s\n", xrtGetError());
		iRet = 1;
		goto cleanup;
	}

	pInfo = WaitIo(xrtAsyncFileWriteAt(pFile, 0u, sPartA, sizeof(sPartA) - 1u), "write_part_a");
	if ( pInfo == NULL ) {
		iRet = 1;
		goto cleanup;
	}
	printf("write_part_a_bytes = %llu\n", (unsigned long long)pInfo->iValue);
	printf("write_part_a_offset = %llu\n", (unsigned long long)pInfo->iOffset);
	xrtAsyncFileIoDestroy(pInfo);
	pInfo = NULL;

	pInfo = WaitIo(xrtAsyncFileWriteAt(pFile, sizeof(sPartA) - 1u, sPartB, sizeof(sPartB) - 1u), "write_part_b");
	if ( pInfo == NULL ) {
		iRet = 1;
		goto cleanup;
	}
	printf("write_part_b_bytes = %llu\n", (unsigned long long)pInfo->iValue);
	printf("write_part_b_offset = %llu\n", (unsigned long long)pInfo->iOffset);
	xrtAsyncFileIoDestroy(pInfo);
	pInfo = NULL;

	if ( !WaitOk(xrtAsyncFileFlush(pFile), "flush") ) {
		iRet = 1;
		goto cleanup;
	}

	pInfo = WaitIo(xrtAsyncFileGetSize(pFile), "get_size");
	if ( pInfo == NULL ) {
		iRet = 1;
		goto cleanup;
	}
	printf("file_size = %llu\n", (unsigned long long)pInfo->iValue);
	xrtAsyncFileIoDestroy(pInfo);
	pInfo = NULL;

	pBuf = WaitBuf(xrtAsyncFileReadAt(pFile, 0u, sizeof(sPartA) + sizeof(sPartB) - 2u), "read_back");
	if ( pBuf == NULL ) {
		iRet = 1;
		goto cleanup;
	}
	printf("read_size = %llu\n", (unsigned long long)pBuf->iSize);
	printf("read_offset = %llu\n", (unsigned long long)pBuf->iOffset);
	printf("read_eof = %s\n", exBoolText(pBuf->bEOF));
	printf("read_text = %.*s\n", (int)pBuf->iSize, (const char*)pBuf->pData);
	xrtAsyncFileBufDestroy(pBuf);
	pBuf = NULL;

cleanup:
	if ( pBuf != NULL ) {
		xrtAsyncFileBufDestroy(pBuf);
	}
	if ( pInfo != NULL ) {
		xrtAsyncFileIoDestroy(pInfo);
	}
	if ( pFile != NULL ) {
		xrtAsyncFileClose(pFile);
	}
	if ( sPath != NULL ) {
		xrtFileDelete(sPath);
		xrtFree(sPath);
	}
	xrtUnit();
	return iRet;
}
