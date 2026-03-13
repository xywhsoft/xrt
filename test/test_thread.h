


// 线程测试数据
typedef struct {
	xthread Thread;
	int Counter;
	xmutex Mutex;
} ThreadTestData;

typedef struct {
	volatile int bAutoAttached;
	volatile int bNestedAttachOk;
	volatile int bTempOk;
	volatile int bErrorOk;
	volatile int bCleanupRan;
	volatile int bRandOk;
} ThreadRuntimeData;

// 测试线程函数
static uint32 TestThreadProc(ptr param)
{
	ThreadTestData* data = (ThreadTestData*)param;
	printf("  [线程] 启动, TID=%" PRIu64 "\n", xrtThreadGetCurrentId());
	
	for ( int i = 0; i < 10; i++ ) {
		// 检查停止信号
		if ( xrtThreadShouldStop(data->Thread) ) {
			printf("  [线程] 收到停止信号, 退出\n");
			return 1;
		}
		
		// 使用互斥体保护共享数据
		xrtMutexLock(data->Mutex);
		data->Counter++;
		printf("  [线程] Counter = %d\n", data->Counter);
		xrtMutexUnlock(data->Mutex);
		
		xrtSleep(100);
	}
	
	printf("  [线程] 正常结束\n");
	return 0;
}

static void ThreadCleanupProc(xrtThreadData* pThreadData, ptr pArg)
{
	(void)pThreadData;
	(*(volatile int*)pArg)++;
}

static uint32 RuntimeThreadProc(ptr param)
{
	ThreadRuntimeData* pData = (ThreadRuntimeData*)param;
	xrtThreadData* pThreadData = xrtThreadGetCurrent();
	char* sTemp = NULL;

	pData->bAutoAttached = xrtThreadIsAttached();
	pData->bNestedAttachOk = FALSE;
	pData->bTempOk = FALSE;
	pData->bErrorOk = FALSE;
	pData->bRandOk = FALSE;

	if ( pThreadData ) {
		xrtThreadData* pAttach = xrtThreadAttachCurrent();
		pData->bNestedAttachOk = (pAttach == pThreadData);
		xrtThreadDetachCurrent();
	}

	sTemp = (char*)xrtTempMemory(64);
	if ( sTemp ) {
		strcpy(sTemp, "thread-temp-ok");
		pData->bTempOk = (strcmp(sTemp, "thread-temp-ok") == 0);
	}

	xrtSetError("thread-error", FALSE);
	pData->bErrorOk = (xrtStrComp(xrtGetError(), "thread-error", 0, 0) == 0);

	(void)xrtRand32();
	(void)xrtRand64();
	(void)xrtRandRange(1, 100);
	pData->bRandOk = TRUE;

	xrtThreadPushCleanup(ThreadCleanupProc, (ptr)&pData->bCleanupRan);

	return 77;
}

// 信号量测试 - 生产者线程
static xsem g_SemEmpty = NULL;
static xsem g_SemFull = NULL;
static xmutex g_BufferMutex = NULL;
static int g_Buffer[5];
static int g_BufferIndex = 0;
static volatile int g_ProducerStop = 0;
static volatile int g_ConsumerStop = 0;

static uint32 ProducerThread(ptr param)
{
	for ( int i = 1; i <= 10 && !g_ProducerStop; i++ ) {
		xrtSemWait(g_SemEmpty);  // 等待空槽位
		
		xrtMutexLock(g_BufferMutex);
		g_Buffer[g_BufferIndex++] = i;
		printf("  [生产者] 生产: %d, 缓冲区大小: %d\n", i, g_BufferIndex);
		xrtMutexUnlock(g_BufferMutex);
		
		xrtSemPost(g_SemFull);   // 通知有新数据
		xrtSleep(50);
	}
	printf("  [生产者] 结束\n");
	return 0;
}

static uint32 ConsumerThread(ptr param)
{
	for ( int i = 0; i < 10 && !g_ConsumerStop; i++ ) {
		int ret = xrtSemWaitTimeout(g_SemFull, 2000);  // 等待数据，2秒超时
		if ( ret == XRT_WAIT_TIMEOUT ) {
			printf("  [消费者] 等待超时\n");
			break;
		}
		
		xrtMutexLock(g_BufferMutex);
		int value = g_Buffer[--g_BufferIndex];
		printf("  [消费者] 消费: %d, 缓冲区大小: %d\n", value, g_BufferIndex);
		xrtMutexUnlock(g_BufferMutex);
		
		xrtSemPost(g_SemEmpty);  // 通知有空槽位
		xrtSleep(100);
	}
	printf("  [消费者] 结束\n");
	return 0;
}

