/* Line-oriented public-API probe for the independent Python zoneinfo oracle. */
#include <xcron.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int Argc,char** Argv)
{
    xcronzone* Z=NULL;
    if(Argc==3 && !strcmp(Argv[1],"--tzif")){
        FILE* F=fopen(Argv[2],"rb");if(!F)return 1;
        if(fseek(F,0,SEEK_END)){fclose(F);return 1;}long Size=ftell(F);
        if(Size<0 || (unsigned long)Size>XCRON_ZONE_TZIF_MAX || fseek(F,0,SEEK_SET)){fclose(F);return 1;}
        uint8* Data=malloc((size_t)Size+1);if(!Data){fclose(F);return 1;}
        bool Read=fread(Data,1,(size_t)Size,F)==(size_t)Size;fclose(F);xcronzoneissue I={0};
        bool OK=Read && xcronZoneCreateTzif((xbytesview){Data,(size_t)Size},&Z,&I);free(Data);
        if(!OK){fprintf(stderr,"TZif error %d at %zu\n",I.Code,I.Offset);return 2;}
    }else if(Argc==2){if(!xcronZoneCreatePosix((xstrview){Argv[1],strlen(Argv[1])},XCRON_ZONE_TZIF3,&Z,NULL))return 2;}
    else return 1;
    char Line[128];
    while(fgets(Line,sizeof(Line),stdin)){
        xtime T,V[258];xdatetime D;xcronzonestate S;xcronzonetransition N;size_t Count;xcronzoneissue I;
        if(sscanf(Line,"%" SCNd64,&T)!=1){xcronZoneRelease(Z);return 3;}
        if(!xcronZoneAt(Z,T,&S,&I)){
            if(I.Code==XCRON_ZONE_UNKNOWN){puts("UNKNOWN");xrtClearError();continue;}
            xcronZoneRelease(Z);return 3;
        }
        if(!xcronZoneSplit(Z,T,&D,NULL)){xcronZoneRelease(Z);return 3;}
        if(xcronZoneResolve(Z,&D,V,258,&Count,&I)!=XCRON_ZONE_FOUND){
            if(I.Code==XCRON_ZONE_UNKNOWN){puts("PARTIAL");xrtClearError();continue;}
            xcronZoneRelease(Z);return 3;
        }
        xcronzonestatus Status=xcronZoneNextTransition(Z,T,&N,NULL);
        if(Status==XCRON_ZONE_ERROR){xcronZoneRelease(Z);return 4;}
        printf("%d %d %" PRId64 " %d %d %d %d %d %d %.*s %zu",
            S.Offset,S.IsDST,D.Year,D.Month,D.Day,D.Hour,D.Minute,D.Second,D.Millisecond,
            S.Abbreviation.Size?(int)S.Abbreviation.Size:1,S.Abbreviation.Size?S.Abbreviation.Data:"_",Count);
        for(size_t K=0;K<Count;++K)printf(" %" PRId64,V[K]);
        printf(" %d %" PRId64 "\n",Status,Status==XCRON_ZONE_FOUND?N.When:0);
    }
    xcronZoneRelease(Z);return 0;
}
