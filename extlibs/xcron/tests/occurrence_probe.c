/* Application byte-loading probe: all sync/optional-Issue/cursor paths are
 * physically executed and compared; zone-owned input copy outlives retired bytes. */
#include <xcron.h>
#include <xrt/error.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int Argc,char** Argv)
{
    if(Argc!=2)return 1;
    FILE* F=fopen(Argv[1],"rb");if(!F)return 2;
    if(fseek(F,0,SEEK_END)){fclose(F);return 2;}long Size=ftell(F);
    if(Size<0 || (unsigned long)Size>XCRON_ZONE_TZIF_MAX || fseek(F,0,SEEK_SET)){fclose(F);return 2;}
    uint8* Raw=malloc((size_t)Size+1);if(!Raw){fclose(F);return 2;}bool Read=fread(Raw,1,(size_t)Size,F)==(size_t)Size;fclose(F);xcronzone* Z=NULL;
    bool OK=Read && xcronZoneCreateTzif((xbytesview){Raw,(size_t)Size},&Z,NULL);memset(Raw,0,(size_t)Size);free(Raw);if(!OK)return 3;
    cstr Text[]={"30 1 * * *","30 2 * * *","45 1 * * *","0 0 9-17 * * MON-FRI"};xcronexpression E[4];
    for(unsigned J=0;J<4;++J)if(!xcronExpressionCompile((xstrview){Text[J],strlen(Text[J])},J==3?XCRON_SECONDS6:XCRON_UNIX5,&E[J],NULL)){xcronZoneRelease(Z);return 4;}
    puts("selected_occurrence=2 sync_pair_cursor=3");char Line[256];
    while(fgets(Line,sizeof(Line),stdin)){xtime Begin,End;unsigned Flags,Fold,Missing,Profile;
        if(sscanf(Line,"%" SCNd64 " %" SCNd64 " %u %u %u %u",&Begin,&End,&Flags,&Fold,&Missing,&Profile)!=6 || Fold>3 || Missing>1 || Profile>3){xcronZoneRelease(Z);return 5;}
        xcrondeliverypolicy P={(xcronfoldpolicy)Fold,(xcronmissingpolicy)Missing};xcronoccurrence A,B,Empty;memset(&Empty,0x5a,sizeof(Empty));memcpy(&A,&Empty,sizeof(A));memcpy(&B,&Empty,sizeof(B));xcronoccurrenceissue I;
        xcronoccurrencestatus S=xcronOccurrenceNext(&E[Profile],Z,Begin,End,Flags,P,&A,&I),Other=xcronOccurrenceNext(&E[Profile],Z,Begin,End,Flags,P,&B,NULL);
        if(Other!=S || memcmp(&A,&B,sizeof(A)) || (S==XCRON_OCCURRENCE_ERROR && (xrtErrorCode(xrtGetError())!=(int32)I.Code || strcmp(xrtErrorDomain(xrtGetError()),"xrt.cron.occurrence")))){xcronZoneRelease(Z);return 6;}
        xcronoccurrencecursor C;xcronoccurrenceissue CI;unsigned Calls=0;if(!xcronOccurrenceCursorInit(&C,&E[Profile],Z,Begin,End,Flags,P,&CI)){xcronZoneRelease(Z);return 7;}memcpy(&B,&Empty,sizeof(B));
        do {xcronoccurrenceissue Before=CI;xcronoccurrencebudget Budget={1+Calls%2,1+Calls%3,1+Calls%2,1+Calls%3,1+Calls%5};Other=xcronOccurrenceCursorStep(&C,Z,Budget,&B,&CI);++Calls;
            if(Calls>100000 || CI.SegmentsExamined-Before.SegmentsExamined>Budget.Segments || CI.MonthsExamined-Before.MonthsExamined>Budget.Months || CI.RuleYearsExamined-Before.RuleYearsExamined>Budget.RuleYears || CI.ChecksExamined-Before.ChecksExamined>Budget.Checks || CI.OffsetsExamined-Before.OffsetsExamined>Budget.Offsets ||
               (Other==XCRON_OCCURRENCE_PENDING && (memcmp(&B,&Empty,sizeof(B)) || CI.Candidates || CI.When || CI.Local.Year || CI.Code))){xcronZoneRelease(Z);return 8;}
        }while(Other==XCRON_OCCURRENCE_PENDING);
        if(Other!=S || memcmp(&A,&B,sizeof(A)) || CI.Code!=I.Code || CI.Field!=I.Field || CI.Zone!=I.Zone || CI.When!=I.When || CI.Candidates!=I.Candidates || memcmp(&CI.Local,&I.Local,sizeof(I.Local))){xcronZoneRelease(Z);return 9;}
        printf("%d %d %d %" PRId64 " %zu %" PRId64 " %d %d %d %d %d %d %d %u %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n",S,I.Code,I.Zone,I.When,I.Candidates,I.Local.Year,I.Local.Month,I.Local.Day,I.Local.Hour,I.Local.Minute,I.Local.Second,I.Local.Weekday,I.Local.YearDay,Calls,CI.SegmentsExamined,CI.MonthsExamined,CI.RuleYearsExamined,CI.ChecksExamined,CI.OffsetsExamined,CI.LabelsExamined);xrtClearError();
    }xcronZoneRelease(Z);return ferror(stdin)?10:0;
}
