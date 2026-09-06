/*
 * 范例：file/async_tour —— 异步文件与目录操作补集
 * ----------------------------------------------------------------
 * 演示 API：
 *   【异步文件】  xrtAsyncFileAdopt（接管已开文件）/
 *                  AsyncFileFlags / WriteAtRef（零复制+释放回调）/
 *                  WriteAtTake（接管分配）/ Flush / Size / Resize /
 *                  AppendAsync
 *   【整文件】    xrtFileReadAllLimitAsync / WriteAllAsync /
 *                  FileMoveAsync
 *   【目录】      xrtDirCreateModeAsync / CreateAllAsync /
 *                  CreateAllModeAsync / EmptyAsync / StatsAsync /
 *                  SizeAsync / EnsureEmptyAsync / CleanAsync /
 *                  DirMoveAsync
 *   【目录树】    xrtFileTreeCopyAsync / TreeRemoveAsync
 * 模块宏：XRT_MODULE_FILE_ASYNC + XRT_MODULE_DIR_ASYNC
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/file/async_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   async-file: adopt read+write=1 write ref+take+flush ok
 *   whole: read-limit/write-all/move ok
 *   async-file: size=22 resize=16 append=4
 *   dir: create-mode/all stats files=1 size=6
 *   dir: ensure-empty/move/clean ok
 *   tree: copy+remove ok
 *
 * 任务池驱动全部 Future（xrtFutureWaitFor 收割）；
 *   WriteAtRef 的释放回调在数据离开任务后恰好执行一次。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define EXAMPLE_TIMEOUT_US	UINT64_C(5000000)

static xatomic32 g_Released;

static void exampleRelease(ptr pContext, cbytes pData,
	size_t iSize)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
	xrtAtomic32FetchAdd(&g_Released, 1, XMEMORY_RELEASE);
}

/* 保留最新 Future，让返回的借用值存活到下一次调用；不泄漏旧 Future。 */
static bool exampleWaitValue(xfuture** ppCurrent, xfuture* pFuture, ptr* pValue)
{
	xrtFutureDestroy(*ppCurrent);
	*ppCurrent = pFuture;
	if ( pValue != NULL ) *pValue = NULL;
	if ( (pFuture == NULL) ||
		(xrtFutureWaitFor(pFuture, EXAMPLE_TIMEOUT_US) !=
			XWAIT_OK) ||
		(xrtFutureState(pFuture) != XFUTURE_RESOLVED) ) {
		return false;
	}
	if ( pValue != NULL ) {
		*pValue = xrtFutureValue(pFuture);
	}
	return true;
}

