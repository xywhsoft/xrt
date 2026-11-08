#include "../src/timezone/timezone_internal.h"
static void zonedCursorExpect(const xcronexpression* E,const xcronzone* Z,xtime Begin,xtime End,uint32 Flags,xcronzonedstatus Expected,xtime Value)
{
    xcronzonedcursor C;xcronzonedcursorissue I,Previous={0};xtime Next=777;
    testRequire(xcronZonedCursorInit(&C,E,Z,Begin,End,Flags,&I) && !I.SegmentsExamined && !I.MonthsExamined && !I.RuleYearsExamined,"cursor init does no traversal");
    xcronzonedstatus Status;unsigned Calls=0;
    do{
        Previous=I;xcronzonedbudget B={1+Calls%3,1+Calls%7,1+Calls%2};
        Status=xcronZonedCursorStep(&C,Z,B,&Next,&I);++Calls;
        testRequire(Calls<100000 && I.SegmentsExamined-Previous.SegmentsExamined<=B.Segments &&
            I.MonthsExamined-Previous.MonthsExamined<=B.Months && I.RuleYearsExamined-Previous.RuleYearsExamined<=B.RuleYears,"all three traversal budgets hold on each resume");
        if(Status==XCRON_ZONED_PENDING)testRequire(Next==777 && I.Code==XCRON_ZONED_OK,"pending preserves result and typed OK");
    }while(Status==XCRON_ZONED_PENDING);
    testRequire(Status==Expected && I.Code==XCRON_ZONED_OK && I.Field==XCRON_FIELD_NONE && I.Zone==XCRON_ZONE_OK && Next==(Expected==XCRON_ZONED_FOUND?Value:777),"cursor matches independent earliest/status/output oracle");
    xcronzonedcursor Before=C;Previous=I;Next=999;
    testRequire(xcronZonedCursorStep(&C,Z,(xcronzonedbudget){1,1,1},&Next,&I)==Expected &&
        !memcmp(&Before,&C,sizeof(C)) && !memcmp(&I,&Previous,sizeof(I)) && Next==(Expected==XCRON_ZONED_FOUND?Value:999),"terminal cursor reads are bytewise idempotent");
}
static bool zonedTransitionSame(const xcronzonetransition* A,const xcronzonetransition* B)
{
    return A->When==B->When && A->Before.Offset==B->Before.Offset && A->After.Offset==B->After.Offset &&
        A->Before.IsDST==B->Before.IsDST && A->After.IsDST==B->After.IsDST &&
        A->Before.Abbreviation.Size==B->Before.Abbreviation.Size && A->After.Abbreviation.Size==B->After.Abbreviation.Size &&
        !memcmp(A->Before.Abbreviation.Data,B->Before.Abbreviation.Data,A->Before.Abbreviation.Size) &&
        !memcmp(A->After.Abbreviation.Data,B->After.Abbreviation.Data,A->After.Abbreviation.Size);
}
static void testZonedRuleWalk(void)
{
    const cstr Rules[]={"UTC0","EST5EDT,M3.2.0,M11.1.0","STD0DST,59/0,J60/1","STD0DST,J1/0,J1/1","AAA0BBB0,M3.2.0,M11.1.0","STD-24:59:59DST,M3.2.0/-167:59:59,M11.1.0/167:59:59"};
    for(unsigned R=0;R<6;++R){xcronzone* Z=zonedZone(Rules[R]);
        for(unsigned J=0;J<100;++J){xtime T=J==0?INT64_MIN:J==1?INT64_MAX:J==2?zonedTime(2096,3,2,0,0,0,0):(xtime)(UINT64_C(9126711198736789)*(J+1));
            xcronzonetransition Expected;xcronzonestatus Status=xcronZoneNextTransition(Z,T,&Expected,NULL);xcronzonewalk W;xcronZoneWalkInit(Z,T,&W);
            testRequire(!W.Years,"rule walk init does not scan years");unsigned Calls=0;
            while(W.Status==XCRON_ZONE_WALK_PENDING){uint32 Before=W.Years;xcronZoneWalkStep(&W,1+J%5);++Calls;
                testRequire(Calls<=403 && W.Years-Before<=1+J%5,"private rule-year walk resumes inside its budget");}
            testRequire(W.Status==Status && (Status!=XCRON_ZONE_FOUND || zonedTransitionSame(&W.Result,&Expected)),"bounded rule walk agrees with specialized synchronous transition algorithm");
        }xcronZoneRelease(Z);
    }
    /* The factory recognizes this coincident season as constant. Exercise the
     * general engine's independent 403-year completeness fallback, exclusively. */
    xcronzone* Z=zonedZone("STD0DST,J1/0,J1/1");uint8 Mode=Z->Mode;Z->Mode=2;
    xcronzonewalk W;xcronZoneWalkInit(Z,zonedTime(2000,1,1,0,0,0,0),&W);
    for(unsigned J=0;J<403;++J)testRequire(xcronZoneWalkStep(&W,1)==(J==402?XCRON_ZONE_NONE:XCRON_ZONE_WALK_PENDING) && W.Years==J+1,"complete fallback yields after every actual rule year");
    Z->Mode=Mode;xcronZoneRelease(Z);
}
static void testZonedCursorErrors(void)
{
    testRequire(sizeof(xcronzonedcursor)==512 && sizeof(xcronzonedissue)==24 && sizeof(xcronzonedcursorissue)==32,"existing/new cursor ABI sizes");
    xcronzone* Z=zonedZone("UTC0"),*Other=zonedZone("UTC0");xcronexpression E=zonedExpr("0 0 0 29 FEB */7",XCRON_SECONDS6),Saved=E;
    xtime Begin=zonedTime(2024,3,1,0,0,0,0),End=zonedTime(2040,1,1,0,0,0,0),Expected=zonedTime(2032,2,29,0,0,0,0),Next=777;
    xcronzonedcursor C,Before,Copy;xcronzonedcursorissue I,Old;uint32 Refs=xrtAtomic32Load(&Z->Refs,XMEMORY_RELAXED);
    testRequire(xcronZonedCursorInit(&C,&E,Z,Begin,End,0,&I),"snapshot zoned cursor");E.Fields[0]=0;Before=C;Copy=C;
    testRequire(xcronZonedCursorStep(&Copy,Z,(xcronzonedbudget){1,1,1},&Next,&I)==XCRON_ZONED_ERROR && I.Code==XCRON_ZONED_STATE && !memcmp(&Before,&C,sizeof(C)) && Next==777,"relocated context rejected");xrtClearError();
    testRequire(xcronZonedCursorStep(&C,Other,(xcronzonedbudget){1,1,1},&Next,&I)==XCRON_ZONED_ERROR && I.Code==XCRON_ZONED_STATE && !memcmp(&Before,&C,sizeof(C)),"same rules in a different live owner are not the same borrowed identity");xrtClearError();
    for(unsigned J=0;J<3;++J){xcronzonedbudget B={1,1,1};if(J==0)B.Segments=0;else if(J==1)B.Months=0;else B.RuleYears=0;
        testRequire(xcronZonedCursorStep(&C,Z,B,&Next,&I)==XCRON_ZONED_ERROR && I.Code==XCRON_ZONED_ARGUMENT && !memcmp(&Before,&C,sizeof(C)) && Next==777,"each zero budget is atomic argument failure");xrtClearError();}
    Old=I;
    testRequire(xcronZonedCursorStep(&C,Z,(xcronzonedbudget){1,1,1},(xtime*)&C,&I)==XCRON_ZONED_ERROR && !memcmp(&Old,&I,sizeof(I)) && !memcmp(&Before,&C,sizeof(C)),"cursor/result alias preserves every output");xrtClearError();
    xcronzonestate State;testRequire(xcronZoneAt(Z,Begin,&State,NULL),"name alias fixture");
    testRequire(xcronZonedCursorStep(&C,Z,(xcronzonedbudget){1,1,1},(xtime*)State.Abbreviation.Data,&I)==XCRON_ZONED_ERROR && !memcmp(&I,&Old,sizeof(I)),"borrowed zone-name output alias rejected");xrtClearError();
    testRequire(!xcronZonedCursorInit(&C,&Saved,Z,End,Begin,0,&I) && I.Code==XCRON_ZONED_WINDOW && !memcmp(&C,&Before,sizeof(C)),"failed reset preserves prior live cursor");xrtClearError();
    testRequire(!xcronZonedCursorInit((xcronzonedcursor*)(uintptr_t)(UINTPTR_MAX-128),&Saved,Z,Begin,End,0,&I) && I.Code==XCRON_ZONED_WINDOW,"wrapped init span preserves issue");xrtClearError();
    xcronzonedstatus S;do{S=xcronZonedCursorStep(&C,Z,(xcronzonedbudget){1,1,1},&Next,&I);}while(S==XCRON_ZONED_PENDING);
    testRequire(S==XCRON_ZONED_FOUND && Next==Expected && xrtAtomic32Load(&Z->Refs,XMEMORY_RELAXED)==Refs,"copied expression and no hidden zone retain");
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"cursor OOM scope");Before=C;
    testRequire(!xcronZonedCursorInit(&C,&E,Z,Begin,End,0,&I) && I.Code==XCRON_ZONED_EXPRESSION && I.Field==XCRON_FIELD_SECOND && !memcmp(&C,&Before,sizeof(C)) && xrtErrorKind(xrtGetError())==XERR_MEMORY,"typed expression issue survives diagnostic OOM");
    testRequire(xcronZonedCursorStep(&Copy,Z,(xcronzonedbudget){1,1,1},&Next,&I)==XCRON_ZONED_ERROR && I.Code==XCRON_ZONED_STATE && xrtErrorKind(xrtGetError())==XERR_MEMORY,"typed state issue survives diagnostic OOM");
    testRequire(xrtAllocationBudgetLeave(&Scope),"cursor OOM leave");xrtClearError();xrtAllocationBudgetFree(Budget);xcronZoneRelease(Z);xcronZoneRelease(Other);
}
static void testZonedCursorWarm(void)
{
    xcronzone* Z=zonedZone("STD0DST,59/0,J60/1");xcronexpression E=zonedExpr("0 0 31 DEC *",XCRON_UNIX5);
    xtime Begin=zonedTime(2096,3,2,0,0,0,0),End=Begin+XRT_TIME_DAY,Next=777;xcronzonedcursor C;xcronzonedcursorissue I;
    testRequire(xcronZonedCursorInit(&C,&E,Z,Begin,End,0,&I),"rare rule init");xrtSetErrorInfo(XERR_VALUE,"cursor.ambient",9,"preserve");const xerror* Ambient=xrtGetError();
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(true);xrtMemStatsGet(&Before);
#endif
    xcronzonedstatus S;unsigned Calls=0;uint64 Years=0;
    do{S=xcronZonedCursorStep(&C,Z,(xcronzonedbudget){1,1,1},&Next,&I);++Calls;
        testRequire(I.RuleYearsExamined-Years<=1 && xrtGetError()==Ambient && Next==777,"rare rule search actually yields inside transition scan and preserves ambient output/error");Years=I.RuleYearsExamined;
    }while(S==XCRON_ZONED_PENDING);
    testRequire(S==XCRON_ZONED_NONE && Calls>=8 && I.RuleYearsExamined>=8 && I.RuleYearsExamined<=403,"century rule query spans multiple bounded host turns");
    for(unsigned J=0;J<1024;++J){testRequire(xcronZonedCursorInit(&C,&E,Z,Begin,End,0,NULL),"warm cursor reset");do{S=xcronZonedCursorStep(&C,Z,(xcronzonedbudget){2,2,2},&Next,NULL);}while(S==XCRON_ZONED_PENDING);testRequire(S==XCRON_ZONED_NONE && xrtGetError()==Ambient,"warm complete cursor preserves error");}
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);testRequire(Before.MallocCalls==After.MallocCalls && Before.CallocCalls==After.CallocCalls && Before.ReallocCalls==After.ReallocCalls && Before.BlockAllocCalls==After.BlockAllocCalls,"resumable zoned traversal requests no heap");
#endif
    xrtClearError();xcronZoneRelease(Z);
}
static void testZonedCursorUnknown(void)
{
    tzifblob Blob;const int64 Times[]={0,86400};const uint8 Indices[]={1,1};const tzifkind Types[]={{0,false,"-00"},{0,false,"AAA"}};
    tzifBuild(&Blob,'3',Times,Indices,2,Types,2,"");xcronzone* Z=zonedBinary(&Blob);xcronexpression E=zonedExpr("0 0 * * *",XCRON_UNIX5);
    xcronzonedcursor C;xcronzonedcursorissue I;xtime Next=777;
    testRequire(xcronZonedCursorInit(&C,&E,Z,XRT_TIME_UNIX_EPOCH-1000,XRT_TIME_UNIX_EPOCH,0,&I) &&
        xcronZonedCursorStep(&C,Z,(xcronzonedbudget){1,1,1},&Next,&I)==XCRON_ZONED_ERROR && I.Code==XCRON_ZONED_ZONE && I.Zone==XCRON_ZONE_UNKNOWN && I.SegmentsExamined==1 && Next==777,"unknown prefix is never skipped by cursor");xrtClearError();
    testRequire(xcronZonedCursorInit(&C,&E,Z,XRT_TIME_UNIX_EPOCH+12*XRT_TIME_HOUR,XRT_TIME_UNIX_EPOCH+2*XRT_TIME_DAY,0,&I) &&
        xcronZonedCursorStep(&C,Z,(xcronzonedbudget){1,1,1},&Next,&I)==XCRON_ZONED_PENDING && I.SegmentsExamined==1 && Next==777,"known prefix is searched before yielding at unknown boundary");
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"unknown cursor OOM scope");
    testRequire(xcronZonedCursorStep(&C,Z,(xcronzonedbudget){1,1,1},&Next,&I)==XCRON_ZONED_ERROR && I.Code==XCRON_ZONED_ZONE && I.Zone==XCRON_ZONE_UNKNOWN && I.SegmentsExamined==2 && Next==777 && xrtErrorKind(xrtGetError())==XERR_MEMORY,"resumed unknown error remains typed under diagnostic OOM");
    xcronzonedcursor Before=C;xcronzonedcursorissue Old=I;
    testRequire(xcronZonedCursorStep(&C,Z,(xcronzonedbudget){9,9,9},&Next,&I)==XCRON_ZONED_ERROR && !memcmp(&C,&Before,sizeof(C)) && !memcmp(&I,&Old,sizeof(I)),"terminal mapping failure does not restart traversal");
    testRequire(xrtAllocationBudgetLeave(&Scope),"unknown cursor leave");xrtClearError();xrtAllocationBudgetFree(Budget);xcronZoneRelease(Z);
    const tzifkind Fixed[]={{0,false,"AAA"}};tzifBuild(&Blob,'3',NULL,NULL,0,Fixed,1,"AAA0");Z=zonedBinary(&Blob);xcronzonestate State;
    testRequire(xcronZoneAt(Z,0,&State,NULL) && xcronZonedCursorInit(&C,&E,Z,0,1000,0,&I),"owned footer alias fixture");Old=I;Before=C;
    testRequire(xcronZonedCursorStep(&C,Z,(xcronzonedbudget){1,1,1},&Next,(xcronzonedcursorissue*)State.Abbreviation.Data)==XCRON_ZONED_ERROR && !memcmp(&C,&Before,sizeof(C)) && !memcmp(&I,&Old,sizeof(I)),"owned footer outputs are protected on every step");xrtClearError();xcronZoneRelease(Z);
}
#if defined(XRT_FEATURE_THREAD)
typedef struct zonedhandoff {xcronzonedcursor* Cursor;const xcronzone* Zone;xcronzonedstatus Status;xtime Next;} zonedhandoff;
static int32 zonedHandoffWorker(ptr Raw)
{zonedhandoff* H=Raw;H->Status=xcronZonedCursorStep(H->Cursor,H->Zone,(xcronzonedbudget){1,1,1},&H->Next,NULL);return 0;}
static void testZonedCursorHandoff(void)
{
    xcronzone* Z=zonedZone("STD0DST,59/0,J60/1");xcronexpression E=zonedExpr("0 0 31 DEC *",XCRON_UNIX5);xcronzonedcursor C;
    xtime Begin=zonedTime(2096,3,2,0,0,0,0);testRequire(xcronZonedCursorInit(&C,&E,Z,Begin,Begin+XRT_TIME_DAY,0,NULL),"handoff cursor init");
    testRequire(xcronZoneRetain(Z),"explicit lifetime owner");xcronZoneRelease(Z);zonedhandoff H={&C,Z,XCRON_ZONED_ERROR,777};
    xthread* T=xrtThreadCreate(zonedHandoffWorker,&H,0);testRequire(T && xrtThreadWaitFor(T,10000)==XWAIT_OK,"serialized cursor handoff physical join");xrtThreadDestroy(T);
    testRequire(H.Status==XCRON_ZONED_PENDING && H.Next==777,"other thread performs a real bounded rule-year step");
    do{H.Status=xcronZonedCursorStep(&C,Z,(xcronzonedbudget){1,1,1},&H.Next,NULL);}while(H.Status==XCRON_ZONED_PENDING);
    testRequire(H.Status==XCRON_ZONED_NONE && H.Next==777,"creator resumes same live context and borrowed owner");xcronZoneRelease(Z);
}
#endif
