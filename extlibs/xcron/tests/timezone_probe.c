/* Line-oriented public-API probe for the independent Python zoneinfo oracle. */
#include <xcron.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
int main(int Argc,char** Argv)
{
    if(Argc!=2)return 1;
    xcronzone* Z=NULL;
    if(!xcronZoneCreatePosix((xstrview){Argv[1],strlen(Argv[1])},XCRON_ZONE_TZIF3,&Z,NULL))return 2;
    char Line[128];
    while(fgets(Line,sizeof(Line),stdin)){
        xtime T,V[2];xdatetime D;xcronzonestate S;xcronzonetransition N;size_t Count;
        if(sscanf(Line,"%" SCNd64,&T)!=1 || !xcronZoneAt(Z,T,&S,NULL) || !xcronZoneSplit(Z,T,&D,NULL) ||
           xcronZoneResolve(Z,&D,V,2,&Count,NULL)!=XCRON_ZONE_FOUND)return 3;
        xcronzonestatus Status=xcronZoneNextTransition(Z,T,&N,NULL);
        if(Status==XCRON_ZONE_ERROR)return 4;
        printf("%d %d %" PRId64 " %d %d %d %d %d %d %.*s %zu %" PRId64 " %" PRId64 " %d %" PRId64 "\n",
            S.Offset,S.IsDST,D.Year,D.Month,D.Day,D.Hour,D.Minute,D.Second,D.Millisecond,
            (int)S.Abbreviation.Size,S.Abbreviation.Data,Count,V[0],Count==2?V[1]:V[0],Status,Status==XCRON_ZONE_FOUND?N.When:0);
    }
    xcronZoneRelease(Z);return 0;
}
