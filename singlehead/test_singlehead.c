// 可通过定义对 xrt 库进行裁剪
#define XRT_NO_FILE
#define XRT_NO_REGEX
#define XRT_NO_COROUTINE
#define XRT_NO_NETWORK
#define XRT_NO_CRYPTO
#define XRT_NO_NETSOCK
#define XRT_NO_NETPOLL
#define XRT_NO_NETTLS
#define XRT_NO_NETLOOP
#define XRT_NO_NETTCP
#define XRT_NO_NETUDP
#define XRT_NO_NETPROXY
#define XRT_NO_XID
#define XRT_NO_BUFFER
#define XRT_NO_NETHTTP
#define XRT_NO_NETWS
#define XRT_NO_NETHTTPD
#define XRT_NO_STACK
#define XRT_NO_JSON
#define XRT_NO_TEMPLATE

// 定义 XRT_IMPLEMENTATION 导入功能实现
#define XRT_IMPLEMENTATION
#include "xrt.h"

typedef struct {
	xvalue Table;
	xvalue Tags;
	volatile int FailCount;
} singlehead_shared_ctx;

typedef struct {
	uint32 Count;
	uintptr_t Values[8];
} singlehead_queue_drain_ctx;

static void singlehead_print_result(const char* sName, bool bPassed)
{
	printf("  [%s] %s\n", bPassed ? "OK" : "FAIL", sName);
}

static bool singlehead_has_shared_value_error(void)
{
	str sError = xrtGetError();
	return sError && strstr((const char*)sError, "real shared") != NULL;
}

static void singlehead_queue_drain_proc(ptr pItem, ptr pUserData)
{
	singlehead_queue_drain_ctx* pCtx = (singlehead_queue_drain_ctx*)pUserData;

	if ( pCtx == NULL || pCtx->Count >= 8 ) {
		return;
	}

	pCtx->Values[pCtx->Count] = (uintptr_t)pItem;
	pCtx->Count++;
}

static bool singlehead_queue_smoke(void)
{
	xqueue_config tCfg;
	xspscq hSPSC = NULL;
	xmpscq hMPSC = NULL;
	xmpmcq hMPMC = NULL;
	singlehead_queue_drain_ctx tDrain;
	ptr pItem = NULL;
	bool bOk = TRUE;

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 3;

	hSPSC = xrtSPSCQCreate(&tCfg);
	if ( hSPSC == NULL || hSPSC->iCapacity != 4u ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtSPSCQTryPush(hSPSC, (ptr)(uintptr_t)1u) != XQUEUE_OK ||
		xrtSPSCQTryPush(hSPSC, (ptr)(uintptr_t)2u) != XQUEUE_OK ||
		xrtSPSCQTryPush(hSPSC, (ptr)(uintptr_t)3u) != XQUEUE_OK ||
		xrtSPSCQTryPush(hSPSC, (ptr)(uintptr_t)4u) != XQUEUE_OK ||
		xrtSPSCQTryPush(hSPSC, (ptr)(uintptr_t)5u) != XQUEUE_FULL) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtSPSCQTryPop(hSPSC, &pItem) != XQUEUE_OK || (uintptr_t)pItem != 1u ) {
		bOk = FALSE;
		goto cleanup;
	}
	xrtSPSCQClose(hSPSC);
	memset(&tDrain, 0, sizeof(tDrain));
	if ( xrtSPSCQDrain(hSPSC, singlehead_queue_drain_proc, &tDrain) != 3u ||
		tDrain.Count != 3u ||
		tDrain.Values[0] != 2u ||
		tDrain.Values[1] != 3u ||
		tDrain.Values[2] != 4u ||
		!xrtQueueIsDrained(&hSPSC->tBase) ||
		!xrtSPSCQReset(hSPSC) ) {
		bOk = FALSE;
		goto cleanup;
	}

	hMPSC = xrtMPSCQCreate(&tCfg);
	if ( hMPSC == NULL || hMPSC->iCapacity != 4u ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtMPSCQTryPush(hMPSC, (ptr)(uintptr_t)11u) != XQUEUE_OK ||
		xrtMPSCQTryPush(hMPSC, (ptr)(uintptr_t)22u) != XQUEUE_OK ||
		xrtMPSCQTryPush(hMPSC, (ptr)(uintptr_t)33u) != XQUEUE_OK) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtMPSCQTryPop(hMPSC, &pItem) != XQUEUE_OK || (uintptr_t)pItem != 11u ) {
		bOk = FALSE;
		goto cleanup;
	}
	xrtMPSCQClose(hMPSC);
	memset(&tDrain, 0, sizeof(tDrain));
	if ( xrtMPSCQDrain(hMPSC, singlehead_queue_drain_proc, &tDrain) != 2u ||
		tDrain.Count != 2u ||
		tDrain.Values[0] != 22u ||
		tDrain.Values[1] != 33u ||
		!xrtQueueIsDrained(&hMPSC->tBase) ||
		!xrtMPSCQReset(hMPSC) ) {
		bOk = FALSE;
		goto cleanup;
	}

	hMPMC = xrtMPMCQCreate(&tCfg);
	if ( hMPMC == NULL || hMPMC->iCapacity != 4u ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtMPMCQTryPush(hMPMC, (ptr)(uintptr_t)111u) != XQUEUE_OK ||
		xrtMPMCQTryPush(hMPMC, (ptr)(uintptr_t)222u) != XQUEUE_OK ||
		xrtMPMCQTryPush(hMPMC, (ptr)(uintptr_t)333u) != XQUEUE_OK) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtMPMCQTryPop(hMPMC, &pItem) != XQUEUE_OK || (uintptr_t)pItem != 111u ) {
		bOk = FALSE;
		goto cleanup;
	}
	xrtMPMCQClose(hMPMC);
	memset(&tDrain, 0, sizeof(tDrain));
	if ( xrtMPMCQDrain(hMPMC, singlehead_queue_drain_proc, &tDrain) != 2u ||
		tDrain.Count != 2u ||
		tDrain.Values[0] != 222u ||
		tDrain.Values[1] != 333u ||
		!xrtQueueIsDrained(&hMPMC->tBase) ||
		!xrtMPMCQReset(hMPMC) ) {
		bOk = FALSE;
		goto cleanup;
	}

