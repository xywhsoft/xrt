


/* ================================ 线程管理 ================================ */

#if !defined(_WIN32) && !defined(_WIN64)
	#if defined(CLOCK_MONOTONIC)
		#define __XRT_THREAD_WAIT_CLOCK CLOCK_MONOTONIC
	#else
		#define __XRT_THREAD_WAIT_CLOCK CLOCK_REALTIME
	#endif

	// 内部函数：构建线程绝对超时时间
	static bool __xrtThreadMakeAbsTimeout(struct timespec* pTs, uint32 iTimeoutMs)
	{
		uint64 iNs;
		if ( !pTs ) { return false; }
		if ( clock_gettime(__XRT_THREAD_WAIT_CLOCK, pTs) != 0 ) { return false; }
		iNs = (uint64)pTs->tv_nsec + ((uint64)iTimeoutMs * 1000000ULL);
		pTs->tv_sec += (time_t)(iNs / 1000000000ULL);
		pTs->tv_nsec = (long)(iNs % 1000000000ULL);
		return true;
	}


	// 内部函数：初始化单调时钟条件变量
	static bool __xrtThreadInitMonotonicCond(pthread_cond_t* pCond)
	{
		pthread_condattr_t tAttr;
		if ( !pCond ) { return false; }
		if ( pthread_condattr_init(&tAttr) != 0 ) { return false; }
		#if defined(CLOCK_MONOTONIC)
			(void)pthread_condattr_setclock(&tAttr, CLOCK_MONOTONIC);
		#endif
		if ( pthread_cond_init(pCond, &tAttr) != 0 ) {
			pthread_condattr_destroy(&tAttr);
			return false;
		}
		pthread_condattr_destroy(&tAttr);
		return true;
	}
#endif

// 内部函数：分配线程对象内存
static inline ptr __xrtThreadObjAlloc(size_t iSize)
{
	ptr (*procMalloc)(size_t) = xCore.malloc ? xCore.malloc : malloc;
	return procMalloc(iSize);
}


// 内部函数：释放线程对象内存
static inline void __xrtThreadObjFree(ptr pMem)
{
	void (*procFree)(ptr) = xCore.free ? xCore.free : free;
	if ( pMem ) {
		procFree(pMem);
	}
}

// 线程包装函数（统一完成 attach / detach / exit code 保存）
#if defined(_WIN32) || defined(_WIN64)
static DWORD WINAPI xrtThreadWrapper(LPVOID lpParameter)
{
	xthread pThread = (xthread)lpParameter;
	uint32 iExitCode = 0;

	if ( pThread == NULL ) {
		return 0;
	}

	__xrtThreadAttachManaged(pThread);

	if ( pThread->Proc ) {
		iExitCode = pThread->Proc(pThread->Param);
	}

	__xrtThreadExitManaged(pThread, iExitCode);

	return iExitCode;
}
#else
// 线程包装函数（统一完成 attach / detach / exit code 保存）
static void* xrtThreadWrapper(void* pParameter)
{
	xthread pThread = (xthread)pParameter;
	uint32 iExitCode = 0;

	if ( pThread == NULL ) {
		return (void*)(uintptr_t)0;
	}

	__xrtThreadAttachManaged(pThread);

	if ( pThread->Proc ) {
		iExitCode = pThread->Proc(pThread->Param);
	}

	__xrtThreadExitManaged(pThread, iExitCode);

	return (void*)(uintptr_t)iExitCode;
}
#endif

// 创建线程
XXAPI xthread xrtThreadCreate(ptr pProc, ptr pParam, size_t iStackSize)
{
	xthread pThread = __xrtThreadObjAlloc(sizeof(xthread_struct));
	if ( !pThread ) { return NULL; }

	pThread->Proc = pProc;
	pThread->Param = pParam;
	pThread->StopFlag = 0;
	pThread->bFinished = FALSE;
	pThread->bJoined = FALSE;
	pThread->bAutoDestroy = FALSE;
	pThread->ExitCode = 0;
	pThread->TID = 0;

	#if defined(_WIN32) || defined(_WIN64)
		DWORD iThreadID;
		pThread->Handle = CreateThread(NULL, iStackSize, xrtThreadWrapper, pThread, 0, &iThreadID);
		pThread->TID = iThreadID;
		if ( !pThread->Handle ) {
			__xrtThreadObjFree(pThread);
			return NULL;
		}
	#else
		pthread_t tid;
		pthread_attr_t attr;
		if ( pthread_mutex_init(&pThread->FinishLock, NULL) != 0 ) {
			__xrtThreadObjFree(pThread);
			return NULL;
		}
		if ( !__xrtThreadInitMonotonicCond(&pThread->FinishCond) ) {
			pthread_mutex_destroy(&pThread->FinishLock);
			__xrtThreadObjFree(pThread);
			return NULL;
		}
		pthread_attr_init(&attr);
		if ( iStackSize > 0 ) {
			pthread_attr_setstacksize(&attr, iStackSize);
		}
		if ( pthread_create(&tid, &attr, xrtThreadWrapper, pThread) != 0 ) {
			pthread_attr_destroy(&attr);
			pthread_cond_destroy(&pThread->FinishCond);
			pthread_mutex_destroy(&pThread->FinishLock);
			__xrtThreadObjFree(pThread);
			return NULL;
		}
		pthread_attr_destroy(&attr);
		pThread->Handle = (ptr)tid;
	#endif

	return pThread;
}

