/* Explicit-clock, single-owner scheduler kernel; no background worker needed.
 * Run from the repository root:
 * python tools/build.py --compiler gcc --manifest extlibs/xcron/config/modules.json --suite xcron_scheduler --no-single --cflag=-O2
 * Expect: one coalesced [0,3500] lease; cancellation cannot reclaim Data until
 * Complete; then exactly one retirement receipt. Cleanup releases every zone,
 * job and scheduler. A real executor/timer adapter is a separate layer. */
#include <xcron.h>
#include <inttypes.h>
#include <stdio.h>
int main(void)
{
    xcronzone* Zone=NULL;xcronscheduler* Scheduler=NULL;xcronrun Run={0};int Data=42,Exit=1;bool Found=false;
    xcronschedulerconfig Config={1,1,16,0,65536};xcronjobconfig Job={0};xcronjobid Id;
    if(!xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_TZIF3,&Zone,NULL) ||
        !xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&Job.Expression,NULL) ||
        !xcronSchedulerCreate(&Config,&Scheduler,NULL))goto Done;
    Job.Zone=Zone;Job.Begin=0;Job.End=5000;Job.Misfire=XCRON_MISFIRE_COALESCE;Job.GraceMilliseconds=100;Job.RunningLimit=1;Job.Data=&Data;
    if(!xcronSchedulerAdd(Scheduler,&Job,&Id,NULL))goto Done;
    /* Scheduler retained Zone and copied the expression; caller can release. */
    xcronZoneRelease(Zone);Zone=NULL;
    xcronpoll Poll;if(!xcronSchedulerPoll(Scheduler,(xcronclock){3500,100},&Run,1,&Poll,NULL) || Poll.Count!=1 ||
        Run.ScheduledAt!=0 || Run.Through!=3500 || Run.Flags!=XCRON_RUN_COALESCED || Run.Data!=&Data)goto Done;
    if(!xcronSchedulerCancel(Scheduler,Id,NULL))goto Done;
    xcronretired Receipt;if(!xcronSchedulerReap(Scheduler,&Receipt,&Found,NULL) || Found)goto Done;
    printf("coalesced lease: [0,%" PRId64 "]; data remains live after cancel\n",Run.Through);
    /* Physical work, including any data-using tail, has now finished. */
    if(!xcronSchedulerComplete(Scheduler,Run.Id,XCRON_RUN_SUCCEEDED,NULL))goto Done;
    Run.Id=0;
    if(!xcronSchedulerReap(Scheduler,&Receipt,&Found,NULL) || !Found || Receipt.Id!=Id || Receipt.Data!=&Data || Receipt.Info.State!=XCRON_JOB_CANCELLED)goto Done;
    puts("one retirement receipt; data may now be destroyed");Exit=0;
Done:
    if(Scheduler){xcronSchedulerClose(Scheduler,NULL);if(Run.Id)xcronSchedulerComplete(Scheduler,Run.Id,XCRON_RUN_CANCELLED,NULL);
        xcronretired Receipt;do{Found=false;xcronSchedulerReap(Scheduler,&Receipt,&Found,NULL);}while(Found);
        if(!xcronSchedulerDestroy(Scheduler,NULL))Exit=1;}
    xcronZoneRelease(Zone);xrtClearError();return Exit;
}
