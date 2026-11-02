/* Independent explicit rules; no host TZ, installed database or external IO. */
#include <xcron.h>
#include <stdio.h>
#include <stdlib.h>
static void require(bool B){if(!B){fprintf(stderr,"[FAIL] %s\n",xrtErrorMessage(xrtGetError()));exit(1);}}
int main(void)
{
    xcronzone* Z=NULL;require(xcronZoneCreatePosix(XRT_STR_LITERAL("EST5EDT,M3.2.0,M11.1.0"),XCRON_ZONE_TZIF3,&Z,NULL));
    xdatetime Local={.Year=2024,.Month=11,.Day=3,.Hour=1,.Minute=30};xtime Times[2],A,B;size_t Count=0;
    require(xcronZoneResolve(Z,&Local,Times,2,&Count,NULL)==XCRON_ZONE_FOUND && Count==2 && xrtDateTime(2024,11,3,5,30,0,0,&A) && xrtDateTime(2024,11,3,6,30,0,0,&B) && Times[0]==A && Times[1]==B);
    Local.Month=3;Local.Day=10;Local.Hour=2;require(xcronZoneResolve(Z,&Local,Times,2,&Count,NULL)==XCRON_ZONE_NONE && !Count);
    xcronZoneRelease(Z);puts("[PASS] explicit zone: fold has two instants, gap has none, owner released");return 0;
}