// 销毁线程对象（不终止线程，仅释放管理结构）
XXAPI void xrtThreadDestroy(xthread pThread)
{
	if ( pThread ) {
		if ( !pThread->bFinished && !pThread->bJoined ) {
			xrtSetError("thread is still running.", FALSE);
			return;
		}

		#if defined(_WIN32) || defined(_WIN64)
			if ( pThread->Handle ) {
				CloseHandle(pThread->Handle);
				pThread->Handle = NULL;
			}
		#else
			if ( pThread->Handle && !pThread->bJoined ) {
				void* pExit = NULL;
				pthread_join((pthread_t)pThread->Handle, &pExit);
				pThread->bJoined = TRUE;
			}
			pthread_cond_destroy(&pThread->FinishCond);
			pthread_mutex_destroy(&pThread->FinishLock);
			pThread->Handle = NULL;
		#endif
		__xrtThreadObjFree(pThread);
	}
}

// 等待线程结束
XXAPI void xrtThreadWait(xthread pThread)
{
	if ( !pThread || !pThread->Handle ) { return; }
	if ( pThread->bJoined ) { return; }

	#if defined(_WIN32) || defined(_WIN64)
		WaitForSingleObject(pThread->Handle, INFINITE);
		DWORD iExitCode = 0;
		if ( GetExitCodeThread(pThread->Handle, &iExitCode) ) {
			pThread->ExitCode = (uint32)iExitCode;
		}
		pThread->bFinished = TRUE;
		pThread->bJoined = TRUE;
	#else
		void* pExit = NULL;
		pthread_join((pthread_t)pThread->Handle, &pExit);
		pThread->bFinished = TRUE;
		pThread->bJoined = TRUE;
	#endif
}

// 等待线程结束（带超时，毫秒）
XXAPI int xrtThreadWaitTimeout(xthread pThread, uint32 iTimeout)
{
	if ( !pThread || !pThread->Handle ) { return XRT_WAIT_ERROR; }
	if ( pThread->bJoined ) { return XRT_WAIT_OK; }

	#if defined(_WIN32) || defined(_WIN64)
		DWORD ret = WaitForSingleObject(pThread->Handle, iTimeout);
		if ( ret == WAIT_OBJECT_0 ) {
			DWORD iExitCode = 0;
			if ( GetExitCodeThread(pThread->Handle, &iExitCode) ) {
				pThread->ExitCode = (uint32)iExitCode;
			}
			pThread->bFinished = TRUE;
			pThread->bJoined = TRUE;
			return XRT_WAIT_OK;
		}
		if ( ret == WAIT_TIMEOUT ) { return XRT_WAIT_TIMEOUT; }
		return XRT_WAIT_ERROR;
	#else
		int iRet;
		if ( pthread_mutex_lock(&pThread->FinishLock) != 0 ) {
			return XRT_WAIT_ERROR;
		}
		if ( iTimeout == 0 && !pThread->bFinished ) {
			pthread_mutex_unlock(&pThread->FinishLock);
			return XRT_WAIT_TIMEOUT;
		}
		if ( iTimeout == UINT32_MAX ) {
			while ( !pThread->bFinished ) {
				if ( pthread_cond_wait(&pThread->FinishCond, &pThread->FinishLock) != 0 ) {
					pthread_mutex_unlock(&pThread->FinishLock);
					return XRT_WAIT_ERROR;
				}
			}
		} else if ( !pThread->bFinished ) {
			struct timespec ts;
			if ( !__xrtThreadMakeAbsTimeout(&ts, iTimeout) ) {
				pthread_mutex_unlock(&pThread->FinishLock);
				return XRT_WAIT_ERROR;
			}
			while ( !pThread->bFinished ) {
				iRet = pthread_cond_timedwait(&pThread->FinishCond, &pThread->FinishLock, &ts);
				if ( iRet == ETIMEDOUT ) {
					pthread_mutex_unlock(&pThread->FinishLock);
					return XRT_WAIT_TIMEOUT;
				}
				if ( iRet != 0 ) {
					pthread_mutex_unlock(&pThread->FinishLock);
					return XRT_WAIT_ERROR;
				}
			}
		}
		pthread_mutex_unlock(&pThread->FinishLock);
		if ( !pThread->bJoined ) {
			void* pExit = NULL;
			if ( pthread_join((pthread_t)pThread->Handle, &pExit) != 0 ) {
				return XRT_WAIT_ERROR;
			}
			pThread->bJoined = TRUE;
		}
		return XRT_WAIT_OK;
	#endif
}

