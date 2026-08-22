#include "../test.h"
#include "../test_thread.h"



#define TEST_TEMP_THREAD_COUNT 4



/* 每个线程保存自己的默认 arena 和临时地址。 */
typedef struct test_temp_thread_context {
	int Index;
	xtemparena* Arena;
	ptr Memory;
} test_temp_thread_context;



#if defined(_WIN32) || defined(_WIN64)
static volatile LONG __testTempReady;
static volatile LONG __testTempStart;
#else
static pthread_mutex_t __testTempLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t __testTempCond = PTHREAD_COND_INITIALIZER;
static int __testTempReady;
static bool __testTempStart;
#endif



/* 等待所有线程都持有活动临时内存后再继续。 */
static void testTempBarrier(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		(void)InterlockedIncrement(&__testTempReady);
		while ( InterlockedCompareExchange(&__testTempStart, 0, 0) == 0 ) {
			Sleep(0);
		}
	#else
		(void)pthread_mutex_lock(&__testTempLock);
		__testTempReady++;
		(void)pthread_cond_broadcast(&__testTempCond);
		while ( !__testTempStart ) {
			(void)pthread_cond_wait(&__testTempCond, &__testTempLock);
		}
		(void)pthread_mutex_unlock(&__testTempLock);
	#endif
}



/* 在线程默认 arena 中保存并验证独立内容。 */
static int testTempThreadRun(ptr pData)
{
	test_temp_thread_context* pContext = (test_temp_thread_context*)pData;
	char* sMemory;

	pContext->Arena = xrtTempCurrent();
	sMemory = (char*)xrtTemp(64);
	pContext->Memory = sMemory;
	if ( (pContext->Arena == NULL) || (sMemory == NULL) ) {
		return 1;
	}
	sMemory[0] = (char)('A' + pContext->Index);
	sMemory[1] = 0;
	testTempBarrier();
	if ( sMemory[0] != (char)('A' + pContext->Index) ) {
		return 2;
	}
	if ( !xrtTempClear() ) {
		return 3;
	}
	return 0;
}



/* 验证默认 arena 按原生线程隔离。 */
int main(void)
{
	test_temp_thread_context arrContext[TEST_TEMP_THREAD_COUNT];
	testthread arrThread[TEST_TEMP_THREAD_COUNT];

	memset(arrContext, 0, sizeof(arrContext));
	for ( int i = 0; i < TEST_TEMP_THREAD_COUNT; i++ ) {
		arrContext[i].Index = i;
		arrThread[i].Proc = testTempThreadRun;
		arrThread[i].Data = &arrContext[i];
	}
	testThreadsStart(arrThread, TEST_TEMP_THREAD_COUNT);
	#if defined(_WIN32) || defined(_WIN64)
		while ( InterlockedCompareExchange(&__testTempReady, 0, 0) != TEST_TEMP_THREAD_COUNT ) {
			Sleep(0);
		}
		(void)InterlockedExchange(&__testTempStart, 1);
	#else
		(void)pthread_mutex_lock(&__testTempLock);
		while ( __testTempReady != TEST_TEMP_THREAD_COUNT ) {
			(void)pthread_cond_wait(&__testTempCond, &__testTempLock);
		}
		__testTempStart = true;
		(void)pthread_cond_broadcast(&__testTempCond);
		(void)pthread_mutex_unlock(&__testTempLock);
	#endif
	testThreadsJoin(arrThread, TEST_TEMP_THREAD_COUNT);
	for ( int i = 0; i < TEST_TEMP_THREAD_COUNT; i++ ) {
		testRequire(arrThread[i].Result == 0, "temp thread worker failed");
		for ( int j = i + 1; j < TEST_TEMP_THREAD_COUNT; j++ ) {
			testRequire(arrContext[i].Arena != arrContext[j].Arena, "threads shared a default arena");
			testRequire(arrContext[i].Memory != arrContext[j].Memory, "threads shared temporary memory");
		}
	}
	printf("[PASS] temp_threads\n");
	return 0;
}
