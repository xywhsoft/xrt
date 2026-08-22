#include "../internal/xrt_wait.h"



#if defined(XRT_FEATURE_WAIT)

/* 从当前单调时钟和相对微秒数构造截止时间。 */
XRT_API xdeadline xrtDeadlineAfter(uint64 iTimeout)
{
	uint64 iNow;

	if ( iTimeout == UINT64_MAX ) {
		return XRT_DEADLINE_NEVER;
	}
	iNow = xrtClock();
	if ( iTimeout > (UINT64_MAX - iNow) ) {
		return XRT_DEADLINE_NEVER;
	}
	return iNow + iTimeout;
}



/* 判断截止时间是否已经到达。 */
XRT_API bool xrtDeadlineExpired(xdeadline iDeadline)
{
	return (iDeadline != XRT_DEADLINE_NEVER) && (xrtClock() >= iDeadline);
}



/* 返回截止时间前剩余的微秒数。 */
XRT_API uint64 xrtDeadlineRemaining(xdeadline iDeadline)
{
	uint64 iNow;

	if ( iDeadline == XRT_DEADLINE_NEVER ) {
		return UINT64_MAX;
	}
	iNow = xrtClock();
	return iNow < iDeadline ? iDeadline - iNow : 0;
}

#endif
