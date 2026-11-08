#include <xcron.h>
#include <xrt/error.h>
#include <xrt/memory.h>
#include <xrt/thread.h>
#include "test.h"
#include <string.h>
#define XCRON_TEST_TZIF_BYTES 32768
#include "tzif_fixture.h"
static xtime occurrenceTime(int64 Y,int M,int D,int H,int Minute,int S)
{xtime T;testRequire(xrtDateTime(Y,M,D,H,Minute,S,0,&T),"occurrence fixture date");return T;}
static xcronzone* occurrenceZone(cstr Rule)
{xcronzone* Z=NULL;testRequire(xcronZoneCreatePosix((xstrview){Rule,strlen(Rule)},XCRON_ZONE_TZIF3,&Z,NULL),"occurrence zone fixture");return Z;}
static xcronexpression occurrenceExpr(cstr Text)
{xcronexpression E;testRequire(xcronExpressionCompile((xstrview){Text,strlen(Text)},XCRON_SECONDS6,&E,NULL),"occurrence expression fixture");return E;}
static void occurrenceBudget(const xcronoccurrenceissue* I,const xcronoccurrenceissue* P,xcronoccurrencebudget B)
{testRequire(I->SegmentsExamined>=P->SegmentsExamined && I->SegmentsExamined-P->SegmentsExamined<=B.Segments && I->MonthsExamined>=P->MonthsExamined && I->MonthsExamined-P->MonthsExamined<=B.Months && I->RuleYearsExamined>=P->RuleYearsExamined && I->RuleYearsExamined-P->RuleYearsExamined<=B.RuleYears && I->ChecksExamined>=P->ChecksExamined && I->ChecksExamined-P->ChecksExamined<=B.Checks && I->OffsetsExamined>=P->OffsetsExamined && I->OffsetsExamined-P->OffsetsExamined<=B.Offsets,"independent occurrence budgets bound every actual traversal delta");}
static void occurrenceExpect(const xcronexpression* E,const xcronzone* Z,xtime Begin,xtime End,uint32 Flags,xcrondeliverypolicy P,xcronoccurrencestatus Expected,xcronoccurrenceerror Code,xtime When,size_t Candidates)
{
    xcronoccurrence Out,Before,Other;memset(&Out,0x5a,sizeof(Out));memcpy(&Before,&Out,sizeof(Out));memcpy(&Other,&Out,sizeof(Out));xcronoccurrenceissue I;
    xcronoccurrencestatus S=xcronOccurrenceNext(E,Z,Begin,End,Flags,P,&Out,&I);
    testRequire(S==Expected && I.Code==Code && I.Candidates==Candidates,"independent earliest occurrence status/code/global count");
    testRequire(S==XCRON_OCCURRENCE_FOUND?Out.When==When && Out.Candidates==Candidates && !Out.Local.Offset && Out.Local.IsDST==-1 && !Out.Local.Millisecond:!memcmp(&Out,&Before,sizeof(Out)),"FOUND alone publishes canonical occurrence");
    if(S==XCRON_OCCURRENCE_FOUND || S==XCRON_OCCURRENCE_ERROR)testRequire(I.When==When,"UTC result/error anchor");
    testRequire(xcronOccurrenceNext(E,Z,Begin,End,Flags,P,&Other,NULL)==S && !memcmp(&Other,&Out,sizeof(Out)),"optional Issue parity");if(S==XCRON_OCCURRENCE_ERROR)xrtClearError();
    xcronoccurrencecursor C,Snapshot;xcronexpression Copy=*E;xcronoccurrenceissue CI,Previous;
    testRequire(xcronOccurrenceCursorInit(&C,&Copy,Z,Begin,End,Flags,P,&CI) && !CI.SegmentsExamined && !CI.MonthsExamined && !CI.RuleYearsExamined && !CI.ChecksExamined && !CI.OffsetsExamined && !CI.LabelsExamined,"init snapshots expression without traversal");memset(&Copy,0xa5,sizeof(Copy));memcpy(&Snapshot,&C,sizeof(C));memcpy(&Other,&Before,sizeof(Other));
    xcronoccurrencestatus CS=xcronOccurrenceCursorStep(&C,Z,(xcronoccurrencebudget){0},&Other,&CI);testRequire(!memcmp(&C,&Snapshot,sizeof(C)),"all-zero cursor call changes no bytes");unsigned Steps=0;
    while(CS==XCRON_OCCURRENCE_PENDING){Previous=CI;xcronoccurrencebudget B={1,1,1,1,1};CS=xcronOccurrenceCursorStep(&C,Z,B,&Other,&CI);occurrenceBudget(&CI,&Previous,B);testRequire(++Steps<200000,"finite budgeted occurrence search");if(CS==XCRON_OCCURRENCE_PENDING)testRequire(!memcmp(&Other,&Before,sizeof(Other)) && !CI.Candidates && !CI.Local.Year && !CI.When && CI.Code==XCRON_OCCURRENCE_OK,"pending hides provisional candidate/error and preserves output");}
    testRequire(CS==S && !memcmp(&Other,&Out,sizeof(Out)) && CI.Code==I.Code && CI.Zone==I.Zone && CI.Candidates==I.Candidates && CI.When==I.When,"cursor and synchronous result/error metadata agree");
    if(S==XCRON_OCCURRENCE_FOUND || (S==XCRON_OCCURRENCE_ERROR && I.Local.Year))testRequire(!memcmp(&CI.Local,&I.Local,sizeof(I.Local)),"terminal canonical nominal label parity");
    Previous=CI;CS=xcronOccurrenceCursorStep(&C,Z,(xcronoccurrencebudget){0},&Other,&CI);testRequire(CS==S && !memcmp(&CI,&Previous,sizeof(CI)),"zero-budget terminal repeat preserves all counters/issues");if(S==XCRON_OCCURRENCE_ERROR)xrtClearError();
}
static void testOccurrenceGolden(void)
{
    xcronzone* Z=occurrenceZone("EST5EDT,M3.2.0,M11.1.0");xcronexpression E=occurrenceExpr("0 30 1 * * *");xtime A=occurrenceTime(2024,11,3,5,30,0),B=A+3600000;
    for(int Missing=0;Missing<2;++Missing){
        occurrenceExpect(&E,Z,A-1,B,0,(xcrondeliverypolicy){XCRON_FOLD_ALL,Missing},XCRON_OCCURRENCE_FOUND,0,A,2);
        occurrenceExpect(&E,Z,A,B,XCRON_SEARCH_EXCLUSIVE,(xcrondeliverypolicy){XCRON_FOLD_ALL,Missing},XCRON_OCCURRENCE_FOUND,0,B,2);
        occurrenceExpect(&E,Z,A,B,XCRON_SEARCH_EXCLUSIVE,(xcrondeliverypolicy){XCRON_FOLD_FIRST,Missing},XCRON_OCCURRENCE_NONE,0,0,0);
        occurrenceExpect(&E,Z,A-1,B,0,(xcrondeliverypolicy){XCRON_FOLD_LAST,Missing},XCRON_OCCURRENCE_FOUND,0,B,2);
        occurrenceExpect(&E,Z,B,B,0,(xcrondeliverypolicy){XCRON_FOLD_REJECT,Missing},XCRON_OCCURRENCE_ERROR,XCRON_OCCURRENCE_AMBIGUOUS,B,2);
    }
    E=occurrenceExpr("0 30 2 * * *");xtime Gap=occurrenceTime(2024,3,10,7,0,0),Tomorrow=occurrenceTime(2024,3,11,6,30,0);
    occurrenceExpect(&E,Z,Gap-XRT_TIME_HOUR,Tomorrow,0,(xcrondeliverypolicy){0,0},XCRON_OCCURRENCE_FOUND,0,Tomorrow,1);
    occurrenceExpect(&E,Z,Gap-XRT_TIME_HOUR,Tomorrow,0,(xcrondeliverypolicy){0,1},XCRON_OCCURRENCE_ERROR,XCRON_OCCURRENCE_MISSING,Gap,0);
    occurrenceExpect(&E,Z,Gap,Gap,0,(xcrondeliverypolicy){0,1},XCRON_OCCURRENCE_ERROR,XCRON_OCCURRENCE_MISSING,Gap,0);
    occurrenceExpect(&E,Z,Gap,Tomorrow,XCRON_SEARCH_EXCLUSIVE,(xcrondeliverypolicy){0,1},XCRON_OCCURRENCE_FOUND,0,Tomorrow,1);
    occurrenceExpect(&E,Z,Gap+1,Tomorrow,0,(xcrondeliverypolicy){0,1},XCRON_OCCURRENCE_FOUND,0,Tomorrow,1);
    occurrenceExpect(&E,Z,Gap-1,Gap-1,0,(xcrondeliverypolicy){0,1},XCRON_OCCURRENCE_NONE,0,0,0);
    E=occurrenceExpr("0 0 2,3 * * *");occurrenceExpect(&E,Z,Gap,Gap,0,(xcrondeliverypolicy){0,1},XCRON_OCCURRENCE_ERROR,XCRON_OCCURRENCE_MISSING,Gap,0);xcronZoneRelease(Z);
    tzifblob Blob;const int64 Times[]={-1800,0,3600};const uint8 Indices[]={1,0,1};const tzifkind Types[]={{3600,false,"OLD"},{0,false,"UTC"}};tzifBuild(&Blob,'3',Times,Indices,3,Types,2,"UTC0");testRequire(xcronZoneCreateTzif((xbytesview){Blob.Data,Blob.Size},&Z,NULL),"globally filled apparent gap");E=occurrenceExpr("0 15 0 * * *");
    occurrenceExpect(&E,Z,XRT_TIME_UNIX_EPOCH,XRT_TIME_UNIX_EPOCH+1000000,0,(xcrondeliverypolicy){0,1},XCRON_OCCURRENCE_NONE,0,0,0);xcronZoneRelease(Z);
    Z=occurrenceZone("UTC0");E=occurrenceExpr("* * * * * *");
    occurrenceExpect(&E,Z,INT64_MIN,INT64_MIN+2000,0,(xcrondeliverypolicy){0,1},XCRON_OCCURRENCE_FOUND,0,INT64_MIN+808,1);
    occurrenceExpect(&E,Z,INT64_MAX-1000,INT64_MAX,0,(xcrondeliverypolicy){0,1},XCRON_OCCURRENCE_FOUND,0,INT64_MAX-807,1);
    occurrenceExpect(&E,Z,INT64_MAX,INT64_MAX,0,(xcrondeliverypolicy){0,1},XCRON_OCCURRENCE_NONE,0,0,0);xcronZoneRelease(Z);
}
static uint64 OccurrenceRandom=UINT64_C(0x7345258a712c9431);
static uint32 occurrenceRandom(void){OccurrenceRandom^=OccurrenceRandom<<13;OccurrenceRandom^=OccurrenceRandom>>7;OccurrenceRandom^=OccurrenceRandom<<17;return (uint32)OccurrenceRandom;}
static int occurrenceOffset(int64 T,const int64* Times,const uint8* Indices,const int32* Offsets)
{unsigned I=0;while(I<8 && Times[I]<=T)++I;return Offsets[I?Indices[I-1]:0];}
static bool occurrenceModelMatch(int64 Local)
{int S=(int)(Local%60);if(S<0)S+=60;return S%17==0;}
static size_t occurrenceModelCandidates(int64 Local,const int64* Times,const uint8* Indices,const int32* Offsets,int64* First,int64* Last)
{size_t N=0;for(unsigned I=0;I<5;++I){int64 T=Local-Offsets[I];if(occurrenceOffset(T,Times,Indices,Offsets)==Offsets[I]){if(!N || T<*First)*First=T;if(!N || T>*Last)*Last=T;++N;}}return N;}
static void testOccurrenceBrute(void)
{
    const int32 Offsets[]={0,60,-60,120,-120};const tzifkind Types[]={{0,false,"UTC"},{60,false,"AAA"},{-60,false,"BBB"},{120,false,"CCC"},{-120,false,"DDD"}};xcronexpression E=occurrenceExpr("*/17 * * * * *");
    for(unsigned H=0;H<32;++H){int64 Times[8];uint8 Indices[8];for(unsigned I=0;I<8;++I){Times[I]=-100+(int64)I*50;Indices[I]=(uint8)(occurrenceRandom()%5);}Indices[7]=0;tzifblob Blob;tzifBuild(&Blob,'3',Times,Indices,8,Types,5,"UTC0");xcronzone* Z=NULL;testRequire(xcronZoneCreateTzif((xbytesview){Blob.Data,Blob.Size},&Z,NULL),"independent small history model");
        for(unsigned W=0;W<8;++W){int64 Begin=-160+(int64)(occurrenceRandom()%400),End=Begin+1+occurrenceRandom()%80;uint32 Flags=W%2?XCRON_SEARCH_EXCLUSIVE:0;
            for(int F=0;F<4;++F)for(int M=0;M<2;++M){xcronoccurrencestatus S=XCRON_OCCURRENCE_NONE;xcronoccurrenceerror Code=0;int64 Result=0;size_t Count=0;
                for(int64 T=Begin+(Flags?1:0);T<=End;++T){int64 L=T+occurrenceOffset(T,Times,Indices,Offsets);if(!occurrenceModelMatch(L))continue;int64 First=0,Last=0;size_t N=occurrenceModelCandidates(L,Times,Indices,Offsets,&First,&Last);
                    if(F==XCRON_FOLD_REJECT && N>1){S=XCRON_OCCURRENCE_ERROR;Code=XCRON_OCCURRENCE_AMBIGUOUS;Result=T;Count=N;break;}
                    if((F==XCRON_FOLD_FIRST && T!=First) || (F==XCRON_FOLD_LAST && T!=Last))continue;
                    S=XCRON_OCCURRENCE_FOUND;Result=T;Count=N;break;
                }
                if(M)for(unsigned J=0;J<8;++J){int64 T=Times[J];if(T<Begin+(Flags?1:0) || T>End || (S!=XCRON_OCCURRENCE_NONE && T>Result))continue;int Before=J?Offsets[Indices[J-1]]:Offsets[0],After=Offsets[Indices[J]];
                    for(int64 L=T+Before;L<T+After;++L){if(!occurrenceModelMatch(L))continue;int64 First=0,Last=0;if(occurrenceModelCandidates(L,Times,Indices,Offsets,&First,&Last))continue;S=XCRON_OCCURRENCE_ERROR;Code=XCRON_OCCURRENCE_MISSING;Result=T;Count=0;break;}
                }
                occurrenceExpect(&E,Z,XRT_TIME_UNIX_EPOCH+Begin*1000,XRT_TIME_UNIX_EPOCH+End*1000,Flags,(xcrondeliverypolicy){F,M},S,Code,S==XCRON_OCCURRENCE_NONE?0:XRT_TIME_UNIX_EPOCH+Result*1000,Count);
            }
        }xcronZoneRelease(Z);
    }
}
#include "occurrence_adversarial.h"
#include "occurrence_ordered.h"
int main(void)
{testOccurrenceGolden();testOccurrenceBrute();testOccurrenceHistory();testOccurrenceOrdered();testOccurrenceReinit();testOccurrenceArguments();testOccurrenceWarmAndOOM();
#if defined(XRT_FEATURE_THREAD)
testOccurrenceHandoff();
#endif
puts("[PASS] selected recurring occurrence: global folds, missing gap anchors/fills/ties, budgets, snapshots, endpoints and independent UTC model");return 0;}
