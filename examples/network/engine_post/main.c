/*
 * XRT Example - Network Engine Post
 * XRT 范例 - 网络引擎任务投递
 *
 * Description / 说明:
 *   EN: Demonstrates immediate post, delayed post, and post-future task
 *       bridging on an XNet engine worker.
 *   CN: 演示在 XNet 引擎 worker 上进行即时投递、延迟投递，以及带 future
 *       返回值的任务桥接。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/network_engine_post.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/network_engine_post -lm -lpthread
 */

#include "../../../xrt.c"
#include "../../example_wait.h"
#include <stdio.h>
#include <string.h>

typedef struct {
	volatile long iPostCount;
	volatile long iDelayedCount;
	volatile long iFutureCount;
	uint32 iWorkerId;
	int iFutureValue;
} ex_engine_post_ctx;


static void procPosted(xnetworker* pWorker, ptr pArg)
{
	ex_engine_post_ctx* pCtx = (ex_engine_post_ctx*)pArg;

	if ( pCtx == NULL ) {
		return;
	}

	++pCtx->iPostCount;
	pCtx->iWorkerId = pWorker ? pWorker->iId : 0u;
}


static void procDelayed(xnetworker* pWorker, ptr pArg)
{
	ex_engine_post_ctx* pCtx = (ex_engine_post_ctx*)pArg;

	if ( pCtx == NULL ) {
		return;
	}

	(void)pWorker;
	++pCtx->iDelayedCount;
}


static xnet_result procFutureTask(xnetworker* pWorker, ptr pArg, ptr* ppValue)
{
	ex_engine_post_ctx* pCtx = (ex_engine_post_ctx*)pArg;

	if ( pCtx == NULL || ppValue == NULL ) {
		return XRT_NET_ERROR;
	}

	++pCtx->iFutureCount;
	pCtx->iFutureValue = 100 + (int)(pWorker ? pWorker->iId : 0u);
	*ppValue = &pCtx->iFutureValue;
	return XRT_NET_OK;
}


int main(void)
{
	ex_engine_post_ctx tCtx;
	xnetengineconfig tCfg;
	xnetengine* pEngine = NULL;
	xnetfuture* pFuture = NULL;
	xnetwaitsrc tWaitSrc;
	ptr pValue = NULL;
	xnet_result iFutureStatus = XRT_NET_ERROR;
	int iRet = 0;

	memset(&tCtx, 0, sizeof(tCtx));
	memset(&tCfg, 0, sizeof(tCfg));
	xrtInit();

	xrtNetEngineConfigInit(&tCfg);
	tCfg.iWorkerCount = 1u;

	pEngine = xrtNetEngineCreate(&tCfg);
	if ( pEngine == NULL || xrtNetEngineStart(pEngine) != XRT_NET_OK ) {
		printf("engine = failed\n");
		iRet = 1;
		goto cleanup;
	}

	if ( xrtNetEnginePost(pEngine, 0u, procPosted, &tCtx) != XRT_NET_OK ) {
		printf("post = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( xrtNetEnginePostDelayed(pEngine, 0u, 30u, procDelayed, &tCtx) != XRT_NET_OK ) {
		printf("post_delayed = failed\n");
		iRet = 1;
		goto cleanup;
	}

	pFuture = xrtNetEnginePostFuture(pEngine, 0u, procFutureTask, &tCtx);
	if ( pFuture == NULL ) {
		printf("post_future = failed\n");
		iRet = 1;
		goto cleanup;
	}

	if ( !exWaitLongMin(&tCtx.iPostCount, 1, 1000u) || !exWaitLongMin(&tCtx.iDelayedCount, 1, 1000u) ) {
		printf("post_wait = timeout\n");
		iRet = 1;
		goto cleanup;
	}

	tWaitSrc = xrtNetWaitSourceFuture(pFuture);
	iFutureStatus = xrtNetWaitSourceWaitValueTimeout(&tWaitSrc, 1000u, &pValue);
	if ( iFutureStatus != XRT_NET_OK || pValue == NULL ) {
		printf("future_wait = failed\n");
		iRet = 1;
		goto cleanup;
	}

	printf("post_count = %ld\n", tCtx.iPostCount);
	printf("delayed_count = %ld\n", tCtx.iDelayedCount);
	printf("future_count = %ld\n", tCtx.iFutureCount);
	printf("worker_id = %u\n", (unsigned)tCtx.iWorkerId);
	printf("future_status = %d\n", (int)iFutureStatus);
	printf("future_value = %d\n", *(int*)pValue);

	if ( tCtx.iFutureCount != 1 ) {
		iRet = 1;
	}

cleanup:
	if ( pFuture != NULL ) {
		xrtNetFutureDestroy(pFuture);
	}
	if ( pEngine != NULL ) {
		xrtNetEngineStop(pEngine);
		xrtNetEngineDestroy(pEngine);
	}
	xrtUnit();
	return iRet;
}
