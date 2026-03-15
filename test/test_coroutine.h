



/* ================================ 协程测试辅助数据 ================================ */

// 基础 yield 测试用
static int __test_co_step = 0;

static void __test_co_basic_func(ptr pParam)
{
	int* pCounter = (int*)pParam;
	__test_co_step = 1;
	(*pCounter)++;
	xrtCoYield();
	
	__test_co_step = 2;
	(*pCounter)++;
	xrtCoYield();
	
	__test_co_step = 3;
	(*pCounter)++;
	// 函数返回 → 协程自动变为 DEAD
}

// 多协程交替测试用
static int __test_co_order[10];
static int __test_co_order_idx = 0;

static void __test_co_a_func(ptr pParam)
{
	__test_co_order[__test_co_order_idx++] = 1;
	xrtCoYield();
	__test_co_order[__test_co_order_idx++] = 3;
	xrtCoYield();
	__test_co_order[__test_co_order_idx++] = 5;
}

static void __test_co_b_func(ptr pParam)
{
	__test_co_order[__test_co_order_idx++] = 2;
	xrtCoYield();
	__test_co_order[__test_co_order_idx++] = 4;
	xrtCoYield();
	__test_co_order[__test_co_order_idx++] = 6;
}

// GetCurrent 测试用
static xcoro __test_co_got_current = NULL;

static void __test_co_current_func(ptr pParam)
{
	__test_co_got_current = xrtCoGetCurrent();
	xrtCoYield();
}

// 调度器测试用
static int __test_sched_done[4] = {0};

static void __test_sched_func(ptr pParam)
{
	int iIdx = (int)(intptr)pParam;
	xrtCoYield();
	xrtCoYield();
	__test_sched_done[iIdx] = 1;
}

// CoSleep 测试用
static int __test_sleep_order[10];
static int __test_sleep_idx = 0;

static void __test_sleep_fast(ptr pParam)
{
	xrtCoSleep(50);
	__test_sleep_order[__test_sleep_idx++] = 1;  // fast 先完成
}

static void __test_sleep_slow(ptr pParam)
{
	xrtCoSleep(150);
	__test_sleep_order[__test_sleep_idx++] = 2;  // slow 后完成
}

typedef struct {
	xcoro pTarget;
	int arrEvents[8];
	int iEventCount;
	bool bJoined;
	bool bCancelled;
	int iLoopCount;
} __test_co_lifecycle_case;

typedef struct {
	xcoro pTarget;
	int arrEvents[8];
	int iEventCount;
	bool arrJoined[2];
} __test_co_multi_join_case;

static void __test_co_lifecycle_push(__test_co_lifecycle_case* pCase, int iValue)
{
	if ( pCase == NULL ) {
		return;
	}

	if ( pCase->iEventCount < (int)(sizeof(pCase->arrEvents) / sizeof(pCase->arrEvents[0])) ) {
		pCase->arrEvents[pCase->iEventCount++] = iValue;
	}
}

static void __test_co_join_target(ptr pParam)
{
	__test_co_lifecycle_case* pCase = (__test_co_lifecycle_case*)pParam;
	__test_co_lifecycle_push(pCase, 1);
	xrtCoYield();
	__test_co_lifecycle_push(pCase, 2);
}

static void __test_co_join_waiter(ptr pParam)
{
	__test_co_lifecycle_case* pCase = (__test_co_lifecycle_case*)pParam;
	__test_co_lifecycle_push(pCase, 10);
	if ( xrtCoJoin(pCase->pTarget) ) {
		pCase->bJoined = TRUE;
		__test_co_lifecycle_push(pCase, 20);
	}
}

static void __test_co_multi_join_target(ptr pParam)
{
	__test_co_multi_join_case* pCase = (__test_co_multi_join_case*)pParam;
	if ( pCase == NULL ) {
		return;
	}
	if ( pCase->iEventCount < 8 ) pCase->arrEvents[pCase->iEventCount++] = 1;
	xrtCoYield();
	if ( pCase->iEventCount < 8 ) pCase->arrEvents[pCase->iEventCount++] = 2;
}

static void __test_co_multi_join_waiter_1(ptr pParam)
{
	__test_co_multi_join_case* pCase = (__test_co_multi_join_case*)pParam;
	if ( pCase == NULL ) {
		return;
	}
	if ( pCase->iEventCount < 8 ) pCase->arrEvents[pCase->iEventCount++] = 10;
	if ( xrtCoJoin(pCase->pTarget) ) {
		pCase->arrJoined[0] = TRUE;
		if ( pCase->iEventCount < 8 ) pCase->arrEvents[pCase->iEventCount++] = 20;
	}
}

static void __test_co_multi_join_waiter_2(ptr pParam)
{
	__test_co_multi_join_case* pCase = (__test_co_multi_join_case*)pParam;
	if ( pCase == NULL ) {
		return;
	}
	if ( pCase->iEventCount < 8 ) pCase->arrEvents[pCase->iEventCount++] = 11;
	if ( xrtCoJoin(pCase->pTarget) ) {
		pCase->arrJoined[1] = TRUE;
		if ( pCase->iEventCount < 8 ) pCase->arrEvents[pCase->iEventCount++] = 21;
	}
}

static void __test_co_cancel_worker(ptr pParam)
{
	__test_co_lifecycle_case* pCase = (__test_co_lifecycle_case*)pParam;

	while ( TRUE ) {
		pCase->iLoopCount++;
		if ( xrtCoIsCancelRequested() ) {
			pCase->bCancelled = TRUE;
			return;
		}
		xrtCoSleep(1000);
	}
}

typedef struct {
	xcosched* pSched;
	xcoro pTarget;
	xcosched* pSeenSched;
	bool bWoke;
	bool bPosted;
} __test_co_post_case;

typedef struct {
	bool bPass;
	int iFramesChecked;
	uint32 iLeafChecksum;
	uint32 iFinalChecksum;
} __test_co_deep_stack_case;

typedef struct {
	bool bAfterExit;
	int iBeforeExit;
} __test_co_exit_case;

typedef struct {
	bool bDone;
	int64 iStartMs;
	int64 iEndMs;
	int64 iElapsedMs;
} __test_co_deadline_case;

typedef struct {
	int iCounter;
} __test_co_createex_case;

typedef struct {
	xcoro pTarget;
	bool bWaitResult;
	bool bCancelledObserved;
} __test_co_waitdeadline_case;

typedef struct {
	xcoevent pEvent;
	bool bWaitResult;
	bool bDone;
	bool bSetDone;
	bool bImmediateResult;
	bool bImmediateDone;
	bool bTimeoutResult;
	bool bTimeoutDone;
	int64 iTimeoutElapsedMs;
	bool bDeadlineResult;
	bool bDeadlineDone;
	int64 iDeadlineElapsedMs;
	xcosched* pSeenSched;
} __test_co_event_case;

