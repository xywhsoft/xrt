/* Application-owned file input; production selection itself has no IO. */
#include <xcron.h>
#include <xrt/error.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int Argc,char** Argv)
{
    if(Argc!=2 && (Argc!=3 || strcmp(Argv[2],"--cursor")))return 1;
    bool CursorMode=Argc==3;
    FILE* F=fopen(Argv[1],"rb");if(!F)return 2;
    if(fseek(F,0,SEEK_END)){fclose(F);return 2;}long Size=ftell(F);
    if(Size<0 || (unsigned long)Size>XCRON_ZONE_TZIF_MAX || fseek(F,0,SEEK_SET)){fclose(F);return 2;}
    uint8* Raw=malloc(Size?(size_t)Size:1);if(!Raw){fclose(F);return 2;}
    bool Read=fread(Raw,1,(size_t)Size,F)==(size_t)Size;fclose(F);xcronzone* Z=NULL;
    bool OK=Read && xcronZoneCreateTzif((xbytesview){Raw,(size_t)Size},&Z,NULL);memset(Raw,0,(size_t)Size);free(Raw);if(!OK)return 3;
    puts(CursorMode?"cursor_compare=3":"paired_issue=2");char Line[256];
    while(fgets(Line,sizeof(Line),stdin)){long long Year;xdatetime D={0};
        if(sscanf(Line,"%lld %d %d %d %d %d %d",&Year,&D.Month,&D.Day,&D.Hour,&D.Minute,&D.Second,&D.Millisecond)!=7){xcronZoneRelease(Z);return 4;}D.Year=(int64)Year;
        for(int Fold=0;Fold<4;++Fold)for(int Missing=0;Missing<2;++Missing){xtime Values[258],Without[258];memset(Values,0x5a,sizeof(Values));memcpy(Without,Values,sizeof(Values));size_t Count=777,WithoutCount=777;xcrondeliveryissue I;
            xcrondeliverystatus S=xcronDeliveryResolve(Z,&D,(xcrondeliverypolicy){Fold,Missing},Values,258,&Count,&I);
            xcrondeliverystatus Other=xcronDeliveryResolve(Z,&D,(xcrondeliverypolicy){Fold,Missing},Without,258,&WithoutCount,NULL);
            if(Other!=S || WithoutCount!=Count || memcmp(Values,Without,sizeof(Values)) ||
                (S==XCRON_DELIVERY_ERROR && (xrtErrorCode(xrtGetError())!=(int32)I.Code || strcmp(xrtErrorDomain(xrtGetError()),"xrt.cron.delivery")))){xcronZoneRelease(Z);return 6;}
            xcrondeliverycursorissue CI={0};unsigned Steps=0;
            if(CursorMode){xcrondeliverycursor C;xtime FromCursor[258],Empty[258];memset(Empty,0x5a,sizeof(Empty));memcpy(FromCursor,Empty,sizeof(Empty));size_t CursorCount=777;
                if(!xcronDeliveryCursorInit(&C,Z,&D,(xcrondeliverypolicy){Fold,Missing},&CI)){xcronZoneRelease(Z);return 7;}
                xcrondeliverystatus CS;
                do {xcrondeliverycursorissue Previous=CI;xcrondeliverybudget B={1,(uint32)(1+(Fold+Missing)%3),(uint32)(1+Fold%2)};
                    CS=xcronDeliveryCursorStep(&C,Z,B,FromCursor,258,&CursorCount,&CI);
                    if(++Steps>100000 || CI.ChecksExamined<Previous.ChecksExamined || CI.ChecksExamined-Previous.ChecksExamined>B.Checks || CI.OffsetsExamined<Previous.OffsetsExamined || CI.OffsetsExamined-Previous.OffsetsExamined>B.Offsets || CI.RuleYearsExamined<Previous.RuleYearsExamined || CI.RuleYearsExamined-Previous.RuleYearsExamined>B.RuleYears ||
                        (CS==XCRON_DELIVERY_PENDING && (CursorCount!=777 || memcmp(FromCursor,Empty,sizeof(Empty)) || CI.Issue.Candidates))){xcronZoneRelease(Z);return 8;}
                }while(CS==XCRON_DELIVERY_PENDING);
                if(CS!=S || CursorCount!=Count || memcmp(FromCursor,Values,sizeof(Values)) || CI.Issue.Code!=I.Code || CI.Issue.Zone!=I.Zone || CI.Issue.Candidates!=I.Candidates){xcronZoneRelease(Z);return 9;}
            }
            printf("%d %zu %d %d %zu",S,Count,I.Code,I.Zone,I.Candidates);
            if(S==XCRON_DELIVERY_FOUND)for(size_t J=0;J<Count;++J)printf(" %" PRId64,Values[J]);
            if(CursorMode)printf(" %u %" PRIu64 " %" PRIu64 " %" PRIu64,Steps,CI.ChecksExamined,CI.OffsetsExamined,CI.RuleYearsExamined);
            printf("%s",Fold==3 && Missing==1?"\n":"|");xrtClearError();
        }
    }xcronZoneRelease(Z);return ferror(stdin)?5:0;
}
