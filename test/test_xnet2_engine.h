#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#else
	#include <unistd.h>
#endif

#include "test_atomic_compat.h"


typedef struct {
	volatile long iCount;
	volatile long arrWorkerHits[8];
} __test_xnet2_engine_counter;

typedef struct {
	xsem hStarted;
	xsem hRelease;
	volatile long iCount;
} __test_xnet2_engine_block_ctx;


// 内部函数：__Test_XNet2_EngineSleepMs
static void __Test_XNet2_EngineSleepMs(uint32 iDelayMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelayMs);
	#else
		usleep((useconds_t)iDelayMs * 1000u);
	#endif
}


// 内部函数：__Test_XNet2_AtomicInc
static long __Test_XNet2_AtomicInc(volatile long* pValue)
{
	return __xrtTestAtomicAddFetchLong(pValue, 1);
}


// 内部函数：__Test_XNet2_AtomicLoad
static long __Test_XNet2_AtomicLoad(volatile long* pValue)
{
	return __xrtTestAtomicLoadLong(pValue);
}


// 内部函数：__Test_XNet2_EngineCmdQFreeCount
static uint32 __Test_XNet2_EngineCmdQFreeCount(xnetengine* pEngine, uint32 iWorkerId)
{
	__xnet_engine_cmdq* pQ;
	uint32 iCount = 0;

	if ( pEngine == NULL || iWorkerId >= pEngine->iWorkerCount || pEngine->arrWorkers[iWorkerId].pCmdQ == NULL ) {
		return 0;
	}

	pQ = (__xnet_engine_cmdq*)pEngine->arrWorkers[iWorkerId].pCmdQ;
	__xrtOwnerSpinLock(&pQ->iFreeLock);
	iCount = pQ->iFreeCount;
	__xrtOwnerSpinUnlock(&pQ->iFreeLock);
	return iCount;
}


// 内部函数：__Test_XNet2_EngineTask
static void __Test_XNet2_EngineTask(xnetworker* pWorker, ptr pArg)
{
	__test_xnet2_engine_counter* pCounter = (__test_xnet2_engine_counter*)pArg;
	if ( !pWorker || !pCounter ) return;
	if ( pWorker->iId < 8 ) {
		__Test_XNet2_AtomicInc(&pCounter->arrWorkerHits[pWorker->iId]);
	}
	__Test_XNet2_AtomicInc(&pCounter->iCount);
}


// 内部函数：__Test_XNet2_EngineTimerTask
static void __Test_XNet2_EngineTimerTask(xnetworker* pWorker, ptr pArg)
{
	__Test_XNet2_EngineTask(pWorker, pArg);
}


// 内部函数：__Test_XNet2_EngineBlockingTask
static void __Test_XNet2_EngineBlockingTask(xnetworker* pWorker, ptr pArg)
{
	__test_xnet2_engine_block_ctx* pCtx = (__test_xnet2_engine_block_ctx*)pArg;
	(void)pWorker;
	if ( !pCtx ) return;
	if ( pCtx->hStarted ) {
		(void)xrtSemPost(pCtx->hStarted);
	}
	if ( pCtx->hRelease ) {
		(void)xrtSemWaitTimeout(pCtx->hRelease, 1000);
	}
	__Test_XNet2_AtomicInc(&pCtx->iCount);
}


