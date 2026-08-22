/*
 * XRT Example - Async Directory Tasks
 * XRT 范例 - 异步目录任务
 *
 * Description / 说明:
 *   EN: Demonstrates async directory create-all, copy, move, and delete tasks.
 *   CN: 演示异步目录创建、复制、移动和删除任务。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/file_async_dir_tasks.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/file_async_dir_tasks -lm -lpthread
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


int main(void)
{
	str sSrcDir = NULL;
	str sSrcNested = NULL;
	str sSrcFile = NULL;
	str sCopyDir = NULL;
	str sMovedDir = NULL;
	xasyncfileio* pInfo = NULL;
	int iRet = 0;

	xrtInit();

	sSrcDir = exMakeAppFilePath("file_async_dir_tasks_src");
	sCopyDir = exMakeAppFilePath("file_async_dir_tasks_copy");
	sMovedDir = exMakeAppFilePath("file_async_dir_tasks_moved");
	sSrcNested = xrtPathJoin(2, sSrcDir, "a/b");
	sSrcFile = xrtPathJoin(2, sSrcNested, "seed.txt");

	xrtDirDelete(sSrcDir);
	xrtDirDelete(sCopyDir);
	xrtDirDelete(sMovedDir);

	pInfo = WaitIo(xrtDirCreateAllAsync(sSrcNested), "create_all");
	if ( pInfo == NULL ) {
		iRet = 1;
		goto cleanup;
	}
	printf("create_all_result = %llu\n", (unsigned long long)pInfo->iValue);
	xrtAsyncFileIoDestroy(pInfo);
	pInfo = NULL;

	if ( xrtFileWriteAll(sSrcFile, (str)"nested-seed", 0u, XRT_CP_UTF8) <= 0 ) {
		printf("seed_write = failed\n");
		iRet = 1;
		goto cleanup;
	}

	pInfo = WaitIo(xrtDirCopyAsync(sSrcDir, sCopyDir, TRUE), "copy_dir");
	if ( pInfo == NULL ) {
		iRet = 1;
		goto cleanup;
	}
	printf("copy_dir_result = %llu\n", (unsigned long long)pInfo->iValue);
	xrtAsyncFileIoDestroy(pInfo);
	pInfo = NULL;

	pInfo = WaitIo(xrtDirMoveAsync(sCopyDir, sMovedDir, TRUE), "move_dir");
	if ( pInfo == NULL ) {
		iRet = 1;
		goto cleanup;
	}
	printf("move_dir_result = %llu\n", (unsigned long long)pInfo->iValue);
	xrtAsyncFileIoDestroy(pInfo);
	pInfo = NULL;

	pInfo = WaitIo(xrtDirDeleteAsync(sMovedDir), "delete_dir");
	if ( pInfo == NULL ) {
		iRet = 1;
		goto cleanup;
	}
	printf("delete_dir_result = %llu\n", (unsigned long long)pInfo->iValue);
	xrtAsyncFileIoDestroy(pInfo);
	pInfo = NULL;

	printf("src_exists = %s\n", exBoolText(xrtDirExists(sSrcDir)));
	printf("copy_exists = %s\n", exBoolText(xrtDirExists(sCopyDir)));
	printf("moved_exists = %s\n", exBoolText(xrtDirExists(sMovedDir)));

cleanup:
	if ( pInfo != NULL ) {
		xrtAsyncFileIoDestroy(pInfo);
	}
	if ( sSrcDir != NULL ) {
		xrtDirDelete(sSrcDir);
		xrtFree(sSrcDir);
	}
	if ( sCopyDir != NULL ) {
		xrtDirDelete(sCopyDir);
		xrtFree(sCopyDir);
	}
	if ( sMovedDir != NULL ) {
		xrtDirDelete(sMovedDir);
		xrtFree(sMovedDir);
	}
	if ( sSrcNested != NULL ) {
		xrtFree(sSrcNested);
	}
	if ( sSrcFile != NULL ) {
		xrtFree(sSrcFile);
	}
	xrtUnit();
	return iRet;
}
