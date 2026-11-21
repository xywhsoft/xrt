#define XCRON_MODULE_XCRON_SCHEDULER
#include <xcron/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#include "../test.h"
#include <math.h>

typedef struct clockstep {
    double Deadline;
    xtime Wall;
    double Monotonic;
    bool Wake;
} clockstep;

typedef struct clockjob {
    unsigned Marker;
    xtime MoveWallTo;
} clockjob;

enum { MODEL_CAPACITY = 24, MODEL_STEPS = 2048 };

static struct {
    xcronscheduler* Scheduler;
    xtime Wall;
    double Monotonic;
    double DelayAfterWall;
    uint32 SubNs;
    const clockstep* Steps;
    size_t Count, Consumed;
    unsigned Trace[MODEL_CAPACITY];
    size_t Called;
} Clock;

static xtime readWall(void)
{
    return Clock.Wall;
}

static xtime readWallWithSubMs(uint32* SubNs)
{
    xtime Wall = Clock.Wall;
    *SubNs = Clock.SubNs;
    Clock.Monotonic += Clock.DelayAfterWall;
    Clock.DelayAfterWall = 0;
    return Wall;
}

static double readMonotonic(void)
{
    return Clock.Monotonic;
}

static xwaitresult waitStep(xeventwaiter* Waiter, double Deadline)
{
    testRequire(Clock.Consumed < Clock.Count, "unexpected clock wait");
    const clockstep* Step = &Clock.Steps[Clock.Consumed++];
    testRequire(Deadline >= Step->Deadline - 1e-12 &&
        Deadline <= nextafter(Step->Deadline, INFINITY) + 1e-12,
        "absolute wait preserves the earlier of the occurrence and caller deadline");
    Clock.SubNs = 0;
    Clock.Wall = Step->Wall;
    Clock.Monotonic = Step->Monotonic;
    if (!Step->Wake) return XWAIT_TIMEOUT;
    testRequire(xcronSchedulerWake(Clock.Scheduler), "signal the real wake event after a wall-clock change");
    return xrtEventWaiterWaitFor(Waiter, 0);
}

/* Script only the scheduler's three clock/wait call sites. XRT keeps its
 * native clock, including the real event used by the explicit wake case. */
#define xrtNow readWall
#define __xrtNowWithSubMs readWallWithSubMs
#define xrtTimer readMonotonic
#define __xrtEventWaiterWaitUntil waitStep
#define XCRON_IMPLEMENTATION
#include "../../../../single/extlibs/xcron.h"
#undef __xrtEventWaiterWaitUntil
#undef xrtTimer
#undef __xrtNowWithSubMs
#undef xrtNow

static void recordCallback(void* Context)
{
    const clockjob* Job = Context;
    testRequire(Clock.Called < sizeof(Clock.Trace) / sizeof(Clock.Trace[0]), "extra callback");
    Clock.Trace[Clock.Called++] = Job->Marker;
    if (Job->MoveWallTo) Clock.Wall = Job->MoveWallTo;
}

static void beginScheduler(xtime Wall, double Monotonic, size_t Capacity, xcrondialect Dialect)
{
    Clock.Wall = Wall;
    Clock.Monotonic = Monotonic;
    Clock.SubNs = 0;
    Clock.Steps = NULL;
    Clock.Count = Clock.Consumed = Clock.Called = 0;
    testRequire(xcronSchedulerCreate(Capacity, Dialect, &Clock.Scheduler), "create clock-test scheduler");
}

static void begin(xtime Wall, double Monotonic)
{
    beginScheduler(Wall, Monotonic, 2, XCRON_SECONDS6);
}

static void end(void)
{
    testRequire(Clock.Consumed == Clock.Count, "every planned wait was observed");
    testRequire(xcronSchedulerDestroy(Clock.Scheduler), "destroy clock-test scheduler");
}

static void plan(const clockstep* Steps, size_t Count)
{
    testRequire(Clock.Consumed == Clock.Count, "previous wait script is complete");
    Clock.Steps = Steps;
    Clock.Count = Count;
    Clock.Consumed = 0;
}

