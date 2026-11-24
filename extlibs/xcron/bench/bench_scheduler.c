#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include "../../../dev/bench/bench_common.h"
#include <xcron.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xrt/detail/wait.h>
#include <xrt/event_waiter.h>
#include <xrt/thread.h>
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

#if defined(XCRON_BENCH_WAIT_PHASE)
#define COHORT_WAIT_FOR phaseWaitFor
#define COHORT_WAIT_UNTIL phaseWaitUntil
#else
#define COHORT_WAIT_FOR xrtEventWaiterWaitFor
#define COHORT_WAIT_UNTIL __xrtEventWaiterWaitUntil
#endif

static bool waitUntil(xeventwaiter* Waiter, xtime Due, xtime* Observed)
{
    double Started = xrtTimer();
    for (;;) {
        xtime Now = xrtNow();
        if (!Now) return false;
        if (Now >= Due) {
            *Observed = Now;
            return true;
        }
        double Elapsed = (xrtTimer() - Started) * 1000.0;
        if (!(Elapsed >= 0.0 && Elapsed < 3000.0)) return false;
        int64 Remaining = 3000 - (int64)Elapsed;
        int64 Delay = Due - Now;
        if (COHORT_WAIT_FOR(Waiter, Delay < Remaining ? Delay : Remaining) != XWAIT_TIMEOUT)
            return false;
    }
}

/* Match the scheduler's clock and absolute arming in the direct cohort control. */
static bool waitPrecise(xeventwaiter* Waiter, xtime Due, xtime* Observed)
{
    double Sample = xrtTimer();
    double Limit = Sample + 3.0;
    if (!isfinite(Limit)) return false;
    for (;;) {
        uint32 SubNs;
        xtime Now = __xrtNowWithSubMs(&SubNs);
        if (!Now) return false;
        if (Now >= Due) {
            *Observed = Now;
            return true;
        }
        double Current = xrtTimer();
        if (!isfinite(Sample) || !isfinite(Current) || Current >= Limit) return false;
        uint64 DelayMs = (uint64)Due - (uint64)Now;
        double Delay = (double)(DelayMs / 1000) + (double)(DelayMs % 1000) * 0.001;
        double Deadline = nextafter(Sample + (Delay - (double)SubNs * 1e-9), INFINITY);
        if (COHORT_WAIT_UNTIL(Waiter, Deadline < Limit ? Deadline : Limit) != XWAIT_TIMEOUT)
            return false;
        Sample = xrtTimer();
    }
}

static bool waitDirect(xeventwaiter* Waiter, xtime Due, xtime* Fired, xtime* Next)
{
    xtime Observed;
    if (!waitUntil(Waiter, Due, &Observed)) return false;
    *Next = nextSecond(Observed);
    sample(Fired);
    return *Fired >= Due;
}

typedef struct cronwave {
    unsigned Called, Count;
    bool Unexpected;
    xtime FirstWall, LastWall;
    unsigned FirstSubNs, LastSubNs;
    uint64 FirstNs, LastNs;
} cronwave;

typedef struct cronjob {
    cronwave* Wave;
    unsigned Index;
} cronjob;

#if defined(_WIN32)
static void (WINAPI* preciseFileTime)(LPFILETIME);
#endif

static bool cohortWallTime(xtime* Milliseconds, unsigned* SubNs)
{
#if defined(_WIN32)
    FILETIME Now;
    if (preciseFileTime) preciseFileTime(&Now);
    else GetSystemTimeAsFileTime(&Now);
    uint64 Ticks = ((uint64)Now.dwHighDateTime << 32) | Now.dwLowDateTime;
    const int64 UnixOffsetMs = INT64_C(11644473600000);
    *SubNs = (unsigned)(Ticks % 10000) * 100u;
    return xrtTimeFromUnixMs((int64)(Ticks / 10000) - UnixOffsetMs, Milliseconds);
#else
    struct timespec Now;
    if (clock_gettime(CLOCK_REALTIME, &Now) || !xrtTimeFromUnix((int64)Now.tv_sec, Milliseconds)) return false;
    int64 WholeMs = Now.tv_nsec / 1000000;
    if (*Milliseconds > INT64_MAX - WholeMs) return false;
    *Milliseconds += WholeMs;
    *SubNs = (unsigned)(Now.tv_nsec % 1000000);
    return true;
#endif
}

