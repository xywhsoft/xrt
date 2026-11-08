#include "../src/timezone/timezone_internal.h"
static void deliveryCursorBudget(const xcrondeliverycursorissue* I,const xcrondeliverycursorissue* Previous,xcrondeliverybudget B)
{testRequire(I->ChecksExamined>=Previous->ChecksExamined && I->ChecksExamined-Previous->ChecksExamined<=B.Checks && I->OffsetsExamined>=Previous->OffsetsExamined && I->OffsetsExamined-Previous->OffsetsExamined<=B.Offsets && I->RuleYearsExamined>=Previous->RuleYearsExamined && I->RuleYearsExamined-Previous->RuleYearsExamined<=B.RuleYears,"each independent cursor budget bounds actual cumulative work");}
static void deliveryCursorExpect(const xcronzone* Z,const xdatetime* D,xcrondeliverypolicy P,const xtime* Expected,size_t N)
{
    xcrondeliverycursor C,Before;xdatetime Copy=*D;xcrondeliverycursorissue I,Previous={0};xtime Out[258],Original[258];memset(Out,0x5a,sizeof(Out));memcpy(Original,Out,sizeof(Out));size_t Count=777;
    testRequire(xcronDeliveryCursorInit(&C,Z,&Copy,P,&I) && !I.ChecksExamined && !I.OffsetsExamined && !I.RuleYearsExamined && I.Issue.Code==XCRON_DELIVERY_OK,"cursor init does no traversal");memset(&Copy,0xa5,sizeof(Copy));memcpy(&Before,&C,sizeof(C));
    xcrondeliverystatus S=xcronDeliveryCursorStep(&C,Z,(xcrondeliverybudget){0},Out,258,&Count,&I);unsigned Calls=0;
    if(S==XCRON_DELIVERY_PENDING)testRequire(!memcmp(&C,&Before,sizeof(C)) && Count==777 && !memcmp(Out,Original,sizeof(Out)),"all-zero pending budget changes no cursor/output bytes");
    while(S==XCRON_DELIVERY_PENDING){
        xcrondeliverybudget B={1,1,1};Previous=I;S=xcronDeliveryCursorStep(&C,Z,B,Out,258,&Count,&I);deliveryCursorBudget(&I,&Previous,B);testRequire(++Calls<100000,"finite cursor progress");
        if(S==XCRON_DELIVERY_PENDING)testRequire(Count==777 && !memcmp(Out,Original,sizeof(Out)) && !I.Issue.Candidates && I.Issue.Code==XCRON_DELIVERY_OK,"pending does not expose partial candidates/count");
    }
    bool Error=(!N && P.Missing==XCRON_MISSING_REJECT) || (N>1 && P.Fold==XCRON_FOLD_REJECT);size_t Need=P.Fold==XCRON_FOLD_ALL?N:N?1:0;
    testRequire(I.Issue.Candidates==N,"complete cursor multiplicity before selection");
    if(Error){testRequire(S==XCRON_DELIVERY_ERROR && I.Issue.Code==(!N?XCRON_DELIVERY_MISSING:XCRON_DELIVERY_AMBIGUOUS) && Count==777 && !memcmp(Out,Original,sizeof(Out)),"cursor typed policy error/atomicity");xrtClearError();}
    else {
        testRequire(S==(Need?XCRON_DELIVERY_FOUND:XCRON_DELIVERY_NONE) && Count==Need && I.Issue.Code==XCRON_DELIVERY_OK,"cursor status/selected capacity");
        if(Need)testRequire(!memcmp(Out,Expected+(P.Fold==XCRON_FOLD_LAST?N-1:0),Need*sizeof(*Out)),"budgeted cursor agrees with independent earliest/latest/all candidates");
        testRequire(!memcmp(Out+Need,Original+Need,(258-Need)*sizeof(*Out)),"cursor output tail untouched");
        Previous=I;Count=777;S=xcronDeliveryCursorStep(&C,Z,(xcrondeliverybudget){0},NULL,0,&Count,&I);deliveryCursorBudget(&I,&Previous,(xcrondeliverybudget){0});
        testRequire(S==(Need?XCRON_DELIVERY_CAPACITY:XCRON_DELIVERY_NONE) && Count==Need,"terminal cursor capacity retry does no work");
        memcpy(Original,Out,sizeof(Out));Count=777;S=xcronDeliveryCursorStep(&C,Z,(xcrondeliverybudget){0},Out,Need?Need-1:0,&Count,&I);
        testRequire(S==(Need?XCRON_DELIVERY_CAPACITY:XCRON_DELIVERY_NONE) && Count==Need && !memcmp(Out,Original,sizeof(Out)),"terminal short capacity preserves every candidate byte");
    }
    Previous=I;Count=777;S=xcronDeliveryCursorStep(&C,Z,(xcrondeliverybudget){0},Out,258,&Count,&I);deliveryCursorBudget(&I,&Previous,(xcrondeliverybudget){0});
    testRequire(S==(Error?XCRON_DELIVERY_ERROR:Need?XCRON_DELIVERY_FOUND:XCRON_DELIVERY_NONE) && Count==(Error?777:Need),"terminal repeats at zero budget");if(Error)xrtClearError();
    testRequire(xcronDeliveryCursorStep(&C,Z,(xcrondeliverybudget){0},Out,258,&Count,NULL)==S,"optional terminal cursor Issue");if(Error)xrtClearError();
}
static void testDeliveryCursorArguments(void)
{
    xcronzone* Z=deliveryZone("UTC0"),*Other=deliveryZone("UTC0");xdatetime D=deliveryDate(2024,1,1,0,0,0,0);xcrondeliverycursor C,Before;xcrondeliverycursorissue I,Old;memset(&C,0x5a,sizeof(C));memcpy(&Before,&C,sizeof(C));memset(&I,0x5a,sizeof(I));memcpy(&Old,&I,sizeof(I));size_t Count=777;xtime Out[2]={11,22};
    testRequire(!xcronDeliveryCursorInit(&C,Z,&D,(xcrondeliverypolicy){(xcronfoldpolicy)-1,0},&I) && !memcmp(&C,&Before,sizeof(C)) && !memcmp(&I,&Old,sizeof(I)),"invalid cursor policy preserves all outputs");xrtClearError();
    testRequire(!xcronDeliveryCursorInit(&C,Z,(const xdatetime*)&C,(xcrondeliverypolicy){0,0},&I) && !memcmp(&C,&Before,sizeof(C)) && !memcmp(&I,&Old,sizeof(I)),"cursor/input alias");xrtClearError();
    testRequire(xcronDeliveryCursorStep(&C,Z,(xcrondeliverybudget){1,1,1},Out,2,&Count,&I)==XCRON_DELIVERY_ERROR && I.Issue.Code==XCRON_DELIVERY_STATE && Count==777 && Out[0]==11 && !memcmp(&C,&Before,sizeof(C)),"uninitialized cursor typed state error");xrtClearError();
    testRequire(xcronDeliveryCursorInit(&C,Z,&D,(xcrondeliverypolicy){0,0},NULL),"argument fixture init");memcpy(&Before,&C,sizeof(C));memset(&I,0x5a,sizeof(I));memcpy(&Old,&I,sizeof(I));
    testRequire(xcronDeliveryCursorStep(&C,Z,(xcrondeliverybudget){1,1,1},Out,SIZE_MAX,&Count,&I)==XCRON_DELIVERY_ERROR && !memcmp(&I,&Old,sizeof(I)) && !memcmp(&C,&Before,sizeof(C)),"wrapped cursor output span");xrtClearError();
    testRequire(xcronDeliveryCursorStep(&C,Z,(xcrondeliverybudget){1,1,1},(xtime*)C.Storage,1,&Count,&I)==XCRON_DELIVERY_ERROR && !memcmp(&I,&Old,sizeof(I)) && !memcmp(&C,&Before,sizeof(C)),"opaque result storage is not a public output span");xrtClearError();
    testRequire(xcronDeliveryCursorStep(&C,Z,(xcrondeliverybudget){1,1,1},Out,2,(size_t*)Out,&I)==XCRON_DELIVERY_ERROR && Out[0]==11 && !memcmp(&I,&Old,sizeof(I)) && !memcmp(&C,&Before,sizeof(C)),"cursor candidate/count alias");xrtClearError();
    testRequire(xcronDeliveryCursorStep(&Before,Z,(xcrondeliverybudget){1,1,1},Out,2,&Count,&I)==XCRON_DELIVERY_ERROR && I.Issue.Code==XCRON_DELIVERY_STATE && Count==777,"relocated cursor rejected");xrtClearError();
    testRequire(xcronDeliveryCursorStep(&C,Other,(xcrondeliverybudget){1,1,1},Out,2,&Count,&I)==XCRON_DELIVERY_ERROR && I.Issue.Code==XCRON_DELIVERY_STATE && !memcmp(&C,&Before,sizeof(C)),"different exact zone rejected without traversal");xrtClearError();
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"cursor diagnostic OOM");D.Month=13;
    testRequire(!xcronDeliveryCursorInit(&C,Z,&D,(xcrondeliverypolicy){0,0},&I) && I.Issue.Code==XCRON_DELIVERY_ZONE && I.Issue.Zone==XCRON_ZONE_DATE && !memcmp(&C,&Before,sizeof(C)),"cursor date issue survives OOM, init atomic");testRequire(xrtAllocationBudgetLeave(&Scope),"leave cursor OOM");xrtClearError();xrtAllocationBudgetFree(B);xcronZoneRelease(Other);xcronZoneRelease(Z);
}
static void testDeliveryCursorCoverage(void)
{
    tzifblob Blob;tzifkind Types[]={{0,false,"STD"}};tzifBuild(&Blob,'3',NULL,NULL,0,Types,1,"STD0<-00>,59/0,J60/1");xcronzone* Z=NULL;testRequire(xcronZoneCreateTzif((xbytesview){Blob.Data,Blob.Size},&Z,NULL),"leap-only unspecified footer fixture");
    xtime Low=deliveryTime(2097,1,1,0,0,0,0);xdatetime D;testRequire(xrtTimeSplit(Low+(int64)INT32_MAX*1000,&D),"legal offset-window label");xcrondeliverycursor C;testRequire(xcronDeliveryCursorInit(&C,Z,&D,(xcrondeliverypolicy){XCRON_FOLD_FIRST,0},NULL),"unknown footer cursor init");size_t Count=777;xtime Out=11;xcrondeliverycursorissue I,Previous={0};
    testRequire(xcronDeliveryCursorStep(&C,Z,(xcrondeliverybudget){0,10,10},&Out,1,&Count,&I)==XCRON_DELIVERY_PENDING && !I.ChecksExamined && !I.OffsetsExamined && !I.RuleYearsExamined,"needed check dimension can independently block progress");
    testRequire(xcronDeliveryCursorStep(&C,Z,(xcrondeliverybudget){1,0,0},&Out,1,&Count,&I)==XCRON_DELIVERY_PENDING && I.ChecksExamined==1 && !I.RuleYearsExamined,"coverage setup does not hide transition years");
    Previous=I;testRequire(xcronDeliveryCursorStep(&C,Z,(xcrondeliverybudget){1,10,0},&Out,1,&Count,&I)==XCRON_DELIVERY_PENDING && !memcmp(&Previous,&I,sizeof(I)),"zero rule-year budget cannot traverse footer");
    xcrondeliverystatus S=XCRON_DELIVERY_PENDING;unsigned Calls=0;
    while(S==XCRON_DELIVERY_PENDING){Previous=I;S=xcronDeliveryCursorStep(&C,Z,(xcrondeliverybudget){1,1,1},&Out,1,&Count,&I);deliveryCursorBudget(&I,&Previous,(xcrondeliverybudget){1,1,1});testRequire(++Calls<500 && Count==777 && Out==11,"unknown footer yields atomically");}
    printf("[INFO] delivery unknown footer: status=%d code=%d zone=%d checks=%llu offsets=%llu years=%llu\n",S,I.Issue.Code,I.Issue.Zone,(unsigned long long)I.ChecksExamined,(unsigned long long)I.OffsetsExamined,(unsigned long long)I.RuleYearsExamined);
    testRequire(S==XCRON_DELIVERY_ERROR && I.Issue.Code==XCRON_DELIVERY_ZONE && I.Issue.Zone==XCRON_ZONE_UNKNOWN && !I.OffsetsExamined && I.RuleYearsExamined>=8,"global unknown wins before selected candidate, crosses non-leap century");xrtClearError();xcronZoneRelease(Z);
    /* Disable only the constructor's empty-season fast path to prove the raw
     * inherited403-year fallback is actually resumable. No benchmark uses it. */
    Z=deliveryZone("STD0<-00>,J1/0,J1/1");D=deliveryDate(2024,1,1,0,0,0,0);xcronzoneresolution V;xcronZoneResolutionInit(&D,&V);
    uint8 Mode=Z->Mode;Z->Mode=2;xtime Start;testRequire(xcronZoneSecondValue(V.Local-INT32_MAX,D.Millisecond,&Start) && xcronZoneSecondValue(V.Local+INT32_MAX,D.Millisecond,&V.UnknownEnd),"raw footer window");
    xcronZoneWalkInit(Z,Start,&V.Walk);V.Phase=1;V.OffsetsCount=2;
    for(unsigned J=0;J<403;++J)testRequire(xcronZoneResolutionStep(&V,Z,(xcronresolutionbudget){1,0,1},NULL)==XCRON_ZONE_WALK_PENDING && V.RuleYears==J+1,"actual403-year fallback yields each year without truncating completeness");
    testRequire(V.Checks==1 && !V.Offsets && V.Phase==2,"completed fallback does not hide unbudgeted offset work");
    testRequire(xcronZoneResolutionStep(&V,Z,(xcronresolutionbudget){0,1,0},NULL)==XCRON_ZONE_WALK_PENDING && xcronZoneResolutionStep(&V,Z,(xcronresolutionbudget){0,1,0},NULL)==XCRON_ZONE_FOUND && V.Count==1 && V.Offsets==2,"compact no-array resolver retains selected count/first/last");
    Z->Mode=Mode;xcronZoneRelease(Z);
}
static void testDeliveryCursorWarm(void)
{
    xcronzone* Z=deliveryZone("EST5EDT,M3.2.0,M11.1.0");xdatetime D=deliveryDate(2024,11,3,1,30,0,137);xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B),"cursor warm quota");xrtSetErrorInfo(XERR_VALUE,"cursor.ambient",4,"preserve");const xerror* Ambient=xrtGetError();testRequire(xrtAllocationBudgetEnter(B,&Scope),"cursor warm scope");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(true);xrtMemStatsGet(&Before);
