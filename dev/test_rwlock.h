

#include "xrt.h"

// 测试用数据结构
typedef struct {
	xrwlock Lock;
	xlist List;
	int ReadCount;
	int WriteCount;
} RWLockTestData;


// ========== 读线程函数 ==========
uint32 ReaderThread(void* param)
{
	RWLockTestData* data = (RWLockTestData*)param;
	
	for ( int i = 0; i < 1000; i++ ) {
		// 获取读锁
		xrtRWLockReadLock(data->Lock);
		
		data->ReadCount++;
		
		// 模拟读操作
		uint32 count = xvoListItemCount(data->List);
		
		// 释放读锁
		xrtRWLockReadUnlock(data->Lock);
		
		xrtSleep(1);
	}
	
	return 0;
}



// ========== 写线程函数 ==========
uint32 WriterThread(void* param)
{
	RWLockTestData* data = (RWLockTestData*)param;
	
	for ( int i = 0; i < 100; i++ ) {
		// 获取写锁
		xrtRWLockWriteLock(data->Lock);
		
		data->WriteCount++;
		
		// 模拟写操作
		xvoListSetInt(data->List, data->WriteCount % 10, data->WriteCount);
		
		// 释放写锁
		xrtRWLockWriteUnlock(data->Lock);
		
		xrtSleep(10);
	}
	
	return 0;
}



// ========== 性能测试：对比 Mutex 和 RWLock ==========
void Test_RWLock_Performance(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n RWLock 性能测试 :\n\n");
	
	const int READ_THREADS = 10;
	const int WRITE_THREADS = 2;
	const int ITERATIONS = 10000;
	
	// ========== 测试 1: 使用 Mutex ==========
	printf("Test 1: 使用 Mutex\n");
	clock_t start = clock();
	
	xmutex mutex = xrtMutexCreate();
	xlist mutexList = xvoCreateList();
	
	for ( int i = 0; i < ITERATIONS; i++ ) {
		// 90% 读操作
		if ( i % 10 != 0 ) {
			xrtMutexLock(mutex);
			xvoListGetInt(mutexList, 0);
			xrtMutexUnlock(mutex);
		} else {
			// 10% 写操作
			xrtMutexLock(mutex);
			xvoListSetInt(mutexList, 0, i);
			xrtMutexUnlock(mutex);
		}
	}
	
	xrtMutexDestroy(mutex);
	xvoUnref(mutexList);
	
	clock_t end = clock();
	double mutexTime = (double)(end - start) / CLOCKS_PER_SEC;
	printf("  耗时: %.3f 秒\n\n", mutexTime);
	
	
	
	// ========== 测试 2: 使用 RWLock ==========
	printf("Test 2: 使用 RWLock\n");
	start = clock();
	
	xrwlock rwlock = xrtRWLockCreate();
	xlist rwlockList = xvoCreateList();
	
	for ( int i = 0; i < ITERATIONS; i++ ) {
		// 90% 读操作
		if ( i % 10 != 0 ) {
			xrtRWLockReadLock(rwlock);
			xvoListGetInt(rwlockList, 0);
			xrtRWLockReadUnlock(rwlock);
		} else {
			// 10% 写操作
			xrtRWLockWriteLock(rwlock);
			xvoListSetInt(rwlockList, 0, i);
			xrtRWLockWriteUnlock(rwlock);
		}
	}
	
	xrtRWLockDestroy(rwlock);
	xvoUnref(rwlockList);
	
	end = clock();
	double rwlockTime = (double)(end - start) / CLOCKS_PER_SEC;
	printf("  耗时: %.3f 秒\n", rwlockTime);
	printf("  性能提升: %.2f%%\n\n", (mutexTime - rwlockTime) / mutexTime * 100);
	
	
	
	// ========== 测试 3: 多线程并发 ==========
	printf("Test 3: 多线程并发（%d 读线程 + %d 写线程）\n", READ_THREADS, WRITE_THREADS);
	
	RWLockTestData testData;
	testData.Lock = xrtRWLockCreate();
	testData.List = xvoCreateList();
	testData.ReadCount = 0;
	testData.WriteCount = 0;
	
	// 创建读线程
	xthread readers[READ_THREADS];
	for ( int i = 0; i < READ_THREADS; i++ ) {
		readers[i] = xrtThreadCreate(ReaderThread, &testData, 0);
	}
	
	// 创建写线程
	xthread writers[WRITE_THREADS];
	for ( int i = 0; i < WRITE_THREADS; i++ ) {
		writers[i] = xrtThreadCreate(WriterThread, &testData, 0);
	}
	
	// 等待所有写线程完成
	for ( int i = 0; i < WRITE_THREADS; i++ ) {
		xrtThreadWait(writers[i]);
		xrtThreadDestroy(writers[i]);
	}
	
	// 等待所有读线程完成
	for ( int i = 0; i < READ_THREADS; i++ ) {
		xrtThreadWait(readers[i]);
		xrtThreadDestroy(readers[i]);
	}
	
	printf("  读操作次数: %d\n", testData.ReadCount);
	printf("  写操作次数: %d\n", testData.WriteCount);
	printf("  列表元素数: %u\n", xvoListItemCount(testData.List));
	
	xrtRWLockDestroy(testData.Lock);
	xvoUnref(testData.List);
}