// 发送停止信号
XXAPI void xrtThreadStop(xthread pThread)
{
	if ( pThread ) {
		__xrtAtomicStoreU32((volatile uint32*)&pThread->StopFlag, 1u);
	}
}



// 检查是否应该停止
XXAPI bool xrtThreadShouldStop(xthread pThread)
{
	if ( pThread ) {
		return __xrtAtomicLoadU32((const volatile uint32*)&pThread->StopFlag) != 0;
	}
	return FALSE;
}



// 强制终止线程




// 挂起线程




// 恢复线程




// 获取线程状态
XXAPI int xrtThreadGetState(xthread pThread)
{
	if ( !pThread || !pThread->Handle ) { return XRT_THREAD_STOPPED; }
	if ( pThread->bFinished ) { return XRT_THREAD_STOPPED; }
	
	#if defined(_WIN32) || defined(_WIN64)
		DWORD exitCode;
		if ( GetExitCodeThread(pThread->Handle, &exitCode) ) {
			return (exitCode == STILL_ACTIVE) ? XRT_THREAD_RUNNING : XRT_THREAD_STOPPED;
		}
		return XRT_THREAD_STOPPED;
	#else
		return XRT_THREAD_RUNNING;
	#endif
}

// 获取线程退出码
XXAPI uint32 xrtThreadGetExitCode(xthread pThread)
{
	if ( !pThread ) { return 0; }
	return pThread->ExitCode;
}



// 获取当前线程ID
XXAPI uint64 xrtThreadGetCurrentId()
{
	#if defined(_WIN32) || defined(_WIN64)
		return GetCurrentThreadId();
	#else
		return (uint64)(uintptr_t)pthread_self();
	#endif
}



// 让出当前线程的时间片
XXAPI void xrtThreadYield()
{
	#if defined(_WIN32) || defined(_WIN64)
		SwitchToThread();
	#else
		sched_yield();
	#endif
}



/* ================================ 互斥体 ================================ */

// 创建互斥体
XXAPI xmutex xrtMutexCreate()
{
	xmutex pMutex = __xrtThreadObjAlloc(sizeof(xmutex_struct));
	if ( !pMutex ) { return NULL; }
	
	#if defined(_WIN32) || defined(_WIN64)
		InitializeSRWLock(&pMutex->objLock);
		pMutex->iOwnerThreadId = 0;
	#else
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
		int ret = pthread_mutex_init(&pMutex->objLock, &attr);
		pthread_mutexattr_destroy(&attr);
		if ( ret != 0 ) {
			__xrtThreadObjFree(pMutex);
			pMutex = NULL;
			return NULL;
		}
	#endif
	
	return pMutex;
}



// 销毁互斥体
XXAPI void xrtMutexDestroy(xmutex pMutex)
{
	if ( pMutex ) {
		#if defined(_WIN32) || defined(_WIN64)
			if ( pMutex->iOwnerThreadId != 0 ) {
				xrtSetError("mutex is still locked.", FALSE);
				return;
			}
			xrtMutexUnit(pMutex);
		#else
			if ( pthread_mutex_trylock(&pMutex->objLock) != 0 ) {
				xrtSetError("mutex is still locked.", FALSE);
				return;
			}
			pthread_mutex_unlock(&pMutex->objLock);
			if ( pthread_mutex_destroy(&pMutex->objLock) != 0 ) {
				xrtSetError("mutex destroy failed.", FALSE);
				return;
			}
		#endif
		__xrtThreadObjFree(pMutex);
	}
}



// 初始化互斥体
XXAPI void xrtMutexInit(xmutex pMutex)
{
	if ( !pMutex ) { return; }
	
	#if defined(_WIN32) || defined(_WIN64)
		InitializeSRWLock(&pMutex->objLock);
		pMutex->iOwnerThreadId = 0;
	#else
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
		pthread_mutex_init(&pMutex->objLock, &attr);
		pthread_mutexattr_destroy(&attr);
	#endif
}