static xtime cohortLateness(xtime Due, xtime Wall, unsigned SubNs)
{
    return (Wall - Due) * 1000000 + SubNs;
}

static void cohortCallback(void* Context)
{
    cronjob* Job = Context;
    cronwave* Wave = Job->Wave;
    if (Job->Index != Wave->Called++) Wave->Unexpected = true;
    bool First = Job->Index == 0, Last = Job->Index + 1 == Wave->Count;
    if (!First && !Last) return;
    xtime Wall;
    unsigned SubNs;
    if (!cohortWallTime(&Wall, &SubNs)) {
        Wave->Unexpected = true;
        return;
    }
    uint64 NowNs = xbenchNowNs();
    if (First) {
        Wave->FirstWall = Wall;
        Wave->FirstSubNs = SubNs;
        Wave->FirstNs = NowNs;
    }
    if (Last) {
        Wave->LastWall = Wall;
        Wave->LastSubNs = SubNs;
        Wave->LastNs = NowNs;
    }
}

static void dormantCallback(void* Context)
{
    ((cronwave*)Context)->Unexpected = true;
}

static bool cohortValue(const char* Text, unsigned Limit, unsigned* Value)
{
    char* End;
    errno = 0;
    unsigned long Parsed = strtoul(Text, &End, 10);
    if (*Text < '0' || *Text > '9' || *End || errno || Parsed > Limit) return false;
    *Value = (unsigned)Parsed;
    return true;
}

static bool cohortAffinity(void)
{
    const char* Requested = getenv("XBENCH_PIN_CPU");
    if (!Requested || !*Requested) {
        puts("affinity: inherited");
        return true;
    }
    unsigned Cpu;
    if (!cohortValue(Requested, 1023, &Cpu)) return false;
    xbenchApplyCpuPinFromEnv();
#if defined(_WIN32)
    DWORD_PTR Mask, System;
    if (Cpu >= sizeof(Mask) * CHAR_BIT || !GetProcessAffinityMask(GetCurrentProcess(), &Mask, &System) ||
        Mask != ((DWORD_PTR)1u << Cpu)) return false;
#elif defined(__linux__)
    cpu_set_t Set;
    if (Cpu >= CPU_SETSIZE || sched_getaffinity(0, sizeof(Set), &Set) ||
        !CPU_ISSET((int)Cpu, &Set) || CPU_COUNT(&Set) != 1) return false;
#else
    return false;
#endif
    printf("affinity: cpu-%u\n", Cpu);
    return true;
}

static xstrview dormantExpression(xtime Now, unsigned Waves)
{
    const xstrview Dates[] = {XRT_STR_LITERAL("0 0 0 1 1 *"), XRT_STR_LITERAL("0 0 0 1 7 *")};
    for (size_t Index = 0; Index < sizeof(Dates) / sizeof(*Dates); ++Index) {
        xcronexpression Expression;
        xtime Next;
        if (xcronExpressionCompile(Dates[Index], XCRON_SECONDS6, &Expression, NULL) &&
            xcronCalendarNext(&Expression, Now, INT64_MAX, XCRON_SEARCH_EXCLUSIVE, &Next, NULL) ==
                XCRON_CALENDAR_FOUND && Next - Now > 1000 * ((xtime)Waves + 5)) return Dates[Index];
    }
    return (xstrview){0};
}

