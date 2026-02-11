



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



/* ================================ 测试主函数 ================================ */

void Test_Coroutine(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n Coroutine 协程库测试 :\n\n");
	
	
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
	
	
	printf("\n Coroutine 协程库测试完成\n\n");
}


