#include <xcron/scheduler.h>
#include <xrt/error.h>
#include <xrt/event_waiter.h>
#include <xrt/memory.h>
#include <math.h>

#if defined(XCRON_FEATURE_XCRON_SCHEDULER)

#define XCRON_NO_SLOT UINT32_MAX

typedef struct xcronslot {
    xcronexpression Expression;
    xcroncallback Callback;
    void* Context;
    xtime Next;
    uint32 Serial;
    uint32 Position;
} xcronslot;

struct xcronscheduler {
    xcronslot* Jobs;
    uint32* Heap;
    uint32 Capacity;
    uint32 Count;
    uint32 Free;
    uint32 Serial;
    xcrondialect Dialect;
    bool Driving;
    bool HasWaiter;
    xevent Wake;
    xeventwaiter Waiter;
};

static bool xcronScheduleError(xerrkind Kind, cstr Message)
{
    xrtSetErrorInfo(Kind, "xrt.cron.scheduler", (int32)Kind, Message);
    return false;
}

static bool xcronScheduleOwner(xcronscheduler* S)
{
    if (!S) return xcronScheduleError(XERR_ARGUMENT, "scheduler required");
    if (S->Driving) return xcronScheduleError(XERR_STATE, "scheduler drive is active");
    return true;
}

static bool xcronScheduleEarlier(const xcronscheduler* S, uint32 A, uint32 B)
{
    const xcronslot* L = &S->Jobs[A];
    const xcronslot* R = &S->Jobs[B];
    return L->Next < R->Next || (L->Next == R->Next && L->Serial < R->Serial);
}

static void xcronSchedulePlace(xcronscheduler* S, uint32 Position, uint32 Slot)
{
    S->Heap[Position] = Slot;
    S->Jobs[Slot].Position = Position;
}

static void xcronScheduleUp(xcronscheduler* S, uint32 Position, uint32 Slot)
{
    while (Position) {
        uint32 Parent = (Position - 1) / 2;
        uint32 Other = S->Heap[Parent];
        if (!xcronScheduleEarlier(S, Slot, Other)) break;
        xcronSchedulePlace(S, Position, Other);
        Position = Parent;
    }
    xcronSchedulePlace(S, Position, Slot);
}

static void xcronScheduleDown(xcronscheduler* S, uint32 Position, uint32 Slot)
{
    while (Position < S->Count / 2) {
        uint32 Child = Position * 2 + 1;
        if (Child + 1 < S->Count &&
            xcronScheduleEarlier(S, S->Heap[Child + 1], S->Heap[Child]))
            ++Child;
        uint32 Other = S->Heap[Child];
        if (!xcronScheduleEarlier(S, Other, Slot)) break;
        xcronSchedulePlace(S, Position, Other);
        Position = Child;
    }
    xcronSchedulePlace(S, Position, Slot);
}

static void xcronScheduleRemoveSlot(xcronscheduler* S, uint32 Slot)
{
    xcronslot* Job = &S->Jobs[Slot];
    uint32 Position = Job->Position;
    uint32 Last = S->Heap[--S->Count];
    if (Position < S->Count) {
        if (Position && xcronScheduleEarlier(S, Last, S->Heap[(Position - 1) / 2]))
            xcronScheduleUp(S, Position, Last);
        else
            xcronScheduleDown(S, Position, Last);
    }
    *Job = (xcronslot){.Position = S->Free};
    S->Free = Slot;
}

