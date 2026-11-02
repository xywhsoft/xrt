#include "test.h"
#include <limits.h>
#if defined(XRT_FEATURE_THREAD)
#include <xrt/thread.h>
#endif
#include "tzif_fixture.h"
static xcronzone* tzifCreate(const tzifblob* B)
{xcronzone* Z=NULL;testRequire(xcronZoneCreateTzif((xbytesview){B->Data,B->Size},&Z,NULL),"valid TZif fixture");return Z;}
static void tzifFold(tzifblob* B)
{const int64 T[]={0,3600,7200};const uint8 I[]={1,2,2};const tzifkind K[]={{7200,false,"AAA"},{3600,false,"BBB"},{0,false,"CCC"}};tzifBuild(B,'3',T,I,3,K,3,"CCC0");}
static void tzifBad(tzifblob* B,xcronzoneerror Code)
{xcronzone* Z=(xcronzone*)(uintptr_t)1;xcronzoneissue I;testRequire(!xcronZoneCreateTzif((xbytesview){B->Data,B->Size},&Z,&I) && Z==(xcronzone*)(uintptr_t)1 && I.Code==Code && I.Offset<=B->Size,"TZif rejection, typed issue and factory atomicity");xrtClearError();}
static void testTzifKnown(void)
{
    tzifblob B;tzifFold(&B);xcronzone* Z=tzifCreate(&B);memset(B.Data,'?',B.Size);
    xdatetime Label={.Year=1970,.Month=1,.Day=1,.Hour=1,.Minute=30};xtime V[3]={11,22,33};size_t Count=99;xcronzoneissue I;
    testRequire(xcronZoneResolve(Z,&Label,V,2,&Count,&I)==XCRON_ZONE_CAPACITY && Count==3 && V[0]==11,"more than two candidates and capacity atomicity");
    testRequire(xcronZoneResolve(Z,&Label,V,3,&Count,NULL)==XCRON_ZONE_FOUND && Count==3 && V[0]==XRT_TIME_UNIX_EPOCH-1800000 && V[1]==XRT_TIME_UNIX_EPOCH+1800000 && V[2]==XRT_TIME_UNIX_EPOCH+5400000,"all three sorted fold instants");
    xcronzonetransition N;testRequire(xcronZoneNextTransition(Z,XRT_TIME_UNIX_EPOCH-1,&N,NULL)==XCRON_ZONE_FOUND && N.When==XRT_TIME_UNIX_EPOCH && N.Before.Offset==7200 && N.After.Offset==3600,"historical next transition");
    testRequire(xcronZoneNextTransition(Z,XRT_TIME_UNIX_EPOCH+3600000,&N,NULL)==XCRON_ZONE_NONE,"redundant final transition is prefiltered");
    xcronzonestate S;testRequire(xcronZoneAt(Z,INT64_MAX,&S,NULL) && S.Offset==0 && S.Abbreviation.Size==3 && !memcmp(S.Abbreviation.Data,"CCC",3),"owned immutable future footer after mutable input gone");
    I=(xcronzoneissue){XCRON_ZONE_DATE,123,456};xcronzoneissue Old=I;testRequire(!xcronZoneAt(Z,0,(xcronzonestate*)S.Abbreviation.Data,&I) && !memcmp(&I,&Old,sizeof(I)),"separately owned footer output alias rejected");xrtClearError();xcronZoneRelease(Z);
    const int64 Times[]={INT64_MIN,-1,INT64_MAX};const uint8 Indices[]={0,1,2};const tzifkind Types[]={{0,false,"AAA"},{3600,false,"BBB"},{0,false,"CCC"}};
    tzifBuild(&B,'4',Times,Indices,3,Types,3,"CCC0");Z=tzifCreate(&B);
    testRequire(xcronZoneAt(Z,INT64_MIN,&S,NULL) && S.Offset==0 && xcronZoneAt(Z,INT64_MAX,&S,NULL) && S.Offset==3600,"full int64 Unix transition seconds without millisecond overflow");
    testRequire(xcronZoneNextTransition(Z,XRT_TIME_UNIX_EPOCH,&N,NULL)==XCRON_ZONE_NONE,"out-of-timeline transition never overflows");xcronZoneRelease(Z);
    const tzifkind Huge[]={{INT32_MAX,false,""}};tzifBuild(&B,0,NULL,NULL,0,Huge,1,"");Z=tzifCreate(&B);
    const xtime Instants[]={INT64_MIN,INT64_MAX,-1,0,XRT_TIME_UNIX_EPOCH};
    for(unsigned J=0;J<sizeof(Instants)/sizeof(*Instants);++J){xdatetime D;testRequire(xcronZoneSplit(Z,Instants[J],&D,NULL) && D.Offset==INT32_MAX && xcronZoneResolve(Z,&D,V,3,&Count,NULL)==XCRON_ZONE_FOUND && Count==1 && V[0]==Instants[J],"legal extreme int32 offset full-range roundtrip");}
    xcronZoneRelease(Z);
    const tzifkind Numeric[]={{20700,false,"A B"}};tzifBuild(&B,0,NULL,NULL,0,Numeric,1,"");Z=tzifCreate(&B);testRequire(xcronZoneAt(Z,0,&S,NULL) && S.Abbreviation.Size==5 && !memcmp(S.Abbreviation.Data,"+0545",5),"unsafe designation numeric fallback");xcronZoneRelease(Z);
    const int64 SuffixTimes[]={0,1};const uint8 SuffixIndices[]={1,1};const tzifkind Suffix[]={{0,false,"ABCD"},{0,false,"BCD"}};
    tzifBuild(&B,'3',SuffixTimes,SuffixIndices,2,Suffix,2,"BCD0");B.Data[B.Types+11]=1;Z=tzifCreate(&B);
    testRequire(xcronZoneAt(Z,XRT_TIME_UNIX_EPOCH,&S,NULL) && S.Abbreviation.Size==3 && !memcmp(S.Abbreviation.Data,"BCD",3) &&
        xcronZoneNextTransition(Z,XRT_TIME_UNIX_EPOCH-1,&N,NULL)==XCRON_ZONE_FOUND && N.Before.Offset==N.After.Offset && N.Before.Abbreviation.Size==4 && N.After.Abbreviation.Size==3,"suffix-shared names and name-only effective transition");xcronZoneRelease(Z);
}
static void testTzifUnknown(void)
{
    tzifblob B;const int64 Times[]={0,86400};const uint8 Indices[]={1,1};const tzifkind Types[]={{0,false,"-00"},{0,false,"AAA"}};
    tzifBuild(&B,'2',Times,Indices,2,Types,2,"");xcronzone* Z=tzifCreate(&B);xcronzonestate S={.Offset=123};xcronzoneissue I;
    testRequire(!xcronZoneAt(Z,XRT_TIME_UNIX_EPOCH-1,&S,&I) && I.Code==XCRON_ZONE_UNKNOWN && S.Offset==123,"truncated past is unknown, never a guessed offset");xrtClearError();
    testRequire(xcronZoneAt(Z,XRT_TIME_UNIX_EPOCH,&S,NULL) && S.Offset==0,"start of known interval inclusive");
    xdatetime D={.Year=11};testRequire(!xcronZoneSplit(Z,XRT_TIME_UNIX_EPOCH+XRT_TIME_DAY,&D,&I) && D.Year==11 && I.Code==XCRON_ZONE_UNKNOWN,"last transition without footer ends known interval");xrtClearError();
    xcronzonetransition N;testRequire(xcronZoneNextTransition(Z,XRT_TIME_UNIX_EPOCH-1,&N,NULL)==XCRON_ZONE_FOUND && N.Before.Offset==XCRON_ZONE_OFFSET_UNKNOWN && N.After.Offset==0,"unknown-to-known boundary is observable");
    I=(xcronzoneissue){XCRON_ZONE_DATE,123,456};xcronzoneissue Old=I;
    testRequire(!xcronZoneAt(Z,XRT_TIME_UNIX_EPOCH,(xcronzonestate*)N.Before.Abbreviation.Data,&I) && !memcmp(&I,&Old,sizeof(I)),"unknown designation is owned and output alias protected");xrtClearError();
    testRequire(xcronZoneNextTransition(Z,XRT_TIME_UNIX_EPOCH,&N,NULL)==XCRON_ZONE_FOUND && N.When==XRT_TIME_UNIX_EPOCH+XRT_TIME_DAY && N.After.Offset==XCRON_ZONE_OFFSET_UNKNOWN,"known-to-unknown boundary is observable");
    D=(xdatetime){.Year=1970,.Month=1,.Day=1,.Hour=12};xtime V[3]={11,22,33};size_t Count=99;
    testRequire(xcronZoneResolve(Z,&D,V,3,&Count,&I)==XCRON_ZONE_ERROR && I.Code==XCRON_ZONE_UNKNOWN && Count==99 && V[0]==11,"uncertain resolution cannot publish an incomplete candidate list");xrtClearError();
    D.Year=INT64_MAX;testRequire(xcronZoneResolve(Z,&D,V,3,&Count,NULL)==XCRON_ZONE_NONE && !Count,"outside all possible offsets is NONE even for truncated data");xcronZoneRelease(Z);
    const tzifkind Unknown[]={{0,false,"-00"}};tzifBuild(&B,'3',NULL,NULL,0,Unknown,1,"<-00>0");Z=tzifCreate(&B);testRequire(!xcronZoneAt(Z,0,&S,&I) && I.Code==XCRON_ZONE_UNKNOWN,"unspecified footer remains unknown");xrtClearError();xcronZoneRelease(Z);
}
static uint64 TzifRandom=UINT64_C(0x5b91cf9ec142a3d7);
static uint64 tzifRandom(void){TzifRandom^=TzifRandom<<13;TzifRandom^=TzifRandom>>7;TzifRandom^=TzifRandom<<17;return TzifRandom;}
static void testTzifInvalid(void)
{
    tzifblob B,Original;tzifFold(&Original);
    for(size_t J=0;J<Original.Size;++J){B=Original;B.Size=J;tzifBad(&B,XCRON_ZONE_TZIF);}
    B=Original;B.Data[4]='9';tzifBad(&B,XCRON_ZONE_TZIF);
    B=Original;B.Data[B.Header+4]='2';tzifBad(&B,XCRON_ZONE_TZIF);
    B=Original;tzifU32(B.Data+B.Header+32,UINT32_MAX);tzifBad(&B,XCRON_ZONE_LIMIT);
    B=Original;B.Data[B.Indices]=3;tzifBad(&B,XCRON_ZONE_TZIF);
    B=Original;memcpy(B.Data+B.Times+8,B.Data+B.Times,8);tzifBad(&B,XCRON_ZONE_TZIF);
    B=Original;tzifU32(B.Data+B.Types,UINT32_C(0x80000000));tzifBad(&B,XCRON_ZONE_TZIF);
    B=Original;B.Data[B.Types+4]=2;tzifBad(&B,XCRON_ZONE_TZIF);
    B=Original;B.Data[B.Types+5]=255;tzifBad(&B,XCRON_ZONE_TZIF);
    B=Original;B.Data[B.End-1]='X';tzifBad(&B,XCRON_ZONE_TZIF);
    B=Original;B.Data[B.End+1]='D';tzifBad(&B,XCRON_ZONE_TZIF);
    B=Original;B.Data[B.End+1]=0;tzifBad(&B,XCRON_ZONE_TZIF);
    B=Original;B.Data[B.Header+20+3]=1;tzifBad(&B,XCRON_ZONE_TZIF);
    /* Legacy block is skipped except for locating the authoritative 64-bit block. */
    B=Original;B.Data[48]=255;xcronzone* Z=tzifCreate(&B);xcronZoneRelease(Z);
    /* A valid leap-aware table is explicitly unsupported in XRT's non-leap scale. */
    const tzifkind UTC[]={{0,false,"UTC"}};tzifBuild(&B,'4',NULL,NULL,0,UTC,1,"");
    tzifU32(B.Data+B.Header+28,1);tzifTime(B.Data+B.End,78796800,8);tzifU32(B.Data+B.End+8,1);B.Size=B.End+12;B.Data[B.Size++]='\n';B.Data[B.Size++]='\n';tzifBad(&B,XCRON_ZONE_SCALE);
    B.Data[B.End+7]--;tzifBad(&B,XCRON_ZONE_TZIF);
    tzifBuild(&B,'3',NULL,NULL,0,UTC,1,"UTC0");memmove(B.Data+B.End+2,B.Data+B.End,B.Size-B.End);B.Data[B.End]=1;B.Data[B.End+1]=1;B.Size+=2;
    tzifU32(B.Data+B.Header+20,1);tzifU32(B.Data+B.Header+24,1);Z=tzifCreate(&B);xcronZoneRelease(Z);
    B.Data[B.End]=0;tzifBad(&B,XCRON_ZONE_TZIF);B.Data[B.End]=2;tzifBad(&B,XCRON_ZONE_TZIF);
    for(unsigned J=0;J<7000;++J){B=Original;if(J&1){for(unsigned K=0;K<1+J%4;++K)B.Data[tzifRandom()%B.Size]^=(uint8)(1u<<(tzifRandom()%8));}else {B.Size=(size_t)(tzifRandom()%sizeof(B.Data));for(size_t K=0;K<B.Size;++K)B.Data[K]=(uint8)tzifRandom();}
        Z=(xcronzone*)(uintptr_t)1;xcronzoneissue I;
        if(xcronZoneCreateTzif((xbytesview){B.Data,B.Size},&Z,&I))xcronZoneRelease(Z);
        else testRequire(Z==(xcronzone*)(uintptr_t)1 && I.Code!=XCRON_ZONE_OK && I.Offset<=B.Size,"mutated/truncated parser output invariant");
        xrtClearError();
    }
}
static void testTzifOomAndWarm(void)
{
    tzifblob B;tzifFold(&B);xcronzone* Z=tzifCreate(&B);xallocationbudget* Budget=NULL;xallocationscope Scope={0};
    testRequire(xrtAllocationBudgetCreate(128,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"TZif partial factory OOM scope");
    xcronzone* Out=(xcronzone*)(uintptr_t)1;xcronzoneissue I;
    testRequire(!xcronZoneCreateTzif((xbytesview){B.Data,B.Size},&Out,&I) && I.Code==XCRON_ZONE_MEMORY && Out==(xcronzone*)(uintptr_t)1,"history OOM after footer allocation rolls back owner");
    testRequire(xrtAllocationBudgetLeave(&Scope),"leave TZif budget");xrtClearError();xallocationbudgetinfo Info={.Size=sizeof(Info)};
    testRequire(xrtAllocationBudgetInfo(Budget,&Info) && !Info.LiveBytes && !Info.LiveCount,"partial footer factory owns no leaked allocation");xrtAllocationBudgetFree(Budget);
    xrtSetErrorInfo(XERR_STATE,"sentinel",9,"preserve");const xerror* Ambient=xrtGetError();
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats A,C;xrtMemStatsEnable(true);xrtMemStatsGet(&A);
#endif
    for(unsigned J=0;J<10000;++J){xcronzonestate S;xdatetime D;xcronzonetransition N;xtime V[258];size_t Count;
        testRequire(xcronZoneAt(Z,XRT_TIME_UNIX_EPOCH+1800000,&S,NULL) && xcronZoneSplit(Z,XRT_TIME_UNIX_EPOCH+1800000,&D,NULL) &&
            xcronZoneResolve(Z,&D,V,258,&Count,NULL)==XCRON_ZONE_FOUND && Count==3 && xcronZoneNextTransition(Z,XRT_TIME_UNIX_EPOCH-1,&N,NULL)==XCRON_ZONE_FOUND && xrtGetError()==Ambient,"warm history queries preserve error identity");}
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&C);xrtMemStatsEnable(false);testRequire(A.MallocCalls==C.MallocCalls && A.CallocCalls==C.CallocCalls && A.ReallocCalls==C.ReallocCalls && A.BlockAllocCalls==C.BlockAllocCalls,"warm history queries allocated");
#endif
    xcronZoneRelease(Z);xrtClearError();
}
#if defined(XRT_FEATURE_THREAD)
static int32 tzifWorker(ptr Data)
{
    xcronzone* Z=Data;
    for(unsigned J=0;J<12000;++J){xtime T=XRT_TIME_UNIX_EPOCH+(xtime)(J%7200)*1000,V[258];xdatetime D;size_t Count;bool Found=false;
        if(!xcronZoneRetain(Z) || !xcronZoneSplit(Z,T,&D,NULL) || xcronZoneResolve(Z,&D,V,258,&Count,NULL)!=XCRON_ZONE_FOUND)return 1;
        for(size_t K=0;K<Count;++K)if(V[K]==T)Found=true;
        xcronZoneRelease(Z);if(!Found)return 2;
    }
    xcronZoneRelease(Z);return 0;
}
static void testTzifThreads(void)
{
    tzifblob B;tzifFold(&B);xcronzone* Z=tzifCreate(&B);xthread* Threads[4];
    for(unsigned I=0;I<4;++I){testRequire(xcronZoneRetain(Z),"retained history worker");Threads[I]=xrtThreadCreate(tzifWorker,Z,0);testRequire(Threads[I]!=NULL,"start history worker");}
    xcronZoneRelease(Z);
    for(unsigned I=0;I<4;++I){testRequire(xrtThreadWait(Threads[I])==XWAIT_OK && !xrtThreadExitCode(Threads[I]),"immutable history worker and final footer retirement");xrtThreadDestroy(Threads[I]);}
}
#endif
int main(void){testTzifKnown();testTzifUnknown();testTzifInvalid();testTzifOomAndWarm();
#if defined(XRT_FEATURE_THREAD)
testTzifThreads();
#endif
puts("[PASS] TZif: history/footer/triple folds/unknown/full offsets/endpoints/mutations/OOM");return 0;}