#endif
    for(unsigned J=0;J<1024;++J){xcrondeliverycursor C;xcrondeliverycursorissue I;xtime Out[2];size_t Count=777;testRequire(xcronDeliveryCursorInit(&C,Z,&D,(xcrondeliverypolicy){J%3,0},NULL),"warm cursor reset");xcrondeliverystatus S;
        do {S=xcronDeliveryCursorStep(&C,Z,(xcrondeliverybudget){1,1,1},Out,2,&Count,&I);testRequire(xrtGetError()==Ambient,"cursor pending/terminal preserves ambient identity");}while(S==XCRON_DELIVERY_PENDING);
        testRequire(S==XCRON_DELIVERY_FOUND && Count==(J%3?1u:2u) && I.OffsetsExamined==2,"warm resumable exact result");}
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);testRequire(Before.MallocCalls==After.MallocCalls && Before.CallocCalls==After.CallocCalls && Before.ReallocCalls==After.ReallocCalls && Before.BlockAllocCalls==After.BlockAllocCalls,"resumable normal operations request no heap");
#endif
    testRequire(!Scope.Rejections && xrtAllocationBudgetLeave(&Scope),"no hidden cursor allocations");xrtClearError();xrtAllocationBudgetFree(B);xcronZoneRelease(Z);
}
#if defined(XRT_FEATURE_THREAD)
typedef struct deliveryhandoff {xcrondeliverycursor* C;xcronzone* Z;xtime Out[2];size_t Count;xcrondeliverystatus Status;} deliveryhandoff;
static int32 deliveryCursorWorker(ptr Raw)
{deliveryhandoff* H=Raw;do{H->Status=xcronDeliveryCursorStep(H->C,H->Z,(xcrondeliverybudget){1,1,1},H->Out,2,&H->Count,NULL);}while(H->Status==XCRON_DELIVERY_PENDING);return 0;}
static void testDeliveryCursorHandoff(void)
{xcronzone* Z=deliveryZone("EST5EDT,M3.2.0,M11.1.0");xdatetime D=deliveryDate(2024,11,3,1,30,0,137);xcrondeliverycursor C;deliveryhandoff H={.C=&C,.Z=Z};testRequire(xcronDeliveryCursorInit(&C,Z,&D,(xcrondeliverypolicy){0,0},NULL) && xcronDeliveryCursorStep(&C,Z,(xcrondeliverybudget){1,1,1},H.Out,2,&H.Count,NULL)==XCRON_DELIVERY_PENDING,"handoff starts with real pending work");xthread* T=xrtThreadCreate(deliveryCursorWorker,&H,0);testRequire(T && xrtThreadWaitFor(T,15000)==XWAIT_OK && !xrtThreadExitCode(T) && H.Status==XCRON_DELIVERY_FOUND && H.Count==2,"serialized same-address cursor handoff");xrtThreadDestroy(T);xcronZoneRelease(Z);}
#endif
