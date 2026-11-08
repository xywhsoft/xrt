/* Includes public Init/Step copies, validation and every host turn. */
#include <xcron.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int Argc,char** Argv)
{
    uint64 Count=Argc>1?strtoull(Argv[1],NULL,10):100000,Sum=0,Calls=0,Segments=0,Months=0,Years=0;
    cstr Mode=Argc>2?Argv[2]:"fold";uint64 Amount=Argc>3?strtoull(Argv[3],NULL,10):16;
    if(!Count || Count>1000000000 || !Amount || Amount>UINT32_MAX || (strcmp(Mode,"dense") && strcmp(Mode,"fold") && strcmp(Mode,"cycle")))return 1;
    bool Cycle=!strcmp(Mode,"cycle"),Fold=!strcmp(Mode,"fold");xcronexpression E;xcronzone* Z=NULL;
    cstr Text=Cycle?"0 30 2 1 MAR *":Fold?"0 30 1 * * *":"* * * * * *";
    cstr Rule=Cycle?"STD0DST,J60/2,J300/2":"EST5EDT,M3.2.0,M11.1.0";
    if(!xcronExpressionCompile((xstrview){Text,strlen(Text)},XCRON_SECONDS6,&E,NULL) ||
        !xcronZoneCreatePosix((xstrview){Rule,strlen(Rule)},XCRON_ZONE_TZIF3,&Z,NULL))return 2;
    xtime First,Last;bool OK=Fold?xrtDateTime(2024,11,3,5,45,0,0,&First):xrtDate(2024,1,1,&First);
    if(!OK){xcronZoneRelease(Z);return 3;}Last=Cycle?INT64_MAX:Fold?First+2*XRT_TIME_HOUR:First+XRT_TIME_DAY;
    xcronzonedbudget Budget={(uint32)Amount,(uint32)Amount,(uint32)Amount};double Start=xrtTimer();
    for(uint64 J=0;J<Count;++J){
        xtime Next=777,Begin=Cycle || Fold?First:First+(xtime)(J%86400)*1000;
        xcronzonedcursor Cursor;xcronzonedcursorissue I;xcronzonedstatus S;
        if(!xcronZonedCursorInit(&Cursor,&E,Z,Begin,Last,0,&I)){xcronZoneRelease(Z);return 4;}
        do{S=xcronZonedCursorStep(&Cursor,Z,Budget,&Next,&I);++Calls;}while(S==XCRON_ZONED_PENDING);
        if(S!=(Cycle?XCRON_ZONED_NONE:XCRON_ZONED_FOUND)){xcronZoneRelease(Z);return 5;}
        Sum+=(uint64)Next;Segments+=I.SegmentsExamined;Months+=I.MonthsExamined;Years+=I.RuleYearsExamined;
    }
    double Elapsed=xrtTimer()-Start;xcronZoneRelease(Z);if(!(Elapsed>0))return 6;
    printf("searches_per_sec: %.3f\nsteps_per_search: %.3f\nsegments_per_search: %.3f\nmonths_per_search: %.3f\nrule_years_per_search: %.3f\nchecksum: %" PRIu64 "\n",
        Count/Elapsed,(double)Calls/Count,(double)Segments/Count,(double)Months/Count,(double)Years/Count,Sum);return 0;
}
