#include <xcron.h>
#include <stdio.h>

static void tick(void* Context)
{
    unsigned* Count = Context;
    ++*Count;
}

int main(void)
{
    xcronscheduler* Scheduler = NULL;
    if (!xcronSchedulerCreate(2, XCRON_SECONDS6, &Scheduler)) return 1;
    unsigned Polled = 0, Waited = 0;
    xcronjobid Job;
    bool OK = xcronSchedulerAdd(Scheduler, XRT_STR_LITERAL("* * * * * *"), tick, &Polled, &Job);

    /* An APP main loop can poll each turn while it also handles other work. */
    double Until = xrtTimer() + 3.0;
    while (OK && !Polled && xrtTimer() < Until) {
        OK = xcronSchedulerPoll(Scheduler, NULL) != XCRON_SCHEDULE_ERROR;
        if (!Polled) xrtSleep(10);
    }
    OK = OK && Polled == 1 && xcronSchedulerRemove(Scheduler, Job);

    /* An APP thread that has no other work can sleep until the next deadline. */
    OK = OK && xcronSchedulerAdd(Scheduler, XRT_STR_LITERAL("* * * * * *"), tick, &Waited, &Job)
        && xcronSchedulerWait(Scheduler, 3000, NULL) == XCRON_SCHEDULE_DISPATCHED && Waited == 1;
    if (!xcronSchedulerDestroy(Scheduler)) return 2;
    if (!OK) return 3;
    printf("poll=%u wait=%u\n", Polled, Waited);
    return 0;
}
