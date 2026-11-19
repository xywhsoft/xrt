#include <xcron.h>
#include <xrt/executor.h>
#include <stdio.h>

typedef struct application {
    xexecutor* Workers;
    unsigned Finished;
    bool Accepted;
} application;

static void work(ptr Context)
{
    application* App = Context;
    ++App->Finished;
}

static void submit(void* Context)
{
    application* App = Context;
    App->Accepted = xrtExecutorSubmit(App->Workers, work, App, NULL, NULL);
}

int main(void)
{
    /* The APP chooses worker count, queue limit, rejection and shutdown policy. */
    xexecutorconfig Config = {.Threads = 1, .QueueLimit = 2};
    application App = {.Workers = xrtExecutorCreate(&Config)};
    if (!App.Workers) return 1;
    xcronscheduler* Scheduler = NULL;
    xcronjobid Job;
    bool OK = xcronSchedulerCreate(1, XCRON_SECONDS6, &Scheduler)
        && xcronSchedulerAdd(Scheduler, XRT_STR_LITERAL("* * * * * *"), submit, &App, &Job)
        && xcronSchedulerWait(Scheduler, 3000, NULL) == XCRON_SCHEDULE_DISPATCHED
        && App.Accepted;
    if (Scheduler && !xcronSchedulerDestroy(Scheduler)) OK = false;
    bool Closed = xrtExecutorClose(App.Workers);
    xwaitresult Wait = xrtExecutorWaitFor(App.Workers, 3000);
    bool Destroyed = xrtExecutorDestroy(App.Workers);
    if (!OK || !Closed || Wait != XWAIT_OK || !Destroyed || App.Finished != 1) return 2;
    puts("APP worker completed one cron task");
    return 0;
}
