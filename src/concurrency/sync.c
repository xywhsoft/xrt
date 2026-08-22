#include "../internal/xrt_sync.h"

#include <errno.h>



#if defined(XRT_FEATURE_SYNC)

/* 设置同步原语的平台错误。 */
void __xrtSyncSetSystemError(cstr sOperation, int iCode, cstr sMessage)
{
	xerrordesc tDesc;
	xerror* pError;

	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.Kind = __xrtSystemErrorKind(iCode);
	tDesc.Code = 1;
	tDesc.SystemCode = iCode;
	tDesc.Domain = "xrt.sync";
	tDesc.Operation = sOperation;
	tDesc.Message = sMessage;
	pError = xrtErrorBuild(&tDesc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



#if !defined(_WIN32) && !defined(_WIN64) && defined(XRT_FEATURE_WAIT)
/* 把单调截止时间转换为条件变量配置的绝对时钟。 */
bool __xrtSyncDeadlineTime(
	xdeadline iDeadline,
	bool bMonotonic,
	struct timespec* pTime
)
{
	uint64 iRemaining;
	uint64 iNanoseconds;

	if ( pTime == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( bMonotonic ) {
		pTime->tv_sec = (time_t)(iDeadline / UINT64_C(1000000));
		pTime->tv_nsec = (long)((iDeadline % UINT64_C(1000000)) * UINT64_C(1000));
		return true;
	}
	iRemaining = xrtDeadlineRemaining(iDeadline);
	if ( clock_gettime(CLOCK_REALTIME, pTime) != 0 ) {
		__xrtSyncSetSystemError("clock", errno, "realtime clock is unavailable");
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

#endif
