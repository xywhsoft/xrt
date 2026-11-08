#include "occurrence_internal.h"
#include <xrt/error.h>
#include <string.h>
#if defined(XCRON_FEATURE_XCRON_OCCURRENCE)
#define XCRON_OCCURRENCE_CYCLE_MS INT64_C(12622780800000)
static void xcronOccurrenceLocal(xtime T,int32 Offset,xdatetime* D)
{xcronZoneDecompose(T,Offset,false,D);D->Offset=0;D->IsDST=-1;}
void xcronOccurrenceValueInit(const xcronexpression* E,const xcronzone* Z,xtime Begin,xtime End,uint32 Flags,xcrondeliverypolicy Policy,xcronoccurrencevalue* V)
{
    memset(V,0,sizeof(*V));V->Expression=*E;V->Policy=Policy;V->Work.Field=XCRON_FIELD_NONE;V->Status=XCRON_OCCURRENCE_NONE;
    int64 R=Begin%1000,D=R?(R>0?1000-R:-R):((Flags&XCRON_SEARCH_EXCLUSIVE)?1000:0);
    if(Begin>INT64_MAX-D || Begin+D>End)return;
    V->First=Begin+D;V->Limit=V->ActualLimit=End;xtime Repeat;
    const int64 Margin=(int64)INT32_MAX*2000,Span=XCRON_OCCURRENCE_CYCLE_MS-1000;
    if(xcronZoneRepeat(Z,V->First,&Repeat)){
        if(Repeat<=INT64_MAX-Span){xtime Proof=Repeat+Span;if(Proof<V->ActualLimit)V->ActualLimit=Proof;}
        if(Repeat<=INT64_MAX-Margin && Repeat+Margin<=INT64_MAX-Span){xtime Proof=Repeat+Margin+Span;if(Proof<V->Limit)V->Limit=Proof;}
    }
    V->Status=XCRON_OCCURRENCE_PENDING;V->Phase=XCRON_OCCURRENCE_PHASE_START;
}
static xcronoccurrencestatus xcronOccurrenceFail(xcronoccurrencevalue* V,xcronoccurrenceerror Code,xcronzoneerror Zone,xtime When,const xdatetime* Local,size_t Count)
{
    V->Work.Code=Code;V->Work.Zone=Zone;V->Work.When=When;V->Work.Candidates=Count;
    if(Local)V->Work.Local=*Local;
    return V->Status=XCRON_OCCURRENCE_ERROR;
}
static void xcronOccurrenceGapBegin(xcronoccurrencevalue* V,int32 Before)
{
    xdatetime First,End;xcronOccurrenceLocal(V->First,Before,&First);xcronOccurrenceLocal(V->First-1,V->Offset,&End);End.Millisecond=0;
    xcronCalendarValueBegin(&V->Expression,&First,&End,&V->Calendar);V->Phase=XCRON_OCCURRENCE_PHASE_GAP;
}
static void xcronOccurrenceCalendarBegin(xcronoccurrencevalue* V,bool After)
{
    xtime Last=V->SegmentEnd;if(!V->SawActual && Last>V->ActualLimit)Last=V->ActualLimit;
    xdatetime First,End;xcronOccurrenceLocal(Last,V->Offset,&End);
    if(After)xcronCalendarValueAfter(&V->Expression,&V->Candidate.Local,&End,&V->Calendar);
    else{xcronOccurrenceLocal(V->First,V->Offset,&First);xcronCalendarValueBegin(&V->Expression,&First,&End,&V->Calendar);}
    V->AfterReady=false;V->Phase=XCRON_OCCURRENCE_PHASE_CALENDAR;
}
static xcronzonestatus xcronOccurrenceResolve(xcronoccurrencevalue* V,const xcronzone* Z,xcronoccurrencebudget* B)
{
    uint64 Checks=V->Resolve.Checks,Offsets=V->Resolve.Offsets,Years=V->Resolve.RuleYears;
    xcronzonestatus S=xcronZoneResolutionStep(&V->Resolve,Z,(xcronresolutionbudget){B->Checks,B->Offsets,B->RuleYears},NULL);
    uint32 C=(uint32)(V->Resolve.Checks-Checks),O=(uint32)(V->Resolve.Offsets-Offsets),Y=(uint32)(V->Resolve.RuleYears-Years);
    B->Checks-=C;B->Offsets-=O;B->RuleYears-=Y;V->Work.ChecksExamined+=C;V->Work.OffsetsExamined+=O;V->Work.RuleYearsExamined+=Y;
    if(S!=XCRON_ZONE_WALK_PENDING)++V->Work.LabelsExamined;
    return S;
}
xcronoccurrencestatus xcronOccurrenceValueStep(xcronoccurrencevalue* V,const xcronzone* Z,xcronoccurrencebudget B)
{
    if(!(B.Segments|B.Months|B.RuleYears|B.Checks|B.Offsets))return V->Status;
    while(V->Status==XCRON_OCCURRENCE_PENDING){
        if(V->Phase==XCRON_OCCURRENCE_PHASE_START || V->Phase==XCRON_OCCURRENCE_PHASE_NEXT){
            bool Initial=V->Phase==XCRON_OCCURRENCE_PHASE_START;
            if(!Initial){
                if(V->Transition.Status==XCRON_ZONE_NONE || V->Transition.Result.When>V->Limit)return V->Status=XCRON_OCCURRENCE_NONE;
                if(!V->SawActual && V->Transition.Result.When>V->ActualLimit){
                    if(V->Policy.Missing==XCRON_MISSING_SKIP)return V->Status=XCRON_OCCURRENCE_NONE;
                    V->ActualDone=true;
                }
            }
            if(!B.Segments)return V->Status;
            --B.Segments;++V->Work.SegmentsExamined;
            int32 Before;
            if(Initial){V->Offset=xcronZoneStateValue(Z,V->First).Offset;Before=V->Policy.Missing==XCRON_MISSING_REJECT?xcronZoneStateValue(Z,V->First-1).Offset:V->Offset;}
            else{V->First=V->Transition.Result.When;V->Offset=V->Transition.Result.After.Offset;Before=V->Transition.Result.Before.Offset;}
            V->WalkReady=V->AfterReady=false;
            if(V->Offset==XCRON_ZONE_OFFSET_UNKNOWN || (V->Policy.Missing==XCRON_MISSING_REJECT && Before==XCRON_ZONE_OFFSET_UNKNOWN))
                return xcronOccurrenceFail(V,XCRON_OCCURRENCE_ZONE,XCRON_ZONE_UNKNOWN,V->First,NULL,0);
            if(V->Policy.Missing==XCRON_MISSING_REJECT && V->Offset>Before){xcronOccurrenceGapBegin(V,Before);continue;}
            V->Phase=XCRON_OCCURRENCE_PHASE_READY;
        }
        if(V->Phase==XCRON_OCCURRENCE_PHASE_READY){
            if(!V->ActualDone){
                if(!B.Months)return V->Status;
                xdatetime Local;xcronOccurrenceLocal(V->First,V->Offset,&Local);
                if(xcronCalendarPartsMatch(&V->Expression,&Local)){
                    --B.Months;++V->Work.MonthsExamined;V->Candidate=(xcronoccurrence){.When=V->First,.Local=Local};V->SawActual=true;
                    xcronZoneResolutionInit(&Local,&V->Resolve);V->Phase=XCRON_OCCURRENCE_PHASE_RESOLVE;continue;
                }
            }
            V->Phase=XCRON_OCCURRENCE_PHASE_WALK_BEGIN;
        }
        if(V->Phase==XCRON_OCCURRENCE_PHASE_WALK_BEGIN){
            if(V->First>=V->Limit)return V->Status=XCRON_OCCURRENCE_NONE;
            xcronZoneWalkInit(Z,V->First,&V->Transition);V->Phase=XCRON_OCCURRENCE_PHASE_WALK;
        }
        if(V->Phase==XCRON_OCCURRENCE_PHASE_WALK){
            uint32 Before=V->Transition.Years;xcronzonestatus S=xcronZoneWalkStep(&V->Transition,B.RuleYears);uint32 Used=V->Transition.Years-Before;
            B.RuleYears-=Used;V->Work.RuleYearsExamined+=Used;if(S==XCRON_ZONE_WALK_PENDING)return V->Status;
            V->WalkReady=true;V->SegmentEnd=V->Limit;
            if(S==XCRON_ZONE_FOUND && V->Transition.Result.When<=V->Limit)V->SegmentEnd=V->Transition.Result.When-1;
            if(V->ActualDone){V->Phase=XCRON_OCCURRENCE_PHASE_NEXT;continue;}
            xcronOccurrenceCalendarBegin(V,V->AfterReady);
        }
        if(V->Phase==XCRON_OCCURRENCE_PHASE_CALENDAR){
            uint32 Before=V->Calendar.Months;xcroncalendarstatus S=xcronCalendarValueStep(&V->Expression,&V->Calendar,B.Months);uint32 Used=V->Calendar.Months-Before;
            B.Months-=Used;V->Work.MonthsExamined+=Used;if(S==XCRON_CALENDAR_PENDING)return V->Status;
            if(S==XCRON_CALENDAR_NONE){
                if(!V->SawActual && (V->SegmentEnd>=V->ActualLimit || (V->First<=INT64_MAX-(XCRON_OCCURRENCE_CYCLE_MS-1000) && V->SegmentEnd>=V->First+(XCRON_OCCURRENCE_CYCLE_MS-1000)))){
                    if(V->Policy.Missing==XCRON_MISSING_SKIP)return V->Status=XCRON_OCCURRENCE_NONE;
                    V->ActualDone=true;
                }
                V->Phase=XCRON_OCCURRENCE_PHASE_NEXT;continue;
            }
            int64 Seconds;xtime Result;
            if(!xcronZoneCivilSeconds(&V->Calendar.Date,&Seconds) || !xcronZoneSecondValue(Seconds-V->Offset,0,&Result) || Result<V->First || Result>V->SegmentEnd || (!V->SawActual && Result>V->ActualLimit))
                return xcronOccurrenceFail(V,XCRON_OCCURRENCE_RANGE,XCRON_ZONE_OK,V->First,NULL,0);
            V->Candidate=(xcronoccurrence){.When=Result};xcronOccurrenceLocal(Result,V->Offset,&V->Candidate.Local);V->SawActual=true;
            xcronZoneResolutionInit(&V->Candidate.Local,&V->Resolve);V->Phase=XCRON_OCCURRENCE_PHASE_RESOLVE;
        }
        if(V->Phase==XCRON_OCCURRENCE_PHASE_RESOLVE){
            xcronzonestatus S=xcronOccurrenceResolve(V,Z,&B);if(S==XCRON_ZONE_WALK_PENDING)return V->Status;
            V->Candidate.Candidates=V->Resolve.Count;
            if(S==XCRON_ZONE_ERROR)return xcronOccurrenceFail(V,XCRON_OCCURRENCE_ZONE,V->Resolve.Error,V->Candidate.When,&V->Candidate.Local,V->Resolve.Count);
            if(!V->Resolve.Count)return xcronOccurrenceFail(V,XCRON_OCCURRENCE_RANGE,XCRON_ZONE_OK,V->Candidate.When,&V->Candidate.Local,0);
            if(V->Resolve.Count>1 && V->Policy.Fold==XCRON_FOLD_REJECT)return xcronOccurrenceFail(V,XCRON_OCCURRENCE_AMBIGUOUS,XCRON_ZONE_OK,V->Candidate.When,&V->Candidate.Local,V->Resolve.Count);
            if((V->Policy.Fold==XCRON_FOLD_FIRST && V->Candidate.When!=V->Resolve.First) || (V->Policy.Fold==XCRON_FOLD_LAST && V->Candidate.When!=V->Resolve.Last)){
                if(V->WalkReady)xcronOccurrenceCalendarBegin(V,true);
                else{V->AfterReady=true;V->Phase=XCRON_OCCURRENCE_PHASE_WALK_BEGIN;}
                continue;
            }
            V->Work.When=V->Candidate.When;V->Work.Local=V->Candidate.Local;V->Work.Candidates=V->Candidate.Candidates;
            return V->Status=XCRON_OCCURRENCE_FOUND;
        }
        if(V->Phase==XCRON_OCCURRENCE_PHASE_GAP){
            uint32 Before=V->Calendar.Months;xcroncalendarstatus S=xcronCalendarValueStep(&V->Expression,&V->Calendar,B.Months);uint32 Used=V->Calendar.Months-Before;
            B.Months-=Used;V->Work.MonthsExamined+=Used;if(S==XCRON_CALENDAR_PENDING)return V->Status;
            if(S==XCRON_CALENDAR_NONE){V->Phase=XCRON_OCCURRENCE_PHASE_READY;continue;}
            int64 Seconds;if(!xcronZoneCivilSeconds(&V->Calendar.Date,&Seconds))return xcronOccurrenceFail(V,XCRON_OCCURRENCE_RANGE,XCRON_ZONE_OK,V->First,NULL,0);
            int64 Offset=Seconds-V->First/1000;
            if(Offset<-INT32_MAX || Offset>INT32_MAX)return xcronOccurrenceFail(V,XCRON_OCCURRENCE_RANGE,XCRON_ZONE_OK,V->First,NULL,0);
            xcronOccurrenceLocal(V->First,(int32)Offset,&V->Calendar.Date);xcronZoneResolutionInit(&V->Calendar.Date,&V->Resolve);V->Phase=XCRON_OCCURRENCE_PHASE_GAP_RESOLVE;
        }
        if(V->Phase==XCRON_OCCURRENCE_PHASE_GAP_RESOLVE){
            xcronzonestatus S=xcronOccurrenceResolve(V,Z,&B);if(S==XCRON_ZONE_WALK_PENDING)return V->Status;
            if(S==XCRON_ZONE_ERROR)return xcronOccurrenceFail(V,XCRON_OCCURRENCE_ZONE,V->Resolve.Error,V->First,&V->Calendar.Date,0);
            if(!V->Resolve.Count)return xcronOccurrenceFail(V,XCRON_OCCURRENCE_MISSING,XCRON_ZONE_OK,V->First,&V->Calendar.Date,0);
            xcronCalendarValueAfter(&V->Expression,&V->Calendar.Date,&V->Calendar.End,&V->Calendar);V->Phase=XCRON_OCCURRENCE_PHASE_GAP;
        }
    }
    return V->Status;
}
static bool xcronOccurrencePolicy(xcrondeliverypolicy P)
{return P.Fold>=XCRON_FOLD_ALL && P.Fold<=XCRON_FOLD_REJECT && P.Missing>=XCRON_MISSING_SKIP && P.Missing<=XCRON_MISSING_REJECT;}
static void xcronOccurrenceReport(xcronoccurrenceissue Work,xcronoccurrenceissue* Issue)
{
    if(Issue)*Issue=Work;
    xerrkind Kind=Work.Code==XCRON_OCCURRENCE_ARGUMENT?XERR_ARGUMENT:Work.Code==XCRON_OCCURRENCE_ZONE || Work.Code==XCRON_OCCURRENCE_STATE?XERR_STATE:Work.Code==XCRON_OCCURRENCE_RANGE || Work.Code==XCRON_OCCURRENCE_WINDOW?XERR_RANGE:XERR_VALUE;
    xrtSetErrorInfo(Kind,"xrt.cron.occurrence",Work.Code,"cron occurrence calculation failed");
}
static bool xcronOccurrenceInputs(const xcronexpression* E,xtime Begin,xtime End,xcronoccurrenceissue* Issue)
{
    xcronfield Field;
    if(!xcronCalendarValidateExpression(E,&Field)){xcronOccurrenceReport((xcronoccurrenceissue){.Code=XCRON_OCCURRENCE_EXPRESSION,.Field=Field},Issue);return false;}
    if(Begin>End){xcronOccurrenceReport((xcronoccurrenceissue){.Code=XCRON_OCCURRENCE_WINDOW,.Field=XCRON_FIELD_NONE},Issue);return false;}
    return true;
}
XCRON_API xcronoccurrencestatus xcronOccurrenceNext(const xcronexpression* E,const xcronzone* Z,xtime Begin,xtime End,uint32 Flags,xcrondeliverypolicy Policy,xcronoccurrence* Next,xcronoccurrenceissue* Issue)
{
    void* O[]={Next,Issue};size_t N[]={sizeof(*Next),Issue?sizeof(*Issue):0};
    if(!E || !Next || (Flags&~XCRON_SEARCH_EXCLUSIVE) || !xcronOccurrencePolicy(Policy) || !xcronZoneOutputs(Z,E,sizeof(*E),O,N,2)){
        xcronOccurrenceReport((xcronoccurrenceissue){.Code=XCRON_OCCURRENCE_ARGUMENT,.Field=XCRON_FIELD_NONE},NULL);return XCRON_OCCURRENCE_ERROR;
    }
    if(!xcronOccurrenceInputs(E,Begin,End,Issue))return XCRON_OCCURRENCE_ERROR;
    xcronoccurrencevalue V;xcronOccurrenceValueInit(E,Z,Begin,End,Flags,Policy,&V);xcronoccurrencestatus S;
    do{S=xcronOccurrenceValueStep(&V,Z,(xcronoccurrencebudget){UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX});}while(S==XCRON_OCCURRENCE_PENDING);
    if(S==XCRON_OCCURRENCE_FOUND)*Next=V.Candidate;
    if(S==XCRON_OCCURRENCE_ERROR)xcronOccurrenceReport(V.Work,Issue);else if(Issue)*Issue=V.Work;
    return S;
}
#define XCRON_OCCURRENCE_CURSOR_MAGIC UINT32_C(0x584f4332)
typedef struct xcronoccurrenceimpl {uint32 Magic;uintptr_t Address,Zone;xcronoccurrencevalue Value;} xcronoccurrenceimpl;
typedef char xcron_occurrence_cursor_size_check[(sizeof(xcronoccurrenceimpl)<=sizeof(xcronoccurrencecursor))?1:-1];
XCRON_API bool xcronOccurrenceCursorInit(xcronoccurrencecursor* C,const xcronexpression* E,const xcronzone* Z,xtime Begin,xtime End,uint32 Flags,xcrondeliverypolicy Policy,xcronoccurrenceissue* Issue)
{
    void* O[]={C,Issue};size_t N[]={sizeof(*C),Issue?sizeof(*Issue):0};
    if(!C || !E || (Flags&~XCRON_SEARCH_EXCLUSIVE) || !xcronOccurrencePolicy(Policy) || !xcronZoneOutputs(Z,E,sizeof(*E),O,N,2)){
        xcronOccurrenceReport((xcronoccurrenceissue){.Code=XCRON_OCCURRENCE_ARGUMENT,.Field=XCRON_FIELD_NONE},NULL);return false;
    }
    if(!xcronOccurrenceInputs(E,Begin,End,Issue))return false;
    xcronoccurrenceimpl I;memset(&I,0,sizeof(I));I.Magic=XCRON_OCCURRENCE_CURSOR_MAGIC;I.Address=(uintptr_t)C;I.Zone=(uintptr_t)Z;xcronOccurrenceValueInit(E,Z,Begin,End,Flags,Policy,&I.Value);
    memset(C,0,sizeof(*C));memcpy(C,&I,sizeof(I));if(Issue)*Issue=I.Value.Work;return true;
}
XCRON_API xcronoccurrencestatus xcronOccurrenceCursorStep(xcronoccurrencecursor* C,const xcronzone* Z,xcronoccurrencebudget B,xcronoccurrence* Next,xcronoccurrenceissue* Issue)
{
    void* O[]={C,Next,Issue};size_t N[]={sizeof(*C),sizeof(*Next),Issue?sizeof(*Issue):0};
    if(!C || !Next || !xcronZoneOutputs(Z,NULL,0,O,N,3)){
        xcronOccurrenceReport((xcronoccurrenceissue){.Code=XCRON_OCCURRENCE_ARGUMENT,.Field=XCRON_FIELD_NONE},NULL);return XCRON_OCCURRENCE_ERROR;
    }
    xcronoccurrenceimpl I;memcpy(&I,C,sizeof(I));
    if(I.Magic!=XCRON_OCCURRENCE_CURSOR_MAGIC || I.Address!=(uintptr_t)C || I.Zone!=(uintptr_t)Z || I.Value.Phase>XCRON_OCCURRENCE_PHASE_GAP_RESOLVE || !xcronOccurrencePolicy(I.Value.Policy) ||
       (I.Value.Status!=XCRON_OCCURRENCE_PENDING && I.Value.Status!=XCRON_OCCURRENCE_FOUND && I.Value.Status!=XCRON_OCCURRENCE_NONE && I.Value.Status!=XCRON_OCCURRENCE_ERROR)){
        xcronOccurrenceReport((xcronoccurrenceissue){.Code=XCRON_OCCURRENCE_STATE,.Field=XCRON_FIELD_NONE},Issue);return XCRON_OCCURRENCE_ERROR;
    }
    xcronoccurrencestatus S=xcronOccurrenceValueStep(&I.Value,Z,B);memcpy(C,&I,sizeof(I));if(S==XCRON_OCCURRENCE_FOUND)*Next=I.Value.Candidate;
    if(S==XCRON_OCCURRENCE_ERROR)xcronOccurrenceReport(I.Value.Work,Issue);else if(Issue)*Issue=I.Value.Work;
    return S;
}
#endif