XCRON_API bool xcronSchedulerCreate(size_t Capacity, xcrondialect Dialect, xcronscheduler** Output)
{
    if (!xrtMemRangeValid(Output, sizeof(*Output)) || !Capacity || Capacity >= UINT32_MAX ||
        Capacity > SIZE_MAX / sizeof(xcronslot) || Capacity > SIZE_MAX / sizeof(uint32) ||
        (Dialect != XCRON_UNIX5 && Dialect != XCRON_SECONDS6))
        return xcronScheduleError(XERR_ARGUMENT, "invalid scheduler capacity, dialect or output");

    xcronscheduler* S = xrtCalloc(1, sizeof(*S));
    if (!S) return false;
    S->Jobs = xrtCalloc(Capacity, sizeof(*S->Jobs));
    if (S->Jobs) S->Heap = xrtMalloc(Capacity * sizeof(*S->Heap));
    if (!S->Heap || !xrtEventInit(&S->Wake, false, false)) {
        xrtFree(S->Heap);
        xrtFree(S->Jobs);
        xrtFree(S);
        return false;
    }
    S->Capacity = (uint32)Capacity;
    S->Dialect = Dialect;
    for (uint32 I = 0; I < S->Capacity; ++I)
        S->Jobs[I].Position = I + 1;
    S->Jobs[Capacity - 1].Position = XCRON_NO_SLOT;
    *Output = S;
    return true;
}

XCRON_API bool xcronSchedulerAdd(xcronscheduler* S, xstrview Text, xcroncallback Callback,
    void* Context, xcronjobid* Output)
{
    if (!xcronScheduleOwner(S)) return false;
    if (!Callback || !xrtMemRangeValid(Output, sizeof(*Output)) ||
        xrtMemRangesOverlap(Text.Data, Text.Size, Output, sizeof(*Output)))
        return xcronScheduleError(XERR_ARGUMENT, "invalid scheduler callback or job output");
    if (S->Free == XCRON_NO_SLOT || S->Serial == UINT32_MAX)
        return xcronScheduleError(XERR_RANGE, "scheduler job capacity or identifier limit reached");

    xcronexpression Expression;
    if (!xcronExpressionCompile(Text, S->Dialect, &Expression, NULL)) return false;
    xtime Now = xrtNow();
    if (!Now) return xcronScheduleError(XERR_IO, "wall clock unavailable");
    xtime Next;
    xcroncalendarstatus Search = xcronCalendarNext(&Expression, Now, INT64_MAX,
        XCRON_SEARCH_EXCLUSIVE, &Next, NULL);
    if (Search == XCRON_CALENDAR_ERROR) return false;
    if (Search != XCRON_CALENDAR_FOUND)
        return xcronScheduleError(XERR_RANGE, "cron expression has no future occurrence");

    uint32 Slot = S->Free;
    S->Free = S->Jobs[Slot].Position;
    uint32 Serial = ++S->Serial;
    S->Jobs[Slot] = (xcronslot){.Expression = Expression, .Callback = Callback,
        .Context = Context, .Next = Next, .Serial = Serial};
    xcronScheduleUp(S, S->Count++, Slot);
    *Output = ((uint64)Serial << 32) | ((uint64)Slot + 1);
    return true;
}

XCRON_API bool xcronSchedulerRemove(xcronscheduler* S, xcronjobid Job)
{
    if (!xcronScheduleOwner(S)) return false;
    uint32 Slot = (uint32)Job - 1;
    uint32 Serial = (uint32)(Job >> 32);
    if (Slot >= S->Capacity || !Serial || S->Jobs[Slot].Serial != Serial)
        return xcronScheduleError(XERR_ARGUMENT, "unknown scheduler job");
    xcronScheduleRemoveSlot(S, Slot);
    return true;
}

static xcronschedulestatus xcronScheduleDue(xcronscheduler* S, xtime Now, xcronpoll* Result)
{
    xcronpoll Report = {0};
    while (S->Count) {
        uint32 Slot = S->Heap[0];
        xcronslot* Job = &S->Jobs[Slot];
        if (Job->Next > Now) break;
        xcroncallback Callback = Job->Callback;
        void* Context = Job->Context;
        xtime Next;
        xcroncalendarstatus Search = xcronCalendarNext(&Job->Expression, Now, INT64_MAX,
            XCRON_SEARCH_EXCLUSIVE, &Next, NULL);
        if (Search == XCRON_CALENDAR_ERROR) return XCRON_SCHEDULE_ERROR;
        if (Search == XCRON_CALENDAR_FOUND) {
            Job->Next = Next;
            xcronScheduleDown(S, 0, Slot);
        } else {
            xcronScheduleRemoveSlot(S, Slot);
        }
        ++Report.Count;
        Callback(Context);
    }
    if (S->Count) {
        Report.HasNext = true;
        Report.Next = S->Jobs[S->Heap[0]].Next;
    }
    *Result = Report;
    return Report.Count ? XCRON_SCHEDULE_DISPATCHED : XCRON_SCHEDULE_IDLE;
}

