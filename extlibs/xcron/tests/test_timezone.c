#include "test.h"
#include <limits.h>
#if defined(XRT_FEATURE_THREAD)
#include <xrt/thread.h>
#endif
static xtime zoneStamp(int64 Y,int M,int D,int H,int Min,int S,int Ms)
{xtime T;testRequire(xrtDateTime(Y,M,D,H,Min,S,Ms,&T),"zone fixture time");return T;}
static xcronzone* zoneCreate(cstr Text)
{xcronzone* Z=NULL;testRequire(xcronZoneCreatePosix((xstrview){Text,strlen(Text)},XCRON_ZONE_TZIF3,&Z,NULL),"zone rule fixture");return Z;}
static void zoneExpect(xcronzone* Z,xtime T,int Offset,bool DST,cstr Name)
{xcronzonestate S;testRequire(xcronZoneAt(Z,T,&S,NULL) && S.Offset==Offset && S.IsDST==DST && S.Abbreviation.Size==strlen(Name) && !memcmp(S.Abbreviation.Data,Name,strlen(Name)),"known timezone state");}
static void testZoneKnown(void)
{
    xcronzone* UTC=zoneCreate("UTC0");xdatetime Epoch;xtime EpochCandidate;size_t EpochCount;
    testRequire(xcronZoneSplit(UTC,0,&Epoch,NULL) && Epoch.Year==1 && Epoch.Month==1 && Epoch.Day==1,"XRT zero is civil epoch");
    Epoch=(xdatetime){.Year=1970,.Month=1,.Day=1};
    testRequire(xcronZoneResolve(UTC,&Epoch,&EpochCandidate,1,&EpochCount,NULL)==XCRON_ZONE_FOUND && EpochCount==1 && EpochCandidate==XRT_TIME_UNIX_EPOCH,"Unix epoch bridge is explicit");xcronZoneRelease(UTC);
    xcronzone* Z=zoneCreate("EST5EDT,M3.2.0,M11.1.0");xtime Spring=zoneStamp(2024,3,10,7,0,0,0),Fall=zoneStamp(2024,11,3,6,0,0,0);
    zoneExpect(Z,Spring-1,-18000,false,"EST");zoneExpect(Z,Spring,-14400,true,"EDT");zoneExpect(Z,Fall-1,-14400,true,"EDT");zoneExpect(Z,Fall,-18000,false,"EST");
    xcronzonetransition N;xcronzoneissue I;testRequire(xcronZoneNextTransition(Z,Spring-1,&N,&I)==XCRON_ZONE_FOUND && N.When==Spring && N.Before.Offset==-18000 && N.After.Offset==-14400 && I.YearsExamined<=403,"exact spring transition");
    testRequire(xcronZoneNextTransition(Z,Spring,&N,NULL)==XCRON_ZONE_FOUND && N.When==Fall,"strictly next fall transition");
    xdatetime D={.Year=2024,.Month=3,.Day=10,.Hour=2,.Minute=30};xtime Times[2]={111,222};size_t Count=9;
    testRequire(xcronZoneResolve(Z,&D,Times,2,&Count,NULL)==XCRON_ZONE_NONE && !Count && Times[0]==111,"DST gap has no candidate");D.Month=11;D.Day=3;D.Hour=1;
    testRequire(xcronZoneResolve(Z,&D,NULL,0,&Count,&I)==XCRON_ZONE_CAPACITY && Count==2 && I.Code==XCRON_ZONE_LIMIT,"fold capacity sizing");
    testRequire(xcronZoneResolve(Z,&D,Times,1,&Count,NULL)==XCRON_ZONE_CAPACITY && Count==2 && Times[0]==111,"capacity failure preserves candidate array");
    testRequire(xcronZoneResolve(Z,&D,Times,2,&Count,NULL)==XCRON_ZONE_FOUND && Count==2 && Times[0]==zoneStamp(2024,11,3,5,30,0,0) && Times[1]==zoneStamp(2024,11,3,6,30,0,0),"ordered fold candidates");xcronZoneRelease(Z);
    Z=zoneCreate("AEST-10AEDT,M10.1.0,M4.1.0/3");zoneExpect(Z,zoneStamp(2024,1,1,0,0,0,0),39600,true,"AEDT");zoneExpect(Z,zoneStamp(2024,6,1,0,0,0,0),36000,false,"AEST");xcronZoneRelease(Z);
    Z=zoneCreate("LHST-10:30LHDT-11,M10.1.0,M4.1.0");D=(xdatetime){.Year=2024,.Month=4,.Day=7,.Hour=1,.Minute=45};
    testRequire(xcronZoneResolve(Z,&D,Times,2,&Count,NULL)==XCRON_ZONE_FOUND && Count==2 && Times[0]==zoneStamp(2024,4,6,14,45,0,0) && Times[1]==zoneStamp(2024,4,6,15,15,0,0),"half-hour fold");xcronZoneRelease(Z);
    Z=zoneCreate("IST-1GMT0,M10.5.0,M3.5.0/1");zoneExpect(Z,zoneStamp(2024,1,1,0,0,0,0),0,true,"GMT");zoneExpect(Z,zoneStamp(2024,7,1,0,0,0,0),3600,false,"IST");xcronZoneRelease(Z);
    Z=zoneCreate("STD0DST,J60/0,J61/0");zoneExpect(Z,zoneStamp(2024,2,29,12,0,0,0),0,false,"STD");xcronZoneRelease(Z);
    Z=zoneCreate("STD0DST,59/0,60/0");zoneExpect(Z,zoneStamp(2024,2,29,12,0,0,0),3600,true,"DST");xcronZoneRelease(Z);
    Z=zoneCreate("XXX3EDT4,0/0,J365/23");zoneExpect(Z,0,-14400,true,"EDT");testRequire(xcronZoneNextTransition(Z,0,&N,NULL)==XCRON_ZONE_NONE,"all-year negative DST has no effective transition");xcronZoneRelease(Z);
    Z=zoneCreate("STD0DST,J1/0,J1/1");zoneExpect(Z,0,0,false,"STD");testRequire(xcronZoneNextTransition(Z,0,&N,NULL)==XCRON_ZONE_NONE,"coincident empty season");xcronZoneRelease(Z);
    Z=zoneCreate("STD0DST,59/0,J60/1");
    testRequire(xcronZoneNextTransition(Z,zoneStamp(2096,3,2,0,0,0,0),&N,&I)==XCRON_ZONE_FOUND &&
        N.When==zoneStamp(2104,2,29,0,0,0,0) && I.YearsExamined>=8,"leap-only season crosses non-leap century without lookahead cutoff");xcronZoneRelease(Z);
    Z=zoneCreate("AAA0BBB0,M3.2.0,M11.1.0");
    testRequire(xcronZoneNextTransition(Z,Spring-1,&N,NULL)==XCRON_ZONE_FOUND && N.Before.Offset==N.After.Offset && N.Before.IsDST!=N.After.IsDST,"zero-delta DST changes remain effective");xcronZoneRelease(Z);
    char Mutable[]="ABC-3";Z=zoneCreate(Mutable);memset(Mutable,'?',sizeof(Mutable)-1);zoneExpect(Z,0,10800,false,"ABC");xcronZoneRelease(Z);
}
static uint64 ZoneRandom=UINT64_C(0x91fb42417234abcd);
static uint64 zoneRandom(void){ZoneRandom^=ZoneRandom<<13;ZoneRandom^=ZoneRandom>>7;ZoneRandom^=ZoneRandom<<17;return ZoneRandom;}
static void testZoneRoundtrips(void)
{
    const cstr Texts[]={"UTC0","<+03>-3","STD24:59:59","STD-24:59:59DST,0/0,J365/25","EST5EDT,M3.2.0/-167:59:59,M11.1.0/167:59:59"};
    for(unsigned K=0;K<sizeof(Texts)/sizeof(*Texts);++K){xcronzone* Z=zoneCreate(Texts[K]);for(unsigned J=0;J<3000;++J){xtime T=J==0?INT64_MIN:J==1?INT64_MAX:J==2?-1:(xtime)zoneRandom();xdatetime D;testRequire(xcronZoneSplit(Z,T,&D,NULL),"full timeline split");
            if(D.Offset>=-86399 && D.Offset<=86399){xdatetime Core;testRequire(xrtTimeSplitAt(T,D.Offset,&Core) && Core.Year==D.Year && Core.Month==D.Month && Core.Day==D.Day && Core.Hour==D.Hour && Core.Minute==D.Minute && Core.Second==D.Second && Core.Millisecond==D.Millisecond && Core.Weekday==D.Weekday && Core.YearDay==D.YearDay,"independent core fixed-offset decomposition agreement");}
            D.Offset=INT_MIN;D.IsDST=-99;D.Weekday=-99;D.YearDay=-99;xtime Values[2];size_t Count;
            testRequire(xcronZoneResolve(Z,&D,Values,2,&Count,NULL)==XCRON_ZONE_FOUND && Count && (Values[0]==T || (Count==2 && Values[1]==T)),"full timeline civil roundtrip incl extreme offsets/BCE/endpoints");}
        xcronzonetransition N;testRequire(xcronZoneNextTransition(Z,INT64_MAX,&N,NULL)==XCRON_ZONE_NONE,"transition beyond timestamp maximum");xcronZoneRelease(Z);}
}
static void testZoneInvalidAndOom(void)
{
    const cstr Bad[]={"","UTC","UT0","UTC25","UTC0junk","EST5EDT","ABC0DEF,M0.1.0,M11.1.0","ABC0DEF,M3.0.0,M11.1.0","ABC0DEF,M3.2.7,M11.1.0","ABC0DEF,J0,J365","ABC0DEF,366,1","ABC0DEF,M3.2.0/168,M11.1.0","ABC0DEF,M3.2.0/2:60,M11.1.0","<bad!>0","UTC0\n"};
    for(unsigned K=0;K<sizeof(Bad)/sizeof(*Bad);++K){xcronzone* Out=(xcronzone*)(uintptr_t)1;xcronzoneissue I;testRequire(!xcronZoneCreatePosix((xstrview){Bad[K],strlen(Bad[K])},XCRON_ZONE_TZIF3,&Out,&I) && Out==(xcronzone*)(uintptr_t)1 && I.Code!=XCRON_ZONE_OK && I.Offset<=strlen(Bad[K]),"invalid rules preserve output and classify failure");xrtClearError();}
    xcronzone* Out=(xcronzone*)(uintptr_t)1;xcronzoneissue I;testRequire(!xcronZoneCreatePosix(XRT_STR_LITERAL("ABC0DEF,M3.2.0/-1,M11.1.0"),XCRON_ZONE_POSIX,&Out,&I) && I.Code==XCRON_ZONE_SYNTAX,"version2 forbids signed transition hours");xrtClearError();
    char Text[XCRON_ZONE_TEXT_MAX+1];memset(Text,'A',sizeof(Text));testRequire(!xcronZoneCreatePosix((xstrview){Text,sizeof(Text)},XCRON_ZONE_TZIF3,&Out,&I) && I.Code==XCRON_ZONE_LIMIT,"bounded parser input");xrtClearError();
    for(unsigned J=0;J<3000;++J){unsigned char Raw[96];size_t Size=(size_t)(zoneRandom()%sizeof(Raw));for(size_t K=0;K<Size;++K)Raw[K]=(unsigned char)zoneRandom();Out=NULL;if(xcronZoneCreatePosix((xstrview){(char*)Raw,Size},XCRON_ZONE_TZIF3,&Out,NULL))xcronZoneRelease(Out);xrtClearError();}
    xcronzone* Z=zoneCreate("EST5EDT,M3.2.0,M11.1.0");xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"zone OOM scope");
    Out=(xcronzone*)(uintptr_t)1;testRequire(!xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_TZIF3,&Out,&I) && I.Code==XCRON_ZONE_MEMORY && Out==(xcronzone*)(uintptr_t)1,"factory OOM atomicity");
    testRequire(!xcronZoneCreatePosix(XRT_STR_LITERAL("BAD0DST"),XCRON_ZONE_TZIF3,&Out,&I) && I.Code==XCRON_ZONE_RULES_REQUIRED && xrtErrorKind(xrtGetError())==XERR_MEMORY,"typed parse issue survives diagnostic OOM");
    xcronzonestate S;xdatetime D;xtime V[2];size_t Count;xcronzonetransition N;testRequire(xcronZoneAt(Z,0,&S,NULL) && xcronZoneSplit(Z,0,&D,NULL) && xcronZoneResolve(Z,&D,V,2,&Count,NULL)==XCRON_ZONE_FOUND && xcronZoneNextTransition(Z,0,&N,NULL)==XCRON_ZONE_FOUND,"queries do not require heap");
    testRequire(xrtAllocationBudgetLeave(&Scope),"leave zone OOM scope");xrtClearError();xallocationbudgetinfo Info={0};Info.Size=sizeof(Info);testRequire(xrtAllocationBudgetInfo(B,&Info) && !Info.LiveBytes && !Info.LiveCount,"zone OOM ownership balance");xrtAllocationBudgetFree(B);xcronZoneRelease(Z);
}
static void testZoneArgumentsAndWarm(void)
{
    xcronzone* Z=zoneCreate("EST5EDT,M3.2.0,M11.1.0");xcronzonestate S;testRequire(xcronZoneAt(Z,0,&S,NULL),"borrowed zone state");xcronzoneissue I={XCRON_ZONE_DATE,999,999},Before=I;
    testRequire(!xcronZoneAt(Z,0,NULL,&I) && !memcmp(&I,&Before,sizeof(I)),"invalid output leaves issue unchanged");
    testRequire(!xcronZoneAt(Z,0,(xcronzonestate*)S.Abbreviation.Data,&I) && !memcmp(&I,&Before,sizeof(I)),"owned storage output alias rejected");xrtClearError();
    testRequire(xcronZoneNextTransition(Z,0,NULL,&I)==XCRON_ZONE_ERROR && !memcmp(&I,&Before,sizeof(I)) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT,"missing transition preserves issue and diagnoses argument");xrtClearError();
    xdatetime D={.Year=2024,.Month=2,.Day=30};xtime V[2]={11,22};size_t Count=33;testRequire(xcronZoneResolve(Z,&D,V,2,&Count,&I)==XCRON_ZONE_ERROR && I.Code==XCRON_ZONE_DATE && Count==33 && V[0]==11,"invalid civil result atomicity");xrtClearError();
    xrtSetErrorInfo(XERR_STATE,"sentinel",73,"preserve");const xerror* Ambient=xrtGetError();
    #if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats A,B;xrtMemStatsEnable(true);xrtMemStatsGet(&A);
    #endif
    for(unsigned J=0;J<10000;++J){xtime T=(xtime)J*123456789;xcronzonetransition N;testRequire(xcronZoneAt(Z,T,&S,NULL) && xcronZoneSplit(Z,T,&D,NULL) && xcronZoneResolve(Z,&D,V,2,&Count,NULL)==XCRON_ZONE_FOUND && xcronZoneNextTransition(Z,T,&N,NULL)==XCRON_ZONE_FOUND && xrtGetError()==Ambient,"warm queries preserve error identity");}
    #if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&B);xrtMemStatsEnable(false);testRequire(A.MallocCalls==B.MallocCalls && A.CallocCalls==B.CallocCalls && A.ReallocCalls==B.ReallocCalls && A.BlockAllocCalls==B.BlockAllocCalls,"warm timezone allocated");
    #endif
    testRequire(xcronZoneRetain(Z),"explicit retained owner");xcronZoneRelease(Z);zoneExpect(Z,0,-18000,false,"EST");xcronZoneRelease(Z);xrtClearError();
}
#if defined(XRT_FEATURE_THREAD)
static int32 zoneWorker(ptr Data)
{
    xcronzone* Z=Data;
    for(unsigned J=0;J<12000;++J){xdatetime D;xcronzonestate S;xtime Values[2];size_t Count;xtime T=INT64_C(63839664000000)+(xtime)J*123456789;
        if(!xcronZoneRetain(Z) || !xcronZoneSplit(Z,T,&D,NULL) || !xcronZoneAt(Z,T,&S,NULL) ||
           xcronZoneResolve(Z,&D,Values,2,&Count,NULL)!=XCRON_ZONE_FOUND ||
           !(Values[0]==T || (Count==2 && Values[1]==T)))return 1;
        xcronZoneRelease(Z);
    }
    xcronZoneRelease(Z);return 0;
}
static void testZoneThreads(void)
{
    xcronzone* Z=zoneCreate("EST5EDT,M3.2.0,M11.1.0");xthread* Threads[4];
    for(unsigned I=0;I<4;++I){testRequire(xcronZoneRetain(Z),"thread zone owner");Threads[I]=xrtThreadCreate(zoneWorker,Z,0);testRequire(Threads[I]!=NULL,"zone worker start");}
    xcronZoneRelease(Z);
    for(unsigned I=0;I<4;++I){testRequire(xrtThreadWait(Threads[I])==XWAIT_OK && !xrtThreadExitCode(Threads[I]),"shared immutable queries and retained lifetime");xrtThreadDestroy(Threads[I]);}
}
#endif
int main(void){testZoneKnown();testZoneRoundtrips();testZoneInvalidAndOom();testZoneArgumentsAndWarm();
#if defined(XRT_FEATURE_THREAD)
testZoneThreads();
#endif
puts("[PASS] timezone POSIX rules: transitions/gaps/folds/endpoints/OOM/ownership");return 0;}