cleanup:
	if ( hSPSC ) {
		xrtSPSCQDestroy(hSPSC);
	}
	if ( hMPSC ) {
		xrtMPSCQDestroy(hMPSC);
	}
	if ( hMPMC ) {
		xrtMPMCQDestroy(hMPMC);
	}
	return bOk;
}

static bool singlehead_queue_failure_smoke(void)
{
	xqueue_config tCfg;
	xqueue_config tBadCfg;
	xspscq hSPSC = NULL;
	xmpscq hMPSC = NULL;
	xmpmcq hMPMC = NULL;
	singlehead_queue_drain_ctx tDrain;
	ptr pItem = NULL;
	bool bOk = TRUE;

	memset(&tCfg, 0, sizeof(tCfg));
	memset(&tBadCfg, 0, sizeof(tBadCfg));
	tCfg.iCapacity = 3;

	if ( xrtSPSCQCreate(NULL) != NULL ||
		xrtSPSCQCreate(&tBadCfg) != NULL ||
		xrtSPSCQInit(NULL, &tCfg) != FALSE ||
		xrtSPSCQTryPush(NULL, (ptr)(uintptr_t)1u) != XQUEUE_ERROR ||
		xrtSPSCQTryPop(NULL, &pItem) != XQUEUE_ERROR ||
		xrtSPSCQApproxCount(NULL) != 0u ||
		xrtSPSCQDrain(NULL, singlehead_queue_drain_proc, &tDrain) != 0u ||
		xrtSPSCQReset(NULL) != FALSE ) {
		return FALSE;
	}

	if ( xrtMPSCQCreate(NULL) != NULL ||
		xrtMPSCQCreate(&tBadCfg) != NULL ||
		xrtMPSCQInit(NULL, &tCfg) != FALSE ||
		xrtMPSCQTryPush(NULL, (ptr)(uintptr_t)1u) != XQUEUE_ERROR ||
		xrtMPSCQTryPop(NULL, &pItem) != XQUEUE_ERROR ||
		xrtMPSCQApproxCount(NULL) != 0u ||
		xrtMPSCQDrain(NULL, singlehead_queue_drain_proc, &tDrain) != 0u ||
		xrtMPSCQReset(NULL) != FALSE ) {
		return FALSE;
	}

	if ( xrtMPMCQCreate(NULL) != NULL ||
		xrtMPMCQCreate(&tBadCfg) != NULL ||
		xrtMPMCQInit(NULL, &tCfg) != FALSE ||
		xrtMPMCQTryPush(NULL, (ptr)(uintptr_t)1u) != XQUEUE_ERROR ||
		xrtMPMCQTryPop(NULL, &pItem) != XQUEUE_ERROR ||
		xrtMPMCQApproxCount(NULL) != 0u ||
		xrtMPMCQDrain(NULL, singlehead_queue_drain_proc, &tDrain) != 0u ||
		xrtMPMCQReset(NULL) != FALSE ) {
		return FALSE;
	}

	hSPSC = xrtSPSCQCreate(&tCfg);
	hMPSC = xrtMPSCQCreate(&tCfg);
	hMPMC = xrtMPMCQCreate(&tCfg);
	if ( hSPSC == NULL || hMPSC == NULL || hMPMC == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}

	memset(&tDrain, 0, sizeof(tDrain));
	if ( xrtSPSCQTryPush(hSPSC, (ptr)(uintptr_t)7u) != XQUEUE_OK ||
		xrtSPSCQTryPop(hSPSC, NULL) != XQUEUE_ERROR ||
		xrtSPSCQDrain(hSPSC, NULL, &tDrain) != 0u ||
		xrtSPSCQApproxCount(hSPSC) != 1u ) {
		bOk = FALSE;
		goto cleanup;
	}

	memset(&tDrain, 0, sizeof(tDrain));
	if ( xrtMPSCQTryPush(hMPSC, (ptr)(uintptr_t)8u) != XQUEUE_OK ||
		xrtMPSCQTryPop(hMPSC, NULL) != XQUEUE_ERROR ||
		xrtMPSCQDrain(hMPSC, NULL, &tDrain) != 0u ||
		xrtMPSCQApproxCount(hMPSC) != 1u ) {
		bOk = FALSE;
		goto cleanup;
	}

	memset(&tDrain, 0, sizeof(tDrain));
	if ( xrtMPMCQTryPush(hMPMC, (ptr)(uintptr_t)9u) != XQUEUE_OK ||
		xrtMPMCQTryPop(hMPMC, NULL) != XQUEUE_ERROR ||
		xrtMPMCQDrain(hMPMC, NULL, &tDrain) != 0u ||
		xrtMPMCQApproxCount(hMPMC) != 1u ) {
		bOk = FALSE;
		goto cleanup;
	}

cleanup:
	if ( hSPSC ) {
		xrtSPSCQDestroy(hSPSC);
	}
	if ( hMPSC ) {
		xrtMPSCQDestroy(hMPSC);
	}
	if ( hMPMC ) {
		xrtMPMCQDestroy(hMPMC);
	}

	return bOk;
}

