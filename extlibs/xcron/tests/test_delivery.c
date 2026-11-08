#include "test.h"
#include <xrt/memory.h>
#if defined(XRT_FEATURE_THREAD)
#include <xrt/thread.h>
#endif
#define XCRON_TEST_TZIF_BYTES 8192
#include "tzif_fixture.h"
static xdatetime deliveryDate(int64 Y,int M,int D,int H,int Minute,int S,int Milli)
{return (xdatetime){.Year=Y,.Month=M,.Day=D,.Hour=H,.Minute=Minute,.Second=S,.Millisecond=Milli};}
static xcronzone* deliveryZone(cstr Rule)
{xcronzone* Z=NULL;testRequire(xcronZoneCreatePosix((xstrview){Rule,strlen(Rule)},XCRON_ZONE_TZIF3,&Z,NULL),"delivery fixture rule");return Z;}
static xtime deliveryTime(int64 Y,int M,int D,int H,int Minute,int S,int Milli)
{xtime T;testRequire(xrtDateTime(Y,M,D,H,Minute,S,Milli,&T),"delivery fixture timestamp");return T;}
#include "delivery_cursor_cases.h"
static void deliveryExpect(const xcronzone* Z,const xdatetime* D,const xtime* Expected,size_t N)
{
    for(int F=0;F<4;++F)for(int M=0;M<2;++M){xcrondeliverypolicy P={(xcronfoldpolicy)F,(xcronmissingpolicy)M};xtime Out[258];memset(Out,0x5a,sizeof(Out));xtime Before[258];memcpy(Before,Out,sizeof(Out));size_t Count=777;xcrondeliveryissue I;
        xcrondeliverystatus S=xcronDeliveryResolve(Z,D,P,Out,258,&Count,&I);
        xtime WithoutIssue[258];memcpy(WithoutIssue,Before,sizeof(WithoutIssue));size_t WithoutCount=777;
        testRequire(xcronDeliveryResolve(Z,D,P,WithoutIssue,258,&WithoutCount,NULL)==S && WithoutCount==Count && !memcmp(Out,WithoutIssue,sizeof(Out)),"optional Issue preserves every result and untouched array byte");
        deliveryCursorExpect(Z,D,P,Expected,N);
        bool Missing=!N && M==XCRON_MISSING_REJECT,Ambiguous=N>1 && F==XCRON_FOLD_REJECT;
        if(Missing || Ambiguous){testRequire(S==XCRON_DELIVERY_ERROR && I.Code==(Missing?XCRON_DELIVERY_MISSING:XCRON_DELIVERY_AMBIGUOUS) && I.Candidates==N && Count==777 && !memcmp(Out,Before,sizeof(Out)),"policy rejection preserves count/array and complete typed multiplicity");xrtClearError();continue;}
        size_t Need=F==XCRON_FOLD_ALL?N:N?1:0;
        testRequire(S==(Need?XCRON_DELIVERY_FOUND:XCRON_DELIVERY_NONE) && Count==Need && I.Code==XCRON_DELIVERY_OK && I.Zone==XCRON_ZONE_OK && I.Candidates==N,"selection count/status/complete multiplicity");
        if(Need)testRequire(!memcmp(Out,Expected+(F==XCRON_FOLD_LAST?N-1:0),Need*sizeof(*Out)),"selection uses complete UTC ordering");
        testRequire(!memcmp(Out+Need,Before+Need,(258-Need)*sizeof(*Out)),"unselected tail untouched");
        Count=777;S=xcronDeliveryResolve(Z,D,P,NULL,0,&Count,&I);testRequire(S==(Need?XCRON_DELIVERY_CAPACITY:XCRON_DELIVERY_NONE) && Count==Need && I.Candidates==N && I.Code==(Need?XCRON_DELIVERY_LIMIT:XCRON_DELIVERY_OK),"sizing publishes selected required count, not all multiplicity");
        if(Need){memcpy(Out,Before,sizeof(Out));Count=777;S=xcronDeliveryResolve(Z,D,P,Out,Need-1,&Count,&I);testRequire(S==XCRON_DELIVERY_CAPACITY && Count==Need && !memcmp(Out,Before,sizeof(Out)),"short selected capacity publishes no partial result");}
    }
}
static void testDeliveryGolden(void)
{
    xcronzone* Z=deliveryZone("EST5EDT,M3.2.0,M11.1.0");xdatetime D=deliveryDate(2024,11,3,1,30,0,137);xtime E[]={deliveryTime(2024,11,3,5,30,0,137),deliveryTime(2024,11,3,6,30,0,137)};deliveryExpect(Z,&D,E,2);
    D.Offset=INT32_MAX;D.IsDST=true;D.Weekday=6;D.YearDay=999;deliveryExpect(Z,&D,E,2);
    D=deliveryDate(2024,3,10,2,30,0,0);deliveryExpect(Z,&D,NULL,0);D=deliveryDate(2024,3,10,3,30,0,0);E[0]=deliveryTime(2024,3,10,7,30,0,0);deliveryExpect(Z,&D,E,1);xcronZoneRelease(Z);
    Z=deliveryZone("LHST-10:30LHDT-11,M10.1.0,M4.1.0");D=deliveryDate(2024,4,7,1,45,0,0);E[0]=deliveryTime(2024,4,6,14,45,0,0);E[1]=deliveryTime(2024,4,6,15,15,0,0);deliveryExpect(Z,&D,E,2);xcronZoneRelease(Z);
    Z=deliveryZone("IST-1GMT0,M10.5.0,M3.5.0/1");D=deliveryDate(2024,10,27,1,30,0,0);E[0]=deliveryTime(2024,10,27,0,30,0,0);E[1]=deliveryTime(2024,10,27,1,30,0,0);deliveryExpect(Z,&D,E,2);xcronZoneRelease(Z);
    Z=deliveryZone("STD0DST0,M3.2.0,M11.1.0");D=deliveryDate(2024,11,3,2,30,0,0);E[0]=deliveryTime(2024,11,3,2,30,0,0);deliveryExpect(Z,&D,E,1);xcronZoneRelease(Z);
}
static void testDeliveryMany(void)
{
    /* Independent raw TZif v3:256 offsets/one shared designation. All256 UTC
     * seconds0..255 map to the same local second256. Explicit matching footer
     * keeps future coverage known; no two-fold assumption or unknown shortcut. */
    uint8 B[8192]={0};memcpy(B,"TZif3",5);tzifU32(B+36,1);tzifU32(B+40,1);size_t H=51;memcpy(B+H,"TZif3",5);tzifU32(B+H+32,255);tzifU32(B+H+36,256);tzifU32(B+H+40,4);
    size_t Times=H+44,Indices=Times+255*8,Types=Indices+255,Names=Types+256*6;
    for(unsigned K=0;K<255;++K){tzifTime(B+Times+K*8,K+1,8);B[Indices+K]=(uint8)(K+1);}
    for(unsigned K=0;K<256;++K)tzifU32(B+Types+K*6,256-K);
    memcpy(B+Names,"AAA",4);cstr Footer="\nAAA-0:00:01\n";size_t Size=Names+4+strlen(Footer);memcpy(B+Names+4,Footer,strlen(Footer));
    xcronzone* Z=NULL;testRequire(xcronZoneCreateTzif((xbytesview){B,Size},&Z,NULL),"256-fold binary fixture");memset(B,0,sizeof(B));
    xdatetime D=deliveryDate(1970,1,1,0,4,16,321);xtime E[256];for(unsigned K=0;K<256;++K)E[K]=XRT_TIME_UNIX_EPOCH+K*1000+321;deliveryExpect(Z,&D,E,256);xcronZoneRelease(Z);
}
static void testDeliveryBrute(void)
{
    unsigned Labels=0;
    for(unsigned Case=0;Case<64;++Case){tzifblob B;int64 Times[16];uint8 Indices[16];tzifkind K[]={{0,false,"AAA"},{(int32)(Case%63)+1,false,"BBB"},{-(int32)(Case%61)-1,false,"CCC"}};
        for(unsigned J=0;J<16;++J){Times[J]=(int64)J*16-120;Indices[J]=(uint8)((J*7+Case)%3);}Indices[15]=0;tzifBuild(&B,'3',Times,Indices,16,K,3,"AAA0");
        xcronzone* Z=NULL;testRequire(xcronZoneCreateTzif((xbytesview){B.Data,B.Size},&Z,NULL),"random transition fixture");
        for(int L=-100;L<=100;L+=17){xtime Expected[258];size_t N=0;xtime Nominal=XRT_TIME_UNIX_EPOCH+L*1000+137;xdatetime D;testRequire(xrtTimeSplit(Nominal,&D),"brute nominal label");
            for(int T=-200;T<=200;++T){xtime When=XRT_TIME_UNIX_EPOCH+T*1000+137;xcronzonestate State;testRequire(xcronZoneAt(Z,When,&State,NULL),"brute complete offset state");if((int64)T+State.Offset==L){testRequire(N<258,"brute bounded multiplicity");Expected[N++]=When;}}
            deliveryExpect(Z,&D,Expected,N);++Labels;
        }xcronZoneRelease(Z);
    }printf("[PASS] delivery independent UTC stepping: %u labels, all8 policies, capacity and untouched outputs\n",Labels);
}
static void testDeliveryBoundsAndFailures(void)
{
    xcronzone* Z=deliveryZone("UTC0");xdatetime D;xtime E;
    testRequire(xrtTimeSplit(INT64_MIN,&D),"minimum civil");E=INT64_MIN;deliveryExpect(Z,&D,&E,1);testRequire(xrtTimeSplit(INT64_MAX,&D),"maximum civil");E=INT64_MAX;deliveryExpect(Z,&D,&E,1);
    D=deliveryDate(INT64_MAX,1,1,0,0,0,0);deliveryExpect(Z,&D,NULL,0);D=deliveryDate(2024,1,1,0,0,0,0);
    xcrondeliverypolicy P={XCRON_FOLD_FIRST,XCRON_MISSING_SKIP};xtime Out[3]={11,22,33};size_t Count=777;xcrondeliveryissue I,Before;memset(&I,0x5a,sizeof(I));Before=I;
    testRequire(xcronDeliveryResolve(Z,&D,(xcrondeliverypolicy){(xcronfoldpolicy)-1,0},Out,3,&Count,&I)==XCRON_DELIVERY_ERROR && Count==777 && !memcmp(&I,&Before,sizeof(I)),"bad policy leaves every output");xrtClearError();
    testRequire(xcronDeliveryResolve(Z,&D,P,Out,SIZE_MAX,&Count,&I)==XCRON_DELIVERY_ERROR && Count==777 && Out[0]==11 && !memcmp(&I,&Before,sizeof(I)),"wrapped output span");xrtClearError();
    testRequire(xcronDeliveryResolve(Z,&D,P,Out,3,(size_t*)Out,&I)==XCRON_DELIVERY_ERROR && Out[0]==11 && !memcmp(&I,&Before,sizeof(I)),"array/count alias");xrtClearError();
    xdatetime Copy=D;testRequire(xcronDeliveryResolve(Z,&D,P,(xtime*)&D,1,&Count,&I)==XCRON_DELIVERY_ERROR && !memcmp(&D,&Copy,sizeof(D)) && !memcmp(&I,&Before,sizeof(I)),"local/output alias");xrtClearError();
    testRequire(xcronDeliveryResolve(Z,&D,P,Out,3,&Count,(xcrondeliveryissue*)Z)==XCRON_DELIVERY_ERROR && Count==777,"owner/output alias");xrtClearError();
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"diagnostic OOM");D.Month=13;
    testRequire(xcronDeliveryResolve(Z,&D,P,Out,3,&Count,&I)==XCRON_DELIVERY_ERROR && I.Code==XCRON_DELIVERY_ZONE && I.Zone==XCRON_ZONE_DATE && Count==777 && Out[0]==11,"typed invalid civil despite both diagnostic OOMs");
    testRequire(xrtAllocationBudgetLeave(&Scope),"leave OOM");xrtClearError();xrtAllocationBudgetFree(Budget);xcronZoneRelease(Z);
    tzifblob B;int64 Times[]={0,86400};uint8 Ix[]={1,1};tzifkind K[]={{0,false,"-00"},{0,false,"AAA"}};tzifBuild(&B,'3',Times,Ix,2,K,2,"");testRequire(xcronZoneCreateTzif((xbytesview){B.Data,B.Size},&Z,NULL),"unknown coverage fixture");D=deliveryDate(1970,1,1,12,0,0,0);
    for(int F=0;F<4;++F)for(int M=0;M<2;++M){Count=777;testRequire(xcronDeliveryResolve(Z,&D,(xcrondeliverypolicy){F,M},Out,3,&Count,&I)==XCRON_DELIVERY_ERROR && I.Code==XCRON_DELIVERY_ZONE && I.Zone==XCRON_ZONE_UNKNOWN && Count==777 && Out[0]==11,"selection cannot waive global unknown completeness");xrtClearError();}
    xcronZoneRelease(Z);
    Z=deliveryZone("EST5EDT,M3.2.0,M11.1.0");testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"policy diagnostic OOM");D=deliveryDate(2024,11,3,1,30,0,0);
    testRequire(xcronDeliveryResolve(Z,&D,(xcrondeliverypolicy){XCRON_FOLD_REJECT,0},Out,3,&Count,&I)==XCRON_DELIVERY_ERROR && I.Code==XCRON_DELIVERY_AMBIGUOUS && I.Candidates==2 && Count==777,"typed fold reject under OOM");xrtClearError();D=deliveryDate(2024,3,10,2,30,0,0);
    testRequire(xcronDeliveryResolve(Z,&D,(xcrondeliverypolicy){0,XCRON_MISSING_REJECT},Out,3,&Count,&I)==XCRON_DELIVERY_ERROR && I.Code==XCRON_DELIVERY_MISSING && !I.Candidates && Count==777,"typed missing reject under OOM");testRequire(xrtAllocationBudgetLeave(&Scope),"leave policy OOM");xrtClearError();xrtAllocationBudgetFree(Budget);xcronZoneRelease(Z);
}
static void deliveryNullError(const xcronzone* Z,const xdatetime* D,xtime* Out,size_t Capacity,size_t* Count,xcrondeliveryerror Expected)
{
    testRequire(xcronDeliveryResolve(Z,D,(xcrondeliverypolicy){XCRON_FOLD_ALL,XCRON_MISSING_SKIP},Out,Capacity,Count,NULL)==XCRON_DELIVERY_ERROR &&
        xrtErrorCode(xrtGetError())==(int32)Expected && !strcmp(xrtErrorDomain(xrtGetError()),"xrt.cron.delivery"),"optional Issue preserves delivery argument/value error identity");
    xrtClearError();
}
static void testDeliveryNullIssue(void)
{
    xcronzone* Z=deliveryZone("UTC0");xdatetime D=deliveryDate(2024,1,1,0,0,0,0),Original=D;xtime Out[3]={11,22,33};size_t Count=777;
    deliveryNullError(NULL,&D,Out,3,&Count,XCRON_DELIVERY_ARGUMENT);
    deliveryNullError(Z,NULL,Out,3,&Count,XCRON_DELIVERY_ARGUMENT);
    deliveryNullError(Z,&D,NULL,1,&Count,XCRON_DELIVERY_ARGUMENT);
    deliveryNullError(Z,&D,Out,3,NULL,XCRON_DELIVERY_ARGUMENT);
    deliveryNullError(Z,&D,Out,SIZE_MAX,&Count,XCRON_DELIVERY_ARGUMENT);
    deliveryNullError(Z,&D,(xtime*)(uintptr_t)(UINTPTR_MAX-3),2,&Count,XCRON_DELIVERY_ARGUMENT);
    deliveryNullError(Z,(const xdatetime*)(uintptr_t)(UINTPTR_MAX-3),Out,3,&Count,XCRON_DELIVERY_ARGUMENT);
    deliveryNullError(Z,&D,Out,3,(size_t*)Out,XCRON_DELIVERY_ARGUMENT);
    deliveryNullError(Z,&D,(xtime*)&D,1,&Count,XCRON_DELIVERY_ARGUMENT);
    deliveryNullError(Z,&D,NULL,0,(size_t*)&D,XCRON_DELIVERY_ARGUMENT);
    deliveryNullError(Z,&D,(xtime*)Z,1,&Count,XCRON_DELIVERY_ARGUMENT);
    deliveryNullError(Z,&D,Out,3,(size_t*)Z,XCRON_DELIVERY_ARGUMENT);
    testRequire(Count==777 && Out[0]==11 && Out[1]==22 && Out[2]==33 && !memcmp(&D,&Original,sizeof(D)),"delegated argument validation preserves every caller byte");
    D.Month=13;deliveryNullError(Z,&D,Out,3,&Count,XCRON_DELIVERY_ZONE);D=Original;
    xallocationbudget* B=NULL;xallocationscope S={0};testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&S),"optional Issue diagnostic OOM");
    testRequire(xcronDeliveryResolve(Z,NULL,(xcrondeliverypolicy){0,0},Out,3,&Count,NULL)==XCRON_DELIVERY_ERROR && Count==777 && Out[0]==11,"delegated structural error remains atomic under diagnostic OOM");
    testRequire(xrtAllocationBudgetLeave(&S),"leave optional Issue OOM");xrtClearError();xrtAllocationBudgetFree(B);xcronZoneRelease(Z);
    tzifblob Blob;int64 Times[]={0};uint8 Index[]={0};tzifkind Types[]={{0,false,"UTC"}};tzifBuild(&Blob,'3',Times,Index,1,Types,1,"UTC0");
    testRequire(xcronZoneCreateTzif((xbytesview){Blob.Data,Blob.Size},&Z,NULL),"separately owned footer fixture");xcronzonestate State;
    testRequire(xcronZoneAt(Z,XRT_TIME_UNIX_EPOCH,&State,NULL) && State.Abbreviation.Size==3,"footer abbreviation owner");
    deliveryNullError(Z,&D,(xtime*)State.Abbreviation.Data,1,&Count,XCRON_DELIVERY_ARGUMENT);
    deliveryNullError(Z,&D,Out,3,(size_t*)State.Abbreviation.Data,XCRON_DELIVERY_ARGUMENT);
    testRequire(xcronZoneAt(Z,XRT_TIME_UNIX_EPOCH,&State,NULL) && State.Abbreviation.Size==3 && !memcmp(State.Abbreviation.Data,"UTC",3) && Count==777,"footer graph remains immutable");xcronZoneRelease(Z);
    int64 UTimes[]={0,86400};uint8 UIndex[]={1,1};tzifkind Unknown[]={{0,false,"-00"},{0,false,"AAA"}};tzifBuild(&Blob,'3',UTimes,UIndex,2,Unknown,2,"");
    testRequire(xcronZoneCreateTzif((xbytesview){Blob.Data,Blob.Size},&Z,NULL),"optional Issue unknown fixture");D=deliveryDate(1970,1,1,12,0,0,0);
    deliveryNullError(Z,&D,Out,3,&Count,XCRON_DELIVERY_ZONE);testRequire(Count==777 && Out[0]==11,"unknown remains an atomic error with no Issue");xcronZoneRelease(Z);
}
static void testDeliveryWarm(void)
{
    xcronzone* Z=deliveryZone("EST5EDT,M3.2.0,M11.1.0");xdatetime Fold=deliveryDate(2024,11,3,1,30,0,0),Gap=deliveryDate(2024,3,10,2,30,0,0);xallocationbudget* B=NULL;xallocationscope S={0};testRequire(xrtAllocationBudgetCreate(1,&B),"warm quota");xrtSetErrorInfo(XERR_VALUE,"delivery.ambient",7,"preserve");const xerror* Ambient=xrtGetError();testRequire(xrtAllocationBudgetEnter(B,&S),"warm enter");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(true);xrtMemStatsGet(&Before);
#endif
    for(unsigned J=0;J<10000;++J){xtime Out[2];size_t N;xcrondeliveryissue I;int F=J%3;xcrondeliveryissue* Issue=J%2?&I:NULL;
        testRequire(xcronDeliveryResolve(Z,&Fold,(xcrondeliverypolicy){F,0},Out,2,&N,Issue)==XCRON_DELIVERY_FOUND && N==(F?1u:2u) && xrtGetError()==Ambient,"warmed selected result/error");
        testRequire(xcronDeliveryResolve(Z,&Gap,(xcrondeliverypolicy){F,0},NULL,0,&N,Issue)==XCRON_DELIVERY_NONE && !N && xrtGetError()==Ambient,"warmed missing skip/error");
        testRequire(xcronDeliveryResolve(Z,&Fold,(xcrondeliverypolicy){F,0},NULL,0,&N,Issue)==XCRON_DELIVERY_CAPACITY && N==(F?1u:2u) && xrtGetError()==Ambient,"warmed selected capacity/error");
    }
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);testRequire(Before.MallocCalls==After.MallocCalls && Before.CallocCalls==After.CallocCalls && Before.ReallocCalls==After.ReallocCalls && Before.BlockAllocCalls==After.BlockAllocCalls,"normal selection requests no heap");
#endif
    testRequire(!S.Rejections && xrtAllocationBudgetLeave(&S),"no hidden quota failures");xrtClearError();xrtAllocationBudgetFree(B);xcronZoneRelease(Z);
}
#if defined(XRT_FEATURE_THREAD)
static int32 deliveryReader(ptr Raw)
{const xcronzone* Z=Raw;xdatetime D=deliveryDate(2024,11,3,1,30,0,137);for(unsigned J=0;J<4000;++J){xtime T[2];size_t N;testRequire(xcronDeliveryResolve(Z,&D,(xcrondeliverypolicy){J%3,0},T,2,&N,NULL)==XCRON_DELIVERY_FOUND && N==(J%3?1u:2u),"concurrent immutable selection");}xcronZoneRelease(Z);return 0;}
static void testDeliveryThreads(void)
{xcronzone* Z=deliveryZone("EST5EDT,M3.2.0,M11.1.0");xthread* Threads[4];for(unsigned J=0;J<4;++J){testRequire(xcronZoneRetain(Z),"reader owner");Threads[J]=xrtThreadCreate(deliveryReader,Z,0);testRequire(Threads[J]!=NULL,"reader start");}xcronZoneRelease(Z);for(unsigned J=0;J<4;++J){testRequire(xrtThreadWaitFor(Threads[J],15000)==XWAIT_OK && !xrtThreadExitCode(Threads[J]),"reader physical join");xrtThreadDestroy(Threads[J]);}}
#endif
int main(void)
{testDeliveryGolden();testDeliveryMany();testDeliveryBrute();testDeliveryBoundsAndFailures();testDeliveryNullIssue();testDeliveryWarm();testDeliveryCursorArguments();testDeliveryCursorCoverage();testDeliveryCursorWarm();
#if defined(XRT_FEATURE_THREAD)
testDeliveryThreads();testDeliveryCursorHandoff();
#endif
puts("[PASS] nominal label selection: complete256-fold/first/last/reject/missing/unknown/capacity/full-int64/aliases/OOM/zero-heap/concurrency");return 0;}
