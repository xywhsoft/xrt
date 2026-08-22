#include "../internal/xrt_wait.h"

#include <errno.h>

#if !defined(_WIN32) && !defined(_WIN64)
	#include <pthread.h>
#endif



#if defined(XRT_FEATURE_THREAD)

/* 线程对象同时保存平台句柄、完成通知和用户可观测状态。 */
struct xthread {
	volatile int32 RefCount;
	xthreadproc Proc;
	ptr Data;
	uint64 Id;
	int32 ExitCode;
	xthreadstate State;
	#if defined(_WIN32) || defined(_WIN64)
		HANDLE Handle;
		CRITICAL_SECTION Lock;
		CONDITION_VARIABLE Condition;
		volatile LONG StopRequested;
	#else
		pthread_t Handle;
		pthread_mutex_t Lock;
		pthread_cond_t Condition;
		volatile int32 StopRequested;
		bool ConditionMonotonic;
	#endif
};



/* 把平台错误写入当前 XRT 错误上下文。 */
static void __xrtThreadSetSystemError(cstr sOperation, int iCode, cstr sMessage)
{
	xerrordesc tDesc;
	xerror* pError;

	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.Kind = __xrtSystemErrorKind(iCode);
	tDesc.Code = 1;
	tDesc.SystemCode = iCode;
	tDesc.Domain = "xrt.thread";
	tDesc.Operation = sOperation;
	tDesc.Message = sMessage;
	pError = xrtErrorBuild(&tDesc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



#if defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))

static DWORD __xrtThreadTlsIndex = TLS_OUT_OF_INDEXES;
static volatile LONG __xrtThreadTlsState;



/* 为 TinyCC Windows 构建准备当前线程对象的系统 TLS 槽。 */
static bool __xrtThreadTlsEnsure(void)
{
	LONG iState = InterlockedCompareExchange(&__xrtThreadTlsState, 1, 0);

	if ( iState == 0 ) {
		__xrtThreadTlsIndex = TlsAlloc();
		InterlockedExchange(
			&__xrtThreadTlsState,
			__xrtThreadTlsIndex != TLS_OUT_OF_INDEXES ? 2 : 3
		);
		return __xrtThreadTlsIndex != TLS_OUT_OF_INDEXES;
	}
	while ( (iState = InterlockedCompareExchange(&__xrtThreadTlsState, 0, 0)) == 1 ) {
		Sleep(0);
	}
	return iState == 2;
}



/* 读取 TinyCC Windows 当前线程对象。 */
static xthread* __xrtThreadCurrentGet(void)
{
	return __xrtThreadTlsEnsure() ? (xthread*)TlsGetValue(__xrtThreadTlsIndex) : NULL;
}



/* 绑定 TinyCC Windows 当前线程对象。 */
static void __xrtThreadCurrentSet(xthread* pThread)
{
	if ( __xrtThreadTlsEnsure() ) {
		(void)TlsSetValue(__xrtThreadTlsIndex, pThread);
	}
}

#elif defined(__TINYC__)

static pthread_key_t __xrtThreadTlsKey;
static pthread_once_t __xrtThreadTlsOnce = PTHREAD_ONCE_INIT;
static bool __xrtThreadTlsReady;



/* 为 TinyCC POSIX 构建创建当前线程对象的 TLS key。 */
static void __xrtThreadTlsInit(void)
{
	__xrtThreadTlsReady = pthread_key_create(&__xrtThreadTlsKey, NULL) == 0;
}



/* 读取 TinyCC POSIX 当前线程对象。 */
static xthread* __xrtThreadCurrentGet(void)
{
	(void)pthread_once(&__xrtThreadTlsOnce, __xrtThreadTlsInit);
	return __xrtThreadTlsReady ? (xthread*)pthread_getspecific(__xrtThreadTlsKey) : NULL;
}



/* 绑定 TinyCC POSIX 当前线程对象。 */
static void __xrtThreadCurrentSet(xthread* pThread)
{
	(void)pthread_once(&__xrtThreadTlsOnce, __xrtThreadTlsInit);
	if ( __xrtThreadTlsReady ) {
		(void)pthread_setspecific(__xrtThreadTlsKey, pThread);
	}
}

#else

static XRT_THREAD_LOCAL xthread* __xrtThreadCurrentObject;



/* 读取编译器 TLS 保存的当前线程对象。 */
static xthread* __xrtThreadCurrentGet(void)
{
	return __xrtThreadCurrentObject;
}



/* 绑定编译器 TLS 保存的当前线程对象。 */
static void __xrtThreadCurrentSet(xthread* pThread)
{
	__xrtThreadCurrentObject = pThread;
}

#endif



#if !defined(_WIN32) && !defined(_WIN64)
/* 把 XRT 截止时间转换为线程条件变量实际使用的绝对时钟。 */
static bool __xrtThreadDeadlineTime(
	const xthread* pThread,
	xdeadline iDeadline,
	struct timespec* pTime
)
{
	uint64 iRemaining;
	uint64 iNanoseconds;

	if ( pThread->ConditionMonotonic ) {
		pTime->tv_sec = (time_t)(iDeadline / UINT64_C(1000000));
		pTime->tv_nsec = (long)((iDeadline % UINT64_C(1000000)) * UINT64_C(1000));
		return true;
	}
	iRemaining = xrtDeadlineRemaining(iDeadline);
	if ( clock_gettime(CLOCK_REALTIME, pTime) != 0 ) {
		__xrtThreadSetSystemError("clock", errno, "realtime clock is unavailable");
		return false;
	}
	iNanoseconds = (uint64)pTime->tv_nsec +
		((iRemaining % UINT64_C(1000000)) * UINT64_C(1000));
	pTime->tv_sec += (time_t)(iRemaining / UINT64_C(1000000));
	pTime->tv_sec += (time_t)(iNanoseconds / UINT64_C(1000000000));
	pTime->tv_nsec = (long)(iNanoseconds % UINT64_C(1000000000));
	return true;
}
#endif



/* 释放已经没有外部引用和运行引用的线程对象。 */
static void __xrtThreadFree(xthread* pThread)
{
	#if defined(_WIN32) || defined(_WIN64)
		if ( pThread->Handle != NULL ) {
			(void)CloseHandle(pThread->Handle);
		}
		DeleteCriticalSection(&pThread->Lock);
	#else
		(void)pthread_cond_destroy(&pThread->Condition);
		(void)pthread_mutex_destroy(&pThread->Lock);
	#endif
	xrtFree(pThread);
}



/* 释放一个内部或外部线程引用。 */
static void __xrtThreadRelease(xthread* pThread)
{
	bool bFree;

	if ( pThread == NULL ) {
		return;
	}
	#if defined(_WIN32) || defined(_WIN64)
		EnterCriticalSection(&pThread->Lock);
		bFree = xrtRefRelease(&pThread->RefCount) == 0;
		LeaveCriticalSection(&pThread->Lock);
	#else
		(void)pthread_mutex_lock(&pThread->Lock);
		bFree = xrtRefRelease(&pThread->RefCount) == 0;
		(void)pthread_mutex_unlock(&pThread->Lock);
	#endif
	if ( bFree ) {
		__xrtThreadFree(pThread);
	}
}



/* 释放运行引用、发布完成状态并唤醒全部等待者。 */
static void __xrtThreadFinish(xthread* pThread, int32 iExitCode)
{
	bool bFree;

	#if defined(_WIN32) || defined(_WIN64)
		EnterCriticalSection(&pThread->Lock);
		pThread->ExitCode = iExitCode;
		bFree = xrtRefRelease(&pThread->RefCount) == 0;
		pThread->State = XTHREAD_FINISHED;
		WakeAllConditionVariable(&pThread->Condition);
		LeaveCriticalSection(&pThread->Lock);
	#else
		(void)pthread_mutex_lock(&pThread->Lock);
		pThread->ExitCode = iExitCode;
		bFree = xrtRefRelease(&pThread->RefCount) == 0;
		pThread->State = XTHREAD_FINISHED;
		(void)pthread_cond_broadcast(&pThread->Condition);
		(void)pthread_mutex_unlock(&pThread->Lock);
	#endif
	if ( bFree ) {
		__xrtThreadFree(pThread);
	}
}



#if defined(_WIN32) || defined(_WIN64)
/* Windows 线程包装器维护当前对象、完成状态和运行引用。 */
static DWORD WINAPI __xrtThreadEntry(LPVOID pData)
{
	xthread* pThread = (xthread*)pData;
	int32 iExitCode;

	EnterCriticalSection(&pThread->Lock);
	LeaveCriticalSection(&pThread->Lock);
	__xrtThreadCurrentSet(pThread);
	iExitCode = pThread->Proc(pThread->Data);
	#if defined(XRT_FEATURE_COROUTINE)
		(void)xrtCoThreadDetach();
	#endif
	#if defined(XRT_FEATURE_THREAD_KEY)
		(void)xrtThreadKeysClear();
	#endif
	xrtClearError();
	__xrtThreadCurrentSet(NULL);
	__xrtThreadFinish(pThread, iExitCode);
	return (DWORD)iExitCode;
}
#else
/* POSIX 线程包装器维护当前对象、完成状态和运行引用。 */
static void* __xrtThreadEntry(void* pData)
{
	xthread* pThread = (xthread*)pData;
	int32 iExitCode;

	(void)pthread_mutex_lock(&pThread->Lock);
	(void)pthread_mutex_unlock(&pThread->Lock);
	__xrtThreadCurrentSet(pThread);
	iExitCode = pThread->Proc(pThread->Data);
	#if defined(XRT_FEATURE_COROUTINE)
		(void)xrtCoThreadDetach();
	#endif
	#if defined(XRT_FEATURE_THREAD_KEY)
		(void)xrtThreadKeysClear();
	#endif
	xrtClearError();
	__xrtThreadCurrentSet(NULL);
	__xrtThreadFinish(pThread, iExitCode);
	return NULL;
}
#endif



/* 在 Windows 条件变量上等待线程完成。 */
#if defined(_WIN32) || defined(_WIN64)
static xwaitresult __xrtThreadWaitDeadline(xthread* pThread, xdeadline iDeadline)
{
	EnterCriticalSection(&pThread->Lock);
	while ( pThread->State != XTHREAD_FINISHED ) {
		uint64 iRemaining = xrtDeadlineRemaining(iDeadline);
		DWORD iMilliseconds;

		if ( iRemaining == 0 ) {
			LeaveCriticalSection(&pThread->Lock);
			return XWAIT_TIMEOUT;
		}
		iMilliseconds = iRemaining == UINT64_MAX ? INFINITE :
			(DWORD)__xrtWaitMilliseconds(iRemaining);
		if ( !SleepConditionVariableCS(&pThread->Condition, &pThread->Lock, iMilliseconds) ) {
			int iCode = (int)GetLastError();

			if ( iCode != ERROR_TIMEOUT ) {
				LeaveCriticalSection(&pThread->Lock);
				__xrtThreadSetSystemError("wait", iCode, "thread wait failed");
				return XWAIT_ERROR;
			}
		}
	}
	LeaveCriticalSection(&pThread->Lock);
	return XWAIT_OK;
}
#else
/* 在单调时钟条件变量上等待 POSIX 线程完成。 */
static xwaitresult __xrtThreadWaitDeadline(xthread* pThread, xdeadline iDeadline)
{
	int iResult;

	iResult = pthread_mutex_lock(&pThread->Lock);
	if ( iResult != 0 ) {
		__xrtThreadSetSystemError("wait", iResult, "thread state lock failed");
		return XWAIT_ERROR;
	}
	while ( pThread->State != XTHREAD_FINISHED ) {
		if ( iDeadline == XRT_DEADLINE_NEVER ) {
			iResult = pthread_cond_wait(&pThread->Condition, &pThread->Lock);
		} else {
			struct timespec tDeadline;

			if ( xrtDeadlineExpired(iDeadline) ) {
				(void)pthread_mutex_unlock(&pThread->Lock);
				return XWAIT_TIMEOUT;
			}
			if ( !__xrtThreadDeadlineTime(pThread, iDeadline, &tDeadline) ) {
				(void)pthread_mutex_unlock(&pThread->Lock);
				return XWAIT_ERROR;
			}
			iResult = pthread_cond_timedwait(&pThread->Condition, &pThread->Lock, &tDeadline);
		}
		if ( iResult == ETIMEDOUT ) {
			continue;
		}
		if ( iResult != 0 ) {
			(void)pthread_mutex_unlock(&pThread->Lock);
			__xrtThreadSetSystemError("wait", iResult, "thread wait failed");
			return XWAIT_ERROR;
		}
	}
	(void)pthread_mutex_unlock(&pThread->Lock);
	return XWAIT_OK;
}
#endif



/* 创建并立即启动线程。 */
XRT_API xthread* xrtThreadCreate(xthreadproc pProc, ptr pData, size_t iStackSize)
{
	xthread* pThread;

	if ( pProc == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pThread = (xthread*)xrtCalloc(1, sizeof(xthread));
	if ( pThread == NULL ) {
		return NULL;
	}
	pThread->RefCount = 2;
	pThread->Proc = pProc;
	pThread->Data = pData;
	pThread->State = XTHREAD_RUNNING;
	#if defined(_WIN32) || defined(_WIN64)
		{
			DWORD iThreadId;

			InitializeCriticalSection(&pThread->Lock);
			InitializeConditionVariable(&pThread->Condition);
			EnterCriticalSection(&pThread->Lock);
			pThread->Handle = CreateThread(
				NULL,
				iStackSize,
				__xrtThreadEntry,
				pThread,
				0,
				&iThreadId
			);
			if ( pThread->Handle == NULL ) {
				int iCode = (int)GetLastError();

				LeaveCriticalSection(&pThread->Lock);
				pThread->RefCount = 1;
				__xrtThreadFree(pThread);
				__xrtThreadSetSystemError("create", iCode, "thread creation failed");
				return NULL;
			}
			pThread->Id = (uint64)iThreadId;
			LeaveCriticalSection(&pThread->Lock);
		}
	#else
		{
			pthread_attr_t tAttr;
			pthread_condattr_t tCondAttr;
			int iResult;

			iResult = pthread_mutex_init(&pThread->Lock, NULL);
			if ( iResult != 0 ) {
				xrtFree(pThread);
				__xrtThreadSetSystemError("create", iResult, "thread state lock initialization failed");
				return NULL;
			}
			iResult = pthread_condattr_init(&tCondAttr);
			if ( iResult != 0 ) {
				(void)pthread_mutex_destroy(&pThread->Lock);
				xrtFree(pThread);
				__xrtThreadSetSystemError("create", iResult, "thread condition initialization failed");
				return NULL;
			}
			#if defined(CLOCK_MONOTONIC) && !defined(__APPLE__)
				if ( pthread_condattr_setclock(&tCondAttr, CLOCK_MONOTONIC) == 0 ) {
					pThread->ConditionMonotonic = true;
				}
			#endif
			iResult = pthread_cond_init(&pThread->Condition, &tCondAttr);
			(void)pthread_condattr_destroy(&tCondAttr);
			if ( iResult != 0 ) {
				(void)pthread_mutex_destroy(&pThread->Lock);
				xrtFree(pThread);
				__xrtThreadSetSystemError("create", iResult, "thread condition initialization failed");
				return NULL;
			}
			iResult = pthread_attr_init(&tAttr);
			if ( iResult != 0 ) {
				pThread->RefCount = 1;
				__xrtThreadFree(pThread);
				__xrtThreadSetSystemError(
					"create",
					iResult,
					"thread attribute initialization failed"
				);
				return NULL;
			}
			iResult = pthread_attr_setdetachstate(&tAttr, PTHREAD_CREATE_DETACHED);
			if ( iResult != 0 ) {
				(void)pthread_attr_destroy(&tAttr);
				pThread->RefCount = 1;
				__xrtThreadFree(pThread);
				__xrtThreadSetSystemError(
					"create",
					iResult,
					"detached thread configuration failed"
				);
				return NULL;
			}
			if ( iStackSize != 0 ) {
				iResult = pthread_attr_setstacksize(&tAttr, iStackSize);
				if ( iResult != 0 ) {
					(void)pthread_attr_destroy(&tAttr);
					pThread->RefCount = 1;
					__xrtThreadFree(pThread);
					__xrtThreadSetSystemError("create", iResult, "thread stack size is invalid");
					return NULL;
				}
			}
			(void)pthread_mutex_lock(&pThread->Lock);
			iResult = pthread_create(&pThread->Handle, &tAttr, __xrtThreadEntry, pThread);
			(void)pthread_attr_destroy(&tAttr);
			if ( iResult != 0 ) {
				(void)pthread_mutex_unlock(&pThread->Lock);
				pThread->RefCount = 1;
				__xrtThreadFree(pThread);
				__xrtThreadSetSystemError("create", iResult, "thread creation failed");
				return NULL;
			}
			pThread->Id = __xrtNativeThreadId(pThread->Handle);
			(void)pthread_mutex_unlock(&pThread->Lock);
		}
	#endif
	return pThread;
}



/* 增加线程对象引用。 */
XRT_API xthread* xrtThreadRef(xthread* pThread)
{
	if ( (pThread == NULL) || (xrtRefRetain(&pThread->RefCount) < 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return pThread;
}



/* 释放线程对象引用。 */
XRT_API void xrtThreadDestroy(xthread* pThread)
{
	__xrtThreadRelease(pThread);
}



/* 等待线程完成。 */
XRT_API xwaitresult xrtThreadWait(xthread* pThread)
{
	return xrtThreadWaitUntil(pThread, XRT_DEADLINE_NEVER);
}



/* 在相对微秒数内等待线程完成。 */
XRT_API xwaitresult xrtThreadWaitFor(xthread* pThread, uint64 iTimeout)
{
	return xrtThreadWaitUntil(pThread, xrtDeadlineAfter(iTimeout));
}



/* 等待线程完成到指定截止时间。 */
XRT_API xwaitresult xrtThreadWaitUntil(xthread* pThread, xdeadline iDeadline)
{
	if ( pThread == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XWAIT_ERROR;
	}
	if ( __xrtThreadCurrentGet() == pThread ) {
		__xrtErrorSetInvalidState();
		return XWAIT_ERROR;
	}
	return __xrtThreadWaitDeadline(pThread, iDeadline);
}



/* 幂等地请求线程协作停止。 */
XRT_API bool xrtThreadStop(xthread* pThread)
{
	if ( pThread == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		(void)InterlockedExchange(&pThread->StopRequested, 1);
	#elif defined(__TINYC__)
		(void)pthread_mutex_lock(&pThread->Lock);
		pThread->StopRequested = 1;
		(void)pthread_mutex_unlock(&pThread->Lock);
	#else
		(void)__sync_lock_test_and_set(&pThread->StopRequested, 1);
	#endif
	return true;
}



/* 判断指定线程是否收到停止请求。 */
XRT_API bool xrtThreadStopRequested(const xthread* pThread)
{
	if ( pThread == NULL ) {
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedCompareExchange((volatile LONG*)&pThread->StopRequested, 0, 0) != 0;
	#elif defined(__TINYC__)
		bool bRequested;

		(void)pthread_mutex_lock((pthread_mutex_t*)&pThread->Lock);
		bRequested = pThread->StopRequested != 0;
		(void)pthread_mutex_unlock((pthread_mutex_t*)&pThread->Lock);
		return bRequested;
	#else
		return __sync_val_compare_and_swap((volatile int32*)&pThread->StopRequested, 0, 0) != 0;
	#endif
}



/* 判断当前 XRT 线程是否收到停止请求。 */
XRT_API bool xrtThreadStopping(void)
{
	return xrtThreadStopRequested(__xrtThreadCurrentGet());
}



/* 返回线程状态快照。 */
XRT_API xthreadstate xrtThreadState(const xthread* pThread)
{
	xthreadstate State;

	if ( pThread == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XTHREAD_FINISHED;
	}
	#if defined(_WIN32) || defined(_WIN64)
		EnterCriticalSection((CRITICAL_SECTION*)&pThread->Lock);
		State = pThread->State;
		LeaveCriticalSection((CRITICAL_SECTION*)&pThread->Lock);
	#else
		(void)pthread_mutex_lock((pthread_mutex_t*)&pThread->Lock);
		State = pThread->State;
		(void)pthread_mutex_unlock((pthread_mutex_t*)&pThread->Lock);
	#endif
	return State;
}



/* 返回完成线程的退出码。 */
XRT_API int32 xrtThreadExitCode(const xthread* pThread)
{
	int32 iExitCode;

	if ( pThread == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	#if defined(_WIN32) || defined(_WIN64)
		EnterCriticalSection((CRITICAL_SECTION*)&pThread->Lock);
		if ( pThread->State != XTHREAD_FINISHED ) {
			LeaveCriticalSection((CRITICAL_SECTION*)&pThread->Lock);
			__xrtErrorSetInvalidState();
			return 0;
		}
		iExitCode = pThread->ExitCode;
		LeaveCriticalSection((CRITICAL_SECTION*)&pThread->Lock);
	#else
		(void)pthread_mutex_lock((pthread_mutex_t*)&pThread->Lock);
		if ( pThread->State != XTHREAD_FINISHED ) {
			(void)pthread_mutex_unlock((pthread_mutex_t*)&pThread->Lock);
			__xrtErrorSetInvalidState();
			return 0;
		}
		iExitCode = pThread->ExitCode;
		(void)pthread_mutex_unlock((pthread_mutex_t*)&pThread->Lock);
	#endif
	return iExitCode;
}



/* 返回线程对象记录的稳定标识。 */
XRT_API uint64 xrtThreadId(const xthread* pThread)
{
	if ( pThread == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	return pThread->Id;
}



/* 返回当前平台线程标识。 */
XRT_API uint64 xrtThreadCurrentId(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (uint64)GetCurrentThreadId();
	#else
		return __xrtCurrentThreadId();
	#endif
}



/* 返回当前 XRT 创建线程的借用对象。 */
XRT_API xthread* xrtThreadCurrent(void)
{
	return __xrtThreadCurrentGet();
}



/* 主动让出当前线程的处理器时间片。 */
XRT_API void xrtThreadYield(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		(void)SwitchToThread();
	#else
		(void)sched_yield();
	#endif
}

#endif