static void add(xstrview Cron, clockjob* Job)
{
    xcronjobid Id = 0;
    testRequire(xcronSchedulerAdd(Clock.Scheduler, Cron, recordCallback, Job, &Id) && Id,
        "register clock-test job");
}

static bool report(xcronpoll Result, size_t Count, xtime Next)
{
    return Result.Count == Count && Result.HasNext && Result.Next == Next;
}

static void testForwardThenBack(void)
{
    begin(10500, 10.0);
    clockjob First = {.Marker = 1}, Second = {.Marker = 2};
    add(XRT_STR_LITERAL("* * * * * *"), &First);
    add(XRT_STR_LITERAL("* * * * * *"), &Second);
    const clockstep Forward[] = {{10.5, 16200, 10.125, true}};
    plan(Forward, 1);
    xcronpoll Result;
    testRequire(xcronSchedulerWait(Clock.Scheduler, XRT_WAIT_FOREVER, &Result) == XCRON_SCHEDULE_WOKEN &&
        report(Result, 0, 11000) && !Clock.Called, "wake returns the sampled deadline for the APP to recheck");
    testRequire(xcronSchedulerPoll(Clock.Scheduler, &Result) == XCRON_SCHEDULE_DISPATCHED &&
        report(Result, 2, 17000) && Clock.Called == 2 && Clock.Trace[0] == 1 && Clock.Trace[1] == 2,
        "a forward step delivers each late job once in registration order");

    Clock.Wall = 10900;
    testRequire(xcronSchedulerWait(Clock.Scheduler, 0, &Result) == XCRON_SCHEDULE_TIMEOUT &&
        report(Result, 0, 17000) && Clock.Called == 2, "rollback preserves advanced deadlines");
    Clock.Wall = 16999;
    const clockstep Next[] = {{10.126, 17000, 10.25, false}};
    plan(Next, 1);
    testRequire(xcronSchedulerWait(Clock.Scheduler, 1000, &Result) == XCRON_SCHEDULE_DISPATCHED &&
        report(Result, 2, 18000) && Clock.Called == 4 && Clock.Trace[2] == 1 && Clock.Trace[3] == 2,
        "the next wave runs once when its preserved deadline arrives");
    end();
}

static void testBackwardTimeout(void)
{
    begin(20750, 100.0);
    clockjob Job = {.Marker = 1};
    add(XRT_STR_LITERAL("* * * * * *"), &Job);
    const clockstep Backward[] = {{100.25, 19500, 100.25, false}, {101.0, 20250, 101.0, false}};
    plan(Backward, 2);
    xcronpoll Result;
    testRequire(xcronSchedulerWait(Clock.Scheduler, 1000, &Result) == XCRON_SCHEDULE_TIMEOUT &&
        report(Result, 0, 21000) && !Clock.Called, "the monotonic timeout survives a backward wall step");
    const clockstep Resume[] = {{101.75, 21000, 101.75, false}};
    plan(Resume, 1);
    testRequire(xcronSchedulerWait(Clock.Scheduler, 1000, &Result) == XCRON_SCHEDULE_DISPATCHED &&
        report(Result, 1, 22000) && Clock.Called == 1, "a later drive still delivers the original occurrence");
    end();
}