int main(void)
{
	xtaskpoolconfig PoolConfig = { 2, 32, 0 };
	xtaskpool* pPool = NULL;
	xasyncfile* pFile = NULL;
	xfuture* pFuture = NULL;
	xfile File = NULL;
	xfiledata* pRead = NULL;
	xfilesize* pSize = NULL;
	xwalkstats* pWalk = NULL;
	uint8* pTake = NULL;
	ptr pValue = NULL;
	uint32 iFlags = 0;
	int iResult = 1;

	xrtAtomic32Init(&g_Released, 0);
	pPool = xrtTaskPoolCreate(&PoolConfig);
	if ( pPool == NULL ) {
		goto Cleanup;
	}

	/* ---- Adopt：接管已开文件 + Flags + 零复制双形态 ---- */
	File = xrtOpen("xrt-async-tour.tmp",
		XFILE_READ | XFILE_WRITE | XFILE_CREATE |
		XFILE_TRUNCATE);
	if ( (File == NULL) ||
		((pFile = xrtAsyncFileAdopt(pPool, File)) == NULL) ) {
		goto Cleanup;
	}
	File = NULL;  /* 关闭责任已转移 */
	iFlags = xrtAsyncFileFlags(pFile);
	if ( (iFlags & XFILE_READ) == 0u ||
		(iFlags & XFILE_WRITE) == 0u ) {
		goto Cleanup;
	}
	/* WriteAt：复制写入（ReadAt 核对见 async 范例）。 */
	if ( !exampleWaitValue(&pFuture, xrtAsyncFileWriteAt(pFile, 0u,
			(xbytesview) { (cbytes)"ref-take", 8u }),
			&pValue) ) {
		goto Cleanup;
	}
	/* WriteAtRef：零复制 + 释放回调恰好一次。 */
	if ( !exampleWaitValue(&pFuture, xrtAsyncFileWriteAtRef(pFile, 8u,
			(xbytesview) { (cbytes)"REF-EXTRA", 9u },
			exampleRelease, NULL), &pValue) ||
		!exampleWaitValue(&pFuture, xrtAsyncFileFlush(pFile), NULL) ) {
		goto Cleanup;
	}
	/* WriteAtTake：接管 xrtMalloc 分配。 */
	pTake = (uint8*)xrtMalloc(5);
	if ( pTake == NULL ) {
		goto Cleanup;
	}
	memcpy(pTake, "take!", 5u);
	{
		xfuture* pSubmitted = xrtAsyncFileWriteAtTake(pFile, 17u, pTake, 5u);

		if ( pSubmitted == NULL ) goto Cleanup;
		pTake = NULL;  /* 受理即转移；等待失败也不能再次释放。 */
		if ( !exampleWaitValue(&pFuture, pSubmitted, &pValue) ) goto Cleanup;
	}
	if ( xrtAtomic32Load(&g_Released, XMEMORY_ACQUIRE) != 1 ) goto Cleanup;
	printf("async-file: adopt read+write=1 write ref+take+flush ok\n");

	/* ---- Size / Resize / Append ---- */
	if ( !exampleWaitValue(&pFuture, xrtAsyncFileSize(pFile), &pValue) ||
		((pSize = (xfilesize*)pValue) == NULL) ||
		(pSize->Size != 22u) ||
		!exampleWaitValue(&pFuture, xrtAsyncFileResize(pFile, 16u),
			&pValue) ) {
		goto Cleanup;
	}
	{
		xfuture* pClosing = xrtAsyncFileClose(pFile);

		if ( pClosing == NULL ) goto Cleanup;
		pFile = NULL;  /* Close 已受理并释放对象所有权。 */
		if ( !exampleWaitValue(&pFuture, pClosing, NULL) ) goto Cleanup;
	}
	/* AppendAsync：整文件级追加。 */
	if ( !exampleWaitValue(&pFuture, xrtFileAppendAsync(pPool,
			"xrt-async-tour.tmp",
			(xbytesview) { (cbytes)"tail", 4u }), NULL) ) {
		goto Cleanup;
	}
	/* WriteAllAsync：整文件覆写。 */
	if ( !exampleWaitValue(&pFuture, xrtFileWriteAllAsync(pPool,
			"xrt-async-whole.tmp",
			(xbytesview) { (cbytes)"whole-file", 10u }),
			NULL) ) {
		goto Cleanup;
	}
	/* ReadAllLimitAsync：读回并限长。 */
	if ( !exampleWaitValue(&pFuture, xrtFileReadAllLimitAsync(pPool,
			"xrt-async-whole.tmp", 64u), &pValue) ||
		((pRead = (xfiledata*)pValue) == NULL) ||
		(pRead->Size != 10u) ||
		(memcmp(pRead->Data, "whole-file", 10u) != 0) ) {
		goto Cleanup;
	}
	/* FileMoveAsync：改名。 */
	if ( !exampleWaitValue(&pFuture, xrtFileMoveAsync(pPool,
			"xrt-async-whole.tmp",
			"xrt-async-moved.tmp", true), NULL) ) {
		goto Cleanup;
	}
	printf("whole: read-limit/write-all/move ok\n");
	printf("async-file: size=22 resize=16 append=4\n");

	/* ---- 目录族：权限/递归建链/统计/空判定 ---- */
	if ( !exampleWaitValue(&pFuture, xrtDirCreateModeAsync(pPool,
			"xrt-async-dir", 0755), NULL) ||
		!exampleWaitValue(&pFuture, xrtDirCreateAllAsync(pPool,
			"xrt-async-deep/a/b"), NULL) ||
		!exampleWaitValue(&pFuture, xrtDirCreateAllModeAsync(pPool,
			"xrt-async-deep2/x/y", 0755), NULL) ||
		!exampleWaitValue(&pFuture, xrtFileWriteAllAsync(pPool,
			"xrt-async-dir/f.bin",
			(xbytesview) { (cbytes)"123456", 6u }), NULL) ) {
		goto Cleanup;
	}
	/* DirStatsAsync 按公开契约发布 xwalkstats（Items/Files/Bytes）。 */
	if ( !exampleWaitValue(&pFuture, xrtDirStatsAsync(pPool,
			"xrt-async-dir", true), &pValue) ||
		((pWalk = (xwalkstats*)pValue) == NULL) ) {
		goto Cleanup;
	}
	{
		if ( (pWalk->Files != 1u) ||
			(pWalk->Bytes != 6u) ) {
			goto Cleanup;
		}
	}
	if ( !exampleWaitValue(&pFuture, xrtDirSizeAsync(pPool,
			"xrt-async-dir", true), &pValue) ||
		((pSize = (xfilesize*)pValue) == NULL) ||
		(pSize->Size != 6u) ||
		!exampleWaitValue(&pFuture, xrtDirEmptyAsync(pPool,
			"xrt-async-deep/a/b"), NULL) ) {
		goto Cleanup;
	}
	printf("dir: create-mode/all stats files=1 size=6\n");

	/* ---- EnsureEmpty / Move / Clean ---- */
	if ( !exampleWaitValue(&pFuture, xrtDirEnsureEmptyAsync(pPool,
			"xrt-async-dir"), NULL) ||
		!exampleWaitValue(&pFuture, xrtDirStatsAsync(pPool,
			"xrt-async-dir", true), &pValue) ||
		((pWalk = (xwalkstats*)pValue) == NULL) ) {
		goto Cleanup;
	}
	{
		/* Items 含根目录本身（清空后 = 1）；文件数归零即空。 */
		if ( pWalk->Files != 0u ) {
			goto Cleanup;
		}
	}
	if ( !exampleWaitValue(&pFuture, xrtDirMoveAsync(pPool,
			"xrt-async-deep", "xrt-async-deep-moved", true),
		NULL) ) {
		goto Cleanup;
	}
	if ( !exampleWaitValue(&pFuture, xrtDirCleanAsync(pPool,
			"xrt-async-deep-moved"), NULL) ) {
		goto Cleanup;
	}
	printf("dir: ensure-empty/move/clean ok\n");

	/* ---- 目录树：复制与整树删除 ---- */
	if ( !exampleWaitValue(&pFuture, xrtFileTreeCopyAsync(pPool,
			"xrt-async-deep2", "xrt-async-tree-copy", NULL),
		NULL) ) {
		goto Cleanup;
	}
	if ( !exampleWaitValue(&pFuture, xrtFileTreeRemoveAsync(pPool,
			"xrt-async-tree-copy", true), NULL) ||
		!exampleWaitValue(&pFuture, xrtFileTreeRemoveAsync(pPool,
			"xrt-async-deep2", true), NULL) ) {
		goto Cleanup;
	}
	printf("tree: copy+remove ok\n");
	iResult = 0;

Cleanup:
	xrtFree(pTake);
	if ( pFile != NULL ) {
		(void)exampleWaitValue(&pFuture, xrtAsyncFileClose(pFile), NULL);
	}
	xrtFutureDestroy(pFuture);
	(void)xrtClose(File);
	if ( pPool != NULL ) {
		(void)xrtTaskPoolDestroy(pPool);
	}
	(void)xrtFileDelete("xrt-async-tour.tmp");
	(void)xrtFileDelete("xrt-async-whole.tmp");
	(void)xrtFileDelete("xrt-async-moved.tmp");
	(void)xrtDirRemoveAll("xrt-async-dir");
	(void)xrtDirRemoveAll("xrt-async-deep");
	(void)xrtDirRemoveAll("xrt-async-deep-moved");
	(void)xrtDirRemoveAll("xrt-async-deep2");
	(void)xrtDirRemoveAll("xrt-async-tree-copy");
	return iResult;
}
