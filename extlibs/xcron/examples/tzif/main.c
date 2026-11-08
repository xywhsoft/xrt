/* Applications supply trusted/pinned TZif bytes; the extension performs no IO. */
#include <xcron.h>
#include <xrt/memory.h>
#include <stdio.h>
#include <stdlib.h>
int main(int Argc,char** Argv)
{
    static const uint8 UTC[54]={[0]='T',[1]='Z',[2]='i',[3]='f',[39]=1,[43]=4,[50]='U',[51]='T',[52]='C'};
    uint8* Owned=NULL;xbytesview Data={UTC,sizeof(UTC)};
    if(Argc==2){FILE* F=fopen(Argv[1],"rb");if(!F)return 1;
        if(fseek(F,0,SEEK_END)){fclose(F);return 1;}long Size=ftell(F);
        if(Size<0 || (unsigned long)Size>XCRON_ZONE_TZIF_MAX || fseek(F,0,SEEK_SET)){fclose(F);return 1;}
        Owned=xrtMalloc((size_t)Size+1);if(!Owned){fclose(F);return 1;}
        bool Read=fread(Owned,1,(size_t)Size,F)==(size_t)Size;fclose(F);
        if(!Read){xrtFree(Owned);return 1;}Data=(xbytesview){Owned,(size_t)Size};
    }else if(Argc!=1)return 1;
    xcronzone* Zone=NULL;xcronzoneissue Issue={0};bool OK=xcronZoneCreateTzif(Data,&Zone,&Issue);xrtFree(Owned);
    if(!OK){fprintf(stderr,"TZif issue=%d byte=%zu\n",Issue.Code,Issue.Offset);xrtClearError();return 2;}
    xcronzonestate State;xdatetime Date;xtime Candidates[258];size_t Count=0;
    OK=xcronZoneAt(Zone,XRT_TIME_UNIX_EPOCH,&State,&Issue) && xcronZoneSplit(Zone,XRT_TIME_UNIX_EPOCH,&Date,&Issue) &&
       xcronZoneResolve(Zone,&Date,Candidates,258,&Count,&Issue)==XCRON_ZONE_FOUND;
    if(OK && Argc==1)OK=State.Offset==0 && Date.Year==1970 && Date.Month==1 && Date.Day==1 && Count==1 && Candidates[0]==XRT_TIME_UNIX_EPOCH;
    if(OK)printf("[PASS] TZif owned bytes: %.*s offset=%d candidates=%zu\n",(int)State.Abbreviation.Size,State.Abbreviation.Data,State.Offset,Count);
    else fprintf(stderr,"query issue=%d (unknown data is not a gap)\n",Issue.Code);
    xcronZoneRelease(Zone);xrtClearError();return OK?0:3;
}
