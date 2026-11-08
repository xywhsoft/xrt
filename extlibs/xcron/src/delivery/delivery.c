#include <xcron/delivery.h>
#include "../timezone/timezone_internal.h"
#include <xrt/memory.h>
#include <xrt/error.h>
#include <string.h>
#if defined(XCRON_FEATURE_XCRON_DELIVERY)
static xcrondeliverystatus xcronDeliveryFail(xcrondeliveryerror Code,xcronzoneerror Zone,size_t Candidates,xcrondeliveryissue* Issue)
{
    if(Issue)*Issue=(xcrondeliveryissue){Code,Zone,Candidates};
    xrtSetErrorInfo(Code==XCRON_DELIVERY_ARGUMENT?XERR_ARGUMENT:Code==XCRON_DELIVERY_ZONE && Zone==XCRON_ZONE_UNKNOWN?XERR_STATE:XERR_VALUE,
        "xrt.cron.delivery",Code,"cron nominal label selection failed");return XCRON_DELIVERY_ERROR;
}
static xcrondeliverystatus xcronDeliverySelected(const xcronzone* Z,const xdatetime* Local,xcrondeliverypolicy P,
    xtime* Out,size_t Capacity,size_t* Count,xcrondeliveryissue* Issue)
{
    /* TZif has at most256 types plus two footer offsets; POSIX has at most two.
     * Keep completeness before selecting, including conservative UNKNOWN checks. */
    xtime Values[258];size_t N=0;xcronzoneissue Work={0};
    xcronzonestatus Status=xcronZoneResolve(Z,Local,Values,258,&N,&Work);
    if(Status==XCRON_ZONE_ERROR)return xcronDeliveryFail(XCRON_DELIVERY_ZONE,Work.Code,0,Issue);
    if(Status==XCRON_ZONE_CAPACITY)return xcronDeliveryFail(XCRON_DELIVERY_LIMIT,XCRON_ZONE_LIMIT,N,Issue);
    if(!N && P.Missing==XCRON_MISSING_REJECT)return xcronDeliveryFail(XCRON_DELIVERY_MISSING,XCRON_ZONE_OK,0,Issue);
    if(N>1 && P.Fold==XCRON_FOLD_REJECT)return xcronDeliveryFail(XCRON_DELIVERY_AMBIGUOUS,XCRON_ZONE_OK,N,Issue);
    size_t Required=P.Fold==XCRON_FOLD_ALL?N:N?1:0;
    if(Issue)*Issue=(xcrondeliveryissue){Required>Capacity?XCRON_DELIVERY_LIMIT:XCRON_DELIVERY_OK,XCRON_ZONE_OK,N};
    *Count=Required;
    if(Required>Capacity)return XCRON_DELIVERY_CAPACITY;
    if(Required)memcpy(Out,Values+(P.Fold==XCRON_FOLD_LAST?N-1:0),Required*sizeof(*Out));
    return Required?XCRON_DELIVERY_FOUND:XCRON_DELIVERY_NONE;
}
XCRON_API xcrondeliverystatus xcronDeliveryResolve(const xcronzone* Z,const xdatetime* Local,xcrondeliverypolicy P,
    xtime* Out,size_t Capacity,size_t* Count,xcrondeliveryissue* Issue)
{
    void* O[]={Out,Count,Issue};size_t S[]={Capacity<=SIZE_MAX/sizeof(*Out)?Capacity*sizeof(*Out):0,Count?sizeof(*Count):0,Issue?sizeof(*Issue):0};
    if(!Local || !Count || Capacity>SIZE_MAX/sizeof(*Out) || (!Out && Capacity) ||
        P.Fold<XCRON_FOLD_ALL || P.Fold>XCRON_FOLD_REJECT || P.Missing<XCRON_MISSING_SKIP || P.Missing>XCRON_MISSING_REJECT ||
        !xcronZoneOutputs(Z,Local,sizeof(*Local),O,S,3))return xcronDeliveryFail(XCRON_DELIVERY_ARGUMENT,XCRON_ZONE_OK,0,NULL);
    if(P.Fold!=XCRON_FOLD_ALL || P.Missing!=XCRON_MISSING_SKIP)return xcronDeliverySelected(Z,Local,P,Out,Capacity,Count,Issue);
    xcronzoneissue Work={0};xcronzonestatus Status=xcronZoneResolve(Z,Local,Out,Capacity,Count,&Work);
    if(Status==XCRON_ZONE_ERROR)return xcronDeliveryFail(XCRON_DELIVERY_ZONE,Work.Code,0,Issue);
    if(Issue)*Issue=(xcrondeliveryissue){Status==XCRON_ZONE_CAPACITY?XCRON_DELIVERY_LIMIT:XCRON_DELIVERY_OK,XCRON_ZONE_OK,*Count};
    return Status==XCRON_ZONE_CAPACITY?XCRON_DELIVERY_CAPACITY:Status==XCRON_ZONE_FOUND?XCRON_DELIVERY_FOUND:XCRON_DELIVERY_NONE;
}
#endif
