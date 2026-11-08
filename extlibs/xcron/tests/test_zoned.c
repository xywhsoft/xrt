#include "test.h"
#include <limits.h>
#if defined(XRT_FEATURE_THREAD)
#include <xrt/thread.h>
#endif
#define XCRON_TEST_TZIF_BYTES 16384
#include "tzif_fixture.h"
static xtime zonedTime(int64 Y,int M,int D,int H,int Min,int S,int Ms)
{xtime T;testRequire(xrtDateTime(Y,M,D,H,Min,S,Ms,&T),"zoned fixture time");return T;}
static xcronexpression zonedExpr(cstr Text,xcrondialect Dialect)
{xcronexpression E;testRequire(xcronExpressionCompile((xstrview){Text,strlen(Text)},Dialect,&E,NULL),"zoned expression fixture");return E;}
static xcronzone* zonedZone(cstr Rule)
{xcronzone* Z=NULL;testRequire(xcronZoneCreatePosix((xstrview){Rule,strlen(Rule)},XCRON_ZONE_TZIF3,&Z,NULL),"zoned rule fixture");return Z;}
static xcronzone* zonedBinary(const tzifblob* B)
{xcronzone* Z=NULL;testRequire(xcronZoneCreateTzif((xbytesview){B->Data,B->Size},&Z,NULL),"zoned binary fixture");return Z;}
#include "zoned_cursor_cases.h"
static void zonedExpect(const xcronexpression* E,const xcronzone* Z,xtime Begin,xtime End,uint32 Flags,xcronzonedstatus Expected,xtime Value)
{xtime Next=777;xcronzonedissue I;testRequire(xcronZonedNext(E,Z,Begin,End,Flags,&Next,&I)==Expected && I.Code==XCRON_ZONED_OK && I.Field==XCRON_FIELD_NONE && I.Zone==XCRON_ZONE_OK && Next==(Expected==XCRON_ZONED_FOUND?Value:777),"zoned earliest/status/output atomicity");zonedCursorExpect(E,Z,Begin,End,Flags,Expected,Value);}
static void testZonedKnown(void)
{
    xcronzone* Z=zonedZone("EST5EDT,M3.2.0,M11.1.0");xcronexpression E=zonedExpr("30 1 * * *",XCRON_UNIX5);
    xtime A=zonedTime(2024,11,3,5,30,0,0),B=zonedTime(2024,11,3,6,30,0,0),End=zonedTime(2024,11,5,0,0,0,0);
    zonedExpect(&E,Z,A-1,End,0,XCRON_ZONED_FOUND,A);zonedExpect(&E,Z,A,End,XCRON_SEARCH_EXCLUSIVE,XCRON_ZONED_FOUND,B);
    zonedExpect(&E,Z,zonedTime(2024,11,3,5,45,0,0),End,0,XCRON_ZONED_FOUND,B);
    zonedExpect(&E,Z,B,End,XCRON_SEARCH_EXCLUSIVE,XCRON_ZONED_FOUND,zonedTime(2024,11,4,6,30,0,0));
    bool Matches=false;testRequire(xcronZonedMatch(&E,Z,A,&Matches,NULL) && Matches && xcronZonedMatch(&E,Z,B,&Matches,NULL) && Matches,"both actual repeated instants match");
    testRequire(xcronZonedMatch(&E,Z,B+1,&Matches,NULL) && !Matches,"fraction does not match");
    E=zonedExpr("30 2 * * *",XCRON_UNIX5);xtime Spring=zonedTime(2024,3,10,0,0,0,0);
    zonedExpect(&E,Z,Spring,Spring+XRT_TIME_DAY-1,0,XCRON_ZONED_NONE,0);
    zonedExpect(&E,Z,Spring,Spring+2*XRT_TIME_DAY,0,XCRON_ZONED_FOUND,zonedTime(2024,3,11,6,30,0,0));xcronZoneRelease(Z);
    Z=zonedZone("LHST-10:30LHDT-11,M10.1.0,M4.1.0");E=zonedExpr("45 1 * * *",XCRON_UNIX5);
    zonedExpect(&E,Z,zonedTime(2024,4,6,14,50,0,0),zonedTime(2024,4,7,0,0,0,0),0,XCRON_ZONED_FOUND,zonedTime(2024,4,6,15,15,0,0));xcronZoneRelease(Z);
    Z=zonedZone("IST-1GMT0,M10.5.0,M3.5.0/1");E=zonedExpr("30 1 * * *",XCRON_UNIX5);
    zonedExpect(&E,Z,zonedTime(2024,10,27,0,45,0,0),zonedTime(2024,10,28,0,0,0,0),0,XCRON_ZONED_FOUND,zonedTime(2024,10,27,1,30,0,0));xcronZoneRelease(Z);
    tzifblob Blob;const int64 Times[]={0,3600,7200};const uint8 Indices[]={1,2,2};const tzifkind Types[]={{7200,false,"AAA"},{3600,false,"BBB"},{0,false,"CCC"}};
    tzifBuild(&Blob,'3',Times,Indices,3,Types,3,"CCC0");Z=zonedBinary(&Blob);E=zonedExpr("30 1 * * *",XCRON_UNIX5);
    const xtime Expected[]={XRT_TIME_UNIX_EPOCH-1800000,XRT_TIME_UNIX_EPOCH+1800000,XRT_TIME_UNIX_EPOCH+5400000};xtime First=Expected[0]-1;
    for(unsigned I=0;I<3;++I){zonedExpect(&E,Z,First,XRT_TIME_UNIX_EPOCH+XRT_TIME_DAY,0,XCRON_ZONED_FOUND,Expected[I]);First=Expected[I]+1;}
    xcronZoneRelease(Z);
}
static void testZonedCompleteness(void)
{
    xcronexpression E=zonedExpr("30 2 1 MAR *",XCRON_UNIX5);xcronzone* Z=zonedZone("STD0DST,J60/2,J300/2");xtime Next=777;xcronzonedissue I;
    testRequire(xcronZonedNext(&E,Z,zonedTime(2000,1,1,0,0,0,0),INT64_MAX,0,&Next,&I)==XCRON_ZONED_NONE && Next==777 && I.SegmentsExamined<=803 && I.SegmentsExamined>=799,"entire periodic cycle proves always-gap NONE");
    zonedCursorExpect(&E,Z,zonedTime(2000,1,1,0,0,0,0),INT64_MAX,0,XCRON_ZONED_NONE,0);xcronZoneRelease(Z);
    /* 500 years of historical yearly gaps precede a different fixed footer.
     * A global Begin+400-year cutoff would miss the first 2500 occurrence. */
    tzifblob Blob;int64 Times[1000];uint8 Indices[1000];const tzifkind Types[]={{0,false,"STD"},{3600,true,"DST"}};
    for(unsigned Y=0;Y<500;++Y){Times[Y*2]=(zonedTime(2000+Y,3,1,2,0,0,0)-XRT_TIME_UNIX_EPOCH)/1000;Times[Y*2+1]=(zonedTime(2000+Y,10,1,1,0,0,0)-XRT_TIME_UNIX_EPOCH)/1000;Indices[Y*2]=1;Indices[Y*2+1]=0;}
    tzifBuild(&Blob,'3',Times,Indices,1000,Types,2,"STD0");Z=zonedBinary(&Blob);
    testRequire(xcronZonedNext(&E,Z,zonedTime(2000,1,1,0,0,0,0),zonedTime(2600,1,1,0,0,0,0),0,&Next,&I)==XCRON_ZONED_FOUND && Next==zonedTime(2500,3,1,2,30,0,0) && I.SegmentsExamined==1001,"history is completed before applying footer periodicity");
    zonedCursorExpect(&E,Z,zonedTime(2000,1,1,0,0,0,0),zonedTime(2600,1,1,0,0,0,0),0,XCRON_ZONED_FOUND,Next);xcronZoneRelease(Z);
}
static uint64 ZonedRandom=UINT64_C(0x7c19ad42cfff9013);
static uint64 zonedRandom(void){ZonedRandom^=ZonedRandom<<13;ZonedRandom^=ZonedRandom>>7;ZonedRandom^=ZonedRandom<<17;return ZonedRandom;}
static bool zonedBruteMatch(const xcronexpression* E,const xcronzone* Z,xtime T)
{
    xcronzonestate S;xdatetime D;testRequire(xcronZoneAt(Z,T,&S,NULL) && xrtTimeSplitAt(T,S.Offset,&D),"independent brute fixed-offset decomposition");
    unsigned Parts[]={D.Second,D.Minute,D.Hour,D.Day,D.Month,D.Weekday};
    for(unsigned F=0;F<3;++F)if(!((E->Fields[F]>>Parts[F])&1))return false;
    if(!((E->Fields[XCRON_FIELD_MONTH]>>D.Month)&1))return false;
    bool DOM=((E->Fields[XCRON_FIELD_DAY]>>D.Day)&1)!=0,DOW=((E->Fields[XCRON_FIELD_WEEKDAY]>>D.Weekday)&1)!=0;
    return E->Flags?(DOM && DOW):(DOM || DOW);
}
static void testZonedOraclesAndEnds(void)
{
    const cstr Rules[]={"EST5EDT,M3.2.0,M11.1.0","AEST-10AEDT,M10.1.0,M4.1.0/3","IST-1GMT0,M10.5.0,M3.5.0/1","STD0DST,M3.2.0/-167,M11.1.0/167"};
    for(unsigned R=0;R<4;++R){xcronzone* Z=zonedZone(Rules[R]);for(unsigned J=0;J<40;++J){xcronexpression E=zonedExpr(J%2?"0 17,47 * * * *":"0 30 2 * * *",XCRON_SECONDS6);
            xtime Begin=zonedTime(2024,1,1,0,0,0,0)+(xtime)(zonedRandom()%31622400)*1000,End=Begin+7200*1000,Expected=0;bool Found=false;
            for(xtime T=Begin;T<=End;T+=1000)if(zonedBruteMatch(&E,Z,T)){Expected=T;Found=true;break;}
            zonedExpect(&E,Z,Begin,End,0,Found?XCRON_ZONED_FOUND:XCRON_ZONED_NONE,Expected);
        }xcronZoneRelease(Z);}
    xcronzone* UTC=zonedZone("UTC0");const cstr Texts[]={"* * * * * *","13,37 */17 * * * *","0 0 0 29 FEB *","0 0 0 30 FEB *"};
    for(unsigned J=0;J<1000;++J){xcronexpression E=zonedExpr(Texts[J%4],XCRON_SECONDS6);xtime Begin=(xtime)zonedRandom(),Span=(xtime)(zonedRandom()%(60*XRT_TIME_DAY)),End=Begin>INT64_MAX-Span?INT64_MAX:Begin+Span,A=11,B=22;uint32 Flags=J%2;
        xcroncalendarstatus C=xcronCalendarNext(&E,Begin,End,Flags,&A,NULL);xcronzonedstatus Z=xcronZonedNext(&E,UTC,Begin,End,Flags,&B,NULL);testRequire((int)C==(int)Z && (C==XCRON_CALENDAR_FOUND?A==B:B==22),"full-range UTC specialization agrees with independent calendar entrypoint");
        zonedCursorExpect(&E,UTC,Begin,End,Flags,(xcronzonedstatus)C,A);}
    xcronZoneRelease(UTC);
    tzifblob Blob;const tzifkind Huge[]={{INT32_MAX,false,""}};tzifBuild(&Blob,0,NULL,NULL,0,Huge,1,"");xcronzone* Z=zonedBinary(&Blob);xcronexpression E=zonedExpr("* * * * * *",XCRON_SECONDS6);
    zonedExpect(&E,Z,INT64_MIN,INT64_MAX,0,XCRON_ZONED_FOUND,INT64_MIN-INT64_MIN%1000);
    zonedExpect(&E,Z,INT64_MIN,INT64_MIN+807,0,XCRON_ZONED_NONE,0);zonedExpect(&E,Z,INT64_MAX,INT64_MAX,0,XCRON_ZONED_NONE,0);
    zonedExpect(&E,Z,INT64_MAX-807,INT64_MAX,XCRON_SEARCH_EXCLUSIVE,XCRON_ZONED_NONE,0);xcronZoneRelease(Z);
    const tzifkind Seconds[]={{17,false,"SEC"}};tzifBuild(&Blob,0,NULL,NULL,0,Seconds,1,"");Z=zonedBinary(&Blob);E=zonedExpr("* * * * *",XCRON_UNIX5);
    zonedExpect(&E,Z,XRT_TIME_UNIX_EPOCH,XRT_TIME_UNIX_EPOCH+60000,0,XCRON_ZONED_FOUND,XRT_TIME_UNIX_EPOCH+43000);xcronZoneRelease(Z);
}
static void testZonedUnknownAndErrors(void)
{
    tzifblob Blob;const int64 Times[]={0,86400};const uint8 Indices[]={1,1};const tzifkind Types[]={{0,false,"-00"},{0,false,"AAA"}};tzifBuild(&Blob,'3',Times,Indices,2,Types,2,"");
    xcronzone* Z=zonedBinary(&Blob);xcronexpression E=zonedExpr("0 12 * * *",XCRON_UNIX5);xtime Next=777;xcronzonedissue I;bool Matches=true;
    zonedExpect(&E,Z,XRT_TIME_UNIX_EPOCH,XRT_TIME_UNIX_EPOCH+XRT_TIME_DAY-1,0,XCRON_ZONED_FOUND,XRT_TIME_UNIX_EPOCH+12*XRT_TIME_HOUR);
    testRequire(xcronZonedNext(&E,Z,XRT_TIME_UNIX_EPOCH-1000,XRT_TIME_UNIX_EPOCH+XRT_TIME_DAY,0,&Next,&I)==XCRON_ZONED_ERROR && I.Code==XCRON_ZONED_ZONE && I.Zone==XCRON_ZONE_UNKNOWN && Next==777,"unknown prefix cannot be skipped");xrtClearError();
    testRequire(!xcronZonedMatch(&E,Z,XRT_TIME_UNIX_EPOCH-1000,&Matches,&I) && Matches && I.Zone==XCRON_ZONE_UNKNOWN,"unknown whole-second match is an error");xrtClearError();
    testRequire(xcronZonedMatch(&E,Z,XRT_TIME_UNIX_EPOCH-1,&Matches,&I) && !Matches && !I.SegmentsExamined,"fraction is false independently of unknown offset");
    zonedExpect(&E,Z,XRT_TIME_UNIX_EPOCH-1,XRT_TIME_UNIX_EPOCH-1,0,XCRON_ZONED_NONE,0);xcronZoneRelease(Z);
    Z=zonedZone("UTC0");I=(xcronzonedissue){XCRON_ZONED_RANGE,XCRON_FIELD_MINUTE,XCRON_ZONE_DATE,123,456};xcronzonedissue Old=I;
    testRequire(xcronZonedNext(&E,Z,0,1,2,&Next,&I)==XCRON_ZONED_ERROR && Next==777 && !memcmp(&I,&Old,sizeof(I)),"invalid flags preserve all outputs");xrtClearError();
    testRequire(!xcronZonedMatch(&E,Z,0,(bool*)&E,&I) && !memcmp(&I,&Old,sizeof(I)),"expression output alias preserves issue");xrtClearError();
    xcronzonestate S;testRequire(xcronZoneAt(Z,0,&S,NULL),"owned view fixture");testRequire(!xcronZonedMatch(&E,Z,0,(bool*)S.Abbreviation.Data,&I) && !memcmp(&I,&Old,sizeof(I)),"zone owned output alias protected");xrtClearError();
    testRequire(xcronZonedNext(&E,Z,2,1,0,&Next,&I)==XCRON_ZONED_ERROR && I.Code==XCRON_ZONED_WINDOW && Next==777,"reversed value window");xrtClearError();
    E.Fields[XCRON_FIELD_MINUTE]=0;xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"zoned diagnostic OOM scope");
    testRequire(xcronZonedNext(&E,Z,0,1,0,&Next,&I)==XCRON_ZONED_ERROR && I.Code==XCRON_ZONED_EXPRESSION && I.Field==XCRON_FIELD_MINUTE && Next==777 && xrtErrorKind(xrtGetError())==XERR_MEMORY,"typed zoned value failure survives diagnostic OOM");
    testRequire(xrtAllocationBudgetLeave(&Scope),"leave zoned OOM scope");xrtClearError();xallocationbudgetinfo Info={.Size=sizeof(Info)};testRequire(xrtAllocationBudgetInfo(Budget,&Info) && !Info.LiveBytes && !Info.LiveCount,"zoned diagnostic ownership balance");xrtAllocationBudgetFree(Budget);xcronZoneRelease(Z);
}
static void testZonedWarm(void)
{
    xcronzone* Z=zonedZone("EST5EDT,M3.2.0,M11.1.0");xcronexpression E=zonedExpr("30 1 * * *",XCRON_UNIX5);xtime A=zonedTime(2024,11,3,5,30,0,0),B=zonedTime(2024,11,3,6,30,0,0);
    xcronzone* Cycle=zonedZone("STD0DST,J60/2,J300/2");xcronexpression Impossible=zonedExpr("30 2 1 MAR *",XCRON_UNIX5);xtime CycleBegin=zonedTime(2000,1,1,0,0,0,0);
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget),"allocation-free zoned scope");xrtSetErrorInfo(XERR_STATE,"sentinel",7,"preserve");const xerror* Ambient=xrtGetError();testRequire(xrtAllocationBudgetEnter(Budget,&Scope),"enter warm scope");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(true);xrtMemStatsGet(&Before);
#endif
    for(unsigned J=0;J<10000;++J){xtime Next=777;bool Match=false;testRequire(xcronZonedMatch(&E,Z,B,&Match,NULL) && Match && xcronZonedNext(&E,Z,A,B,XCRON_SEARCH_EXCLUSIVE,&Next,NULL)==XCRON_ZONED_FOUND && Next==B && xcronZonedNext(&E,Z,B,B,0,&Next,NULL)==XCRON_ZONED_FOUND && Next==B && xrtGetError()==Ambient,"warm zoned queries preserve ambient error");}
    xtime Next=777;testRequire(xcronZonedNext(&Impossible,Cycle,CycleBegin,INT64_MAX,0,&Next,NULL)==XCRON_ZONED_NONE && Next==777 && xrtGetError()==Ambient,"complete cycle NONE also preserves allocation/error state");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);testRequire(Before.MallocCalls==After.MallocCalls && Before.CallocCalls==After.CallocCalls && Before.ReallocCalls==After.ReallocCalls && Before.BlockAllocCalls==After.BlockAllocCalls,"zoned warm query allocated");
