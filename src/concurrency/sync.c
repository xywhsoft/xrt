#include <xrt/detail/wait.h>
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
bool __xrtSyncDeadlineTime(double iDeadline, bool bMonotonic, struct timespec* pTime)
{
    int64 Remaining;
    int64 Seconds;
    int64 Nanoseconds;
    time_t NativeSeconds;
    if ( pTime == NULL ) { __xrtErrorSetInvalidArgument(); return false; }
    Remaining = __xrtWaitRemaining(iDeadline);
    if ( Remaining < 0 ) { return false; }
    if ( clock_gettime(bMonotonic ? CLOCK_MONOTONIC : CLOCK_REALTIME, pTime) != 0 ) {
        __xrtSyncSetSystemError("timer", errno, "condition clock is unavailable"); return false;
    }
    /* Limit each native call to one day; callers recheck the shared budget. */
    if ( Remaining > INT64_C(86400000) ) { Remaining = INT64_C(86400000); }
    Nanoseconds = (int64)pTime->tv_nsec + (Remaining % 1000) * INT64_C(1000000);
    Seconds = (int64)pTime->tv_sec + Remaining / 1000 + Nanoseconds / INT64_C(1000000000);
    NativeSeconds = (time_t)Seconds;
    if ( (int64)NativeSeconds != Seconds ) { __xrtErrorSetInvalidArgument(); return false; }
    pTime->tv_sec = NativeSeconds;
    pTime->tv_nsec = (long)(Nanoseconds % INT64_C(1000000000));
    return true;
}
#endif

#endif
