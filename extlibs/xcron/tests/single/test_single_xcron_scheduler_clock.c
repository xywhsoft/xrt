#define XCRON_MODULE_XCRON_SCHEDULER
#include <xcron/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#include "../test.h"
#include <math.h>

typedef struct clockstep {
    int64 Delay;
    xtime Wall;
    double Monotonic;
    bool Wake;
} clockstep;

typedef struct clockjob {
    unsigned Marker;
    xtime MoveWallTo;
} clockjob;

static struct {
    xcronscheduler* Scheduler;
    xtime Wall;
    double Monotonic;
    const clockstep* Steps;
    size_t Count, Consumed;
    unsigned Trace[4];
    size_t Called;
} Clock;

static xtime readWall(void)
{
    return Clock.Wall;
}

static double readMonotonic(void)
{
    return Clock.Monotonic;
}

static xwaitresult waitStep(xeventwaiter* Waiter, int64 Delay)
{
    testRequire(Clock.Consumed < Clock.Count, "unexpected clock wait");
    const clockstep* Step = &Clock.Steps[Clock.Consumed++];
    testRequire(Delay == Step->Delay, "relative wait uses the next deadline and remaining timeout");
    Clock.Wall = Step->Wall;
    Clock.Monotonic = Step->Monotonic;
    if (!Step->Wake) return XWAIT_TIMEOUT;
    testRequire(xcronSchedulerWake(Clock.Scheduler), "signal the real wake event after a wall-clock change");
    return xrtEventWaiterWaitFor(Waiter, 0);
}

/* Script only the scheduler's three clock/wait call sites. XRT keeps its
 * native clock, including the real event used by the explicit wake case. */
#define xrtNow readWall
#define xrtTimer readMonotonic
#define xrtEventWaiterWaitFor waitStep
#define XCRON_IMPLEMENTATION
#include "../../../../single/extlibs/xcron.h"
#undef xrtEventWaiterWaitFor
#undef xrtTimer
#undef xrtNow

static void recordCallback(void* Context)
{
    const clockjob* Job = Context;
    testRequire(Clock.Called < sizeof(Clock.Trace) / sizeof(Clock.Trace[0]), "extra callback");
    Clock.Trace[Clock.Called++] = Job->Marker;
    if (Job->MoveWallTo) Clock.Wall = Job->MoveWallTo;
}

static void begin(xtime Wall, double Monotonic)
{
    Clock.Wall = Wall;
    Clock.Monotonic = Monotonic;
    Clock.Steps = NULL;
    Clock.Count = Clock.Consumed = Clock.Called = 0;
    testRequire(xcronSchedulerCreate(2, XCRON_SECONDS6, &Clock.Scheduler), "create clock-test scheduler");
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
    const clockstep Forward[] = {{500, 16200, 10.125, true}};
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
    const clockstep Next[] = {{1, 17000, 10.25, false}};
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
    const clockstep Backward[] = {{250, 19500, 100.25, false}, {750, 20250, 101.0, false}};
    plan(Backward, 2);
    xcronpoll Result;
    testRequire(xcronSchedulerWait(Clock.Scheduler, 1000, &Result) == XCRON_SCHEDULE_TIMEOUT &&
        report(Result, 0, 21000) && !Clock.Called, "the monotonic timeout survives a backward wall step");
    const clockstep Resume[] = {{750, 21000, 101.75, false}};
    plan(Resume, 1);
    testRequire(xcronSchedulerWait(Clock.Scheduler, 1000, &Result) == XCRON_SCHEDULE_DISPATCHED &&
        report(Result, 1, 22000) && Clock.Called == 1, "a later drive still delivers the original occurrence");
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
    const clockstep Broken[] = {{500, 30600, NAN, false}};
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

static void testLastInstant(void)
{
    const xtime Last = INT64_MAX - INT64_MAX % 1000;
    const xstrview EverySecond = XRT_STR_LITERAL("* * * * * *");
    begin(Last - 1, 50.0);
    clockjob Job = {.Marker = 1};
    xcronjobid Id = 0;
    testRequire(xcronSchedulerAdd(Clock.Scheduler, EverySecond, recordCallback, &Job, &Id),
        "register before the final representable occurrence");
    Clock.Wall = INT64_MIN;
    const clockstep Wide[] = {{INT64_MAX, Last, 50.5, true}};
    plan(Wide, 1);
    xcronpoll Result;
    testRequire(xcronSchedulerWait(Clock.Scheduler, XRT_WAIT_FOREVER, &Result) == XCRON_SCHEDULE_WOKEN &&
        report(Result, 0, Last) && !Clock.Called, "full-range wall difference clamps to the wait limit");
    testRequire(xcronSchedulerPoll(Clock.Scheduler, &Result) == XCRON_SCHEDULE_DISPATCHED &&
        Result.Count == 1 && !Result.HasNext && Clock.Called == 1, "final representable occurrence retires the job");
    testRequire(!xcronSchedulerRemove(Clock.Scheduler, Id), "naturally retired handle is stale");
    xrtClearError();
    Clock.Wall = 40500;
    add(EverySecond, &Job);
    add(EverySecond, &Job);
    testRequire(xcronSchedulerPoll(Clock.Scheduler, &Result) == XCRON_SCHEDULE_IDLE &&
        report(Result, 0, 41000), "retirement restores the slot for future registration");
    end();
}

int main(void)
{
    testForwardThenBack();
    testBackwardTimeout();
    testWaveSample();
    testClockFailure();
    testLastInstant();
    puts("[PASS] xcron scheduler clock steps, timeout, wave cutoff, recovery and time bounds");
    return 0;
}
