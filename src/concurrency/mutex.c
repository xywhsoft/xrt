#include "../internal/xrt_sync.h"

#include <errno.h>



#if defined(XRT_FEATURE_MUTEX)

/* 检查互斥锁已经初始化。 */
static xrt_mutex_impl* __xrtMutexRequire(xmutex* pMutex)
{
	xrt_mutex_impl* pImpl;

	if ( pMutex == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pImpl = __xrtMutexImpl(pMutex);
	if ( pImpl->Magic != XRT_MUTEX_MAGIC ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pImpl;
}



/* 初始化调用方存储中的非递归互斥锁。 */
XRT_API bool xrtMutexInit(xmutex* pMutex)
{
	xrt_mutex_impl* pImpl;

	if ( pMutex == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pMutex, 0, sizeof(xmutex));
	pImpl = __xrtMutexImpl(pMutex);
	#if defined(_WIN32) || defined(_WIN64)
		InitializeSRWLock(&pImpl->Lock);
	#else
		{
			pthread_mutexattr_t tAttr;
			int iResult = pthread_mutexattr_init(&tAttr);

			if ( iResult != 0 ) {
				__xrtSyncSetSystemError("mutex.init", iResult, "mutex attribute initialization failed");
				return false;
			}
			iResult = pthread_mutexattr_settype(&tAttr, PTHREAD_MUTEX_ERRORCHECK);
			if ( iResult != 0 ) {
				(void)pthread_mutexattr_destroy(&tAttr);
				__xrtSyncSetSystemError(
					"mutex.init",
					iResult,
					"mutex error-check configuration failed"
				);
				return false;
			}
			iResult = pthread_mutex_init(&pImpl->Lock, &tAttr);
			(void)pthread_mutexattr_destroy(&tAttr);
			if ( iResult != 0 ) {
				__xrtSyncSetSystemError("mutex.init", iResult, "mutex initialization failed");
				return false;
			}
		}
	#endif
	pImpl->Magic = XRT_MUTEX_MAGIC;
	return true;
}



/* 释放互斥锁平台资源。 */
XRT_API bool xrtMutexUnit(xmutex* pMutex)
{
	xrt_mutex_impl* pImpl = __xrtMutexRequire(pMutex);

	if ( pImpl == NULL ) {
		return false;
	}
	if ( __xrtMutexOwnerLoad(pImpl) != 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
	#else
		{
			int iResult = pthread_mutex_destroy(&pImpl->Lock);

			if ( iResult != 0 ) {
				if ( iResult == EBUSY ) {
					__xrtErrorSetInvalidState();
				} else {
					__xrtSyncSetSystemError("mutex.unit", iResult, "mutex destruction failed");
				}
				return false;
			}
		}
	#endif
	memset(pMutex, 0, sizeof(xmutex));
	return true;
}



/* 创建一个非递归互斥锁。 */
XRT_API xmutex* xrtMutexCreate(void)
{
	xmutex* pMutex = (xmutex*)xrtMalloc(sizeof(xmutex));

	if ( pMutex == NULL ) {
		return NULL;
	}
	if ( !xrtMutexInit(pMutex) ) {
		xrtFree(pMutex);
		return NULL;
	}
	return pMutex;
}



/* 释放 Create 返回的互斥锁。 */
XRT_API bool xrtMutexDestroy(xmutex* pMutex)
{
	if ( pMutex == NULL ) {
		return true;
	}
	if ( !xrtMutexUnit(pMutex) ) {
		return false;
	}
	xrtFree(pMutex);
	return true;
}



/* 阻塞到获得互斥锁。 */
XRT_API bool xrtMutexLock(xmutex* pMutex)
{
	xrt_mutex_impl* pImpl = __xrtMutexRequire(pMutex);
	uint64 iCurrent;

	if ( pImpl == NULL ) {
		return false;
	}
	iCurrent = __xrtCurrentThreadId();
	if ( __xrtMutexOwnerLoad(pImpl) == iCurrent ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		AcquireSRWLockExclusive(&pImpl->Lock);
	#else
		{
			int iResult = pthread_mutex_lock(&pImpl->Lock);

			if ( iResult != 0 ) {
				if ( iResult == EDEADLK ) {
					__xrtErrorSetInvalidState();
				} else {
					__xrtSyncSetSystemError("mutex.lock", iResult, "mutex lock failed");
				}
				return false;
			}
		}
	#endif
	__xrtMutexOwnerStore(pImpl, iCurrent);
	return true;
}



/* 尝试获得互斥锁。 */
XRT_API bool xrtMutexTryLock(xmutex* pMutex)
{
	xrt_mutex_impl* pImpl = __xrtMutexRequire(pMutex);
	uint64 iCurrent;

	if ( pImpl == NULL ) {
		return false;
	}
	iCurrent = __xrtCurrentThreadId();
	if ( __xrtMutexOwnerLoad(pImpl) == iCurrent ) {
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( !TryAcquireSRWLockExclusive(&pImpl->Lock) ) {
			return false;
		}
	#else
		{
			int iResult = pthread_mutex_trylock(&pImpl->Lock);

			if ( iResult == EBUSY ) {
				return false;
			}
			if ( iResult != 0 ) {
				__xrtSyncSetSystemError("mutex.try", iResult, "mutex try-lock failed");
				return false;
			}
		}
	#endif
	__xrtMutexOwnerStore(pImpl, iCurrent);
	return true;
}



/* 释放当前线程持有的互斥锁。 */
XRT_API bool xrtMutexUnlock(xmutex* pMutex)
{
	xrt_mutex_impl* pImpl = __xrtMutexRequire(pMutex);
	uint64 iCurrent;

	if ( pImpl == NULL ) {
		return false;
	}
	iCurrent = __xrtCurrentThreadId();
	if ( __xrtMutexOwnerLoad(pImpl) != iCurrent ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	__xrtMutexOwnerStore(pImpl, 0);
	#if defined(_WIN32) || defined(_WIN64)
		ReleaseSRWLockExclusive(&pImpl->Lock);
	#else
		{
			int iResult = pthread_mutex_unlock(&pImpl->Lock);

			if ( iResult != 0 ) {
				__xrtMutexOwnerStore(pImpl, iCurrent);
				if ( iResult == EPERM ) {
					__xrtErrorSetInvalidState();
				} else {
					__xrtSyncSetSystemError("mutex.unlock", iResult, "mutex unlock failed");
				}
				return false;
			}
		}
	#endif
	return true;
}

#endif
