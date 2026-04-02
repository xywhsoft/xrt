/*
 * XRT Example - Coroutine Cancel And Join
 * XRT 范例 - 协程取消与 Join
 *
 * Description / 说明:
 *   EN: Demonstrates cooperative coroutine cancellation and joining from
 *       another coroutine in the same scheduler.
 *   CN: 演示在同一调度器中进行协作式协程取消，以及由另一个协程执行 join。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/coroutine_cancel_and_join.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/coroutine_cancel_and_join -lm -lpthread
 */

#include "../../../xrt.c"
#include <stdio.h>
#include <string.h>

typedef struct {
	xcoro pWorker;
	bool bJoinOk;
	volatile long iLoopCount;
	char aResult[64];
} ex_co_cancel_ctx;


static void procWorker(ptr pArg)
{
	ex_co_cancel_ctx* pCtx = (ex_co_cancel_ctx*)pArg;

	if ( pCtx == NULL ) {
		return;
	}

	while ( !xrtCoIsCancelRequested() ) {
		++pCtx->iLoopCount;
		xrtCoSleep(10u);
	}

	snprintf(pCtx->aResult, sizeof(pCtx->aResult), "cancel-requested");
	xrtCoSetResult(pCtx->aResult);
	xrtCoExit(77);
}


static void procCanceller(ptr pArg)
{
	ex_co_cancel_ctx* pCtx = (ex_co_cancel_ctx*)pArg;

	if ( pCtx == NULL || pCtx->pWorker == NULL ) {
		return;
	}

	xrtCoSleep(35u);
	(void)xrtCoCancel(pCtx->pWorker);
}


static void procJoiner(ptr pArg)
{
	ex_co_cancel_ctx* pCtx = (ex_co_cancel_ctx*)pArg;

	if ( pCtx == NULL || pCtx->pWorker == NULL ) {
		return;
	}

	pCtx->bJoinOk = xrtCoJoin(pCtx->pWorker);
}


int main(void)
{
	ex_co_cancel_ctx tCtx;
	xcosched* pSched = NULL;
	xcoro pCanceller = NULL;
	xcoro pJoiner = NULL;
	bool bWasCancelled = FALSE;
	int64 iExitCode = 0;
	const char* sResult = NULL;
	int iRet = 0;

	memset(&tCtx, 0, sizeof(tCtx));
	xrtInit();

	pSched = xrtCoSchedCreate();
	if ( pSched == NULL ) {
		printf("scheduler = failed\n");
		iRet = 1;
		goto cleanup;
	}

	tCtx.pWorker = xrtCoSchedSpawn(pSched, procWorker, &tCtx, 0u);
	pCanceller = xrtCoSchedSpawn(pSched, procCanceller, &tCtx, 0u);
	pJoiner = xrtCoSchedSpawn(pSched, procJoiner, &tCtx, 0u);
	if ( tCtx.pWorker == NULL || pCanceller == NULL || pJoiner == NULL ) {
		printf("spawn = failed\n");
		iRet = 1;
		goto cleanup;
	}

	xrtCoSchedRun(pSched);

	bWasCancelled = xrtCoWasCancelled(tCtx.pWorker);
	iExitCode = xrtCoGetExitCode(tCtx.pWorker);
	sResult = (const char*)xrtCoGetResult(tCtx.pWorker);

	printf("join_ok = %s\n", tCtx.bJoinOk ? "TRUE" : "FALSE");
	printf("was_cancelled = %s\n", bWasCancelled ? "TRUE" : "FALSE");
	printf("exit_code = %lld\n", (long long)iExitCode);
	printf("loop_count = %ld\n", tCtx.iLoopCount);
	printf("result = %s\n", sResult ? sResult : "(null)");

	if ( !tCtx.bJoinOk || !bWasCancelled || iExitCode != 77 ) {
		iRet = 1;
	}

cleanup:
	if ( pJoiner != NULL ) {
		(void)xrtCoClose(pJoiner);
	}
	if ( pCanceller != NULL ) {
		(void)xrtCoClose(pCanceller);
	}
	if ( tCtx.pWorker != NULL ) {
		(void)xrtCoClose(tCtx.pWorker);
	}
	if ( pSched != NULL ) {
		xrtCoSchedDestroy(pSched);
	}
	xrtUnit();
	return iRet;
}
