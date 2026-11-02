#include "zoned_internal.h"
#include "../calendar/calendar_internal.h"
#include "../timezone/timezone_internal.h"
#include <xrt/memory.h>
#if defined(XCRON_FEATURE_XCRON_ZONED)
#define XCRON_ZONED_CYCLE_MS INT64_C(12622780800000)
static bool xcronZonedFail(xcronzonederror Code,xcronfield Field,xcronzoneerror Zone,xcronzonedissue* Work,xcronzonedissue* Issue,cstr Message)
{
    Work->Code=Code;Work->Field=Field;Work->Zone=Zone;if(Issue)*Issue=*Work;
    xrtSetErrorInfo(Code==XCRON_ZONED_ARGUMENT?XERR_ARGUMENT:Code==XCRON_ZONED_EXPRESSION?XERR_VALUE:Code==XCRON_ZONED_ZONE?XERR_STATE:XERR_RANGE,
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
#endif