// 释放互斥体
XXAPI void xrtMutexUnit(xmutex pMutex)
{
	if ( !pMutex ) { return; }
	
	#if defined(_WIN32) || defined(_WIN64)
		if ( pMutex->iOwnerThreadId != 0 ) {
			xrtSetError("mutex is still locked.", FALSE);
			return;
		}
		// SRWLOCK 不需要显式销毁
		pMutex->iOwnerThreadId = 0;
	#else
		int iRet = pthread_mutex_trylock(&pMutex->objLock);
		if ( iRet != 0 ) {
			xrtSetError("mutex is still locked.", FALSE);
			return;
		}
		pthread_mutex_unlock(&pMutex->objLock);
		if ( pthread_mutex_destroy(&pMutex->objLock) != 0 ) {
			xrtSetError("mutex destroy failed.", FALSE);
		}
	#endif
}



// 锁定互斥体
XXAPI void xrtMutexLock(xmutex pMutex)
{
	if ( !pMutex ) { return; }
	
	#if defined(_WIN32) || defined(_WIN64)
		AcquireSRWLockExclusive(&pMutex->objLock);
		pMutex->iOwnerThreadId = GetCurrentThreadId();
	#else
		pthread_mutex_lock(&pMutex->objLock);
	#endif
}



// 尝试锁定互斥体
XXAPI bool xrtMutexTryLock(xmutex pMutex)
{
	if ( !pMutex ) { return FALSE; }
	
	#if defined(_WIN32) || defined(_WIN64)
		if ( pMutex->iOwnerThreadId == GetCurrentThreadId() ) {
			return FALSE;
		}
		if ( TryAcquireSRWLockExclusive(&pMutex->objLock) == 0 ) {
			return FALSE;
		}
		pMutex->iOwnerThreadId = GetCurrentThreadId();
		return TRUE;
	#else
		return pthread_mutex_trylock(&pMutex->objLock) == 0;
	#endif
}



// 解锁互斥体
XXAPI void xrtMutexUnlock(xmutex pMutex)
{
	if ( !pMutex ) { return; }
	
	#if defined(_WIN32) || defined(_WIN64)
		pMutex->iOwnerThreadId = 0;
		ReleaseSRWLockExclusive(&pMutex->objLock);
	#else
		pthread_mutex_unlock(&pMutex->objLock);
	#endif
}



/* ================================ 信号量 ================================ */

// 创建信号量
XXAPI xsem xrtSemCreate(uint32 iInitValue, uint32 iMaxValue)
{
	xsem pSem = __xrtThreadObjAlloc(sizeof(xsem_struct));
	if ( !pSem ) { return NULL; }
	
	#if defined(_WIN32) || defined(_WIN64)
		pSem->objSem = CreateSemaphoreW(NULL, iInitValue, iMaxValue, NULL);
		if ( !pSem->objSem ) {
			__xrtThreadObjFree(pSem);
			return NULL;
		}
	#else
		if ( pthread_mutex_init(&pSem->objLock, NULL) != 0 ) {
			__xrtThreadObjFree(pSem);
			return NULL;
		}
		if ( !__xrtThreadInitMonotonicCond(&pSem->objCond) ) {
			pthread_mutex_destroy(&pSem->objLock);
			__xrtThreadObjFree(pSem);
			return NULL;
		}
		pSem->iValue = iInitValue;
		pSem->iMaxValue = iMaxValue;
	#endif
	
	return pSem;
}

// 销毁信号量
XXAPI void xrtSemDestroy(xsem pSem)
{
	if ( pSem ) {
		xrtSemUnit(pSem);
		__xrtThreadObjFree(pSem);
	}
}



// 初始化信号量
XXAPI void xrtSemInit(xsem pSem, uint32 iInitValue, uint32 iMaxValue)
{
	if ( !pSem ) { return; }
	
	#if defined(_WIN32) || defined(_WIN64)
		pSem->objSem = CreateSemaphoreW(NULL, iInitValue, iMaxValue, NULL);
	#else
		if ( pthread_mutex_init(&pSem->objLock, NULL) != 0 ) { return; }
		if ( !__xrtThreadInitMonotonicCond(&pSem->objCond) ) {
			pthread_mutex_destroy(&pSem->objLock);
			return;
		}
		pSem->iValue = iInitValue;
		pSem->iMaxValue = iMaxValue;
	#endif
}


