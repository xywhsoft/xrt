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

typedef struct {
	volatile uint32 Value;
	uint32 Guard;
} singlehead_atomic_width_probe;

#ifndef XRT_NO_QUEUE_WAIT
typedef struct {
	xmpscqwait hQueue;
	xsem hReady;
	uint32 TimeoutMs;
	xqueue_result Result;
	uintptr_t Value;
	volatile int Failure;
} singlehead_wait_consumer_ctx;
#endif

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
	ptr arrBatch[3];
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
		xrtMPSCQPushBatch(NULL, arrBatch, 1u) != 0u ||
		xrtMPSCQPushBatch((xmpscq)(uintptr_t)1u, NULL, 1u) != 0u ||
		xrtMPSCQPopBatch(NULL, arrBatch, 1u) != 0u ||
		xrtMPSCQPopBatch((xmpscq)(uintptr_t)1u, NULL, 1u) != 0u ||
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
		xrtMPMCQPushBatch(NULL, arrBatch, 1u) != 0u ||
		xrtMPMCQPushBatch((xmpmcq)(uintptr_t)1u, NULL, 1u) != 0u ||
		xrtMPMCQPopBatch(NULL, arrBatch, 1u) != 0u ||
		xrtMPMCQPopBatch((xmpmcq)(uintptr_t)1u, NULL, 1u) != 0u ||
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

