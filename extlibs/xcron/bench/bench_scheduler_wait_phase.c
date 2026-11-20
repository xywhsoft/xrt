#if defined(_WIN32)
#include <winsock2.h>
#endif
#define XCRON_MODULE_XCRON_SCHEDULER
#define XRT_MODULE_THREAD
#include <xcron/features.h>
#define XRT_IMPLEMENTATION
#include "single/xrt.h"

static xwaitresult phaseWaitFor(xeventwaiter* Waiter, int64 Timeout);
static xwaitresult phaseWaitUntil(xeventwaiter* Waiter, double Deadline);

/* Route only the scheduler's calls through the observer. */
#define __xrtEventWaiterWaitUntil phaseWaitUntil
#define XCRON_IMPLEMENTATION
#include "single/extlibs/xcron.h"
#undef __xrtEventWaiterWaitUntil

typedef struct waitphase {
    unsigned Calls;
    int64 RequestNs;
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
    int64 RequestNs;
    xtime EntryWall, ReturnWall;
    unsigned EntrySubNs, ReturnSubNs;
    uint64 EntryNs, ReturnNs;
} LastWait;

static xeventwaiterinfo WaitInfo;
static bool HaveWaitInfo;

static void phaseEnter(void)
{
    ++LastWait.Calls;
    LastWait.ClockOk = cohortWallTime(&LastWait.EntryWall, &LastWait.EntrySubNs);
    LastWait.EntryNs = xbenchNowNs();
}

static xwaitresult phaseLeave(xeventwaiter* Waiter, xwaitresult Result)
{
    LastWait.ReturnNs = xbenchNowNs();
    bool ReturnOk = cohortWallTime(&LastWait.ReturnWall, &LastWait.ReturnSubNs);
    LastWait.ClockOk = LastWait.ClockOk && ReturnOk && LastWait.ReturnNs >= LastWait.EntryNs;
    if (!HaveWaitInfo) HaveWaitInfo = xrtEventWaiterGet(Waiter, &WaitInfo);
    return Result;
}

static xwaitresult phaseWaitFor(xeventwaiter* Waiter, int64 Timeout)
{
    phaseEnter();
    LastWait.RequestNs = Timeout * INT64_C(1000000);
    return phaseLeave(Waiter, xrtEventWaiterWaitFor(Waiter, Timeout));
}

static xwaitresult phaseWaitUntil(xeventwaiter* Waiter, double Deadline)
{
    phaseEnter();
    double Remaining = Deadline - (double)LastWait.EntryNs * 1e-9;
    LastWait.RequestNs = (int64)(fmax(0, Remaining) * 1e9);
    return phaseLeave(Waiter, __xrtEventWaiterWaitUntil(Waiter, Deadline));
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
    Phase->RequestNs = LastWait.RequestNs;
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
            (long long)Phase->RequestNs, (long long)Phase->EntryLatenessNs,
            (long long)Phase->ReturnLatenessNs, (unsigned long long)Phase->ReturnToFirstNs,
            (unsigned long long)Phase->WaitNs);
    }
}