// XNET2引擎测试
void Test_XNet2_Engine(void)
{
	printf("\n\n\n------------------------------------\n\n XNet2 Engine Skeleton Test:\n\n");

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine;
		__test_xnet2_engine_counter tCounter;
		__test_xnet2_engine_counter tTimerCounter;
		long iCount = 0;
		memset(&tCounter, 0, sizeof(tCounter));
		memset(&tTimerCounter, 0, sizeof(tTimerCounter));

		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 2;
		tCfg.iTimerTickMs = 10;
		tCfg.iTimerWheelSlots = 64;

		pEngine = xrtNetEngineCreate(&tCfg);
		printf("  Engine create : %s\n", pEngine != NULL ? "PASS" : "FAIL");
		printf("  Engine worker count preset : %s\n",
			pEngine && xrtNetEngineGetWorkerCount(pEngine) == 2 ? "PASS" : "FAIL");

		if ( pEngine ) {
			printf("  Engine start : %s\n", xrtNetEngineStart(pEngine) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  Engine running flag : %s\n", pEngine->bRunning ? "PASS" : "FAIL");
			printf("  Worker 0 running : %s\n", pEngine->arrWorkers[0].bRunning ? "PASS" : "FAIL");
			printf("  Worker 0 thread handle : %s\n", pEngine->arrWorkers[0].hThread != NULL ? "PASS" : "FAIL");
			printf("  Worker 0 port ops : %s\n", pEngine->arrWorkers[0].tPort.pOps != NULL ? "PASS" : "FAIL");

			printf("  Engine post #0 : %s\n", xrtNetEnginePost(pEngine, 0, __Test_XNet2_EngineTask, &tCounter) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  Engine post #1 : %s\n", xrtNetEnginePost(pEngine, 1, __Test_XNet2_EngineTask, &tCounter) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  Engine post #2 : %s\n", xrtNetEnginePost(pEngine, 2, __Test_XNet2_EngineTask, &tCounter) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  Engine post #3 : %s\n", xrtNetEnginePost(pEngine, 3, __Test_XNet2_EngineTask, &tCounter) == XRT_NET_OK ? "PASS" : "FAIL");

			for ( int iLoop = 0; iLoop < 100; ++iLoop ) {
				iCount = __Test_XNet2_AtomicLoad(&tCounter.iCount);
				if ( iCount >= 4 ) break;
				__Test_XNet2_EngineSleepMs(10);
			}

			printf("  Engine post executes tasks : %s\n", iCount == 4 ? "PASS" : "FAIL");
			printf("  Worker 0 receives affinity hits : %s\n", __Test_XNet2_AtomicLoad(&tCounter.arrWorkerHits[0]) >= 2 ? "PASS" : "FAIL");
			printf("  Worker 1 receives affinity hits : %s\n", __Test_XNet2_AtomicLoad(&tCounter.arrWorkerHits[1]) >= 2 ? "PASS" : "FAIL");

			printf("  Engine delayed post #0 : %s\n", xrtNetEnginePostDelayed(pEngine, 0, 40, __Test_XNet2_EngineTimerTask, &tTimerCounter) == XRT_NET_OK ? "PASS" : "FAIL");
			printf("  Engine delayed post #1 : %s\n", xrtNetEnginePostDelayed(pEngine, 1, 60, __Test_XNet2_EngineTimerTask, &tTimerCounter) == XRT_NET_OK ? "PASS" : "FAIL");
			__Test_XNet2_EngineSleepMs(5);
			printf("  Delayed tasks not immediate : %s\n", __Test_XNet2_AtomicLoad(&tTimerCounter.iCount) == 0 ? "PASS" : "FAIL");

			for ( int iLoop = 0; iLoop < 100; ++iLoop ) {
				iCount = __Test_XNet2_AtomicLoad(&tTimerCounter.iCount);
				if ( iCount >= 2 ) break;
				__Test_XNet2_EngineSleepMs(10);
			}

			printf("  Delayed tasks execute : %s\n", iCount == 2 ? "PASS" : "FAIL");
			printf("  Delayed task worker 0 hit : %s\n", __Test_XNet2_AtomicLoad(&tTimerCounter.arrWorkerHits[0]) >= 1 ? "PASS" : "FAIL");
			printf("  Delayed task worker 1 hit : %s\n", __Test_XNet2_AtomicLoad(&tTimerCounter.arrWorkerHits[1]) >= 1 ? "PASS" : "FAIL");

			xrtNetEngineStop(pEngine);
			printf("  Engine stop clears flag : %s\n", !pEngine->bRunning ? "PASS" : "FAIL");
			printf("  Worker 0 stop clears flag : %s\n", !pEngine->arrWorkers[0].bRunning ? "PASS" : "FAIL");
			printf("  Engine post after stop fails : %s\n",
				xrtNetEnginePost(pEngine, 0, __Test_XNet2_EngineTask, &tCounter) == XRT_NET_ERROR ? "PASS" : "FAIL");
			printf("  Engine delayed post after stop fails : %s\n",
				xrtNetEnginePostDelayed(pEngine, 0, 10, __Test_XNet2_EngineTimerTask, &tTimerCounter) == XRT_NET_ERROR ? "PASS" : "FAIL");

			xrtNetEngineDestroy(pEngine);
			printf("  Engine destroy : PASS\n");
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine;
		__test_xnet2_engine_counter tCounter;
		__test_xnet2_engine_counter tTimerCounter;
		__test_xnet2_engine_block_ctx tBlockCtx;
		long iCount = 0;
		int bStarted = 0;

		memset(&tCounter, 0, sizeof(tCounter));
		memset(&tTimerCounter, 0, sizeof(tTimerCounter));
		memset(&tBlockCtx, 0, sizeof(tBlockCtx));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		tCfg.iCmdQueueSize = 2;
		tCfg.iTimerTickMs = 10;
		tCfg.iTimerWheelSlots = 64;

		tBlockCtx.hStarted = xrtSemCreate(0, 1);
		tBlockCtx.hRelease = xrtSemCreate(0, 1);
		pEngine = xrtNetEngineCreate(&tCfg);
		printf("  Engine create small cmdq : %s\n", (pEngine != NULL && tBlockCtx.hStarted != NULL && tBlockCtx.hRelease != NULL) ? "PASS" : "FAIL");

		if ( pEngine && tBlockCtx.hStarted && tBlockCtx.hRelease ) {
			bStarted = (xrtNetEngineStart(pEngine) == XRT_NET_OK);
			printf("  Engine start small cmdq : %s\n", bStarted ? "PASS" : "FAIL");
			if ( bStarted ) {
				printf("  Blocking post : %s\n", xrtNetEnginePost(pEngine, 0, __Test_XNet2_EngineBlockingTask, &tBlockCtx) == XRT_NET_OK ? "PASS" : "FAIL");
				printf("  Blocking task starts : %s\n", xrtSemWaitTimeout(tBlockCtx.hStarted, 1000) == XRT_WAIT_OK ? "PASS" : "FAIL");
				printf("  Queue fill post #1 : %s\n", xrtNetEnginePost(pEngine, 0, __Test_XNet2_EngineTask, &tCounter) == XRT_NET_OK ? "PASS" : "FAIL");
				printf("  Queue fill post #2 : %s\n", xrtNetEnginePost(pEngine, 0, __Test_XNet2_EngineTask, &tCounter) == XRT_NET_OK ? "PASS" : "FAIL");
				printf("  Queue full returns again : %s\n", xrtNetEnginePost(pEngine, 0, __Test_XNet2_EngineTask, &tCounter) == XRT_NET_AGAIN ? "PASS" : "FAIL");
				printf("  Delayed queue full returns again : %s\n",
					xrtNetEnginePostDelayed(pEngine, 0, 10, __Test_XNet2_EngineTimerTask, &tTimerCounter) == XRT_NET_AGAIN ? "PASS" : "FAIL");
				(void)xrtSemPost(tBlockCtx.hRelease);

				for ( int iLoop = 0; iLoop < 100; ++iLoop ) {
					iCount = __Test_XNet2_AtomicLoad(&tCounter.iCount);
					if ( iCount >= 2 && __Test_XNet2_AtomicLoad(&tBlockCtx.iCount) >= 1 ) break;
					__Test_XNet2_EngineSleepMs(10);
				}

				printf("  Queue resumes after release : %s\n", iCount == 2 && __Test_XNet2_AtomicLoad(&tBlockCtx.iCount) == 1 ? "PASS" : "FAIL");
				printf("  Full delayed post stays dropped : %s\n", __Test_XNet2_AtomicLoad(&tTimerCounter.iCount) == 0 ? "PASS" : "FAIL");
			}
			xrtNetEngineDestroy(pEngine);
			printf("  Engine destroy small cmdq : PASS\n");
		} else if ( pEngine ) {
			xrtNetEngineDestroy(pEngine);
		}

		if ( tBlockCtx.hStarted ) {
			xrtSemDestroy(tBlockCtx.hStarted);
		}
		if ( tBlockCtx.hRelease ) {
			xrtSemDestroy(tBlockCtx.hRelease);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine;
		__test_xnet2_engine_counter tCounter;
		__test_xnet2_engine_block_ctx tBlockCtx;
		uint32 iFreeBefore = 0;
		uint32 iFreeDuring = 0;
		uint32 iFreeAfter = 0;
		int bStarted = 0;

		memset(&tCounter, 0, sizeof(tCounter));
		memset(&tBlockCtx, 0, sizeof(tBlockCtx));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		tCfg.iCmdQueueSize = 8;
		tCfg.iTimerTickMs = 10;
		tCfg.iTimerWheelSlots = 64;

		tBlockCtx.hStarted = xrtSemCreate(0, 1);
		tBlockCtx.hRelease = xrtSemCreate(0, 1);
		pEngine = xrtNetEngineCreate(&tCfg);
		printf("  Engine create cmd cache test : %s\n", (pEngine != NULL && tBlockCtx.hStarted != NULL && tBlockCtx.hRelease != NULL) ? "PASS" : "FAIL");

		if ( pEngine && tBlockCtx.hStarted && tBlockCtx.hRelease ) {
			bStarted = (xrtNetEngineStart(pEngine) == XRT_NET_OK);
			printf("  Engine start cmd cache test : %s\n", bStarted ? "PASS" : "FAIL");
			if ( bStarted ) {
				printf("  Cache warm blocker post : %s\n", xrtNetEnginePost(pEngine, 0, __Test_XNet2_EngineBlockingTask, &tBlockCtx) == XRT_NET_OK ? "PASS" : "FAIL");
				printf("  Cache warm blocker starts : %s\n", xrtSemWaitTimeout(tBlockCtx.hStarted, 1000) == XRT_WAIT_OK ? "PASS" : "FAIL");
				printf("  Cache warm post #1 : %s\n", xrtNetEnginePost(pEngine, 0, __Test_XNet2_EngineTask, &tCounter) == XRT_NET_OK ? "PASS" : "FAIL");
				printf("  Cache warm post #2 : %s\n", xrtNetEnginePost(pEngine, 0, __Test_XNet2_EngineTask, &tCounter) == XRT_NET_OK ? "PASS" : "FAIL");
				(void)xrtSemPost(tBlockCtx.hRelease);

				for ( int iLoop = 0; iLoop < 100; ++iLoop ) {
					iFreeBefore = __Test_XNet2_EngineCmdQFreeCount(pEngine, 0);
					if ( __Test_XNet2_AtomicLoad(&tCounter.iCount) >= 2 &&
						__Test_XNet2_AtomicLoad(&tBlockCtx.iCount) >= 1 &&
						iFreeBefore >= 3u ) {
						break;
					}
					__Test_XNet2_EngineSleepMs(10);
				}

				printf("  Cmd node cache warm count : tasks=%ld blocks=%ld free=%u\n",
					__Test_XNet2_AtomicLoad(&tCounter.iCount),
					__Test_XNet2_AtomicLoad(&tBlockCtx.iCount),
					iFreeBefore);
				printf("  Cmd node cache warms : %s\n", __Test_XNet2_AtomicLoad(&tCounter.iCount) == 2 &&
					__Test_XNet2_AtomicLoad(&tBlockCtx.iCount) >= 1 &&
					iFreeBefore >= 3u ? "PASS" : "FAIL");
				printf("  Blocking cache post : %s\n", xrtNetEnginePost(pEngine, 0, __Test_XNet2_EngineBlockingTask, &tBlockCtx) == XRT_NET_OK ? "PASS" : "FAIL");
				printf("  Blocking cache task starts : %s\n", xrtSemWaitTimeout(tBlockCtx.hStarted, 1000) == XRT_WAIT_OK ? "PASS" : "FAIL");

				iFreeDuring = __Test_XNet2_EngineCmdQFreeCount(pEngine, 0);
				printf("  Cmd node cache reuses entry : %s\n", iFreeBefore > 0u && iFreeDuring + 1u == iFreeBefore ? "PASS" : "FAIL");

				(void)xrtSemPost(tBlockCtx.hRelease);
				for ( int iLoop = 0; iLoop < 100; ++iLoop ) {
					iFreeAfter = __Test_XNet2_EngineCmdQFreeCount(pEngine, 0);
					if ( __Test_XNet2_AtomicLoad(&tBlockCtx.iCount) >= 2 && iFreeAfter >= iFreeBefore ) break;
					__Test_XNet2_EngineSleepMs(10);
				}

				printf("  Cmd node cache returns entry : %s\n", __Test_XNet2_AtomicLoad(&tBlockCtx.iCount) >= 2 && iFreeAfter >= iFreeBefore ? "PASS" : "FAIL");
			}

			xrtNetEngineDestroy(pEngine);
			printf("  Engine destroy cmd cache test : PASS\n");
		} else if ( pEngine ) {
			xrtNetEngineDestroy(pEngine);
		}

		if ( tBlockCtx.hStarted ) {
			xrtSemDestroy(tBlockCtx.hStarted);
		}
		if ( tBlockCtx.hRelease ) {
			xrtSemDestroy(tBlockCtx.hRelease);
		}
	}

	{
		xnetengineconfig tCfg;
		xnetengine* pEngine;
		__test_xnet2_engine_counter tTimerCounter;
		__test_xnet2_engine_block_ctx tBlockCtx;
		uint32 iFreeWarm = 0;
		uint32 iFreeDuringBlock = 0;
		uint32 iFreeDuringQueued = 0;
		uint32 iFreeAfter = 0;
		int bStarted = 0;

		memset(&tTimerCounter, 0, sizeof(tTimerCounter));
		memset(&tBlockCtx, 0, sizeof(tBlockCtx));
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		tCfg.iCmdQueueSize = 8;
		tCfg.iTimerTickMs = 10;
		tCfg.iTimerWheelSlots = 64;

		tBlockCtx.hStarted = xrtSemCreate(0, 1);
		tBlockCtx.hRelease = xrtSemCreate(0, 1);
		pEngine = xrtNetEngineCreate(&tCfg);
		printf("  Engine create delayed cmd cache test : %s\n", (pEngine != NULL && tBlockCtx.hStarted != NULL && tBlockCtx.hRelease != NULL) ? "PASS" : "FAIL");

		if ( pEngine && tBlockCtx.hStarted && tBlockCtx.hRelease ) {
			bStarted = (xrtNetEngineStart(pEngine) == XRT_NET_OK);
			printf("  Engine start delayed cmd cache test : %s\n", bStarted ? "PASS" : "FAIL");
			if ( bStarted ) {
				printf("  Delayed cache warm blocker post : %s\n", xrtNetEnginePost(pEngine, 0, __Test_XNet2_EngineBlockingTask, &tBlockCtx) == XRT_NET_OK ? "PASS" : "FAIL");
				printf("  Delayed cache warm blocker starts : %s\n", xrtSemWaitTimeout(tBlockCtx.hStarted, 1000) == XRT_WAIT_OK ? "PASS" : "FAIL");
				printf("  Delayed cache warm post #1 : %s\n", xrtNetEnginePostDelayed(pEngine, 0, 20, __Test_XNet2_EngineTimerTask, &tTimerCounter) == XRT_NET_OK ? "PASS" : "FAIL");
				printf("  Delayed cache warm post #2 : %s\n", xrtNetEnginePostDelayed(pEngine, 0, 30, __Test_XNet2_EngineTimerTask, &tTimerCounter) == XRT_NET_OK ? "PASS" : "FAIL");
				(void)xrtSemPost(tBlockCtx.hRelease);

				for ( int iLoop = 0; iLoop < 100; ++iLoop ) {
					iFreeWarm = __Test_XNet2_EngineCmdQFreeCount(pEngine, 0);
					if ( __Test_XNet2_AtomicLoad(&tBlockCtx.iCount) >= 1 &&
						__Test_XNet2_AtomicLoad(&tTimerCounter.iCount) >= 2 &&
						iFreeWarm >= 3u ) {
						break;
					}
					__Test_XNet2_EngineSleepMs(10);
				}

				printf("  Delayed cmd cache warms : %s\n", __Test_XNet2_AtomicLoad(&tBlockCtx.iCount) >= 1 &&
					__Test_XNet2_AtomicLoad(&tTimerCounter.iCount) >= 2 &&
					iFreeWarm >= 3u ? "PASS" : "FAIL");
				printf("  Delayed blocker post : %s\n", xrtNetEnginePost(pEngine, 0, __Test_XNet2_EngineBlockingTask, &tBlockCtx) == XRT_NET_OK ? "PASS" : "FAIL");
				printf("  Delayed blocker starts : %s\n", xrtSemWaitTimeout(tBlockCtx.hStarted, 1000) == XRT_WAIT_OK ? "PASS" : "FAIL");

				iFreeDuringBlock = __Test_XNet2_EngineCmdQFreeCount(pEngine, 0);
				printf("  Blocking task reuses cached cmd node : %s\n", iFreeWarm > 0u && iFreeDuringBlock + 1u == iFreeWarm ? "PASS" : "FAIL");
				printf("  Delayed cached post : %s\n", xrtNetEnginePostDelayed(pEngine, 0, 20, __Test_XNet2_EngineTimerTask, &tTimerCounter) == XRT_NET_OK ? "PASS" : "FAIL");

				iFreeDuringQueued = __Test_XNet2_EngineCmdQFreeCount(pEngine, 0);
				printf("  Delayed cmd reuses cached node : %s\n", iFreeDuringBlock > 0u && iFreeDuringQueued + 1u == iFreeDuringBlock ? "PASS" : "FAIL");

				(void)xrtSemPost(tBlockCtx.hRelease);
				for ( int iLoop = 0; iLoop < 100; ++iLoop ) {
					iFreeAfter = __Test_XNet2_EngineCmdQFreeCount(pEngine, 0);
					if ( __Test_XNet2_AtomicLoad(&tBlockCtx.iCount) >= 1 &&
						__Test_XNet2_AtomicLoad(&tTimerCounter.iCount) >= 3 &&
						iFreeAfter >= iFreeWarm ) {
						break;
					}
					__Test_XNet2_EngineSleepMs(10);
				}

				printf("  Delayed cmd cache returns entries : %s\n", __Test_XNet2_AtomicLoad(&tBlockCtx.iCount) >= 1 &&
					__Test_XNet2_AtomicLoad(&tTimerCounter.iCount) >= 3 &&
					iFreeAfter >= iFreeWarm ? "PASS" : "FAIL");
			}

			xrtNetEngineDestroy(pEngine);
			printf("  Engine destroy delayed cmd cache test : PASS\n");
		} else if ( pEngine ) {
			xrtNetEngineDestroy(pEngine);
		}

		if ( tBlockCtx.hStarted ) {
			xrtSemDestroy(tBlockCtx.hStarted);
		}
		if ( tBlockCtx.hRelease ) {
			xrtSemDestroy(tBlockCtx.hRelease);
		}
	}

	{
		xnetengine* pEngine = xrtNetEngineCreate(NULL);
		printf("  Engine create auto cfg : %s\n", pEngine != NULL ? "PASS" : "FAIL");
		printf("  Engine auto workers >= 1 : %s\n",
			pEngine && xrtNetEngineGetWorkerCount(pEngine) >= 1 ? "PASS" : "FAIL");
		if ( pEngine ) {
			xrtNetEngineDestroy(pEngine);
		}
	}
}
