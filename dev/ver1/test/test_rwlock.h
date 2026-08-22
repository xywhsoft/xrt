

#include <string.h>
#include <time.h>

#if defined(__APPLE__) && defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
	#define __XRT_TEST_RWLOCK_SCREEN_STEP() do {} while (0)
#elif defined(__ANDROID__)
	#define __XRT_TEST_RWLOCK_SCREEN_STEP() do {} while (0)
#else
	#define __XRT_TEST_RWLOCK_SCREEN_STEP() do { system("pause"); system("cls"); } while (0)
#endif

// RWLock 测试数据结构
typedef struct {
	xrwlock Lock;
	xvalue List;
	int ReadCount;
	int WriteCount;
} RWLockTestData;


// 读线程函数
static uint32 ReaderThread_RW(ptr param)
{
	RWLockTestData* data = (RWLockTestData*)param;
	
	for ( int i = 0; i < 100; i++ ) {
		xrtRWLockReadLock(data->Lock);
		
		data->ReadCount++;
		uint32 count = xvoListItemCount(data->List);
		
		xrtRWLockReadUnlock(data->Lock);
		
		xrtSleep(1);
	}
	
	return 0;
}


// 写线程函数
static uint32 WriterThread_RW(ptr param)
{
	RWLockTestData* data = (RWLockTestData*)param;
	
	for ( int i = 0; i < 10; i++ ) {
		xrtRWLockWriteLock(data->Lock);
		
		data->WriteCount++;
		xvoListSetValue(data->List, data->WriteCount % 10, xvoCreateInt(data->WriteCount), TRUE);
		
		xrtRWLockWriteUnlock(data->Lock);
		
		xrtSleep(10);
	}
	
	return 0;
}


// 简单读线程函数（用于多读者测试）
typedef struct {
	xrwlock lock;
	volatile int* result;
	int iterations;
} SimpleReaderParam;


// SIMPLEREADER线程相关处理
static uint32 SimpleReaderThread(ptr param)
{
	SimpleReaderParam* p = (SimpleReaderParam*)param;
	
	for ( int i = 0; i < p->iterations; i++ ) {
		xrtRWLockReadLock(p->lock);
		(*p->result)++;
		xrtRWLockReadUnlock(p->lock);
		xrtSleep(1);
	}
	
	return 0;
}