// ========== 功能测试 ==========
void Test_RWLock_Functions(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n RWLock 功能测试 :\n\n");
	
	xrwlock rwlock = xrtRWLockCreate();
	
	
	
	// Test 1: 基础读写锁
	printf("Test 1: 基础读写锁\n");
	
	xrtRWLockReadLock(rwlock);
	printf("  ✓ 获取读锁成功\n");
	xrtRWLockReadUnlock(rwlock);
	printf("  ✓ 释放读锁成功\n");
	
	xrtRWLockWriteLock(rwlock);
	printf("  ✓ 获取写锁成功\n");
	xrtRWLockWriteUnlock(rwlock);
	printf("  ✓ 释放写锁成功\n");
	
	
	
	// Test 2: 多个读者并发
	printf("\nTest 2: 多个读者并发\n");
	
	xrtRWLockReadLock(rwlock);
	xrtRWLockReadLock(rwlock);  // 同一线程可以重复获取读锁
	printf("  ✓ 同一线程重复获取读锁成功\n");
	xrtRWLockReadUnlock(rwlock);
	xrtRWLockReadUnlock(rwlock);
	
	
	
	// Test 3: TryLock
	printf("\nTest 3: TryLock\n");
	
	if ( xrtRWLockTryReadLock(rwlock) ) {
		printf("  ✓ TryReadLock 成功\n");
		if ( xrtRWLockTryReadLock(rwlock) ) {
			printf("  ✓ 重复 TryReadLock 成功\n");
			xrtRWLockReadUnlock(rwlock);
		}
		xrtRWLockReadUnlock(rwlock);
	}
	
	if ( xrtRWLockTryWriteLock(rwlock) ) {
		printf("  ✓ TryWriteLock 成功\n");
		xrtRWLockWriteUnlock(rwlock);
	}
	
	
	
	// Test 4: 锁降级
	printf("\nTest 4: 锁降级\n");
	
	xrtRWLockWriteLock(rwlock);
	printf("  ✓ 获取写锁\n");
	xrtRWLockDowngrade(rwlock);
	printf("  ✓ 降级为读锁\n");
	xrtRWLockReadUnlock(rwlock);
	printf("  ✓ 释放读锁\n");
	
	
	
	// Test 5: 锁升级（可能失败）
	printf("\nTest 5: 锁升级\n");
	
	xrtRWLockReadLock(rwlock);
	printf("  ✓ 获取读锁\n");
	if ( xrtRWLockUpgrade(rwlock) ) {
		printf("  ✓ 升级为写锁成功\n");
		xrtRWLockWriteUnlock(rwlock);
	} else {
		printf("  ✗ 升级失败（可能被其他线程抢占）\n");
		xrtRWLockReadUnlock(rwlock);
	}
	
	
	
	#ifdef DEBUG_TRACE
		// Test 6: 调试信息
		printf("\nTest 6: 调试信息\n");
		
		xrtRWLockReadLock(rwlock);
		printf("  读锁状态: %s\n", xrtRWLockIsReadLocked(rwlock) ? "已锁定" : "未锁定");
		printf("  读者数量: %u\n", xrtRWLockGetReaderCount(rwlock));
		xrtRWLockReadUnlock(rwlock);
	#endif
	
	
	
	xrtRWLockDestroy(rwlock);
	printf("\n所有测试完成！\n");
}



// ========== 主测试函数 ==========
void Test_RWLock(xrtGlobalData* xCore)
{
	Test_RWLock_Functions(xCore);
	Test_RWLock_Performance(xCore);
}