static void testLateTimerReturn(void)
{
    const struct {
        int64 Timeout;
        double Deadline;
        xcronschedulestatus Expected;
    } Cases[] = {
        {125, 100.125, XCRON_SCHEDULE_TIMEOUT},
        {250, 100.25, XCRON_SCHEDULE_DISPATCHED},
        {500, 100.25, XCRON_SCHEDULE_DISPATCHED}
    };

    for (size_t I = 0; I < sizeof(Cases) / sizeof(Cases[0]); ++I) {
        begin(20750, 100.0);
        clockjob Job = {.Marker = 1};
        add(XRT_STR_LITERAL("* * * * * *"), &Job);
        const clockstep Late = {Cases[I].Deadline, 21375, 100.625, false};
        plan(&Late, 1);
        xcronpoll Result;
        testRequire(xcronSchedulerWait(Clock.Scheduler, Cases[I].Timeout, &Result) == Cases[I].Expected,
            "late timer return preserves caller and occurrence precedence");
        if (Cases[I].Expected == XCRON_SCHEDULE_TIMEOUT) {
            testRequire(report(Result, 0, 21000) && !Clock.Called,
                "a shorter caller timeout leaves the occurrence pending");
            testRequire(xcronSchedulerPoll(Clock.Scheduler, &Result) == XCRON_SCHEDULE_DISPATCHED,
                "the next drive dispatches the pending occurrence");
        }
        testRequire(report(Result, 1, 22000) && Clock.Called == 1,
            "late timer return delivers the occurrence once");
        end();
    }
}

static void testFractionalDeadline(void)
{
    begin(10999, 10.0);
    clockjob Job = {.Marker = 1};
    add(XRT_STR_LITERAL("* * * * * *"), &Job);
    Clock.SubNs = 250000;
    const clockstep Early[] = {{10.00075, 10999, 10.00070, false},
        {10.00170, 11000, 10.00170, false}};
    plan(Early, 2);
    xcronpoll Result;
    testRequire(xcronSchedulerWait(Clock.Scheduler, 20, &Result) == XCRON_SCHEDULE_DISPATCHED &&
        report(Result, 1, 12000) && Clock.Called == 1,
        "fractional wait rechecks the wall occurrence before dispatch");
    end();
}

static void testWaitAfterSamplingDelay(void)
{
	begin(30400, 80.0);
	clockjob Job = {.Marker = 1};
	add(XRT_STR_LITERAL("* * * * * *"), &Job);
	Clock.DelayAfterWall = 0.45;
	xcronpoll Result;
	testRequire(xcronSchedulerWait(Clock.Scheduler, 250, &Result) == XCRON_SCHEDULE_TIMEOUT &&
		report(Result, 0, 31000) && !Clock.Called, "elapsed caller limit is checked before arming");

	Clock.Wall = 30400;
	Clock.Monotonic = 80.0;
	Clock.DelayAfterWall = 0.45;
	const clockstep AtDue[] = {{80.6, 31000, 80.6, false}};
	plan(AtDue, 1);
	testRequire(xcronSchedulerWait(Clock.Scheduler, 1500, &Result) == XCRON_SCHEDULE_DISPATCHED &&
		report(Result, 1, 32000) && Clock.Called == 1, "time after a UTC sample is not added to the deadline");
	end();
}

static void testWaveSample(void)
{
    begin(10500, 20.0);
    clockjob First = {.Marker = 1, .MoveWallTo = 20000}, Second = {.Marker = 2};
    add(XRT_STR_LITERAL("11 * * * * *"), &First);
    add(XRT_STR_LITERAL("12 * * * * *"), &Second);
    Clock.Wall = 11000;
    xcronpoll Result;
    testRequire(xcronSchedulerPoll(Clock.Scheduler, &Result) == XCRON_SCHEDULE_DISPATCHED &&
        report(Result, 1, 12000) && Clock.Called == 1 && Clock.Trace[0] == 1,
        "a callback moving the wall clock leaves the current wave's cutoff intact");
    testRequire(xcronSchedulerPoll(Clock.Scheduler, &Result) == XCRON_SCHEDULE_DISPATCHED &&
        report(Result, 1, 71000) && Clock.Called == 2 && Clock.Trace[1] == 2,
        "the next drive uses the newer wall-clock sample");
    end();
}