// xrtSemUnit 相关处理
XXAPI void xrtSemUnit(xsem pSem)
{
	if ( !pSem ) { return; }
	
	#if defined(_WIN32) || defined(_WIN64)
		CloseHandle(pSem->objSem);
		pSem->objSem = NULL;
	#else
		pthread_cond_destroy(&pSem->objCond);
		pthread_mutex_destroy(&pSem->objLock);
		memset(&pSem->objCond, 0, sizeof(pSem->objCond));
		memset(&pSem->objLock, 0, sizeof(pSem->objLock));
		pSem->iValue = 0;
		pSem->iMaxValue = 0;
	#endif
}


// xrtSemWait 相关处理
XXAPI void xrtSemWait(xsem pSem)
{
	if ( !pSem ) { return; }
	
	#if defined(_WIN32) || defined(_WIN64)
		WaitForSingleObject(pSem->objSem, INFINITE);
	#else
		pthread_mutex_lock(&pSem->objLock);
		while ( pSem->iValue == 0 ) {
			pthread_cond_wait(&pSem->objCond, &pSem->objLock);
		}
		pSem->iValue--;
		pthread_mutex_unlock(&pSem->objLock);
	#endif
}


// xrtSemTryWait 相关处理
XXAPI bool xrtSemTryWait(xsem pSem)
{
	if ( !pSem ) { return FALSE; }
	
	#if defined(_WIN32) || defined(_WIN64)
		return WaitForSingleObject(pSem->objSem, 0) == WAIT_OBJECT_0;
	#else
		bool bOk = FALSE;
		pthread_mutex_lock(&pSem->objLock);
		if ( pSem->iValue > 0 ) {
			pSem->iValue--;
			bOk = TRUE;
		}
		pthread_mutex_unlock(&pSem->objLock);
		return bOk;
	#endif
}


// xrtSemWaitTimeout 相关处理
XXAPI int xrtSemWaitTimeout(xsem pSem, uint32 iTimeout)
{
	if ( !pSem ) { return XRT_WAIT_ERROR; }
	
	#if defined(_WIN32) || defined(_WIN64)
		DWORD ret = WaitForSingleObject(pSem->objSem, iTimeout);
		if ( ret == WAIT_OBJECT_0 ) { return XRT_WAIT_OK; }
		if ( ret == WAIT_TIMEOUT ) { return XRT_WAIT_TIMEOUT; }
		return XRT_WAIT_ERROR;
	#else
		struct timespec ts;
		int ret = XRT_WAIT_ERROR;
		if ( !__xrtThreadMakeAbsTimeout(&ts, iTimeout) ) { return XRT_WAIT_ERROR; }
		pthread_mutex_lock(&pSem->objLock);
		while ( pSem->iValue == 0 ) {
			int iWaitRet = pthread_cond_timedwait(&pSem->objCond, &pSem->objLock, &ts);
			if ( iWaitRet == ETIMEDOUT ) {
				ret = XRT_WAIT_TIMEOUT;
				goto exit_wait;
			}
			if ( iWaitRet != 0 ) {
				ret = XRT_WAIT_ERROR;
				goto exit_wait;
			}
		}
		pSem->iValue--;
		ret = XRT_WAIT_OK;
	exit_wait:
		pthread_mutex_unlock(&pSem->objLock);
		return ret;
	#endif
}


// xrtSemPost 相关处理
XXAPI bool xrtSemPost(xsem pSem)
{
	if ( !pSem ) { return FALSE; }
	
	#if defined(_WIN32) || defined(_WIN64)
		return ReleaseSemaphore(pSem->objSem, 1, NULL) != 0;
	#else
		bool bOk = FALSE;
		pthread_mutex_lock(&pSem->objLock);
		if ( pSem->iValue < pSem->iMaxValue ) {
			pSem->iValue++;
			bOk = TRUE;
			pthread_cond_signal(&pSem->objCond);
		}
		pthread_mutex_unlock(&pSem->objLock);
		return bOk;
	#endif
}


// xrtSemPostMultiple 相关处理
XXAPI bool xrtSemPostMultiple(xsem pSem, uint32 iCount)
{
	if ( !pSem || iCount == 0 ) { return FALSE; }
	
	#if defined(_WIN32) || defined(_WIN64)
		return ReleaseSemaphore(pSem->objSem, iCount, NULL) != 0;
	#else
		bool bOk = FALSE;
		uint32 iPosted = 0;
		pthread_mutex_lock(&pSem->objLock);
		while ( iPosted < iCount && pSem->iValue < pSem->iMaxValue ) {
			pSem->iValue++;
			iPosted++;
		}
		if ( iPosted > 0 ) {
			pthread_cond_broadcast(&pSem->objCond);
			bOk = (iPosted == iCount);
		}
		pthread_mutex_unlock(&pSem->objLock);
		return bOk;
	#endif
}


