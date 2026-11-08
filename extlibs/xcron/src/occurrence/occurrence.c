#include "occurrence_internal.h"
#include <xrt/error.h>
#include <string.h>
#if defined(XCRON_FEATURE_XCRON_OCCURRENCE)
#define XCRON_OCCURRENCE_CYCLE_MS INT64_C(12622780800000)
static void xcronOccurrenceLocal(xtime T,int32 Offset,xdatetime* D)
{xcronZoneDecompose(T,Offset,false,D);D->Offset=0;D->IsDST=-1;}
void xcronOccurrenceValueInit(const xcronexpression* E,const xcronzone* Z,xtime Begin,xtime End,uint32 Flags,xcrondeliverypolicy Policy,xcronoccurrencevalue* V)
{
    memset(V,0,sizeof(*V));V->Policy=Policy;V->Work.Field=XCRON_FIELD_NONE;V->Status=XCRON_OCCURRENCE_NONE;
    int64 R=Begin%1000,D=R?(R>0?1000-R:-R):((Flags&XCRON_SEARCH_EXCLUSIVE)?1000:0);
    if(Begin>INT64_MAX-D || Begin+D>End)return;
    V->First=Begin+D;V->Limit=End;xtime Repeat;
    const int64 Margin=(int64)INT32_MAX*2000,Span=XCRON_OCCURRENCE_CYCLE_MS-1000;
    if(xcronZoneRepeat(Z,V->First,&Repeat) && Repeat<=INT64_MAX-Margin && Repeat+Margin<=INT64_MAX-Span){
        xtime ProofEnd=Repeat+Margin+Span;if(ProofEnd<V->Limit)V->Limit=ProofEnd;
    }
    xcronZonedValueInit(E,Z,V->First,V->Limit,0,&V->Search);V->Status=XCRON_OCCURRENCE_PENDING;
}
static xcronoccurrencestatus xcronOccurrenceFinish(xcronoccurrencevalue* V)
{
    V->Work.Code=V->OutcomeCode;V->Work.Zone=V->OutcomeZone;
    V->Work.When=V->Candidate.When;V->Work.Local=V->Candidate.Local;V->Work.Candidates=V->Candidate.Candidates;
    return V->Status=V->Outcome;
}
static xcronoccurrencestatus xcronOccurrenceGapFail(xcronoccurrencevalue* V,xcronoccurrenceerror Code,xcronzoneerror Zone,bool Label)
{
    V->Work.Code=Code;V->Work.Zone=Zone;V->Work.When=V->After;
    if(Label)V->Work.Local=V->Search.Calendar.Date;
    return V->Status=XCRON_OCCURRENCE_ERROR;
}
static void xcronOccurrenceGapBegin(xcronoccurrencevalue* V,const xcronzonestate* Before,const xcronzonestate* After)
{
    xdatetime First,End;xcronOccurrenceLocal(V->After,Before->Offset,&First);xcronOccurrenceLocal(V->After-1,After->Offset,&End);End.Millisecond=0;
    xcronCalendarValueBegin(&V->Search.Expression,&First,&End,&V->Search.Calendar);V->Phase=5;
}
static void xcronOccurrencePrepareGap(xcronoccurrencevalue* V)
{V->Bound=V->Outcome==XCRON_OCCURRENCE_NONE?V->Limit:V->Candidate.When;V->After=V->First;V->Phase=2;}
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
        if(V->Phase==0){
            xcronzonedcursorissue Before=V->Search.Work;
            xcronzonedstatus S=xcronZonedValueStep(&V->Search,Z,(xcronzonedbudget){B.Segments,B.Months,B.RuleYears});
            uint32 Segments=V->Search.Work.SegmentsExamined-Before.SegmentsExamined,Months=(uint32)(V->Search.Work.MonthsExamined-Before.MonthsExamined),Years=(uint32)(V->Search.Work.RuleYearsExamined-Before.RuleYearsExamined);
            B.Segments-=Segments;B.Months-=Months;B.RuleYears-=Years;V->Work.SegmentsExamined+=Segments;V->Work.MonthsExamined+=Months;V->Work.RuleYearsExamined+=Years;
            if(S==XCRON_ZONED_PENDING)return V->Status;
            if(S==XCRON_ZONED_FOUND){V->Candidate.When=V->Search.Result;xcronOccurrenceLocal(V->Search.Result,V->Search.Offset,&V->Candidate.Local);xcronZoneResolutionInit(&V->Candidate.Local,&V->Resolve);V->Phase=1;continue;}
            memset(&V->Candidate,0,sizeof(V->Candidate));V->Outcome=S==XCRON_ZONED_NONE?XCRON_OCCURRENCE_NONE:XCRON_OCCURRENCE_ERROR;
            if(S==XCRON_ZONED_ERROR){V->Candidate.When=V->Search.First;V->OutcomeCode=V->Search.Work.Code==XCRON_ZONED_ZONE?XCRON_OCCURRENCE_ZONE:XCRON_OCCURRENCE_RANGE;V->OutcomeZone=V->Search.Work.Zone;}
            if(V->Policy.Missing==XCRON_MISSING_SKIP)return xcronOccurrenceFinish(V);
            xcronOccurrencePrepareGap(V);continue;
        }
        if(V->Phase==1){
            xcronzonestatus S=xcronOccurrenceResolve(V,Z,&B);if(S==XCRON_ZONE_WALK_PENDING)return V->Status;
            V->Candidate.Candidates=V->Resolve.Count;V->Outcome=XCRON_OCCURRENCE_FOUND;
            if(S==XCRON_ZONE_ERROR){V->Outcome=XCRON_OCCURRENCE_ERROR;V->OutcomeCode=XCRON_OCCURRENCE_ZONE;V->OutcomeZone=V->Resolve.Error;}
            else if(!V->Resolve.Count){V->Outcome=XCRON_OCCURRENCE_ERROR;V->OutcomeCode=XCRON_OCCURRENCE_RANGE;}
            else if(V->Resolve.Count>1 && V->Policy.Fold==XCRON_FOLD_REJECT){V->Outcome=XCRON_OCCURRENCE_ERROR;V->OutcomeCode=XCRON_OCCURRENCE_AMBIGUOUS;}
            else if((V->Policy.Fold==XCRON_FOLD_FIRST && V->Candidate.When!=V->Resolve.First) || (V->Policy.Fold==XCRON_FOLD_LAST && V->Candidate.When!=V->Resolve.Last)){
                xcronZonedValueRestart(&V->Search.Expression,Z,V->Candidate.When,V->Limit,XCRON_SEARCH_EXCLUSIVE,&V->Search);V->Phase=0;continue;
            }
            if(V->Policy.Missing==XCRON_MISSING_SKIP)return xcronOccurrenceFinish(V);
            xcronOccurrencePrepareGap(V);continue;
        }
        if(V->Phase==2){
            if(!B.Segments)return V->Status;
            --B.Segments;++V->Work.SegmentsExamined;
            xcronzonestate Before=xcronZoneStateValue(Z,V->After-1),After=xcronZoneStateValue(Z,V->After);
            if(Before.Offset==XCRON_ZONE_OFFSET_UNKNOWN || After.Offset==XCRON_ZONE_OFFSET_UNKNOWN)return xcronOccurrenceGapFail(V,XCRON_OCCURRENCE_ZONE,XCRON_ZONE_UNKNOWN,false);
            if(After.Offset>Before.Offset){xcronOccurrenceGapBegin(V,&Before,&After);continue;}
            V->Phase=3;
        }
        if(V->Phase==3){
            if(V->After>=V->Bound)return xcronOccurrenceFinish(V);
            if(!B.Segments)return V->Status;
            --B.Segments;++V->Work.SegmentsExamined;xcronZoneWalkInit(Z,V->After,&V->Search.Transition);V->Phase=4;
        }
        if(V->Phase==4){
            xcronzonewalk* W=&V->Search.Transition;uint32 Before=W->Years;xcronzonestatus S=xcronZoneWalkStep(W,B.RuleYears);uint32 Used=W->Years-Before;B.RuleYears-=Used;V->Work.RuleYearsExamined+=Used;
            if(S==XCRON_ZONE_WALK_PENDING)return V->Status;
            if(S==XCRON_ZONE_NONE || W->Result.When>V->Bound)return xcronOccurrenceFinish(V);
            V->After=W->Result.When;
            if(W->Result.Before.Offset==XCRON_ZONE_OFFSET_UNKNOWN || W->Result.After.Offset==XCRON_ZONE_OFFSET_UNKNOWN)return xcronOccurrenceGapFail(V,XCRON_OCCURRENCE_ZONE,XCRON_ZONE_UNKNOWN,false);
            if(W->Result.After.Offset>W->Result.Before.Offset){xcronOccurrenceGapBegin(V,&W->Result.Before,&W->Result.After);continue;}
            V->Phase=3;continue;
        }
        if(V->Phase==5){
            xcroncalendarvalue* G=&V->Search.Calendar;uint32 Before=G->Months;xcroncalendarstatus S=xcronCalendarValueStep(&V->Search.Expression,G,B.Months);uint32 Used=G->Months-Before;B.Months-=Used;V->Work.MonthsExamined+=Used;
            if(S==XCRON_CALENDAR_PENDING)return V->Status;
            if(S==XCRON_CALENDAR_NONE){V->Phase=3;continue;}
            int64 Seconds;if(!xcronZoneCivilSeconds(&G->Date,&Seconds))return xcronOccurrenceGapFail(V,XCRON_OCCURRENCE_RANGE,XCRON_ZONE_OK,false);
            int64 Offset=Seconds-V->After/1000;
            if(Offset<-INT32_MAX || Offset>INT32_MAX)return xcronOccurrenceGapFail(V,XCRON_OCCURRENCE_RANGE,XCRON_ZONE_OK,false);
            xcronOccurrenceLocal(V->After,(int32)Offset,&G->Date);xcronZoneResolutionInit(&G->Date,&V->Resolve);V->Phase=6;
        }
        if(V->Phase==6){
            xcronzonestatus S=xcronOccurrenceResolve(V,Z,&B);if(S==XCRON_ZONE_WALK_PENDING)return V->Status;
            if(S==XCRON_ZONE_ERROR)return xcronOccurrenceGapFail(V,XCRON_OCCURRENCE_ZONE,V->Resolve.Error,true);
            if(!V->Resolve.Count)return xcronOccurrenceGapFail(V,XCRON_OCCURRENCE_MISSING,XCRON_ZONE_OK,true);
            xcroncalendarvalue* G=&V->Search.Calendar;xcronCalendarValueAfter(&V->Search.Expression,&G->Date,&G->End,G);V->Phase=5;
        }
    }return V->Status;
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
#define XCRON_OCCURRENCE_CURSOR_MAGIC UINT32_C(0x584f4331)
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
    if(I.Magic!=XCRON_OCCURRENCE_CURSOR_MAGIC || I.Address!=(uintptr_t)C || I.Zone!=(uintptr_t)Z || I.Value.Phase>6 || !xcronOccurrencePolicy(I.Value.Policy) ||
       (I.Value.Status!=XCRON_OCCURRENCE_PENDING && I.Value.Status!=XCRON_OCCURRENCE_FOUND && I.Value.Status!=XCRON_OCCURRENCE_NONE && I.Value.Status!=XCRON_OCCURRENCE_ERROR)){
        xcronOccurrenceReport((xcronoccurrenceissue){.Code=XCRON_OCCURRENCE_STATE,.Field=XCRON_FIELD_NONE},Issue);return XCRON_OCCURRENCE_ERROR;
    }
    xcronoccurrencestatus S=xcronOccurrenceValueStep(&I.Value,Z,B);memcpy(C,&I,sizeof(I));if(S==XCRON_OCCURRENCE_FOUND)*Next=I.Value.Candidate;
    if(S==XCRON_OCCURRENCE_ERROR)xcronOccurrenceReport(I.Value.Work,Issue);else if(Issue)*Issue=I.Value.Work;
    return S;
}
#endif
