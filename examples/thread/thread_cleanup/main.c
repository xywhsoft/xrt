/*
 * XRT Example - Thread Cleanup
 * XRT 范例 - 线程清理栈
 *
 * Description / 说明:
 *   EN: Demonstrates pushing thread cleanup callbacks, popping the top cleanup
 *       on the normal path, and letting the remaining cleanup run automatically
 *       when the thread exits.
 *   CN: 演示压入线程 cleanup 回调、在正常路径弹出顶部 cleanup，
 *       并让剩余 cleanup 在线程退出时自动执行。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/thread_thread_cleanup.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/thread_thread_cleanup -lm -lpthread
 */

#include "../../../xrt.c"
#include <stdio.h>
#include <string.h>

typedef struct {
	char* pText;
	volatile long* pCount;
	char* sResultText;
	size_t iResultCap;
} ex_thread_cleanup_item;

typedef struct {
	volatile long iAutoCleanupCount;
	volatile long iManualCleanupCount;
	char aAutoText[64];
	char aManualText[64];
	ex_thread_cleanup_item tAutoItem;
	ex_thread_cleanup_item tManualItem;
} ex_thread_cleanup_ctx;


static void procThreadCleanupFree(xrtThreadData* pThreadData, ptr pArg)
{
	ex_thread_cleanup_item* pItem = (ex_thread_cleanup_item*)pArg;
	(void)pThreadData;

	if ( pItem == NULL ) {
		return;
	}

	if ( pItem->sResultText != NULL && pItem->iResultCap > 0u && pItem->pText != NULL ) {
		snprintf(pItem->sResultText, pItem->iResultCap, "%s", pItem->pText);
	}

	if ( pItem->pText != NULL ) {
		xrtFree(pItem->pText);
		pItem->pText = NULL;
	}

	if ( pItem->pCount != NULL ) {
		++(*pItem->pCount);
	}
}


static uint32 procWorker(ptr pArg)
{
	ex_thread_cleanup_ctx* pCtx = (ex_thread_cleanup_ctx*)pArg;

	if ( pCtx == NULL ) {
		return 1u;
	}

	memset(&pCtx->tAutoItem, 0, sizeof(pCtx->tAutoItem));
	memset(&pCtx->tManualItem, 0, sizeof(pCtx->tManualItem));

	pCtx->tAutoItem.pText = xrtCopyStr("auto-cleanup", 0u);
	pCtx->tAutoItem.pCount = &pCtx->iAutoCleanupCount;
	pCtx->tAutoItem.sResultText = pCtx->aAutoText;
	pCtx->tAutoItem.iResultCap = sizeof(pCtx->aAutoText);

	pCtx->tManualItem.pText = xrtCopyStr("manual-cleanup", 0u);
	pCtx->tManualItem.pCount = &pCtx->iManualCleanupCount;
	pCtx->tManualItem.sResultText = pCtx->aManualText;
	pCtx->tManualItem.iResultCap = sizeof(pCtx->aManualText);

	if ( pCtx->tAutoItem.pText == NULL || pCtx->tManualItem.pText == NULL ) {
		procThreadCleanupFree(NULL, &pCtx->tManualItem);
		procThreadCleanupFree(NULL, &pCtx->tAutoItem);
		return 2u;
	}

	if ( !xrtThreadPushCleanup(procThreadCleanupFree, &pCtx->tAutoItem) ) {
		procThreadCleanupFree(NULL, &pCtx->tManualItem);
		procThreadCleanupFree(NULL, &pCtx->tAutoItem);
		return 3u;
	}
	if ( !xrtThreadPushCleanup(procThreadCleanupFree, &pCtx->tManualItem) ) {
		(void)xrtThreadPopCleanup(procThreadCleanupFree, &pCtx->tAutoItem);
		procThreadCleanupFree(NULL, &pCtx->tManualItem);
		procThreadCleanupFree(NULL, &pCtx->tAutoItem);
		return 4u;
	}

	if ( !xrtThreadPopCleanup(procThreadCleanupFree, &pCtx->tManualItem) ) {
		return 5u;
	}

	procThreadCleanupFree(xrtThreadGetCurrent(), &pCtx->tManualItem);
	return 0u;
}


int main(void)
{
	ex_thread_cleanup_ctx tCtx;
	xthread pThread = NULL;
	int iRet = 0;

	memset(&tCtx, 0, sizeof(tCtx));
	xrtInit();

	pThread = xrtThreadCreate(procWorker, &tCtx, 0u);
	if ( pThread == NULL ) {
		printf("thread = failed\n");
		iRet = 1;
		goto cleanup;
	}

	xrtThreadWait(pThread);

	printf("manual_cleanup_count = %ld\n", tCtx.iManualCleanupCount);
	printf("auto_cleanup_count = %ld\n", tCtx.iAutoCleanupCount);
	printf("manual_cleanup_text = %s\n", tCtx.aManualText);
	printf("auto_cleanup_text = %s\n", tCtx.aAutoText);
	printf("thread_exit_code = %u\n", (unsigned)xrtThreadGetExitCode(pThread));

	if ( tCtx.iManualCleanupCount != 1 || tCtx.iAutoCleanupCount != 1 ) {
		iRet = 1;
	}

cleanup:
	if ( pThread != NULL ) {
		xrtThreadDestroy(pThread);
	}
	xrtUnit();
	return iRet;
}