// xrtCondCreate 相关处理
XXAPI xcond xrtCondCreate()
{
	xcond pCond = __xrtThreadObjAlloc(sizeof(xcond_struct));
	if ( !pCond ) { return NULL; }
	
	#if defined(_WIN32) || defined(_WIN64)
		InitializeConditionVariable(&pCond->objCond);
	#else
		if ( !__xrtThreadInitMonotonicCond(&pCond->objCond) ) {
			__xrtThreadObjFree(pCond);
			return NULL;
		}
	#endif
	
	return pCond;
}

// 销毁条件变量
XXAPI void xrtCondDestroy(xcond pCond)
{
	if ( pCond ) {
		xrtCondUnit(pCond);
		__xrtThreadObjFree(pCond);
	}
}



// 初始化条件变量
XXAPI void xrtCondInit(xcond pCond)
{
	if ( !pCond ) { return; }
	
	#if defined(_WIN32) || defined(_WIN64)
		InitializeConditionVariable(&pCond->objCond);
	#else
		if ( !__xrtThreadInitMonotonicCond(&pCond->objCond) ) {
			memset(&pCond->objCond, 0, sizeof(pCond->objCond));
		}
	#endif
}

// 释放条件变量
XXAPI void xrtCondUnit(xcond pCond)
{
	if ( !pCond ) { return; }
	
	#if defined(_WIN32) || defined(_WIN64)
		// Windows CONDITION_VARIABLE 不需要显式销毁
		memset(&pCond->objCond, 0, sizeof(pCond->objCond));
	#else
		pthread_cond_destroy(&pCond->objCond);
		memset(&pCond->objCond, 0, sizeof(pCond->objCond));
	#endif
}



// 等待条件变量
XXAPI void xrtCondWait(xcond pCond, xmutex pMutex)
{
	if ( !pCond || !pMutex ) { return; }
	
	#if defined(_WIN32) || defined(_WIN64)
		pMutex->iOwnerThreadId = 0;
		SleepConditionVariableSRW(&pCond->objCond, 
			&pMutex->objLock, INFINITE, 0);
		pMutex->iOwnerThreadId = GetCurrentThreadId();
	#else
		pthread_cond_wait(&pCond->objCond, 
			(pthread_mutex_t*)&pMutex->objLock);
	#endif
}



// 等待条件变量（带超时）
XXAPI int xrtCondWaitTimeout(xcond pCond, xmutex pMutex, uint32 iTimeout)
{
	if ( !pCond || !pMutex ) {
		return XRT_WAIT_ERROR;
	}
	
	#if defined(_WIN32) || defined(_WIN64)
		pMutex->iOwnerThreadId = 0;
		if ( SleepConditionVariableSRW(&pCond->objCond, 
				&pMutex->objLock, iTimeout, 0) ) {
			pMutex->iOwnerThreadId = GetCurrentThreadId();
			return XRT_WAIT_OK;
		}
		pMutex->iOwnerThreadId = GetCurrentThreadId();
		if ( GetLastError() == ERROR_TIMEOUT ) {
			return XRT_WAIT_TIMEOUT;
		}
		return XRT_WAIT_ERROR;
	#else
		struct timespec ts;
		if ( !__xrtThreadMakeAbsTimeout(&ts, iTimeout) ) {
			return XRT_WAIT_ERROR;
		}
		int ret = pthread_cond_timedwait(&pCond->objCond, 
			(pthread_mutex_t*)&pMutex->objLock, &ts);
		if ( ret == 0 ) { return XRT_WAIT_OK; }
		if ( ret == ETIMEDOUT ) { return XRT_WAIT_TIMEOUT; }
		return XRT_WAIT_ERROR;
	#endif
}

// 唤醒一个等待的线程
XXAPI void xrtCondSignal(xcond pCond)
{
	if ( !pCond ) { return; }
	
	#if defined(_WIN32) || defined(_WIN64)
		WakeConditionVariable(&pCond->objCond);
	#else
		pthread_cond_signal(&pCond->objCond);
	#endif
}



// 唤醒所有等待的线程
XXAPI void xrtCondBroadcast(xcond pCond)
{
	if ( !pCond ) { return; }
	
	#if defined(_WIN32) || defined(_WIN64)
		WakeAllConditionVariable(&pCond->objCond);
	#else
		pthread_cond_broadcast(&pCond->objCond);
	#endif
}



/* ================================ 读写锁实现 ================================ */

// 内部函数：__xrtRWLockStateLock
static inline void __xrtRWLockStateLock(xrwlock pRWLock)
{
	#if defined(_WIN32) || defined(_WIN64)
		EnterCriticalSection(&pRWLock->objStateLock);
	#else
		pthread_mutex_lock(&pRWLock->objStateLock);
	#endif
}