// 条件变量测试
static xcond g_Cond = NULL;
static xmutex g_CondMutex = NULL;
static volatile int g_DataReady = 0;

static uint32 WaiterThread(ptr param)
{
	int id = (int)(intptr)param;
	printf("  [等待者 %d] 开始等待条件...\n", id);
	
	xrtMutexLock(g_CondMutex);
	while ( !g_DataReady ) {
		xrtCondWait(g_Cond, g_CondMutex);
	}
	printf("  [等待者 %d] 条件满足, 继续执行\n", id);
	xrtMutexUnlock(g_CondMutex);
	
	return 0;
}

// Thread 库测试
void Test_Thread(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n Thread 库测试 :\n\n");
	
	// 测试 1: 基本线程创建和等待
	printf("\n[Test 1] 基本线程创建和等待:\n");
	{
		ThreadTestData data;
		data.Counter = 0;
		data.Mutex = xrtMutexCreate();
		data.Thread = xrtThreadCreate(TestThreadProc, &data, 0);
		
		if ( data.Thread ) {
			printf("  主线程: 线程已创建, TID=%" PRIu64 "\n", data.Thread->TID);
			
			// 等待线程结束
			xrtThreadWait(data.Thread);
			printf("  主线程: 线程已结束, Counter = %d, ExitCode = %u\n", data.Counter, xrtThreadGetExitCode(data.Thread));
			
			xrtThreadDestroy(data.Thread);
		} else {
			printf("  错误: 线程创建失败\n");
		}
		
		xrtMutexDestroy(data.Mutex);
	}
	
	// 测试 2: 发送停止信号
	printf("\n[Test 2] 发送停止信号:\n");
	{
		ThreadTestData data;
		data.Counter = 0;
		data.Mutex = xrtMutexCreate();
		data.Thread = xrtThreadCreate(TestThreadProc, &data, 0);
		
		if ( data.Thread ) {
			printf("  主线程: 线程已创建\n");
			
			// 等待 300ms 后发送停止信号
			xrtSleep(350);
			printf("  主线程: 发送停止信号\n");
			xrtThreadStop(data.Thread);
			
			// 等待线程结束
			xrtThreadWait(data.Thread);
			printf("  主线程: 线程已停止, Counter = %d\n", data.Counter);
			
			xrtThreadDestroy(data.Thread);
		}
		
		xrtMutexDestroy(data.Mutex);
	}
	
	// 测试 3: 带超时的等待
	printf("\n[Test 3] 带超时的等待:\n");
	{
		ThreadTestData data;
		data.Counter = 0;
		data.Mutex = xrtMutexCreate();
		data.Thread = xrtThreadCreate(TestThreadProc, &data, 0);
		
		if ( data.Thread ) {
			// 等待 200ms，应该超时
			int ret = xrtThreadWaitTimeout(data.Thread, 200);
			printf("  等待 200ms 结果: %s\n", 
				ret == XRT_WAIT_TIMEOUT ? "超时" : "完成");
			
			// 发送停止信号并等待
			xrtThreadStop(data.Thread);
			xrtThreadWait(data.Thread);
			
			xrtThreadDestroy(data.Thread);
		}
		
		xrtMutexDestroy(data.Mutex);
	}
	
	// 测试 4: 互斥体
	printf("\n[Test 4] 互斥体测试:\n");
	{
		xmutex mutex = xrtMutexCreate();
		printf("  互斥体已创建\n");
		
		// 测试锁定
		xrtMutexLock(mutex);
		printf("  互斥体已锁定\n");
		
		// 测试 TryLock
		bool tryResult = xrtMutexTryLock(mutex);
		printf("  TryLock 结果: %s (非递归互斥体应失败)\n", 
			tryResult ? "成功" : "失败");
		if ( tryResult ) {
			xrtMutexUnlock(mutex);
		}
		
		xrtMutexUnlock(mutex);
		printf("  互斥体已解锁\n");
		
		xrtMutexDestroy(mutex);
		printf("  互斥体已销毁\n");
	}
	
	// 测试 5: 信号量 - 生产者消费者模式
	printf("\n[Test 5] 信号量测试 (生产者-消费者):\n");
	{
		g_SemEmpty = xrtSemCreate(5, 5);  // 5个空槽位
		g_SemFull = xrtSemCreate(0, 5);   // 0个数据
		g_BufferMutex = xrtMutexCreate();
		g_BufferIndex = 0;
		g_ProducerStop = 0;
		g_ConsumerStop = 0;
		
		xthread producer = xrtThreadCreate(ProducerThread, NULL, 0);
		xthread consumer = xrtThreadCreate(ConsumerThread, NULL, 0);
		
		xrtThreadWait(producer);
		xrtThreadWait(consumer);
		
		xrtThreadDestroy(producer);
		xrtThreadDestroy(consumer);
		xrtSemDestroy(g_SemEmpty);
		xrtSemDestroy(g_SemFull);
		xrtMutexDestroy(g_BufferMutex);
		
		printf("  生产者-消费者测试完成\n");
	}
	
	// 测试 6: 条件变量
	printf("\n[Test 6] 条件变量测试:\n");
	{
		g_Cond = xrtCondCreate();
		g_CondMutex = xrtMutexCreate();
		g_DataReady = 0;
		
		// 创建多个等待线程
		xthread waiters[3];
		for ( int i = 0; i < 3; i++ ) {
			waiters[i] = xrtThreadCreate(WaiterThread, (ptr)(intptr)(i + 1), 0);
		}
		
		xrtSleep(200);  // 等待线程开始等待
		
		printf("  主线程: 设置条件并广播...\n");
		xrtMutexLock(g_CondMutex);
		g_DataReady = 1;
		xrtCondBroadcast(g_Cond);
		xrtMutexUnlock(g_CondMutex);
		
		// 等待所有线程结束
		for ( int i = 0; i < 3; i++ ) {
			xrtThreadWait(waiters[i]);
			xrtThreadDestroy(waiters[i]);
		}
		
		xrtCondDestroy(g_Cond);
		xrtMutexDestroy(g_CondMutex);
		
		printf("  条件变量测试完成\n");
	}
	
	// 测试 7: 获取当前线程ID
	printf("\n[Test 7] 获取当前线程ID:\n");
	{
		uint64 tid = xrtThreadGetCurrentId();
		printf("  当前线程ID: %" PRIu64 "\n", tid);
	}

	// 测试 8: runtime 自动 attach / 线程级状态隔离 / cleanup
	printf("\n[Test 8] runtime 自动 attach 与线程级状态:\n");
	{
		ThreadRuntimeData data = { 0 };
		xthread pThread = NULL;

		xrtSetError("main-error", FALSE);
		pThread = xrtThreadCreate(RuntimeThreadProc, &data, 0);
		if ( pThread ) {
			xrtThreadWait(pThread);
			printf("  自动 attach: %s\n", data.bAutoAttached ? "成功" : "失败");
			printf("  嵌套 attach: %s\n", data.bNestedAttachOk ? "成功" : "失败");
			printf("  临时内存: %s\n", data.bTempOk ? "成功" : "失败");
			printf("  线程错误: %s\n", data.bErrorOk ? "成功" : "失败");
			printf("  cleanup: %s\n", data.bCleanupRan ? "成功" : "失败");
			printf("  随机数: %s\n", data.bRandOk ? "成功" : "失败");
			printf("  退出码: %u\n", xrtThreadGetExitCode(pThread));
			printf("  主线程错误隔离: %s\n",
				xrtStrComp(xrtGetError(), "main-error", 0, 0) == 0 ? "成功" : "失败");
			xrtThreadDestroy(pThread);
		} else {
			printf("  错误: runtime 线程创建失败\n");
		}

		xrtClearError();
	}

	// 测试 9: 当前线程 attach / detach 深度
	printf("\n[Test 9] 当前线程 attach / detach 深度:\n");
	{
		xrtThreadData* pThreadData1 = xrtThreadGetCurrent();
		xrtThreadData* pThreadData2 = xrtThreadAttachCurrent();
		printf("  重复 attach: %s\n", (pThreadData1 && pThreadData1 == pThreadData2) ? "成功" : "失败");
		xrtThreadDetachCurrent();
		printf("  detach 后仍附加: %s\n", xrtThreadIsAttached() ? "成功" : "失败");
	}

	// 测试 10: 销毁运行中线程应被拒绝
	printf("\n[Test 10] 销毁运行中线程保护:\n");
	{
		ThreadTestData data;
		data.Counter = 0;
		data.Mutex = xrtMutexCreate();
		data.Thread = xrtThreadCreate(TestThreadProc, &data, 0);

		if ( data.Thread ) {
			xrtSleep(50);
			xrtClearError();
			xrtThreadDestroy(data.Thread);
			printf("  Destroy 保护: %s\n",
				xrtStrComp(xrtGetError(), "thread is still running.", 0, 0) == 0 ? "成功" : "失败");
			xrtThreadStop(data.Thread);
			xrtThreadWait(data.Thread);
			xrtThreadDestroy(data.Thread);
			xrtClearError();
		}

		xrtMutexDestroy(data.Mutex);
	}
	
	printf("\n Thread 库测试完成\n");
}