static bool cohortRegister(xcronscheduler* Scheduler, cronwave* Wave, cronjob* Jobs,
    unsigned Dormant, unsigned Waves, xtime* Due)
{
    if (Scheduler && Dormant) {
        xstrview Text = dormantExpression(xrtNow(), Waves);
        if (!Text.Size) return false;
        for (unsigned Index = 0; Index < Dormant; ++Index) {
            xcronjobid Job;
            if (!xcronSchedulerAdd(Scheduler, Text, dormantCallback, Wave, &Job)) return false;
        }
    }
    xtime Now = xrtNow();
    if (Now <= 0) return false;
    if (Now % 1000 > 200) xrtSleep(nextSecond(Now) - Now);
    *Due = nextSecond(xrtNow());
    for (unsigned Index = 0; Index < Wave->Count; ++Index) {
        Jobs[Index] = (cronjob){Wave, Index};
        xcronjobid Job;
        if (Scheduler && !xcronSchedulerAdd(Scheduler, XRT_STR_LITERAL("* * * * * *"),
            cohortCallback, &Jobs[Index], &Job)) return false;
    }
    if (!Scheduler) return true;
    xcronpoll Ready;
    return xcronSchedulerPoll(Scheduler, &Ready) == XCRON_SCHEDULE_IDLE && Ready.HasNext && Ready.Next == *Due;
}

static bool cohortWave(xcronscheduler* Scheduler, xeventwaiter* Waiter, bool Precise,
    cronwave* Wave, cronjob* Jobs, xtime Due)
{
#if defined(XCRON_BENCH_WAIT_PHASE)
    phaseReset();
#endif
    *Wave = (cronwave){.Count = Wave->Count};
    if (Scheduler) {
        xcronpoll Result;
        if (xcronSchedulerWait(Scheduler, 3000, &Result) != XCRON_SCHEDULE_DISPATCHED ||
            Result.Count != Wave->Count || !Result.HasNext || Result.Next != Due + 1000) return false;
    } else {
        xtime Observed;
        bool Ready = Precise ? waitPrecise(Waiter, Due, &Observed) : waitUntil(Waiter, Due, &Observed);
        if (!Ready || Observed >= Due + 1000) return false;
        for (unsigned Index = 0; Index < Wave->Count; ++Index) cohortCallback(&Jobs[Index]);
    }
    return !Wave->Unexpected && Wave->Called == Wave->Count && Wave->FirstWall >= Due &&
        Wave->LastWall >= Wave->FirstWall && Wave->LastWall < Due + 1000 && Wave->LastNs >= Wave->FirstNs &&
        cohortLateness(Due, Wave->LastWall, Wave->LastSubNs) >= cohortLateness(Due, Wave->FirstWall, Wave->FirstSubNs);
}

static void cohortQuantiles(const char* Name, const char* Unit, xtime* Values, unsigned Count, double Scale)
{
    static const unsigned Ranks[] = {500, 950, 990, 999};
    static const char* const Labels[] = {"50", "95", "99", "999"};
    qsort(Values, Count, sizeof(*Values), byTime);
    for (size_t Index = 0; Index < sizeof(Ranks) / sizeof(*Ranks); ++Index)
        printf("%s_p%s_%s: %.3f\n", Name, Labels[Index], Unit,
            (double)Values[(Count * Ranks[Index] + 999u) / 1000u - 1u] * Scale);
}

