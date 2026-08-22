/*
 * XRT Example - Coroutine Cleanup And Result
 * XRT 范例 - 协程 Cleanup 与结果
 *
 * Description / 说明:
 *   EN: Demonstrates coroutine cleanup stack handling, explicit cleanup pop,
 *       result storage, and user-data retrieval after the coroutine finishes.
 *   CN: 演示协程 cleanup 栈、显式弹出 cleanup、结果存储，以及协程结束后的
 *       user-data 读取。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/coroutine_cleanup_and_result.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/coroutine_cleanup_and_result -lm -lpthread
 */

#include "../../../xrt.c"
#include <stdio.h>
#include <string.h>

typedef struct ex_co_cleanup_item ex_co_cleanup_item;

typedef struct {
	volatile long iAutoCleanupCount;
	volatile long iManualCleanupCount;
	char aAutoText[64];
	char aManualText[64];
	char aResult[64];
	char aUserData[64];
	ex_co_cleanup_item* pAutoItem;
	ex_co_cleanup_item* pManualItem;
} ex_co_cleanup_ctx;

struct ex_co_cleanup_item {
	ex_co_cleanup_ctx* pCtx;
	char* pText;
	bool bAuto;
};


static void procCoCleanup(ptr pArg)
{
	ex_co_cleanup_item* pItem = (ex_co_cleanup_item*)pArg;

	if ( pItem == NULL || pItem->pCtx == NULL ) {
		return;
	}

	if ( pItem->bAuto ) {
		++pItem->pCtx->iAutoCleanupCount;
		if ( pItem->pText != NULL ) {
			snprintf(pItem->pCtx->aAutoText, sizeof(pItem->pCtx->aAutoText), "%s", pItem->pText);
		}
	}
	else {
		++pItem->pCtx->iManualCleanupCount;
		if ( pItem->pText != NULL ) {
			snprintf(pItem->pCtx->aManualText, sizeof(pItem->pCtx->aManualText), "%s", pItem->pText);
		}
	}

	if ( pItem->pText != NULL ) {
		xrtFree(pItem->pText);
		pItem->pText = NULL;
	}
}


static void procCleanupResult(ptr pArg)
{
	ex_co_cleanup_ctx* pCtx = (ex_co_cleanup_ctx*)pArg;
	xcoro pCurrent = NULL;

	if ( pCtx == NULL || pCtx->pAutoItem == NULL || pCtx->pManualItem == NULL ) {
		return;
	}

	pCurrent = xrtCoGetCurrent();
	pCtx->pAutoItem->pText = xrtCopyStr("auto-cleanup", 0u);
	pCtx->pManualItem->pText = xrtCopyStr("manual-cleanup", 0u);
	if ( pCtx->pAutoItem->pText == NULL || pCtx->pManualItem->pText == NULL ) {
		return;
	}

	(void)xrtCoPushCleanup(procCoCleanup, pCtx->pAutoItem);
	(void)xrtCoPushCleanup(procCoCleanup, pCtx->pManualItem);
	(void)xrtCoPopCleanup(procCoCleanup, pCtx->pManualItem, TRUE);

	snprintf(pCtx->aResult, sizeof(pCtx->aResult), "cleanup-result-ready");
	snprintf(pCtx->aUserData, sizeof(pCtx->aUserData), "user-data-ready");
	xrtCoSetUserData(pCurrent, pCtx->aUserData);
	xrtCoSetResult(pCtx->aResult);
}


int main(void)
{
	ex_co_cleanup_ctx tCtx;
	ex_co_cleanup_item tAutoItem;
	ex_co_cleanup_item tManualItem;
	xcosched* pSched = NULL;
	xcoro pWorker = NULL;
	const char* sResult = NULL;
	const char* sUserData = NULL;
	int iRet = 0;

	memset(&tCtx, 0, sizeof(tCtx));
	memset(&tAutoItem, 0, sizeof(tAutoItem));
	memset(&tManualItem, 0, sizeof(tManualItem));
	xrtInit();

	tAutoItem.pCtx = &tCtx;
	tAutoItem.bAuto = TRUE;
	tManualItem.pCtx = &tCtx;
	tManualItem.bAuto = FALSE;
	tCtx.pAutoItem = &tAutoItem;
	tCtx.pManualItem = &tManualItem;

	pSched = xrtCoSchedCreate();
	if ( pSched == NULL ) {
		printf("scheduler = failed\n");
		iRet = 1;
		goto cleanup;
	}

	pWorker = xrtCoSchedSpawn(pSched, procCleanupResult, &tCtx, 0u);
	if ( pWorker == NULL ) {
		printf("spawn = failed\n");
		iRet = 1;
		goto cleanup;
	}

	xrtCoSchedRun(pSched);

	sResult = (const char*)xrtCoGetResult(pWorker);
	sUserData = (const char*)xrtCoGetUserData(pWorker);

	printf("manual_cleanup_count = %ld\n", tCtx.iManualCleanupCount);
	printf("auto_cleanup_count = %ld\n", tCtx.iAutoCleanupCount);
	printf("manual_cleanup_text = %s\n", tCtx.aManualText);
	printf("auto_cleanup_text = %s\n", tCtx.aAutoText);
	printf("result = %s\n", sResult ? sResult : "(null)");
	printf("user_data = %s\n", sUserData ? sUserData : "(null)");

	if ( tCtx.iManualCleanupCount != 1 || tCtx.iAutoCleanupCount != 1 ) {
		iRet = 1;
	}

cleanup:
	if ( pWorker != NULL ) {
		(void)xrtCoClose(pWorker);
	}
	if ( pSched != NULL ) {
		xrtCoSchedDestroy(pSched);
	}
	xrtUnit();
	return iRet;
}