typedef struct {
	xcoevent pEvent;
	bool arrWaitResult[2];
	bool arrDone[2];
} __test_co_multi_event_case;

typedef struct {
	int arrOrder[8];
	int iCount;
	bool bCancelCleanupRan;
} __test_co_cleanup_case;

typedef struct {
	int iValue;
} __test_co_result_case;

static void __test_co_cleanup_record1(ptr pArg)
{
	__test_co_cleanup_case* pCase = (__test_co_cleanup_case*)pArg;
	if ( pCase && pCase->iCount < 8 ) {
		pCase->arrOrder[pCase->iCount++] = 1;
	}
}

static void __test_co_cleanup_record2(ptr pArg)
{
	__test_co_cleanup_case* pCase = (__test_co_cleanup_case*)pArg;
	if ( pCase && pCase->iCount < 8 ) {
		pCase->arrOrder[pCase->iCount++] = 2;
	}
}

static void __test_co_cleanup_record3(ptr pArg)
{
	__test_co_cleanup_case* pCase = (__test_co_cleanup_case*)pArg;
	if ( pCase && pCase->iCount < 8 ) {
		pCase->arrOrder[pCase->iCount++] = 3;
	}
}

static void __test_co_cleanup_cancel_mark(ptr pArg)
{
	__test_co_cleanup_case* pCase = (__test_co_cleanup_case*)pArg;
	if ( pCase ) {
		pCase->bCancelCleanupRan = TRUE;
	}
}

static uint32 __test_co_deep_stack_walk(__test_co_deep_stack_case* pCase, int iDepth, uint32 iSeed)
{
	volatile uint32 arrMarker[16];
	uint32 iChecksum = 0;
	int i = 0;

	for ( i = 0; i < 16; ++i ) {
		arrMarker[i] = (uint32)(iSeed + (uint32)(iDepth * 131) + (uint32)(i * 17));
		iChecksum ^= arrMarker[i] + (uint32)(i * 31);
	}

	if ( iDepth == 0 ) {
		pCase->iLeafChecksum = iChecksum;
		xrtCoYield();
	} else {
		iChecksum ^= __test_co_deep_stack_walk(pCase, iDepth - 1, iSeed + 97u);
	}

	for ( i = 0; i < 16; ++i ) {
		uint32 iExpect = (uint32)(iSeed + (uint32)(iDepth * 131) + (uint32)(i * 17));
		if ( arrMarker[i] != iExpect ) {
			pCase->bPass = FALSE;
		}
	}

	pCase->iFramesChecked++;
	return iChecksum ^ (uint32)(iDepth * 257);
}

static void __test_co_deep_stack_func(ptr pParam)
{
	__test_co_deep_stack_case* pCase = (__test_co_deep_stack_case*)pParam;
	uint32 iValue = __test_co_deep_stack_walk(pCase, 48, 0x13572468u);

	if ( pCase ) {
		pCase->iFinalChecksum = iValue;
		pCase->bPass = pCase->bPass && (iValue == 14384u);
	}
}

static void __test_co_exit_func(ptr pParam)
{
	__test_co_exit_case* pCase = (__test_co_exit_case*)pParam;

	if ( pCase ) {
		pCase->iBeforeExit = 7;
	}

	xrtCoExit(1234);

	if ( pCase ) {
		pCase->bAfterExit = TRUE;
	}
}

static void __test_co_sleep_until_func(ptr pParam)
{
	__test_co_deadline_case* pCase = (__test_co_deadline_case*)pParam;

	if ( pCase == NULL ) {
		return;
	}

	pCase->iStartMs = (int64)(xrtTimer() * 1000.0);
	xrtCoSleepUntil(pCase->iStartMs + 60);
	pCase->iEndMs = (int64)(xrtTimer() * 1000.0);
	pCase->iElapsedMs = pCase->iEndMs - pCase->iStartMs;
	pCase->bDone = TRUE;
}

static void __test_co_createex_func(ptr pParam)
{
	__test_co_createex_case* pCase = (__test_co_createex_case*)pParam;

	if ( pCase ) {
		pCase->iCounter++;
	}
}

static void __test_co_waitdeadline_target(ptr pParam)
{
	__test_co_waitdeadline_case* pCase = (__test_co_waitdeadline_case*)pParam;
	int64 iStartMs = (int64)(xrtTimer() * 1000.0);

	if ( pCase == NULL ) {
		return;
	}

	pCase->bWaitResult = xrtCoWaitDeadline(iStartMs + 1000);
	pCase->bCancelledObserved = xrtCoIsCancelRequested();
}

static void __test_co_waitdeadline_canceller(ptr pParam)
{
	__test_co_waitdeadline_case* pCase = (__test_co_waitdeadline_case*)pParam;

	if ( pCase == NULL || pCase->pTarget == NULL ) {
		return;
	}

	xrtCoSleep(50);
	(void)xrtCoCancel(pCase->pTarget);
}

static void __test_co_event_waiter(ptr pParam)
{
	__test_co_event_case* pCase = (__test_co_event_case*)pParam;

	if ( pCase == NULL ) {
		return;
	}

	pCase->pSeenSched = xrtCoSchedCurrent();
	pCase->bWaitResult = xrtCoWaitEvent(pCase->pEvent);
	pCase->bDone = TRUE;
}

static void __test_co_event_immediate_waiter(ptr pParam)
{
	__test_co_event_case* pCase = (__test_co_event_case*)pParam;

	if ( pCase == NULL ) {
		return;
	}

	pCase->bImmediateResult = xrtCoWaitEvent(pCase->pEvent);
	pCase->bImmediateDone = TRUE;
}

static void __test_co_event_timeout_waiter(ptr pParam)
{
	__test_co_event_case* pCase = (__test_co_event_case*)pParam;
	int64 iStartMs = 0;
	int64 iEndMs = 0;

	if ( pCase == NULL ) {
		return;
	}

	iStartMs = (int64)(xrtTimer() * 1000.0);
	pCase->bTimeoutResult = xrtCoWaitEventTimeout(pCase->pEvent, 40);
	iEndMs = (int64)(xrtTimer() * 1000.0);
	pCase->iTimeoutElapsedMs = iEndMs - iStartMs;
	pCase->bTimeoutDone = TRUE;
}

static void __test_co_event_deadline_waiter(ptr pParam)
{
	__test_co_event_case* pCase = (__test_co_event_case*)pParam;
	int64 iStartMs = 0;
	int64 iEndMs = 0;

	if ( pCase == NULL ) {
		return;
	}

	iStartMs = (int64)(xrtTimer() * 1000.0);
	pCase->bDeadlineResult = xrtCoWaitEventUntil(pCase->pEvent, iStartMs + 45);
	iEndMs = (int64)(xrtTimer() * 1000.0);
	pCase->iDeadlineElapsedMs = iEndMs - iStartMs;
	pCase->bDeadlineDone = TRUE;
}