#endif
    testRequire(xrtAllocationBudgetLeave(&Scope),"leave warm scope");xrtClearError();xrtAllocationBudgetFree(Budget);xcronZoneRelease(Z);xcronZoneRelease(Cycle);
}
#if defined(XRT_FEATURE_THREAD)
typedef struct zonedcontext {xcronexpression Expression;xcronzone* Zone;xtime Begin,End;} zonedcontext;
static int32 zonedWorker(ptr Data)
{
    const zonedcontext* C=Data;
    for(unsigned J=0;J<12000;++J){xtime Next;bool Match=false;
        if(xcronZonedNext(&C->Expression,C->Zone,C->Begin,C->End,XCRON_SEARCH_EXCLUSIVE,&Next,NULL)!=XCRON_ZONED_FOUND || Next!=C->End ||
            !xcronZonedMatch(&C->Expression,C->Zone,Next,&Match,NULL) || !Match)return 1;
    }
    xcronZoneRelease(C->Zone);return 0;
}
static void testZonedThreads(void)
{
    zonedcontext C={zonedExpr("30 1 * * *",XCRON_UNIX5),zonedZone("EST5EDT,M3.2.0,M11.1.0"),zonedTime(2024,11,3,5,30,0,0),zonedTime(2024,11,3,6,30,0,0)};xthread* T[4];
    for(unsigned J=0;J<4;++J){testRequire(xcronZoneRetain(C.Zone),"retained zoned worker");T[J]=xrtThreadCreate(zonedWorker,&C,0);testRequire(T[J]!=NULL,"start zoned worker");}
    xcronZoneRelease(C.Zone);
    for(unsigned J=0;J<4;++J){testRequire(xrtThreadWait(T[J])==XWAIT_OK && !xrtThreadExitCode(T[J]),"shared immutable expression/zone lifetime");xrtThreadDestroy(T[J]);}
}
#endif
int main(void){testZonedKnown();testZonedCompleteness();testZonedOraclesAndEnds();testZonedUnknownAndErrors();testZonedWarm();testZonedRuleWalk();testZonedCursorErrors();testZonedCursorWarm();testZonedCursorUnknown();
#if defined(XRT_FEATURE_THREAD)
testZonedThreads();testZonedCursorHandoff();
#endif
puts("[PASS] zoned cron: ordered folds/gaps/history/periodic proof/UTC oracles/endpoints/OOM/zero-heap");return 0;}
