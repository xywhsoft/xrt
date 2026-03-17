#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#else
	#include <unistd.h>
#endif


typedef struct {
	volatile long iCount;
	volatile long arrWorkerHits[8];
} __test_xnet2_engine_counter;


static void __Test_XNet2_EngineSleepMs(uint32 iDelayMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelayMs);
	#else
		usleep((useconds_t)iDelayMs * 1000u);
	#endif
}

static long __Test_XNet2_AtomicInc(volatile long* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedIncrement((volatile LONG*)pValue);
	#else
		return __sync_add_and_fetch(pValue, 1);
	#endif
}

static long __Test_XNet2_AtomicLoad(volatile long* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedCompareExchange((volatile LONG*)pValue, 0, 0);
	#else
		return __sync_add_and_fetch(pValue, 0);
	#endif
}

static void __Test_XNet2_EngineTask(xnetworker* pWorker, ptr pArg)
{
	__test_xnet2_engine_counter* pCounter = (__test_xnet2_engine_counter*)pArg;
	if ( !pWorker || !pCounter ) return;
	if ( pWorker->iId < 8 ) {
		__Test_XNet2_AtomicInc(&pCounter->arrWorkerHits[pWorker->iId]);
	}
	__Test_XNet2_AtomicInc(&pCounter->iCount);
}

static void __Test_XNet2_EngineTimerTask(xnetworker* pWorker, ptr pArg)
{
	__Test_XNet2_EngineTask(pWorker, pArg);
}


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
		xnetengine* pEngine = xrtNetEngineCreate(NULL);
		printf("  Engine create auto cfg : %s\n", pEngine != NULL ? "PASS" : "FAIL");
		printf("  Engine auto workers >= 1 : %s\n",
			pEngine && xrtNetEngineGetWorkerCount(pEngine) >= 1 ? "PASS" : "FAIL");
		if ( pEngine ) {
			xrtNetEngineDestroy(pEngine);
		}
	}
}