static uint32 __test_co_event_set_worker(ptr pParam)
{
	__test_co_event_case* pCase = (__test_co_event_case*)pParam;

	#if defined(_WIN32) || defined(_WIN64)
		Sleep(50);
	#else
		struct timespec ts;
		ts.tv_sec = 0;
		ts.tv_nsec = 50 * 1000000L;
		nanosleep(&ts, NULL);
	#endif

	if ( pCase && pCase->pEvent ) {
		xrtCoEventSet(pCase->pEvent);
		pCase->bSetDone = TRUE;
	}
	return 0;
}

static void __test_co_multi_event_waiter_1(ptr pParam)
{
	__test_co_multi_event_case* pCase = (__test_co_multi_event_case*)pParam;
	if ( pCase == NULL ) {
		return;
	}
	pCase->arrWaitResult[0] = xrtCoWaitEvent(pCase->pEvent);
	pCase->arrDone[0] = TRUE;
}

static void __test_co_multi_event_waiter_2(ptr pParam)
{
	__test_co_multi_event_case* pCase = (__test_co_multi_event_case*)pParam;
	if ( pCase == NULL ) {
		return;
	}
	pCase->arrWaitResult[1] = xrtCoWaitEvent(pCase->pEvent);
	pCase->arrDone[1] = TRUE;
}

static uint32 __test_co_multi_event_set_worker(ptr pParam)
{
	__test_co_multi_event_case* pCase = (__test_co_multi_event_case*)pParam;

	#if defined(_WIN32) || defined(_WIN64)
		Sleep(50);
	#else
		struct timespec ts;
		ts.tv_sec = 0;
		ts.tv_nsec = 50 * 1000000L;
		nanosleep(&ts, NULL);
	#endif

	if ( pCase && pCase->pEvent ) {
		xrtCoEventSet(pCase->pEvent);
	}
	return 0;
}

static void __test_co_cleanup_func(ptr pParam)
{
	__test_co_cleanup_case* pCase = (__test_co_cleanup_case*)pParam;

	(void)xrtCoPushCleanup(__test_co_cleanup_record1, pCase);
	(void)xrtCoPushCleanup(__test_co_cleanup_record2, pCase);
	(void)xrtCoPopCleanup(__test_co_cleanup_record2, pCase, FALSE);
	(void)xrtCoPushCleanup(__test_co_cleanup_record3, pCase);
	xrtCoExit(77);
}

static void __test_co_cleanup_cancel_worker(ptr pParam)
{
	__test_co_cleanup_case* pCase = (__test_co_cleanup_case*)pParam;

	(void)xrtCoPushCleanup(__test_co_cleanup_cancel_mark, pCase);
	while ( !xrtCoIsCancelRequested() ) {
		xrtCoSleep(1000);
	}
}

static void __test_co_result_func(ptr pParam)
{
	__test_co_result_case* pCase = (__test_co_result_case*)pParam;

	xrtCoSetResult(pCase);
	xrtCoExit(88);
}

static void __test_co_post_target(ptr pParam)
{
	__test_co_post_case* pCase = (__test_co_post_case*)pParam;
	pCase->pSeenSched = xrtCoSchedCurrent();
	xrtCoSleep(1000);
	pCase->bWoke = TRUE;
}

static uint32 __test_co_post_worker(ptr pParam)
{
	__test_co_post_case* pCase = (__test_co_post_case*)pParam;

	#if defined(_WIN32) || defined(_WIN64)
		Sleep(50);
	#else
		struct timespec ts;
		ts.tv_sec = 0;
		ts.tv_nsec = 50 * 1000000L;
		nanosleep(&ts, NULL);
	#endif

	pCase->bPosted = xrtCoSchedPost(pCase->pSched, pCase->pTarget);
	return 0;
}

#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__) && defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
static double __test_co_xmm6_after[2] = {0, 0};

static void __test_co_xmm6_func(ptr pParam)
{
	static const double arrLoad[2] = {123.25, -456.5};

	(void)pParam;
	__asm__ volatile (
		"movupd %0, %%xmm6"
		:
		: "m"(arrLoad[0])
		: "xmm6", "memory"
	);

	xrtCoYield();

	__asm__ volatile (
		"movupd %%xmm6, %0"
		: "=m"(__test_co_xmm6_after[0])
		:
		: "memory"
	);
}
#endif

typedef struct {
	xcoro pCo;
	xcosched* pSched;
	bool bResumeResult;
	bool bSpawnBlocked;
	char sResumeError[128];
	char sSpawnError[128];
	char sDestroyError[128];
} __test_co_guard_case;

static void __test_co_copy_error(char* sDest, size_t iDestSize)
{
	str sError = xrtGetError();

	if ( sDest == NULL || iDestSize == 0 ) {
		return;
	}

	if ( sError == NULL ) {
		sDest[0] = 0;
		return;
	}

	snprintf(sDest, iDestSize, "%s", sError);
}

static uint32 __test_co_guard_worker(ptr pParam)
{
	__test_co_guard_case* pCase = (__test_co_guard_case*)pParam;

	xrtClearError();
	pCase->bResumeResult = xrtCoResume(pCase->pCo);
	__test_co_copy_error(pCase->sResumeError, sizeof(pCase->sResumeError));

	xrtClearError();
	pCase->bSpawnBlocked = xrtCoSchedSpawn(pCase->pSched, __test_sched_func, (ptr)(intptr)3, 0) == NULL;
	__test_co_copy_error(pCase->sSpawnError, sizeof(pCase->sSpawnError));

	xrtClearError();
	xrtCoDestroy(pCase->pCo);
	__test_co_copy_error(pCase->sDestroyError, sizeof(pCase->sDestroyError));
	return 0;
}



/* ================================ 测试主函数 ================================ */

