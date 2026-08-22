#include "../internal/xrt_sync.h"



#if defined(XRT_FEATURE_RWLOCK)

/* 检查读写锁已经初始化。 */
static xrt_rwlock_impl* __xrtRWLockRequire(xrwlock* pLock)
{
	xrt_rwlock_impl* pImpl;

	if ( pLock == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pImpl = __xrtRWLockImpl(pLock);
	if ( pImpl->Magic != XRT_RWLOCK_MAGIC ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pImpl;
}



/* 锁定读写锁内部状态。 */
static bool __xrtRWLockStateLock(xrt_rwlock_impl* pImpl)
{
	#if defined(_WIN32) || defined(_WIN64)
		EnterCriticalSection(&pImpl->Lock);
		return true;
	#else
		int iResult = pthread_mutex_lock(&pImpl->Lock);

		if ( iResult != 0 ) {
			__xrtSyncSetSystemError("rwlock.state", iResult, "rwlock state lock failed");
			return false;
		}
		return true;
	#endif
}



/* 解锁读写锁内部状态。 */
static void __xrtRWLockStateUnlock(xrt_rwlock_impl* pImpl)
{
	#if defined(_WIN32) || defined(_WIN64)
		LeaveCriticalSection(&pImpl->Lock);
	#else
		(void)pthread_mutex_unlock(&pImpl->Lock);
	#endif
}



/* 等待读者条件。 */
static bool __xrtRWLockWaitReader(xrt_rwlock_impl* pImpl)
{
	#if defined(_WIN32) || defined(_WIN64)
		if ( SleepConditionVariableCS(&pImpl->ReadCondition, &pImpl->Lock, INFINITE) ) {
			return true;
		}
		__xrtSyncSetSystemError("rwlock.read", (int)GetLastError(), "rwlock reader wait failed");
		return false;
	#else
		int iResult = pthread_cond_wait(&pImpl->ReadCondition, &pImpl->Lock);

		if ( iResult != 0 ) {
			__xrtSyncSetSystemError("rwlock.read", iResult, "rwlock reader wait failed");
			return false;
		}
		return true;
	#endif
}



/* 等待写者条件。 */
static bool __xrtRWLockWaitWriter(xrt_rwlock_impl* pImpl)
{
	#if defined(_WIN32) || defined(_WIN64)
		if ( SleepConditionVariableCS(&pImpl->WriteCondition, &pImpl->Lock, INFINITE) ) {
			return true;
		}
		__xrtSyncSetSystemError("rwlock.write", (int)GetLastError(), "rwlock writer wait failed");
		return false;
	#else
		int iResult = pthread_cond_wait(&pImpl->WriteCondition, &pImpl->Lock);

		if ( iResult != 0 ) {
			__xrtSyncSetSystemError("rwlock.write", iResult, "rwlock writer wait failed");
			return false;
		}
		return true;
	#endif
}



/* 唤醒一个写者。 */
static void __xrtRWLockWakeWriter(xrt_rwlock_impl* pImpl)
{
	#if defined(_WIN32) || defined(_WIN64)
		WakeConditionVariable(&pImpl->WriteCondition);
	#else
		(void)pthread_cond_signal(&pImpl->WriteCondition);
	#endif
}



/* 唤醒全部读者。 */
static void __xrtRWLockWakeReaders(xrt_rwlock_impl* pImpl)
{
	#if defined(_WIN32) || defined(_WIN64)
		WakeAllConditionVariable(&pImpl->ReadCondition);
	#else
		(void)pthread_cond_broadcast(&pImpl->ReadCondition);
	#endif
}



/* 初始化写者优先的读写锁。 */
XRT_API bool xrtRWLockInit(xrwlock* pLock)
{
	xrt_rwlock_impl* pImpl;

	if ( pLock == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pLock, 0, sizeof(xrwlock));
	pImpl = __xrtRWLockImpl(pLock);
	#if defined(_WIN32) || defined(_WIN64)
		InitializeCriticalSection(&pImpl->Lock);
		InitializeConditionVariable(&pImpl->ReadCondition);
		InitializeConditionVariable(&pImpl->WriteCondition);
	#else
		{
			int iResult = pthread_mutex_init(&pImpl->Lock, NULL);

			if ( iResult != 0 ) {
				__xrtSyncSetSystemError("rwlock.init", iResult, "rwlock state initialization failed");
				return false;
			}
			iResult = pthread_cond_init(&pImpl->ReadCondition, NULL);
			if ( iResult != 0 ) {
				(void)pthread_mutex_destroy(&pImpl->Lock);
				__xrtSyncSetSystemError("rwlock.init", iResult, "rwlock reader condition failed");
				return false;
			}
			iResult = pthread_cond_init(&pImpl->WriteCondition, NULL);
			if ( iResult != 0 ) {
				(void)pthread_cond_destroy(&pImpl->ReadCondition);
				(void)pthread_mutex_destroy(&pImpl->Lock);
				__xrtSyncSetSystemError("rwlock.init", iResult, "rwlock writer condition failed");
				return false;
			}
		}
	#endif
	pImpl->Magic = XRT_RWLOCK_MAGIC;
	return true;
}



/* 释放读写锁平台资源。 */
XRT_API bool xrtRWLockUnit(xrwlock* pLock)
{
	xrt_rwlock_impl* pImpl = __xrtRWLockRequire(pLock);
	bool bBusy;

	if ( pImpl == NULL ) {
		return false;
	}
	if ( !__xrtRWLockStateLock(pImpl) ) {
		return false;
	}
	bBusy = pImpl->Writer || (pImpl->Readers != 0) ||
		(pImpl->WaitingReaders != 0) || (pImpl->WaitingWriters != 0);
	__xrtRWLockStateUnlock(pImpl);
	if ( bBusy ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		DeleteCriticalSection(&pImpl->Lock);
	#else
		{
			int iResult = pthread_cond_destroy(&pImpl->ReadCondition);

			if ( iResult == 0 ) {
				iResult = pthread_cond_destroy(&pImpl->WriteCondition);
			}
			if ( iResult == 0 ) {
				iResult = pthread_mutex_destroy(&pImpl->Lock);
			}
			if ( iResult != 0 ) {
				__xrtSyncSetSystemError("rwlock.unit", iResult, "rwlock destruction failed");
				return false;
			}
		}
	#endif
	memset(pLock, 0, sizeof(xrwlock));
	return true;
}



/* 创建写者优先的读写锁。 */
XRT_API xrwlock* xrtRWLockCreate(void)
{
	xrwlock* pLock = (xrwlock*)xrtMalloc(sizeof(xrwlock));

	if ( pLock == NULL ) {
		return NULL;
	}
	if ( !xrtRWLockInit(pLock) ) {
		xrtFree(pLock);
		return NULL;
	}
	return pLock;
}



/* 释放 Create 返回的读写锁。 */
XRT_API bool xrtRWLockDestroy(xrwlock* pLock)
{
	if ( pLock == NULL ) {
		return true;
	}
	if ( !xrtRWLockUnit(pLock) ) {
		return false;
	}
	xrtFree(pLock);
	return true;
}



/* 获得共享读锁。 */
XRT_API bool xrtRWLockRead(xrwlock* pLock)
{
	xrt_rwlock_impl* pImpl = __xrtRWLockRequire(pLock);

	if ( (pImpl == NULL) || !__xrtRWLockStateLock(pImpl) ) {
		return false;
	}
	pImpl->WaitingReaders++;
	while ( pImpl->Writer || (pImpl->WaitingWriters != 0) ) {
		if ( !__xrtRWLockWaitReader(pImpl) ) {
			pImpl->WaitingReaders--;
			__xrtRWLockStateUnlock(pImpl);
			return false;
		}
	}
	pImpl->WaitingReaders--;
	pImpl->Readers++;
	__xrtRWLockStateUnlock(pImpl);
	return true;
}



/* 尝试获得共享读锁。 */
XRT_API bool xrtRWLockTryRead(xrwlock* pLock)
{
	xrt_rwlock_impl* pImpl = __xrtRWLockRequire(pLock);
	bool bResult;

	if ( (pImpl == NULL) || !__xrtRWLockStateLock(pImpl) ) {
		return false;
	}
	bResult = !pImpl->Writer && (pImpl->WaitingWriters == 0);
	if ( bResult ) {
		pImpl->Readers++;
	}
	__xrtRWLockStateUnlock(pImpl);
	return bResult;
}



/* 释放一个读锁。 */
XRT_API bool xrtRWLockReadUnlock(xrwlock* pLock)
{
	xrt_rwlock_impl* pImpl = __xrtRWLockRequire(pLock);

	if ( (pImpl == NULL) || !__xrtRWLockStateLock(pImpl) ) {
		return false;
	}
	if ( pImpl->Readers == 0 ) {
		__xrtRWLockStateUnlock(pImpl);
		__xrtErrorSetInvalidState();
		return false;
	}
	pImpl->Readers--;
	if ( (pImpl->Readers == 0) && (pImpl->WaitingWriters != 0) ) {
		__xrtRWLockWakeWriter(pImpl);
	}
	__xrtRWLockStateUnlock(pImpl);
	return true;
}



/* 获得独占写锁。 */
XRT_API bool xrtRWLockWrite(xrwlock* pLock)
{
	xrt_rwlock_impl* pImpl = __xrtRWLockRequire(pLock);
	uint64 iCurrent = __xrtCurrentThreadId();

	if ( (pImpl == NULL) || !__xrtRWLockStateLock(pImpl) ) {
		return false;
	}
	if ( pImpl->Writer && (pImpl->WriterId == iCurrent) ) {
		__xrtRWLockStateUnlock(pImpl);
		__xrtErrorSetInvalidState();
		return false;
	}
	pImpl->WaitingWriters++;
	while ( pImpl->Writer || (pImpl->Readers != 0) ) {
		if ( !__xrtRWLockWaitWriter(pImpl) ) {
			pImpl->WaitingWriters--;
			__xrtRWLockStateUnlock(pImpl);
			return false;
		}
	}
	pImpl->WaitingWriters--;
	pImpl->Writer = true;
	pImpl->WriterId = iCurrent;
	__xrtRWLockStateUnlock(pImpl);
	return true;
}



/* 尝试获得独占写锁。 */
XRT_API bool xrtRWLockTryWrite(xrwlock* pLock)
{
	xrt_rwlock_impl* pImpl = __xrtRWLockRequire(pLock);
	bool bResult;

	if ( (pImpl == NULL) || !__xrtRWLockStateLock(pImpl) ) {
		return false;
	}
	bResult = !pImpl->Writer && (pImpl->Readers == 0);
	if ( bResult ) {
		pImpl->Writer = true;
		pImpl->WriterId = __xrtCurrentThreadId();
	}
	__xrtRWLockStateUnlock(pImpl);
	return bResult;
}



/* 释放当前线程持有的写锁。 */
XRT_API bool xrtRWLockWriteUnlock(xrwlock* pLock)
{
	xrt_rwlock_impl* pImpl = __xrtRWLockRequire(pLock);

	if ( (pImpl == NULL) || !__xrtRWLockStateLock(pImpl) ) {
		return false;
	}
	if ( !pImpl->Writer || (pImpl->WriterId != __xrtCurrentThreadId()) ) {
		__xrtRWLockStateUnlock(pImpl);
		__xrtErrorSetInvalidState();
		return false;
	}
	pImpl->Writer = false;
	pImpl->WriterId = 0;
	if ( pImpl->WaitingWriters != 0 ) {
		__xrtRWLockWakeWriter(pImpl);
	} else if ( pImpl->WaitingReaders != 0 ) {
		__xrtRWLockWakeReaders(pImpl);
	}
	__xrtRWLockStateUnlock(pImpl);
	return true;
}



/* 原子地把当前线程的写锁降级为一个读锁。 */
XRT_API bool xrtRWLockDowngrade(xrwlock* pLock)
{
	xrt_rwlock_impl* pImpl = __xrtRWLockRequire(pLock);

	if ( (pImpl == NULL) || !__xrtRWLockStateLock(pImpl) ) {
		return false;
	}
	if ( !pImpl->Writer || (pImpl->WriterId != __xrtCurrentThreadId()) ) {
		__xrtRWLockStateUnlock(pImpl);
		__xrtErrorSetInvalidState();
		return false;
	}
	pImpl->Writer = false;
	pImpl->WriterId = 0;
	pImpl->Readers = 1;
	if ( (pImpl->WaitingWriters == 0) && (pImpl->WaitingReaders != 0) ) {
		__xrtRWLockWakeReaders(pImpl);
	}
	__xrtRWLockStateUnlock(pImpl);
	return true;
}



/* 释放调用方的一个读锁并排队获得写锁。 */
XRT_API bool xrtRWLockUpgrade(xrwlock* pLock)
{
	xrt_rwlock_impl* pImpl = __xrtRWLockRequire(pLock);
	uint64 iCurrent = __xrtCurrentThreadId();

	if ( (pImpl == NULL) || !__xrtRWLockStateLock(pImpl) ) {
		return false;
	}
	if ( pImpl->Writer || (pImpl->Readers == 0) ) {
		__xrtRWLockStateUnlock(pImpl);
		__xrtErrorSetInvalidState();
		return false;
	}
	pImpl->Readers--;
	pImpl->WaitingWriters++;
	while ( pImpl->Writer || (pImpl->Readers != 0) ) {
		if ( !__xrtRWLockWaitWriter(pImpl) ) {
			pImpl->WaitingWriters--;
			pImpl->Readers++;
			__xrtRWLockStateUnlock(pImpl);
			return false;
		}
	}
	pImpl->WaitingWriters--;
	pImpl->Writer = true;
	pImpl->WriterId = iCurrent;
	__xrtRWLockStateUnlock(pImpl);
	return true;
}

#endif
