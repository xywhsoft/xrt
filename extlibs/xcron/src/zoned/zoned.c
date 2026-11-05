#include "zoned_internal.h"
#include "../calendar/calendar_internal.h"
#include "../timezone/timezone_internal.h"
#include <xrt/memory.h>
#include <string.h>
#include <stddef.h>
#if defined(XCRON_FEATURE_XCRON_ZONED)
#define XCRON_ZONED_CYCLE_MS INT64_C(12622780800000)
static bool xcronZonedFail(xcronzonederror Code,xcronfield Field,xcronzoneerror Zone,xcronzonedissue* Work,xcronzonedissue* Issue,cstr Message)
{
    Work->Code=Code;Work->Field=Field;Work->Zone=Zone;if(Issue)*Issue=*Work;
    xrtSetErrorInfo(Code==XCRON_ZONED_ARGUMENT?XERR_ARGUMENT:Code==XCRON_ZONED_EXPRESSION?XERR_VALUE:(Code==XCRON_ZONED_ZONE || Code==XCRON_ZONED_STATE)?XERR_STATE:XERR_RANGE,
        "xrt.cron.zoned",Code,Message);return false;
}
static bool xcronZonedArgs(const xcronexpression* E,const xcronzone* Z,uint32 Flags,void* Result,size_t ResultSize,xcronzonedissue* Issue)
{
    void* Outputs[]={Result,Issue};size_t Sizes[]={ResultSize,Issue?sizeof(*Issue):0};
    return E && Result && !(Flags&~XCRON_SEARCH_EXCLUSIVE) && xcronZoneOutputs(Z,E,sizeof(*E),Outputs,Sizes,2);
}
static bool xcronZonedValidate(const xcronexpression* E,xcronzonedissue* Work,xcronzonedissue* Issue)
{
    xcronfield Field;if(xcronCalendarValidateExpression(E,&Field))return true;
    return xcronZonedFail(XCRON_ZONED_EXPRESSION,Field,XCRON_ZONE_OK,Work,Issue,"invalid compiled cron expression");
}
XCRON_API bool xcronZonedMatch(const xcronexpression* E,const xcronzone* Z,xtime When,bool* Matches,xcronzonedissue* Issue)
{
    xcronzonedissue Work={XCRON_ZONED_OK,XCRON_FIELD_NONE,XCRON_ZONE_OK,0,0};
    if(!xcronZonedArgs(E,Z,0,Matches,sizeof(*Matches),Issue))return xcronZonedFail(XCRON_ZONED_ARGUMENT,XCRON_FIELD_NONE,XCRON_ZONE_OK,&Work,NULL,"invalid zoned cron arguments/spans/output aliases");
    if(!xcronZonedValidate(E,&Work,Issue))return false;
    bool Value=false;
    if(!(When%1000)){
        ++Work.SegmentsExamined;xcronzonestate State=xcronZoneStateValue(Z,When);
        if(State.Offset==XCRON_ZONE_OFFSET_UNKNOWN)return xcronZonedFail(XCRON_ZONED_ZONE,XCRON_FIELD_NONE,XCRON_ZONE_UNKNOWN,&Work,Issue,"cron instant has unknown zone coverage");
        xdatetime Date;xcronZoneDecompose(When,State.Offset,State.IsDST,&Date);Value=xcronCalendarPartsMatch(E,&Date);
    }
    *Matches=Value;if(Issue)*Issue=Work;return true;
}
static void xcronZonedValueFail(xcronzonederror Code,xcronzoneerror Zone,xcronzonedissue* Work,xcronzonedissue* Issue)
{Work->Code=Code;Work->Zone=Zone;if(Issue)*Issue=*Work;}
xcronzonedstatus xcronZonedNextValue(const xcronexpression* E,const xcronzone* Z,xtime Begin,xtime End,uint32 Flags,xtime* Next,xcronzonedissue* Issue)
{
    xcronzonedissue Work={XCRON_ZONED_OK,XCRON_FIELD_NONE,XCRON_ZONE_OK,0,0};
    int64 Remainder=Begin%1000,Delta=Remainder?(Remainder>0?1000-Remainder:-Remainder):((Flags&XCRON_SEARCH_EXCLUSIVE)?1000:0);
    if(Begin>INT64_MAX-Delta || Begin+Delta>End){if(Issue)*Issue=Work;return XCRON_ZONED_NONE;}
    xtime First=Begin+Delta,Limit=End,Repeat;
    if(xcronZoneRepeat(Z,First,&Repeat) && Repeat<=End && Repeat<=INT64_MAX-(XCRON_ZONED_CYCLE_MS-1000)){
        xtime ProofEnd=Repeat+(XCRON_ZONED_CYCLE_MS-1000);if(ProofEnd<Limit)Limit=ProofEnd;
    }
    for(;;){
        ++Work.SegmentsExamined;xcronzonestate State=xcronZoneStateValue(Z,First);
        if(State.Offset==XCRON_ZONE_OFFSET_UNKNOWN){xcronZonedValueFail(XCRON_ZONED_ZONE,XCRON_ZONE_UNKNOWN,&Work,Issue);return XCRON_ZONED_ERROR;}
        xdatetime LocalBegin;xcronZoneDecompose(First,State.Offset,State.IsDST,&LocalBegin);
        if(xcronCalendarPartsMatch(E,&LocalBegin)){++Work.MonthsExamined;*Next=First;if(Issue)*Issue=Work;return XCRON_ZONED_FOUND;}
        xcronzonetransition Transition;xcronzoneissue ZI;
        xcronzonestatus ZS=xcronZoneNextValue(Z,First,&Transition,&ZI);
        if(ZS==XCRON_ZONE_ERROR){xcronZonedValueFail(XCRON_ZONED_ZONE,ZI.Code,&Work,Issue);return XCRON_ZONED_ERROR;}
        xtime SegmentEnd=Limit;if(ZS==XCRON_ZONE_FOUND && Transition.When<=Limit)SegmentEnd=Transition.When-1;
        xdatetime LocalEnd,Value;xcronZoneDecompose(SegmentEnd,State.Offset,State.IsDST,&LocalEnd);
        xcroncalendarissue CI;xcroncalendarstatus CS=xcronCalendarSearch(E,&LocalBegin,&LocalEnd,0,&Value,&CI);Work.MonthsExamined+=CI.MonthsExamined;
        if(CS==XCRON_CALENDAR_FOUND){int64 Seconds;xtime Result;
            if(!xcronZoneCivilSeconds(&Value,&Seconds) || !xcronZoneSecondValue(Seconds-State.Offset,0,&Result) || Result<First || Result>SegmentEnd){
                xcronZonedValueFail(XCRON_ZONED_RANGE,XCRON_ZONE_OK,&Work,Issue);return XCRON_ZONED_ERROR;
            }
            *Next=Result;if(Issue)*Issue=Work;return XCRON_ZONED_FOUND;
        }
        if(CS==XCRON_CALENDAR_ERROR){xcronZonedValueFail(XCRON_ZONED_RANGE,XCRON_ZONE_OK,&Work,Issue);return XCRON_ZONED_ERROR;}
        /* A complete civil cycle without a label proves global impossibility,
         * independently of all later offsets (including unknown coverage). */
        if((First<=INT64_MAX-(XCRON_ZONED_CYCLE_MS-1000) && SegmentEnd>=First+(XCRON_ZONED_CYCLE_MS-1000)) || ZS==XCRON_ZONE_NONE || Transition.When>Limit)break;
        First=Transition.When;
    }
    if(Issue)*Issue=Work;
    return XCRON_ZONED_NONE;
}
XCRON_API xcronzonedstatus xcronZonedNext(const xcronexpression* E,const xcronzone* Z,xtime Begin,xtime End,uint32 Flags,xtime* Next,xcronzonedissue* Issue)
{
    xcronzonedissue Work={XCRON_ZONED_OK,XCRON_FIELD_NONE,XCRON_ZONE_OK,0,0};
    if(!xcronZonedArgs(E,Z,Flags,Next,sizeof(*Next),Issue)){xcronZonedFail(XCRON_ZONED_ARGUMENT,XCRON_FIELD_NONE,XCRON_ZONE_OK,&Work,NULL,"invalid zoned cron arguments/spans/output aliases");return XCRON_ZONED_ERROR;}
    if(!xcronZonedValidate(E,&Work,Issue))return XCRON_ZONED_ERROR;
    if(Begin>End){xcronZonedFail(XCRON_ZONED_WINDOW,XCRON_FIELD_NONE,XCRON_ZONE_OK,&Work,Issue,"zoned cron window is reversed");return XCRON_ZONED_ERROR;}
    xcronzonedstatus Status=xcronZonedNextValue(E,Z,Begin,End,Flags,Next,&Work);
    if(Status==XCRON_ZONED_ERROR)xcronZonedFail(Work.Code,Work.Field,Work.Zone,&Work,NULL,
        Work.Code==XCRON_ZONED_ZONE?(Work.Zone==XCRON_ZONE_UNKNOWN?"cron search reached unknown zone coverage":"cron transition search failed"):
        "invalid derived zoned civil window or candidate");
    if(Issue)*Issue=Work;
    return Status;
}
void xcronZonedValueInit(const xcronexpression* E,const xcronzone* Z,xtime Begin,xtime End,uint32 Flags,xcronzonedvalue* V)
{
    memset(V,0,sizeof(*V));V->Expression=*E;V->Work.Field=XCRON_FIELD_NONE;V->Status=XCRON_ZONED_NONE;
    int64 R=Begin%1000,Delta=R?(R>0?1000-R:-R):((Flags&XCRON_SEARCH_EXCLUSIVE)?1000:0);
    if(Begin>INT64_MAX-Delta || Begin+Delta>End)return;
    V->First=Begin+Delta;V->Limit=End;xtime Repeat;
    if(xcronZoneRepeat(Z,V->First,&Repeat) && Repeat<=End && Repeat<=INT64_MAX-(XCRON_ZONED_CYCLE_MS-1000)){
        xtime ProofEnd=Repeat+(XCRON_ZONED_CYCLE_MS-1000);if(ProofEnd<V->Limit)V->Limit=ProofEnd;
    }
    V->Status=XCRON_ZONED_PENDING;
}
static xcronzonedstatus xcronZonedValueError(xcronzonedvalue* V,xcronzonederror Code,xcronzoneerror Zone)
{V->Work.Code=Code;V->Work.Zone=Zone;return V->Status=XCRON_ZONED_ERROR;}
xcronzonedstatus xcronZonedValueStep(xcronzonedvalue* V,const xcronzone* Z,xcronzonedbudget B)
{
    while(V->Status==XCRON_ZONED_PENDING){
        if(V->Phase==0){
            if(!B.Segments || !B.Months)return V->Status;
            --B.Segments;++V->Work.SegmentsExamined;
            xcronzonestate State=xcronZoneStateValue(Z,V->First);
            if(State.Offset==XCRON_ZONE_OFFSET_UNKNOWN)return xcronZonedValueError(V,XCRON_ZONED_ZONE,XCRON_ZONE_UNKNOWN);
            V->Offset=State.Offset;xcronZoneDecompose(V->First,State.Offset,State.IsDST,&V->Calendar.First);
            if(xcronCalendarPartsMatch(&V->Expression,&V->Calendar.First)){
                ++V->Work.MonthsExamined;V->Result=V->First;return V->Status=XCRON_ZONED_FOUND;
            }
            xcronZoneWalkInit(Z,V->First,&V->Transition);V->Phase=1;
        }
        if(V->Phase==1){
            uint32 Before=V->Transition.Years;
            xcronzonestatus Status=xcronZoneWalkStep(&V->Transition,B.RuleYears);
            uint32 Used=V->Transition.Years-Before;B.RuleYears-=Used;V->Work.RuleYearsExamined+=Used;
            if(Status==XCRON_ZONE_WALK_PENDING)return V->Status;
            V->SegmentEnd=V->Limit;
            if(Status==XCRON_ZONE_FOUND && V->Transition.Result.When<=V->Limit)V->SegmentEnd=V->Transition.Result.When-1;
            xdatetime End,First=V->Calendar.First;xcronZoneDecompose(V->SegmentEnd,V->Offset,false,&End);
            xcronCalendarValueBegin(&V->Expression,&First,&End,&V->Calendar);V->Phase=2;
        }
        if(V->Phase==2){
            uint32 Before=V->Calendar.Months;
            xcroncalendarstatus Status=xcronCalendarValueStep(&V->Expression,&V->Calendar,B.Months);
            uint32 Used=V->Calendar.Months-Before;B.Months-=Used;V->Work.MonthsExamined+=Used;
            if(Status==XCRON_CALENDAR_PENDING)return V->Status;
            if(Status==XCRON_CALENDAR_FOUND){int64 Seconds;xtime Result;
                if(!xcronZoneCivilSeconds(&V->Calendar.Date,&Seconds) || !xcronZoneSecondValue(Seconds-V->Offset,0,&Result) || Result<V->First || Result>V->SegmentEnd)
                    return xcronZonedValueError(V,XCRON_ZONED_RANGE,XCRON_ZONE_OK);
                V->Result=Result;return V->Status=XCRON_ZONED_FOUND;
            }
            if((V->First<=INT64_MAX-(XCRON_ZONED_CYCLE_MS-1000) && V->SegmentEnd>=V->First+(XCRON_ZONED_CYCLE_MS-1000)) ||
                V->Transition.Status==XCRON_ZONE_NONE || V->Transition.Result.When>V->Limit)return V->Status=XCRON_ZONED_NONE;
            V->First=V->Transition.Result.When;V->Phase=0;
        }
    }
    return V->Status;
}
#define XCRON_ZONED_CURSOR_MAGIC UINT32_C(0x585a4331)
typedef struct xcronzonedimpl {
    uint32 Magic;
    uintptr_t Address,Zone;
    xcronzonedvalue Value;
} xcronzonedimpl;
typedef char xcron_zoned_cursor_size_check[(sizeof(xcronzonedimpl)<=sizeof(xcronzonedcursor))?1:-1];
static void xcronZonedCursorReport(xcronzonedcursorissue* Issue,xcronzonederror Code,xcronfield Field,cstr Message)
{
    if(Issue)*Issue=(xcronzonedcursorissue){Code,Field,XCRON_ZONE_OK,0,0,0};
    xcronzonedissue Work={0};xcronZonedFail(Code,Field,XCRON_ZONE_OK,&Work,NULL,Message);
}
XCRON_API bool xcronZonedCursorInit(xcronzonedcursor* C,const xcronexpression* E,const xcronzone* Z,
    xtime Begin,xtime End,uint32 Flags,xcronzonedcursorissue* Issue)
{
    void* Outputs[]={C,Issue};size_t Sizes[]={sizeof(*C),Issue?sizeof(*Issue):0};
    if(!C || !E || (Flags&~XCRON_SEARCH_EXCLUSIVE) || !xcronZoneOutputs(Z,E,sizeof(*E),Outputs,Sizes,2)){
        xcronZonedCursorReport(NULL,XCRON_ZONED_ARGUMENT,XCRON_FIELD_NONE,"invalid zoned cursor init spans/aliases/flags");return false;
    }
    xcronfield Field;
    if(!xcronCalendarValidateExpression(E,&Field)){xcronZonedCursorReport(Issue,XCRON_ZONED_EXPRESSION,Field,"invalid zoned cursor expression");return false;}
    if(Begin>End){xcronZonedCursorReport(Issue,XCRON_ZONED_WINDOW,XCRON_FIELD_NONE,"zoned cursor window is reversed");return false;}
    xcronzonedimpl I;memset(&I,0,offsetof(xcronzonedimpl,Value));I.Magic=XCRON_ZONED_CURSOR_MAGIC;I.Address=(uintptr_t)C;I.Zone=(uintptr_t)Z;
    xcronZonedValueInit(E,Z,Begin,End,Flags,&I.Value);
    memcpy(C,&I,sizeof(I));memset(C->Storage+sizeof(I),0,sizeof(*C)-sizeof(I));if(Issue)*Issue=I.Value.Work;return true;
}
XCRON_API xcronzonedstatus xcronZonedCursorStep(xcronzonedcursor* C,const xcronzone* Z,xcronzonedbudget Budget,
    xtime* Next,xcronzonedcursorissue* Issue)
{
    void* Outputs[]={C,Next,Issue};size_t Sizes[]={sizeof(*C),sizeof(*Next),Issue?sizeof(*Issue):0};
    if(!C || !Next || !xcronZoneOutputs(Z,NULL,0,Outputs,Sizes,3)){
        xcronZonedCursorReport(NULL,XCRON_ZONED_ARGUMENT,XCRON_FIELD_NONE,"invalid zoned cursor step spans/aliases");return XCRON_ZONED_ERROR;
    }
    if(!Budget.Segments || !Budget.Months || !Budget.RuleYears){
        xcronZonedCursorReport(Issue,XCRON_ZONED_ARGUMENT,XCRON_FIELD_NONE,"zoned cursor budgets must be positive");return XCRON_ZONED_ERROR;
    }
    xcronzonedimpl I;memcpy(&I,C,sizeof(I));
    if(I.Magic!=XCRON_ZONED_CURSOR_MAGIC || I.Address!=(uintptr_t)C || I.Zone!=(uintptr_t)Z || I.Value.Phase>2 ||
        (I.Value.Status!=XCRON_ZONED_PENDING && I.Value.Status!=XCRON_ZONED_FOUND && I.Value.Status!=XCRON_ZONED_NONE && I.Value.Status!=XCRON_ZONED_ERROR)){
        xcronZonedCursorReport(Issue,XCRON_ZONED_STATE,XCRON_FIELD_NONE,"uninitialized/copied cursor or wrong live zone");return XCRON_ZONED_ERROR;
    }
    xcronzonedstatus Status=xcronZonedValueStep(&I.Value,Z,Budget);
    memcpy(C,&I,sizeof(I));if(Status==XCRON_ZONED_FOUND)*Next=I.Value.Result;if(Issue)*Issue=I.Value.Work;
    if(Status==XCRON_ZONED_ERROR){xcronzonedissue Work={0};
        xcronZonedFail(I.Value.Work.Code,I.Value.Work.Field,I.Value.Work.Zone,&Work,NULL,"zoned cursor reached unknown coverage or an invalid derived candidate");
    }
    return Status;
}
#endif
