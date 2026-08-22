#ifndef XRT_INTERNAL_WAIT_H
#define XRT_INTERNAL_WAIT_H

#include "xrt_internal.h"



#if defined(XRT_FEATURE_WAIT)

/* 把剩余微秒向上取整为平台等待使用的毫秒。 */
static inline uint32 __xrtWaitMilliseconds(uint64 iRemaining)
{
	uint64 iMilliseconds;

	if ( iRemaining == UINT64_MAX ) {
		return UINT32_MAX;
	}
	iMilliseconds = (iRemaining / UINT64_C(1000)) +
		((iRemaining % UINT64_C(1000)) != 0 ? 1u : 0u);
	if ( iMilliseconds >= UINT32_MAX ) {
		return UINT32_MAX - 1u;
	}
	return (uint32)iMilliseconds;
}

#endif

#endif
