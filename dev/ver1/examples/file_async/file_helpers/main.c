/*
 * XRT Example - Async File Helpers
 * XRT 范例 - 异步文件辅助函数
 *
 * Description / 说明:
 *   EN: Demonstrates async text/binary whole-file helpers and file copy/move/delete.
 *   CN: 演示异步文本/二进制整文件 helper，以及文件复制、移动、删除。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/file_async_file_helpers.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/file_async_file_helpers -lm -lpthread
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


int main(void)
{
	const char sText[] = "name=xrt\nmode=async\n";
	const uint8 aucBinary[] = { 0x10u, 0x20u, 0x30u, 0x40u, 0x50u };
	str sTextPath = NULL;
	str sCopyPath = NULL;
	str sMovePath = NULL;
	str sBinaryPath = NULL;
	xasyncfileio* pInfo = NULL;
	xasyncfilebuf* pBuf = NULL;
	int iRet = 0;

	xrtInit();

	sTextPath = exMakeAppFilePath("file_async_file_helpers.txt");
	sCopyPath = exMakeAppFilePath("file_async_file_helpers_copy.txt");
	sMovePath = exMakeAppFilePath("file_async_file_helpers_move.txt");
	sBinaryPath = exMakeAppFilePath("file_async_file_helpers.bin");

	xrtFileDelete(sTextPath);
	xrtFileDelete(sCopyPath);
	xrtFileDelete(sMovePath);
	xrtFileDelete(sBinaryPath);

	pInfo = WaitIo(xrtFileWriteAllAsync(sTextPath, (str)sText, 0u, XRT_CP_UTF8), "write_text");
	if ( pInfo == NULL ) {
		iRet = 1;
		goto cleanup;
	}
	printf("write_text_bytes = %llu\n", (unsigned long long)pInfo->iValue);
	xrtAsyncFileIoDestroy(pInfo);
	pInfo = NULL;

	pBuf = WaitBuf(xrtFileReadAllAsync(sTextPath, XRT_CP_UTF8), "read_text");
	if ( pBuf == NULL ) {
		iRet = 1;
		goto cleanup;
	}
	printf("read_text_size = %llu\n", (unsigned long long)pBuf->iSize);
	printf("read_text = %.*s\n", (int)pBuf->iSize, (const char*)pBuf->pData);
	xrtAsyncFileBufDestroy(pBuf);
	pBuf = NULL;

	pInfo = WaitIo(xrtFilePutAllAsync(sBinaryPath, aucBinary, sizeof(aucBinary)), "put_binary");
	if ( pInfo == NULL ) {
		iRet = 1;
		goto cleanup;
	}
	printf("put_binary_bytes = %llu\n", (unsigned long long)pInfo->iValue);
	xrtAsyncFileIoDestroy(pInfo);
	pInfo = NULL;

	pBuf = WaitBuf(xrtFileGetAllAsync(sBinaryPath), "get_binary");
	if ( pBuf == NULL ) {
		iRet = 1;
		goto cleanup;
	}
	printf("get_binary_size = %llu\n", (unsigned long long)pBuf->iSize);
	printf("get_binary_hex = ");
	exPrintHex((const uint8*)pBuf->pData, pBuf->iSize);
	printf("\n");
	xrtAsyncFileBufDestroy(pBuf);
	pBuf = NULL;

	pInfo = WaitIo(xrtFileCopyAsync(sTextPath, sCopyPath, TRUE), "copy_file");
	if ( pInfo == NULL ) {
		iRet = 1;
		goto cleanup;
	}
	printf("copy_file_result = %llu\n", (unsigned long long)pInfo->iValue);
	xrtAsyncFileIoDestroy(pInfo);
	pInfo = NULL;

	pInfo = WaitIo(xrtFileMoveAsync(sCopyPath, sMovePath, TRUE), "move_file");
	if ( pInfo == NULL ) {
		iRet = 1;
		goto cleanup;
	}
	printf("move_file_result = %llu\n", (unsigned long long)pInfo->iValue);
	xrtAsyncFileIoDestroy(pInfo);
	pInfo = NULL;

	pInfo = WaitIo(xrtFileDeleteAsync(sMovePath), "delete_file");
	if ( pInfo == NULL ) {
		iRet = 1;
		goto cleanup;
	}
	printf("delete_file_result = %llu\n", (unsigned long long)pInfo->iValue);
	xrtAsyncFileIoDestroy(pInfo);
	pInfo = NULL;

	printf("text_exists = %s\n", exBoolText(xrtFileExists(sTextPath)));
	printf("moved_exists = %s\n", exBoolText(xrtFileExists(sMovePath)));
	printf("binary_exists = %s\n", exBoolText(xrtFileExists(sBinaryPath)));

cleanup:
	if ( pBuf != NULL ) {
		xrtAsyncFileBufDestroy(pBuf);
	}
	if ( pInfo != NULL ) {
		xrtAsyncFileIoDestroy(pInfo);
	}
	if ( sTextPath != NULL ) {
		xrtFileDelete(sTextPath);
		xrtFree(sTextPath);
	}
	if ( sCopyPath != NULL ) {
		xrtFileDelete(sCopyPath);
		xrtFree(sCopyPath);
	}
	if ( sMovePath != NULL ) {
		xrtFileDelete(sMovePath);
		xrtFree(sMovePath);
	}
	if ( sBinaryPath != NULL ) {
		xrtFileDelete(sBinaryPath);
		xrtFree(sBinaryPath);
	}
	xrtUnit();
	return iRet;
}