static uint32 singlehead_worker(ptr pParam)
{
	singlehead_shared_ctx* pCtx = (singlehead_shared_ctx*)pParam;
	char* pTemp = NULL;
	xvalue pStoredTags = NULL;

	if ( !xrtThreadIsAttached() ) {
		pCtx->FailCount++;
		return 21;
	}

	pTemp = (char*)xrtTempMemory(32);
	if ( pTemp == NULL ) {
		pCtx->FailCount++;
		return 22;
	}
	memcpy(pTemp, "worker-ok", 10);

	xrtClearError();
	xvoAddRef(pCtx->Table);
	xvoUnref(pCtx->Table);
	xvoAddRef(pCtx->Tags);
	xvoUnref(pCtx->Tags);
	if ( xrtGetError() != NULL && xrtGetError() != xCore.sNull && xrtGetError()[0] != '\0' ) {
		pCtx->FailCount++;
		return 23;
	}

	if ( !xvoTableExists(pCtx->Table, (str)"tags", 4) ) {
		pCtx->FailCount++;
		return 24;
	}
	pStoredTags = xvoTableGetValue(pCtx->Table, (str)"tags", 4);
	if ( pStoredTags != pCtx->Tags ) {
		pCtx->FailCount++;
		return 25;
	}
	if ( xvoCollItemCount(pCtx->Tags) != 1 ) {
		pCtx->FailCount++;
		return 26;
	}

	return 20;
}

