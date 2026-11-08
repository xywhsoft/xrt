#include <xcron.h>
#include <stdio.h>
int main(void)
{
    xcronzone* Z=NULL;xcronscheduler* S=NULL;xcronschedulerconfig C={1,1,8,0,65536};xcronjobconfig J={.RunningLimit=1};xcronjobid Id;xcronrunid Active=0;int Exit=1;
    if(!xcronZoneCreatePosix(XRT_STR_LITERAL("EST5EDT,M3.2.0,M11.1.0"),XCRON_ZONE_TZIF3,&Z,NULL) || !xcronExpressionCompile(XRT_STR_LITERAL("0 30 1 * * *"),XCRON_SECONDS6,&J.Expression,NULL) || !xrtDateTime(2024,11,3,5,30,0,0,&J.Begin))goto Done;
    J.End=J.Begin+XRT_TIME_HOUR;J.Zone=Z;
    if(!xcronSchedulerCreateSelected(&C,&S,NULL) || !xcronSchedulerAddSelected(S,&J,(xcrondeliverypolicy){XCRON_FOLD_LAST,XCRON_MISSING_REJECT},&Id,NULL))goto Done;
    xcronselectedjobinfo Info;if(!xcronSchedulerJobSelected(S,Id,&Info,NULL) || Info.State!=XCRON_JOB_SEARCHING)goto Done;
    for(unsigned T=0;T<128;++T){xcronselectedrun R;xcronselectedpoll P;
        if(!xcronSchedulerPollSelected(S,(xcronclock){J.End,T},(xcronoccurrencebudget){1,1,1,1,1},&R,1,&P,NULL))goto Done;
        if(P.Poll.Count){Active=R.Run.Id;if(R.Run.ScheduledAt!=J.End || R.Candidates!=2 || !xcronSchedulerComplete(S,Active,XCRON_RUN_SUCCEEDED,NULL))goto Done;Active=0;Exit=0;puts("Selected LAST fold dispatched with complete nominal metadata.");break;}
    }
Done:
    if(S){xcronSchedulerClose(S,NULL);if(Active)xcronSchedulerComplete(S,Active,XCRON_RUN_CANCELLED,NULL);xcronselectedretired R;bool Found;do{Found=false;xcronSchedulerReapSelected(S,&R,&Found,NULL);}while(Found);if(!xcronSchedulerDestroy(S,NULL))Exit=2;}
    xcronZoneRelease(Z);return Exit;
}
