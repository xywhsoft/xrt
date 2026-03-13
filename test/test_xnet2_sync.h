#include "../lib/xnet_sync.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#else
	#include <unistd.h>
#endif


typedef struct {
	xnetfuture* pFuture;
	xnet_result iResult;
	ptr pValue;
	volatile long iHitCount;
	volatile long iWorkerId;
} __test_xnet2_sync_task_ctx;


static void __Test_XNet2_SyncSleepMs(uint32 iDelayMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelayMs);
	#else
		usleep((useconds_t)iDelayMs * 1000u);
	#endif
}

static long __Test_XNet2_SyncAtomicInc(volatile long* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedIncrement((volatile LONG*)pValue);
	#else
		return __sync_add_and_fetch(pValue, 1);
	#endif
}

static void __Test_XNet2_SyncAtomicStore(volatile long* pValue, long iValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		InterlockedExchange((volatile LONG*)pValue, iValue);
	#else
		__sync_lock_test_and_set(pValue, iValue);
	#endif
}

static void __Test_XNet2_SyncResolveTask(xnetworker* pWorker, ptr pArg)
{
	__test_xnet2_sync_task_ctx* pCtx = (__test_xnet2_sync_task_ctx*)pArg;
	if ( !pCtx || !pCtx->pFuture ) return;
	__Test_XNet2_SyncAtomicInc(&pCtx->iHitCount);
	__Test_XNet2_SyncAtomicStore(&pCtx->iWorkerId, pWorker ? (long)pWorker->iId : -1);
	(void)__xnetFutureResolve(pCtx->pFuture, pCtx->iResult, pCtx->pValue);
}


