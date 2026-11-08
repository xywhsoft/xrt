#include <xcron/delivery.h>
#include "../timezone/timezone_internal.h"
#include <xrt/error.h>
#include <xrt/memory.h>
#include <string.h>
#if defined(XCRON_FEATURE_XCRON_DELIVERY)
#define XCRON_DELIVERY_CURSOR_MAGIC UINT32_C(0x58444331)
typedef struct xcrondeliveryimpl {uint32 Magic;uintptr_t Address,Zone;xcrondeliverypolicy Policy;xcronzoneresolution Value;} xcrondeliveryimpl;
typedef char xcron_delivery_cursor_size_check[(sizeof(xcrondeliveryimpl)+258*sizeof(xtime)<=sizeof(xcrondeliverycursor))?1:-1];
void xcronZoneResolutionInit(const xdatetime* D,xcronzoneresolution* V)
{
    memset(V,0,sizeof(*V));V->Milli=D->Millisecond;V->Status=XCRON_ZONE_NONE;
    if(xcronZoneCivilSeconds(D,&V->Local))V->Status=XCRON_ZONE_WALK_PENDING;
}
xcronzonestatus xcronZoneResolutionStep(xcronzoneresolution* V,const xcronzone* Z,xcronresolutionbudget B,uint8* Values)
{
    while(V->Status==XCRON_ZONE_WALK_PENDING){
        if(V->Phase==0){
            if(!B.Checks)return V->Status;
            --B.Checks;++V->Checks;
            if(Z->History)xcronZoneHistoryResolutionBegin(Z,V);
            else {V->OffsetsCount=Z->HasDST && Z->Offsets[0]!=Z->Offsets[1]?2:1;V->Phase=2;}
            continue;
        }
        if(V->Phase==1){
            if(!B.Checks)return V->Status;
            uint32 Before=V->Walk.Years;xcronzonestatus S=xcronZoneWalkStep(&V->Walk,B.RuleYears);
            uint32 Used=V->Walk.Years-Before;B.RuleYears-=Used;V->RuleYears+=Used;
            if(S==XCRON_ZONE_WALK_PENDING)return V->Status;
            --B.Checks;++V->Checks;
            if(S!=XCRON_ZONE_FOUND || V->Walk.Result.When>V->UnknownEnd){V->Phase=2;continue;}
            xstrview Name=V->Walk.Result.After.Abbreviation;
            if(Name.Size==3 && !memcmp(Name.Data,"-00",3)){V->Error=XCRON_ZONE_UNKNOWN;return V->Status=XCRON_ZONE_ERROR;}
            const xcronzone* Rules=V->Walk.Rules;xtime After=V->Walk.Result.When;xcronZoneWalkInit(Rules,After,&V->Walk);continue;
        }
        if(V->Index==V->OffsetsCount)return V->Status=V->Count?XCRON_ZONE_FOUND:XCRON_ZONE_NONE;
        if(!B.Offsets)return V->Status;
        int32 Offset;
        if(Z->History)Offset=xcronZoneHistoryResolutionOffset(Z,V->Index);
        else {bool Reversed=Z->Offsets[0]<Z->Offsets[1];Offset=Z->Offsets[V->OffsetsCount==1?0:(V->Index==0?(Reversed?1:0):(Reversed?0:1))];}
        ++V->Index;--B.Offsets;++V->Offsets;xtime T;
        if(xcronZoneSecondValue(V->Local-Offset,V->Milli,&T) && xcronZoneStateValue(Z,T).Offset==Offset){
            if(!V->Count)V->First=T;
            V->Last=T;if(Values)memcpy(Values+V->Count*sizeof(T),&T,sizeof(T));++V->Count;
        }
    }return V->Status;
}
static void xcronDeliveryCursorReport(xcrondeliverycursorissue Work,xcrondeliverycursorissue* Issue)
{
    if(Issue)*Issue=Work;
    xerrkind Kind=Work.Issue.Code==XCRON_DELIVERY_ARGUMENT?XERR_ARGUMENT:
        Work.Issue.Code==XCRON_DELIVERY_STATE || (Work.Issue.Code==XCRON_DELIVERY_ZONE && Work.Issue.Zone==XCRON_ZONE_UNKNOWN)?XERR_STATE:XERR_VALUE;
    xrtSetErrorInfo(Kind,"xrt.cron.delivery",Work.Issue.Code,"cron nominal cursor operation failed");
}
static bool xcronDeliveryCivil(const xdatetime* D)
{return D->Year && D->Month>=1 && D->Month<=12 && D->Day>=1 && D->Day<=xrtDaysInMonth(D->Year,D->Month) && D->Hour>=0 && D->Hour<=23 && D->Minute>=0 && D->Minute<=59 && D->Second>=0 && D->Second<=59 && D->Millisecond>=0 && D->Millisecond<=999;}
static bool xcronDeliveryPolicy(xcrondeliverypolicy P)
{return P.Fold>=XCRON_FOLD_ALL && P.Fold<=XCRON_FOLD_REJECT && P.Missing>=XCRON_MISSING_SKIP && P.Missing<=XCRON_MISSING_REJECT;}
XCRON_API bool xcronDeliveryCursorInit(xcrondeliverycursor* C,const xcronzone* Z,const xdatetime* Local,xcrondeliverypolicy P,xcrondeliverycursorissue* Issue)
{
    void* O[]={C,Issue};size_t N[]={sizeof(*C),Issue?sizeof(*Issue):0};
    if(!C || !Local || !xcronDeliveryPolicy(P) || !xcronZoneOutputs(Z,Local,sizeof(*Local),O,N,2)){
        xcronDeliveryCursorReport((xcrondeliverycursorissue){.Issue.Code=XCRON_DELIVERY_ARGUMENT},NULL);return false;
    }
    if(!xcronDeliveryCivil(Local)){xcronDeliveryCursorReport((xcrondeliverycursorissue){.Issue={XCRON_DELIVERY_ZONE,XCRON_ZONE_DATE,0}},Issue);return false;}
    xcrondeliveryimpl I;memset(&I,0,sizeof(I));I.Magic=XCRON_DELIVERY_CURSOR_MAGIC;I.Address=(uintptr_t)C;I.Zone=(uintptr_t)Z;I.Policy=P;xcronZoneResolutionInit(Local,&I.Value);
    memset(C,0,sizeof(*C));memcpy(C,&I,sizeof(I));if(Issue)*Issue=(xcrondeliverycursorissue){0};return true;
}
XCRON_API xcrondeliverystatus xcronDeliveryCursorStep(xcrondeliverycursor* C,const xcronzone* Z,xcrondeliverybudget B,xtime* Out,size_t Capacity,size_t* Count,xcrondeliverycursorissue* Issue)
{
    void* O[]={C,Out,Count,Issue};size_t N[]={sizeof(*C),Capacity<=SIZE_MAX/sizeof(*Out)?Capacity*sizeof(*Out):0,Count?sizeof(*Count):0,Issue?sizeof(*Issue):0};
    if(!C || !Count || Capacity>SIZE_MAX/sizeof(*Out) || (!Out && Capacity) || !xcronZoneOutputs(Z,NULL,0,O,N,4)){
        xcronDeliveryCursorReport((xcrondeliverycursorissue){.Issue.Code=XCRON_DELIVERY_ARGUMENT},NULL);return XCRON_DELIVERY_ERROR;
    }
    xcrondeliveryimpl I;memcpy(&I,C,sizeof(I));xcronzoneresolution* V=&I.Value;
    if(I.Magic!=XCRON_DELIVERY_CURSOR_MAGIC || I.Address!=(uintptr_t)C || I.Zone!=(uintptr_t)Z || !xcronDeliveryPolicy(I.Policy) || V->Phase>2 || V->Count>258 || V->Index>V->OffsetsCount || V->OffsetsCount>258 ||
        (V->Status!=XCRON_ZONE_WALK_PENDING && V->Status!=XCRON_ZONE_FOUND && V->Status!=XCRON_ZONE_NONE && V->Status!=XCRON_ZONE_ERROR)){
        xcronDeliveryCursorReport((xcrondeliverycursorissue){.Issue.Code=XCRON_DELIVERY_STATE},Issue);return XCRON_DELIVERY_ERROR;
    }
    xcronzonestatus Status=xcronZoneResolutionStep(V,Z,(xcronresolutionbudget){B.Checks,B.Offsets,B.RuleYears},I.Policy.Fold==XCRON_FOLD_ALL?C->Storage+sizeof(I):NULL);
    memcpy(C,&I,sizeof(I));xcrondeliverycursorissue Work={.ChecksExamined=V->Checks,.OffsetsExamined=V->Offsets,.RuleYearsExamined=V->RuleYears};
    if(Status==XCRON_ZONE_WALK_PENDING){if(Issue)*Issue=Work;return XCRON_DELIVERY_PENDING;}
    Work.Issue.Candidates=V->Count;
    if(Status==XCRON_ZONE_ERROR){Work.Issue.Code=XCRON_DELIVERY_ZONE;Work.Issue.Zone=V->Error;}
    else if(!V->Count && I.Policy.Missing==XCRON_MISSING_REJECT)Work.Issue.Code=XCRON_DELIVERY_MISSING;
    else if(V->Count>1 && I.Policy.Fold==XCRON_FOLD_REJECT)Work.Issue.Code=XCRON_DELIVERY_AMBIGUOUS;
    if(Work.Issue.Code!=XCRON_DELIVERY_OK){xcronDeliveryCursorReport(Work,Issue);return XCRON_DELIVERY_ERROR;}
    size_t Required=I.Policy.Fold==XCRON_FOLD_ALL?V->Count:V->Count?1:0;*Count=Required;
    if(Required>Capacity){Work.Issue.Code=XCRON_DELIVERY_LIMIT;if(Issue)*Issue=Work;return XCRON_DELIVERY_CAPACITY;}
    if(Required){
        if(I.Policy.Fold==XCRON_FOLD_ALL)memcpy(Out,C->Storage+sizeof(I),Required*sizeof(*Out));
        else *Out=I.Policy.Fold==XCRON_FOLD_LAST?V->Last:V->First;
    }
    if(Issue)*Issue=Work;
    return Required?XCRON_DELIVERY_FOUND:XCRON_DELIVERY_NONE;
}
#endif
