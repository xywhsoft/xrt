/* Deterministic actual-occurrence search; no clock/service/database dependency. */
#include <xcron.h>
#include <stdio.h>
int main(void)
{
    xcronexpression E;xcronzone* Z=NULL;xcronzonedissue I={0};xtime Begin,End,Expected,Next=777;
    bool OK=xcronExpressionCompile(XRT_STR_LITERAL("30 1 * * *"),XCRON_UNIX5,&E,NULL) &&
        xcronZoneCreatePosix(XRT_STR_LITERAL("EST5EDT,M3.2.0,M11.1.0"),XCRON_ZONE_TZIF3,&Z,NULL);
    if(OK)OK=xrtDateTime(2024,11,3,5,45,0,0,&Begin) && xrtDateTime(2024,11,3,7,0,0,0,&End) && xrtDateTime(2024,11,3,6,30,0,0,&Expected) &&
        xcronZonedNext(&E,Z,Begin,End,0,&Next,&I)==XCRON_ZONED_FOUND && Next==Expected;
    if(OK)OK=xcronExpressionCompile(XRT_STR_LITERAL("30 2 * * *"),XCRON_UNIX5,&E,NULL) && xrtDate(2024,3,10,&Begin) &&
        xcronZonedNext(&E,Z,Begin,Begin+XRT_TIME_DAY-1,0,&Next,&I)==XCRON_ZONED_NONE && Next==Expected;
    xcronZoneRelease(Z);
    if(!OK){fprintf(stderr,"zoned example failed, issue=%d zone=%d\n",I.Code,I.Zone);xrtClearError();return 1;}
    puts("[PASS] zoned cron: later fold at 06:30 UTC, missing 02:30 has no occurrence, owner released");return 0;
}