// 内部函数：__xrtRWLockStateUnlock
static inline void __xrtRWLockStateUnlock(xrwlock pRWLock)
{
	#if defined(_WIN32) || defined(_WIN64)
		LeaveCriticalSection(&pRWLock->objStateLock);
	#else
		pthread_mutex_unlock(&pRWLock->objStateLock);
	#endif
}


// 内部函数：__xrtRWLockWaitReader
static inline void __xrtRWLockWaitReader(xrwlock pRWLock)
{
	#if defined(_WIN32) || defined(_WIN64)
		SleepConditionVariableCS(&pRWLock->objReadCond, &pRWLock->objStateLock, INFINITE);
	#else
		pthread_cond_wait(&pRWLock->objReadCond, &pRWLock->objStateLock);
	#endif
}


// 内部函数：__xrtRWLockWaitWriter
static inline void __xrtRWLockWaitWriter(xrwlock pRWLock)
{
	#if defined(_WIN32) || defined(_WIN64)
		SleepConditionVariableCS(&pRWLock->objWriteCond, &pRWLock->objStateLock, INFINITE);
	#else
		pthread_cond_wait(&pRWLock->objWriteCond, &pRWLock->objStateLock);
	#endif
}


// 内部函数：__xrtRWLockSignalWriter
static inline void __xrtRWLockSignalWriter(xrwlock pRWLock)
{
	#if defined(_WIN32) || defined(_WIN64)
		WakeConditionVariable(&pRWLock->objWriteCond);
	#else
		pthread_cond_signal(&pRWLock->objWriteCond);
	#endif
}


// 内部函数：__xrtRWLockBroadcastReaders
static inline void __xrtRWLockBroadcastReaders(xrwlock pRWLock)
{
	#if defined(_WIN32) || defined(_WIN64)
		WakeAllConditionVariable(&pRWLock->objReadCond);
	#else
		pthread_cond_broadcast(&pRWLock->objReadCond);
	#endif
}

// 创建读写锁
XXAPI xrwlock xrtRWLockCreate()
{
	xrwlock pRWLock = __xrtThreadObjAlloc(sizeof(xrwlock_struct));
	if ( !pRWLock ) { return NULL; }
	xrtRWLockInit(pRWLock);
	return pRWLock;
}


// 销毁读写锁
XXAPI void xrtRWLockDestroy(xrwlock pRWLock)
{
	if ( pRWLock ) {
		xrtRWLockUnit(pRWLock);
		__xrtThreadObjFree(pRWLock);
	}
}


// 初始化读写锁（对自维护结构体指针使用）
XXAPI void xrtRWLockInit(xrwlock pRWLock)
{
	if ( !pRWLock ) { return; }
	
	memset(pRWLock, 0, sizeof(*pRWLock));
	
	#if defined(_WIN32) || defined(_WIN64)
		InitializeCriticalSection(&pRWLock->objStateLock);
		InitializeConditionVariable(&pRWLock->objReadCond);
		InitializeConditionVariable(&pRWLock->objWriteCond);
	#else
		pthread_mutex_init(&pRWLock->objStateLock, NULL);
		pthread_cond_init(&pRWLock->objReadCond, NULL);
		pthread_cond_init(&pRWLock->objWriteCond, NULL);
	#endif
}


// 释放读写锁（对自维护结构体指针使用）
XXAPI void xrtRWLockUnit(xrwlock pRWLock)
{
	if ( !pRWLock ) { return; }
	
	#if defined(_WIN32) || defined(_WIN64)
		DeleteCriticalSection(&pRWLock->objStateLock);
		memset(pRWLock, 0, sizeof(*pRWLock));
	#else
		pthread_cond_destroy(&pRWLock->objWriteCond);
		pthread_cond_destroy(&pRWLock->objReadCond);
		pthread_mutex_destroy(&pRWLock->objStateLock);
		memset(pRWLock, 0, sizeof(*pRWLock));
	#endif
}


// 获取读锁（阻塞）
XXAPI void xrtRWLockReadLock(xrwlock pRWLock)
{
	if ( !pRWLock ) { return; }
	
	__xrtRWLockStateLock(pRWLock);
	pRWLock->iWaitingReaderCount++;
	while ( pRWLock->bWriterLocked || (pRWLock->iWaitingWriterCount > 0) ) {
		__xrtRWLockWaitReader(pRWLock);
	}
	pRWLock->iWaitingReaderCount--;
	pRWLock->iReaderCount++;
	__xrtRWLockStateUnlock(pRWLock);
}


