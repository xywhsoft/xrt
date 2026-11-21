#include <xcron.h>
#include <xrt/sync.h>
#include <xrt/thread.h>
#include <stdio.h>

enum { WAIT_MS = 5000 };

typedef struct application {
	xcronscheduler* Scheduler;
	xevent TaskRan;
	unsigned Calls;
	bool Notified;
} application;

static int32 request(ptr Context)
{
	application* App = Context;
	/* Request one registration, then wake the owner again after it runs. */
	if (!xcronSchedulerWake(App->Scheduler) || xrtEventWaitFor(&App->TaskRan, WAIT_MS) != XWAIT_OK)
		return 1;
	return xcronSchedulerWake(App->Scheduler) ? 0 : 1;
}

static void tick(void* Context)
{
	application* App = Context;
	++App->Calls;
	App->Notified = xrtEventSet(&App->TaskRan);
}

static bool drive(application* App)
{
	xcronscheduler* Scheduler = App->Scheduler;
	xcronjobid Job;
	/* The APP owner applies the request and runs the callback on this thread. */
	return xcronSchedulerWait(Scheduler, WAIT_MS, NULL) == XCRON_SCHEDULE_WOKEN &&
		xcronSchedulerAdd(Scheduler, XRT_STR_LITERAL("* * * * * *"), tick, App, &Job) &&
		xcronSchedulerWait(Scheduler, WAIT_MS, NULL) == XCRON_SCHEDULE_DISPATCHED &&
		App->Calls == 1 && App->Notified && xcronSchedulerRemove(Scheduler, Job) &&
		xcronSchedulerWait(Scheduler, WAIT_MS, NULL) == XCRON_SCHEDULE_WOKEN;
}

int main(void)
{
	application App = {0};
	if (!xrtEventInit(&App.TaskRan, false, false)) return 1;
	if (!xcronSchedulerCreate(1, XCRON_SECONDS6, &App.Scheduler)) {
		(void)xrtEventUnit(&App.TaskRan);
		return 1;
	}
	xthread* Thread = xrtThreadCreate(request, &App, 0);
	bool OK = Thread && drive(&App);
	if (Thread) {
		/* Retire all Wake callers before releasing the scheduler handle. */
		if (xrtThreadWait(Thread) != XWAIT_OK) return 2;
		OK = xrtThreadExitCode(Thread) == 0 && OK;
		xrtThreadDestroy(Thread);
	}
	bool Destroyed = xcronSchedulerDestroy(App.Scheduler);
	bool Released = xrtEventUnit(&App.TaskRan);
	if (!OK || !Destroyed || !Released) return 2;
	puts("owner registered, dispatched and retired one requested task");
	return 0;
}