// RWLock 库测试
void Test_RWLock(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n RWLock 库测试 :\n\n");
	
	
	
	// Test 1: 基础读写锁
	printf("Test 1: 基础读写锁\n\n");
	
	xrwlock rwlock = xrtRWLockCreate();
	
	xrtRWLockReadLock(rwlock);
	printf("  获取读锁成功\n");
	xrtRWLockReadUnlock(rwlock);
	printf("  释放读锁成功\n");
	
	xrtRWLockWriteLock(rwlock);
	printf("  获取写锁成功\n");
	xrtRWLockWriteUnlock(rwlock);
	printf("  释放写锁成功\n");
	
	xrtRWLockDestroy(rwlock);
	
	printf("\n\n");
	__XRT_TEST_RWLOCK_SCREEN_STEP();
	
	
	
	// Test 2: TryLock
	printf("Test 2: TryLock\n\n");
	
	rwlock = xrtRWLockCreate();
	
	if ( xrtRWLockTryReadLock(rwlock) ) {
		printf("  TryReadLock 成功\n");
		xrtRWLockReadUnlock(rwlock);
	}
	
	if ( xrtRWLockTryWriteLock(rwlock) ) {
		printf("  TryWriteLock 成功\n");
		xrtRWLockWriteUnlock(rwlock);
	}
	
	xrtRWLockDestroy(rwlock);
	
	printf("\n\n");
	__XRT_TEST_RWLOCK_SCREEN_STEP();
	
	
	
	// Test 3: 锁降级
	printf("Test 3: 锁降级\n\n");
	
	rwlock = xrtRWLockCreate();
	
	xrtRWLockWriteLock(rwlock);
	printf("  获取写锁\n");
	xrtRWLockDowngrade(rwlock);
	printf("  降级为读锁\n");
	xrtRWLockReadUnlock(rwlock);
	printf("  释放读锁\n");
	
	xrtRWLockDestroy(rwlock);
	
	printf("\n\n");
	__XRT_TEST_RWLOCK_SCREEN_STEP();
	
	
	
	// Test 4: 锁升级
	printf("Test 4: 锁升级\n\n");
	
	rwlock = xrtRWLockCreate();
	
	xrtRWLockReadLock(rwlock);
	printf("  获取读锁\n");
	
	if ( xrtRWLockUpgrade(rwlock) ) {
		printf("  升级为写锁成功\n");
		xrtRWLockWriteUnlock(rwlock);
	} else {
		printf("  升级失败（正常，可能被其他线程抢占）\n");
		xrtRWLockReadUnlock(rwlock);
	}
	
	xrtRWLockDestroy(rwlock);
	
	printf("\n\n");
	__XRT_TEST_RWLOCK_SCREEN_STEP();
	
	
	
	// Test 5: 多读者并发
	printf("Test 5: 多读者并发\n\n");
	
	rwlock = xrtRWLockCreate();
	
	#define READER_COUNT 5
	xthread readers[READER_COUNT];
	volatile int readerResults[READER_COUNT];
	
	memset((void*)readerResults, 0, sizeof(readerResults));
	
	SimpleReaderParam params[READER_COUNT];
	for ( int i = 0; i < READER_COUNT; i++ ) {
		params[i].lock = rwlock;
		params[i].result = &readerResults[i];
		params[i].iterations = 10;
		
		readers[i] = xrtThreadCreate(SimpleReaderThread, &params[i], 0);
	}
	
	for ( int i = 0; i < READER_COUNT; i++ ) {
		xrtThreadWait(readers[i]);
		xrtThreadDestroy(readers[i]);
		printf("  读者 %d 完成: %d 次读操作\n", i + 1, readerResults[i]);
	}
	
	xrtRWLockDestroy(rwlock);
	
	printf("\n\n");
	__XRT_TEST_RWLOCK_SCREEN_STEP();
	
	
	
	// Test 6: 读写混合
	printf("Test 6: 读写混合\n\n");
	
	RWLockTestData testData;
	testData.Lock = xrtRWLockCreate();
	testData.List = xvoCreateList();
	testData.ReadCount = 0;
	testData.WriteCount = 0;
	
	xthread reader = xrtThreadCreate(ReaderThread_RW, &testData, 0);
	xthread writer = xrtThreadCreate(WriterThread_RW, &testData, 0);
	
	xrtThreadWait(reader);
	xrtThreadWait(writer);
	
	xrtThreadDestroy(reader);
	xrtThreadDestroy(writer);
	
	printf("  读操作次数: %d\n", testData.ReadCount);
	printf("  写操作次数: %d\n", testData.WriteCount);
	printf("  列表元素数: %u\n", xvoListItemCount(testData.List));
	
	xrtRWLockDestroy(testData.Lock);
	xvoUnref(testData.List);
	
	printf("\n\n");
	__XRT_TEST_RWLOCK_SCREEN_STEP();
	
	
	
	// Test 7: 性能对比（Mutex vs RWLock）
	printf("Test 7: 性能对比（Mutex vs RWLock）\n\n");
	
	const int ITERATIONS = 10000;
	
	printf("  测试 %d 次操作（90%% 读，10%% 写）\n\n", ITERATIONS);
	
	// Mutex 测试
	printf("  使用 Mutex:\n");
	clock_t start = clock();
	
	xmutex mutex = xrtMutexCreate();
	xvalue mutexList = xvoCreateList();
	
	for ( int i = 0; i < ITERATIONS; i++ ) {
		if ( i % 10 != 0 ) {
			xrtMutexLock(mutex);
			xvalue val = xvoListGetValue(mutexList, 0);
			xrtMutexUnlock(mutex);
		} else {
			xrtMutexLock(mutex);
			xvoListSetValue(mutexList, 0, xvoCreateInt(i), TRUE);
			xrtMutexUnlock(mutex);
		}
	}
	
	xrtMutexDestroy(mutex);
	xvoUnref(mutexList);
	
	clock_t end = clock();
	double mutexTime = (double)(end - start) / CLOCKS_PER_SEC;
	printf("    耗时: %.3f 秒\n", mutexTime);
	
	// RWLock 测试
	printf("  使用 RWLock:\n");
	start = clock();
	
	xrwlock rwlockPerf = xrtRWLockCreate();
	xvalue rwlockList = xvoCreateList();
	
	for ( int i = 0; i < ITERATIONS; i++ ) {
		if ( i % 10 != 0 ) {
			xrtRWLockReadLock(rwlockPerf);
			xvalue val = xvoListGetValue(rwlockList, 0);
			xrtRWLockReadUnlock(rwlockPerf);
		} else {
			xrtRWLockWriteLock(rwlockPerf);
			xvoListSetValue(rwlockList, 0, xvoCreateInt(i), TRUE);
			xrtRWLockWriteUnlock(rwlockPerf);
		}
	}
	
	xrtRWLockDestroy(rwlockPerf);
	xvoUnref(rwlockList);
	
	end = clock();
	double rwlockTime = (double)(end - start) / CLOCKS_PER_SEC;
	printf("    耗时: %.3f 秒\n", rwlockTime);
	
	if ( rwlockTime < mutexTime ) {
		double improvement = (mutexTime - rwlockTime) / mutexTime * 100;
		printf("    性能提升: %.2f%%\n", improvement);
	} else {
		printf("    性能下降: %.2f%%\n", (rwlockTime - mutexTime) / mutexTime * 100);
	}
	
	printf("\n\n");
	__XRT_TEST_RWLOCK_SCREEN_STEP();
	
	
	
	// Test 8: Init 和 Unit（静态分配）
	printf("Test 8: Init 和 Unit（静态分配）\n\n");
	
	xrwlock_struct staticLock;
	xrtRWLockInit(&staticLock);
	printf("  初始化静态分配的读写锁成功\n");
	
	xrtRWLockReadLock(&staticLock);
	xrtRWLockReadUnlock(&staticLock);
	printf("  使用静态分配的读写锁成功\n");
	
	xrtRWLockUnit(&staticLock);
	printf("  清理静态分配的读写锁成功\n");
	
	printf("\n\n");
	__XRT_TEST_RWLOCK_SCREEN_STEP();
	
	
	
	printf("\n RWLock 库测试完成\n");
}