static int cohort(int Argc, char** Argv)
{
    /* Keep eight ranks above nearest-rank P99.9 in the longest sample. */
    enum { MAX_WAVES = 8192, MAX_JOBS = 4096 };
    unsigned Waves, Active, Dormant;
    bool Samples = Argc == 7 && !strcmp(Argv[6], "--samples");
    if ((Argc != 6 && !Samples) || !cohortValue(Argv[2], MAX_WAVES, &Waves) || !Waves ||
        !cohortValue(Argv[3], MAX_JOBS, &Active) || !Active ||
        !cohortValue(Argv[4], MAX_JOBS, &Dormant) || Active + Dormant > MAX_JOBS ||
        (strcmp(Argv[5], "wait") && strcmp(Argv[5], "waiter") && strcmp(Argv[5], "waiter-precise"))) return 1;
    if (!cohortAffinity()) return 2;
#if defined(_WIN32)
    HMODULE Kernel = GetModuleHandleA("kernel32.dll");
    if (Kernel) preciseFileTime = (void (WINAPI*)(LPFILETIME))(uintptr_t)
        GetProcAddress(Kernel, "GetSystemTimePreciseAsFileTime");
    const char* WallClock = preciseFileTime ? "precise-filetime" : "filetime";
#else
    const char* WallClock = "clock-realtime";
#endif
    bool Direct = strcmp(Argv[5], "wait") != 0;
    bool Precise = !strcmp(Argv[5], "waiter-precise");
    xevent Event = {0};
    xeventwaiter Waiter = {0};
    xcronscheduler* Scheduler = NULL;
    if (Direct) {
        if (!xrtEventInit(&Event, false, false) || !xrtEventWaiterInit(&Waiter, &Event, 0)) return 3;
    } else if (!xcronSchedulerCreate(Active + Dormant, XCRON_SECONDS6, &Scheduler)) {
        return 3;
    }
    cronjob* Jobs = calloc(Active, sizeof(*Jobs));
    cronwave Wave = {.Count = Active};
    xtime Due;
    if (!Jobs || !cohortRegister(Scheduler, &Wave, Jobs, Dormant, Waves, &Due)) return 4;
    if (!cohortWave(Scheduler, &Waiter, Precise, &Wave, Jobs, Due)) return 5;
#if defined(_WIN32)
    ULONG64 CycleStart = 0, CycleEnd = 0;
    bool HaveCycles = Samples && QueryProcessCycleTime(GetCurrentProcess(), &CycleStart);
#endif
    double Cpu = processCpuMilliseconds(), Start = xrtTimer();
    xtime First[MAX_WAVES], Last[MAX_WAVES], Span[MAX_WAVES];
#if defined(XCRON_BENCH_WAIT_PHASE)
    waitphase Phases[MAX_WAVES];
#endif
    for (unsigned Index = 0; Index < Waves; ++Index) {
        Due += 1000;
        if (!cohortWave(Scheduler, &Waiter, Precise, &Wave, Jobs, Due)) return 5;
        First[Index] = cohortLateness(Due, Wave.FirstWall, Wave.FirstSubNs);
        Last[Index] = cohortLateness(Due, Wave.LastWall, Wave.LastSubNs);
        Span[Index] = (xtime)(Wave.LastNs - Wave.FirstNs);
#if defined(XCRON_BENCH_WAIT_PHASE)
        if (!phaseRecord(&Phases[Index], Due, Wave.FirstNs)) return 7;
#endif
    }
    double Seconds = xrtTimer() - Start, CpuEnd = processCpuMilliseconds();
#if defined(_WIN32)
    HaveCycles = HaveCycles && QueryProcessCycleTime(GetCurrentProcess(), &CycleEnd) && CycleEnd > CycleStart;
#endif
    bool Closed = Direct ? xrtEventWaiterUnit(&Waiter) && xrtEventUnit(&Event) : xcronSchedulerDestroy(Scheduler);
    free(Jobs);
    if (!Closed || Seconds <= 0 || Cpu < 0 || CpuEnd < Cpu) return 6;
    printf("drive: %s\nactive_jobs: %u\ndormant_jobs: %u\nwaves: %u\nverified_callbacks: %u\n",
        Argv[5], Active, Dormant, Waves, Active * Waves);
    printf("cpu_ns_per_callback: %.3f\nwait_seconds: %.6f\n", (CpuEnd - Cpu) * 1e6 / (Active * Waves), Seconds);
    printf("wall_clock: %s\n", WallClock);
    if (Samples) {
#if defined(_WIN32)
        if (HaveCycles) printf("process_cycles: %" PRIu64 "\n", (uint64_t)(CycleEnd - CycleStart));
#endif
        for (unsigned Index = 0; Index < Waves; ++Index)
            printf("wave_%u_ns: %lld,%lld,%lld\n", Index + 1, (long long)First[Index],
                (long long)Last[Index], (long long)Span[Index]);
#if defined(XCRON_BENCH_WAIT_PHASE)
        phasePrint(Phases, Waves);
#endif
    }
    cohortQuantiles("first_lateness", "ms", First, Waves, 0.000001);
    cohortQuantiles("last_lateness", "ms", Last, Waves, 0.000001);
    cohortQuantiles("callback_span", "us", Span, Waves, 0.001);
    return 0;
}

int main(int Argc, char** Argv)
{
    if (Argc > 1 && !strcmp(Argv[1], "cohort")) return cohort(Argc, Argv);
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
