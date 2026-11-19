#include "test.h"
#include <xrt/thread.h>

typedef struct schedulertest {
    xcronscheduler* Scheduler;
    xtime Due;
    unsigned Called;
} schedulertest;

static void dispatched(void* Context)
{
    schedulertest* T = Context;
    testRequire(xrtNow() >= T->Due, "scheduler fired before the wall deadline");
    testRequire(xcronSchedulerPoll(T->Scheduler, NULL) == XCRON_SCHEDULE_ERROR,
        "nested drive refused");
    xrtClearError();
    ++T->Called;
}

static int32 wakeScheduler(ptr Data)
{
    xrtSleep(35);
    return xcronSchedulerWake(Data) ? 0 : 1;
}

static void testWait(void)
{
    xcronscheduler* S = NULL;
    testRequire(xcronSchedulerCreate(2, XCRON_SECONDS6, &S), "create scheduler");
    xcronpoll Result;
    testRequire(xcronSchedulerPoll(S, &Result) == XCRON_SCHEDULE_IDLE &&
        !Result.Count && !Result.HasNext, "empty poll");
    testRequire(xcronSchedulerWait(S, 10, &Result) == XCRON_SCHEDULE_TIMEOUT &&
        !Result.HasNext, "empty timed wait");
    xthread* Thread = xrtThreadCreate(wakeScheduler, S, 0);
    testRequire(Thread != NULL, "create wake thread");
    testRequire(xcronSchedulerWait(S, 3000, &Result) == XCRON_SCHEDULE_WOKEN &&
        !Result.Count && !Result.HasNext, "cross-thread wake");
    testRequire(xrtThreadWaitFor(Thread, 3000) == XWAIT_OK &&
        xrtThreadExitCode(Thread) == 0, "wake physically joined");
    xrtThreadDestroy(Thread);

    xtime Before = xrtNow();
    schedulertest T = {.Scheduler = S, .Due = Before + 1000 - Before % 1000};
    xcronjobid Job = 0;
    testRequire(xcronSchedulerAdd(S, XRT_STR_LITERAL("* * * * * *"), dispatched, &T, &Job),
        "register same-thread callback");
    testRequire(xcronSchedulerWait(S, 3000, &Result) == XCRON_SCHEDULE_DISPATCHED &&
        Result.Count == 1 && Result.HasNext && T.Called == 1, "wait dispatches one occurrence");
    testRequire(xcronSchedulerRemove(S, Job), "remove recurring job");
    testRequire(xcronSchedulerWait(S, 0, &Result) == XCRON_SCHEDULE_TIMEOUT &&
        !Result.Count && !Result.HasNext, "removed job stays removed");
    testRequire(xcronSchedulerDestroy(S), "destroy waiter");
}

static void testPoll(void)
{
    xcronscheduler* S = NULL;
    testRequire(xcronSchedulerCreate(2, XCRON_SECONDS6, &S), "create polling scheduler");
    xcronjobid First = 0, Second = 0, Invalid = 99;
    schedulertest A = {.Scheduler = S}, B = {.Scheduler = S};
    testRequire(!xcronSchedulerAdd(S, XRT_STR_LITERAL("broken"), dispatched, &A, &Invalid) &&
        Invalid == 99, "invalid cron is atomic");
    xrtClearError();
    testRequire(xcronSchedulerAdd(S, XRT_STR_LITERAL("* * * * * *"), dispatched, &A, &First) &&
        xcronSchedulerAdd(S, XRT_STR_LITERAL("* * * * * *"), dispatched, &B, &Second), "register two jobs");
    testRequire(!xcronSchedulerAdd(S, XRT_STR_LITERAL("* * * * * *"), dispatched, &B, &Invalid) &&
        Invalid == 99, "capacity refusal is atomic");
    xrtClearError();
    testRequire(xcronSchedulerRemove(S, First), "remove heap member");
    xcronjobid Replacement = 0;
    testRequire(xcronSchedulerAdd(S, XRT_STR_LITERAL("* * * * * *"), dispatched, &A, &Replacement) &&
        First != Replacement && !xcronSchedulerRemove(S, First), "stale ID cannot remove replacement");
    xrtClearError();

    A.Due = B.Due = xrtNow();
    xrtSleep(2100);
    xcronpoll Result;
    testRequire(xcronSchedulerPoll(S, &Result) == XCRON_SCHEDULE_DISPATCHED &&
        Result.Count == 2 && Result.HasNext && A.Called == 1 && B.Called == 1,
        "each late recurring job dispatches once");
    testRequire(xcronSchedulerRemove(S, Second) && xcronSchedulerRemove(S, Replacement),
        "both jobs remain cancellable");
    testRequire(xcronSchedulerDestroy(S), "destroy polling scheduler");
}

static void testAllocationFailure(void)
{
    const size_t Limits[] = {1, 1024};
    for (size_t I = 0; I < sizeof(Limits) / sizeof(Limits[0]); ++I) {
        xallocationbudget* Budget = NULL;
        xallocationscope Scope = {0};
        testRequire(xrtAllocationBudgetCreate(Limits[I], &Budget) &&
            xrtAllocationBudgetEnter(Budget, &Scope), "enter scheduler allocation budget");
        xcronscheduler* Sentinel = (xcronscheduler*)(uintptr_t)1;
        testRequire(!xcronSchedulerCreate(64, XCRON_SECONDS6, &Sentinel) &&
            Sentinel == (xcronscheduler*)(uintptr_t)1, "creation OOM preserves output");
        xrtClearError();
        testRequire(xrtAllocationBudgetLeave(&Scope), "leave scheduler allocation budget");
        xallocationbudgetinfo Info = {.Size = sizeof(Info)};
        testRequire(xrtAllocationBudgetInfo(Budget, &Info) && !Info.LiveBytes &&
            Info.Rejections > 0, "creation OOM releases all partial allocations");
        xrtAllocationBudgetFree(Budget);
    }
}

int main(void)
{
    testAllocationFailure();
    testWait();
    testPoll();
    puts("[PASS] xcron application scheduler poll, wait, wake, bounds, OOM and stale handles");
    return 0;
}
