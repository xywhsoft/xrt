#include "../internal/xrt_sync.h"

#include <errno.h>



#if defined(XRT_FEATURE_COND)

/* 检查条件变量已经初始化。 */
static xrt_cond_impl* __xrtCondRequire(xcond* pCond)
{
	xrt_cond_impl* pImpl;

	if ( pCond == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pImpl = __xrtCondImpl(pCond);
	if ( pImpl->Magic != XRT_COND_MAGIC ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pImpl;
}



/* 检查条件等待使用的互斥锁已经初始化并由当前线程持有。 */
static xrt_mutex_impl* __xrtCondMutexRequire(xmutex* pMutex)
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
	if ( __xrtMutexOwnerLoad(pImpl) != __xrtCurrentThreadId() ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pImpl;
}



/* 初始化调用方存储中的条件变量。 */
XRT_API bool xrtCondInit(xcond* pCond)
{
	xrt_cond_impl* pImpl;

	if ( pCond == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pCond, 0, sizeof(xcond));
	pImpl = __xrtCondImpl(pCond);
	#if defined(_WIN32) || defined(_WIN64)
		InitializeConditionVariable(&pImpl->Condition);
	#else
		{
			pthread_condattr_t tAttr;
			int iResult = pthread_condattr_init(&tAttr);

			if ( iResult != 0 ) {
				__xrtSyncSetSystemError("cond.init", iResult, "condition attribute initialization failed");
				return false;
			}
			#if defined(CLOCK_MONOTONIC) && !defined(__APPLE__)
				if ( pthread_condattr_setclock(&tAttr, CLOCK_MONOTONIC) == 0 ) {
					pImpl->Monotonic = true;
				}
			#endif
			iResult = pthread_cond_init(&pImpl->Condition, &tAttr);
			(void)pthread_condattr_destroy(&tAttr);
			if ( iResult != 0 ) {
				__xrtSyncSetSystemError("cond.init", iResult, "condition initialization failed");
				return false;
			}
		}
	#endif
	pImpl->Magic = XRT_COND_MAGIC;
	return true;
}



/* 释放条件变量平台资源。 */
XRT_API bool xrtCondUnit(xcond* pCond)
{
	xrt_cond_impl* pImpl = __xrtCondRequire(pCond);

	if ( pImpl == NULL ) {
		return false;
	}
	#if !defined(_WIN32) && !defined(_WIN64)
		{
			int iResult = pthread_cond_destroy(&pImpl->Condition);

			if ( iResult != 0 ) {
				__xrtSyncSetSystemError("cond.unit", iResult, "condition destruction failed");
				return false;
			}
		}
	#endif
	memset(pCond, 0, sizeof(xcond));
	return true;
}



/* 创建条件变量。 */
XRT_API xcond* xrtCondCreate(void)
{
	xcond* pCond = (xcond*)xrtMalloc(sizeof(xcond));

	if ( pCond == NULL ) {
		return NULL;
	}
	if ( !xrtCondInit(pCond) ) {
		xrtFree(pCond);
		return NULL;
	}
	return pCond;
}



/* 释放 Create 返回的条件变量。 */
XRT_API bool xrtCondDestroy(xcond* pCond)
{
	if ( pCond == NULL ) {
		return true;
	}
	if ( !xrtCondUnit(pCond) ) {
		return false;
	}
	xrtFree(pCond);
	return true;
}



/* 原子释放 mutex 并等待通知。 */
XRT_API xwaitresult xrtCondWait(xcond* pCond, xmutex* pMutex)
{
	return xrtCondWaitUntil(pCond, pMutex, XRT_DEADLINE_NEVER);
}



/* 在相对微秒数内等待通知。 */
XRT_API xwaitresult xrtCondWaitFor(xcond* pCond, xmutex* pMutex, uint64 iTimeout)
{
	return xrtCondWaitUntil(pCond, pMutex, xrtDeadlineAfter(iTimeout));
}



/* 等待通知到指定单调时钟截止时间。 */
XRT_API xwaitresult xrtCondWaitUntil(
	xcond* pCond,
	xmutex* pMutex,
	xdeadline iDeadline
)
{
	xrt_cond_impl* pCondImpl = __xrtCondRequire(pCond);
	xrt_mutex_impl* pMutexImpl = __xrtCondMutexRequire(pMutex);

	if ( (pCondImpl == NULL) || (pMutexImpl == NULL) ) {
		return XWAIT_ERROR;
	}
	#if defined(_WIN32) || defined(_WIN64)
		{
			uint64 iRemaining = xrtDeadlineRemaining(iDeadline);
			DWORD iMilliseconds;
			BOOL bResult;
			int iCode;

			if ( iRemaining == 0 ) {
				return XWAIT_TIMEOUT;
			}
			iMilliseconds = iRemaining == UINT64_MAX ? INFINITE :
				(DWORD)__xrtWaitMilliseconds(iRemaining);
			__xrtMutexOwnerStore(pMutexImpl, 0);
			bResult = SleepConditionVariableSRW(
				&pCondImpl->Condition,
				&pMutexImpl->Lock,
				iMilliseconds,
				0
			);
			iCode = bResult ? 0 : (int)GetLastError();
			__xrtMutexOwnerStore(pMutexImpl, __xrtCurrentThreadId());
			if ( bResult ) {
				return XWAIT_OK;
			}
			if ( iCode == ERROR_TIMEOUT ) {
				return XWAIT_TIMEOUT;
			}
			__xrtSyncSetSystemError("cond.wait", iCode, "condition wait failed");
			return XWAIT_ERROR;
		}
	#else
		{
			struct timespec tDeadline;
			int iResult;

			if ( iDeadline != XRT_DEADLINE_NEVER ) {
				if ( xrtDeadlineExpired(iDeadline) ) {
					return XWAIT_TIMEOUT;
				}
				if ( !__xrtSyncDeadlineTime(
					iDeadline,
					pCondImpl->Monotonic,
					&tDeadline
				) ) {
					return XWAIT_ERROR;
				}
			}
			__xrtMutexOwnerStore(pMutexImpl, 0);
			if ( iDeadline == XRT_DEADLINE_NEVER ) {
				iResult = pthread_cond_wait(&pCondImpl->Condition, &pMutexImpl->Lock);
			} else {
				iResult = pthread_cond_timedwait(
					&pCondImpl->Condition,
					&pMutexImpl->Lock,
					&tDeadline
				);
			}
			__xrtMutexOwnerStore(pMutexImpl, __xrtCurrentThreadId());
			if ( iResult == 0 ) {
				return XWAIT_OK;
			}
			if ( iResult == ETIMEDOUT ) {
				return XWAIT_TIMEOUT;
			}
			if ( iResult == EPERM ) {
				__xrtErrorSetInvalidState();
			} else {
				__xrtSyncSetSystemError("cond.wait", iResult, "condition wait failed");
			}
			return XWAIT_ERROR;
		}
	#endif
}



/* 唤醒一个等待者。 */
XRT_API bool xrtCondSignal(xcond* pCond)
{
	xrt_cond_impl* pImpl = __xrtCondRequire(pCond);

	if ( pImpl == NULL ) {
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		WakeConditionVariable(&pImpl->Condition);
	#else
		{
			int iResult = pthread_cond_signal(&pImpl->Condition);

			if ( iResult != 0 ) {
				__xrtSyncSetSystemError("cond.signal", iResult, "condition signal failed");
				return false;
			}
		}
	#endif
	return true;
}



/* 唤醒全部当前等待者。 */
XRT_API bool xrtCondBroadcast(xcond* pCond)
{
	xrt_cond_impl* pImpl = __xrtCondRequire(pCond);

	if ( pImpl == NULL ) {
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		WakeAllConditionVariable(&pImpl->Condition);
	#else
		{
			int iResult = pthread_cond_broadcast(&pImpl->Condition);

			if ( iResult != 0 ) {
				__xrtSyncSetSystemError("cond.broadcast", iResult, "condition broadcast failed");
				return false;
			}
		}
	#endif
	return true;
}

#endif
