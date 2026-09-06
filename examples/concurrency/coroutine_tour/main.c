/*
 * 范例：concurrency/coroutine_tour —— 协程全接口（调度/生命周期/事件）
 * ----------------------------------------------------------------
 * 演示 API：
 *   【调度器】  xrtCoSchedCreateLimit（显式投递上限）/
 *              SchedCurrent / SchedPostOwned（接管式投递+恰好一次析构）/
 *              SchedStep（单步执行）/ SchedPollFor / SchedPollUntil /
 *              SchedAlive
 *   【生命周期】 xrtCoCurrent / State / Term / Error / CancelToken /
 *              Stopping / Backend / SleepUntil
 *   【Join/Park】 xrtCoJoinFor / JoinUntil / ParkFor / ParkUntil / Wake
 *   【事件】    xrtCoEventCreate / Destroy / Reset / TryAwait /
 *              EventAwaitFor / EventAwaitUntil（自动复位 FIFO 唤醒）
 * 模块宏：XRT_MODULE_COROUTINE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/concurrency/coroutine_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   coroutine: step-mode poll=1/1 alive=0
 *   coroutine: lifecycle term=1 backend=fiber-win
 *   coroutine: join for=1 until=0 park-for=0 park-until=1
 *   coroutine: events try/for/until = 3 woken=3
 *   coroutine: post-owned destroyed=1
 *
 * 调度器支持单步驱动（Step 跑一段、Poll 带超时）；
 *   Join/Park/事件等待都在协程内挂起，主线程只负责
 *   运行调度器并核对结果结构里的记录。
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <xrt.h>

#define EXAMPLE_SHORT_US	UINT64_C(100000)
#define EXAMPLE_LONG_US	UINT64_C(3000000)

/* PostOwned 析构计数。 */
static volatile int g_Destroyed = 0;

static void examplePostDestroy(ptr pData)
{
	(void)pData;
	g_Destroyed = g_Destroyed + 1;
}

static void examplePostProc(xcosched* pSched, ptr pData)
{
	(void)pSched;
	(void)pData;
}

/* ---- 生命周期协程：在协程内做自省，SleepUntil 后返回。 */
typedef struct examplelife {
	const char* sBackend;
	int iRunning;
	int iStopping;
	xcancel* pToken;
	xcosched* pSched;
} examplelife;

static ptr exampleCoLife(ptr pData)
{
	examplelife* pJob = (examplelife*)pData;
	xcoro* pSelf = xrtCoCurrent();

	pJob->sBackend = xrtCoBackend();
	pJob->iRunning = xrtCoState(pSelf) == XCORO_RUNNING ? 1 : 0;
	pJob->iStopping = xrtCoStopping() ? 1 : 0;
	pJob->pToken = xrtCoCancelToken(pSelf);
	pJob->pSched = xrtCoSchedCurrent();
	(void)xrtCoSleepUntil(xrtDeadlineAfter(EXAMPLE_SHORT_US));
	return (ptr)1;
}

/* ---- Join 目标：睡一小会儿再返回。 */
static ptr exampleCoSleeper(ptr pData)
{
	(void)pData;
	(void)xrtCoSleep(EXAMPLE_SHORT_US);
	return (ptr)2;
}

/* ---- Join 协程：先短超时（TIMEOUT），再长 Until（OK）。 */
typedef struct examplejoin {
	xcoro* pTarget;
	volatile int iForResult;
	volatile int iUntilResult;
} examplejoin;

static ptr exampleCoJoiner(ptr pData)
{
	examplejoin* pJob = (examplejoin*)pData;

	/* 目标在睡：短窗口必然 TIMEOUT。 */
	pJob->iForResult = (int)xrtCoJoinFor(pJob->pTarget,
		UINT64_C(1));
	/* 目标很快完成：长截止必然 OK。 */
	pJob->iUntilResult = (int)xrtCoJoinUntil(pJob->pTarget,
		xrtDeadlineAfter(EXAMPLE_LONG_US));
	return NULL;
}

/* ---- Park 协程：ParkFor 被唤醒；ParkUntil 到期。 */
typedef struct exampleparkjob {
	volatile int iForResult;
	volatile int iUntilResult;
} exampleparkjob;

static ptr exampleCoParker(ptr pData)
{
	exampleparkjob* pJob = (exampleparkjob*)pData;

	/* 长超时 Park，由唤醒协程 Wake。 */
	pJob->iForResult = (int)xrtCoParkFor(EXAMPLE_LONG_US);
	/* 已过期截止：立即 TIMEOUT（无人唤醒）。 */
	pJob->iUntilResult = (int)xrtCoParkUntil(
		xrtDeadlineAfter(UINT64_C(1)));
	return NULL;
}

