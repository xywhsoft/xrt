/* Whole-corpus named import, then state queries after root/manager destruction. */
#include <xcron.h>
#include <xrt/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
int main(int Argc,char** Argv)
{
    if(Argc!=4)return 1;
    FILE* F=fopen(Argv[2],"rb");if(!F)return 2;
    if(fseek(F,0,SEEK_END)){fclose(F);return 2;}long Length=ftell(F);
    if(Length<0 || Length>32*1024*1024 || fseek(F,0,SEEK_SET)){fclose(F);return 2;}
    char* Text=xrtMalloc((size_t)Length+1);if(!Text){fclose(F);return 2;}bool Read=fread(Text,1,(size_t)Length,F)==(size_t)Length;fclose(F);if(!Read){xrtFree(Text);return 2;}Text[Length]=0;
    size_t Capacity=1;for(long I=0;I<Length;++I)Capacity+=Text[I]=='\n';if(Capacity>XCRON_TZDB_NAMES_MAX){xrtFree(Text);return 2;}
    xstrview* Names=xrtMalloc(Capacity*sizeof(*Names));if(!Names){xrtFree(Text);return 2;}size_t Count=0,At=0;
    for(size_t I=0;I<=(size_t)Length;++I)if(!Text[I] || Text[I]=='\n'){size_t End=I;if(End>At && Text[End-1]=='\r')--End;Text[End]=0;if(End>At)Names[Count++]=(xstrview){Text+At,End-At};At=I+1;}
    xroot Root=xrtRootOpen(Argv[1]);xcrontzdb* DB=NULL;xcrontzdbsnapshot* S=NULL;xcrontzdbconfig C={XCRON_TZDB_NAMES_MAX,1,128*1024*1024};uint64 R=0;xcrontzdbissue I={0};
    bool OK=Root && xcronTzdbCreate(&C,&DB,&I) && xcronTzdbLoadRoot(DB,Root,(xstrview){Argv[3],strlen(Argv[3])},Names,Count,0,&R,&I) && xcronTzdbAcquire(DB,&S,&I);
    xrtFree(Names);memset(Text,0,(size_t)Length);xrtFree(Text);
    if(DB && (!xcronTzdbClose(DB,NULL) || !xcronTzdbDestroy(DB,NULL)))OK=false;
    if(Root && !xrtRootClose(Root))OK=false;
    if(!OK){fprintf(stderr,"database issue=%d input=%zu zone=%d\n",I.Code,I.Index,I.Zone.Code);xcronTzdbSnapshotRelease(S);return 3;}
    xcrontzdbsnapshotinfo Info;if(!xcronTzdbSnapshotInfo(S,&Info,NULL)){xcronTzdbSnapshotRelease(S);return 3;}
    printf("READY %zu %zu %llu\n",Info.Names,Info.Bytes,(unsigned long long)Info.Revision);
    char Line[384],Name[256];xtime When;
    while(fgets(Line,sizeof(Line),stdin)){
        if(sscanf(Line,"%255s %" SCNd64,Name,&When)!=2){xcronTzdbSnapshotRelease(S);return 4;}
        xcronzone* Z=NULL;xcrontzdbstatus Found=xcronTzdbFind(S,(xstrview){Name,strlen(Name)},&Z,NULL);
        if(Found==XCRON_TZDB_NONE){puts("-2 123 0");continue;}if(Found!=XCRON_TZDB_FOUND){xcronTzdbSnapshotRelease(S);return 4;}
        xcronzonestate State={.Offset=123};xcronzoneissue ZoneIssue;bool Known=xcronZoneAt(Z,When,&State,&ZoneIssue);
        if(!Known && (ZoneIssue.Code!=XCRON_ZONE_UNKNOWN || State.Offset!=123)){xcronZoneRelease(Z);xcronTzdbSnapshotRelease(S);return 5;}
        printf("%d %d %d\n",Known?1:-1,State.Offset,State.IsDST?1:0);xcronZoneRelease(Z);xrtClearError();
    }
    xcronTzdbSnapshotRelease(S);return 0;
}
