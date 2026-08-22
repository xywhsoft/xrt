#include "../internal/xrt_sync.h"

#include <errno.h>



#if defined(XRT_FEATURE_EVENT)

/* 检查事件已经初始化。 */
static xrt_event_impl* __xrtEventRequire(xevent* pEvent)
{
	xrt_event_impl* pImpl;

	if ( pEvent == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pImpl = __xrtEventImpl(pEvent);
	if ( pImpl->Magic != XRT_EVENT_MAGIC ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pImpl;
}



/* 初始化自动或手动复位事件。 */
XRT_API bool xrtEventInit(xevent* pEvent, bool bManualReset, bool bSignaled)
{
	xrt_event_impl* pImpl;

	if ( pEvent == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pEvent, 0, sizeof(xevent));
	pImpl = __xrtEventImpl(pEvent);
	pImpl->ManualReset = bManualReset;
	#if defined(_WIN32) || defined(_WIN64)
		pImpl->Handle = CreateEventA(NULL, bManualReset, bSignaled, NULL);
		if ( pImpl->Handle == NULL ) {
			__xrtSyncSetSystemError("event.init", (int)GetLastError(), "event initialization failed");
			return false;
		}
	#else
		{
			pthread_condattr_t tAttr;
			int iResult = pthread_mutex_init(&pImpl->Lock, NULL);

			if ( iResult != 0 ) {
				__xrtSyncSetSystemError("event.init", iResult, "event lock initialization failed");
				return false;
			}
			iResult = pthread_condattr_init(&tAttr);
			if ( iResult != 0 ) {
				(void)pthread_mutex_destroy(&pImpl->Lock);
				__xrtSyncSetSystemError("event.init", iResult, "event condition attribute failed");
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
				__xrtSyncSetSystemError("event.init", iResult, "event condition initialization failed");
				return false;
			}
			pImpl->Signaled = bSignaled;
		}
	#endif
	pImpl->Magic = XRT_EVENT_MAGIC;
	return true;
}



/* 释放事件平台资源。 */
XRT_API bool xrtEventUnit(xevent* pEvent)
{
	xrt_event_impl* pImpl = __xrtEventRequire(pEvent);

	if ( pImpl == NULL ) {
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( !CloseHandle(pImpl->Handle) ) {
			__xrtSyncSetSystemError("event.unit", (int)GetLastError(), "event destruction failed");
			return false;
		}
	#else
		{
			int iResult = pthread_cond_destroy(&pImpl->Condition);

			if ( iResult == 0 ) {
				iResult = pthread_mutex_destroy(&pImpl->Lock);
			}
			if ( iResult != 0 ) {
				__xrtSyncSetSystemError("event.unit", iResult, "event destruction failed");
				return false;
			}
		}
	#endif
	memset(pEvent, 0, sizeof(xevent));
	return true;
}



/* 创建自动或手动复位事件。 */
XRT_API xevent* xrtEventCreate(bool bManualReset, bool bSignaled)
{
	xevent* pEvent = (xevent*)xrtMalloc(sizeof(xevent));

	if ( pEvent == NULL ) {
		return NULL;
	}
	if ( !xrtEventInit(pEvent, bManualReset, bSignaled) ) {
		xrtFree(pEvent);
		return NULL;
	}
	return pEvent;
}



/* 释放 Create 返回的事件。 */
XRT_API bool xrtEventDestroy(xevent* pEvent)
{
	if ( pEvent == NULL ) {
		return true;
	}
	if ( !xrtEventUnit(pEvent) ) {
		return false;
	}
	xrtFree(pEvent);
	return true;
}



/* 等待事件进入信号态。 */
XRT_API xwaitresult xrtEventWait(xevent* pEvent)
{
	return xrtEventWaitUntil(pEvent, XRT_DEADLINE_NEVER);
}



/* 非阻塞地检查并消费自动复位事件。 */
XRT_API xwaitresult xrtEventTryWait(xevent* pEvent)
{
	return xrtEventWaitFor(pEvent, 0);
}



/* 在相对微秒数内等待事件。 */
XRT_API xwaitresult xrtEventWaitFor(xevent* pEvent, uint64 iTimeout)
{
	return xrtEventWaitUntil(pEvent, xrtDeadlineAfter(iTimeout));
}



/* 等待事件到指定单调时钟截止时间。 */
XRT_API xwaitresult xrtEventWaitUntil(xevent* pEvent, xdeadline iDeadline)
{
	xrt_event_impl* pImpl = __xrtEventRequire(pEvent);

	if ( pImpl == NULL ) {
		return XWAIT_ERROR;
	}
	#if defined(_WIN32) || defined(_WIN64)
		for ( ;; ) {
			uint64 iRemaining = xrtDeadlineRemaining(iDeadline);
			DWORD iMilliseconds = iRemaining == 0 ? 0 :
				(iRemaining == UINT64_MAX ? INFINITE :
				 (DWORD)__xrtWaitMilliseconds(iRemaining));
			DWORD iResult = WaitForSingleObject(pImpl->Handle, iMilliseconds);

			if ( iResult == WAIT_OBJECT_0 ) {
				return XWAIT_OK;
			}
			if ( iResult == WAIT_TIMEOUT ) {
				if ( (iDeadline != XRT_DEADLINE_NEVER) && xrtDeadlineExpired(iDeadline) ) {
					return XWAIT_TIMEOUT;
				}
				continue;
			}
			__xrtSyncSetSystemError("event.wait", (int)GetLastError(), "event wait failed");
			return XWAIT_ERROR;
		}
	#else
		{
			int iResult = pthread_mutex_lock(&pImpl->Lock);

			if ( iResult != 0 ) {
				__xrtSyncSetSystemError("event.wait", iResult, "event lock failed");
				return XWAIT_ERROR;
			}
			while ( !pImpl->Signaled ) {
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
					__xrtSyncSetSystemError("event.wait", iResult, "event condition wait failed");
					return XWAIT_ERROR;
				}
			}
			if ( !pImpl->ManualReset ) {
				pImpl->Signaled = false;
			}
			(void)pthread_mutex_unlock(&pImpl->Lock);
			return XWAIT_OK;
		}
	#endif
}



/* 设置事件。 */
XRT_API bool xrtEventSet(xevent* pEvent)
{
	xrt_event_impl* pImpl = __xrtEventRequire(pEvent);

	if ( pImpl == NULL ) {
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( !SetEvent(pImpl->Handle) ) {
			__xrtSyncSetSystemError("event.set", (int)GetLastError(), "event set failed");
			return false;
		}
	#else
		{
			int iResult = pthread_mutex_lock(&pImpl->Lock);

			if ( iResult != 0 ) {
				__xrtSyncSetSystemError("event.set", iResult, "event lock failed");
				return false;
			}
			pImpl->Signaled = true;
			iResult = pImpl->ManualReset ? pthread_cond_broadcast(&pImpl->Condition) :
				pthread_cond_signal(&pImpl->Condition);
			(void)pthread_mutex_unlock(&pImpl->Lock);
			if ( iResult != 0 ) {
				__xrtSyncSetSystemError("event.set", iResult, "event wake failed");
				return false;
			}
		}
	#endif
	return true;
}



/* 清除事件信号态。 */
XRT_API bool xrtEventReset(xevent* pEvent)
{
	xrt_event_impl* pImpl = __xrtEventRequire(pEvent);

	if ( pImpl == NULL ) {
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( !ResetEvent(pImpl->Handle) ) {
			__xrtSyncSetSystemError("event.reset", (int)GetLastError(), "event reset failed");
			return false;
		}
	#else
		{
			int iResult = pthread_mutex_lock(&pImpl->Lock);

			if ( iResult != 0 ) {
				__xrtSyncSetSystemError("event.reset", iResult, "event lock failed");
				return false;
			}
			pImpl->Signaled = false;
			(void)pthread_mutex_unlock(&pImpl->Lock);
		}
	#endif
	return true;
}

#endif