static xcronschedulestatus xcronScheduleDrive(xcronscheduler* S, int64 Timeout,
    bool Wait, xcronpoll* Output)
{
    if (!xcronScheduleOwner(S)) return XCRON_SCHEDULE_ERROR;
    if ((Output && !xrtMemRangeValid(Output, sizeof(*Output))) || Timeout < XRT_WAIT_FOREVER) {
        xcronScheduleError(XERR_ARGUMENT, "invalid scheduler timeout or output");
        return XCRON_SCHEDULE_ERROR;
    }

    S->Driving = true;
    double Started = Timeout > 0 ? xrtTimer() : 0;
    xcronpoll Report;
    xcronschedulestatus Status = XCRON_SCHEDULE_ERROR;
    if (!isfinite(Started)) goto Done;
    for (;;) {
        xtime Now = xrtNow();
        if (!Now) {
            xcronScheduleError(XERR_IO, "wall clock unavailable");
            Status = XCRON_SCHEDULE_ERROR;
            break;
        }
        Status = xcronScheduleDue(S, Now, &Report);
        if (Status != XCRON_SCHEDULE_IDLE || !Wait) break;

        int64 Delay = XRT_WAIT_FOREVER;
        if (Timeout >= 0) {
            double Sample = Timeout ? xrtTimer() : Started;
            if (!isfinite(Sample)) { Status = XCRON_SCHEDULE_ERROR; break; }
            double Elapsed = (Sample - Started) * 1000.0;
            if (Elapsed >= (double)Timeout) { Status = XCRON_SCHEDULE_TIMEOUT; break; }
            double Remaining = ceil((double)Timeout - Elapsed);
            Delay = Remaining >= (double)INT64_MAX ? INT64_MAX : (int64)Remaining;
        }
        if (Report.HasNext) {
            uint64 WallDelay = (uint64)Report.Next - (uint64)Now;
            int64 Due = WallDelay > INT64_MAX ? INT64_MAX : (int64)WallDelay;
            if (Delay == XRT_WAIT_FOREVER || Due < Delay) Delay = Due;
        }
        if (!S->HasWaiter) {
            if (!xrtEventWaiterInit(&S->Waiter, &S->Wake, 0)) {
                Status = XCRON_SCHEDULE_ERROR;
                break;
            }
            S->HasWaiter = true;
        }
        xwaitresult Result = xrtEventWaiterWaitFor(&S->Waiter, Delay);
        if (Result == XWAIT_OK) { Status = XCRON_SCHEDULE_WOKEN; break; }
        if (Result != XWAIT_TIMEOUT) { Status = XCRON_SCHEDULE_ERROR; break; }
    }
Done:
    S->Driving = false;
    if (Status != XCRON_SCHEDULE_ERROR && Output) *Output = Report;
    return Status;
}

XCRON_API xcronschedulestatus xcronSchedulerPoll(xcronscheduler* S, xcronpoll* Output)
{
    return xcronScheduleDrive(S, 0, false, Output);
}

XCRON_API xcronschedulestatus xcronSchedulerWait(xcronscheduler* S, int64 Timeout,
    xcronpoll* Output)
{
    return xcronScheduleDrive(S, Timeout, true, Output);
}

XCRON_API bool xcronSchedulerWake(xcronscheduler* S)
{
    return S ? xrtEventSet(&S->Wake) : xcronScheduleError(XERR_ARGUMENT, "scheduler required");
}

XCRON_API bool xcronSchedulerDestroy(xcronscheduler* S)
{
    if (!xcronScheduleOwner(S)) return false;
    if (S->HasWaiter) {
        if (!xrtEventWaiterUnit(&S->Waiter)) return false;
        S->HasWaiter = false;
    }
    if (!xrtEventUnit(&S->Wake)) return false;
    xrtFree(S->Heap);
    xrtFree(S->Jobs);
    xrtFree(S);
    return true;
}

#endif