/* ---- 唤醒协程：稍等 Parker 挂起后 Wake。 */
static ptr exampleCoWaker(ptr pData)
{
	xcoro* pParker = (xcoro*)pData;

	(void)xrtCoSleep(EXAMPLE_SHORT_US);
	(void)xrtCoWake(pParker);
	return NULL;
}

/* ---- 事件族：等待者依次走 Try/AwaitFor/AwaitUntil。 */
typedef struct exampleevt {
	xcoevent* pAuto;
	volatile int iWoken;
} exampleevt;

static ptr exampleCoWaiter(ptr pData)
{
	exampleevt* pJob = (exampleevt*)pData;

	if ( xrtCoEventTryAwait(pJob->pAuto) == XWAIT_OK ) {
		pJob->iWoken = pJob->iWoken + 1;
	}
	if ( xrtCoEventAwaitFor(pJob->pAuto, EXAMPLE_LONG_US) ==
		XWAIT_OK ) {
		pJob->iWoken = pJob->iWoken + 1;
	}
	if ( xrtCoEventAwaitUntil(pJob->pAuto,
			xrtDeadlineAfter(EXAMPLE_LONG_US)) == XWAIT_OK ) {
		pJob->iWoken = pJob->iWoken + 1;
	}
	return NULL;
}

/* 事件驱动协程：让出若干轮后在两个等待点各 Set 一次。 */
static ptr exampleCoEvtDriver(ptr pData)
{
	exampleevt* pJob = (exampleevt*)pData;
	int i;

	for ( i = 0; i < 50; ++i ) {
		(void)xrtCoYield();
	}
	(void)xrtCoEventSet(pJob->pAuto);  /* 唤醒 AwaitFor */
	for ( i = 0; i < 50; ++i ) {
		(void)xrtCoYield();
	}
	(void)xrtCoEventSet(pJob->pAuto);  /* 唤醒 AwaitUntil */
	for ( i = 0; i < 50; ++i ) {
		(void)xrtCoYield();
	}
	(void)xrtCoEventReset(pJob->pAuto);  /* 清信号（覆盖点） */
	return NULL;
}

