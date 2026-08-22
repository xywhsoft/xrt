#include "../internal/xrt_sync.h"

#include <errno.h>



#if defined(XRT_FEATURE_SEM)

/* 检查信号量已经初始化。 */
static xrt_sem_impl* __xrtSemRequire(xsem* pSem)
{
	xrt_sem_impl* pImpl;

	if ( pSem == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pImpl = __xrtSemImpl(pSem);
	if ( pImpl->Magic != XRT_SEM_MAGIC ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pImpl;
}



/* 初始化计数信号量。 */
XRT_API bool xrtSemInit(xsem* pSem, uint32 iInitial, uint32 iMaximum)
{
	xrt_sem_impl* pImpl;

	if ( (pSem == NULL) || (iMaximum == 0) || (iMaximum > INT32_MAX) ||
		 (iInitial > iMaximum) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pSem, 0, sizeof(xsem));
	pImpl = __xrtSemImpl(pSem);
	pImpl->Maximum = iMaximum;
	#if defined(_WIN32) || defined(_WIN64)
		pImpl->Handle = CreateSemaphoreA(
			NULL,
			(LONG)iInitial,
			(LONG)iMaximum,
			NULL
		);
		if ( pImpl->Handle == NULL ) {
			__xrtSyncSetSystemError(
				"sem.init",
				(int)GetLastError(),
				"semaphore initialization failed"
			);
			return false;
		}
	#else
		{
			pthread_condattr_t tAttr;
			int iResult = pthread_mutex_init(&pImpl->Lock, NULL);

			if ( iResult != 0 ) {
				__xrtSyncSetSystemError("sem.init", iResult, "semaphore lock initialization failed");
				return false;
			}
			iResult = pthread_condattr_init(&tAttr);
			if ( iResult != 0 ) {
				(void)pthread_mutex_destroy(&pImpl->Lock);
				__xrtSyncSetSystemError("sem.init", iResult, "semaphore condition attribute failed");
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
				(void)pthread_mutex_destroy(&pImpl->Lock);
				__xrtSyncSetSystemError("sem.init", iResult, "semaphore condition initialization failed");
				return false;
			}
			pImpl->Value = iInitial;
		}
	#endif
	pImpl->Magic = XRT_SEM_MAGIC;
	return true;
}



/* 释放信号量平台资源。 */
XRT_API bool xrtSemUnit(xsem* pSem)
{
	xrt_sem_impl* pImpl = __xrtSemRequire(pSem);

	if ( pImpl == NULL ) {
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( !CloseHandle(pImpl->Handle) ) {
			__xrtSyncSetSystemError(
				"sem.unit",
				(int)GetLastError(),
				"semaphore destruction failed"
			);
			return false;
		}
	#else
		{
			int iCondResult = pthread_cond_destroy(&pImpl->Condition);
			int iMutexResult;

			if ( iCondResult != 0 ) {
				__xrtSyncSetSystemError("sem.unit", iCondResult, "semaphore condition destruction failed");
				return false;
			}
			iMutexResult = pthread_mutex_destroy(&pImpl->Lock);
			if ( iMutexResult != 0 ) {
				__xrtSyncSetSystemError("sem.unit", iMutexResult, "semaphore lock destruction failed");
				return false;
			}
		}
	#endif
	memset(pSem, 0, sizeof(xsem));
	return true;
}



/* 创建计数信号量。 */
XRT_API xsem* xrtSemCreate(uint32 iInitial, uint32 iMaximum)
{
	xsem* pSem = (xsem*)xrtMalloc(sizeof(xsem));

	if ( pSem == NULL ) {
		return NULL;
	}
	if ( !xrtSemInit(pSem, iInitial, iMaximum) ) {
		xrtFree(pSem);
		return NULL;
	}
	return pSem;
}



/* 释放 Create 返回的信号量。 */
XRT_API bool xrtSemDestroy(xsem* pSem)
{
	if ( pSem == NULL ) {
		return true;
	}
	if ( !xrtSemUnit(pSem) ) {
		return false;
	}
	xrtFree(pSem);
	return true;
}



/* 等待并消费一个信号。 */
XRT_API xwaitresult xrtSemWait(xsem* pSem)
{
	return xrtSemWaitUntil(pSem, XRT_DEADLINE_NEVER);
}



/* 非阻塞地尝试消费一个信号。 */
XRT_API xwaitresult xrtSemTryWait(xsem* pSem)
{
	return xrtSemWaitFor(pSem, 0);
}



/* 在相对微秒数内等待并消费一个信号。 */
XRT_API xwaitresult xrtSemWaitFor(xsem* pSem, uint64 iTimeout)
{
	return xrtSemWaitUntil(pSem, xrtDeadlineAfter(iTimeout));
}



/* 等待并消费一个信号到指定截止时间。 */
XRT_API xwaitresult xrtSemWaitUntil(xsem* pSem, xdeadline iDeadline)
{
	xrt_sem_impl* pImpl = __xrtSemRequire(pSem);

	if ( pImpl == NULL ) {
		return XWAIT_ERROR;
	}
	#if defined(_WIN32) || defined(_WIN64)
		for ( ;; ) {
			uint64 iRemaining = xrtDeadlineRemaining(iDeadline);
			DWORD iMilliseconds;
			DWORD iResult;

			if ( iRemaining == 0 ) {
				iMilliseconds = 0;
			} else {
				iMilliseconds = iRemaining == UINT64_MAX ? INFINITE :
					(DWORD)__xrtWaitMilliseconds(iRemaining);
			}
			iResult = WaitForSingleObject(pImpl->Handle, iMilliseconds);
			if ( iResult == WAIT_OBJECT_0 ) {
				return XWAIT_OK;
			}
			if ( iResult == WAIT_TIMEOUT ) {
				if ( (iDeadline == XRT_DEADLINE_NEVER) || !xrtDeadlineExpired(iDeadline) ) {
					continue;
				}
				return XWAIT_TIMEOUT;
			}
			__xrtSyncSetSystemError(
				"sem.wait",
				(int)GetLastError(),
				"semaphore wait failed"
			);
			return XWAIT_ERROR;
		}
	#else
		{
			int iResult = pthread_mutex_lock(&pImpl->Lock);

			if ( iResult != 0 ) {
				__xrtSyncSetSystemError("sem.wait", iResult, "semaphore lock failed");
				return XWAIT_ERROR;
			}
			while ( pImpl->Value == 0 ) {
				if ( iDeadline == XRT_DEADLINE_NEVER ) {
					iResult = pthread_cond_wait(&pImpl->Condition, &pImpl->Lock);
				} else {
					struct timespec tDeadline;

					if ( xrtDeadlineExpired(iDeadline) ) {
						(void)pthread_mutex_unlock(&pImpl->Lock);
						return XWAIT_TIMEOUT;
					}
					if ( !__xrtSyncDeadlineTime(iDeadline, pImpl->Monotonic, &tDeadline) ) {
						(void)pthread_mutex_unlock(&pImpl->Lock);
						return XWAIT_ERROR;
					}
					iResult = pthread_cond_timedwait(
						&pImpl->Condition,
						&pImpl->Lock,
						&tDeadline
					);
				}
				if ( iResult == ETIMEDOUT ) {
					continue;
				}
				if ( iResult != 0 ) {
					(void)pthread_mutex_unlock(&pImpl->Lock);
					__xrtSyncSetSystemError("sem.wait", iResult, "semaphore condition wait failed");
					return XWAIT_ERROR;
				}
			}
			pImpl->Value--;
			(void)pthread_mutex_unlock(&pImpl->Lock);
			return XWAIT_OK;
		}
	#endif
}



/* 发布一个信号。 */
XRT_API bool xrtSemPost(xsem* pSem)
{
	return xrtSemPostMany(pSem, 1);
}



/* 原子发布多个信号。 */
XRT_API bool xrtSemPostMany(xsem* pSem, uint32 iCount)
{
	xrt_sem_impl* pImpl = __xrtSemRequire(pSem);

	if ( pImpl == NULL ) {
		return false;
	}
	if ( iCount == 0 ) {
		return true;
	}
	if ( iCount > pImpl->Maximum ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( !ReleaseSemaphore(pImpl->Handle, (LONG)iCount, NULL) ) {
			int iCode = (int)GetLastError();

			if ( iCode == ERROR_TOO_MANY_POSTS ) {
				__xrtErrorSetInvalidState();
			} else {
				__xrtSyncSetSystemError("sem.post", iCode, "semaphore post failed");
			}
			return false;
		}
	#else
		{
			int iResult = pthread_mutex_lock(&pImpl->Lock);

			if ( iResult != 0 ) {
				__xrtSyncSetSystemError("sem.post", iResult, "semaphore lock failed");
				return false;
			}
			if ( iCount > (pImpl->Maximum - pImpl->Value) ) {
				(void)pthread_mutex_unlock(&pImpl->Lock);
				__xrtErrorSetInvalidState();
				return false;
			}
			pImpl->Value += iCount;
			iResult = iCount == 1 ? pthread_cond_signal(&pImpl->Condition) :
				pthread_cond_broadcast(&pImpl->Condition);
			(void)pthread_mutex_unlock(&pImpl->Lock);
			if ( iResult != 0 ) {
				__xrtSyncSetSystemError("sem.post", iResult, "semaphore wake failed");
				return false;
			}
		}
	#endif
	return true;
}

#endif