void Test_Coroutine(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n Coroutine 协程库测试 :\n\n");
	
	// 测试 0: backend introspection
	printf("[Test 0] backend introspection:\n");
	{
		str sBackend = xrtCoGetBackendName();
		const char* sBackendText = NULL;
		int iTier = xrtCoGetBackendTier();
		int iStyle = xrtCoGetBackendStyle();
		bool bTier = (iTier == XRT_CO_BACKEND_TIER_PRODUCTION);
		bool bStyle = (iStyle == XRT_CO_BACKEND_STYLE_INLINE_ASM);
		bool bName = (sBackend != NULL) && (sBackend[0] != 0);
		sBackendText = bName ? (const char*)sBackend : "(null)";

		printf("  backend 名称: %s  %s\n", sBackendText, bName ? "PASS" : "FAIL");
		printf("  backend tier: %d  %s\n", iTier, bTier ? "PASS" : "FAIL");
		printf("  backend style: %d  %s\n\n", iStyle, bStyle ? "PASS" : "FAIL");
	}
	
	
	// 测试 1: 基础创建销毁
	printf("[Test 1] 基础创建销毁:\n");
	{
		xcoro pCo = xrtCoCreate(__test_co_basic_func, NULL, 0);
		if ( pCo ) {
			printf("  创建协程: %s\n", xrtCoGetState(pCo) == XRT_CO_READY ? "PASS" : "FAIL");
			xrtCoDestroy(pCo);
			printf("  销毁协程: PASS\n");
		} else {
			printf("  创建协程失败: FAIL\n");
		}
	}
	
	
	// 测试 2: 单协程 resume/yield
	printf("\n[Test 2] 单协程 resume/yield:\n");
	{
		int iCounter = 0;
		__test_co_step = 0;
		xcoro pCo = xrtCoCreate(__test_co_basic_func, &iCounter, 0);
		
		printf("  初始状态: %s\n", xrtCoGetState(pCo) == XRT_CO_READY ? "PASS" : "FAIL");
		
		// 第一次 resume: 执行到第一个 yield
		xrtCoResume(pCo);
		printf("  第1次resume后: step=%d, counter=%d, state=%s  %s\n",
			__test_co_step, iCounter,
			xrtCoGetState(pCo) == XRT_CO_SUSPENDED ? "SUSPENDED" : "???",
			(__test_co_step == 1 && iCounter == 1) ? "PASS" : "FAIL");
		
		// 第二次 resume
		xrtCoResume(pCo);
		printf("  第2次resume后: step=%d, counter=%d  %s\n",
			__test_co_step, iCounter,
			(__test_co_step == 2 && iCounter == 2) ? "PASS" : "FAIL");
		
		// 第三次 resume: 函数返回，协程 DEAD
		xrtCoResume(pCo);
		printf("  第3次resume后: step=%d, counter=%d, state=%s  %s\n",
			__test_co_step, iCounter,
			xrtCoGetState(pCo) == XRT_CO_DEAD ? "DEAD" : "???",
			(__test_co_step == 3 && iCounter == 3 && xrtCoGetState(pCo) == XRT_CO_DEAD) ? "PASS" : "FAIL");
		
		// 再次 resume DEAD 协程应返回 false
		bool bResult = xrtCoResume(pCo);
		printf("  Resume DEAD协程: %s\n", !bResult ? "PASS" : "FAIL");
		
		xrtCoDestroy(pCo);
	}
	
	
	// 测试 3: 多协程交替
	printf("\n[Test 3] 多协程交替执行:\n");
	{
		__test_co_order_idx = 0;
		xcoro pCoA = xrtCoCreate(__test_co_a_func, NULL, 0);
		xcoro pCoB = xrtCoCreate(__test_co_b_func, NULL, 0);
		
		// 交替执行: A → B → A → B → A → B
		xrtCoResume(pCoA);  // A记录1, yield
		xrtCoResume(pCoB);  // B记录2, yield
		xrtCoResume(pCoA);  // A记录3, yield
		xrtCoResume(pCoB);  // B记录4, yield
		xrtCoResume(pCoA);  // A记录5, 结束
		xrtCoResume(pCoB);  // B记录6, 结束
		
		bool bPass = (__test_co_order_idx == 6);
		for ( int i = 0; i < 6; i++ ) {
			if ( __test_co_order[i] != i + 1 ) bPass = false;
		}
		printf("  执行顺序 [");
		for ( int i = 0; i < __test_co_order_idx; i++ ) {
			printf("%d%s", __test_co_order[i], i < __test_co_order_idx - 1 ? "," : "");
		}
		printf("]: %s\n", bPass ? "PASS" : "FAIL");
		printf("  A状态: %s, B状态: %s\n",
			xrtCoGetState(pCoA) == XRT_CO_DEAD ? "DEAD" : "???",
			xrtCoGetState(pCoB) == XRT_CO_DEAD ? "DEAD" : "???");
		
		xrtCoDestroy(pCoA);
		xrtCoDestroy(pCoB);
	}
	
	
	// 测试 4: 协程参数传递
	printf("\n[Test 4] 协程参数传递:\n");
	{
		int iVal = 42;
		__test_co_step = 0;
		xcoro pCo = xrtCoCreate(__test_co_basic_func, &iVal, 0);
		
		xrtCoResume(pCo);
		printf("  参数传递 (42→43): %s\n", iVal == 43 ? "PASS" : "FAIL");
		
		// 运行完
		while ( xrtCoGetState(pCo) != XRT_CO_DEAD ) xrtCoResume(pCo);
		printf("  最终值: %d  %s\n", iVal, iVal == 45 ? "PASS" : "FAIL");
		xrtCoDestroy(pCo);
	}
	
	
	// 测试 5: UserData
	printf("\n[Test 5] UserData:\n");
	{
		xcoro pCo = xrtCoCreate(__test_co_basic_func, NULL, 0);
		
		int iData = 999;
		xrtCoSetUserData(pCo, &iData);
		int* pGot = (int*)xrtCoGetUserData(pCo);
		printf("  UserData 设置/获取: %s\n", (pGot == &iData && *pGot == 999) ? "PASS" : "FAIL");
		
		xrtCoDestroy(pCo);
	}
	
	
	// 测试 6: GetCurrent
	printf("\n[Test 6] GetCurrent:\n");
	{
		__test_co_got_current = NULL;
		
		// 主上下文中应返回 NULL
		xcoro pMain = xrtCoGetCurrent();
		printf("  主上下文 GetCurrent: %s\n", pMain == NULL ? "PASS" : "FAIL");
		
		xcoro pCo = xrtCoCreate(__test_co_current_func, NULL, 0);
		xrtCoResume(pCo);
		printf("  协程内 GetCurrent: %s\n", __test_co_got_current == pCo ? "PASS" : "FAIL");
		
		// Resume 返回后应为 NULL
		xcoro pAfter = xrtCoGetCurrent();
		printf("  Resume返回后 GetCurrent: %s\n", pAfter == NULL ? "PASS" : "FAIL");
		
		xrtCoDestroy(pCo);
	}
	
	
	// 测试 7: 调度器基础
	printf("\n[Test 7] 调度器基础:\n");
	{
		memset(__test_sched_done, 0, sizeof(__test_sched_done));
		
		xcosched* pSched = xrtCoSchedCreate();
		xrtCoSchedSpawn(pSched, __test_sched_func, (ptr)(intptr)0, 0);
		xrtCoSchedSpawn(pSched, __test_sched_func, (ptr)(intptr)1, 0);
		xrtCoSchedSpawn(pSched, __test_sched_func, (ptr)(intptr)2, 0);
		
		printf("  创建调度器+3个协程: alive=%d  %s\n",
			xrtCoSchedGetAlive(pSched),
			xrtCoSchedGetAlive(pSched) == 3 ? "PASS" : "FAIL");
		
		xrtCoSchedRun(pSched);
		
		bool bAllDone = __test_sched_done[0] && __test_sched_done[1] && __test_sched_done[2];
		printf("  SchedRun完成: alive=%d, 全部完成=%s  %s\n",
			xrtCoSchedGetAlive(pSched),
			bAllDone ? "是" : "否",
			(xrtCoSchedGetAlive(pSched) == 0 && bAllDone) ? "PASS" : "FAIL");
		
		xrtCoSchedDestroy(pSched);
	}
	
	
	// 测试 8: CoSleep
	printf("\n[Test 8] CoSleep:\n");
	{
		__test_sleep_idx = 0;
		
		xcosched* pSched = xrtCoSchedCreate();
		xrtCoSchedSpawn(pSched, __test_sleep_slow, NULL, 0);  // 先 spawn 慢的
		xrtCoSchedSpawn(pSched, __test_sleep_fast, NULL, 0);  // 后 spawn 快的
		
		int64 iStart = 0;
		#if defined(_WIN32) || defined(_WIN64)
			iStart = (int64)GetTickCount64();
		#else
			struct timespec ts;
			clock_gettime(CLOCK_MONOTONIC, &ts);
			iStart = (int64)ts.tv_sec * 1000 + (int64)ts.tv_nsec / 1000000;
		#endif
		
		xrtCoSchedRun(pSched);
		
		int64 iEnd = 0;
		#if defined(_WIN32) || defined(_WIN64)
			iEnd = (int64)GetTickCount64();
		#else
			clock_gettime(CLOCK_MONOTONIC, &ts);
			iEnd = (int64)ts.tv_sec * 1000 + (int64)ts.tv_nsec / 1000000;
		#endif
		
		int64 iElapsed = iEnd - iStart;
		
		// fast(50ms) 应该先完成，slow(150ms) 后完成
		bool bOrder = (__test_sleep_idx == 2) && (__test_sleep_order[0] == 1) && (__test_sleep_order[1] == 2);
		// 总耗时应大约 150ms (不应是 200ms, 因为是并发的)
		bool bTime = (iElapsed >= 100) && (iElapsed < 500);
		
		printf("  执行顺序: fast=%d, slow=%d  %s\n",
			__test_sleep_order[0], __test_sleep_order[1],
			bOrder ? "PASS" : "FAIL");
		printf("  总耗时: %lldms (预期~150ms)  %s\n",
			(long long)iElapsed, bTime ? "PASS" : "FAIL");
		
		xrtCoSchedDestroy(pSched);
	}

	// 测试 9: 线程归属保护
	printf("\n[Test 9] 线程归属保护:\n");
	{
		__test_co_guard_case tCase;
		memset(&tCase, 0, sizeof(tCase));

		tCase.pCo = xrtCoCreate(__test_co_basic_func, NULL, 0);
		tCase.pSched = xrtCoSchedCreate();
		xrtCoSchedSpawn(tCase.pSched, __test_sched_func, (ptr)(intptr)0, 0);

		xthread pThread = xrtThreadCreate(__test_co_guard_worker, &tCase, 0);
		xrtThreadWait(pThread);
		xrtThreadDestroy(pThread);

		printf("  外线程 Resume 拒绝: %s\n",
			(!tCase.bResumeResult && tCase.sResumeError[0] != 0) ? "PASS" : "FAIL");
		printf("  外线程 Spawn 拒绝: %s\n",
			(tCase.bSpawnBlocked && tCase.sSpawnError[0] != 0) ? "PASS" : "FAIL");
		printf("  外线程 Destroy 拒绝: %s\n",
			(tCase.sDestroyError[0] != 0) ? "PASS" : "FAIL");

		xrtCoDestroy(tCase.pCo);
		xrtCoSchedDestroy(tCase.pSched);
	}

	// 测试 10: 调度器持有销毁保护
	printf("\n[Test 10] 调度器持有销毁保护:\n");
	{
		xcosched* pSched = xrtCoSchedCreate();
		xcoro pCo = xrtCoSchedSpawn(pSched, __test_sched_func, (ptr)(intptr)0, 0);

		xrtClearError();
		xrtCoDestroy(pCo);

		printf("  销毁被调度器持有协程: %s\n",
			xrtGetError()[0] != 0 ? "PASS" : "FAIL");
		printf("  Alive 计数保持: %s\n",
			xrtCoSchedGetAlive(pSched) == 1 ? "PASS" : "FAIL");

		xrtCoSchedDestroy(pSched);
	}

	// 测试 11: Join 驱动 unscheduled 协程
	printf("\n[Test 11] Join 驱动 unscheduled 协程:\n");
	{
		int iValue = 0;
		xcoro pCo = xrtCoCreate(__test_co_basic_func, &iValue, 0);
		bool bJoin = xrtCoJoin(pCo);

		printf("  Join 返回: %s\n", bJoin ? "PASS" : "FAIL");
		printf("  Join 驱动执行到终态: %s\n",
			(bJoin && iValue == 3 && xrtCoGetState(pCo) == XRT_CO_DEAD) ? "PASS" : "FAIL");

		xrtCoDestroy(pCo);
	}

	// 测试 12: scheduler 内部 join
	printf("\n[Test 12] scheduler 内部 join:\n");
	{
		__test_co_lifecycle_case tCase;
		xcosched* pSched = NULL;
		bool bOrder = FALSE;

		memset(&tCase, 0, sizeof(tCase));
		pSched = xrtCoSchedCreate();
		tCase.pTarget = xrtCoSchedSpawn(pSched, __test_co_join_target, &tCase, 0);
		xrtCoSchedSpawn(pSched, __test_co_join_waiter, &tCase, 0);
		xrtCoSchedRun(pSched);

		bOrder =
			(tCase.iEventCount == 4) &&
			(tCase.arrEvents[0] == 1) &&
			(tCase.arrEvents[1] == 10) &&
			(tCase.arrEvents[2] == 2) &&
			(tCase.arrEvents[3] == 20);

		printf("  Join 顺序: %s\n", bOrder ? "PASS" : "FAIL");
		printf("  Waiter Join 成功: %s\n", tCase.bJoined ? "PASS" : "FAIL");

		xrtCoSchedDestroy(pSched);
	}

	printf("\n[Test 12A] scheduler 内部 multi-join:\n");
	{
		__test_co_multi_join_case tCase;
		xcosched* pSched = NULL;
		bool bOrder = FALSE;

		memset(&tCase, 0, sizeof(tCase));
		pSched = xrtCoSchedCreate();
		tCase.pTarget = xrtCoSchedSpawn(pSched, __test_co_multi_join_target, &tCase, 0);
		(void)xrtCoSchedSpawn(pSched, __test_co_multi_join_waiter_1, &tCase, 0);
		(void)xrtCoSchedSpawn(pSched, __test_co_multi_join_waiter_2, &tCase, 0);
		xrtCoSchedRun(pSched);

		bOrder =
			(tCase.iEventCount == 6) &&
			(tCase.arrEvents[0] == 1) &&
			(tCase.arrEvents[1] == 10) &&
			(tCase.arrEvents[2] == 11) &&
			(tCase.arrEvents[3] == 2) &&
			(tCase.arrEvents[4] == 20) &&
			(tCase.arrEvents[5] == 21);

		printf("  Multi-join 顺序: %s\n", bOrder ? "PASS" : "FAIL");
		printf("  Waiter #1 Join 成功: %s\n", tCase.arrJoined[0] ? "PASS" : "FAIL");
		printf("  Waiter #2 Join 成功: %s\n", tCase.arrJoined[1] ? "PASS" : "FAIL");

		xrtCoSchedDestroy(pSched);
	}

	// 测试 13: 协作式 cancel + close
	printf("\n[Test 13] 协作式 cancel + close:\n");
	{
		__test_co_lifecycle_case tCase;
		xcosched* pSched = NULL;
		xcoro pCo = NULL;
		bool bJoin = FALSE;
		bool bCancelled = FALSE;
		bool bClosed = FALSE;

		memset(&tCase, 0, sizeof(tCase));
		pSched = xrtCoSchedCreate();
		pCo = xrtCoSchedSpawn(pSched, __test_co_cancel_worker, &tCase, 0);

		xrtCoSchedStep(pSched);
		xrtCoCancel(pCo);
		bJoin = xrtCoJoin(pCo);
		bCancelled = xrtCoWasCancelled(pCo);
		bClosed = xrtCoClose(pCo);

		printf("  Cancel 请求成功: %s\n", bJoin ? "PASS" : "FAIL");
		printf("  协程观测到取消: %s\n", tCase.bCancelled ? "PASS" : "FAIL");
		printf("  终态记录为取消: %s\n", bCancelled ? "PASS" : "FAIL");
		printf("  Close 释放 dead handle: %s\n", bClosed ? "PASS" : "FAIL");

		xrtCoSchedDestroy(pSched);
	}

	// 测试 14: post queue 唤醒 + current scheduler
	printf("\n[Test 14] post queue 唤醒 + current scheduler:\n");
	{
		__test_co_post_case tCase;
		xcosched* pSched = NULL;
		xthread pThread = NULL;
		int64 iStart = 0;
		int64 iEnd = 0;
		int64 iElapsed = 0;
		bool bTime = FALSE;

		memset(&tCase, 0, sizeof(tCase));
		pSched = xrtCoSchedCreate();
		tCase.pSched = pSched;
		tCase.pTarget = xrtCoSchedSpawn(pSched, __test_co_post_target, &tCase, 0);
		pThread = xrtThreadCreate(__test_co_post_worker, &tCase, 0);

		#if defined(_WIN32) || defined(_WIN64)
			iStart = (int64)GetTickCount64();
		#else
			{
				struct timespec ts;
				clock_gettime(CLOCK_MONOTONIC, &ts);
				iStart = (int64)ts.tv_sec * 1000 + (int64)ts.tv_nsec / 1000000;
			}
		#endif

		xrtCoSchedRun(pSched);

		#if defined(_WIN32) || defined(_WIN64)
			iEnd = (int64)GetTickCount64();
		#else
			{
				struct timespec ts;
				clock_gettime(CLOCK_MONOTONIC, &ts);
				iEnd = (int64)ts.tv_sec * 1000 + (int64)ts.tv_nsec / 1000000;
			}
		#endif

		iElapsed = iEnd - iStart;
		bTime = (iElapsed >= 20) && (iElapsed < 500);

		xrtThreadWait(pThread);
		xrtThreadDestroy(pThread);

		printf("  Post 投递成功: %s\n", tCase.bPosted ? "PASS" : "FAIL");
		printf("  current scheduler 正确: %s\n", tCase.pSeenSched == pSched ? "PASS" : "FAIL");
		printf("  post 唤醒提前生效: %s (%lldms)\n", bTime ? "PASS" : "FAIL", (long long)iElapsed);

		xrtCoSchedDestroy(pSched);
	}

	// 测试 15: 深栈跨 yield 保持
	printf("\n[Test 15] 深栈跨 yield 保持:\n");
	{
		__test_co_deep_stack_case tCase;
		xcoro pCo = NULL;
		bool bPass = FALSE;
		int iStateAfterFirst = 0;

		memset(&tCase, 0, sizeof(tCase));
		tCase.bPass = TRUE;
		pCo = xrtCoCreate(__test_co_deep_stack_func, &tCase, 128 * 1024);

		xrtCoResume(pCo);
		iStateAfterFirst = xrtCoGetState(pCo);
		xrtCoResume(pCo);

		bPass =
			tCase.bPass &&
			(iStateAfterFirst == XRT_CO_SUSPENDED) &&
			(tCase.iFramesChecked == 49) &&
			(tCase.iFinalChecksum == 14384u) &&
			(xrtCoGetState(pCo) == XRT_CO_DEAD);

		printf("  深栈局部帧保持: %s\n", bPass ? "PASS" : "FAIL");
		printf("  第一次恢复后挂起: %s\n", iStateAfterFirst == XRT_CO_SUSPENDED ? "PASS" : "FAIL");
		printf("  检查帧数: %d  %s\n", tCase.iFramesChecked, tCase.iFramesChecked == 49 ? "PASS" : "FAIL");

		xrtCoDestroy(pCo);
	}

	// 测试 16: xrtCoExit + 退出码
	printf("\n[Test 16] xrtCoExit + 退出码:\n");
	{
		__test_co_exit_case tCase;
		xcoro pCo = NULL;
		bool bPass = FALSE;

		memset(&tCase, 0, sizeof(tCase));
		pCo = xrtCoCreate(__test_co_exit_func, &tCase, 0);
		xrtCoResume(pCo);

		bPass =
			(tCase.iBeforeExit == 7) &&
			(!tCase.bAfterExit) &&
			(xrtCoGetState(pCo) == XRT_CO_DEAD) &&
			(xrtCoGetExitCode(pCo) == 1234);

		printf("  Exit 立即终止后续代码: %s\n", (!tCase.bAfterExit) ? "PASS" : "FAIL");
		printf("  退出码记录正确: %s\n", xrtCoGetExitCode(pCo) == 1234 ? "PASS" : "FAIL");
		printf("  协程进入 DEAD: %s\n", bPass ? "PASS" : "FAIL");

		xrtCoDestroy(pCo);
	}

	// 测试 17: xrtCoSleepUntil deadline
	printf("\n[Test 17] xrtCoSleepUntil deadline:\n");
	{
		__test_co_deadline_case tCase;
		xcosched* pSched = NULL;
		bool bTime = FALSE;

		memset(&tCase, 0, sizeof(tCase));
		pSched = xrtCoSchedCreate();
		xrtCoSchedSpawn(pSched, __test_co_sleep_until_func, &tCase, 0);
		xrtCoSchedRun(pSched);

		bTime = (tCase.iElapsedMs >= 40) && (tCase.iElapsedMs < 500);

		printf("  deadline 睡眠完成: %s\n", tCase.bDone ? "PASS" : "FAIL");
		printf("  deadline 耗时: %lldms  %s\n", (long long)tCase.iElapsedMs, bTime ? "PASS" : "FAIL");

		xrtCoSchedDestroy(pSched);
	}

	// 测试 18: xrtCoCreateEx
	printf("\n[Test 18] xrtCoWaitDeadline + cancel:\n");
	{
		__test_co_waitdeadline_case tCase;
		xcosched* pSched = NULL;
		bool bJoined = FALSE;
		bool bPass = FALSE;

		memset(&tCase, 0, sizeof(tCase));
		pSched = xrtCoSchedCreate();
		tCase.pTarget = xrtCoSchedSpawn(pSched, __test_co_waitdeadline_target, &tCase, 0);
		xrtCoSchedSpawn(pSched, __test_co_waitdeadline_canceller, &tCase, 0);
		bJoined = xrtCoJoin(tCase.pTarget);

		bPass =
			bJoined &&
			(!tCase.bWaitResult) &&
			tCase.bCancelledObserved &&
			xrtCoWasCancelled(tCase.pTarget);

		printf("  Join 驱动目标终态: %s\n", bJoined ? "PASS" : "FAIL");
		printf("  WaitDeadline 被取消返回 false: %s\n", !tCase.bWaitResult ? "PASS" : "FAIL");
		printf("  协程观测到取消: %s\n", tCase.bCancelledObserved ? "PASS" : "FAIL");
		printf("  终态记录为取消: %s\n", xrtCoWasCancelled(tCase.pTarget) ? "PASS" : "FAIL");
		printf("  综合结果: %s\n", bPass ? "PASS" : "FAIL");

		xrtCoSchedDestroy(pSched);
	}

	// 测试 19: xrtCoCreateEx
	printf("\n[Test 19] xrtCoCreateEx:\n");
	{
		__test_co_createex_case tCase;
		xco_create_args tArgs;
		xcoro pCo = NULL;
		int iUserData = 2468;
		bool bPass = FALSE;

		memset(&tCase, 0, sizeof(tCase));
		memset(&tArgs, 0, sizeof(tArgs));
		tArgs.iStackSize = 96 * 1024;
		tArgs.pUserData = &iUserData;

		pCo = xrtCoCreateEx(__test_co_createex_func, &tCase, &tArgs);
		if ( pCo ) {
			xrtCoResume(pCo);
		}

		bPass =
			(pCo != NULL) &&
			(tCase.iCounter == 1) &&
			(xrtCoGetState(pCo) == XRT_CO_DEAD) &&
			(xrtCoGetUserData(pCo) == &iUserData);

		printf("  CreateEx 默认路径: %s\n", bPass ? "PASS" : "FAIL");
		printf("  初始 UserData 注入: %s\n", xrtCoGetUserData(pCo) == &iUserData ? "PASS" : "FAIL");

		xrtCoDestroy(pCo);

		memset(&tArgs, 0, sizeof(tArgs));
		tArgs.iFlags = 1;
		xrtClearError();
		pCo = xrtCoCreateEx(__test_co_createex_func, &tCase, &tArgs);
		printf("  未知 flags 拒绝: %s\n", (pCo == NULL && xrtGetError()[0] != 0) ? "PASS" : "FAIL");
	}

	printf("\n[Test 20] xrtCoWaitEvent:\n");
	{
		__test_co_event_case tCase;
		xcosched* pSched = NULL;
		xthread pThread = NULL;
		int64 iStart = 0;
		int64 iEnd = 0;
		int64 iElapsed = 0;
		bool bTime = FALSE;

		memset(&tCase, 0, sizeof(tCase));
		pSched = xrtCoSchedCreate();
		tCase.pEvent = xrtCoEventCreate(FALSE, FALSE);
		xrtCoSchedSpawn(pSched, __test_co_event_waiter, &tCase, 0);
		pThread = xrtThreadCreate(__test_co_event_set_worker, &tCase, 0);

		#if defined(_WIN32) || defined(_WIN64)
			iStart = (int64)GetTickCount64();
		#else
			{
				struct timespec ts;
				clock_gettime(CLOCK_MONOTONIC, &ts);
				iStart = (int64)ts.tv_sec * 1000 + (int64)ts.tv_nsec / 1000000;
			}
		#endif

		xrtCoSchedRun(pSched);

		#if defined(_WIN32) || defined(_WIN64)
			iEnd = (int64)GetTickCount64();
		#else
			{
				struct timespec ts;
				clock_gettime(CLOCK_MONOTONIC, &ts);
				iEnd = (int64)ts.tv_sec * 1000 + (int64)ts.tv_nsec / 1000000;
			}
		#endif

		iElapsed = iEnd - iStart;
		bTime = (iElapsed >= 20) && (iElapsed < 500);

		xrtThreadWait(pThread);
		xrtThreadDestroy(pThread);

		printf("  event 跨线程 set: %s\n", tCase.bSetDone ? "PASS" : "FAIL");
		printf("  waiter 返回 true: %s\n", tCase.bWaitResult ? "PASS" : "FAIL");
		printf("  current scheduler 正确: %s\n", tCase.pSeenSched == pSched ? "PASS" : "FAIL");
		printf("  event 唤醒耗时: %s (%lldms)\n", bTime ? "PASS" : "FAIL", (long long)iElapsed);

		xrtCoEventDestroy(tCase.pEvent);
		xrtCoSchedDestroy(pSched);
	}

	printf("\n[Test 21] manual-reset 预置位 event:\n");
	{
		__test_co_event_case tCase;
		xcosched* pSched = NULL;

		memset(&tCase, 0, sizeof(tCase));
		pSched = xrtCoSchedCreate();
		tCase.pEvent = xrtCoEventCreate(TRUE, TRUE);
		xrtCoSchedSpawn(pSched, __test_co_event_immediate_waiter, &tCase, 0);
		xrtCoSchedRun(pSched);

		printf("  预置位立即返回: %s\n", (tCase.bImmediateDone && tCase.bImmediateResult) ? "PASS" : "FAIL");

		xrtCoEventDestroy(tCase.pEvent);
		xrtCoSchedDestroy(pSched);
	}

	printf("\n[Test 21A] manual-reset multi-waiter event:\n");
	{
		__test_co_multi_event_case tCase;
		xcosched* pSched = NULL;
		xthread pThread = NULL;

		memset(&tCase, 0, sizeof(tCase));
		pSched = xrtCoSchedCreate();
		tCase.pEvent = xrtCoEventCreate(TRUE, FALSE);
		(void)xrtCoSchedSpawn(pSched, __test_co_multi_event_waiter_1, &tCase, 0);
		(void)xrtCoSchedSpawn(pSched, __test_co_multi_event_waiter_2, &tCase, 0);
		pThread = xrtThreadCreate(__test_co_multi_event_set_worker, &tCase, 0);
		xrtCoSchedRun(pSched);

		if ( pThread ) {
			xrtThreadWait(pThread);
			xrtThreadDestroy(pThread);
		}

		printf("  Multi-event waiter #1 : %s\n", (tCase.arrDone[0] && tCase.arrWaitResult[0]) ? "PASS" : "FAIL");
		printf("  Multi-event waiter #2 : %s\n", (tCase.arrDone[1] && tCase.arrWaitResult[1]) ? "PASS" : "FAIL");

		xrtCoEventDestroy(tCase.pEvent);
		xrtCoSchedDestroy(pSched);
	}

	printf("\n[Test 22] xrtCoWaitEventTimeout:\n");
	{
		__test_co_event_case tCase;
		xcosched* pSched = NULL;
		bool bTime = FALSE;

		memset(&tCase, 0, sizeof(tCase));
		pSched = xrtCoSchedCreate();
		tCase.pEvent = xrtCoEventCreate(FALSE, FALSE);
		xrtCoSchedSpawn(pSched, __test_co_event_timeout_waiter, &tCase, 0);
		xrtCoSchedRun(pSched);

		bTime = (tCase.iTimeoutElapsedMs >= 20) && (tCase.iTimeoutElapsedMs < 500);

		printf("  timeout 返回 false: %s\n", (!tCase.bTimeoutResult && tCase.bTimeoutDone) ? "PASS" : "FAIL");
		printf("  timeout 耗时: %lldms  %s\n", (long long)tCase.iTimeoutElapsedMs, bTime ? "PASS" : "FAIL");

		xrtCoEventDestroy(tCase.pEvent);
		xrtCoSchedDestroy(pSched);
	}

	printf("\n[Test 23] xrtCoWaitEventUntil:\n");
	{
		__test_co_event_case tCase;
		xcosched* pSched = NULL;
		bool bTime = FALSE;

		memset(&tCase, 0, sizeof(tCase));
		pSched = xrtCoSchedCreate();
		tCase.pEvent = xrtCoEventCreate(FALSE, FALSE);
		xrtCoSchedSpawn(pSched, __test_co_event_deadline_waiter, &tCase, 0);
		xrtCoSchedRun(pSched);

		bTime = (tCase.iDeadlineElapsedMs >= 20) && (tCase.iDeadlineElapsedMs < 500);

		printf("  deadline 返回 false: %s\n", (!tCase.bDeadlineResult && tCase.bDeadlineDone) ? "PASS" : "FAIL");
		printf("  deadline 耗时: %lldms  %s\n", (long long)tCase.iDeadlineElapsedMs, bTime ? "PASS" : "FAIL");

		xrtCoEventDestroy(tCase.pEvent);
		xrtCoSchedDestroy(pSched);
	}

	printf("\n[Test 24] xrtCoCleanup push/pop:\n");
	{
		__test_co_cleanup_case tCase;
		xcoro pCo = NULL;
		bool bOrder = FALSE;

		memset(&tCase, 0, sizeof(tCase));
		pCo = xrtCoCreate(__test_co_cleanup_func, &tCase, 0);
		xrtCoResume(pCo);

		bOrder =
			(xrtCoGetState(pCo) == XRT_CO_DEAD) &&
			(xrtCoGetExitCode(pCo) == 77) &&
			(tCase.iCount == 2) &&
			(tCase.arrOrder[0] == 3) &&
			(tCase.arrOrder[1] == 1);

		printf("  cleanup LIFO 顺序: %s\n", bOrder ? "PASS" : "FAIL");
		printf("  被 pop 的 cleanup 不执行: %s\n",
			(tCase.iCount == 2 && tCase.arrOrder[0] == 3 && tCase.arrOrder[1] == 1) ? "PASS" : "FAIL");

		xrtCoDestroy(pCo);
	}

	printf("\n[Test 25] cancel 路径 cleanup:\n");
	{
		__test_co_cleanup_case tCase;
		xcosched* pSched = NULL;
		xcoro pCo = NULL;
		bool bJoined = FALSE;

		memset(&tCase, 0, sizeof(tCase));
		pSched = xrtCoSchedCreate();
		pCo = xrtCoSchedSpawn(pSched, __test_co_cleanup_cancel_worker, &tCase, 0);
		xrtCoSchedStep(pSched);
		(void)xrtCoCancel(pCo);
		bJoined = xrtCoJoin(pCo);

		printf("  cancel 后 cleanup 执行: %s\n", (bJoined && tCase.bCancelCleanupRan) ? "PASS" : "FAIL");
		printf("  终态记录为取消: %s\n", xrtCoWasCancelled(pCo) ? "PASS" : "FAIL");

		xrtCoSchedDestroy(pSched);
	}

	printf("\n[Test 26] xrtCoResult:\n");
	{
		__test_co_result_case tCase;
		xcoro pCo = NULL;
		bool bPass = FALSE;

		memset(&tCase, 0, sizeof(tCase));
		tCase.iValue = 4321;
		pCo = xrtCoCreate(__test_co_result_func, &tCase, 0);
		xrtCoResume(pCo);

		bPass =
			(xrtCoGetState(pCo) == XRT_CO_DEAD) &&
			(xrtCoGetExitCode(pCo) == 88) &&
			(xrtCoGetResult(pCo) == &tCase);

		printf("  result 指针记录: %s\n", xrtCoGetResult(pCo) == &tCase ? "PASS" : "FAIL");
		printf("  result + exit code 协同: %s\n", bPass ? "PASS" : "FAIL");

		xrtCoDestroy(pCo);
	}

	#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__) && defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
	printf("\n[Test 27] Win64 XMM 非易失寄存器保持:\n");
	{
		xcoro pCo = NULL;
		bool bPass = false;

		__test_co_xmm6_after[0] = 0;
		__test_co_xmm6_after[1] = 0;
		pCo = xrtCoCreate(__test_co_xmm6_func, NULL, 0);

		xrtCoResume(pCo);
		xrtCoResume(pCo);

		bPass =
			(__test_co_xmm6_after[0] == 123.25) &&
			(__test_co_xmm6_after[1] == -456.5) &&
			(xrtCoGetState(pCo) == XRT_CO_DEAD);

		printf("  xmm6 跨 yield 保持: %s\n", bPass ? "PASS" : "FAIL");

		xrtCoDestroy(pCo);
	}
	#endif

	
	printf("\n Coroutine 协程库测试完成\n\n");
}