// 尝试获取读锁（非阻塞）
XXAPI bool xrtRWLockTryReadLock(xrwlock pRWLock)
{
	if ( !pRWLock ) { return FALSE; }
	
	bool bResult;
	__xrtRWLockStateLock(pRWLock);
	bResult = !pRWLock->bWriterLocked && (pRWLock->iWaitingWriterCount == 0);
	if ( bResult ) {
		pRWLock->iReaderCount++;
	}
	__xrtRWLockStateUnlock(pRWLock);
	
	return bResult != FALSE;
}


// 释放读锁
XXAPI void xrtRWLockReadUnlock(xrwlock pRWLock)
{
	if ( !pRWLock ) { return; }
	
	__xrtRWLockStateLock(pRWLock);
	if ( pRWLock->iReaderCount > 0 ) {
		pRWLock->iReaderCount--;
		if ( pRWLock->iReaderCount == 0 ) {
			if ( pRWLock->iWaitingWriterCount > 0 ) {
				__xrtRWLockSignalWriter(pRWLock);
			} else if ( pRWLock->iWaitingReaderCount > 0 ) {
				__xrtRWLockBroadcastReaders(pRWLock);
			}
		}
	}
	__xrtRWLockStateUnlock(pRWLock);
}


// 获取写锁（阻塞）
XXAPI void xrtRWLockWriteLock(xrwlock pRWLock)
{
	if ( !pRWLock ) { return; }
	
	__xrtRWLockStateLock(pRWLock);
	pRWLock->iWaitingWriterCount++;
	while ( pRWLock->bWriterLocked || (pRWLock->iReaderCount > 0) ) {
		__xrtRWLockWaitWriter(pRWLock);
	}
	pRWLock->iWaitingWriterCount--;
	pRWLock->bWriterLocked = TRUE;
	__xrtRWLockStateUnlock(pRWLock);
}


// 尝试获取写锁（非阻塞）
XXAPI bool xrtRWLockTryWriteLock(xrwlock pRWLock)
{
	if ( !pRWLock ) { return FALSE; }
	
	bool bResult;
	__xrtRWLockStateLock(pRWLock);
	bResult = !pRWLock->bWriterLocked && (pRWLock->iReaderCount == 0);
	if ( bResult ) {
		pRWLock->bWriterLocked = TRUE;
	}
	__xrtRWLockStateUnlock(pRWLock);
	
	return bResult != FALSE;
}


// 释放写锁
XXAPI void xrtRWLockWriteUnlock(xrwlock pRWLock)
{
	if ( !pRWLock ) { return; }
	
	__xrtRWLockStateLock(pRWLock);
	if ( pRWLock->bWriterLocked ) {
		pRWLock->bWriterLocked = FALSE;
		if ( pRWLock->iWaitingWriterCount > 0 ) {
			__xrtRWLockSignalWriter(pRWLock);
		} else if ( pRWLock->iWaitingReaderCount > 0 ) {
			__xrtRWLockBroadcastReaders(pRWLock);
		}
	}
	__xrtRWLockStateUnlock(pRWLock);
}


// 写锁降级为读锁（原子保留锁状态）
XXAPI void xrtRWLockDowngrade(xrwlock pRWLock)
{
	if ( !pRWLock ) { return; }
	
	__xrtRWLockStateLock(pRWLock);
	if ( pRWLock->bWriterLocked ) {
		pRWLock->bWriterLocked = FALSE;
		pRWLock->iReaderCount++;
		if ( (pRWLock->iWaitingWriterCount == 0) && (pRWLock->iWaitingReaderCount > 0) ) {
			__xrtRWLockBroadcastReaders(pRWLock);
		}
	}
	__xrtRWLockStateUnlock(pRWLock);
}


// 读锁升级为写锁（原子转换，返回 FALSE 表示锁状态无效）
XXAPI bool xrtRWLockUpgrade(xrwlock pRWLock)
{
	if ( !pRWLock ) { return FALSE; }
	
	__xrtRWLockStateLock(pRWLock);
	if ( pRWLock->iReaderCount == 0 ) {
		__xrtRWLockStateUnlock(pRWLock);
		return FALSE;
	}
	pRWLock->iWaitingWriterCount++;
	pRWLock->iReaderCount--;
	while ( pRWLock->bWriterLocked || (pRWLock->iReaderCount > 0) ) {
		__xrtRWLockWaitWriter(pRWLock);
	}
	pRWLock->iWaitingWriterCount--;
	pRWLock->bWriterLocked = TRUE;
	__xrtRWLockStateUnlock(pRWLock);
	return TRUE;
}



