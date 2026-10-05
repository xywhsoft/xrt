#ifndef XRT_INTERNAL_WAIT_H
#define XRT_INTERNAL_WAIT_H
#include "xrt_internal.h"
#include <xrt/detail/wait.h>
#if defined(XRT_FEATURE_WAIT)
static inline uint32 __xrtWaitMilliseconds(int64 Milliseconds)
{
    if ( Milliseconds == XRT_WAIT_FOREVER ) { return UINT32_MAX; }
    if ( Milliseconds < 0 ) { return 0; }
    return Milliseconds >= UINT32_MAX ? UINT32_MAX - 1u : (uint32)Milliseconds;
}
#endif
#endif
