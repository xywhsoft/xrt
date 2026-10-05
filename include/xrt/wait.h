#ifndef XRT_WAIT_H
#define XRT_WAIT_H
#include <xrt/core.h>
#if defined(XRT_FEATURE_WAIT) && !defined(XRT_FEATURE_TIME)
#error "XRT_FEATURE_WAIT requires XRT_FEATURE_TIME"
#endif
#if defined(XRT_FEATURE_WAIT)
/* Relative wait parameters use signed milliseconds; -1 means no timeout. */
#define XRT_WAIT_FOREVER INT64_C(-1)
typedef enum xwaitresult {
    XWAIT_ERROR = -1, XWAIT_OK = 0, XWAIT_TIMEOUT = 1,
    XWAIT_CANCELLED = 2, XWAIT_CLOSED = 3
} xwaitresult;
#endif
#endif