void Test_XNet2_Sync(void)
{
	printf("\n\n\n------------------------------------\n\n XNet2 Sync Skeleton Test:\n\n");

	{
		xnetfuture* pFuture = xrtNetFutureCreate();
		int iValue = 1234;
		printf("  Future create : %s\n", pFuture != NULL ? "PASS" : "FAIL");
		printf("  Future pending status : %s\n",
			pFuture && xrtNetFutureStatus(pFuture) == XRT_NET_AGAIN ? "PASS" : "FAIL");
		printf("  Future wait timeout : %s\n",
			pFuture && xrtNetFutureWait(pFuture, 20) == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
		printf("  Future value pending : %s\n",
			pFuture && xrtNetFutureValue(pFuture) == NULL ? "PASS" : "FAIL");
		printf("  Future first resolve : %s\n",
			pFuture && __xnetFutureResolve(pFuture, XRT_NET_OK, &iValue) ? "PASS" : "FAIL");
		printf("  Future second resolve ignored : %s\n",
			pFuture && !__xnetFutureResolve(pFuture, XRT_NET_ERROR, NULL) ? "PASS" : "FAIL");
		printf("  Future wait after resolve : %s\n",
			pFuture && xrtNetFutureWait(pFuture, 0) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Future final status : %s\n",
			pFuture && xrtNetFutureStatus(pFuture) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Future value after resolve : %s\n",
			pFuture && xrtNetFutureValue(pFuture) == &iValue ? "PASS" : "FAIL");
		if ( pFuture ) {
			__xnetFutureReset(pFuture);
			printf("  Future reset to pending : %s\n",
				xrtNetFutureStatus(pFuture) == XRT_NET_AGAIN ? "PASS" : "FAIL");
			xrtNetFutureDestroy(pFuture);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine;
		xnetfuture* pFuture;
		__test_xnet2_sync_task_ctx tCtx;
		int iPayload = 5678;

		memset(&tCtx, 0, sizeof(tCtx));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;

		pEngine = xrtNetEngineCreate(&tCfg);
		pFuture = xrtNetFutureCreate();
		tCtx.pFuture = pFuture;
		tCtx.iResult = XRT_NET_OK;
		tCtx.pValue = &iPayload;
		printf("  Sync engine create : %s\n", pEngine != NULL ? "PASS" : "FAIL");
		printf("  Sync future create : %s\n", pFuture != NULL ? "PASS" : "FAIL");

		if ( pEngine && pFuture ) {
			printf("  Sync engine start : %s\n", xrtNetEngineStart(pEngine) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  Delayed resolve post : %s\n",
				xrtNetEnginePostDelayed(pEngine, 0, 50, __Test_XNet2_SyncResolveTask, &tCtx) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  Delayed future timeout first : %s\n",
				xrtNetFutureWait(pFuture, 5) == XRT_NET_TIMEOUT ? "PASS" : "FAIL");
			printf("  Delayed future still pending : %s\n",
				xrtNetFutureStatus(pFuture) == XRT_NET_AGAIN ? "PASS" : "FAIL");
			printf("  Delayed future eventual resolve : %s\n",
				xrtNetFutureWait(pFuture, 250) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  Delayed future value : %s\n",
				xrtNetFutureValue(pFuture) == &iPayload ? "PASS" : "FAIL");
			printf("  Delayed resolve hit count : %s\n", tCtx.iHitCount == 1 ? "PASS" : "FAIL");
			printf("  Delayed resolve worker id : %s\n", tCtx.iWorkerId == 0 ? "PASS" : "FAIL");
			xrtNetEngineStop(pEngine);
		}

		if ( pFuture ) xrtNetFutureDestroy(pFuture);
		if ( pEngine ) xrtNetEngineDestroy(pEngine);
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pCustom;
		xnetengine* pHidden1;
		xnetengine* pHidden2;
		xnetfuture* pFuture;
		__test_xnet2_sync_task_ctx tCtx;
		int iPayload = 9012;

		memset(&tCtx, 0, sizeof(tCtx));
		xrtNetSyncShutdownHiddenEngine();
		pHidden1 = xrtNetSyncGetHiddenEngine();
		pHidden2 = xrtNetSyncGetHiddenEngine();

		printf("  Hidden engine create : %s\n", pHidden1 != NULL ? "PASS" : "FAIL");
		printf("  Hidden engine running : %s\n", pHidden1 && pHidden1->bRunning ? "PASS" : "FAIL");
		printf("  Hidden engine singleton : %s\n", pHidden1 && pHidden1 == pHidden2 ? "PASS" : "FAIL");
		printf("  Hidden engine one worker policy : %s\n",
			pHidden1 && xrtNetEngineGetWorkerCount(pHidden1) == 1 ? "PASS" : "FAIL");
		printf("  Hidden engine resolver returns hidden : %s\n",
			__xnetSyncResolveEngine(NULL) == pHidden1 ? "PASS" : "FAIL");

		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 2;
		pCustom = xrtNetEngineCreate(&tCfg);
		if ( pCustom ) {
			(void)xrtNetEngineStart(pCustom);
		}
		printf("  Custom engine resolver preserves explicit engine : %s\n",
			pCustom && __xnetSyncResolveEngine(pCustom) == pCustom ? "PASS" : "FAIL");

		pFuture = xrtNetFutureCreate();
		tCtx.pFuture = pFuture;
		tCtx.iResult = XRT_NET_OK;
		tCtx.pValue = &iPayload;
		printf("  Hidden engine post resolve : %s\n",
			pHidden1 && pFuture &&
			xrtNetEnginePost(pHidden1, 0, __Test_XNet2_SyncResolveTask, &tCtx) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Hidden engine future wait : %s\n",
			pFuture && xrtNetFutureWait(pFuture, 250) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Hidden engine future value : %s\n",
			pFuture && xrtNetFutureValue(pFuture) == &iPayload ? "PASS" : "FAIL");

		if ( pFuture ) xrtNetFutureDestroy(pFuture);
		if ( pCustom ) {
			xrtNetEngineStop(pCustom);
			xrtNetEngineDestroy(pCustom);
		}
		xrtNetSyncShutdownHiddenEngine();
		__Test_XNet2_SyncSleepMs(10);
		printf("  Hidden engine shutdown and recreate : %s\n",
			xrtNetSyncGetHiddenEngine() != NULL ? "PASS" : "FAIL");
		xrtNetSyncShutdownHiddenEngine();
	}
}
