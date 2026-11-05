#ifndef XLLM_TIME_H
#define XLLM_TIME_H

/* Request and persistence APIs retain microsecond fields. XRT waits use
 * signed relative milliseconds; convert only at that boundary. Include after
 * the XRT declarations. Monotonic values are never serialized across boots. */
#include <stdint.h>
#define XLLM_DEADLINE_NEVER UINT64_MAX
/* New civil time starts at year 1; product persistence uses Unix microseconds. */
static inline int64_t xllmTimeToUnixUs(xtime time)
{
    return (time - XRT_TIME_UNIX_EPOCH) * INT64_C(1000);
}
static inline int64_t xllmUnixUs(void) { return xllmTimeToUnixUs(xrtNow()); }
static inline uint64_t xllmClockUs(void)
{
    double t = xrtTimer() * 1000000.0;
    if (!(t >= 0.0))
        return 0u;
    return t >= (double)(UINT64_MAX - 1u) ? UINT64_MAX - 1u : (uint64_t)t;
}
static inline uint64_t xllmDeadlineAfterUs(uint64_t duration)
{
    uint64_t now = xllmClockUs();
    return duration >= UINT64_MAX - 1u - now ? UINT64_MAX - 1u : now + duration;
}
static inline uint64_t xllmDeadlineRemainingUs(uint64_t deadline)
{
    uint64_t now;
    if (deadline == XLLM_DEADLINE_NEVER)
        return UINT64_MAX;
    now = xllmClockUs();
    return deadline > now ? deadline - now : 0u;
}
static inline bool xllmDeadlineExpired(uint64_t deadline)
{
    return deadline != XLLM_DEADLINE_NEVER && xllmDeadlineRemainingUs(deadline) == 0u;
}
static inline int64_t xllmDeadlineWaitMs(uint64_t deadline)
{
    uint64_t left, ms;
    if (deadline == XLLM_DEADLINE_NEVER)
        return XRT_WAIT_FOREVER;
    left = xllmDeadlineRemainingUs(deadline);
    ms = left / 1000u + (left % 1000u != 0u);
    return ms > INT64_MAX ? INT64_MAX : (int64_t)ms;
}
#endif
