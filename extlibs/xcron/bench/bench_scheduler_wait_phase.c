#if defined(_WIN32)
#include <winsock2.h>
#endif
#define XCRON_MODULE_XCRON_SCHEDULER
#define XRT_MODULE_THREAD
#include <xcron/features.h>
#define XRT_IMPLEMENTATION
#include "single/xrt.h"

static xwaitresult phaseWaitFor(xeventwaiter* Waiter, int64 Timeout);

/* Route only the scheduler's calls through the observer. */
#define xrtEventWaiterWaitFor phaseWaitFor
#define XCRON_IMPLEMENTATION
#include "single/extlibs/xcron.h"
#undef xrtEventWaiterWaitFor

typedef struct waitphase {
    unsigned Calls;
    int64 TimeoutMs;
    xtime EntryLatenessNs, ReturnLatenessNs;
    uint64 ReturnToFirstNs, WaitNs;
} waitphase;

static void phaseReset(void);
static bool phaseRecord(waitphase* Phase, xtime Due, uint64 FirstNs);
static void phasePrint(const waitphase* Phases, unsigned Waves);

#ifndef _WIN32
#undef _POSIX_C_SOURCE
#endif
#define XCRON_BENCH_WAIT_PHASE
#include "bench_scheduler.c"

static struct {
    unsigned Calls;
    bool ClockOk;
    int64 TimeoutMs;
    xtime EntryWall, ReturnWall;
    unsigned EntrySubNs, ReturnSubNs;
    uint64 EntryNs, ReturnNs;
} LastWait;

static xeventwaiterinfo WaitInfo;
static bool HaveWaitInfo;

static xwaitresult phaseWaitFor(xeventwaiter* Waiter, int64 Timeout)
{
    ++LastWait.Calls;
    LastWait.TimeoutMs = Timeout;
    bool EntryOk = cohortWallTime(&LastWait.EntryWall, &LastWait.EntrySubNs);
    LastWait.EntryNs = xbenchNowNs();
    xwaitresult Result = xrtEventWaiterWaitFor(Waiter, Timeout);
    LastWait.ReturnNs = xbenchNowNs();
    bool ReturnOk = cohortWallTime(&LastWait.ReturnWall, &LastWait.ReturnSubNs);
    LastWait.ClockOk = EntryOk && ReturnOk && LastWait.ReturnNs >= LastWait.EntryNs;
    if (!HaveWaitInfo) HaveWaitInfo = xrtEventWaiterGet(Waiter, &WaitInfo);
    return Result;
}

static void phaseReset(void)
{
    LastWait.Calls = 0;
}

static bool phaseRecord(waitphase* Phase, xtime Due, uint64 FirstNs)
{
    *Phase = (waitphase){0};
    if (!LastWait.Calls) return true;
    if (!LastWait.ClockOk || FirstNs < LastWait.ReturnNs || !HaveWaitInfo) return false;
    Phase->Calls = LastWait.Calls;
    Phase->TimeoutMs = LastWait.TimeoutMs;
    Phase->EntryLatenessNs = cohortLateness(Due, LastWait.EntryWall, LastWait.EntrySubNs);
    Phase->ReturnLatenessNs = cohortLateness(Due, LastWait.ReturnWall, LastWait.ReturnSubNs);
    Phase->ReturnToFirstNs = FirstNs - LastWait.ReturnNs;
    Phase->WaitNs = LastWait.ReturnNs - LastWait.EntryNs;
    return true;
}

static void phasePrint(const waitphase* Phases, unsigned Waves)
{
    printf("waiter_backend: %d\n", HaveWaitInfo ? (int)WaitInfo.Backend : -1);
    for (unsigned Index = 0; Index < Waves; ++Index) {
        const waitphase* Phase = &Phases[Index];
        printf("phase_%u_ns: %u,%lld,%lld,%lld,%llu,%llu\n", Index + 1, Phase->Calls,
            (long long)Phase->TimeoutMs, (long long)Phase->EntryLatenessNs,
            (long long)Phase->ReturnLatenessNs, (unsigned long long)Phase->ReturnToFirstNs,
            (unsigned long long)Phase->WaitNs);
    }
}
