#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include <xcron.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xrt/event_waiter.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

static double processCpuMilliseconds(void)
{
#if defined(_WIN32)
    FILETIME Created, Exited, Kernel, User;
    if (!GetProcessTimes(GetCurrentProcess(), &Created, &Exited, &Kernel, &User)) return -1;
    uint64 K = ((uint64)Kernel.dwHighDateTime << 32) | Kernel.dwLowDateTime;
    uint64 U = ((uint64)User.dwHighDateTime << 32) | User.dwLowDateTime;
    return (double)(K + U) / 10000.0;
#else
    struct timespec Time;
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &Time)) return -1;
    return (double)Time.tv_sec * 1000.0 + (double)Time.tv_nsec / 1000000.0;
#endif
}

static void sample(void* Context)
{
    *(xtime*)Context = xrtNow();
}

static int byTime(const void* L, const void* R)
{
    xtime A = *(const xtime*)L, B = *(const xtime*)R;
    return (A > B) - (A < B);
}

static xtime nextSecond(xtime Now)
{
    return (Now / 1000 + 1) * 1000;
}

static bool waitDirect(xeventwaiter* Waiter, xtime Due, xtime* Fired, xtime* Next)
{
    double Started = xrtTimer();
    for (;;) {
        xtime Now = xrtNow();
        if (!Now) return false;
        if (Now >= Due) {
            *Next = nextSecond(Now);
            sample(Fired);
            return *Fired >= Due;
        }
        double Elapsed = (xrtTimer() - Started) * 1000.0;
        if (!(Elapsed >= 0.0 && Elapsed < 3000.0)) return false;
        int64 Remaining = 3000 - (int64)Elapsed;
        int64 Delay = Due - Now;
        if (xrtEventWaiterWaitFor(Waiter, Delay < Remaining ? Delay : Remaining) != XWAIT_TIMEOUT)
            return false;
    }
}

int main(int Argc, char** Argv)
{
    unsigned long Samples = Argc > 1 ? strtoul(Argv[1], NULL, 10) : 16;
    bool Direct = Argc == 3 && strcmp(Argv[2], "waiter") == 0;
    if (!Samples || Samples > 64 || (Argc > 2 && !Direct)) return 1;

    xcronscheduler* Scheduler = NULL;
    xevent Event = {0};
    xeventwaiter Waiter = {0};
    if (Direct) {
        if (!xrtEventInit(&Event, false, false)) return 2;
        if (!xrtEventWaiterInit(&Waiter, &Event, 0)) {
            xrtEventUnit(&Event);
            return 2;
        }
    } else if (!xcronSchedulerCreate(1, XCRON_SECONDS6, &Scheduler)) {
        return 2;
    }

    const unsigned Polls = 100000;
    double Start = xrtTimer();
    if (Direct) {
        for (unsigned I = 0; I < Polls; ++I)
            if (!xrtNow()) return 3;
    } else {
        for (unsigned I = 0; I < Polls; ++I)
            if (xcronSchedulerPoll(Scheduler, NULL) != XCRON_SCHEDULE_IDLE) return 3;
    }
    double PollSeconds = xrtTimer() - Start;

    xcronjobid Job;
    xtime Fired = 0, Lateness[64];
    xcronpoll Result;
    if (Direct) {
        xtime Now = xrtNow();
        if (!Now) return 4;
        Result.Next = nextSecond(Now);
    } else if (!xcronSchedulerAdd(Scheduler, XRT_STR_LITERAL("* * * * * *"), sample, &Fired, &Job) ||
        xcronSchedulerPoll(Scheduler, &Result) == XCRON_SCHEDULE_ERROR || !Result.HasNext) {
        return 4;
    }

    double CpuStart = 0;
    for (unsigned long I = 0; I <= Samples; ++I) {
        xtime Due = Result.Next;
        if (Direct) {
            if (!waitDirect(&Waiter, Due, &Fired, &Result.Next)) return 5;
        } else if (xcronSchedulerWait(Scheduler, 3000, &Result) != XCRON_SCHEDULE_DISPATCHED ||
            Result.Count != 1 || !Result.HasNext || Fired < Due) {
            return 5;
        }
        if (I == 0) {
            /* The first activation initializes the lazy scheduler waiter. */
            CpuStart = processCpuMilliseconds();
            Start = xrtTimer();
        } else {
            Lateness[I - 1] = Fired - Due;
        }
    }
    double WaitSeconds = xrtTimer() - Start;
    double CpuEnd = processCpuMilliseconds();
    bool Closed = Direct ? xrtEventWaiterUnit(&Waiter) && xrtEventUnit(&Event) :
        xcronSchedulerDestroy(Scheduler);
    if (PollSeconds <= 0 || WaitSeconds <= 0 || CpuStart < 0 || CpuEnd < CpuStart || !Closed) return 6;

    qsort(Lateness, Samples, sizeof(*Lateness), byTime);
    printf("idle_polls_per_sec: %.3f\n", Polls / PollSeconds);
    printf("wait_cpu_percent: %.6f\n", (CpuEnd - CpuStart) / (WaitSeconds * 10.0));
    printf("wake_lateness_p50_ms: %lld\n", (long long)Lateness[(Samples * 50 + 99) / 100 - 1]);
    printf("wake_lateness_p95_ms: %lld\n", (long long)Lateness[(Samples * 95 + 99) / 100 - 1]);
    printf("wake_lateness_p99_ms: %lld\n", (long long)Lateness[(Samples * 99 + 99) / 100 - 1]);
    printf("wake_samples: %lu\n", Samples);
    return 0;
}