static void testClockFailure(void)
{
    begin(0, 30.0);
    clockjob Job = {.Marker = 1};
    xcronjobid Id = 17;
    const xstrview EverySecond = XRT_STR_LITERAL("* * * * * *");
    testRequire(!xcronSchedulerAdd(Clock.Scheduler, EverySecond, recordCallback, &Job, &Id) && Id == 17,
        "unavailable registration clock preserves the job output");
    xrtClearError();
    Clock.Wall = 30500;
    add(EverySecond, &Job);
    xcronpoll Result = {.Count = 17, .HasNext = true, .Next = 42};
    Clock.Monotonic = NAN;
    testRequire(xcronSchedulerWait(Clock.Scheduler, 1000, &Result) == XCRON_SCHEDULE_ERROR &&
        report(Result, 17, 42) && !Clock.Called, "unavailable entry timer preserves output");
    Clock.Monotonic = 30.0;
    const clockstep Broken[] = {{30.5, 30600, NAN, false}};
    plan(Broken, 1);
    testRequire(xcronSchedulerWait(Clock.Scheduler, 1000, &Result) == XCRON_SCHEDULE_ERROR &&
        report(Result, 17, 42) && !Clock.Called, "timer loss during a wait preserves output");
    Clock.Monotonic = 30.5;
    Clock.Wall = 0;
    testRequire(xcronSchedulerPoll(Clock.Scheduler, &Result) == XCRON_SCHEDULE_ERROR &&
        report(Result, 17, 42) && !Clock.Called, "wall-clock loss preserves output");
    xrtClearError();
    Clock.Wall = 31000;
    testRequire(xcronSchedulerPoll(Clock.Scheduler, &Result) == XCRON_SCHEDULE_DISPATCHED &&
        report(Result, 1, 32000) && Clock.Called == 1, "clock recovery preserves the pending job");
    end();
}

static void testDistinctRecurrences(void)
{
    const xtime Monday = XRT_TIME_UNIX_EPOCH + INT64_C(4) * 86400000;
    const struct {
        xstrview First, Second;
        xtime Due, Next;
    } Cases[] = {
        /* Equal masks and different DOM star provenance have different deadlines. */
        {XRT_STR_LITERAL("0 0 0 * * MON"), XRT_STR_LITERAL("0 0 0 1-31 * MON"),
            Monday, Monday + 86400000},
        {XRT_STR_LITERAL("0 * * * * *"), XRT_STR_LITERAL("* * * * * *"), 60000, 61000},
    };
    for (size_t I = 0; I < sizeof(Cases) / sizeof(*Cases); ++I) {
        begin(Cases[I].Due - 1, 40.0);
        clockjob First = {.Marker = 1}, Second = {.Marker = 2};
        add(Cases[I].First, &First);
        add(Cases[I].Second, &Second);
        xcronpoll Result;
        Clock.Wall = Cases[I].Due;
        testRequire(xcronSchedulerPoll(Clock.Scheduler, &Result) == XCRON_SCHEDULE_DISPATCHED &&
            report(Result, 2, Cases[I].Next) && Clock.Called == 2 &&
            Clock.Trace[0] == 1 && Clock.Trace[1] == 2, "equal due times preserve distinct recurrences");
        Clock.Wall = Cases[I].Next;
        testRequire(xcronSchedulerPoll(Clock.Scheduler, &Result) == XCRON_SCHEDULE_DISPATCHED &&
            Result.Count == 1 && Clock.Called == 3 && Clock.Trace[2] == 2,
            "only the earlier recurrence fires next");
        end();
    }
}

static void testReusedSlotWaveOrder(void)
{
    beginScheduler(10500, 40.0, 4, XCRON_SECONDS6);
    const xstrview EverySecond = XRT_STR_LITERAL("* * * * * *");
    clockjob Jobs[] = {{.Marker = 1}, {.Marker = 2}, {.Marker = 3}, {.Marker = 4}, {.Marker = 5}};
    xcronjobid Ids[4];
    for (unsigned I = 0; I < 4; ++I)
        testRequire(xcronSchedulerAdd(Clock.Scheduler, EverySecond, recordCallback, &Jobs[I], &Ids[I]),
            "register equal-deadline job before slot reuse");
    testRequire(xcronSchedulerRemove(Clock.Scheduler, Ids[1]), "remove the second job");
    add(EverySecond, &Jobs[4]);

    Clock.Wall = 11000;
    const unsigned Expected[] = {1, 3, 4, 5};
    xcronpoll Result;
    testRequire(xcronSchedulerPoll(Clock.Scheduler, &Result) == XCRON_SCHEDULE_DISPATCHED &&
        report(Result, 4, 12000) && Clock.Called == 4 && !memcmp(Clock.Trace, Expected, sizeof(Expected)),
        "equal deadlines dispatch in registration order after heap slot reuse");
    end();
}

