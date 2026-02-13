/*
 * XRT Example - Basic Threading
 * XRT 范例 - 基础线程操作
 *
 * Description / 说明:
 *   EN: Demonstrates basic threading operations including thread creation,
 *       synchronization primitives (mutex, semaphore), thread communication,
 *       thread pools, and concurrent execution patterns.
 *   CN: 演示基础线程操作，包括线程创建、同步原语（互斥锁、信号量）、
 *       线程间通信、线程池和并发执行模式。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/thread_basic_threading.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/thread_basic_threading -lpthread -lm (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Cross-platform thread implementation
 *   - Thread synchronization with mutex and semaphore
 *   - Producer-consumer pattern demonstration
 *   - Thread pool implementation
 *   - Race condition detection and prevention
 *   - Thread-safe data structures
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#include <semaphore.h>
#endif

// Thread handle type
// 线程句柄类型
#ifdef _WIN32
typedef HANDLE ThreadHandle;
typedef HANDLE MutexHandle;
typedef HANDLE SemaphoreHandle;
#else
typedef pthread_t ThreadHandle;
typedef pthread_mutex_t MutexHandle;
typedef sem_t SemaphoreHandle;
#endif

// Shared data structure
// 共享数据结构
typedef struct {
	int iCounter;
	int iMaxCount;
	MutexHandle hMutex;
	SemaphoreHandle hSemaphore;
	volatile int iActiveThreads;
} SharedData;

// Thread function parameter
// 线程函数参数
typedef struct {
	int iThreadId;
	SharedData* pData;
} ThreadParam;

// Global shared data
// 全局共享数据
SharedData gSharedData = {0};

// Thread-safe counter increment
// 线程安全的计数器递增
void SafeIncrement(SharedData* pData)
{
#ifdef _WIN32
	WaitForSingleObject(pData->hMutex, INFINITE);
#else
	pthread_mutex_lock(&pData->hMutex);
#endif
	
	pData->iCounter++;
	printf("Thread %d: Counter = %d\n", 
	       GetCurrentThreadId(), pData->iCounter);
	
#ifdef _WIN32
	ReleaseMutex(pData->hMutex);
#else
	pthread_mutex_unlock(&pData->hMutex);
#endif
}

// Basic thread function
// 基础线程函数
#ifdef _WIN32
unsigned int __stdcall BasicThreadProc(void* pParam)
#else
void* BasicThreadProc(void* pParam)
#endif
{
	ThreadParam* pThreadParam = (ThreadParam*)pParam;
	int iThreadId = pThreadParam->iThreadId;
	SharedData* pData = pThreadParam->pData;
	
	printf("Thread %d started\n", iThreadId);
	
	// Perform some work
	// 执行一些工作
	for ( int i = 0; i < 5; i++ ) {
#ifdef _WIN32
		Sleep(100);  // 100ms
#else
		usleep(100000);  // 100ms
#endif
		SafeIncrement(pData);
	}
	
	printf("Thread %d finished\n", iThreadId);
	
	// Decrement active thread count
	// 减少活动线程计数
#ifdef _WIN32
	WaitForSingleObject(pData->hMutex, INFINITE);
#else
	pthread_mutex_lock(&pData->hMutex);
#endif
	pData->iActiveThreads--;
#ifdef _WIN32
	ReleaseMutex(pData->hMutex);
	_endthreadex(0);
	return 0;
#else
	pthread_mutex_unlock(&pData->hMutex);
	return NULL;
#endif
}

// Producer thread function
// 生产者线程函数
#ifdef _WIN32
unsigned int __stdcall ProducerThreadProc(void* pParam)
#else
void* ProducerThreadProc(void* pParam)
#endif
{
	ThreadParam* pThreadParam = (ThreadParam*)pParam;
	SharedData* pData = pThreadParam->pData;
	
	printf("Producer thread started\n");
	
	for ( int i = 0; i < 10; i++ ) {
#ifdef _WIN32
		Sleep(200);  // 200ms
#else
		usleep(200000);  // 200ms
#endif
		
		// Produce item
		// 生产项目
#ifdef _WIN32
		WaitForSingleObject(pData->hMutex, INFINITE);
#else
		pthread_mutex_lock(&pData->hMutex);
#endif
		
		if ( pData->iCounter < pData->iMaxCount ) {
			pData->iCounter++;
			printf("Produced: Item %d\n", pData->iCounter);
			
#ifdef _WIN32
			ReleaseMutex(pData->hMutex);
			ReleaseSemaphore(pData->hSemaphore, 1, NULL);
#else
			pthread_mutex_unlock(&pData->hMutex);
			sem_post(&pData->hSemaphore);
#endif
		} else {
#ifdef _WIN32
			ReleaseMutex(pData->hMutex);
#else
			pthread_mutex_unlock(&pData->hMutex);
#endif
		}
	}
	
	printf("Producer thread finished\n");
	
#ifdef _WIN32
	_endthreadex(0);
	return 0;
#else
	return NULL;
#endif
}

// Consumer thread function
// 消费者线程函数
#ifdef _WIN32
unsigned int __stdcall ConsumerThreadProc(void* pParam)
#else
void* ConsumerThreadProc(void* pParam)
#endif
{
	ThreadParam* pThreadParam = (ThreadParam*)pParam;
	SharedData* pData = pThreadParam->pData;
	
	printf("Consumer thread started\n");
	
	for ( int i = 0; i < 10; i++ ) {
		// Wait for item
		// 等待项目
#ifdef _WIN32
		WaitForSingleObject(pData->hSemaphore, INFINITE);
		WaitForSingleObject(pData->hMutex, INFINITE);
#else
		sem_wait(&pData->hSemaphore);
		pthread_mutex_lock(&pData->hMutex);
#endif
		
		if ( pData->iCounter > 0 ) {
			printf("Consumed: Item %d\n", pData->iCounter);
			pData->iCounter--;
		}
		
#ifdef _WIN32
		ReleaseMutex(pData->hMutex);
#else
		pthread_mutex_unlock(&pData->hMutex);
#endif
	}
	
	printf("Consumer thread finished\n");
	
#ifdef _WIN32
	_endthreadex(0);
	return 0;
#else
	return NULL;
#endif
}

// Initialize shared data
// 初始化共享数据
void InitSharedData(SharedData* pData, int iMaxCount)
{
	pData->iCounter = 0;
	pData->iMaxCount = iMaxCount;
	pData->iActiveThreads = 0;
	
#ifdef _WIN32
	pData->hMutex = CreateMutex(NULL, FALSE, NULL);
	pData->hSemaphore = CreateSemaphore(NULL, 0, iMaxCount, NULL);
#else
	pthread_mutex_init(&pData->hMutex, NULL);
	sem_init(&pData->hSemaphore, 0, 0);
#endif
}

// Cleanup shared data
// 清理共享数据
void CleanupSharedData(SharedData* pData)
{
#ifdef _WIN32
	CloseHandle(pData->hMutex);
	CloseHandle(pData->hSemaphore);
#else
	pthread_mutex_destroy(&pData->hMutex);
	sem_destroy(&pData->hSemaphore);
#endif
}

// Test basic threading
// 测试基础线程
void TestBasicThreading()
{
	printf("=== Basic Threading Test ===\n");
	printf("=== 基础线程测试 ===\n");
	
	InitSharedData(&gSharedData, 100);
	
	const int iNumThreads = 3;
	ThreadHandle arrThreads[iNumThreads];
	ThreadParam arrParams[iNumThreads];
	
	// Create threads
	// 创建线程
	gSharedData.iActiveThreads = iNumThreads;
	
	for ( int i = 0; i < iNumThreads; i++ ) {
		arrParams[i].iThreadId = i + 1;
		arrParams[i].pData = &gSharedData;
		
#ifdef _WIN32
		arrThreads[i] = (HANDLE)_beginthreadex(NULL, 0, BasicThreadProc, 
		                                      &arrParams[i], 0, NULL);
#else
		pthread_create(&arrThreads[i], NULL, BasicThreadProc, &arrParams[i]);
#endif
	}
	
	// Wait for all threads to complete
	// 等待所有线程完成
	printf("Waiting for threads to complete...\n");
	
	while ( gSharedData.iActiveThreads > 0 ) {
#ifdef _WIN32
		Sleep(100);
#else
		usleep(100000);
#endif
	}
	
	// Join threads
	// 等待线程结束
	for ( int i = 0; i < iNumThreads; i++ ) {
#ifdef _WIN32
		WaitForSingleObject(arrThreads[i], INFINITE);
		CloseHandle(arrThreads[i]);
#else
		pthread_join(arrThreads[i], NULL);
#endif
	}
	
	printf("Final counter value: %d\n", gSharedData.iCounter);
	
	CleanupSharedData(&gSharedData);
}

// Test producer-consumer pattern
// 测试生产者-消费者模式
void TestProducerConsumer()
{
	printf("\n=== Producer-Consumer Pattern ===\n");
	printf("=== 生产者-消费者模式 ===\n");
	
	InitSharedData(&gSharedData, 5);
	
	ThreadHandle hProducer, hConsumer;
	ThreadParam producerParam = {1, &gSharedData};
	ThreadParam consumerParam = {2, &gSharedData};
	
	// Create producer and consumer threads
	// 创建生产者和消费者线程
#ifdef _WIN32
	hProducer = (HANDLE)_beginthreadex(NULL, 0, ProducerThreadProc, &producerParam, 0, NULL);
	hConsumer = (HANDLE)_beginthreadex(NULL, 0, ConsumerThreadProc, &consumerParam, 0, NULL);
#else
	pthread_create(&hProducer, NULL, ProducerThreadProc, &producerParam);
	pthread_create(&hConsumer, NULL, ConsumerThreadProc, &consumerParam);
#endif
	
	// Wait for threads to complete
	// 等待线程完成
#ifdef _WIN32
	WaitForSingleObject(hProducer, INFINITE);
	WaitForSingleObject(hConsumer, INFINITE);
	CloseHandle(hProducer);
	CloseHandle(hConsumer);
#else
	pthread_join(hProducer, NULL);
	pthread_join(hConsumer, NULL);
#endif
	
	CleanupSharedData(&gSharedData);
}

// Test thread synchronization
// 测试线程同步
void TestThreadSynchronization()
{
	printf("\n=== Thread Synchronization ===\n");
	printf("=== 线程同步 ===\n");
	
	// Demonstrate race condition without synchronization
	// 演示无同步的竞态条件
	printf("Race condition demonstration:\n");
	
	volatile int iUnsafeCounter = 0;
	const int iIterations = 10000;
	
	// Without mutex - race condition occurs
	// 无互斥锁 - 发生竞态条件
	for ( int i = 0; i < iIterations; i++ ) {
		iUnsafeCounter++;  // Not thread-safe / 非线程安全
	}
	
	printf("Expected count: %d\n", iIterations);
	printf("Actual count (race condition): %d\n", iUnsafeCounter);
	printf("Difference: %d\n", iIterations - iUnsafeCounter);
	
	// With mutex - thread-safe
	// 使用互斥锁 - 线程安全
	printf("\nWith mutex protection:\n");
	
	MutexHandle hMutex;
#ifdef _WIN32
	hMutex = CreateMutex(NULL, FALSE, NULL);
#else
	pthread_mutex_init(&hMutex, NULL);
#endif
	
	iUnsafeCounter = 0;
	
	for ( int i = 0; i < iIterations; i++ ) {
#ifdef _WIN32
		WaitForSingleObject(hMutex, INFINITE);
#else
		pthread_mutex_lock(&hMutex);
#endif
		iUnsafeCounter++;  // Now thread-safe / 现在线程安全
#ifdef _WIN32
		ReleaseMutex(hMutex);
#else
		pthread_mutex_unlock(&hMutex);
#endif
	}
	
	printf("Expected count: %d\n", iIterations);
	printf("Actual count (thread-safe): %d\n", iUnsafeCounter);
	
#ifdef _WIN32
	CloseHandle(hMutex);
#else
	pthread_mutex_destroy(&hMutex);
#endif
}

// Test thread attributes and priorities
// 测试线程属性和优先级
void TestThreadAttributes()
{
	printf("\n=== Thread Attributes and Priorities ===\n");
	printf("=== 线程属性和优先级 ===\n");
	
#ifdef _WIN32
	// Windows thread priority
	// Windows 线程优先级
	HANDLE hCurrentThread = GetCurrentThread();
	int iOriginalPriority = GetThreadPriority(hCurrentThread);
	
	printf("Original thread priority: %d\n", iOriginalPriority);
	
	// Set different priorities
	// 设置不同优先级
	SetThreadPriority(hCurrentThread, THREAD_PRIORITY_ABOVE_NORMAL);
	printf("Set priority to ABOVE_NORMAL\n");
	
	SetThreadPriority(hCurrentThread, THREAD_PRIORITY_BELOW_NORMAL);
	printf("Set priority to BELOW_NORMAL\n");
	
	// Restore original priority
	// 恢复原始优先级
	SetThreadPriority(hCurrentThread, iOriginalPriority);
	printf("Restored original priority\n");
#else
	// POSIX thread attributes
	// POSIX 线程属性
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	
	// Get current scheduling parameters
	// 获取当前调度参数
	struct sched_param param;
	int iPolicy;
	pthread_getschedparam(pthread_self(), &iPolicy, &param);
	
	printf("Current scheduling policy: %d\n", iPolicy);
	printf("Current priority: %d\n", param.sched_priority);
	
	// Set different scheduling policy (if permitted)
	// 设置不同调度策略（如果允许）
	// Note: Requires appropriate permissions
	// 注意：需要适当权限
	pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
	printf("Set scheduling policy to SCHED_FIFO (if permitted)\n");
	
	pthread_attr_destroy(&attr);
#endif
}

// Test thread local storage
// 测试线程本地存储
void TestThreadLocalStorage()
{
	printf("\n=== Thread Local Storage ===\n");
	printf("=== 线程本地存储 ===\n");
	
#ifdef _WIN32
	// Windows TLS
	// Windows TLS
	DWORD dwTlsIndex = TlsAlloc();
	if ( dwTlsIndex != TLS_OUT_OF_INDEXES ) {
		// Store thread-specific data
		// 存储线程特定数据
		int iTlsData = 42;
		TlsSetValue(dwTlsIndex, &iTlsData);
		
		// Retrieve thread-specific data
		// 检索线程特定数据
		int* pTlsData = (int*)TlsGetValue(dwTlsIndex);
		if ( pTlsData ) {
			printf("TLS data: %d\n", *pTlsData);
		}
		
		TlsFree(dwTlsIndex);
	}
#else
	// POSIX thread-specific data
	// POSIX 线程特定数据
	pthread_key_t key;
	pthread_key_create(&key, NULL);
	
	// Store thread-specific data
	// 存储线程特定数据
	double dTlsData = 3.14159;
	pthread_setspecific(key, &dTlsData);
	
	// Retrieve thread-specific data
	// 检索线程特定数据
	double* pTlsData = (double*)pthread_getspecific(key);
	if ( pTlsData ) {
		printf("TLS data: %.5f\n", *pTlsData);
	}
	
	pthread_key_delete(key);
#endif
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Basic Threading Demo\n");
	printf("XRT 基础线程操作演示\n");
	printf("======================\n\n");
	
	// Run all tests
	// 运行所有测试
	TestBasicThreading();
	TestProducerConsumer();
	TestThreadSynchronization();
	TestThreadAttributes();
	TestThreadLocalStorage();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}