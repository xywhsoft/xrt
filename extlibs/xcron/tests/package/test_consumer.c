#include <xcron.h>
#include <stdio.h>

#if defined(XCRON_FEATURE_XCRON_SCHEDULER)
static void tick(void* Context)
{
    ++*(unsigned*)Context;
}
#endif

int main(void)
{
    xcronexpression Rule;
    xtime Next;
    if (!xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"), XCRON_SECONDS6, &Rule, NULL) ||
        xcronCalendarNext(&Rule, 0, 5000, XCRON_SEARCH_EXCLUSIVE, &Next, NULL) != XCRON_CALENDAR_FOUND ||
        Next != 1000)
        return 1;
#if defined(XCRON_FEATURE_XCRON_SCHEDULER)
    xcronscheduler* Scheduler = NULL;
    unsigned Count = 0;
    xcronjobid Job;
    if (!xcronSchedulerCreate(1, XCRON_SECONDS6, &Scheduler)) return 2;
    xcronpoll Poll;
    bool OK = xcronSchedulerPoll(Scheduler, &Poll) == XCRON_SCHEDULE_IDLE &&
        Poll.Count == 0 && !Poll.HasNext &&
        xcronSchedulerAdd(Scheduler, XRT_STR_LITERAL("* * * * * *"), tick, &Count, &Job) &&
        xcronSchedulerWait(Scheduler, 3000, NULL) == XCRON_SCHEDULE_DISPATCHED &&
        Count == 1 && xcronSchedulerRemove(Scheduler, Job);
    if (!xcronSchedulerDestroy(Scheduler) || !OK) return 3;
#endif
    puts("xcron calendar package consumer passed");
    return 0;
}
