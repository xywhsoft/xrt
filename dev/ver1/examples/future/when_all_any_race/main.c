/*
 * XRT Example - Future When All Any Race
 * XRT 范例 - Future WhenAll / WhenAny / Race
 *
 * Description / 说明:
 *   EN: Demonstrates grouped future completion, winner source index, and all-value access.
 *   CN: 演示 Future 聚合完成、胜出源索引和全部值读取。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/future_when_all_any_race.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/future_when_all_any_race -lm -lpthread
 */

#include "../../../xrt.c"
#include <stdio.h>


typedef struct {
	xpromise* pPromise;
	int* pValue;
	uint32 iDelayMs;
} resolve_ctx;


static int32 ResolveAfterDelay(ptr pArg, xfuture_result* pOut)
{
	resolve_ctx* pCtx = (resolve_ctx*)pArg;

	if ( pCtx == NULL || pCtx->pPromise == NULL || pOut == NULL ) {
		return XRT_NET_ERROR;
	}

	xrtSleep(pCtx->iDelayMs);
	(void)xPromiseResolve(pCtx->pPromise, pCtx->pValue);
	pOut->pValue = pCtx->pValue;
	return XRT_NET_OK;
}


int main(void)
{
	xfuture* pA = NULL;
	xfuture* pB = NULL;
	xpromise* pPromiseA = NULL;
	xpromise* pPromiseB = NULL;
	xfuture* pAny = NULL;
	xfuture* pRace = NULL;
	xfuture* pAll = NULL;
	xfuture* pTaskA = NULL;
	xfuture* pTaskB = NULL;
	xfuture* arrSource[2];
	resolve_ctx tCtxA;
	resolve_ctx tCtxB;
	const xfuture_all_value* pAllValue = NULL;
	int iValueA = 10;
	int iValueB = 20;
	int iRet = 0;

	xrtInit();

	memset(arrSource, 0, sizeof(arrSource));
	memset(&tCtxA, 0, sizeof(tCtxA));
	memset(&tCtxB, 0, sizeof(tCtxB));

	pA = xFutureCreate();
	pB = xFutureCreate();
	pPromiseA = xPromiseCreate(pA);
	pPromiseB = xPromiseCreate(pB);
	arrSource[0] = pA;
	arrSource[1] = pB;

	pAny = xFutureWhenAny(arrSource, 2);
	pRace = xFutureRace(arrSource, 2);
	pAll = xFutureWhenAll(arrSource, 2);
	if ( pA == NULL || pB == NULL || pPromiseA == NULL || pPromiseB == NULL || pAny == NULL || pRace == NULL || pAll == NULL ) {
		printf("group_create = failed\n");
		iRet = 1;
		goto cleanup;
	}

	tCtxA.pPromise = pPromiseA;
	tCtxA.pValue = &iValueA;
	tCtxA.iDelayMs = 120u;
	tCtxB.pPromise = pPromiseB;
	tCtxB.pValue = &iValueB;
	tCtxB.iDelayMs = 40u;

	pTaskA = xTaskRunThread(ResolveAfterDelay, &tCtxA, 0u);
	pTaskB = xTaskRunThread(ResolveAfterDelay, &tCtxB, 0u);
	if ( pTaskA == NULL || pTaskB == NULL ) {
		printf("resolver_task = failed\n");
		iRet = 1;
		goto cleanup;
	}

	if ( !xFutureWaitTimeout(pAny, 2000) || !xFutureWaitTimeout(pRace, 2000) || !xFutureWaitTimeout(pAll, 2000) ) {
		printf("group_wait = timeout\n");
		iRet = 1;
		goto cleanup;
	}

	pAllValue = xFuturePeekAllValue(pAll);

	printf("any_status = %d\n", (int)xFutureStatus(pAny));
	printf("any_source_index = %d\n", xFutureGetGroupSourceIndex(pAny));
	printf("any_value = %d\n", *(int*)xFutureValue(pAny));
	printf("race_status = %d\n", (int)xFutureStatus(pRace));
	printf("race_source_index = %d\n", xFutureGetGroupSourceIndex(pRace));
	printf("race_value = %d\n", *(int*)xFutureValue(pRace));
	printf("all_status = %d\n", (int)xFutureStatus(pAll));
	printf("all_count = %d\n", xFutureGetAllValueCount(pAll));
	if ( pAllValue != NULL && pAllValue->iCount == 2 ) {
		printf("all_value_0 = %d\n", *(int*)pAllValue->arrValue[0]);
		printf("all_value_1 = %d\n", *(int*)pAllValue->arrValue[1]);
	}

cleanup:
	if ( pTaskB != NULL ) {
		xFutureWaitTimeout(pTaskB, 2000);
		xFutureRelease(pTaskB);
	}
	if ( pTaskA != NULL ) {
		xFutureWaitTimeout(pTaskA, 2000);
		xFutureRelease(pTaskA);
	}
	if ( pAll != NULL ) {
		xFutureRelease(pAll);
	}
	if ( pRace != NULL ) {
		xFutureRelease(pRace);
	}
	if ( pAny != NULL ) {
		xFutureRelease(pAny);
	}
	if ( pPromiseB != NULL ) {
		xPromiseDestroy(pPromiseB);
	}
	if ( pPromiseA != NULL ) {
		xPromiseDestroy(pPromiseA);
	}
	if ( pB != NULL ) {
		xFutureRelease(pB);
	}
	if ( pA != NULL ) {
		xFutureRelease(pA);
	}
	xrtUnit();
	return iRet;
}