int main(void)
{
	bool bOk = TRUE;
	bool bQueueOk = TRUE;
	bool bQueueFailureOk = TRUE;
	char* pTemp = NULL;
	xvalue pTable = NULL;
	xvalue pTags = NULL;
	xvalue pLocalColl = NULL;
	xthread pThread = NULL;
	singlehead_shared_ctx tCtx;

	memset(&tCtx, 0, sizeof(tCtx));

	printf("========================================\n");
	printf("  XRT Single Header Runtime Smoke Test\n");
	printf("========================================\n\n");

	printf("Initializing XRT...\n");
	xrtInit();

	singlehead_print_result("bootstrap thread attached", xrtThreadIsAttached());
	if ( !xrtThreadIsAttached() ) {
		bOk = FALSE;
		goto cleanup;
	}

	pTemp = (char*)xrtTempMemory(64);
	if ( pTemp == NULL ) {
		singlehead_print_result("thread-local temp memory", FALSE);
		bOk = FALSE;
		goto cleanup;
	}
	memcpy(pTemp, "singlehead-main", 16);
	singlehead_print_result("thread-local temp memory", TRUE);

	xrtSetError((str)"singlehead-error", FALSE);
	singlehead_print_result("thread-local error state", strcmp((const char*)xrtGetError(), "singlehead-error") == 0);
	if ( strcmp((const char*)xrtGetError(), "singlehead-error") != 0 ) {
		bOk = FALSE;
		goto cleanup;
	}
	xrtClearError();

	bQueueOk = singlehead_queue_smoke();
	singlehead_print_result("queue smoke", bQueueOk);
	if ( !bQueueOk ) {
		bOk = FALSE;
		goto cleanup;
	}

	bQueueFailureOk = singlehead_queue_failure_smoke();
	singlehead_print_result("queue failure smoke", bQueueFailureOk);
	if ( !bQueueFailureOk ) {
		bOk = FALSE;
		goto cleanup;
	}

	pTable = xvoCreateTableEx(XRT_OBJMODE_SHARED);
	pTags = xvoCreateCollEx(XRT_OBJMODE_SHARED);
	singlehead_print_result("shared value roots", pTable != NULL && pTags != NULL);
	if ( pTable == NULL || pTags == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}

	xrtClearError();
	if ( !xvoCollSetText(pTags, "singlehead", 0, FALSE) ) {
		singlehead_print_result("shared coll write", FALSE);
		bOk = FALSE;
		goto cleanup;
	}
	singlehead_print_result("shared coll write", TRUE);

	xrtClearError();
	if ( !xvoTableSetValue(pTable, "tags", 4, pTags, FALSE) ) {
		singlehead_print_result("shared nested value store", FALSE);
		bOk = FALSE;
		goto cleanup;
	}
	singlehead_print_result("shared nested value store", TRUE);

	tCtx.Table = pTable;
	tCtx.Tags = pTags;
	pThread = xrtThreadCreate((ptr)singlehead_worker, &tCtx, 0);
	singlehead_print_result("auto-attached worker thread", pThread != NULL);
	if ( pThread == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	xrtThreadWait(pThread);
	singlehead_print_result("worker runtime access", xrtThreadGetExitCode(pThread) == 20 && tCtx.FailCount == 0);
	if ( xrtThreadGetExitCode(pThread) != 20 || tCtx.FailCount != 0 ) {
		bOk = FALSE;
		goto cleanup;
	}

	pLocalColl = xvoCreateColl();
	if ( pLocalColl == NULL ) {
		singlehead_print_result("local coll construction", FALSE);
		bOk = FALSE;
		goto cleanup;
	}
	if ( !xvoCollSetInt(pLocalColl, 1) ) {
		singlehead_print_result("local coll construction", FALSE);
		bOk = FALSE;
		goto cleanup;
	}
	singlehead_print_result("local coll construction", TRUE);

	xrtClearError();
	if ( xvoTableSetValue(pTable, "bad", 3, pLocalColl, FALSE) ) {
		singlehead_print_result("reject local nested container on shared root", FALSE);
		bOk = FALSE;
		goto cleanup;
	}
	singlehead_print_result("reject local nested container on shared root", singlehead_has_shared_value_error());
	if ( !singlehead_has_shared_value_error() ) {
		bOk = FALSE;
		goto cleanup;
	}

cleanup:
	if ( pThread ) {
		xrtThreadDestroy(pThread);
	}
	if ( pLocalColl ) {
		xvoUnref(pLocalColl);
	}
	if ( pTable ) {
		xvoUnref(pTable);
	}
	if ( pTags ) {
		xvoUnref(pTags);
	}
	xrtClearError();
	xrtUnit();

	printf("\n========================================\n");
	printf("  %s\n", bOk ? "All tests passed!" : "Test failed!");
	printf("========================================\n\n");

	return bOk ? 0 : 1;
}
