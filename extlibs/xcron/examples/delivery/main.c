#include <xcron.h>
#include <stdio.h>
int main(void)
{
    xcronzone* Z=NULL;int Exit=1;
    if(!xcronZoneCreatePosix(XRT_STR_LITERAL("EST5EDT,M3.2.0,M11.1.0"),XCRON_ZONE_TZIF3,&Z,NULL))return 1;
    xdatetime D={.Year=2024,.Month=11,.Day=3,.Hour=1,.Minute=30};xtime First,Last;size_t Count;xcrondeliveryissue I;
    if(xcronDeliveryResolve(Z,&D,(xcrondeliverypolicy){XCRON_FOLD_FIRST,XCRON_MISSING_SKIP},&First,1,&Count,&I)!=XCRON_DELIVERY_FOUND || Count!=1 || I.Candidates!=2)goto Done;
    if(xcronDeliveryResolve(Z,&D,(xcrondeliverypolicy){XCRON_FOLD_LAST,XCRON_MISSING_SKIP},&Last,1,&Count,&I)!=XCRON_DELIVERY_FOUND || Last-First!=3600000)goto Done;
    D.Month=3;D.Day=10;D.Hour=2;
    if(xcronDeliveryResolve(Z,&D,(xcrondeliverypolicy){XCRON_FOLD_ALL,XCRON_MISSING_SKIP},NULL,0,&Count,&I)!=XCRON_DELIVERY_NONE || Count)goto Done;
    puts("Selected first/last repeated label one hour apart; missing label skipped.");Exit=0;
Done:
    xcronZoneRelease(Z);return Exit;
}