int main(void)
{
	xcosched* pStep = NULL;
	xcosched* pSched = NULL;
	xcoro* pLife = NULL;
	xcoro* pSleeper = NULL;
	xcoro* pJoiner = NULL;
	xcoro* pParker = NULL;
	xcoro* pWaker = NULL;
	xcoro* pWaiter = NULL;
	xcoro* pEvtDriver = NULL;
	xcoevent* pAuto = NULL;
	examplelife Life;
	examplejoin Join;
	exampleparkjob Park;
	exampleevt Evt;
	int iResult = 1;

	memset(&Life, 0, sizeof(Life));
	memset(&Join, 0, sizeof(Join));
	memset(&Park, 0, sizeof(Park));
	memset(&Evt, 0, sizeof(Evt));

	/* ---- 单步调度：CreateLimit/Step/PollFor/PollUntil/Alive ---- */
	pStep = xrtCoSchedCreateLimit(16u);
	if ( (pStep == NULL) ||
		!xrtCoSchedPost(pStep, examplePostProc, NULL) ||
		(xrtCoSchedAlive(pStep) != 0u) ||
		(xrtCoSchedStep(pStep) != XWAIT_OK) ||
		!xrtCoSchedPost(pStep, examplePostProc, NULL) ||
		(xrtCoSchedPollFor(pStep, EXAMPLE_LONG_US) !=
			XWAIT_OK) ||
		!xrtCoSchedPost(pStep, examplePostProc, NULL) ||
		(xrtCoSchedPollUntil(pStep,
			xrtDeadlineAfter(EXAMPLE_LONG_US)) !=
			XWAIT_OK) ||
		!xrtCoSchedDestroy(pStep) ) {
		goto Cleanup;
	}
	pStep = NULL;
	printf("coroutine: step-mode poll=1/1 alive=0\n");

	/* ---- 生命周期：协程内自省 + 主线程终态核对 ---- */
	pSched = xrtCoSchedCreate();
	if ( (pSched == NULL) ||
		(xrtCoSchedCurrent() != NULL) ) {  /* 主线程无调度器 */
		goto Cleanup;
	}
	pLife = xrtCoSpawn(pSched, exampleCoLife, &Life, NULL);
	if ( (pLife == NULL) || !xrtCoSchedRun(pSched) ) {
		goto Cleanup;
	}
	if ( (Life.iRunning != 1) || (Life.iStopping != 0) ||
		(Life.pToken == NULL) || (Life.sBackend == NULL) ||
		(Life.pSched != pSched) ||
		(xrtCoState(pLife) != XCORO_DONE) ||
		(xrtCoTerm(pLife) != XCORO_TERM_RETURNED) ||
		(xrtCoError(pLife) != NULL) ||
		(xrtCoResult(pLife) != (ptr)1) ) {
		goto Cleanup;
	}
	printf("coroutine: lifecycle term=1 backend=%s\n",
		Life.sBackend);
	xrtCoDestroy(pLife);
	pLife = NULL;

	/* ---- Join/Park/Wake：三条协程同场 ---- */
	pSleeper = xrtCoSpawn(pSched, exampleCoSleeper, NULL, NULL);
	Join.pTarget = pSleeper;
	pJoiner = xrtCoSpawn(pSched, exampleCoJoiner, &Join, NULL);
	pParker = xrtCoSpawn(pSched, exampleCoParker, &Park, NULL);
	pWaker = xrtCoSpawn(pSched, exampleCoWaker, pParker, NULL);
	if ( (pSleeper == NULL) || (pJoiner == NULL) ||
		(pParker == NULL) || (pWaker == NULL) ||
		!xrtCoSchedRun(pSched) ) {
		goto Cleanup;
	}
	/* JoinFor 短窗 TIMEOUT=1；JoinUntil OK=0；
	 * ParkFor 被唤醒 OK=0；ParkUntil 过期 TIMEOUT=1。 */
	if ( (Join.iForResult != XWAIT_TIMEOUT) ||
		(Join.iUntilResult != XWAIT_OK) ||
		(Park.iForResult != XWAIT_OK) ||
		(Park.iUntilResult != XWAIT_TIMEOUT) ) {
		goto Cleanup;
	}
	printf("coroutine: join for=%d until=%d park-for=%d"
		" park-until=%d\n",
		Join.iForResult, Join.iUntilResult, Park.iForResult,
		Park.iUntilResult);
	xrtCoDestroy(pSleeper);
	pSleeper = NULL;
	xrtCoDestroy(pJoiner);
	pJoiner = NULL;
	xrtCoDestroy(pParker);
	pParker = NULL;
	xrtCoDestroy(pWaker);
	pWaker = NULL;

	/* ---- 事件族：预置位 Try 消费 + 双 Set + Reset ---- */
	pAuto = xrtCoEventCreate(false, false);
	if ( (pAuto == NULL) || !xrtCoEventSet(pAuto) ) {
		goto Cleanup;  /* 预置位供 TryAwait 立即消费 */
	}
	Evt.pAuto = pAuto;
	pWaiter = xrtCoSpawn(pSched, exampleCoWaiter, &Evt, NULL);
	pEvtDriver = xrtCoSpawn(pSched, exampleCoEvtDriver, &Evt,
		NULL);
	if ( (pWaiter == NULL) || (pEvtDriver == NULL) ||
		!xrtCoSchedRun(pSched) ||
		(Evt.iWoken != 3) ||
		!xrtCoEventDestroy(pAuto) ) {
		goto Cleanup;
	}
	pAuto = NULL;
	printf("coroutine: events try/for/until = 3 woken=%d\n",
		Evt.iWoken);
	xrtCoDestroy(pWaiter);
	pWaiter = NULL;
	xrtCoDestroy(pEvtDriver);
	pEvtDriver = NULL;

	/* ---- PostOwned：受理后析构恰好一次 ---- */
	g_Destroyed = 0;
	if ( !xrtCoSchedPostOwned(pSched, examplePostProc, (ptr)7,
			examplePostDestroy) ||
		!xrtCoSchedRun(pSched) ||
		(g_Destroyed != 1) ) {
		goto Cleanup;
	}
	printf("coroutine: post-owned destroyed=%d\n", g_Destroyed);
	iResult = 0;

Cleanup:
	xrtCoDestroy(pLife);
	xrtCoDestroy(pSleeper);
	xrtCoDestroy(pJoiner);
	xrtCoDestroy(pParker);
	xrtCoDestroy(pWaker);
	xrtCoDestroy(pWaiter);
	xrtCoDestroy(pEvtDriver);
	if ( pAuto != NULL ) {
		(void)xrtCoEventDestroy(pAuto);
	}
	if ( pSched != NULL ) {
		xrtCoSchedDestroy(pSched);
		xrtCoThreadDetach();
	}
	if ( pStep != NULL ) {
		xrtCoSchedDestroy(pStep);
	}
	return iResult;
}