static void testLastInstant(void)
{
    const xtime Last = INT64_MAX - INT64_MAX % 1000;
    const xstrview EverySecond = XRT_STR_LITERAL("* * * * * *");
    begin(Last - 1, 50.0);
    clockjob Job = {.Marker = 1};
    xcronjobid Id = 0;
    testRequire(xcronSchedulerAdd(Clock.Scheduler, EverySecond, recordCallback, &Job, &Id),
        "register before the final representable occurrence");
    add(EverySecond, &Job);
    Clock.Wall = INT64_MIN;
    const clockstep Wide[] = {{50.0 + (double)INT64_MAX * 0.001, Last, 50.5, true}};
    plan(Wide, 1);
    xcronpoll Result;
    testRequire(xcronSchedulerWait(Clock.Scheduler, XRT_WAIT_FOREVER, &Result) == XCRON_SCHEDULE_WOKEN &&
        report(Result, 0, Last) && !Clock.Called, "full-range wall difference clamps to the wait limit");
    testRequire(xcronSchedulerPoll(Clock.Scheduler, &Result) == XCRON_SCHEDULE_DISPATCHED &&
        Result.Count == 2 && !Result.HasNext && Clock.Called == 2, "final representable occurrence retires both jobs");
    testRequire(!xcronSchedulerRemove(Clock.Scheduler, Id), "naturally retired handle is stale");
    xrtClearError();
    Clock.Wall = 40500;
    add(EverySecond, &Job);
    add(EverySecond, &Job);
    testRequire(xcronSchedulerPoll(Clock.Scheduler, &Result) == XCRON_SCHEDULE_IDLE &&
        report(Result, 0, 41000), "retirement restores the slot for future registration");
    end();
}

typedef struct modeljob {
    xcronjobid Id, Retired;
    unsigned Marker, Rule;
    xtime Next;
} modeljob;

/* The reference scans wall labels and keeps a flat list of jobs. */
static xtime modelNext(xtime Now, xtime Unit, unsigned Rule)
{
    xtime At = (Now - XRT_TIME_UNIX_EPOCH) / Unit + 1;
    for (;; ++At) {
        const unsigned Field = (unsigned)(At % 60);
        const bool Matches[] = {Field % 2 == 0, Field % 3 == 1, Field % 15 == 0,
            true, Field == 7 || Field == 11 || Field == 18 || Field == 35 || Field == 48};
        if (Matches[Rule]) return XRT_TIME_UNIX_EPOCH + At * Unit;
    }
}

static void modelCheckWave(modeljob* Jobs, xtime Unit, bool Wait)
{
    unsigned Expected[MODEL_CAPACITY];
    size_t Count = 0;
    xtime Next;
    for (;;) {
        modeljob* First = NULL;
        for (size_t I = 0; I < MODEL_CAPACITY; ++I) {
            modeljob* Job = Jobs + I;
            if (Job->Id && (!First || Job->Next < First->Next ||
                (Job->Next == First->Next && Job->Marker < First->Marker))) First = Job;
        }
        Next = First ? First->Next : 0;
        if (!First || Next > Clock.Wall) break;
        testRequire(Count < MODEL_CAPACITY, "each reference job dispatches at most once");
        Expected[Count++] = First->Marker;
        First->Next = modelNext(Clock.Wall, Unit, First->Rule);
    }
    xcronpoll Result;
    Clock.Called = 0;
    xcronschedulestatus Status = Wait ? xcronSchedulerWait(Clock.Scheduler, 0, &Result) :
        xcronSchedulerPoll(Clock.Scheduler, &Result);
    testRequire(Status == (Count ? XCRON_SCHEDULE_DISPATCHED :
        (Wait ? XCRON_SCHEDULE_TIMEOUT : XCRON_SCHEDULE_IDLE)), "mixed scheduler status");
    testRequire(Result.Count == Count && Result.HasNext == (Next != 0) &&
        (!Next || Result.Next == Next), "mixed scheduler count and next UTC deadline");
    testRequire(Clock.Called == Count && !memcmp(Clock.Trace, Expected, Count * sizeof(*Expected)),
        "mixed scheduler callback order and context");
}

