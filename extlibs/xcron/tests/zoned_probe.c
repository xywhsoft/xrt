/* Public-query batch probe: legacy byte loading or actual named-root loading. */
#include <xcron.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(XCRON_FEATURE_XCRON_TZDB_FILE)
static bool zonedProbeNamed(cstr Path,cstr Name,xcronzone** Zone)
{
    xroot Root=xrtRootOpen(Path);xcrontzdb* D=NULL;xcrontzdbsnapshot* S=NULL;xcrontzdbconfig C={1,1,64*1024*1024};
    xstrview Key={Name,strlen(Name)};uint64 Revision=0;
    bool OK=Root && xcronTzdbCreate(&C,&D,NULL) && xcronTzdbLoadRoot(D,Root,XRT_STR_LITERAL("oracle"),&Key,1,0,&Revision,NULL) &&
        xcronTzdbAcquire(D,&S,NULL) && xcronTzdbFind(S,Key,Zone,NULL)==XCRON_TZDB_FOUND;
    xcronTzdbSnapshotRelease(S);
    if(D && (!xcronTzdbClose(D,NULL) || !xcronTzdbDestroy(D,NULL)))OK=false;
    if(Root && !xrtRootClose(Root))OK=false;
    if(!OK){xcronZoneRelease(*Zone);*Zone=NULL;}
    return OK;
}
#endif
int main(int Argc,char** Argv)
{
    if(Argc!=5 && Argc!=6)return 1;
    xcronzone* Z=NULL;xcronzoneissue ZI={0};int DialectAt=3;
    if(Argc==5 && !strcmp(Argv[1],"--tzif")){
        FILE* F=fopen(Argv[2],"rb");if(!F)return 2;
        if(fseek(F,0,SEEK_END)){fclose(F);return 2;}long Size=ftell(F);
        if(Size<0 || (unsigned long)Size>XCRON_ZONE_TZIF_MAX || fseek(F,0,SEEK_SET)){fclose(F);return 2;}
        uint8* Data=malloc((size_t)Size+1);if(!Data){fclose(F);return 2;}
        bool Read=fread(Data,1,(size_t)Size,F)==(size_t)Size;fclose(F);
        bool OK=Read && xcronZoneCreateTzif((xbytesview){Data,(size_t)Size},&Z,&ZI);free(Data);
        if(!OK){fprintf(stderr,"zone issue %d at %zu\n",ZI.Code,ZI.Offset);return 3;}
    }else if(Argc==5 && !strcmp(Argv[1],"--posix")){
        if(!xcronZoneCreatePosix((xstrview){Argv[2],strlen(Argv[2])},XCRON_ZONE_TZIF3,&Z,&ZI))return 3;
#if defined(XCRON_FEATURE_XCRON_TZDB_FILE)
    }else if(Argc==6 && !strcmp(Argv[1],"--database")){
        if(!zonedProbeNamed(Argv[2],Argv[3],&Z))return 3;
        DialectAt=4;
#endif
    }else return 1;
    xcrondialect Dialect=!strcmp(Argv[DialectAt],"5")?XCRON_UNIX5:!strcmp(Argv[DialectAt],"6")?XCRON_SECONDS6:0;xcronexpression E;
    if(!xcronExpressionCompile((xstrview){Argv[DialectAt+1],strlen(Argv[DialectAt+1])},Dialect,&E,NULL)){xcronZoneRelease(Z);return 4;}
    char Line[256];
    while(fgets(Line,sizeof(Line),stdin)){
        xcronzonedissue I={XCRON_ZONED_RANGE,XCRON_FIELD_NONE,XCRON_ZONE_OK,0,0};xtime Begin,End,Next=777;unsigned Flags;int Status;
        if(Line[0]=='M'){
            if(sscanf(Line+1,"%" SCNd64,&Begin)!=1){xcronZoneRelease(Z);return 5;}
            bool Value=true;bool OK=xcronZonedMatch(&E,Z,Begin,&Value,&I);Status=OK?1:-1;Next=Value?1:0;
        }else if(Line[0]=='N'){
            if(sscanf(Line+1,"%" SCNd64 " %" SCNd64 " %u",&Begin,&End,&Flags)!=3){xcronZoneRelease(Z);return 5;}
#if defined(XCRON_PROBE_CURSOR)
            xcronzonedcursor C;xcronzonedcursorissue CI={.Code=XCRON_ZONED_RANGE,.Field=XCRON_FIELD_NONE};unsigned Calls=0;
            if(!xcronZonedCursorInit(&C,&E,Z,Begin,End,Flags,&CI))Status=XCRON_ZONED_ERROR;
            else do{
                xcronzonedcursorissue Previous=CI;xcronzonedbudget B={1+Calls%3,1+Calls%7,1+Calls%2};
                Status=xcronZonedCursorStep(&C,Z,B,&Next,&CI);++Calls;
                if(Calls>100000 || CI.SegmentsExamined-Previous.SegmentsExamined>B.Segments ||
                    CI.MonthsExamined-Previous.MonthsExamined>B.Months || CI.RuleYearsExamined-Previous.RuleYearsExamined>B.RuleYears){xcronZoneRelease(Z);return 6;}
            }while(Status==XCRON_ZONED_PENDING);
            I=(xcronzonedissue){CI.Code,CI.Field,CI.Zone,CI.SegmentsExamined,CI.MonthsExamined};
#else
            Status=xcronZonedNext(&E,Z,Begin,End,Flags,&Next,&I);
#endif
        }else {xcronZoneRelease(Z);return 5;}
        printf("%d %" PRId64 " %d %d %d %u %" PRIu64 "\n",Status,Next,I.Code,I.Field,I.Zone,I.SegmentsExamined,I.MonthsExamined);xrtClearError();
    }
    xcronZoneRelease(Z);return 0;
}
