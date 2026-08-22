#ifndef XRT_TEST_THREAD_H
#define XRT_TEST_THREAD_H

#if defined(_WIN32) || defined(_WIN64)
	#if defined(__TINYC__)
		#include <winapi/windows.h>
	#else
		#include <windows.h>
	#endif
#else
	#include <pthread.h>
	#include <sched.h>
#endif



/* 测试线程返回零表示成功，非零值直接保存在槽中。 */
typedef int (*testthreadproc)(ptr pData);



/* 一个可启动和回收的跨平台测试线程槽。 */
typedef struct testthread {
	testthreadproc Proc;
	ptr Data;
	int Result;
	#if defined(_WIN32) || defined(_WIN64)
		HANDLE Handle;
	#else
		pthread_t Handle;
	#endif
} testthread;



#if defined(_WIN32) || defined(_WIN64)
/* Windows 测试线程统一入口。 */
static DWORD WINAPI testThreadEntry(LPVOID pData)
{
	testthread* pThread = (testthread*)pData;

	pThread->Result = pThread->Proc(pThread->Data);
	return (DWORD)pThread->Result;
}
#else
/* POSIX 测试线程统一入口。 */
static void* testThreadEntry(void* pData)
{
	testthread* pThread = (testthread*)pData;

	pThread->Result = pThread->Proc(pThread->Data);
	return NULL;
}
#endif



/* 启动一组已经填入回调和数据的测试线程。 */
static void testThreadsStart(testthread* arrThread, size_t iCount)
{
	for ( size_t i = 0; i < iCount; i++ ) {
		arrThread[i].Result = -1;
		#if defined(_WIN32) || defined(_WIN64)
			arrThread[i].Handle = CreateThread(NULL, 0, testThreadEntry, &arrThread[i], 0, NULL);
			testRequire(arrThread[i].Handle != NULL, "CreateThread failed");
		#else
			testRequire(pthread_create(&arrThread[i].Handle, NULL, testThreadEntry, &arrThread[i]) == 0,
				"pthread_create failed");
		#endif
	}
}



/* 等待并回收一组已经启动的测试线程。 */
static void testThreadsJoin(testthread* arrThread, size_t iCount)
{
	for ( size_t i = 0; i < iCount; i++ ) {
		#if defined(_WIN32) || defined(_WIN64)
			testRequire(WaitForSingleObject(arrThread[i].Handle, INFINITE) == WAIT_OBJECT_0,
				"thread wait failed");
			CloseHandle(arrThread[i].Handle);
		#else
			testRequire(pthread_join(arrThread[i].Handle, NULL) == 0, "pthread_join failed");
		#endif
	}
}



/* 让出当前测试线程的处理器时间片。 */
static inline void testThreadYield(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		(void)SwitchToThread();
	#else
		(void)sched_yield();
	#endif
}

#endif
