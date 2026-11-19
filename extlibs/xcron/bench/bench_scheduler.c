#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include <xcron.h>
#include <stdio.h>
#include <stdlib.h>
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

int main(int Argc, char** Argv)
{
    unsigned long Samples = Argc > 1 ? strtoul(Argv[1], NULL, 10) : 16;
    if (!Samples || Samples > 64) return 1;
    xcronscheduler* Scheduler = NULL;
    if (!xcronSchedulerCreate(1, XCRON_SECONDS6, &Scheduler)) return 2;
    const unsigned Polls = 100000;
    double Start = xrtTimer();
    for (unsigned I = 0; I < Polls; ++I)
        if (xcronSchedulerPoll(Scheduler, NULL) != XCRON_SCHEDULE_IDLE) return 3;
    double PollSeconds = xrtTimer() - Start;

    xcronjobid Job;
    xtime Fired = 0, Lateness[64];
    xcronpoll Result;
    if (!xcronSchedulerAdd(Scheduler, XRT_STR_LITERAL("* * * * * *"), sample, &Fired, &Job) ||
        xcronSchedulerPoll(Scheduler, &Result) == XCRON_SCHEDULE_ERROR || !Result.HasNext) return 4;
    double CpuStart = processCpuMilliseconds();
    Start = xrtTimer();
    for (unsigned long I = 0; I < Samples; ++I) {
        xtime Due = Result.Next;
        if (xcronSchedulerWait(Scheduler, 3000, &Result) != XCRON_SCHEDULE_DISPATCHED ||
            Result.Count != 1 || !Result.HasNext || Fired < Due) return 5;
        Lateness[I] = Fired - Due;
    }
    double WaitSeconds = xrtTimer() - Start;
    double CpuEnd = processCpuMilliseconds();
    if (PollSeconds <= 0 || WaitSeconds <= 0 || CpuStart < 0 || CpuEnd < CpuStart ||
        !xcronSchedulerDestroy(Scheduler)) return 6;
    qsort(Lateness, Samples, sizeof(*Lateness), byTime);
    printf("idle_polls_per_sec: %.3f\n", Polls / PollSeconds);
    printf("wait_cpu_percent: %.6f\n", (CpuEnd - CpuStart) / (WaitSeconds * 10.0));
    printf("wake_lateness_p99_ms: %lld\n", (long long)Lateness[(Samples * 99 + 99) / 100 - 1]);
    printf("wake_samples: %lu\n", Samples);
    return 0;
}