static void testMixedScheduler(xcrondialect Dialect)
{
    const char* Fields[] = {"*/2", "1-59/3", "0,15,30,45", "*", "7,11,18,35,48"};
    xtime Unit = Dialect == XCRON_UNIX5 ? 60000 : 1000;
    modeljob Jobs[MODEL_CAPACITY] = {0};
    clockjob Contexts[MODEL_STEPS];
    unsigned Registered = 0, Removed = 0, Reused = 0;
    uint32 Random = 0x93792155u;
    beginScheduler(XRT_TIME_UNIX_EPOCH + 3600000 + 123, 100.0, MODEL_CAPACITY, Dialect);
    for (unsigned Step = 0; Step < MODEL_STEPS; ++Step) {
        Random = Random * 1664525u + 1013904223u;
        modeljob* Job = Jobs + ((Random >> 16) % MODEL_CAPACITY);
        if ((Random >> 29) < 3) {
            if (Job->Id) {
                testRequire(xcronSchedulerRemove(Clock.Scheduler, Job->Id), "remove mixed scheduler job");
                Job->Retired = Job->Id;
                Job->Id = 0;
                ++Removed;
            } else {
                Job->Rule = (Random >> 8) % (sizeof(Fields) / sizeof(*Fields));
                Job->Marker = Registered + 1;
                Job->Next = modelNext(Clock.Wall, Unit, Job->Rule);
                Contexts[Registered++] = (clockjob){.Marker = Job->Marker};
                char Cron[40];
                int Size = snprintf(Cron, sizeof(Cron), "%s %s", Fields[Job->Rule],
                    Dialect == XCRON_UNIX5 ? "* * * *" : "* * * * *");
                testRequire(Size > 0 && (size_t)Size < sizeof(Cron) &&
                    xcronSchedulerAdd(Clock.Scheduler, (xstrview){Cron, (size_t)Size}, recordCallback,
                        Contexts + Registered - 1, &Job->Id) && Job->Id,
                    "register a mixed scheduler rule and context");
                if (Job->Retired) {
                    testRequire(!xcronSchedulerRemove(Clock.Scheduler, Job->Retired),
                        "stale handle cannot remove a mixed scheduler job");
                    xrtClearError();
                    ++Reused;
                }
            }
        } else {
            /* Include backward steps and forward jumps across whole periods. */
            const int Steps = (int)((Random >> 6) % 21) - 6;
            Clock.Wall += (xtime)Steps * Unit + (xtime)(Random % 1000u);
        }
        modelCheckWave(Jobs, Unit, Step % 2 != 0);
    }
    testRequire(Registered > 300 && Removed > 300 && Reused > 300,
        "mixed scheduler fixture covered repeated registration, removal and reuse");
    printf("[PASS] xcron mixed scheduler dialect=%d transitions=%u added=%u removed=%u stale=%u\n",
        (int)Dialect, (unsigned)MODEL_STEPS, Registered, Removed, Reused);
    end();
}

int main(void)
{
    testForwardThenBack();
    testBackwardTimeout();
    testLateTimerReturn();
    testFractionalDeadline();
    testWaitAfterSamplingDelay();
    testWaveSample();
    testClockFailure();
    testDistinctRecurrences();
    testReusedSlotWaveOrder();
    testLastInstant();
    testMixedScheduler(XCRON_UNIX5);
    testMixedScheduler(XCRON_SECONDS6);
    puts("[PASS] xcron scheduler clock steps, timeout, wave cutoff, recovery and time bounds");
    return 0;
}