static bool singlehead_queue_batch_smoke(void)
{
	xqueue_config tCfg;
	xmpscq hMPSC = NULL;
	xmpmcq hMPMC = NULL;
	ptr arrPush[3];
	ptr arrPop[4];
	uint32 iCount = 0;
	bool bOk = TRUE;

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 3;
	arrPush[0] = (ptr)(uintptr_t)30u;
	arrPush[1] = (ptr)(uintptr_t)40u;
	arrPush[2] = (ptr)(uintptr_t)50u;

	hMPSC = xrtMPSCQCreate(&tCfg);
	if ( hMPSC == NULL ) {
		return FALSE;
	}
	if ( xrtMPSCQTryPush(hMPSC, (ptr)(uintptr_t)10u) != XQUEUE_OK ||
		xrtMPSCQTryPush(hMPSC, (ptr)(uintptr_t)20u) != XQUEUE_OK ) {
		bOk = FALSE;
		goto cleanup;
	}
	iCount = xrtMPSCQPushBatch(hMPSC, arrPush, 3u);
	if ( iCount != 2u || xrtMPSCQApproxCount(hMPSC) != 4u ) {
		bOk = FALSE;
		goto cleanup;
	}
	arrPop[0] = (ptr)(uintptr_t)90u;
	arrPop[1] = (ptr)(uintptr_t)91u;
	arrPop[2] = (ptr)(uintptr_t)92u;
	arrPop[3] = (ptr)(uintptr_t)93u;
	iCount = xrtMPSCQPopBatch(hMPSC, arrPop, 3u);
	if ( iCount != 3u ||
		(uintptr_t)arrPop[0] != 10u ||
		(uintptr_t)arrPop[1] != 20u ||
		(uintptr_t)arrPop[2] != 30u ||
		(uintptr_t)arrPop[3] != 93u ||
		xrtMPSCQApproxCount(hMPSC) != 1u ) {
		bOk = FALSE;
		goto cleanup;
	}
	xrtMPSCQClose(hMPSC);
	if ( xrtMPSCQPushBatch(hMPSC, arrPush, 2u) != 0u ||
		xrtMPSCQPopBatch(hMPSC, arrPop, 2u) != 1u ||
		(uintptr_t)arrPop[0] != 40u ||
		xrtMPSCQPopBatch(hMPSC, arrPop, 1u) != 0u ) {
		bOk = FALSE;
		goto cleanup;
	}

	hMPMC = xrtMPMCQCreate(&tCfg);
	if ( hMPMC == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	if ( xrtMPMCQTryPush(hMPMC, (ptr)(uintptr_t)110u) != XQUEUE_OK ||
		xrtMPMCQTryPush(hMPMC, (ptr)(uintptr_t)120u) != XQUEUE_OK ) {
		bOk = FALSE;
		goto cleanup;
	}
	iCount = xrtMPMCQPushBatch(hMPMC, arrPush, 3u);
	if ( iCount != 2u || xrtMPMCQApproxCount(hMPMC) != 4u ) {
		bOk = FALSE;
		goto cleanup;
	}
	arrPop[0] = (ptr)(uintptr_t)190u;
	arrPop[1] = (ptr)(uintptr_t)191u;
	arrPop[2] = (ptr)(uintptr_t)192u;
	arrPop[3] = (ptr)(uintptr_t)193u;
	iCount = xrtMPMCQPopBatch(hMPMC, arrPop, 3u);
	if ( iCount != 3u ||
		(uintptr_t)arrPop[0] != 110u ||
		(uintptr_t)arrPop[1] != 120u ||
		(uintptr_t)arrPop[2] != 30u ||
		(uintptr_t)arrPop[3] != 193u ||
		xrtMPMCQApproxCount(hMPMC) != 1u ) {
		bOk = FALSE;
		goto cleanup;
	}
	xrtMPMCQClose(hMPMC);
	if ( xrtMPMCQPushBatch(hMPMC, arrPush, 2u) != 0u ||
		xrtMPMCQPopBatch(hMPMC, arrPop, 2u) != 1u ||
		(uintptr_t)arrPop[0] != 40u ||
		xrtMPMCQPopBatch(hMPMC, arrPop, 1u) != 0u ) {
		bOk = FALSE;
		goto cleanup;
	}

cleanup:
	if ( hMPSC ) {
		xrtMPSCQDestroy(hMPSC);
	}
	if ( hMPMC ) {
		xrtMPMCQDestroy(hMPMC);
	}
	return bOk;
}

#ifndef XRT_NO_QUEUE_WAIT
static uint32 singlehead_wait_consumer(ptr pArg)
{
	singlehead_wait_consumer_ctx* pCtx = (singlehead_wait_consumer_ctx*)pArg;
	ptr pItem = NULL;

	if ( pCtx == NULL || pCtx->hQueue == NULL ) {
		return 40;
	}

	if ( pCtx->hReady != NULL ) {
		(void)xrtSemPost(pCtx->hReady);
	}

	pCtx->Result = xrtMPSCQWaitPopTimeout(pCtx->hQueue, &pItem, pCtx->TimeoutMs);
	pCtx->Value = (uintptr_t)pItem;
	if ( pCtx->Result == XQUEUE_ERROR ) {
		pCtx->Failure = 1;
		return 41;
	}
	return 0;
}

static bool singlehead_queue_wait_smoke(void)
{
	xqueue_config tCfg;
	xmpscqwait hQueue = NULL;
	xthread hThread = NULL;
	xsem hReady = NULL;
	singlehead_wait_consumer_ctx tCtx;
	ptr pItem = NULL;
	bool bOk = TRUE;

	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = 3;

	hQueue = xrtMPSCQWaitCreate(&tCfg);
	if ( hQueue == NULL ) {
		return FALSE;
	}

	if ( xrtMPSCQWaitTryPop(hQueue, &pItem) != XQUEUE_EMPTY ||
		xrtMPSCQWaitPopTimeout(hQueue, &pItem, 0u) != XQUEUE_TIMEOUT ||
		xrtMPSCQWaitTryPush(hQueue, (ptr)(uintptr_t)10u) != XQUEUE_OK ||
		xrtMPSCQWaitApproxCount(hQueue) != 1u ||
		xrtMPSCQWaitTryPop(hQueue, &pItem) != XQUEUE_OK ||
		(uintptr_t)pItem != 10u ) {
		bOk = FALSE;
		goto cleanup;
	}

	hReady = xrtSemCreate(0u, 1u);
	if ( hReady == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}

	memset(&tCtx, 0, sizeof(tCtx));
	tCtx.hQueue = hQueue;
	tCtx.hReady = hReady;
	tCtx.TimeoutMs = 1000u;
	hThread = xrtThreadCreate((ptr)singlehead_wait_consumer, &tCtx, 0);
	if ( hThread == NULL ||
		xrtSemWaitTimeout(hReady, 1000u) != XRT_WAIT_OK ||
		xrtMPSCQWaitTryPush(hQueue, (ptr)(uintptr_t)20u) != XQUEUE_OK ||
		xrtThreadWaitTimeout(hThread, 5000u) != XRT_WAIT_OK ||
		xrtThreadGetExitCode(hThread) != 0u ||
		tCtx.Failure != 0 ||
		tCtx.Result != XQUEUE_OK ||
		tCtx.Value != 20u ) {
		bOk = FALSE;
		goto cleanup;
	}
	xrtThreadDestroy(hThread);
	hThread = NULL;
	xrtSemDestroy(hReady);
	hReady = NULL;

	hReady = xrtSemCreate(0u, 1u);
	if ( hReady == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}

	memset(&tCtx, 0, sizeof(tCtx));
	tCtx.hQueue = hQueue;
	tCtx.hReady = hReady;
	tCtx.TimeoutMs = 1000u;
	hThread = xrtThreadCreate((ptr)singlehead_wait_consumer, &tCtx, 0);
	if ( hThread == NULL ||
		xrtSemWaitTimeout(hReady, 1000u) != XRT_WAIT_OK ) {
		bOk = FALSE;
		goto cleanup;
	}

	xrtMPSCQWaitClose(hQueue);
	if ( xrtThreadWaitTimeout(hThread, 5000u) != XRT_WAIT_OK ||
		xrtThreadGetExitCode(hThread) != 0u ||
		tCtx.Failure != 0 ||
		tCtx.Result != XQUEUE_CLOSED ||
		xrtMPSCQWaitTryPush(hQueue, (ptr)(uintptr_t)30u) != XQUEUE_CLOSED ) {
		bOk = FALSE;
		goto cleanup;
	}

cleanup:
	if ( hThread ) {
		xrtThreadWaitTimeout(hThread, 5000u);
		xrtThreadDestroy(hThread);
	}
	if ( hReady ) {
		xrtSemDestroy(hReady);
	}
	if ( hQueue ) {
		xrtMPSCQWaitDestroy(hQueue);
	}
	return bOk;
}
#endif

static bool singlehead_internal_atomic_width_smoke(void)
{
	singlehead_atomic_width_probe tProbe;

	tProbe.Value = 1u;
	tProbe.Guard = 0x1a2b3c4du;
	__xrtAtomicStoreU32(&tProbe.Value, 7u);
	if ( __xrtAtomicLoadU32(&tProbe.Value) != 7u ||
		tProbe.Value != 7u ||
		tProbe.Guard != 0x1a2b3c4du ) {
		return FALSE;
	}

	tProbe.Value = 1u;
	tProbe.Guard = 0x1a2b3c4du;
	return __xvoAtomicCompareExchange32(&tProbe.Value, 2u, 1u) == 1u &&
		tProbe.Value == 2u &&
		tProbe.Guard == 0x1a2b3c4du;
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
	bool bQueueBatchOk = TRUE;
	bool bQueueFailureOk = TRUE;
	bool bQueueWaitOk = TRUE;
	bool bAtomicWidthOk = TRUE;
	char* pTemp = NULL;
	xvalue pTable = NULL;
	xvalue pTags = NULL;
	xvalue pLocalColl = NULL;
	xvalue pPublishedColl = NULL;
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

	bQueueBatchOk = singlehead_queue_batch_smoke();
	singlehead_print_result("queue batch smoke", bQueueBatchOk);
	if ( !bQueueBatchOk ) {
		bOk = FALSE;
		goto cleanup;
	}

	#ifndef XRT_NO_QUEUE_WAIT
		bQueueWaitOk = singlehead_queue_wait_smoke();
		singlehead_print_result("queue wait smoke", bQueueWaitOk);
		if ( !bQueueWaitOk ) {
			bOk = FALSE;
			goto cleanup;
		}
	#endif

	bAtomicWidthOk = singlehead_internal_atomic_width_smoke();
	singlehead_print_result("internal atomic width guard", bAtomicWidthOk);
	if ( !bAtomicWidthOk ) {
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
	if ( !xvoTableSetValue(pTable, "published", 9, pLocalColl, FALSE) ) {
		singlehead_print_result("publish local nested container on shared root", FALSE);
		bOk = FALSE;
		goto cleanup;
	}
	pPublishedColl = xvoTableGetValue(pTable, "published", 9);
	singlehead_print_result(
		"publish local nested container on shared root",
		pPublishedColl != NULL &&
		pPublishedColl != pLocalColl &&
		xvoIsShared_Inline(pPublishedColl) &&
		xvoCollItemCount(pPublishedColl) == 1 &&
		!singlehead_has_shared_value_error());
	if ( pPublishedColl == NULL ||
		 pPublishedColl == pLocalColl ||
		 !xvoIsShared_Inline(pPublishedColl) ||
		 xvoCollItemCount(pPublishedColl) != 1 ||
		 singlehead_has_shared_value_error() ) {
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
